//
//  ildaProjector.h
//  example
//
//  Created by Eduard Frigola Bagué on 10/02/2021.
//

#ifndef ildaProjector_h
#define ildaProjector_h

#include "ofxOceanodeNodeModel.h"
#include "ildaController.h"


class ildaProjector : public ofxOceanodeNodeModel {
public:
    ildaProjector(shared_ptr<ildaController> iController) : controller(iController), ofxOceanodeNodeModel("ILDA Projector"){}
    
    void setup(){
        projector = controller->addLaser();
        
        // Arm the laser DAC by default
        projector->armed = true;
        
  addParameter(intensity.set("Intensity", 1, 0, 1));
  parameterListeners.push(intensity.newListener([this](float &f){
   projector->intensity = f;
  }));
  // Make sure the projector intensity reflects the default value (1.0)
  projector->intensity = intensity.get();

        addParameter(pps.set("PPS", 30000,1000,90000));
        parameterListeners.push(pps.newListener([this](int &f){
            projector->pps = f;
        }));
		
		addParameter(colourChangeShift.set("Offset", 0,0,6));
		parameterListeners.push(colourChangeShift.newListener([this](float &f){
			projector->scannerSync = f;
		}));
        addParameter(moveSpeed.set("Speed", 5,0.1,50));
		parameterListeners.push(moveSpeed.newListener([this](float &f){
			projector->scannerSettings.moveSpeed = f;
		}));
        addParameter(shapePreBlank.set("Blank b", 1,0,8));
		parameterListeners.push(shapePreBlank.newListener([this](int &f){
            projector->scannerSettings.shapePreBlank = f;
        }));
        addParameter(shapePreOn.set("On b", 1,0,8));
		parameterListeners.push(shapePreOn.newListener([this](int &f){
            projector->scannerSettings.shapePreOn = f;
        }));
        addParameter(shapePostOn.set("On a", 1,0,8));
		parameterListeners.push(shapePostOn.newListener([this](int &f){
            projector->scannerSettings.shapePostOn = f;
        }));
        addParameter(shapePostBlank.set("Blank a", 1,0,8));
		parameterListeners.push (shapePostBlank.newListener([this](int &f){
            projector->scannerSettings.shapePostBlank = f;
        }));
    }
    
private:
    shared_ptr<ildaController> controller;
    std::shared_ptr<ofxLaser::Laser> projector;
    
	ofParameter<float> intensity;
	ofParameter<int> pps;
	ofParameter<float> colourChangeShift;
	ofParameter<float> moveSpeed;
	ofParameter<int> shapePreBlank;
	ofParameter<int> shapePostBlank;
	ofParameter<int> shapePreOn;
	ofParameter<int> shapePostOn;
	ofEventListeners parameterListeners;
};


#endif /* ildaProjector_h */
