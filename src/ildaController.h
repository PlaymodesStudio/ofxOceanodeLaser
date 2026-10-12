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
#include "ofxFatLine.h"
#include <array>

class ildaController : public ofxOceanodeBaseController{
public:
    static constexpr float ZoneSize = 800.0f;

    struct ZoneConfig {
        string stableId;
        string name;
        ofxLaser::ZoneId laserZoneId;
        vector<bool> assigned;
        vector<array<glm::vec2, 4>> warps;
    };

    ildaController() : ofxOceanodeBaseController("ILDA"){
        // ------------------------------------------------------------------
        // Strategy A: detach ofxLaser::Manager from the global oF input
        // events.
        //
        // Background:
        // ofxLaser::Manager registers itself as a listener on
        // ofEvents().mouse{Entered,Exited,Moved,Pressed,Released,Dragged,
        // Scrolled} and ofEvents().keyPressed (see ofxLaserManager.cpp
        // lines 130-139) at OF_EVENT_ORDER_BEFORE_APP. We do not draw any
        // of ofxLaser's built-in ofxGui/canvas UI here — we only use it as
        // an ILDA output API. However, those listeners still fire on every
        // mouse drag (even drags that happen entirely inside ImGui
        // widgets, because ImGui does not consume oF events by default
        // and ofxLaser's mouseDragged() handler does not gate on
        // isGuiVisible()/isGuiMouseDisabled() the way mousePressed() does).
        //
        // The consequence is that any mouse drag — e.g. dragging the
        // "Master Intensity" slider in this controller's ImGui panel —
        // reaches CanvasViewController::mouseDragged, dirties a
        // MoveablePoly, makes canvasViewController->update() return true
        // next frame, and triggers Manager::update() lines 410-412 to
        // call updateZonesFromUI() (which unconditionally rewrites every
        // InputZone rect) and then scheduleSaveSettings(). After the
        // built-in 1-second debounce, ManagerBase::update() flushes the
        // pending save and rewrites ofxLaser/laserSettings.json on disk.
        // The diff that appears in the file is in the
        // "zoneidobject"/"inputzone" rectangles (not in any parameter we
        // actually touched), because that's where the per-frame UI->model
        // sync pushes values.
        //
        // Since we never draw ofxLaser's GUI in this app, the cleanest
        // fix is to disconnect those listeners entirely. We also flip
        // setGuiVisible(false) and setGuiMouseDisabled(true) for good
        // measure so any future code path inside ofxLaser that DOES
        // consult those flags will also bail out.
        //
        // Legitimate persistence is preserved: explicit save paths
        // (deleteZone, setCanvasSize, LaserMsg::SaveSettings, etc.) and
        // the 1-second debounced flush in ManagerBase::update() still
        // run. We just stop arming that flush from spurious mouse drags.
        // ------------------------------------------------------------------
        laser.setGuiVisible(false);
        laser.setGuiMouseDisabled(true);

        ofRemoveListener(ofEvents().mouseEntered,  &laser, &ofxLaser::Manager::mouseEntered,  OF_EVENT_ORDER_BEFORE_APP);
        ofRemoveListener(ofEvents().mouseExited,   &laser, &ofxLaser::Manager::mouseExited,   OF_EVENT_ORDER_BEFORE_APP);
        ofRemoveListener(ofEvents().mouseMoved,    &laser, &ofxLaser::Manager::mouseMoved,    OF_EVENT_ORDER_BEFORE_APP);
        ofRemoveListener(ofEvents().mousePressed,  &laser, &ofxLaser::Manager::mousePressed,  OF_EVENT_ORDER_BEFORE_APP);
        ofRemoveListener(ofEvents().mouseReleased, &laser, &ofxLaser::Manager::mouseReleased, OF_EVENT_ORDER_BEFORE_APP);
        ofRemoveListener(ofEvents().mouseDragged,  &laser, &ofxLaser::Manager::mouseDragged,  OF_EVENT_ORDER_BEFORE_APP);
        ofRemoveListener(ofEvents().mouseScrolled, &laser, &ofxLaser::Manager::mouseScrolled, OF_EVENT_ORDER_BEFORE_APP);
        ofRemoveListener(ofEvents().keyPressed,    &laser, &ofxLaser::Manager::keyPressed,    OF_EVENT_ORDER_BEFORE_APP);

        // Keep the projectors that Manager already loaded. Recreating them here
        // discarded their armed state and other projector settings (and could
        // leave a second DAC silent). Only the old zone objects need rebuilding.
        for(auto &projector : laser.getLasers()) projector->clearOutputZones();
        laser.getLaserState().canvasTarget->clearZones();
        loadZones();
        
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
        // We defer the re-arm and warp re-application to the first update()
        // call (see below) so that any DAC enumeration / assignment from saved
        // settings has had a chance to complete. Until then, we just remember
        // we need to do it.
        firstUpdate = true;
        
        freeze = false;
        pointDraggingIndex = -1;
    };
    ~ildaController(){ saveZones(); };
    
