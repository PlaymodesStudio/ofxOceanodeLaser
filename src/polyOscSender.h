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

class polyOscSender : public ofxOceanodeNodeModel{
public:
    polyOscSender() : ofxOceanodeNodeModel("Polylines OSC Sender"){};
    
    void setup();
    
    void update(ofEventArgs &a);
private:
    ofParameter<vector<ofxFatLine>> input;
    ofParameter<string> host;
    ofParameter<string> port;
    
    ofParameter<int> kpps;
    
    ofEventListeners listeners;
    ofxOscSender osc;
};

#endif /* polyOscSender_h */
