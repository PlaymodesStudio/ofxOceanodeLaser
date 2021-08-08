//
//  ildaController.h
//  example-basic
//
//  Created by Eduard Frigola Bagué on 10/02/2021.
//

#ifndef testController_h
#define testController_h

#include "ofxOceanodeBaseController.h"
#include "imgui.h"
#include "ofxLaserManager.h"

class ildaController : public ofxOceanodeBaseController{
public:
    ildaController() : ofxOceanodeBaseController("ILDA"){
        //laser.globalBrightness.set("Master Intensity", 0.1,0,1);
        laser.setCanvasSize(800, 800);
		laser.globalBrightness = 1;
    };
    ~ildaController(){};
    
    void update(){
        laser.send();
        laser.update();
    }
    
    void draw(){
        if(ImGui::SliderFloat("Master Intensity", (float*)&laser.globalBrightness.get(), 0.0f, 1.0f)){
            //newValue.notify(f);
        }
        for(int i = 0; i < laser.getNumLasers(); i++){
            auto &projectorRef = laser.getLaser(i);
            if(ImGui::TreeNode(projectorRef.getLabel().c_str())){
                char * cString = new char[256];
                strcpy(cString, "192.168.1.50");
                if (ImGui::InputText("IP", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue)){
                    //projectorRef.setup(cString);
                    //Send setup to DAC?
                }
                //            gui->add(armed.set("ARMED", false));
                //            armed.addListener(this, &ofxLaser::Projector::setArmed);
                if(ImGui::Checkbox("Armed", (bool *)&projectorRef.armed.get())){
					//Trigger event
                    projectorRef.armed = projectorRef.armed;
                }
                
                //            gui->add(testPattern.set("Test Pattern", 0,0,numTestPatterns));
                //            gui->add(resetDac.set("Reset DAC", false));
                ImGui::SliderInt("Test Pattern", (int *)&projectorRef.testPattern.get(), 0, projectorRef.numTestPatterns);
                //ImGui::Checkbox("Reset Dac", (bool *)&projectorRef.resetDac.get());
                
                //            projectorparams.add(laserOnWhileMoving.set("Laser on while moving", false));
                ImGui::Checkbox("Laser on while moving", (bool *)&projectorRef.laserOnWhileMoving.get());
                
                //            projectorparams.add(flipX.set("Flip X", false));
                //            projectorparams.add(flipY.set("Flip Y",false));
                ImGui::Checkbox("Flip X", (bool *)&projectorRef.flipX.get());
                ImGui::Checkbox("Flip Y", (bool *)&projectorRef.flipY.get());
                //            projectorparams.add(rotation.set("Output rotation",0,-90,90));
                ImGui::SliderInt("Output Rotation", (int *)&projectorRef.rotation.get(), -180, 180);
                
                //            projectorparams.add(outputOffset.set("Output position offset", glm::vec2(0,0), glm::vec2(-20,-20),glm::vec2(20,20)));
                ImGui::SliderFloat2("Output position offset", (float *)&projectorRef.outputOffset.get(), -20, 20);
                
                if(ImGui::TreeNode("Advanced")){
//                    advanced.add(speedMultiplier.set("Speed multiplier", 1,0.01,2));
                    ImGui::SliderFloat("Speed multiplier", (float *)&projectorRef.speedMultiplier.get(), 0.01 ,2);
                    //            advanced.add(smoothHomePosition.set("Smooth home position", true));
                    ImGui::Checkbox("Smooth home position", (bool *)&projectorRef.smoothHomePosition.get());
                    //            advanced.add(sortShapes.set("Optimise shape draw order", true));
                    ImGui::Checkbox("Optimise shape draw order", (bool *)&projectorRef.sortShapes.get());
					
					ImGui::Checkbox("Experimental shape sorting", (bool *)&projectorRef.newShapeSortMethod.get());
//					ofParameter<bool> newShapeSortMethod;
//					ofParameter<bool> alwaysClockwise;
                    //            advanced.add(targetFramerate.set("Target framerate (experimental)", 25, 23, 120));
                    ImGui::SliderFloat("Target framerate (experimental)", (float *)&projectorRef.targetFramerate.get(), 23, 120);
                    //            advanced.add(syncToTargetFramerate.set("Sync to Target framerate", false));
                    ImGui::Checkbox("Sync to Target framerate", (bool *)&projectorRef.syncToTargetFramerate.get());
                    //            advanced.add(syncShift.set("Sync shift", 0, -50, 50));
                    ImGui::SliderInt("Sync shift", (int *)&projectorRef.syncShift.get(), -50, 50);
                    
                    ImGui::TreePop();
                }
                
                if(ImGui::TreeNode("Render profiles")){
                    // TODO set up default profiles
					ofxLaser::RenderProfile& fast = projectorRef.getRenderProfile(OFXLASER_PROFILE_FAST);//projectorRef.renderProfiles.at(OFXLASER_PROFILE_FAST);
                    ofxLaser::RenderProfile& defaultProfile = projectorRef.getRenderProfile(OFXLASER_PROFILE_DEFAULT);
                    ofxLaser::RenderProfile& detail = projectorRef.getRenderProfile(OFXLASER_PROFILE_DETAIL);
                    
                    //            renderparams.add(defaultProfile.params);
                    if(ImGui::TreeNode("Default")){
                        //params.add(speed.set("speed",2,1,40));
                        ImGui::SliderFloat("speed", (float *)&defaultProfile.speed.get(), 1, 40);
                        //params.add(acceleration.set("acceleration",1,0.01,4));
                        ImGui::SliderFloat("acceleration", (float *)&defaultProfile.acceleration.get(), 0.01, 4);
                        //params.add(cornerThreshold.set("corner threshold",90,0,180));
                        ImGui::SliderFloat("corner threshold", (float *)&defaultProfile.cornerThreshold.get(), 0, 180);
                        //params.add(dotMaxPoints.set("dot max points", 2, 0, 100));
                        ImGui::SliderInt("dot max points", (int *)&defaultProfile.dotMaxPoints.get(), 0, 100);
                        
                        ImGui::TreePop();
                    }
                    if(ImGui::TreeNode("Fast")){
                        //params.add(speed.set("speed",2,1,40));
                        ImGui::SliderFloat("speed", (float *)&fast.speed.get(), 1, 40);
                        //params.add(acceleration.set("acceleration",1,0.01,4));
                        ImGui::SliderFloat("acceleration", (float *)&fast.acceleration.get(), 0.01, 4);
                        //params.add(cornerThreshold.set("corner threshold",90,0,180));
                        ImGui::SliderFloat("corner threshold", (float *)&fast.cornerThreshold.get(), 0, 180);
                        //params.add(dotMaxPoints.set("dot max points", 2, 0, 100));
                        ImGui::SliderInt("dot max points", (int *)&fast.dotMaxPoints.get(), 0, 100);
                        
                        ImGui::TreePop();
                    }
                    if(ImGui::TreeNode("High quality")){
                        //params.add(speed.set("speed",2,1,40));
                        ImGui::SliderFloat("speed", (float *)&detail.speed.get(), 1, 40);
                        //params.add(acceleration.set("acceleration",1,0.01,4));
                        ImGui::SliderFloat("acceleration", (float *)&detail.acceleration.get(), 0.01, 4);
                        //params.add(cornerThreshold.set("corner threshold",90,0,180));
                        ImGui::SliderFloat("corner threshold", (float *)&detail.cornerThreshold.get(), 0, 180);
                        //params.add(dotMaxPoints.set("dot max points", 2, 0, 100));
                        ImGui::SliderInt("dot max points", (int *)&detail.dotMaxPoints.get(), 0, 100);
                        
                        ImGui::TreePop();
                    }
                    
                    //            renderparams.add(fast.params);
                    
                    //            renderparams.add(detail.params);
                    ImGui::TreePop();
                }
                
				//TODO: Fix color calibration
                if(ImGui::TreeNode("Color Calibration")){
                    //            colourparams.add(red100.set("red 100", 1,0,1));
                    //            colourparams.add(red75.set("red 75", 0.75,0,1));
                    //            colourparams.add(red50.set("red 50", 0.5,0,1));
                    //            colourparams.add(red25.set("red 25", 0.25,0,1));
                    //            colourparams.add(red0.set("red 0", 0,0,1));
                    //TODO: Fer amb curves
                    
                    ImGui::SliderFloat("Red 100", (float *)&projectorRef.colourSettings.red100.get(), 0, 1);
                    ImGui::SliderFloat("Red 75", (float *)&projectorRef.colourSettings.red75.get(), 0, 1);
                    ImGui::SliderFloat("Red 50", (float *)&projectorRef.colourSettings.red50.get(), 0, 1);
                    ImGui::SliderFloat("Red 25", (float *)&projectorRef.colourSettings.red25.get(), 0, 1);
                    ImGui::SliderFloat("Red 0", (float *)&projectorRef.colourSettings.red0.get(), 0, 1);
                    
                    //
                    //            colourparams.add(green100.set("green 100", 1,0,1));
                    //            colourparams.add(green75.set("green 75", 0.75,0,1));
                    //            colourparams.add(green50.set("green 50", 0.5,0,1));
                    //            colourparams.add(green25.set("green 25", 0.25,0,1));
                    //            colourparams.add(green0.set("green 0", 0,0,1));
                    
                    ImGui::SliderFloat("green 100", (float *)&projectorRef.colourSettings.green100.get(), 0, 1);
                    ImGui::SliderFloat("green 75", (float *)&projectorRef.colourSettings.green75.get(), 0, 1);
                    ImGui::SliderFloat("green 50", (float *)&projectorRef.colourSettings.green50.get(), 0, 1);
                    ImGui::SliderFloat("green 25", (float *)&projectorRef.colourSettings.green25.get(), 0, 1);
                    ImGui::SliderFloat("green 0", (float *)&projectorRef.colourSettings.green0.get(), 0, 1);
                    //
                    //            colourparams.add(blue100.set("blue 100", 1,0,1));
                    //            colourparams.add(blue75.set("blue 75", 0.75,0,1));
                    //            colourparams.add(blue50.set("blue 50", 0.5,0,1));
                    //            colourparams.add(blue25.set("blue 25", 0.25,0,1));
                    //            colourparams.add(blue0.set("blue 0", 0,0,1));
                    
                    ImGui::SliderFloat("blue 100", (float *)&projectorRef.colourSettings.blue100.get(), 0, 1);
                    ImGui::SliderFloat("blue 75", (float *)&projectorRef.colourSettings.blue75.get(), 0, 1);
                    ImGui::SliderFloat("blue 50", (float *)&projectorRef.colourSettings.blue50.get(), 0, 1);
                    ImGui::SliderFloat("blue 25", (float *)&projectorRef.colourSettings.blue25.get(), 0, 1);
                    ImGui::SliderFloat("blue 0", (float *)&projectorRef.colourSettings.blue0.get(), 0, 1);
                    
                    ImGui::TreePop();
                }
                
                
                ImGui::TreePop();
            }
              
//            ofParameterGroup colourparams;
//            colourparams.setName("Colour calibration");
//            
//
//            
//            
        }
    }
    
    ofxLaser::Laser* addLaser(ofxLaser::DacBase& dac){
        //laser.addLaser(dac);
		//laser.createAndAddLaser();
        return &laser.getLaser(laser.getNumLasers()-1);
    }
    
    ofxLaser::Manager& getManager(){
        return laser;
    }
    
    ofEvent<float> newValue;
private:
    ofxLaser::Manager laser;
};


#endif /* testController_h */
