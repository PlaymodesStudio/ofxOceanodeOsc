
#include "ofxOceanodeOSCVariablesController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeRegistry.h"
#include "oscVariables.h"
#include "imgui.h"

#include <algorithm>
#include <cstring>

//-------------------------------------------------------------------------
// oscVariablesGroup
//-------------------------------------------------------------------------

// Main constructor implementation - make sure this matches the header exactly
oscVariablesGroup::oscVariablesGroup(std::string _name,
                                     std::shared_ptr<ofxOceanodeContainer> _container,
                                     OscMode mode,
                                     int portAux,
                                     std::string host)
: name(_name)
, container(_container)
, oscMode(mode)
, portParam(portAux)
, ipParam(host)
{
    // Ensure proper initialization
    receiver.stop();  // Make sure receiver is stopped initially
    sender.clear();   // Clear any existing sender
    
    // Set up the connection after a brief delay to ensure ports are free
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    resetOSCConnection();
}

oscVariablesGroup::~oscVariablesGroup() 
{
    // Delete all existing nodes
    for(auto &node : nodes) {
        node->deleteSelf();
    }
    string modName;
    if(oscMode==OscMode::Sender) {
        modName = "Sender ";
    }
    else {
        modName = "Receiv. ";
    }
    container->getRegistry()->unregisterModel<oscVariables>("OSC Variables", modName + name, std::weak_ptr<oscVariablesGroup>());
}
//oscVariablesGroup::oscVariablesGroup(){}


void oscVariablesGroup::registerModule() {
    // Capture both name and group in the lambda
    auto weakGroup = weak_from_this();
    auto groupName = name; // Capture the name by value
    
    container->getRegistry()->registerModel<oscVariables>(
                                                          "OSC Variables",  // category
                                                          groupName,        // args passed to constructor
                                                          weakGroup        // additional args passed to constructor
                                                          );
}

void oscVariablesGroup::addNode(oscVariables *node) {
    nodes.push_back(node);
    
    // Add port parameter based on mode
    if(oscMode == OscMode::Sender) {
        portParam.set("Sender Port", portParam, 1024, 65535);
        ipParam.set("Host IP", ipParam);
        node->addParameter(ipParam, ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_ReadOnly);
    } else {
        portParam.set("Receiver Port", portParam, 1024, 65535);
    }
    node->addParameter(portParam, ofxOceanodeParameterFlags_DisableSavePreset | ofxOceanodeParameterFlags_ReadOnly);
    
    for(int i = 0; i < parameters.size(); i++) {
        ofAbstractParameter& param = *parameters[i];
        // Try using explicit flag value
        ofxOceanodeParameterFlags flags = ofxOceanodeParameterFlags_DisableSavePreset;
        node->addParameter(param, flags);
    }
}

void oscVariablesGroup::removeNode(oscVariables *node){
    nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end());
}

void oscVariablesGroup::addFloatParameter(std::string parameterName, float value){
    ofParameter<float> tempParam;
    parameters.push_back(tempParam.set(parameterName, value, -FLT_MAX, FLT_MAX).newReference());
    for(auto &node : nodes){
        node->addParameter(tempParam,ofxOceanodeParameterFlags_DisableSavePreset);
    }
}

void oscVariablesGroup::addFloatVectorParameter(std::string parameterName, std::vector<float> value) {
    // Create and properly initialize the parameter
    ofParameter<vector<float>> tempParam;
    vector<float> defaultVal = {0};
    vector<float> minVal = {FLT_MIN};     // min values
    vector<float> maxVal = {FLT_MAX};     // max values
    
    // Use set() to properly initialize with name, default, min, and max values
    tempParam.set(parameterName, value, minVal, maxVal);
    tempParam.setSerializable(true);
    // Now add the reference to our parameters list
    parameters.push_back(std::make_shared<ofParameter<std::vector<float>>>(tempParam));
    // And add to all nodes
    for(auto &node : nodes) {
        if(node != nullptr) {
            node->addParameter(tempParam, ofxOceanodeParameterFlags_DisableSavePreset);
        }
    }
}

