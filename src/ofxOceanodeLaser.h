//
//  ofxOceanodeLaser.h
//  example
//
//  Created by Eduard Frigola Bagué on 10/02/2021.
//

#ifndef ofxOceanodeLaser_h
#define ofxOceanodeLaser_h

// Declarations only: the node headers and the bodies live in ofxOceanodeLaser.cpp, so
// including this header does not compile every node of the addon again.

#include <memory>

class ofxOceanode;
class ildaController;

namespace ofxOceanodeLaser{

void registerModels(ofxOceanode &o, std::shared_ptr<ildaController> ildaController);
void registerType(ofxOceanode &o);
void registerScope(ofxOceanode &o);
std::shared_ptr<ildaController> addController(ofxOceanode &o);
void registerCollection(ofxOceanode &o);

}

#endif /* ofxOceanodeLaser_h */
