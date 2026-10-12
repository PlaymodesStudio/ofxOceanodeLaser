#ifndef ildaProjector_h
#define ildaProjector_h

#include "ofxOceanodeNodeModel.h"
#include "ildaController.h"
#include <limits>

// Selects and controls a projector created by the ILDA controller.
class ildaProjector : public ofxOceanodeNodeModel {
public:
    ildaProjector(shared_ptr<ildaController> iController)
    : controller(std::move(iController)), ofxOceanodeNodeModel("ILDA Projector"){}

    void setup() override {
        laserDropdown = addParameterDropdown(laserIndex, "Laser", 0, controller->getLaserNames());
        parameterListeners.push(laserIndex.newListener([this](int &){ if(!loading) syncFromProjector(); }));

        addParameter(armed.set("Armed", false));
        parameterListeners.push(armed.newListener([this](bool &v){
            if(auto p=selectedProjector(); p&&!loading) applyArmed(p, v);
        }));
        addParameter(intensity.set("Intensity", 1, 0, 1));
        parameterListeners.push(intensity.newListener([this](float &v){ if(auto p=selectedProjector(); p&&!loading) p->intensity=v; }));
        addParameter(pps.set("PPS", 30000, 1000, 90000));
        parameterListeners.push(pps.newListener([this](int &v){ if(auto p=selectedProjector(); p&&!loading) p->pps=v; }));
        addParameter(colourChangeShift.set("Color Offset", 0, 0, 6));
        parameterListeners.push(colourChangeShift.newListener([this](float &v){ if(auto p=selectedProjector(); p&&!loading) p->scannerSync=v; }));
        addParameter(moveSpeed.set("Move Speed", 5, 0.1, 50));
        parameterListeners.push(moveSpeed.newListener([this](float &v){ if(auto p=selectedProjector(); p&&!loading) p->scannerSettings.moveSpeed=v; }));
        addParameter(flipX.set("Flip X", false));
        parameterListeners.push(flipX.newListener([this](bool &v){ if(auto p=selectedProjector(); p&&!loading) p->flipX=v; }));
        addParameter(flipY.set("Flip Y", false));
        parameterListeners.push(flipY.newListener([this](bool &v){ if(auto p=selectedProjector(); p&&!loading) p->flipY=v; }));
        addParameter(rotation.set("Rotation", 0, -180, 180));
        parameterListeners.push(rotation.newListener([this](int &v){ if(auto p=selectedProjector(); p&&!loading) p->rotation=v; }));
        addParameter(outputOffset.set("Output Offset", glm::vec2(0), glm::vec2(-20), glm::vec2(20)));
        parameterListeners.push(outputOffset.newListener([this](glm::vec2 &v){ if(auto p=selectedProjector(); p&&!loading) p->outputOffset=v; }));
        addParameter(shapePreBlank.set("Blank Before", 1, 0, 8));
        parameterListeners.push(shapePreBlank.newListener([this](int &v){ if(auto p=selectedProjector(); p&&!loading) p->scannerSettings.shapePreBlank=v; }));
        addParameter(shapePreOn.set("On Before", 1, 0, 8));
        parameterListeners.push(shapePreOn.newListener([this](int &v){ if(auto p=selectedProjector(); p&&!loading) p->scannerSettings.shapePreOn=v; }));
        addParameter(shapePostOn.set("On After", 1, 0, 8));
        parameterListeners.push(shapePostOn.newListener([this](int &v){ if(auto p=selectedProjector(); p&&!loading) p->scannerSettings.shapePostOn=v; }));
        addParameter(shapePostBlank.set("Blank After", 1, 0, 8));
        parameterListeners.push(shapePostBlank.newListener([this](int &v){ if(auto p=selectedProjector(); p&&!loading) p->scannerSettings.shapePostBlank=v; }));

        refreshLasers();
        syncFromProjector();
    }

    void update(ofEventArgs &) override { refreshLasers(); }
    void presetWillBeLoaded() override { loading = true; }
    void presetHasLoaded() override { loading = false; applyToProjector(); }

private:
    shared_ptr<ofxLaser::Laser> selectedProjector(){
        return controller->getLaserAt(laserIndex.get());
    }

    void refreshLasers(){
        if(knownLasersRevision == controller->getLasersRevision()) return;
        knownLasersRevision = controller->getLasersRevision();
        auto names = controller->getLaserNames();
        if(names.empty()) names.push_back("No lasers");
        laserDropdown->setDropdownOptions(names);
        laserIndex.setMin(0);
        laserIndex.setMax(std::max(0, static_cast<int>(names.size()) - 1));
        if(laserIndex.get() > laserIndex.getMax()) laserIndex = laserIndex.getMax();
        syncFromProjector();
    }

    void syncFromProjector(){
        auto p = selectedProjector();
        if(!p) return;
        loading = true;
        armed.setWithoutEventNotifications(p->armed.get());
        intensity.setWithoutEventNotifications(p->intensity.get());
        pps.setWithoutEventNotifications(p->pps.get());
        colourChangeShift.setWithoutEventNotifications(p->scannerSync.get());
        moveSpeed.setWithoutEventNotifications(p->scannerSettings.moveSpeed.get());
        flipX.setWithoutEventNotifications(p->flipX.get());
        flipY.setWithoutEventNotifications(p->flipY.get());
        rotation.setWithoutEventNotifications(p->rotation.get());
        outputOffset.setWithoutEventNotifications(p->outputOffset.get());
        shapePreBlank.setWithoutEventNotifications(p->scannerSettings.shapePreBlank.get());
        shapePreOn.setWithoutEventNotifications(p->scannerSettings.shapePreOn.get());
        shapePostOn.setWithoutEventNotifications(p->scannerSettings.shapePostOn.get());
        shapePostBlank.setWithoutEventNotifications(p->scannerSettings.shapePostBlank.get());
        loading = false;
    }

    void applyToProjector(){
        auto p = selectedProjector();
        if(!p) return;
        applyArmed(p, armed.get()); p->intensity=intensity.get(); p->pps=pps.get();
        p->scannerSync=colourChangeShift.get(); p->scannerSettings.moveSpeed=moveSpeed.get();
        p->flipX=flipX.get(); p->flipY=flipY.get(); p->rotation=rotation.get();
        p->outputOffset=outputOffset.get();
        p->scannerSettings.shapePreBlank=shapePreBlank.get();
        p->scannerSettings.shapePreOn=shapePreOn.get();
        p->scannerSettings.shapePostOn=shapePostOn.get();
        p->scannerSettings.shapePostBlank=shapePostBlank.get();
    }

    void applyArmed(const shared_ptr<ofxLaser::Laser>& projector, bool value){
        // The ofParameter listener only runs when its stored value changes.
        // A DAC can be disarmed during assignment while the parameter remains
        // true, so preset loading must also push the state to the DAC itself.
        projector->armed.setWithoutEventNotifications(value);
        if(projector->hasDac()) projector->getDac()->setArmed(value);
    }

    shared_ptr<ildaController> controller;
    shared_ptr<ofxOceanodeParameter<int>> laserDropdown;
    ofEventListeners parameterListeners;
    uint64_t knownLasersRevision = std::numeric_limits<uint64_t>::max();
    bool loading = false;

    ofParameter<int> laserIndex;
    ofParameter<bool> armed, flipX, flipY;
    ofParameter<float> intensity, colourChangeShift, moveSpeed;
    ofParameter<int> pps, rotation, shapePreBlank, shapePostBlank, shapePreOn, shapePostOn;
    ofParameter<glm::vec2> outputOffset;
};

#endif /* ildaProjector_h */