void oscVariablesGroup::addIntParameter(std::string parameterName, int value) {
    ofParameter<int> tempParam;
    parameters.push_back(tempParam.set(parameterName, value, INT_MIN, INT_MAX).newReference());
    for(auto &node : nodes) {
        node->addParameter(tempParam, ofxOceanodeParameterFlags_DisableSavePreset);
    }
}

void oscVariablesGroup::addIntVectorParameter(std::string parameterName, std::vector<int> value) {
    ofParameter<vector<int>> tempParam;
    vector<int> defaultVal = {0};
    vector<int> minVal = {INT_MIN};
    vector<int> maxVal = {INT_MAX};
    
    tempParam.set(parameterName, value, minVal, maxVal);
    tempParam.setSerializable(true);
    parameters.push_back(std::make_shared<ofParameter<std::vector<int>>>(tempParam));
    for(auto &node : nodes) {
        if(node != nullptr) {
            node->addParameter(tempParam, ofxOceanodeParameterFlags_DisableSavePreset);
        }
    }
}

void oscVariablesGroup::addStringParameter(std::string parameterName, std::string value){
    ofParameter<std::string> tempParam;
    parameters.push_back(tempParam.set(parameterName, value).newReference());
    for(auto &node : nodes){
        node->addParameter(tempParam,ofxOceanodeParameterFlags_DisableSavePreset);
    }
}


void oscVariablesGroup::addStringVectorParameter(std::string parameterName, std::vector<string> value) {
    ofParameter<std::vector<string>> tempParam;
    parameters.push_back(tempParam.set(parameterName, value).newReference());
    for(auto &node : nodes){
        node->addParameter(tempParam, ofxOceanodeParameterFlags_DisableSavePreset);
    }
}


void oscVariablesGroup::removeParameter(std::string parameterName){
    for(auto &node : nodes){
        node->removeParameter(parameterName);
    }
    parameters.erase(std::remove_if(parameters.begin(), parameters.end(), [parameterName](auto &parameter){return parameter->getName() == parameterName;}), parameters.end());
}

//bool oscVariablesGroup::isMyOSCPortAvailable(int port) {
//    int sock = socket(AF_INET, SOCK_DGRAM, 0);
//    if (sock < 0) {
//        ofLogError() << "Socket creation failed!";
//        return false;
//    }
//    
//    struct sockaddr_in addr;
//    memset(&addr, 0, sizeof(addr));  // Clear the structure
//    addr.sin_family = AF_INET;
//    addr.sin_port = htons(port);
//    addr.sin_addr.s_addr = INADDR_ANY;
//    
//    // Try binding the socket to the port
//    int result = ::bind(sock, (struct sockaddr*)&addr, sizeof(addr));  // Use global namespace bind
//    close(sock);  // Close the socket immediately after the check
//    
//    return result == 0;  // Return true if bind was successful (port is free)
//}

void oscVariablesGroup::initializeOSC() {
    // Ensure proper initialization
    receiver.stop();  // Make sure receiver is stopped initially
    sender.clear();   // Clear any existing sender
    
    // Set up the connection after a brief delay to ensure ports are free
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    resetOSCConnection();
}

void oscVariablesGroup::resetOSCConnection() {
    // First stop everything
    receiver.stop();
    sender.clear();
    
    if (oscMode == OscMode::Sender) {
        sender.setup(ipParam, portParam);
    } else {
        // For receiver mode
        receiver.stop();
        
        // Small delay to ensure port is fully released
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Try to setup the receiver
        bool setupSuccess = receiver.setup(portParam);
        
        if(setupSuccess) {
            ofLog() << "Successfully set up OSC Receiver on port: " << portParam;
        } else {
            ofLogError() << "Failed to set up OSC Receiver on port: " << portParam;
        }
    }
}

