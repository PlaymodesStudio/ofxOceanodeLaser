#include "ofApp.h"
#include "ofxOceanodeLaser.h"
#include "ofxOceanodeVectorGraphics.h"
#include "oscSender.h"

class fatlineToVector : public ofxOceanodeNodeModel{
public:
    fatlineToVector() : ofxOceanodeNodeModel("Fat to Vec"){}
    
    void setup(){
        addParameter(width.set("Width", 100, 1, 10000));
        addParameter(height.set("Height", 100, 1, 10000));
        addParameter(fatline.set("Fatline", {ofxFatLine()}));
        addParameter(output.set("Output", {0}, {0}, {1}));
        
        listener = fatline.newListener([this](vector<ofxFatLine> &vfl){
            int size = 0;
            for(auto &fat : vfl){
                size += fat.size();
            }
            vector<float> tempOut(size * 5);
            int i = 0;
            for(auto &fat : vfl){
                for(int j = 0; j < fat.size(); j++){
                    tempOut[i+(j*5)] = fat[j].x / width;
                    tempOut[i+(j*5)+1] = fat[j].y / height;
                    tempOut[i+(j*5)+2] = fat.getColor(j).r / 255.0;
                    tempOut[i+(j*5)+3] = fat.getColor(j).g / 255.0;
                    tempOut[i+(j*5)+4] = fat.getColor(j).b / 255.0;
                }
                i += (fat.size()*5);
            }
            output = tempOut;
        });
    }
    
private:
    ofEventListener listener;
    
    ofParameter<int> width, height;
    ofParameter<vector<ofxFatLine>> fatline;
    ofParameter<vector<float>> output;
};

//--------------------------------------------------------------
void ofApp::setup(){
    ofDisableArbTex();
    oceanode.setup();
    ofxOceanodeLaser::registerCollection(oceanode);
    ofxOceanodeVectorGraphics::registerCollection(oceanode);
    
    oceanode.registerModel<fatlineToVector>("OSC");
    oceanode.registerModel<oscSender>("OSC", "ILDA", "vf:1/Points:0:1, vf:2/Points:0:1", oceanode.getController<ofxOceanodeOSCController>());
}

//--------------------------------------------------------------
void ofApp::update(){
    oceanode.update();
}

//--------------------------------------------------------------
void ofApp::draw(){
    oceanode.draw();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
