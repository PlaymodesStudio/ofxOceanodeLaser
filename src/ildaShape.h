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


class ildaShape : public ofxOceanodeNodeModel {
public:
    ildaShape(shared_ptr<ildaController> iController) : controller(iController), ofxOceanodeNodeModel("ILDA Shape"){}
    
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
				for (auto &fat : toSendFatline) {
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

      if (fat.size() == 1) {
       controller->getManager().drawDot(fat.getVertices()[0], colors[0], 1, options[renderProfile]);
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