void oscVariablesGroup::update() {
    if (oscMode != OscMode::Receiver) return;

    // Map to store the latest message for each address
    std::map<string, ofxOscMessage> latestMessages;
    
    // Collect all waiting messages, keeping only the latest for each address
    while (receiver.hasWaitingMessages()) {
        ofxOscMessage message;
        receiver.getNextMessage(message);
        
        string msgAddress = message.getAddress();
        if (!msgAddress.empty() && msgAddress[0] == '/') {
            msgAddress = msgAddress.substr(1);
        }
        
        // Store or overwrite with the latest message for this address
        latestMessages[msgAddress] = std::move(message);
    }
    
    // Now process only the latest message for each address
    std::lock_guard<std::mutex> lock(parameterMutex);
    
    for (const auto& pair : latestMessages) 
    {
        const string& msgAddress = pair.first;
        const ofxOscMessage& message = pair.second;
        
        auto paramIt = std::find_if(parameters.begin(), parameters.end(),
                                  [&msgAddress](const auto& param) {
                                      return param->getName() == msgAddress;
                                  });
        
        if (paramIt != parameters.end()) {
            auto& param = *paramIt;
            
            try {
                // Handle float parameters
                if (param->isOfType<float>()) {
                    if (message.getNumArgs() > 0 && message.getArgType(0) == OFXOSC_TYPE_FLOAT) {
                        float value = message.getArgAsFloat(0);
                        param->cast<float>() = value;
                    }
                }
                else if (param->isOfType<int>()) {
                    if (message.getNumArgs() > 0 && message.getArgType(0) == OFXOSC_TYPE_INT32) {
                        int value = message.getArgAsInt32(0);
                        param->cast<int>() = value;
                    }
                }
                // Handle string parameters
                else if (param->isOfType<string>()) {
                    if (message.getNumArgs() > 0 && message.getArgType(0) == OFXOSC_TYPE_STRING) {
                        string value = message.getArgAsString(0);
                        param->cast<string>() = value;
                    }
                }
                // Handle float vector parameters
                else if (param->isOfType<vector<float>>()) {
                    vector<float> values;
                    for (int i = 0; i < message.getNumArgs(); i++) {
                        if (message.getArgType(i) == OFXOSC_TYPE_FLOAT) {
                            values.push_back(message.getArgAsFloat(i));
                        }
                    }
                    if (!values.empty()) {
                        param->cast<vector<float>>() = values;
                    }
                }
                else if (param->isOfType<vector<int>>()) {
                    vector<int> values;
                    for (int i = 0; i < message.getNumArgs(); i++) {
                        if (message.getArgType(i) == OFXOSC_TYPE_INT32) {
                            values.push_back(message.getArgAsInt32(i));
                        }
                    }
                    if (!values.empty()) {
                        param->cast<vector<int>>() = values;
                    }
                }
                // Handle string vector parameters
                else if (param->isOfType<vector<string>>()) {
                    vector<string> values;
                    for (int i = 0; i < message.getNumArgs(); i++) {
                        if (message.getArgType(i) == OFXOSC_TYPE_STRING) {
                            values.push_back(message.getArgAsString(i));
                        }
                    }
                    if (!values.empty()) {
                        param->cast<vector<string>>() = values;
                    }
                }
            }
            catch (const std::exception& e) {
                ofLogError("oscVariablesGroup") << "Error processing message for parameter "
                    << msgAddress << ": " << e.what();
            }
        }
    }
}
//-------------------------------------------------------------------------
// ofxOceanodeOSCVariablesController
//-------------------------------------------------------------------------

ofxOceanodeOSCVariablesController::ofxOceanodeOSCVariablesController(shared_ptr<ofxOceanodeContainer> _container) : container(_container), ofxOceanodeBaseController("OSC Variables"){
    load();
}

ofxOceanodeOSCVariablesController::~ofxOceanodeOSCVariablesController() {
    // Clean up all groups and unregister all node types
    /*
     for(auto &group : groups) {
     // Unregister from registry
     container->getRegistry()->unregisterModel<oscVariables>(
     "OSC Variables",  // category
     group->name      // model name
     );
     
     
     
     
     // Remove all nodes
     for(auto* node : group->nodes) {
     node->deleteSelf();
     }
     }
     
     groups.clear();
     */
}


