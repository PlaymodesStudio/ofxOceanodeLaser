//
//  ildaProjector.h
//  example
//
//  Created by Eduard Frigola Bagué on 10/02/2021.
//

#ifndef ildaProjector_h
#define ildaProjector_h

#include "ofxOceanodeNodeModel.h"
#include "ildaController.h"
#include "ofxLaserDacEtherdream.h"


class ildaProjector : public ofxOceanodeNodeModel {
public:
    ildaProjector(shared_ptr<ildaController> iController) : controller(iController), ofxOceanodeNodeModel("ILDA Projector"){}
    
    void setup(){
        ip = "192.168.1.51";
        //TODO: restore ip from saved in iController "project save"
        projector = controller->addLaser(dac);
        
		ofxLaser::DacAssigner &dacAssigner = controller->getManager().dacAssigner;// getDacList();
		dacAssigner.updateDacList();

		addParameter(dacSelector.set("Dac Selector", [&dacAssigner, this](){
			// get the dacs from the dacAssigner
			 const vector<ofxLaser::DacData>& dacList = dacAssigner.getDacList();
				 
			 if (ImGui::BeginListBox("##listbox", glm::vec2(200, 30))){
				 
				 if(dacList.empty()) {
				  
					 ImGui::Selectable("No laser controllers found", false, ImGuiSelectableFlags_Disabled );
			
				 } else {
						 
						 
					 // add a combo box item for every element in the list
					 for(const ofxLaser::DacData& dacdata : dacList) {
						 
						 // get the dac label (usually type + unique ID)
						 string itemlabel = dacdata.label;
						 
						 ImGuiSelectableFlags selectableflags = 0;
						 
						 if(!dacdata.available) {
							// ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5);
							//itemlabel += " - no longer available";
							 selectableflags|=ImGuiSelectableFlags_Disabled;
						 } else {
							//
						 }
						 // if this dac is assigned to a laser, show which laser
						 //  - this could be done at the other end?
						 
						 if (ImGui::Selectable(itemlabel.c_str(), (dacdata.assignedLaser == projector), selectableflags)) {
							 // then select dac
							 // TODO : show a warning yes / no if :
							 //      - we already are connected to a DAC
							 //      - the chosen DAC is already being used by another laser
							 dacAssigner.assignToLaser(dacdata.label, *projector);
						 }
						 
						 if(dacdata.assignedLaser != nullptr) {
							 ImGui::SameLine(210 - 10);
							 string label =" > " + dacdata.assignedLaser->getLabel();
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
				dacAssigner.updateDacList();
				
			}
		}));
        
        addParameter(projector->intensity.set("Intensity", 1, 0, 1));

        addParameter(projector->pps.set("PPS", 30000,1000,90000));
        listener = projector->pps.newListener([this](int &f){
            projector->ppsChanged(f);
        });
		
		addParameter(projector->colourChangeShift.set("Offset", 0,0,6));
        addParameter(projector->scannerSettings.moveSpeed.set("Speed", 5,0.1,50));
        addParameter(projector->scannerSettings.shapePreBlank.set("Blank b", 1,0,8));
        addParameter(projector->scannerSettings.shapePreOn.set("On b", 1,0,8));
        addParameter(projector->scannerSettings.shapePostOn.set("On a", 1,0,8));
        addParameter(projector->scannerSettings.shapePostBlank.set("Blank a", 1,0,8));
		
        //addParameter(projector->scannerSettings.combinedOutput.set("Out v", {0}, {0}, {1}));
    }
    
private:
    shared_ptr<ildaController> controller;
    ofxLaser::Laser* projector;
    ofxLaser::DacEtherdream dac;
    std::string ip;
    
    ofEventListener listener;
	customGuiRegion dacSelector;
};


#endif /* ildaProjector_h */
