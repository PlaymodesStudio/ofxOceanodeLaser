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
#include <limits>


class ildaShape : public ofxOceanodeNodeModel {
public:
    ildaShape(shared_ptr<ildaController> iController) : controller(iController), ofxOceanodeNodeModel("ILDA Shape"){}
    
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
					for (auto &fat : toSendFatline) {
						auto routedFat = fat;
						const glm::vec2 zoneOffset = controller->getZoneOffset(selectedZoneId);
						for(auto &vertex : routedFat.getVertices()){
							vertex.x += zoneOffset.x;
							vertex.y += zoneOffset.y;
						}
						vector<ofColor> colors(fat.getColors().size());
					for (int i = 0; i < colors.size(); i++) {
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
				                    colors[i] = ofColor(fat.getColors()[i]);
					}
                    float colorSum = std::accumulate(colors.begin(), colors.end(), 0.0f, [](float current_sum, ofColor const& value) { return current_sum + std::max(value.r, std::max(value.g, value.b));});

	                    if (sendBlackShapes || colorSum > (blackThreshold * 255 * fat.size())){
                        // Feed the simulator buffer with the same data that goes to the laser.
                        // Must be called before drawDot/drawPoly so the frame stamp is updated
                        // on the first shape of the frame regardless of draw order.
                        controller->publishLaserShape(fat, colors);

	                        auto &manager = controller->getManager();
	                        const bool previousUseClipRectangle = manager.useClipRectangle;
	                        const ofRectangle previousClipRectangle = manager.clipRectangle;
	                        // Keep explicitly routed geometry inside its selected tile. The tiny
	                        // inset prevents a clipped point on a shared edge from also matching
	                        // the neighbouring input zone.
	                        manager.useClipRectangle = true;
	                        manager.clipRectangle.set(zoneOffset.x + 0.01f, 0.01f,
	                                                  ildaController::ZoneSize - 0.02f,
	                                                  ildaController::ZoneSize - 0.02f);
	                        if (routedFat.size() == 1) {
	                            manager.drawDot(routedFat.getVertices()[0], colors[0], 1, options[renderProfile]);
	                        }
	                        else {
	                            manager.drawPoly((ofPolyline)routedFat, colors, options[renderProfile]);
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
