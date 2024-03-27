//
//  ofxOceanodeLaser.h
//  example
//
//  Created by Eduard Frigola Bagué on 10/02/2021.
//

#ifndef ofxOceanodeLaser_h
#define ofxOceanodeLaser_h

#include "ildaController.h"
#include "ildaProjector.h"
#include "ildaShape.h"
#include "polyOscSender.h"
#include "ofxOceanodeOSCController.h"

#include "ofxOceanode.h"

class ildaDebugger : public ofxOceanodeNodeModel {
public:
    ildaDebugger(shared_ptr<ildaController> iController) : controller(iController), ofxOceanodeNodeModel("ILDA Debugger"){}
    
    void setup(){
        addParameter(x.set("x", {0}, {0}, {1}));
        addParameter(y.set("y", {0}, {0}, {1}));
        addParameter(r.set("r", {0}, {0}, {1}));
        addParameter(g.set("g", {0}, {0}, {1}));
        addParameter(b.set("b", {0}, {0}, {1}));
    }
    
    void update(ofEventArgs &a){
        vector<ofxLaser::Point> points = controller->getAllLaserPoints();
        vector<float> tempX(points.size());
        vector<float> tempY(points.size());
        vector<float> tempR(points.size());
        vector<float> tempG(points.size());
        vector<float> tempB(points.size());
        
        for(int i = 0; i < points.size(); i++){
            tempX[i] = points[i].x / 800.0f;
            tempY[i] = points[i].y / 800.0f;
            tempR[i] = points[i].r;
            tempG[i] = points[i].g;
            tempB[i] = points[i].b;
        }
  
        x = tempX;
        y = tempY;
        r = tempR;
        g = tempG;
        b = tempB;
    }

private:
    shared_ptr<ildaController> controller;
    
    ofParameter<vector<float>> x;
    ofParameter<vector<float>> y;
    ofParameter<vector<float>> r;
    ofParameter<vector<float>> g;
    ofParameter<vector<float>> b;
};

namespace ofxOceanodeLaser{
static void registerModels(ofxOceanode &o, shared_ptr<ildaController> ildaController){
    o.registerModel<ildaProjector>("ILDA", ildaController);
    o.registerModel<ildaShape>("ILDA", ildaController);
    o.registerModel<polyOscSender>("ILDA", o.getController<ofxOceanodeOSCController>());
    o.registerModel<ildaDebugger>("ILDA", ildaController);
}
static void registerType(ofxOceanode &o){
    
}
static void registerScope(ofxOceanode &o){
    
}
static shared_ptr<ildaController> addController(ofxOceanode &o){
    return o.addController<ildaController>();
}
static void registerCollection(ofxOceanode &o){
    registerModels(o, ofxOceanodeLaser::addController(o));
    registerType(o);
    registerScope(o);
}
}

#endif /* ofxOceanodeLaser_h */
