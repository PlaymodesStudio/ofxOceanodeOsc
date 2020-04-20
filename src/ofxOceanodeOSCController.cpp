//
//  ofxOceanodeOSCController.cpp
//  PLedNodes
//
//  Created by Eduard Frigola on 09/05/2018.
//

#include "ofxOceanodeOSCController.h"
#include "ofxOceanodeContainer.h"
#include "imgui.h"

ofxOceanodeOSCController::ofxOceanodeOSCController(shared_ptr<ofxOceanodeContainer> _container) : container(_container), ofxOceanodeBaseController("OSC"){
    container->setupOscReceiver(12345);
}

void ofxOceanodeOSCController::draw(){
    static int port = 12345;
    ImGui::DragInt("Port In", &port, 1, 0, 100000);
    if(ImGui::IsItemDeactivatedAfterEdit()){
        container->setupOscReceiver(port);
    }
}