    void update(){
        // Phase B: swap the write buffer (accumulated this frame by ildaShape
        // publishers) into the read buffer for next frame's consumers, then
        // clear the write buffer for the next frame's publishes.
        std::swap(simulatorReadBuffer, simulatorWriteBuffer);
        simulatorWriteBuffer.clear();

        laser.send();
//        if(!freeze){
            laser.update();
//        }

        // On the first update tick after construction, force-retrigger the
        // `armed` ofParameter listener for any laser whose persisted state is
        // armed==true. Without this self-assignment, ofDeserialize silently
        // restores armed to true in the parameter storage but never invokes
        // ofxLaser::Laser::setDacArmed(), so the DAC is left disarmed and the
        // laser produces no output until the user manually toggles the GUI
        // checkbox. We also re-apply the stored warp here so the output zone
        // matches warpPoints.json before the user touches the UI.
        if(firstUpdate && laser.getNumLasers() > 0){
            for(int i = 0; i < laser.getNumLasers(); i++){
                auto& projectorRef = laser.getLaser(i);
                if(projectorRef->armed.get()){
                    // Setting an ofParameter to its current value does not fire
                    // its listener. Synchronise the DAC explicitly in case it
                    // was disarmed while being assigned/reconnected.
                    bool shouldBeArmed = true;
                    if(projectorRef->hasDac()) projectorRef->getDac()->setArmed(shouldBeArmed);
                }
                for(int zoneIndex = 0; zoneIndex < static_cast<int>(zones.size()); zoneIndex++){
                    applyWarpPointsToLaser(i, zoneIndex);
                }
            }
            firstUpdate = false;
        }
  
 
    }
    
