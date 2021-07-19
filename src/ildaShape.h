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
        options = {OFXLASER_PROFILE_FAST, OFXLASER_PROFILE_DEFAULT, OFXLASER_PROFILE_DETAIL};
        listener = vInput.newListener([this](vector<ofxFatLine> &vf){
            for(auto &fat : vf){
                vector<ofColor> colors(fat.getColors().size());
                for(int i = 0; i < colors.size(); i++){
                    colors[i] = ofColor(fat.getColors()[i]);
                }
				if(fat.size() == 1){
					controller->getManager().drawDot(fat.getVertices()[0], colors[0], 1, options[renderProfile]);
				}
				else{
					controller->getManager().drawPoly((ofPolyline)fat, colors, options[renderProfile]);
				}
            }
        });
    }
    
private:
    shared_ptr<ildaController> controller;
    vector<string> options;
    ofParameter<int> renderProfile;
    ofParameter<ofxFatLine> input;
    ofParameter<vector<ofxFatLine>> vInput;
	
	ofParameter<vector<float>> oscOutput;
    
    ofEventListener listener;
};



#endif /* ildaShape_h */