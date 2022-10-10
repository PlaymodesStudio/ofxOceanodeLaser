//
//  polyOscSender.h
//  ofxOceanodeLaser
//
//  Created by Eduard Frigola Bagué on 28/03/2020.
//

#ifndef polyOscSender_h
#define polyOscSender_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOsc.h"
#include "ofxFatLine.h"

class ofxOceanodeOSCController;

class polyOscSender : public ofxOceanodeNodeModel{
public:
    polyOscSender(shared_ptr<ofxOceanodeOSCController> _controller) : ofxOceanodeNodeModel("Polylines OSC Sender"), controller(_controller){};
    
    void setup();
    
    void update(ofEventArgs &a);
    
    void loadBeforeConnections(ofJson &json){
        deserializeParameter(json, numInputs);
    }
private:
    shared_ptr<ofxOceanodeOSCController> controller;
    
    vector<pair<bool, ofParameter<vector<ofxFatLine>>>> inputs;
    ofParameter<string> host;
    ofParameter<string> port;
    
    ofParameter<int> kpps;
    ofParameter<int> laserIndex;
    ofParameter<int> numInputs;
    
    ofEventListeners listeners;
    ofEventListeners parameterListeners;
    ofxOscSender osc;
};

#endif /* polyOscSender_h */
