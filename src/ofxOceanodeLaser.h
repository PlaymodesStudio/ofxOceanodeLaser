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

#include "ofxOceanode.h"

namespace ofxOceanodeLaser{
static void registerModels(ofxOceanode &o, shared_ptr<ildaController> ildaController){
    o.registerModel<ildaProjector>("ILDA", ildaController);
    o.registerModel<ildaShape>("ILDA", ildaController);
	o.registerModel<polyOscSender>("ILDA", o.getController<ofxOceanodeOSCController>());
//    o.registerModel<oscillatorTexture>("Textures");
//    o.registerModel<chaoticOscillatorTexture>("Textures");
//    o.registerModel<imageLoader>("Textures");
//    o.registerModel<mixer>("Textures");
//    o.registerModel<interactiveCanvas>("Textures");
}
static void registerType(ofxOceanode &o){
//    o.registerType<ofTexture*>();//"Texture");
}
static void registerScope(ofxOceanode &o){
//    o.registerScope<ofTexture*>([](ofxOceanodeAbstractParameter *p, ImVec2 size){
//        auto tex = p->cast<ofTexture*>().getParameter().get();
//        auto size2 = ImGui::GetContentRegionAvail();
//
//        if(tex != nullptr){
//            ImTextureID textureID = (ImTextureID)(uintptr_t)tex->texData.textureID;
//            ImGui::Image(textureID, size2);
//        }
//    });
}
static void registerCollection(ofxOceanode &o){
    //registerModels(o);
    registerType(o);
    registerScope(o);
}
static shared_ptr<ildaController> addController(ofxOceanode &o){
     return o.addController<ildaController>();
}
}

#endif /* ofxOceanodeLaser_h */
