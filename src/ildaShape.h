//
//  ildaShape.h
//  example
//
//  Created by Eduard Frigola Bagué on 10/02/2021.
//

#ifndef ildaShape_h
#define ildaShape_h

#include "ofxOceanodeNodeModel.h"
#include "ildaController.h"
#include "ofxFatLine.h"
#include "ildaMaskField.h"
#include <limits>


class ildaShape : public ofxOceanodeNodeModel {
public:
    ildaShape(shared_ptr<ildaController> iController) : controller(iController), ofxOceanodeNodeModel("ILDA Shape"){}
    
    std::string macroMixerBlocker() const override { return "Uses the shared ILDA controller. Connect the mixed Fatline Vector to ILDA Shape outside MacroMixer."; }

    void setup() override {
        addParameter(vInput.set("v In", {ofxFatLine()}));
        zoneDropdown = addParameterDropdown(zone, "Zone", 0, controller->getZoneNames());
        selectedZoneId = controller->getZoneStableId(0);
        zoneListener = zone.newListener([this](int &index){
            selectedZoneId = controller->getZoneStableId(index);
        });
        addParameterDropdown(renderProfile, "Profile", 0, {"Fast", "Default", "High Quality"});
		addParameter(sendBlackShapes.set("Black", true));
        addParameter(blackThreshold.set("Black Thr", 0, 0, 1));
        options = {OFXLASER_PROFILE_FAST, OFXLASER_PROFILE_DEFAULT, OFXLASER_PROFILE_DETAIL};
        listener = vInput.newListener([this](vector<ofxFatLine> &vf) {
            if (!disable) {
                refreshZones();
                auto toSendFatline = vf;
                if(controller->isFrozen()){
                    toSendFatline = storedInput;
                }
                else{
                    storedInput = vf;
                }
				const bool masked = controller->hasActiveMask();
				for (auto &fat : toSendFatline) {
                    // NOTE: Upstream (e.g. fatlineGenerator) already delivers
                    // PREMULTIPLIED ofFloatColor vertex colors: its "Opacity"
                    // parameter is multiplied into R, G, B AND A, so a white
                    // line at opacity α arrives here as (α, α, α, α).
                    // Do NOT multiply by .a again — that would square the
                    // alpha (α²) and cause:
                    //   - "Black Thr" to gate at √threshold instead of threshold
                    //     (e.g. Black Thr = 0.05 would cull at ~22% opacity
                    //     instead of the expected 5%),
                    //   - all colored laser output to be dimmed quadratically.
                    // Just narrow ofFloatColor → ofColor (×255).
                    vector<ofFloatColor> sourceColors = fat.getColors();
					vector<ofColor> colors;
                    if (masked) {
                        applyMask(fat, sourceColors, maskedVertices, colors);
                    }
                    else {
                        colors.resize(sourceColors.size());
                        for (int i = 0; i < colors.size(); i++) {
                            colors[i] = ofColor(sourceColors[i]);
                        }
                    }
                    const vector<glm::vec3> &vertices = masked ? maskedVertices : fat.getVertices();
                    float colorSum = std::accumulate(colors.begin(), colors.end(), 0.0f, [](float current_sum, ofColor const& value) { return current_sum + std::max(value.r, std::max(value.g, value.b));});

                    if (sendBlackShapes || colorSum > (blackThreshold * 255 * vertices.size())){
                        // Feed the simulator buffer with the same data that goes to the laser.
                        // Must be called before drawDot/drawPoly so the frame stamp is updated
                        // on the first shape of the frame regardless of draw order.
                        controller->publishLaserShape(vertices, colors);

                        // Mask in the shape's local canvas before routing into a zone.
                        ofPolyline routedPoly(vertices);
                        routedPoly.setClosed(fat.isClosed());
                        // A shape that ends on its start point is a loop: send it
                        // closed, without the repeated point. ofxLaser ends each run
                        // 0.1% short of its end, which left an open loop with a
                        // visible gap just before its start point. A closed loop's
                        // last run is the short closing segment, so the shortfall
                        // vanishes.
                        auto &routedVertices = routedPoly.getVertices();
                        if (!routedPoly.isClosed() && routedVertices.size() > 3 &&
                            glm::distance(routedVertices.front(), routedVertices.back()) < 0.001f) {
                            if (colors.size() == routedVertices.size()) colors.pop_back();
                            routedVertices.pop_back();
                            routedPoly.setClosed(true);
                        }
                        const glm::vec2 zoneOffset = controller->getZoneOffset(selectedZoneId);
                        for(auto &vertex : routedPoly.getVertices()){
                            vertex.x += zoneOffset.x;
                            vertex.y += zoneOffset.y;
                        }

                        auto &manager = controller->getManager();
                        const bool previousUseClipRectangle = manager.useClipRectangle;
                        const ofRectangle previousClipRectangle = manager.clipRectangle;
                        // Inset the clip so a shared edge cannot reach the adjacent zone.
                        manager.useClipRectangle = true;
                        manager.clipRectangle.set(zoneOffset.x + 0.01f, 0.01f,
                                                  ildaController::ZoneSize - 0.02f,
                                                  ildaController::ZoneSize - 0.02f);
                        if (routedPoly.size() == 1) {
                            manager.drawDot(routedPoly.getVertices()[0], colors[0], 1, options[renderProfile]);
                        }
                        else {
                            manager.drawPoly(routedPoly, colors, options[renderProfile]);
                        }
                        manager.useClipRectangle = previousUseClipRectangle;
                        manager.clipRectangle = previousClipRectangle;
                    }
				}
			}
        });

		disable = false;
    }

