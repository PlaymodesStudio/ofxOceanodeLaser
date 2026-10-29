//
//  ofxOceanodeLaser.cpp
//  example
//
//  Created by Eduard Frigola Bagué on 10/02/2021.
//

#include "ofxOceanodeLaser.h"
#include "ofxOceanode.h"
#include "ildaController.h"
#include "ildaProjector.h"
#include "ildaShape.h"
#include "ildaMask.h"
#include "laserSimulator.h"
#include "polyOscSender.h"
#include "ildaDebugger.h"
#include "ofxOceanodeOSCController.h"


namespace ofxOceanodeLaser{
void registerModels(ofxOceanode &o, std::shared_ptr<ildaController> ildaController){
    o.registerModel<ildaProjector>("ILDA", ildaController);
    o.registerModel<ildaShape>("ILDA", ildaController);
    o.registerModel<ildaMask>("ILDA", ofxOceanodeNodeRegistrationFlags::Sink, ildaController);
    o.registerModel<laserSimulator>("ILDA", ofxOceanodeNodeRegistrationFlags::Sink, ildaController);
    o.registerModel<polyOscSender>("ILDA", o.getController<ofxOceanodeOSCController>());
    o.registerModel<ildaDebugger>("ILDA", ofxOceanodeNodeRegistrationFlags::Sink, ildaController);
}
void registerType(ofxOceanode &o){
    
}
void registerScope(ofxOceanode &o){
    
}
std::shared_ptr<ildaController> addController(ofxOceanode &o){
    return o.addController<ildaController>();
}
void registerCollection(ofxOceanode &o){
    registerModels(o, ofxOceanodeLaser::addController(o));
    registerType(o);
    registerScope(o);
}
}