    void draw(){
		//ImGui::Text(("GetNumLasers : " + ofToString(laser.getNumLasers()).c_str()));
		ImGui::Text("GetNumLasers : %d",laser.getNumLasers());
		ImGui::SameLine();
		if(ImGui::Button("Add Laser")) addManagedLaser();
		ImGui::Checkbox("Freeze", &freeze);
        if(ImGui::SliderFloat("Master Intensity", (float*)&laser.globalBrightness.get(), 0.0f, 1.0f)){
            //newValue.notify(f);
        }

        drawZonesGui();

        int laserToDelete = -1;
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
					bool shouldBeArmed = projectorRef->armed.get();
                    if(projectorRef->hasDac()) projectorRef->getDac()->setArmed(shouldBeArmed);
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
                    if(zones.empty()){
                        ImGui::TextDisabled("No zones");
                        ImGui::TreePop();
                        ImGui::TreePop();
                        continue;
                    }
                    selectedZoneIndex = ofClamp(selectedZoneIndex, 0, static_cast<int>(zones.size()) - 1);
                    vector<string> zoneNames = getZoneNames();
                    if(ImGui::BeginCombo("Zone", zoneNames[selectedZoneIndex].c_str())){
                        for(int zoneIndex = 0; zoneIndex < static_cast<int>(zoneNames.size()); zoneIndex++){
                            bool selected = zoneIndex == selectedZoneIndex;
                            if(ImGui::Selectable(zoneNames[zoneIndex].c_str(), selected)) selectedZoneIndex = zoneIndex;
                            if(selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    auto &warpPoints = zones[selectedZoneIndex].warps[i];
                    if(ImGui::Button("Reset Selected Warp")){
                        warpPoints = defaultWarp();
                        pointDraggingIndex = -1;
                        applyWarpPointsToLaser(i, selectedZoneIndex);
                    }
//                    auto screenPos = ImGui::GetCursorScreenPos();
//                    //ImGui::Vec2 size;
//                    ImVec2 screenSize = ImVec2(100, 100);
                    
                    bool pointsUpdated = false;
                    
                    float availableWidth = ImGui::GetContentRegionAvail().x;
                    float squareSize = availableWidth;
                    
                    // Iniciem un Child region perquè reservi espai
                    bool warpCanvasVisible = ImGui::BeginChild("WarpCanvas", ImVec2(squareSize, squareSize), true, ImGuiWindowFlags_NoScrollWithMouse);
                    if(warpCanvasVisible){
                        
                        // Coordenades de dibuix absolutes
                        ImVec2 screenPos = ImGui::GetCursorScreenPos();
                        ImVec2 screenSize = ImGui::GetContentRegionAvail(); // = squareSize, squareSize
                        // Keep all four handles fully inside the child. Previously the
                        // bottom-right handle (point 3) was centred exactly on the child
                        // boundary, which made its hit-testing and dragging unreliable.
                        const float handlePadding = 12.0f;
                        ImVec2 canvasPos = screenPos + ImVec2(handlePadding, handlePadding);
                        ImVec2 canvasSize = screenSize - ImVec2(handlePadding * 2.0f, handlePadding * 2.0f);
                        
                        
                        if(ImGui::IsWindowHovered()){
                            glm::vec2 normPos = (ImGui::GetMousePos() - canvasPos) / canvasSize;
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
                                for(int pointIndex = static_cast<int>(warpPoints.size())-1; pointIndex >= 0 && !foundPoint ; pointIndex--){
                                    auto point = (warpPoints[pointIndex] * canvasSize) + canvasPos;
                                    if(glm::distance(glm::vec2(ImGui::GetMousePos()), point) < 10){
                                        pointDraggingIndex = pointIndex;
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
                                        warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / (canvasSize * ImVec2(100, 100));
                                        pointsUpdated = true;
                                    }else{
                                        warpPoints[pointDraggingIndex] += ImGui::GetIO().MouseDelta / canvasSize;
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
                            if(ImGui::IsKeyPressed(ImGuiKey_Tab) && !warpPoints.empty()){
                                pointDraggingIndex = (pointDraggingIndex + 1) % warpPoints.size();
                            }

                            clampWarpPoint(warpPoints, pointDraggingIndex);
                        }
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        vector<float> x_t(warpPoints.size());
                        vector<float> y_t(warpPoints.size());
                        for(int i = 0; i < warpPoints.size(); i++){
                            if(pointDraggingIndex == i) draw_list->AddCircle((warpPoints[i] * canvasSize) + canvasPos, 10, IM_COL32(255, 255, 0, 255));
                            else draw_list->AddCircle((warpPoints[i] * canvasSize) + canvasPos, 10, IM_COL32(255, 128, 0, 255));
                            string numString = ofToString(i);
                            draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), (warpPoints[i] * canvasSize) + canvasPos - glm::vec2(5, 5), IM_COL32(255,255,255,255), numString.c_str(), numString.c_str()+ (i < 10 ? 1 : 2));
                            x_t[i] = warpPoints[i].x;
                            y_t[i] = warpPoints[i].y;
                        }
                        draw_list->ChannelsMerge();
                        
                        if(pointsUpdated){
                            applyWarpPointsToLaser(i, selectedZoneIndex);
                        }
                    }
                    ImGui::EndChild(); // Always pair BeginChild/EndChild, even when clipped.
                    
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

                if(laser.getNumLasers() > 1 && ImGui::Button("Delete Laser")) laserToDelete = i;
                
                
                ImGui::TreePop();
            }
              
//            ofParameterGroup colourparams;
//            colourparams.setName("Colour calibration");
//            
//
//            
//            
        }

        if(laserToDelete >= 0) deleteManagedLaser(laserToDelete);

		if (ImGui::Button("Save Config")) {
			saveZones();
		}
    }
    
    std::shared_ptr<ofxLaser::Laser>& addLaser(){
        // Kept for source compatibility. Projector creation is now owned by
        // the controller; ILDA Projector nodes only select an existing laser.
        return laser.getLaser(laser.getNumLasers()-1);
    }

    vector<string> getLaserNames() {
        vector<string> names;
        for(const auto &projector : laser.getLasers()) names.push_back(projector->getLabel());
        return names;
    }

    uint64_t getLasersRevision() const { return lasersRevision; }

    std::shared_ptr<ofxLaser::Laser> getLaserAt(int index) {
        if(index < 0 || index >= laser.getNumLasers()) return nullptr;
        return laser.getLaser(index);
    }
    
    ofxLaser::Manager& getManager(){
        return laser;
    }

    vector<string> getZoneNames() const {
        vector<string> names;
        names.reserve(zones.size());
        for(const auto &zoneConfig : zones) names.push_back(zoneConfig.name);
        return names;
    }

    uint64_t getZonesRevision() const { return zonesRevision; }

    string getZoneStableId(int index) const {
        if(index < 0 || index >= static_cast<int>(zones.size())) return "";
        return zones[index].stableId;
    }

    int findZoneIndex(const string &stableId) const {
        for(int i = 0; i < static_cast<int>(zones.size()); i++){
            if(zones[i].stableId == stableId) return i;
        }
        return zones.empty() ? -1 : 0;
    }

    glm::vec2 getZoneOffset(const string &stableId) const {
        int index = findZoneIndex(stableId);
        return index < 0 ? glm::vec2(0) : glm::vec2(index * ZoneSize, 0);
    }

    // -------------------------------------------------------------------
    // Simulator buffer — flat [x, y, r, g, b, ..., -1] per frame.
    // Mirrors the format sent by polyOscSender (x/y normalised by 800,
    // r/g/b in 0..1).  ildaShape calls this for every fatline it sends
    // to the laser. This method is append-only: it writes into the
    // simulatorWriteBuffer during Phase A (node updates). The controller's
    // update() (Phase B) swaps write→read and clears the write buffer, so
    // the laserSimulator always gets fresh data via getSimulatorData()
    // (and renders nothing when no shapes publish this frame).
    // -------------------------------------------------------------------
    void publishLaserShape(const ofxFatLine& fat, const vector<ofColor>& colors) {
        for (int i = 0; i < (int)fat.size(); i++) {
            simulatorWriteBuffer.push_back(fat.getVertices()[i].x / 800.0f);
            simulatorWriteBuffer.push_back(fat.getVertices()[i].y / 800.0f);
            simulatorWriteBuffer.push_back(colors[i].r / 255.0f);
            simulatorWriteBuffer.push_back(colors[i].g / 255.0f);
            simulatorWriteBuffer.push_back(colors[i].b / 255.0f);
        }
        simulatorWriteBuffer.push_back(-1.0f);
    }

    const vector<float>& getSimulatorData() const {
        return simulatorReadBuffer;
    }

    vector<ofxLaser::Point> getAllLaserPoints(){
        vector<ofxLaser::Point> points;
        for(auto &l : laser.getLasers()){
            vector<ofxLaser::Point> newPoints = l->getLaserPoints();
            points.insert(points.end(), newPoints.begin(), newPoints.end());
        }
        return points;
    }

    vector<ofxLaser::Point> getLaserPoints(int laserIndex) {
        if(laserIndex < 0 || laserIndex >= laser.getNumLasers()) return {};
        return laser.getLaser(laserIndex)->getLaserPoints();
    }
    
    bool isFrozen(){return freeze;};
    
    ofEvent<float> newValue;
private:
    static array<glm::vec2, 4> defaultWarp(){
        return {glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(0, 1), glm::vec2(1, 1)};
    }

    void ensureWarpCount(ZoneConfig &zoneConfig){
        while(zoneConfig.warps.size() < static_cast<size_t>(laser.getNumLasers())){
            zoneConfig.warps.push_back(defaultWarp());
        }
        while(zoneConfig.assigned.size() < static_cast<size_t>(laser.getNumLasers())){
            zoneConfig.assigned.push_back(false);
        }
    }

    void syncZoneAssignment(int laserIndex, int zoneIndex){
        if(laserIndex < 0 || laserIndex >= laser.getNumLasers()) return;
        if(zoneIndex < 0 || zoneIndex >= static_cast<int>(zones.size())) return;
        ensureWarpCount(zones[zoneIndex]);
        auto &projector = laser.getLaser(laserIndex);
        const bool present = projector->hasZone(zones[zoneIndex].laserZoneId);
        const bool wanted = zones[zoneIndex].assigned[laserIndex];
        if(wanted && !present) projector->addZone(zones[zoneIndex].laserZoneId);
        if(!wanted && present) projector->removeZone(zones[zoneIndex].laserZoneId);
        if(wanted) applyWarpPointsToLaser(laserIndex, zoneIndex);
    }

    void relayoutInputZones(){
        auto inputZones = laser.getLaserState().canvasTarget->getInputZones();
        for(int i = 0; i < static_cast<int>(inputZones.size()); i++){
            if(inputZones[i]) inputZones[i]->set(i * ZoneSize, 0, ZoneSize, ZoneSize);
        }
    }

    void addManagedLaser(){
        laser.createAndAddLaser();
        const int laserIndex = laser.getNumLasers() - 1;
        auto &projector = laser.getLaser(laserIndex);
        projector->armed = false;
        for(int zoneIndex = 0; zoneIndex < static_cast<int>(zones.size()); zoneIndex++){
            ensureWarpCount(zones[zoneIndex]);
            zones[zoneIndex].assigned[laserIndex] = false;
            syncZoneAssignment(laserIndex, zoneIndex);
        }
        lasersRevision++;
        saveZones();
    }

    void deleteManagedLaser(int laserIndex){
        if(laser.getNumLasers() <= 1 || laserIndex < 0 || laserIndex >= laser.getNumLasers()) return;
        auto projector = laser.getLaser(laserIndex);
        if(!laser.deleteLaser(projector)) return;
        for(auto &zoneConfig : zones){
            if(laserIndex < static_cast<int>(zoneConfig.assigned.size())){
                zoneConfig.assigned.erase(zoneConfig.assigned.begin() + laserIndex);
            }
            if(laserIndex < static_cast<int>(zoneConfig.warps.size())){
                zoneConfig.warps.erase(zoneConfig.warps.begin() + laserIndex);
            }
        }
        lasersRevision++;
        saveZones();
    }

    void addZone(const string &name = "", const string &stableId = "", const vector<bool> &assignments = {}){
        ZoneConfig config;
        config.stableId = stableId.empty()
            ? "zone-" + ofToString(ofGetSystemTimeMicros()) + "-" + ofToString(static_cast<int>(ofRandom(1000000)))
            : stableId;
        config.name = name.empty() ? "Zone " + ofToString(zones.size() + 1) : name;
        config.laserZoneId = laser.addZone(zones.size() * ZoneSize, 0, ZoneSize, ZoneSize);
        ensureWarpCount(config);
        for(int laserIndex = 0; laserIndex < laser.getNumLasers(); laserIndex++){
            config.assigned[laserIndex] = assignments.empty() ||
                (laserIndex < static_cast<int>(assignments.size()) && assignments[laserIndex]);
        }
        zones.push_back(config);
        laser.setCanvasSize(zones.size() * ZoneSize, ZoneSize);
        // ManagerBase::setCanvasSize deliberately expands a zone that matched
        // the previous canvas bounds. That behaviour is useful for the default
        // single-zone workflow, but would make Zone 1 overlap every later tile.
        relayoutInputZones();

        for(int laserIndex = 0; laserIndex < laser.getNumLasers(); laserIndex++){
            syncZoneAssignment(laserIndex, static_cast<int>(zones.size()) - 1);
        }
        zonesRevision++;
    }

    void deleteZone(int index){
        if(index < 0 || index >= static_cast<int>(zones.size()) || zones.size() <= 1) return;
        auto inputZones = laser.getLaserState().canvasTarget->getInputZones();
        if(index >= static_cast<int>(inputZones.size())) return;
        laser.deleteZone(inputZones[index]);
        zones.erase(zones.begin() + index);

        inputZones = laser.getLaserState().canvasTarget->getInputZones();
        for(int i = 0; i < static_cast<int>(zones.size()) && i < static_cast<int>(inputZones.size()); i++)
            zones[i].laserZoneId = inputZones[i]->getZoneId();
        laser.setCanvasSize(zones.size() * ZoneSize, ZoneSize);
        relayoutInputZones();
        selectedZoneIndex = ofClamp(selectedZoneIndex, 0, static_cast<int>(zones.size()) - 1);
        zonesRevision++;
    }

    void drawZonesGui(){
        if(!ImGui::TreeNode("Zones")) return;
        if(ImGui::Button("Add Zone")) addZone();
        ImGui::SameLine();
        ImGui::TextDisabled("Shapes select a zone in the ILDA Shape node");

        for(int zoneIndex = 0; zoneIndex < static_cast<int>(zones.size()); zoneIndex++){
            auto &config = zones[zoneIndex];
            ImGui::PushID(config.stableId.c_str());
            bool open = ImGui::TreeNode(config.name.c_str());
            if(open){
                char nameBuffer[128];
                std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", config.name.c_str());
                if(ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))){
                    config.name = nameBuffer;
                    zonesRevision++;
                }
                ImGui::Text("Input: %.0f..%.0f x 0..%.0f", zoneIndex * ZoneSize,
                            (zoneIndex + 1) * ZoneSize, ZoneSize);
                for(int laserIndex = 0; laserIndex < laser.getNumLasers(); laserIndex++){
                    auto &projector = laser.getLaser(laserIndex);
                    ensureWarpCount(config);
                    bool assigned = config.assigned[laserIndex];
                    string label = "Send to " + projector->getLabel();
                    if(ImGui::Checkbox(label.c_str(), &assigned)){
                        config.assigned[laserIndex] = assigned;
                        syncZoneAssignment(laserIndex, zoneIndex);
                    }
                }
                if(zones.size() > 1 && ImGui::Button("Delete Zone")){
                    deleteZone(zoneIndex);
                    ImGui::TreePop();
                    ImGui::PopID();
                    break;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    void loadZones(){
        ofJson json = ofLoadJson("ildaZones.json");
        if(json.contains("zones") && json["zones"].is_array()){
            for(const auto &zoneJson : json["zones"]){
                vector<bool> assignments;
                if(zoneJson.contains("assigned")) assignments = zoneJson["assigned"].get<vector<bool>>();
                addZone(zoneJson.value("name", ""), zoneJson.value("id", ""), assignments);
                auto &config = zones.back();
                if(zoneJson.contains("warps")){
                    const auto &warpsJson = zoneJson["warps"];
                    for(int laserIndex = 0; laserIndex < static_cast<int>(warpsJson.size()) && laserIndex < laser.getNumLasers(); laserIndex++){
                        for(int pointIndex = 0; pointIndex < 4 && pointIndex < static_cast<int>(warpsJson[laserIndex].size()); pointIndex++){
                            config.warps[laserIndex][pointIndex] = glm::vec2(
                                warpsJson[laserIndex][pointIndex][0].get<float>(),
                                warpsJson[laserIndex][pointIndex][1].get<float>());
                        }
                    }
                }
            }
        }
        if(zones.empty()){
            addZone("Zone 1");
            ofJson legacy = ofLoadJson("warpPoints.json");
            if(!legacy.empty()){
                for(int pointIndex = 0; pointIndex < 4; pointIndex++){
                    string key = "p" + ofToString(pointIndex);
                    if(legacy.contains(key)){
                        glm::vec2 migratedPoint = {
                            legacy[key].value("x", zones[0].warps[0][pointIndex].x),
                            legacy[key].value("y", zones[0].warps[0][pointIndex].y)};
                        for(auto &laserWarp : zones[0].warps) laserWarp[pointIndex] = migratedPoint;
                    }
                }
            }
        }
    }

    void saveZones(){
        ofJson json;
        json["version"] = 1;
        json["zones"] = ofJson::array();
        for(const auto &config : zones){
            ofJson zoneJson;
            zoneJson["id"] = config.stableId;
            zoneJson["name"] = config.name;
            zoneJson["assigned"] = ofJson::array();
            zoneJson["warps"] = ofJson::array();
            for(int laserIndex = 0; laserIndex < laser.getNumLasers(); laserIndex++){
                zoneJson["assigned"].push_back(laserIndex < static_cast<int>(config.assigned.size()) && config.assigned[laserIndex]);
                ofJson warpJson = ofJson::array();
                for(const auto &point : config.warps[laserIndex]) warpJson.push_back({point.x, point.y});
                zoneJson["warps"].push_back(warpJson);
            }
            json["zones"].push_back(zoneJson);
        }
        ofSavePrettyJson("ildaZones.json", json);
        laser.saveSettings();
    }

    void applyWarpPointsToLaser(int laserIndex, int zoneIndex){
        if(laserIndex < 0 || laserIndex >= laser.getNumLasers()) return;
        if(zoneIndex < 0 || zoneIndex >= static_cast<int>(zones.size())) return;
        ensureWarpCount(zones[zoneIndex]);

        auto outputZone = laser.getLaser(laserIndex)->getLaserZoneForZoneId(zones[zoneIndex].laserZoneId);
        if(outputZone == nullptr) return;

        const auto &warpPoints = zones[zoneIndex].warps[laserIndex];

        // This controller owns a single four-corner warp. Force the output zone
        // to use that transform so a previously persisted complex transform
        // cannot silently override it and look like a second warp.
        outputZone->transformType = 0;
        outputZone->zoneTransformQuad.setDstCorners(
            warpPoints[0] * ZoneSize,
            warpPoints[1] * ZoneSize,
            warpPoints[2] * ZoneSize,
            warpPoints[3] * ZoneSize
        );
        outputZone->update();
    }

    void clampWarpPoint(array<glm::vec2, 4> &warpPoints, int pointIndex){
        if(pointIndex < 0 || pointIndex >= static_cast<int>(warpPoints.size())) return;

        if(warpPoints[pointIndex].x > 1.0f) warpPoints[pointIndex].x = 1.0f;
        if(warpPoints[pointIndex].x < 0.0f) warpPoints[pointIndex].x = 0.0f;
        if(warpPoints[pointIndex].y > 1.0f) warpPoints[pointIndex].y = 1.0f;
        if(warpPoints[pointIndex].y < 0.0f) warpPoints[pointIndex].y = 0.0f;
    }

    ofxLaser::Manager laser;
    ofxLaser::DacAssigner* dacAssigner;
    vector<ZoneConfig> zones;
    uint64_t zonesRevision = 0;
    uint64_t lasersRevision = 0;
    int selectedZoneIndex = 0;

 ofParameter<void> saveConfig;
 bool freeze;
 bool firstUpdate;

    // Double-buffered simulator data. Producers (ildaShape via publishLaserShape)
    // append to simulatorWriteBuffer during Phase A (node updates). In Phase B
    // (controller update), we swap write→read and clear the write buffer. The
    // consumer (laserSimulator) reads simulatorReadBuffer during Phase A. This
    // decouples producer/consumer from Oceanode's unordered node-update order
    // and from the controller-runs-last ordering, with one frame of latency.
    vector<float> simulatorWriteBuffer;
    vector<float> simulatorReadBuffer;

    int pointDraggingIndex;
};


#endif /* testController_h */
