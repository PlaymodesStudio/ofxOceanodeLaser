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


class ildaShape : public ofxOceanodeNodeModel {
public:
    ildaShape(shared_ptr<ildaController> iController) : controller(iController), ofxOceanodeNodeModel("ILDA Shape"){}
    
    std::string macroMixerBlocker() const override { return "Uses the shared ILDA controller. Connect the mixed Fatline Vector to ILDA Shape outside MacroMixer."; }

    void setup(){
        addParameter(vInput.set("v In", {ofxFatLine()}));
        addParameterDropdown(renderProfile, "Profile", 0, {"Fast", "Default", "High Quality"});
		addParameter(sendBlackShapes.set("Black", true));
        addParameter(blackThreshold.set("Black Thr", 0, 0, 1));
        options = {OFXLASER_PROFILE_FAST, OFXLASER_PROFILE_DEFAULT, OFXLASER_PROFILE_DETAIL};
		listener = vInput.newListener([this](vector<ofxFatLine> &vf) {
			if (!disable) {
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

                        if (vertices.size() == 1) {
                            controller->getManager().drawDot(vertices[0], colors[0], 1, options[renderProfile]);
                        }
                        else if (masked) {
                            ofPolyline poly(vertices);
                            poly.setClosed(fat.isClosed());
                            controller->getManager().drawPoly(poly, colors, options[renderProfile]);
                        }
                        else {
                            controller->getManager().drawPoly((ofPolyline)fat, colors, options[renderProfile]);
                        }
                    }
				}
			}
        });

		disable = false;
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

    shared_ptr<ildaController> controller;
    vector<string> options;
    ofParameter<int> renderProfile;
	ofParameter<bool> sendBlackShapes;
    ofParameter<float> blackThreshold;
    ofParameter<ofxFatLine> input;
    ofParameter<vector<ofxFatLine>> vInput;
	
	ofParameter<vector<float>> oscOutput;
    
    ofEventListener listener;
    
    vector<ofxFatLine> storedInput;

	bool disable;
};



#endif /* ildaShape_h */
