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
        laser.resetAllLasersToDefault();
        zone = laser.addZone(0, 0, 800, 800);
        laser.getLaser(0)->addZone(zone);
        laser.getLaser(0)->getLaserZoneForZoneId(zone)->zoneTransformQuad.setDst(ofRectangle(0,0,800,800));
        
        // Override ofxLaser's hard-coded safety default of 0.2; we want full brightness on start.
        laser.globalBrightness = 1.0f;
        
        dacAssigner = &laser.dacAssigner;
        dacAssigner->updateDacList();
        
        // Startup re-arm: when ofxLaser::Manager loads its persisted settings,
        // the ofParameter<bool> `armed` is restored via ofDeserialize, which
        // writes the value directly into the parameter storage WITHOUT calling
        // ofParameter::set(). That means the listener
        //   armed.addListener(this, &ofxLaser::Laser::setDacArmed)
        // (see ofxLaserLaser.cpp) never fires at load time, so the underlying
        // DAC stays disarmed even though the GUI shows "Armed = true".
        // Additionally, Laser::setDac() explicitly forces `armed = false` when
        // a DAC is attached, which compounds the mismatch.
        //
        // We defer the re-arm to the first update() call (see below) so that
        // any DAC enumeration / assignment from saved settings has had a chance
        // to complete. Until then, we just remember we need to do it.
        firstUpdate = true;
        
        freeze = false;
        
        pointDraggingIndex = -1;
        ofJson warpPointsJson = ofLoadJson("warpPoints.json");
        if(!warpPointsJson.empty()){
            warpPoints.resize(4);
            warpPoints[0].x = warpPointsJson["p0"]["x"];
            warpPoints[0].y = warpPointsJson["p0"]["y"];
            
            warpPoints[1].x = warpPointsJson["p1"]["x"];
            warpPoints[1].y = warpPointsJson["p1"]["y"];
            
            warpPoints[2].x = warpPointsJson["p2"]["x"];
            warpPoints[2].y = warpPointsJson["p2"]["y"];
            
            warpPoints[3].x = warpPointsJson["p3"]["x"];
            warpPoints[3].y = warpPointsJson["p3"]["y"];
//            laser.getLaser(laser.getNumLasers()-1).getSortedOutputZones()[0]->zoneTransformQuad.setDstCorners(warpPoints[0] * 800, warpPoints[1] * 800, warpPoints[2] * 800, warpPoints[3] * 800);
        }
        else{
            warpPoints = {glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(0, 1), glm::vec2(1, 1)};
        }
    };
    ~ildaController(){};
    
    void update(){
        // On the first update tick after construction, force-retrigger the
        // `armed` ofParameter listener for any laser whose persisted state is
        // armed==true. Without this self-assignment, ofDeserialize silently
        // restores armed to true in the parameter storage but never invokes
        // ofxLaser::Laser::setDacArmed(), so the DAC is left disarmed and the
        // laser produces no output until the user manually toggles the GUI
        // checkbox. The self-assignment goes through ofParameter::set() which
        // does fire the listener, properly calling dac->setArmed(true).
        if(firstUpdate && laser.getNumLasers() > 0){
            for(int i = 0; i < laser.getNumLasers(); i++){
                auto& projectorRef = laser.getLaser(i);
                if(projectorRef->armed.get()){
                    projectorRef->armed = projectorRef->armed.get(); // force listener re-fire
                }
            }
            firstUpdate = false;
        }
        
        laser.send();
//        if(!freeze){
            laser.update();
//        }
  
 
    }
    
    void draw(){
		//ImGui::Text(("GetNumLasers : " + ofToString(laser.getNumLasers()).c_str()));
		ImGui::Text("GetNumLasers : %d",laser.getNumLasers());
		ImGui::Checkbox("Freeze", &freeze);
        if(ImGui::SliderFloat("Master Intensity", (float*)&laser.globalBrightness.get(), 0.0f, 1.0f)){
            //newValue.notify(f);
        }
        for(int i = 0; i < laser.getNumLasers(); i++){
            auto &projectorRef = laser.getLaser(i);
            if(ImGui::TreeNode(projectorRef->getLabel().c_str())){
                const vector<std::shared_ptr<ofxLaser::DacData>>& dacList = dacAssigner->getAvailableDacList();
                    
                if (ImGui::BeginListBox("##listbox", glm::vec2(0, 0))){
                    
                    if(dacList.empty()) {
                        
                        ImGui::Selectable("No laser controllers found", false, ImGuiSelectableFlags_Disabled );
                        
                    } else {
                        // add a combo box item for every element in the list
                        for(const std::shared_ptr<ofxLaser::DacData>& dacdata : dacList) {
                            
                            // get the dac label (usually type + unique ID)
                            string itemlabel = dacdata->getLabel();
                            
                            ImGuiSelectableFlags selectableflags = 0;
                            
                            if(!dacdata->available) {
                                // ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5);
                                //itemlabel += " - no longer available";
                                selectableflags|=ImGuiSelectableFlags_Disabled;
                            } else {
                                //
                            }
                            // if this dac is assigned to a laser, show which laser
                            //  - this could be done at the other end?
                            
                            if (ImGui::Selectable(itemlabel.c_str(), (dacdata->assignedLaser == projectorRef), selectableflags)) {
                                // then select dac
                                // TODO : show a warning yes / no if :
                                //      - we already are connected to a DAC
                                //      - the chosen DAC is already being used by another laser
                                dacAssigner->assignToLaser(dacdata->getLabel(), projectorRef);
                            }
                            
                            if(dacdata->assignedLaser != nullptr) {
                                ImGui::SameLine(210 - 10);
                                string label =" > " + dacdata->assignedLaser->getLabel();
                                ImGui::Text("%s",label.c_str());
                            }
                            
                            //ImGui::PopStyleVar();
                        }
                    }
                    //    if (is_selected)
                    //       ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
                    //ImGui::EndCombo();
                    ImGui::EndListBox();
                }
                if(ImGui::Button("Refresh controller list")) {
                    dacAssigner->updateDacList();
                    
                }
                
                ImGui::Separator();
                
                //            gui->add(armed.set("ARMED", false));
                //            armed.addListener(this, &ofxLaser::Projector::setArmed);
                if(ImGui::Checkbox("Armed", (bool *)&projectorRef->armed.get())){
					//Trigger event
                    projectorRef->armed = projectorRef->armed;
                }
                
                //            gui->add(testPattern.set("Test Pattern", 0,0,numTestPatterns));
                //            gui->add(resetDac.set("Reset DAC", false));
//                ImGui::SliderInt("Test Pattern", (int *)&projectorRef->testPattern.get(), 0, projectorRef->numTestPatterns);
                //ImGui::Checkbox("Reset Dac", (bool *)&projectorRef->resetDac.get());
                
                //            projectorparams.add(laserOnWhileMoving.set("Laser on while moving", false));
                ImGui::Checkbox("Laser on while moving", (bool *)&projectorRef->laserOnWhileMoving.get());
                
                //            projectorparams.add(flipX.set("Flip X", false));
                //            projectorparams.add(flipY.set("Flip Y",false));
                ImGui::Checkbox("Flip X", (bool *)&projectorRef->flipX.get());
                ImGui::Checkbox("Flip Y", (bool *)&projectorRef->flipY.get());
                //            projectorparams.add(rotation.set("Output rotation",0,-90,90));
                ImGui::SliderInt("Output Rotation", (int *)&projectorRef->rotation.get(), -180, 180);
                
                //            projectorparams.add(outputOffset.set("Output position offset", glm::vec2(0,0), glm::vec2(-20,-20),glm::vec2(20,20)));
                ImGui::SliderFloat2("Output position offset", (float *)&projectorRef->outputOffset.get(), -20, 20);
                
                if(ImGui::TreeNode("Advanced")){
//                    advanced.add(speedMultiplier.set("Speed multiplier", 1,0.01,2));
//                    ImGui::SliderFloat("Speed multiplier", (float *)&projectorRef->speedMultiplier.get(), 0.01 ,2);
                    //            advanced.add(smoothHomePosition.set("Smooth home position", true));
                    ImGui::Checkbox("Smooth home position", (bool *)&projectorRef->smoothHomePosition.get());
                    //            advanced.add(sortShapes.set("Optimise shape draw order", true));
                    ImGui::Checkbox("Optimise shape draw order", (bool *)&projectorRef->sortShapes.get());
					
					ImGui::Checkbox("Experimental shape sorting", (bool *)&projectorRef->newShapeSortMethod.get());
//					ofParameter<bool> newShapeSortMethod;
//					ofParameter<bool> alwaysClockwise;
                    //            advanced.add(targetFramerate.set("Target framerate (experimental)", 25, 23, 120));
                    ImGui::SliderFloat("Target framerate (experimental)", (float *)&projectorRef->targetFramerate.get(), 23, 120);
                    //            advanced.add(syncToTargetFramerate.set("Sync to Target framerate", false));
                    ImGui::Checkbox("Sync to Target framerate", (bool *)&projectorRef->syncToTargetFramerate.get());
                    //            advanced.add(syncShift.set("Sync shift", 0, -50, 50));
                    ImGui::SliderInt("Sync shift", (int *)&projectorRef->syncShift.get(), -50, 50);
                    
                    ImGui::TreePop();
                }
                
                if(ImGui::TreeNode("Render profiles")){
                    // TODO set up default profiles
					ofxLaser::RenderProfile& fast = projectorRef->getRenderProfile(OFXLASER_PROFILE_FAST);//projectorRef->renderProfiles.at(OFXLASER_PROFILE_FAST);
                    ofxLaser::RenderProfile& defaultProfile = projectorRef->getRenderProfile(OFXLASER_PROFILE_DEFAULT);
                    ofxLaser::RenderProfile& detail = projectorRef->getRenderProfile(OFXLASER_PROFILE_DETAIL);
                    
                    
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
                    
                    ImGui::SliderFloat("Red 100", (float *)&projectorRef->colourSettings.red100.get(), 0, 1);
                    ImGui::SliderFloat("Red 75", (float *)&projectorRef->colourSettings.red75.get(), 0, 1);
                    ImGui::SliderFloat("Red 50", (float *)&projectorRef->colourSettings.red50.get(), 0, 1);
                    ImGui::SliderFloat("Red 25", (float *)&projectorRef->colourSettings.red25.get(), 0, 1);
                    ImGui::SliderFloat("Red 0", (float *)&projectorRef->colourSettings.red0.get(), 0, 1);
                    
                    //
                    //            colourparams.add(green100.set("green 100", 1,0,1));
                    //            colourparams.add(green75.set("green 75", 0.75,0,1));
                    //            colourparams.add(green50.set("green 50", 0.5,0,1));
                    //            colourparams.add(green25.set("green 25", 0.25,0,1));
                    //            colourparams.add(green0.set("green 0", 0,0,1));
                    
                    ImGui::SliderFloat("green 100", (float *)&projectorRef->colourSettings.green100.get(), 0, 1);
                    ImGui::SliderFloat("green 75", (float *)&projectorRef->colourSettings.green75.get(), 0, 1);
                    ImGui::SliderFloat("green 50", (float *)&projectorRef->colourSettings.green50.get(), 0, 1);
                    ImGui::SliderFloat("green 25", (float *)&projectorRef->colourSettings.green25.get(), 0, 1);
                    ImGui::SliderFloat("green 0", (float *)&projectorRef->colourSettings.green0.get(), 0, 1);
                    //
                    //            colourparams.add(blue100.set("blue 100", 1,0,1));
                    //            colourparams.add(blue75.set("blue 75", 0.75,0,1));
                    //            colourparams.add(blue50.set("blue 50", 0.5,0,1));
                    //            colourparams.add(blue25.set("blue 25", 0.25,0,1));
                    //            colourparams.add(blue0.set("blue 0", 0,0,1));
                    
                    ImGui::SliderFloat("blue 100", (float *)&projectorRef->colourSettings.blue100.get(), 0, 1);
                    ImGui::SliderFloat("blue 75", (float *)&projectorRef->colourSettings.blue75.get(), 0, 1);
                    ImGui::SliderFloat("blue 50", (float *)&projectorRef->colourSettings.blue50.get(), 0, 1);
                    ImGui::SliderFloat("blue 25", (float *)&projectorRef->colourSettings.blue25.get(), 0, 1);
                    ImGui::SliderFloat("blue 0", (float *)&projectorRef->colourSettings.blue0.get(), 0, 1);
                    
                    ImGui::TreePop();
                }
                
                if(ImGui::TreeNode("Warping")){
//                    auto screenPos = ImGui::GetCursorScreenPos();
//                    //ImGui::Vec2 size;
//                    ImVec2 screenSize = ImVec2(100, 100);
                    
                    bool pointsUpdated = false;
                    
                    float availableWidth = ImGui::GetContentRegionAvail().x;
                    float squareSize = availableWidth;
                    
                    // Iniciem un Child region perquè reservi espai
                    if(ImGui::BeginChild("WarpCanvas", ImVec2(squareSize, squareSize), true, ImGuiWindowFlags_NoScrollWithMouse)){
                        
                        // Coordenades de dibuix absolutes
                        ImVec2 screenPos = ImGui::GetCursorScreenPos();
                        ImVec2 screenSize = ImGui::GetContentRegionAvail(); // = squareSize, squareSize
                        
                        
                        if(ImGui::IsWindowHovered()){
                            glm::vec2 normPos = (ImGui::GetMousePos() - screenPos) / screenSize;
                            if(normPos.x>1.0) normPos.x = 1.0;
                            if(normPos.x<0.0) normPos.x = 0.0;
                            if(normPos.y>1.0) normPos.y = 1.0;
                            if(normPos.y<0.0) normPos.y = 0.0;
                            
                            bool mouseClicked = false;
                            if(ImGui::IsMouseClicked(0)){
                                mouseClicked = true;
                            }
                            else if(ImGui::IsMouseClicked(1)){
                                mouseClicked = true;
                            }
                            if(mouseClicked){
                                bool foundPoint = false;
                                for(int i = warpPoints.size()-1; i >= 0 && !foundPoint ; i--){
                                    auto point = (warpPoints[i] * screenSize) + screenPos;
                                    if(glm::distance(glm::vec2(ImGui::GetMousePos()), point) < 10){
                                        pointDraggingIndex = i;
                                        foundPoint = true;
                                    }
                                }
                                if(!foundPoint){
                                    pointDraggingIndex = -1;
                                }
                            }
                            else if(ImGui::IsMouseDragging(0)){
                                if(pointDraggingIndex != -1){
                                    if(ImGui::GetIO().KeyAlt){
                                        warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / (screenSize * ImVec2(100, 100));
                                        pointsUpdated = true;
                                    }else{
                                        warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / screenSize;
                                        pointsUpdated = true;
                                    }
                                }
                            }else if(pointDraggingIndex != -1){
                                float moveAmt = ImGui::GetIO().KeyAlt ? 0.00001 : 0.001;
                                if(ImGui::IsKeyDown(ImGuiKey_LeftArrow)){
                                    warpPoints[pointDraggingIndex] += glm::vec2(-moveAmt, 0);
                                    pointsUpdated = true;
                                }else if(ImGui::IsKeyDown(ImGuiKey_RightArrow)){
                                    warpPoints[pointDraggingIndex] += glm::vec2(moveAmt, 0);
                                    pointsUpdated = true;
                                }if(ImGui::IsKeyDown(ImGuiKey_UpArrow)){
                                    warpPoints[pointDraggingIndex] += glm::vec2(0, -moveAmt);
                                    pointsUpdated = true;
                                }else if(ImGui::IsKeyDown(ImGuiKey_DownArrow)){
                                    warpPoints[pointDraggingIndex] += glm::vec2(0, moveAmt);
                                    pointsUpdated = true;
                                }
                            }
                            if(ImGui::IsKeyPressed(ImGuiKey_Tab)){
                                pointDraggingIndex = (pointDraggingIndex + 1) % warpPoints.size();
                            }
                            
                            if(warpPoints[pointDraggingIndex].x>1.0) warpPoints[pointDraggingIndex].x=1.0;
                            if(warpPoints[pointDraggingIndex].x<0.0) warpPoints[pointDraggingIndex].x=0.0;
                            if(warpPoints[pointDraggingIndex].y>1.0) warpPoints[pointDraggingIndex].y=1.0;
                            if(warpPoints[pointDraggingIndex].y<0.0) warpPoints[pointDraggingIndex].y=0.0;
                        }
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        vector<float> x_t(warpPoints.size());
                        vector<float> y_t(warpPoints.size());
                        for(int i = 0; i < warpPoints.size(); i++){
                            if(pointDraggingIndex == i) draw_list->AddCircle((warpPoints[i] * screenSize) + screenPos, 10, IM_COL32(255, 255, 0, 255));
                            else draw_list->AddCircle((warpPoints[i] * screenSize) + screenPos, 10, IM_COL32(255, 128, 0, 255));
                            string numString = ofToString(i);
                            draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), (warpPoints[i] * screenSize) + screenPos - glm::vec2(5, 5), IM_COL32(255,255,255,255), numString.c_str(), numString.c_str()+ (i < 10 ? 1 : 2));
                            x_t[i] = warpPoints[i].x;
                            y_t[i] = warpPoints[i].y;
                        }
                        draw_list->ChannelsMerge();
                        
                        if(pointsUpdated){
                                laser.getLaser(i)->getLaserZoneForZoneId(zone)->zoneTransformQuad.setDstCorners(warpPoints[0] * 800, warpPoints[1] * 800, warpPoints[2] * 800, warpPoints[3] * 800);
                        }
                        
                        ImGui::EndChild(); // tanca canvas
                    }
                    
                    ImGui::TreePop();
                }
                
                
                if(ImGui::TreeNode("Distortion")){
//                    auto &zoneQuadTransform =  laser.getLaser(laser.getNumLasers()-1).getSortedOutputZones()[0]->zoneTransformQuad;
//                    ImGui::SliderFloat2("Shear", (float *)&zoneQuadTransform.shear, -2, 2);
//                    ImGui::SliderFloat2("Keystone", (float *)&zoneQuadTransform.keystone, -2, 2);
//                    ImGui::SliderFloat2("Linearity", (float *)&zoneQuadTransform.linearity, -2, 2);
//                    ImGui::SliderFloat2("Bow", (float *)&zoneQuadTransform.bow, -2, 2);
//                    ImGui::SliderFloat2("Pincushion", (float *)&zoneQuadTransform.pincushion, -2, 2);
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

		if (ImGui::Button("Save Config")) {
			laser.saveSettings();
            ofJson warpPointsJson;
            warpPointsJson["p0"]["x"] = warpPoints[0].x;
            warpPointsJson["p0"]["y"] = warpPoints[0].y;
            
            warpPointsJson["p1"]["x"] = warpPoints[1].x;
            warpPointsJson["p1"]["y"] = warpPoints[1].y;
            
            warpPointsJson["p2"]["x"] = warpPoints[2].x;
            warpPointsJson["p2"]["y"] = warpPoints[2].y;
            
            warpPointsJson["p3"]["x"] = warpPoints[3].x;
            warpPointsJson["p3"]["y"] = warpPoints[3].y;
            
            ofSavePrettyJson("warpPoints.json", warpPointsJson);
		}
    }
    
    std::shared_ptr<ofxLaser::Laser>& addLaser(){
        //laser.addLaser(dac);
		//laser.createAndAddLaser();

        return laser.getLaser(laser.getNumLasers()-1);
    }
    
    ofxLaser::Manager& getManager(){
        return laser;
    }
    
    vector<ofxLaser::Point> getAllLaserPoints(){
        vector<ofxLaser::Point> points;
        for(auto &l : laser.getLasers()){
            vector<ofxLaser::Point> newPoints = l->getLaserPoints();
            points.insert(points.end(), newPoints.begin(), newPoints.end());
        }
        return points;
    }
    
    bool isFrozen(){return freeze;};
    
    ofEvent<float> newValue;
private:
    ofxLaser::Manager laser;
    ofxLaser::DacAssigner* dacAssigner;
    ofxLaser::ZoneId zone;

	ofParameter<void> saveConfig;
	   bool freeze;
	   bool firstUpdate;
    
    vector<glm::vec2> warpPoints;
    int pointDraggingIndex;
};


#endif /* testController_h */
