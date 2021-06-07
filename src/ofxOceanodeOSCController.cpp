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
    json = ofLoadJson("OscConfig.json");
}

ofxOceanodeOSCController::~ofxOceanodeOSCController(){
    
}

void ofxOceanodeOSCController::save(){
    ofJson _json;
    _json["InPort"] = "12345";
    for(auto &i : hosts){
        string name = i.first;
        _json[name]["Host"] = hosts[name];
        _json[name]["Port"] = ports[name];
    }
    ofSavePrettyJson("OscConfig.json", _json);
}

void ofxOceanodeOSCController::draw(){
    static int port = 12345;
    ImGui::DragInt("Port In", &port, 1, 0, 100000);
    if(ImGui::IsItemDeactivatedAfterEdit()){
        container->setupOscReceiver(port);
    }
    ImGui::Separator();
    ImGui::Separator();
    for(auto &i : hosts){
        string name = i.first;
        if(ImGui::TreeNode(name.c_str())){
            char * cString = new char[256];
            strcpy(cString, hosts[name].c_str());
            if (ImGui::InputText("Host", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                hosts[name] = cString;
                hostEvents[name].notify(hosts[name]);
            }
            strcpy(cString, ports[name].c_str());
            if (ImGui::InputText("Port", cString, 256, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                ports[name] = cString;
                portEvents[name].notify(ports[name]);
            }
            delete[] cString;
            ImGui::TreePop();
        }
    }
    if(ImGui::Selectable("SAVE")){
        save();
    }
}

pair<string, string> ofxOceanodeOSCController::addSender(string name){
    if(hosts.count(name) == 0){
        hosts[name] = "localhost";
        ports[name] = "54321";
        if(!json.is_null()){
            if(json.count(name) == 1){
                hosts[name] = json[name]["Host"];
                ports[name] = json[name]["Port"];
            }
        }
        hostEvents[name] = ofEvent<string>();
        portEvents[name] = ofEvent<string>();
    }
    //Send host and ports
    return make_pair(hosts[name], ports[name]);
}
