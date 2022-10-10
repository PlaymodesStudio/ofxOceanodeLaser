//
//  polyOscSender.cpp
//  ofxOceanodeLaser
//
//  Created by Eduard Frigola Bagué on 28/03/2020.
//

#include "polyOscSender.h"
#include "ofxOceanodeOSCController.h"


void polyOscSender::setup(){
    string strHost = controller->addSender("Polylines");
    
    addInspectorParameter(numInputs.set("Inputs", 4, 1, INT_MAX));
    addParameter(host.set("Host", strHost), ofxOceanodeParameterFlags_DisableSavePreset);
    addParameter(port.set("Port", "6666"));
    
    addParameter(kpps.set("Kpps", 30000, 10000, 90000));
    
    
    inputs.resize(4);
    for(int i = 0; i < inputs.size(); i++){
        auto &in = inputs[i];
        addParameter(in.second.set("In " + ofToString(i), {}));
        parameterListeners.push(in.second.newListener([this, i](vector<ofxFatLine> &vf){
            inputs[i].first = true;
        }));
    }
    
    listeners.push(numInputs.newListener([this](int &i){
        if(inputs.size() != i){
            int oldSize = inputs.size();
            bool remove = oldSize > i;
            inputs.resize(i);
            
            if(remove){
                for(int j = oldSize-1; j >= i; j--){
                    removeParameter("In " + ofToString(j));
                    parameterListeners.unsubscribe(j);
                }
            }else{
                for(int j = oldSize; j < i; j++){
                    auto &in = inputs[j];
                    addParameter(in.second.set("In " + ofToString(j), {}));
                    parameterListeners.push(in.second.newListener([this, j](vector<ofxFatLine> &vf){
                        inputs[j].first = true;
                    }));
                }
            }
        }
    }));
    
    osc.setup(host, ofToInt(port));
    
    listeners.push(controller->hostEvents["Polylines"].newListener([this](string &s){
        host = s;
    }));
	
	listeners.push(host.newListener([this](string &s){
		 osc.setup(host, ofToInt(port));
	}));
	
	listeners.push(port.newListener([this](string &s){
		 osc.setup(host, ofToInt(port));
	}));
    
    listeners.push(kpps.newListener([this](int &i){
        ofxOscMessage m;
        m.setAddress("/0/Parameter/Kpps");
        m.addIntArg(i);
        osc.sendMessage(m);
    }));
	
//	listeners.push(input.newListener([this](vector<ofxFatLine> &vf){
//		ofxOscMessage m;
//		m.setAddress("/Laser/0");
//		for(auto &fat : vf){
//			for(int i = 0; i < fat.size(); i++){
//				m.addFloatArg(fat.getVertices()[i].x / 800.0f);
//				m.addFloatArg(fat.getVertices()[i].y / 800.0f);
//				const ofFloatColor c = fat.getColors()[i];
//				m.addFloatArg(c.r);
//				m.addFloatArg(c.g);
//				m.addFloatArg(c.b);
//			}
//			m.addFloatArg(-1);
//		}
//		osc.sendMessage(m);
//	}));
}

void polyOscSender::update(ofEventArgs &a){
	ofxOscMessage m;
	m.setAddress("/Laser/0");
	for(auto &in : inputs){
        if(in.first){
            auto fatlinesCopy = in.second.get();
            for(auto &fat : fatlinesCopy){
                for(int i = 0; i < fat.size(); i++){
                    m.addFloatArg(fat.getVertices()[i].x / 800.0f);
                    m.addFloatArg(fat.getVertices()[i].y / 800.0f);
                    const ofFloatColor c = fat.getColors()[i];
                    m.addFloatArg(c.r);
                    m.addFloatArg(c.g);
                    m.addFloatArg(c.b);
                }
                m.addFloatArg(-1);
            }
        }
        in.first = false;
	}
	if(m.getNumArgs() == 0){
        //Draw a Black diagonal to clear the "buffer"
        m.addFloatArg(0);
        m.addFloatArg(0);
        m.addFloatArg(0);
        m.addFloatArg(0);
        m.addFloatArg(0);
        m.addFloatArg(1);
        m.addFloatArg(1);
        m.addFloatArg(0);
        m.addFloatArg(0);
        m.addFloatArg(0);
    }
    osc.sendMessage(m);
}
