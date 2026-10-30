//
//  ildaDebugger.h
//  ofxOceanodeLaser
//
//  Moved out of ofxOceanodeLaser.h. Publishes every laser's output points as
//  x/y (0-1), r/g/b (0-255) and Size, reading them in place each frame.
//

#ifndef ildaDebugger_h
#define ildaDebugger_h

#include "ofxOceanodeNodeModel.h"
#include "ildaController.h"

class ildaDebugger : public ofxOceanodeNodeModel {
public:
    ildaDebugger(shared_ptr<ildaController> iController) : controller(iController), ofxOceanodeNodeModel("ILDA Debugger"){}

    std::string macroMixerBlocker() const override { return "Reads shared laser hardware state. Keep ILDA Debugger outside MacroMixer."; }

    void setup(){
        addOutputParameter(x.set("x", {0}, {0}, {1}));
        addOutputParameter(y.set("y", {0}, {0}, {1}));
        addOutputParameter(r.set("r", {0}, {0}, {255}));
        addOutputParameter(g.set("g", {0}, {0}, {255}));
        addOutputParameter(b.set("b", {0}, {0}, {255}));
        addOutputParameter(size.set("Size", {0},{0},{10000}));
    }

    void update(ofEventArgs &a){
        // Read each laser's point buffer where it lives: no concatenated copy
        // of ofxLaser::Point objects, and the scratch vectors keep their
        // capacity from frame to frame, so a frame allocates nothing.
        auto& lasers = controller->getManager().getLasers();
        size_t count = 0;
        for(auto &l : lasers) count += l->getLaserPoints().size();
        tempX.resize(count);
        tempY.resize(count);
        tempR.resize(count);
        tempG.resize(count);
        tempB.resize(count);

        size_t i = 0;
        for(auto &l : lasers){
            for(const ofxLaser::Point &p : l->getLaserPoints()){
                tempX[i] = p.x / 800.0f;
                tempY[i] = p.y / 800.0f;
                tempR[i] = p.r;
                tempG[i] = p.g;
                tempB[i] = p.b;
                ++i;
            }
        }

        // Y before X: consumers such as Bounding Box Aspect Ratio compute on X.
        y = tempY;
        x = tempX;
        r = tempR;
        g = tempG;
        b = tempB;
        size = float(count);
    }

private:
    shared_ptr<ildaController> controller;

    ofParameter<vector<float>> x;
    ofParameter<vector<float>> y;
    ofParameter<vector<float>> r;
    ofParameter<vector<float>> g;
    ofParameter<vector<float>> b;
    ofParameter<float> size;

    vector<float> tempX, tempY, tempR, tempG, tempB;
};

#endif /* ildaDebugger_h */
