//
//  ildaDebugger.h
//  ofxOceanodeLaser
//
//  Moved out of ofxOceanodeLaser.h. Publishes the output points of one laser,
//  or of all lasers one after another, as x/y (0-1), r/g/b (0-255) and Size,
//  reading them in place each frame.
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
        addParameter(on.set("On?", true));
        // "All Lasers" goes after the projectors, as in Laser Simulator, and is
        // the default so patches saved before the selector keep summing every laser.
        auto laserNames = controller->getLaserNames();
        availableLaserCount = static_cast<int>(laserNames.size());
        knownLasersRevision = controller->getLasersRevision();
        laserNames.push_back("All Lasers");
        laserDropdown = addParameterDropdown(laserIndex, "Laser", availableLaserCount, laserNames);
        addOutputParameter(x.set("x", {0}, {0}, {1}));
        addOutputParameter(y.set("y", {0}, {0}, {1}));
        addOutputParameter(r.set("r", {0}, {0}, {255}));
        addOutputParameter(g.set("g", {0}, {0}, {255}));
        addOutputParameter(b.set("b", {0}, {0}, {255}));
        addOutputParameter(size.set("Size", {0},{0},{10000}));
    }

    // The All entry follows the projectors; persist its meaning even if the
    // number of configured projectors changes before the next recall.
    void presetSave(ofJson &json) override {
        json["DebugAllLasers"] = laserIndex.get() == availableLaserCount;
    }

    void presetRecallAfterSettingParameters(ofJson &json) override {
        refreshLasers();
        // Presets saved before the selector existed read every laser.
        auto all = json.find("DebugAllLasers");
        if(all == json.end() || !all->is_boolean() || all->get<bool>()) {
            laserIndex.setWithoutEventNotifications(availableLaserCount);
        } else if(availableLaserCount > 0 && laserIndex.get() >= availableLaserCount) {
            laserIndex.setWithoutEventNotifications(availableLaserCount - 1);
        }
    }

    void update(ofEventArgs &a){
        refreshLasers();
        if(!on.get()) return;

        // Read each laser's point buffer where it lives: no concatenated copy
        // of ofxLaser::Point objects, and the scratch vectors keep their
        // capacity from frame to frame, so a frame allocates nothing.
        auto& lasers = controller->getManager().getLasers();
        const bool allLasers = laserIndex.get() >= availableLaserCount;
        auto isSelected = [&](size_t index){ return allLasers || static_cast<int>(index) == laserIndex.get(); };
        size_t count = 0;
        for(size_t index = 0; index < lasers.size(); ++index){
            if(isSelected(index)) count += lasers[index]->getLaserPoints().size();
        }
        tempX.resize(count);
        tempY.resize(count);
        tempR.resize(count);
        tempG.resize(count);
        tempB.resize(count);

        size_t i = 0;
        for(size_t index = 0; index < lasers.size(); ++index){
            if(!isSelected(index)) continue;
            for(const ofxLaser::Point &p : lasers[index]->getLaserPoints()){
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
    // Follows lasers added or removed in the ILDA window, keeping an All
    // selection on All and clamping a single laser that no longer exists.
    void refreshLasers(){
        if(knownLasersRevision == controller->getLasersRevision()) return;
        const bool wasAll = laserIndex.get() == availableLaserCount;
        knownLasersRevision = controller->getLasersRevision();
        auto names = controller->getLaserNames();
        availableLaserCount = static_cast<int>(names.size());
        names.push_back("All Lasers");
        laserDropdown->setDropdownOptions(names);
        laserIndex.setMin(0);
        laserIndex.setMax(availableLaserCount);
        if(wasAll){
            laserIndex.setWithoutEventNotifications(availableLaserCount);
        } else if(laserIndex.get() >= availableLaserCount){
            laserIndex.setWithoutEventNotifications(std::max(0, availableLaserCount - 1));
        }
    }

    shared_ptr<ildaController> controller;

    ofParameter<bool> on;
    ofParameter<int> laserIndex;
    shared_ptr<ofxOceanodeParameter<int>> laserDropdown;
    int availableLaserCount = 0;  // the appended All Lasers option uses this index
    uint64_t knownLasersRevision = 0;
    ofParameter<vector<float>> x;
    ofParameter<vector<float>> y;
    ofParameter<vector<float>> r;
    ofParameter<vector<float>> g;
    ofParameter<vector<float>> b;
    ofParameter<float> size;

    vector<float> tempX, tempY, tempR, tempG, tempB;
};

#endif /* ildaDebugger_h */
