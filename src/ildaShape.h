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
        addParameterDropdown(renderProfile, "Profile", 0, {"Dafault", "Fast", "High Quality"});
		addParameter(sendBlackShapes.set("Black", true));
        options = {OFXLASER_PROFILE_FAST, OFXLASER_PROFILE_DEFAULT, OFXLASER_PROFILE_DETAIL};
		listener = vInput.newListener([this](vector<ofxFatLine> &vf) {
			if (!disable) {
				for (auto &fat : vf) {
					vector<ofColor> colors(fat.getColors().size());
					for (int i = 0; i < colors.size(); i++) {
						colors[i] = ofColor(fat.getColors()[i]);
					}
					float colorSum = std::accumulate(colors.begin(), colors.end(), 0.0f, [](float current_sum, ofColor const& value) { return current_sum + value.r + value.g + value.b; });

					if (sendBlackShapes || colorSum != 0) {
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
    ofParameter<ofxFatLine> input;
    ofParameter<vector<ofxFatLine>> vInput;
	
	ofParameter<vector<float>> oscOutput;
    
    ofEventListener listener;

	bool disable;
};



#endif /* ildaShape_h */