    void update(ofEventArgs &) override {
        refreshZones();
    }

    void loadBeforeConnections(ofJson &json) override {
        if(json.contains("ildaZoneId")) selectedZoneId = json["ildaZoneId"].get<string>();
        refreshZones();
    }

    void loadCustomPersistent(ofJson &json) override {
        if(json.contains("ildaZoneId")) selectedZoneId = json["ildaZoneId"].get<string>();
        refreshZones();
    }

    void presetSave(ofJson &json) override {
        json["ildaZoneId"] = selectedZoneId;
    }

	void presetWillBeLoaded() override {
		disable = true;
	}

	void presetHasLoaded() override {
		disable = false;
	}


    
private:
    // Inserts vertices where the ILDA Mask changes along the line, interpolates the
    // source colour at each one and darkens it by the mask.
    void applyMask(const ofxFatLine &fat, const vector<ofFloatColor> &sourceColors, vector<glm::vec3> &vertices, vector<ofColor> &colors){
        ildaMasking::maskPolyline(fat.getVertices(), fat.isClosed(), 800.0f,
                                  [this](float u, float v){ return controller->sampleMask(u, v); },
                                  maskSpacing, maskTolerance, maskedSamples);
        vertices.resize(maskedSamples.size());
        colors.resize(maskedSamples.size());
        const int numColors = sourceColors.size();
        for (int i = 0; i < maskedSamples.size(); i++) {
            const auto &sample = maskedSamples[i];
            vertices[i] = sample.position;
            ofFloatColor color = ofFloatColor::black;
            if (numColors > 0) {
                const int i0 = std::min((int)sample.sourceIndex, numColors - 1);
                const int i1 = (i0 + 1) % numColors;
                color = sourceColors[i0].getLerped(sourceColors[i1], sample.sourceIndex - i0);
            }
            colors[i] = ofColor(ofFloatColor(color.r * sample.mask, color.g * sample.mask, color.b * sample.mask, color.a));
        }
    }

    // Mask sampling step along each segment (canvas units, 4 px, about the laser's own
    // point spacing) and the largest mask error tolerated between inserted vertices.
    static constexpr float maskSpacing = 1.0f / 200.0f;
    static constexpr float maskTolerance = 0.02f;
    vector<ildaMasking::MaskedVertex> maskedSamples;
    vector<glm::vec3> maskedVertices;

    void refreshZones(){
        if(knownZonesRevision == controller->getZonesRevision()) return;
        knownZonesRevision = controller->getZonesRevision();
        auto names = controller->getZoneNames();
        if(names.empty()) names.push_back("No zones");
        zoneDropdown->setDropdownOptions(names);
        zone.setMin(0);
        zone.setMax(std::max(0, static_cast<int>(names.size()) - 1));

        int index = controller->findZoneIndex(selectedZoneId);
        if(index < 0) index = 0;
        selectedZoneId = controller->getZoneStableId(index);
        zone.setWithoutEventNotifications(index);
    }

    shared_ptr<ildaController> controller;
    vector<string> options;
    ofParameter<int> zone;
    ofParameter<int> renderProfile;
	ofParameter<bool> sendBlackShapes;
    ofParameter<float> blackThreshold;
    ofParameter<ofxFatLine> input;
    ofParameter<vector<ofxFatLine>> vInput;
	
	ofParameter<vector<float>> oscOutput;
    
    ofEventListener listener;
    ofEventListener zoneListener;
    shared_ptr<ofxOceanodeParameter<int>> zoneDropdown;
    string selectedZoneId;
    uint64_t knownZonesRevision = std::numeric_limits<uint64_t>::max();
    
    vector<ofxFatLine> storedInput;

	bool disable;
};



#endif /* ildaShape_h */
