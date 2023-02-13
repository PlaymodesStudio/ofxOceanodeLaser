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

namespace ofxOceanodeLaser{
static void registerModels(ofxOceanode &o, shared_ptr<ildaController> ildaController){
    o.registerModel<ildaProjector>("ILDA", ildaController);
    o.registerModel<ildaShape>("ILDA", ildaController);
    o.registerModel<polyOscSender>("ILDA", o.getController<ofxOceanodeOSCController>());
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