void ofxOceanodeOSCVariablesController::draw() {
    if(ImGui::MenuItem("Save OSC Variables")) save();
    if(ImGui::MenuItem("Load OSC Variables")) load();
    if(ImGui::MenuItem("New Group...")) newGroupRequested = true;
    ImGui::Separator();
    ImGui::TextUnformatted("Groups");

    string groupToDelete;
    for(const auto& group : groups) {
        ImGui::PushID(group.get());
        ImGui::SetNextItemOpen(groups.size() == 1, ImGuiCond_Once);
        const ImVec4 headerColor = group->oscMode == OscMode::Sender
            ? ImVec4(0.1f, 0.15f, 0.1f, 0.8f)
            : ImVec4(0.15f, 0.1f, 0.1f, 0.8f);
        // Keep the sender/receiver tint at rest; the shared menu hover color
        // applies when the header is hovered or pressed.
        ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
        const string header = string(group->oscMode == OscMode::Sender ? "Sender: " : "Receiver: ") + group->name;
        const bool open = ImGui::TreeNode("##group", "%s", header.c_str());
        ImGui::SameLine();
        if(ImGui::SmallButton("[-]##group")) groupToDelete = group->name;
        ImGui::PopStyleColor();

        if(open) {
            const bool senderMode = group->oscMode == OscMode::Sender;
            const bool connected = senderMode ? group->sender.isReady() : group->receiver.isListening();
            ImGui::Text("%s: %s", senderMode ? "Ready" : "Listening", connected ? "Yes" : "No");

            auto portIt = editablePorts.find(group->name);
            if(portIt == editablePorts.end())
                portIt = editablePorts.emplace(group->name, group->portParam.get()).first;
            ImGui::SetNextItemWidth(100.0f);
            ImGui::InputInt("Port", &portIt->second, 0, 0);
            bool configChanged = false;
            if(ImGui::IsItemDeactivatedAfterEdit()) {
                const int port = ofClamp(portIt->second, 1024, 65535);
                portIt->second = port;
                if(port != group->portParam.get()) {
                    group->portParam = port;
                    configChanged = true;
                }
            }

            if(senderMode) {
                auto hostIt = editableHosts.find(group->name);
                if(hostIt == editableHosts.end()) {
                    std::array<char, 16> host{};
                    std::strncpy(host.data(), group->ipParam.get().c_str(), host.size() - 1);
                    hostIt = editableHosts.emplace(group->name, host).first;
                }
                ImGui::SetNextItemWidth(135.0f);
                if(ImGui::InputText("Host IP", hostIt->second.data(), hostIt->second.size(),
                                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                    const string host(hostIt->second.data());
                    if(host != group->ipParam.get()) {
                        group->ipParam = host;
                        configChanged = true;
                    }
                }
            }
            if(configChanged) group->resetOSCConnection();

            ImGui::Separator();
            string parameterToDelete;
            for(const auto& parameter : group->parameters) {
                ImGui::PushID(parameter.get());
                ofAbstractParameter& value = *parameter;
                const char* typeName = "Unknown";
                if(value.isOfType<float>()) typeName = "Float";
                else if(value.isOfType<vector<float>>()) typeName = "Vec.Float";
                else if(value.isOfType<int>()) typeName = "Int";
                else if(value.isOfType<vector<int>>()) typeName = "Vec.Int";
                else if(value.isOfType<string>()) typeName = "String";
                else if(value.isOfType<vector<string>>()) typeName = "Vec.String";
                ImGui::TextColored(ImVec4(150.0f / 255.0f, 150.0f / 255.0f, 150.0f / 255.0f, 1.0f),
                                   "/%s", value.getName().c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("%s", typeName);
                ImGui::SameLine();
                if(ImGui::SmallButton("[-]##parameter")) parameterToDelete = value.getName();
                ImGui::PopID();
            }
            if(!parameterToDelete.empty()) group->removeParameter(parameterToDelete);
            if(ImGui::Button("New Variable...")) {
                newVariableGroupName = group->name;
                newVariableRequested = true;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if(!groupToDelete.empty()) {
        editablePorts.erase(groupToDelete);
        editableHosts.erase(groupToDelete);
        groups.erase(std::remove_if(groups.begin(), groups.end(), [&](const auto& group) {
            return group->name == groupToDelete;
        }), groups.end());
    }
}

void ofxOceanodeOSCVariablesController::drawPopups() {
    if(newGroupRequested) {
        newGroupName[0] = '\0';
        std::strcpy(newGroupHost, "127.0.0.1");
        newGroupPort = 8000;
        newGroupMode = 0;
        ImGui::OpenPopup("New OSC Variables Group");
        newGroupRequested = false;
    }
    if(newVariableRequested) {
        newVariableName[0] = '\0';
        newVariableType = 0;
        ImGui::OpenPopup("New OSC Variable");
        newVariableRequested = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopupModal("New OSC Variables Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Configure OSC Group");
        ImGui::Separator();
        if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 22.0f);
        const bool enter = ImGui::InputText("Group Name", newGroupName, sizeof(newGroupName),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::RadioButton("Sender", &newGroupMode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Receiver", &newGroupMode, 1);
        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputInt(newGroupMode == 0 ? "Sender Port" : "Receiver Port", &newGroupPort);
        if(newGroupMode == 0) {
            ImGui::SetNextItemWidth(135.0f);
            ImGui::InputText("Destination IP", newGroupHost, sizeof(newGroupHost));
            ImGui::SameLine();
            if(ImGui::Button("localhost")) std::strcpy(newGroupHost, "127.0.0.1");
        }

        const string name(newGroupName);
        const bool duplicate = std::any_of(groups.begin(), groups.end(), [&](const auto& group) {
            return group->name == name;
        });
        const bool canCreate = !name.empty() && !duplicate;
        if(duplicate) ImGui::TextDisabled("A group with this name already exists.");
        ImGui::BeginDisabled(!canCreate);
        const bool create = ImGui::Button("Create");
        ImGui::EndDisabled();
        if(canCreate && (create || enter)) {
            auto group = std::make_shared<oscVariablesGroup>();
            group->name = name;
            group->container = container;
            group->oscMode = newGroupMode == 0 ? OscMode::Sender : OscMode::Receiver;
            group->portParam = ofClamp(newGroupPort, 1024, 65535);
            group->ipParam = string(newGroupHost);
            groups.push_back(group);
            group->initializeOSC();
            group->registerModule();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopupModal("New OSC Variable", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto groupIt = std::find_if(groups.begin(), groups.end(), [&](const auto& group) {
            return group->name == newVariableGroupName;
        });
        if(groupIt == groups.end()) {
            ImGui::TextUnformatted("This group is no longer available.");
            if(ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        } else {
            const auto& group = *groupIt;
            ImGui::Text("New variable for %s", group->name.c_str());
            static const char* types[] = {
                "Float", "Float Vector", "Int", "Int Vector", "String", "String Vector"
            };
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
            ImGui::Combo("Type", &newVariableType, types, 6);
            if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 22.0f);
            const bool enter = ImGui::InputText("Name", newVariableName, sizeof(newVariableName),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
            const string name(newVariableName);
            const bool duplicate = std::any_of(group->parameters.begin(), group->parameters.end(),
                [&](const auto& parameter) { return parameter->getName() == name; });
            const bool canCreate = !name.empty() && !duplicate;
            if(duplicate) ImGui::TextDisabled("A variable with this name already exists.");
            ImGui::BeginDisabled(!canCreate);
            const bool create = ImGui::Button("Create");
            ImGui::EndDisabled();
            if(canCreate && (create || enter)) {
                switch(newVariableType) {
                    case 0: group->addFloatParameter(name); break;
                    case 1: group->addFloatVectorParameter(name, {0}); break;
                    case 2: group->addIntParameter(name); break;
                    case 3: group->addIntVectorParameter(name, {0}); break;
                    case 4: group->addStringParameter(name); break;
                    case 5: group->addStringVectorParameter(name, {}); break;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ofxOceanodeOSCVariablesController::save() {
    ofJson json;
    
    json["version"] = 1;
    json["groups"] = ofJson::array();
    
    for(auto &group : groups) {
        ofJson groupJson;
        
        // Basic group info
        groupJson["name"] = group->name;
        groupJson["mode"] = (group->oscMode == OscMode::Sender) ? "sender" : "receiver";
        
        // Save only relevant connection info based on mode
        if (group->oscMode == OscMode::Sender) {
            groupJson["host"] = group->ipParam;
            groupJson["port"] = group->portParam.get();  // Only save sender port for sender
        } else {
            groupJson["port"] = group->portParam.get();  // Only save receiver port for receiver
        }
        
        // Store parameters
        ofJson parametersJson = ofJson::array();
        for(const auto& param : group->parameters) {
            ofJson paramJson;
            paramJson["name"] = param->getName();
            
            if(param->isOfType<float>()) {
                paramJson["type"] = "float";
            }
            else if(param->isOfType<int>()) {
                paramJson["type"] = "int";
            }
            else if(param->isOfType<string>()) {
                paramJson["type"] = "string";
            }
            else if(param->isOfType<vector<float>>()) {
                paramJson["type"] = "float_vector";
            }
            else if(param->isOfType<vector<int>>()) {
                paramJson["type"] = "int_vector";
            }
            else if(param->isOfType<vector<string>>()) {
                paramJson["type"] = "string_vector";
            }
            
            parametersJson.push_back(paramJson);
        }
        groupJson["parameters"] = parametersJson;
        
        json["groups"].push_back(groupJson);
    }
    
    if(ofSavePrettyJson("oscVars.json", json)) {
        ofLogNotice("ofxOceanodeOSCVariablesController") << "Successfully saved OSC configuration";
    } else {
        ofLogError("ofxOceanodeOSCVariablesController") << "Failed to save OSC configuration";
    }
}

void ofxOceanodeOSCVariablesController::load() {
    ofJson json;
    try {
        json = ofLoadJson("oscVars.json");
    }
    catch(const std::exception& e) {
        ofLogError("ofxOceanodeOSCVariablesController") << "Failed to load oscVars.json: " << e.what();
        return;
    }
    
    if(json.empty()) {
        ofLogNotice("ofxOceanodeOSCVariablesController") << "No existing configuration found";
        return;
    }
    
    // Group destruction removes its nodes and unregisters its model.
    groups.clear();
    editablePorts.clear();
    editableHosts.clear();
    
    for(const auto& groupJson : json["groups"]) {
        try {
            string name = groupJson.value("name", "");
            string modeStr = groupJson.value("mode", "sender");
            OscMode mode = (modeStr == "sender") ? OscMode::Sender : OscMode::Receiver;
            
            // Default ports
            int tmpPort = 8000;
            string host = "127.0.0.1";
            
            // Get only relevant connection info based on mode
            if (mode == OscMode::Sender) {
                host = groupJson.value("host", "127.0.0.1");
                tmpPort = groupJson.value("port", 8000);
            } else {
                tmpPort = groupJson.value("port", 9000);
            }
            
            // Create the group using make_shared
            auto newGroup = std::make_shared<oscVariablesGroup>();
            
            // Add to groups vector FIRST so the shared_ptr exists
            groups.push_back(newGroup);
            
            // Now initialize the members
            newGroup->name = name;
            newGroup->container = container;
            newGroup->oscMode = mode;
            newGroup->portParam.set(tmpPort);
            newGroup->ipParam.set(host);
            
            // Initialize OSC after the group is fully set up
            newGroup->initializeOSC();
            
            // Register the module
            newGroup->registerModule();
            
            // Add parameters
            if(groupJson.contains("parameters") && groupJson["parameters"].is_array()) {
                for(const auto& paramJson : groupJson["parameters"]) {
                    string paramName = paramJson.value("name", "");
                    string paramType = paramJson.value("type", "");
                    
                    if(!paramName.empty() && !paramType.empty()) {
                        if(paramType == "float") {
                            newGroup->addFloatParameter(paramName);
                        }
                        else if(paramType == "int") {
                            newGroup->addIntParameter(paramName);
                        }
                        else if(paramType == "string") {
                            newGroup->addStringParameter(paramName);
                        }
                        else if(paramType == "float_vector") {
                            vector<float> defaultVec = {0};
                            newGroup->addFloatVectorParameter(paramName, defaultVec);
                        }
                        else if(paramType == "int_vector") {
                            vector<int> defaultVec = {0};
                            newGroup->addIntVectorParameter(paramName, defaultVec);
                        }
                        else if(paramType == "string_vector") {
                            vector<string> defaultVec;
                            newGroup->addStringVectorParameter(paramName, defaultVec);
                        }
                    }
                }
            }
            
        } catch(const std::exception& e) {
            ofLogError("ofxOceanodeOSCVariablesController")
            << "Failed to load group: " << e.what();
            
            // If loading fails, remove the last added group
            if (!groups.empty()) {
                groups.pop_back();
            }
        }
    }
}
