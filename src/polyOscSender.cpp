//
//  polyOscSender.cpp
//  ofxOceanodeLaser
//
//  Created by Eduard Frigola Bagué on 28/03/2020.
//

#include "polyOscSender.h"

void polyOscSender::setup(){
    addParameter(input.set("Input", {ofxFatLine()}));
    addParameter(host.set("Host", "192.168.1.101"), ofxOceanodeParameterFlags_DisableSavePreset);
    addParameter(port.set("Port", "34254"));
    
    addParameter(kpps.set("Kpps", 30000, 10000, 90000));
    osc.setup(host, ofToInt(port));
	
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
	
	listeners.push(input.newListener([this](vector<ofxFatLine> &vf){
		ofxOscMessage m;
		m.setAddress("/Laser/0");
		for(auto &fat : vf){
			for(int i = 0; i < fat.size(); i++){
				m.addFloatArg(fat.getVertices()[i].x / 800.0f);
				m.addFloatArg(fat.getVertices()[i].y / 800.0f);
				m.addFloatArg(0);
				const ofFloatColor c = fat.getColors()[i];
				m.addFloatArg(c.r);
				m.addFloatArg(c.g);
				m.addFloatArg(c.b);
			}
			for(int j = 0; j < 6; j++){
				m.addFloatArg(-1);
			}
		}
		osc.sendMessage(m);
	}));
}

void polyOscSender::update(ofEventArgs &a){
//    ofxOscMessage m;
//    m.setAddress("/0/Frame");
    
	/*
	int arraySize = 0;
    for(auto &fat : input.get()){
		arraySize += fat.size() + 1; //add one for the shapes splitter [-1]
	}
	arraySize--; //Remove the last splitter
	
	vector<float> data(arraySize);
	*/
//	for(auto &fat : input.get()){
//		for(int i = 0; i < fat.size(); i++){
//			m.addFloatArg(fat.getVertices()[i].x);
//			m.addFloatArg(fat.getVertices()[i].y);
//			const ofFloatColor c = fat.getColors()[i];
//			m.addFloatArg(c.r);
//		}
		
		
//        for(auto &poly : path.getOutline()){
//            m.addIntArg(poly.size());
//            for(auto &point : poly.getVertices()){
//                if(path.getStrokeColor() == ofColor::white){
//                    m.addStringArg(ofToString(point.x) + "_" + ofToString(point.y));
//                }else{
//                    m.addStringArg(ofToString(point.x) + "_" + ofToString(point.y) + "_" + ofToString((float)path.getStrokeColor().r / 255.0) + "_" + ofToString((float)path.getStrokeColor().g / 255.0) + "_" + ofToString((float)path.getStrokeColor().b / 255.0));
//                }
//            }
//        }
//    }
//    osc.sendMessage(m);
}
