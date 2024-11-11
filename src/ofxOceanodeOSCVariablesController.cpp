
#include "ofxOceanodeOSCVariablesController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeRegistry.h"
#include "oscVariables.h"
#include "imgui.h"

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
        
        string address = message.getAddress();
        if (!address.empty() && address[0] == '/') {
            address = address.substr(1);
        }
        
        // Store or overwrite with the latest message for this address
        latestMessages[address] = std::move(message);
    }
    
    // Now process only the latest message for each address
    std::lock_guard<std::mutex> lock(parameterMutex);
    
    for (const auto& [address, message] : latestMessages) {
        auto paramIt = std::find_if(parameters.begin(), parameters.end(),
                                  [&address](const auto& param) {
            return param->getName() == address;
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
                << address << ": " << e.what();
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
    string groupToDelete = "";
    for(auto &group : groups) {
        auto &groupParams = group->parameters;
        ImGui::PushID(group->name.c_str());
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        bool openGroup = true;
        
        // set collapsing header color and title
        ImVec4 color = (group->oscMode == OscMode::Sender) ?
        ImVec4(0.1f, 0.15f, 0.1f, 0.8f) :     // for Sender
        ImVec4(0.15f, 0.1f, 0.1f, 0.8f);      // for Receiver
        
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImVec4(color.x + 0.1f, color.y + 0.1f, color.z + 0.1f, color.w));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              ImVec4(color.x + 0.2f, color.y + 0.2f, color.z + 0.2f, color.w));
        
        string headerText = group->name;
        if(group->oscMode == OscMode::Sender) {
            headerText = "Sender : " + headerText;
        }
        else headerText = "Receiver : " + headerText;
        
        int labelToValueDistance = 100;
        if(ImGui::CollapsingHeader(headerText.c_str(), &openGroup)) {
            // OSC Configuration Section
            bool configChanged = false;
            
            
            // Port Configuration - Show appropriate port based on mode
            if (group->oscMode == OscMode::Sender) {
                bool isListening = group->sender.isReady();
                ImGui::Checkbox("Is ready?", &isListening);
                ImGui::SameLine();
                ImGui::Text("Port:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                
                // Create unique identifier for each port input
                string portInputId = "##senderport_" + group->name;
                static std::map<string, int> tempPorts;  // Store temp ports per group
                
                // Initialize temp port if needed
                if(tempPorts.find(group->name) == tempPorts.end()) {
                    tempPorts[group->name] = group->portParam;
                }
                
                if (ImGui::InputInt(portInputId.c_str(), &tempPorts[group->name], 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (tempPorts[group->name] != group->portParam) {
                        group->portParam = ofClamp(tempPorts[group->name], 1024, 65535);
                        configChanged = true;
                    }
                }
                ImGui::SameLine();
                // IP Address for Sender
                ImGui::Text("IP Address:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120);
                
                // Create unique identifier for IP input
                string ipInputId = "##ip_" + group->name;
                static std::map<string, char[16]> ipBuffers;  // Store IP buffers per group
                
                // Initialize IP buffer if needed
                if(ipBuffers.find(group->name) == ipBuffers.end()) {
                    strcpy(ipBuffers[group->name], group->ipParam.get().c_str());
                }
                
                if (ImGui::InputText(ipInputId.c_str(), ipBuffers[group->name], 16,
                                     ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    string newIp = string(ipBuffers[group->name]);
                    if (newIp != group->ipParam.get())
                    {
                        group->ipParam = newIp;
                        configChanged = true;
                    }
                }
            }
            else
            {
                // Receiver mode
                bool isListening = group->receiver.isListening();
                ImGui::Checkbox("Is listening?", &isListening);
                ImGui::SameLine();
                ImGui::Text("Port:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                int tempPort = group->portParam.get();
                configChanged = ImGui::InputInt(ofToString("##receiverport"+group->name).c_str(), &tempPort,1,1,ImGuiInputTextFlags_EnterReturnsTrue);
                
                tempPort = ofClamp(tempPort, 1024, 65535);
                group->portParam.set(tempPort);
            }
            
            // If any config parameter changed, reset the OSC connection
            if (configChanged) {
                group->resetOSCConnection();
            }
            
            ImGui::Separator();
            ImGui::Separator();
            
            //-------------------------------------------------
            // Group parameters
            //-------------------------------------------------
            for(int i = 0; i < groupParams.size(); i++)
            {
                ofAbstractParameter &absParam = *groupParams[i];
                std::string uniqueId = absParam.getName();
                ImGui::PushID(uniqueId.c_str());
                
                ImGui::Text("/");
                ImGui::SameLine();
                
                // Get current cursor position
                ImVec2 cursorPos = ImGui::GetCursorPos();
                ImVec2 windowPos = ImGui::GetWindowPos();
                
                
                // Calculate rectangle dimensions
                float rectHeight = ImGui::GetTextLineHeight() + 4;  // Add some padding
                
                // Draw the background rectangle (spans full width to 200)
                ImGui::GetWindowDrawList()->AddRectFilled(
                                                          ImVec2(windowPos.x + cursorPos.x, windowPos.y + cursorPos.y),
                                                          ImVec2(windowPos.x + cursorPos.x + 200, windowPos.y + cursorPos.y + rectHeight),
                                                          IM_COL32(64, 64, 64, 255)  // Mid gray (adjust color as needed)
                                                          );
                
                // Draw the text
                ImGui::Text("%s", uniqueId.c_str());
                ImGui::SameLine(225);
                ImGui::SetNextItemWidth(100);
                
                std::string hiddenUniqueId = "##" + uniqueId;
                if(absParam.isOfType<float>())
                {
                    ImGui::Text("Float");
                }
                else if(absParam.isOfType<string>())
                {
                    ImGui::Text("String");
                    
                }
                else if(absParam.isOfType<vector<float>>()) {
                    ImGui::Text("Vec.Float");
                }
                else if(absParam.isOfType<vector<string>>()) {
                    ImGui::Text("Vec.String");
                }
                
                ImGui::SameLine(225 + 80);
                if(ImGui::Button("[-]")){
                    group->removeParameter(absParam.getName());
                }
                
                ImGui::PopID();
            }
            
            if(ImGui::Button("[+]")){
                ImGui::OpenPopup("New Variable");
            }
            
            // Always center this window when appearing
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if(ImGui::BeginPopupModal("New Variable", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                //ImGui::Text("%s %s %s", "New variable for", group->name.c_str(), "group");
                ImGui::Separator();
                
                enum Types {
                    Type_Float,
                    Type_FloatVector,
                    Type_String,
                    Type_StringVector,
                    Type_COUNT
                };
                
                static int type = Type_Float;
                const char* types_names[Type_COUNT] = {
                    "Float",
                    "Float Vector",
                    "String",
                    "String Vector"
                };
                const char* type_name = (type >= 0 && type < Type_COUNT) ? types_names[type] : "Unknown";
                ImGui::SliderInt("Type", &type, 0, Type_COUNT - 1, type_name);
                
                static char cString[255];
                bool enterPressed = false;
                if(ImGui::InputText("Name", cString, 255, ImGuiInputTextFlags_EnterReturnsTrue)){
                    enterPressed = true;
                }
                
                // Remove the vector size input completely
                
                if (ImGui::Button("OK", ImVec2(120, 0)) || enterPressed) {
                    string proposedNewName(cString);
                    if(proposedNewName != "" && find_if(groupParams.begin(), groupParams.end(),
                                                        [proposedNewName](const auto &param){return param->getName() == proposedNewName;}) == groupParams.end()) {
                        
                        switch(type) {
                            case Type_Float:
                                group->addFloatParameter(proposedNewName);
                                break;
                            case Type_String:
                                group->addStringParameter(proposedNewName);
                                break;
                            case Type_FloatVector: {
                                std::vector<float> defaultVec = {0};  // Single element vector with default value
                                group->addFloatVectorParameter(proposedNewName, defaultVec);
                                break;
                            }
                            case Type_StringVector: {
                                // Initialize with empty vector
                                std::vector<string> defaultVec;
                                group->addStringVectorParameter(proposedNewName, defaultVec);
                                break;
                            }
                            default:
                                ofLog() << "Cannot create variable of unknown type";
                        }
                        
                        ImGui::CloseCurrentPopup();
                    }
                    strcpy(cString, "");
                    ImGui::CloseCurrentPopup();
                }
                
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    strcpy(cString, "");
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        
        // reseting color of the collapsing header
        ImGui::PopStyleColor(3);
        
        if(!openGroup){
            groupToDelete = group->name;
        }
        
        ImGui::PopID();
    }
    
    // Handle group deletion with proper cleanup
    if(groupToDelete != ""){
        // Find the group we're about to delete
        auto groupIt = std::find_if(groups.begin(), groups.end(),
                                    [groupToDelete](const auto &group) {
            cout << "Group->name: " << group->name << "    groupToDelete: " << groupToDelete << endl;
            return group->name == groupToDelete;
        });
        
        if(groupIt != groups.end()) {
            // Get pointer to the group before removing it
            auto group = *groupIt;
            
            // First unregister the node type from the registry
            string modName;
            if(group->oscMode==OscMode::Sender)
            {
                modName = "Sender ";
            }
            else
            {
                modName = "Receiv. ";
            }
            
            container->getRegistry()->unregisterModel<oscVariables>("OSC Variables", modName + group->name, std::weak_ptr<oscVariablesGroup>());
            
            // Remove all existing nodes of this type
            for(auto* node : group->nodes) {
                node->deleteSelf();
            }
            
            // Now remove the group from our list
            groups.erase(groupIt);
            
            ofLogNotice("ofxOceanodeOSCVariablesController")
            << "Removed group and unregistered node type: " << groupToDelete;
        }
    }
    
    ImGui::Separator();
    
    if(ImGui::Button("[New Group]")){
        ImGui::OpenPopup("New OSC Variables Group");
    }
    ImGui::SameLine();
    if(ImGui::Button("[Save]")){
        save();
    }
    ImGui::SameLine();
    if(ImGui::Button("[Load]")){
        load();
    }
    
    bool unusedOpen = true;
    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    // Inside the ofxOceanodeOSCVariablesController::draw() function,
    // replace the existing PopupModal section with:
    if(ImGui::BeginPopupModal("New OSC Variables Group", &unusedOpen, ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::Text("Configure OSC Group");
        ImGui::Separator();
        
        // Group Name Input
        static char groupNameBuffer[255] = "";
        static char ipAddressBuffer[16] = "127.0.0.1";
        static int auxPort = 8000;    // Default sender port
        static int oscMode = 0;        // 0 for sender, 1 for receiver
        
        ImGui::Text("Group Name:");
        bool enterPressed = ImGui::InputText("##groupname", groupNameBuffer, 255,
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        
        // OSC Mode Selection
        ImGui::Separator();
        ImGui::RadioButton("Sender", &oscMode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Receiver", &oscMode, 1);
        
        // Show appropriate port based on mode
        if (oscMode == 0) { // Sender mode
            ImGui::Text("Sender Port:");
            ImGui::SameLine(150);
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("##senderport", &auxPort);
            auxPort = ofClamp(auxPort, 1024, 65535);
            
            // IP Address input
            ImGui::Text("Destination IP:");
            ImGui::SameLine(150);
            ImGui::InputText("##ipaddress", ipAddressBuffer, 16);
            ImGui::SameLine();
            if (ImGui::Button("localhost")) {
                strcpy(ipAddressBuffer, "127.0.0.1");
            }
        } else { // Receiver mode
            ImGui::Text("Receiver Port:");
            ImGui::SameLine(150);
            ImGui::SetNextItemWidth(100);
            ImGui::InputInt("##receiverport", &auxPort);
            auxPort = ofClamp(auxPort, 1024, 65535);
        }
        
        ImGui::Separator();
        
        // Buttons
        if (ImGui::Button("OK", ImVec2(120, 0)) || enterPressed) {
            string newName = string(groupNameBuffer);
            if(newName != "" &&
               find_if(groups.begin(), groups.end(),
                       [&newName](const auto &group){
                return group->name == newName;
            }) == groups.end())
            {
                // First create the group without initializing OSC
                auto newGroup = std::make_shared<oscVariablesGroup>();
                
                // Initialize the basic members
                newGroup->name = newName;
                newGroup->container = container;
                newGroup->oscMode = (oscMode == 0) ? OscMode::Sender : OscMode::Receiver;
                newGroup->portParam = auxPort;
                newGroup->ipParam = string(ipAddressBuffer);
                
                // Add to groups vector so shared_ptr exists
                groups.push_back(newGroup);
                
                // Now that the shared_ptr exists, we can safely initialize OSC
                newGroup->initializeOSC();
                
                // Register the module
                newGroup->registerModule();
                
                // Reset to defaults
                memset(groupNameBuffer, 0, sizeof(groupNameBuffer));
                strcpy(ipAddressBuffer, "127.0.0.1");
                auxPort = 8000;
                oscMode = 0;
                
                ImGui::CloseCurrentPopup();
            }
            else {
                ImGui::OpenPopup("Error##newgroup");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            // Clear all inputs
            memset(groupNameBuffer, 0, sizeof(groupNameBuffer));
            strcpy(ipAddressBuffer, "127.0.0.1");  // Reset to localhost
            auxPort = 8000;     // Reset to default sender port
            oscMode = 0;        // Reset to default
            ImGui::CloseCurrentPopup();
        }
        
        // Error popup
        if (ImGui::BeginPopupModal("Error##newgroup", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            static string lastError;
            ImGui::Text("%s", lastError.c_str());
            
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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
            else if(param->isOfType<string>()) {
                paramJson["type"] = "string";
            }
            else if(param->isOfType<vector<float>>()) {
                paramJson["type"] = "float_vector";
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
    
    // Unregister all existing groups before loading new ones
    for(auto &group : groups) {
        string modName = (group->oscMode == OscMode::Sender) ? "Sender " : "Receiv. ";
        container->getRegistry()->unregisterModel<oscVariables>("OSC Variables", modName + group->name, std::weak_ptr<oscVariablesGroup>());
        
        // Remove all nodes
        for(auto* node : group->nodes) {
            node->deleteSelf();
        }
    }
    
    // Clear the groups list
    groups.clear();
    
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
                        else if(paramType == "string") {
                            newGroup->addStringParameter(paramName);
                        }
                        else if(paramType == "float_vector") {
                            vector<float> defaultVec = {0};
                            newGroup->addFloatVectorParameter(paramName, defaultVec);
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
