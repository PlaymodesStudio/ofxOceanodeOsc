
#include "ofxOceanodeOSCVariablesController.h"
#include "ofxOceanodeContainer.h"
#include "ofxOceanodeNodeRegistry.h"
#include "oscVariables.h"
#include "imgui.h"
#include "ofxOceanodeMenu.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cstring>

namespace {
const char* variableTypeNames[] = {
    "Float", "Float Vector", "Int", "Int Vector", "String", "String Vector"
};
// Type names in oscVars.json.
const char* variableTypeKeys[] = {
    "float", "float_vector", "int", "int_vector", "string", "string_vector"
};
// Pins every node has beside the variables.
const char* reservedPinNames[] = {"Sender Port", "Host IP", "Receiver Port"};

int typeFromKey(const std::string& key){
    for(int type = 0; type < oscVariableType_Count; type++){
        if(key == variableTypeKeys[type]) return type;
    }
    return -1;
}

// Node parameters are looked up by escaped name ("My Var" is "My_Var").
std::string escapedName(const std::string& name){
    ofParameter<float> probe;
    probe.setName(name);
    return probe.getEscapedName();
}

// The table shows names as addresses ("/name"), so one typed leading slash
// is not part of the name.
std::string variableName(const std::string& typed){
    std::string name = ofTrim(typed);
    if(!name.empty() && name[0] == '/') name.erase(0, 1);
    return name;
}

void copyName(char* buffer, size_t size, const std::string& name){
    const size_t length = std::min(name.size(), size - 1);
    std::memcpy(buffer, name.data(), length);
    buffer[length] = '\0';
}

// The registry builds a throwaway node with no group to read the model name,
// which then is "OSC " + this + group name.
std::string registryName(OscMode mode, const std::string& groupName){
    return (mode == OscMode::Sender ? "Sender " : "Receiv. ") + groupName;
}

template<typename T, typename AddArguments>
std::unique_ptr<of::priv::AbstractEventToken> sendOnChange(oscVariablesGroup* group, const std::shared_ptr<ofAbstractParameter>& parameter, AddArguments addArguments){
    std::weak_ptr<ofAbstractParameter> weakParameter = parameter;
    return parameter->cast<T>().newListener([group, weakParameter, addArguments](T& value){
        auto current = weakParameter.lock();
        if(current == nullptr || group->oscMode != OscMode::Sender) return;
        ofxOscMessage message;
        message.setAddress("/" + current->getName());
        addArguments(message, value);
        group->sender.sendMessage(message);
    });
}
}

const char* oscVariableTypeName(int type){
    return type >= 0 && type < oscVariableType_Count ? variableTypeNames[type] : "Unknown";
}

int oscVariableTypeOf(const ofAbstractParameter& parameter){
    if(parameter.isOfType<float>()) return oscVariableType_Float;
    if(parameter.isOfType<vector<float>>()) return oscVariableType_FloatVector;
    if(parameter.isOfType<int>()) return oscVariableType_Int;
    if(parameter.isOfType<vector<int>>()) return oscVariableType_IntVector;
    if(parameter.isOfType<string>()) return oscVariableType_String;
    if(parameter.isOfType<vector<string>>()) return oscVariableType_StringVector;
    return -1;
}

std::shared_ptr<ofAbstractParameter> createOscVariable(int type, const std::string& name){
    switch(type){
        case oscVariableType_Float:
            return std::make_shared<ofParameter<float>>(name, 0, -FLT_MAX, FLT_MAX);
        case oscVariableType_FloatVector:
            return std::make_shared<ofParameter<vector<float>>>(name, vector<float>{0}, vector<float>{-FLT_MAX}, vector<float>{FLT_MAX});
        case oscVariableType_Int:
            return std::make_shared<ofParameter<int>>(name, 0, INT_MIN, INT_MAX);
        case oscVariableType_IntVector:
            return std::make_shared<ofParameter<vector<int>>>(name, vector<int>{0}, vector<int>{INT_MIN}, vector<int>{INT_MAX});
        case oscVariableType_String:
            return std::make_shared<ofParameter<string>>(name, "");
        case oscVariableType_StringVector:
            return std::make_shared<ofParameter<vector<string>>>(name, vector<string>());
    }
    return nullptr;
}

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
    unregisterModule();
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

void oscVariablesGroup::unregisterModule() {
    container->getRegistry()->unregisterModel<oscVariables>("OSC Variables", registryName(oscMode, name), std::weak_ptr<oscVariablesGroup>());
}

std::string oscVariablesGroup::modelName() const {
    return "OSC " + registryName(oscMode, name);
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

    for(auto &parameter : parameters) {
        node->addVariableParameter(*parameter, ofxOceanodeParameterFlags_DisableSavePreset);
    }
}

void oscVariablesGroup::removeNode(oscVariables *node){
    nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end());
}

void oscVariablesGroup::listen(const std::shared_ptr<ofAbstractParameter>& parameter){
    std::unique_ptr<of::priv::AbstractEventToken> listener;
    switch(oscVariableTypeOf(*parameter)){
        case oscVariableType_Float:
            listener = sendOnChange<float>(this, parameter, [](ofxOscMessage& message, float& value){
                message.addFloatArg(value);
            });
            break;
        case oscVariableType_FloatVector:
            listener = sendOnChange<vector<float>>(this, parameter, [](ofxOscMessage& message, vector<float>& values){
                for(const auto& value : values) message.addFloatArg(value);
            });
            break;
        case oscVariableType_Int:
            listener = sendOnChange<int>(this, parameter, [](ofxOscMessage& message, int& value){
                message.addIntArg(value);
            });
            break;
        case oscVariableType_IntVector:
            listener = sendOnChange<vector<int>>(this, parameter, [](ofxOscMessage& message, vector<int>& values){
                for(const auto& value : values) message.addIntArg(value);
            });
            break;
        case oscVariableType_String:
            listener = sendOnChange<string>(this, parameter, [](ofxOscMessage& message, string& value){
                message.addStringArg(value);
            });
            break;
        case oscVariableType_StringVector:
            listener = sendOnChange<vector<string>>(this, parameter, [](ofxOscMessage& message, vector<string>& values){
                for(const auto& value : values) message.addStringArg(value);
            });
            break;
    }
    if(listener != nullptr) senderListeners[parameter.get()] = std::move(listener);
}

void oscVariablesGroup::addParameter(std::shared_ptr<ofAbstractParameter> parameter){
    if(parameter == nullptr) return;
    parameters.push_back(parameter);
    listen(parameter);
    for(auto &node : nodes){
        node->addVariableParameter(*parameter, ofxOceanodeParameterFlags_DisableSavePreset);
    }
}

void oscVariablesGroup::addFloatParameter(std::string parameterName, float value){
    auto parameter = createOscVariable(oscVariableType_Float, parameterName);
    parameter->cast<float>() = value;
    addParameter(parameter);
}

void oscVariablesGroup::addFloatVectorParameter(std::string parameterName, std::vector<float> value) {
    auto parameter = createOscVariable(oscVariableType_FloatVector, parameterName);
    parameter->cast<vector<float>>() = value;
    addParameter(parameter);
}

void oscVariablesGroup::addIntParameter(std::string parameterName, int value) {
    auto parameter = createOscVariable(oscVariableType_Int, parameterName);
    parameter->cast<int>() = value;
    addParameter(parameter);
}

void oscVariablesGroup::addIntVectorParameter(std::string parameterName, std::vector<int> value) {
    auto parameter = createOscVariable(oscVariableType_IntVector, parameterName);
    parameter->cast<vector<int>>() = value;
    addParameter(parameter);
}

void oscVariablesGroup::addStringParameter(std::string parameterName, std::string value){
    auto parameter = createOscVariable(oscVariableType_String, parameterName);
    parameter->cast<string>() = value;
    addParameter(parameter);
}

void oscVariablesGroup::addStringVectorParameter(std::string parameterName, std::vector<string> value) {
    auto parameter = createOscVariable(oscVariableType_StringVector, parameterName);
    parameter->cast<vector<string>>() = value;
    addParameter(parameter);
}

void oscVariablesGroup::removeParameter(std::string parameterName){
    auto it = std::find_if(parameters.begin(), parameters.end(), [&](const auto &parameter){return parameter->getName() == parameterName;});
    if(it == parameters.end()) return;
    auto parameter = *it;
    for(auto &node : nodes){
        node->removeVariableParameter(parameterName);
    }
    senderListeners.erase(parameter.get());
    parameters.erase(std::remove(parameters.begin(), parameters.end(), parameter), parameters.end());
}

bool oscVariablesGroup::isParameterNameAvailable(const std::string& parameterName, const ofAbstractParameter* ignore) const{
    if(parameterName.empty()) return false;
    const std::string escaped = escapedName(parameterName);
    for(const char* reserved : reservedPinNames){
        if(escapedName(reserved) == escaped) return false;
    }
    return std::none_of(parameters.begin(), parameters.end(), [&](const auto &parameter){
        return parameter.get() != ignore && parameter->getEscapedName() == escaped;
    });
}

bool oscVariablesGroup::renameParameter(const std::string& oldName, const std::string& newName){
    auto it = std::find_if(parameters.begin(), parameters.end(), [&](const auto &parameter){return parameter->getName() == oldName;});
    if(it == parameters.end()) return false;
    if(newName == oldName) return true;
    // ofParameter refuses a new name that escapes to the current one.
    if(escapedName(newName) == escapedName(oldName)) return false;
    if(!isParameterNameAvailable(newName, it->get())) return false;
    // The nodes' pins share this parameter, so they follow the new name.
    if(!(*it)->setName(newName)) return false;
    for(auto &node : nodes){
        node->parameterGroupChanged.notify();
    }
    return true;
}

bool oscVariablesGroup::setParameterType(const std::string& parameterName, int type){
    auto it = std::find_if(parameters.begin(), parameters.end(), [&](const auto &parameter){return parameter->getName() == parameterName;});
    if(it == parameters.end()) return false;
    if(oscVariableTypeOf(**it) == type) return true;
    auto newParameter = createOscVariable(type, parameterName);
    if(newParameter == nullptr) return false;
    senderListeners.erase(it->get());
    *it = newParameter;
    listen(newParameter);
    for(auto &node : nodes){
        node->replaceVariableParameter(parameterName, *newParameter);
    }
    return true;
}

bool oscVariablesGroup::moveParameter(const std::string& parameterName, int newIndex){
    auto it = std::find_if(parameters.begin(), parameters.end(), [&](const auto &parameter){return parameter->getName() == parameterName;});
    if(it == parameters.end()) return false;
    const int from = int(it - parameters.begin());
    const int to = std::clamp(newIndex, 0, int(parameters.size()) - 1);
    if(from == to) return false;
    auto parameter = *it;
    parameters.erase(it);
    parameters.insert(parameters.begin() + to, parameter);
    for(auto &node : nodes){
        node->sortVariables(parameters);
    }
    return true;
}

bool oscVariablesGroup::rename(const std::string& newName){
    if(newName.empty() || newName == name) return false;
    auto registry = container->getRegistry();
    const std::string newTypeName = "OSC " + registryName(oscMode, newName);
    if(registry->getRegisteredModels().count(newTypeName) != 0) return false;

    struct Link { ofxOceanodeAbstractParameter* source; ofxOceanodeAbstractParameter* sink; bool persistent; };
    struct Instance { ofxOceanodeContainer* owner; ofxOceanodeNode* node; oscVariables* model; unsigned int identifier; ofxOceanodeNode* replacement = nullptr; };
    std::vector<Instance> instances;
    std::vector<Link> links;
    auto isGroupNode = [&](ofxOceanodeAbstractParameter& parameter){
        return std::find(nodes.begin(), nodes.end(), parameter.getNodeModel()) != nodes.end();
    };
    // Sender pins are inputs and Receiver pins outputs, so links run both ways.
    // A link between two nodes of this group is taken once, from its source.
    for(auto* model : nodes){
        auto* owner = model->getHostContainer();
        if(owner == nullptr) continue;
        const auto& nodesMap = owner->getParameterGroupNodesMap();
        auto found = nodesMap.find(model->getParameterGroup().getEscapedName());
        if(found == nodesMap.end()) continue;
        instances.push_back({owner, found->second, model, model->getNumIdentifier()});
        auto& nodeParameters = model->getParameterGroup();
        for(int i = 0; i < nodeParameters.size(); i++){
            auto& parameter = static_cast<ofxOceanodeAbstractParameter&>(nodeParameters.get(i));
            for(auto* connection : parameter.getOutConnections()){
                links.push_back({&parameter, &connection->getSinkParameter(), connection->getIsPersistent()});
            }
            for(auto* connection : parameter.getInConnections()){
                if(isGroupNode(connection->getSourceParameter())) continue;
                links.push_back({&connection->getSourceParameter(), &parameter, connection->getIsPersistent()});
            }
        }
    }

    unregisterModule();
    name = newName;
    registerModule();

    bool success = true;
    for(auto& instance : instances){
        auto* oldNode = instance.node;
        auto& oldGui = oldNode->getNodeGui();
        const bool persistent = oldNode->getIsPersistent();

        // Keep the instance number, so "OSC Sender Old 2" becomes "OSC Sender New 2".
        const auto& existing = instance.owner->getDynamicNodes();
        auto sameType = existing.find(newTypeName);
        const bool identifierTaken = persistent || (sameType != existing.end() && sameType->second.count(instance.identifier) != 0);
        auto* newNode = instance.owner->createNodeFromName(newTypeName, identifierTaken ? -1 : int(instance.identifier), persistent);
        if(newNode == nullptr){
            // The old node keeps working; it still follows this group.
            ofLogWarning("OSC Variables") << "Could not replace a node with " << newTypeName;
            success = false;
            continue;
        }
        newNode->getNodeGui().setPosition(oldGui.getPosition());
        newNode->getNodeGui().setExpanded(oldGui.getExpanded());
        newNode->getNodeGui().setVisibility(oldGui.getVisibility());
        newNode->getNodeGui().setSelected(oldGui.getSelected());
        newNode->setActive(oldNode->getActive());
        instance.replacement = newNode;
    }

    // Each end on a replaced node moves to the same pin of its replacement.
    auto resolve = [&](ofxOceanodeAbstractParameter* parameter) -> std::pair<ofxOceanodeAbstractParameter*, ofxOceanodeContainer*>{
        for(const auto& instance : instances){
            if(parameter->getNodeModel() != instance.model) continue;
            if(instance.replacement == nullptr) return {parameter, instance.owner};
            auto& replacementParameters = instance.replacement->getParameters();
            if(!replacementParameters.contains(parameter->getName())) return {nullptr, nullptr};
            return {&static_cast<ofxOceanodeAbstractParameter&>(replacementParameters.get(parameter->getName())), instance.owner};
        }
        return {parameter, nullptr};
    };
    struct Reconnection { ofxOceanodeAbstractParameter* source; ofxOceanodeAbstractParameter* sink; ofxOceanodeContainer* owner; bool persistent; };
    std::vector<Reconnection> reconnections;
    for(const auto& link : links){
        auto source = resolve(link.source);
        auto sink = resolve(link.sink);
        auto* owner = source.second != nullptr ? source.second : sink.second;
        if(source.first == link.source && sink.first == link.sink) continue;  // neither end was replaced
        if(source.first == nullptr || sink.first == nullptr || owner == nullptr){
            success = false;
            continue;
        }
        reconnections.push_back({source.first, sink.first, owner, link.persistent});
    }

    // Old nodes go first, so the inputs they fed are free for the replacements.
    for(auto& instance : instances){
        if(instance.replacement != nullptr) instance.node->deleteSelf();
    }
    for(const auto& reconnection : reconnections){
        auto* connection = reconnection.owner->createConnection(*reconnection.source, *reconnection.sink);
        if(connection != nullptr) connection->setIsPersistent(reconnection.persistent);
        else success = false;
    }
    return success;
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

std::shared_ptr<oscVariablesGroup> ofxOceanodeOSCVariablesController::findGroup(const std::string& groupName) const{
    auto it = std::find_if(groups.begin(), groups.end(), [&](const auto& group){return group->name == groupName;});
    return it == groups.end() ? nullptr : *it;
}

bool ofxOceanodeOSCVariablesController::isGroupNameAvailable(const std::string& groupName, const oscVariablesGroup* ignore) const{
    if(groupName.empty()) return false;
    // Groups are found by name whatever their mode, and node identities are
    // escaped ("OSC_Sender_My_Group_1"), so compare those.
    const std::string escaped = escapedName(groupName);
    return std::none_of(groups.begin(), groups.end(), [&](const auto& group){
        return group.get() != ignore && escapedName(group->name) == escaped;
    });
}

void ofxOceanodeOSCVariablesController::draw() {
    if(ImGui::MenuItem("Save OSC Variables", unsavedChanges ? "unsaved" : nullptr)) save();
    if(ImGui::MenuItem("Load OSC Variables")) load();
    ofxOceanodeMenu::section("Groups");
    ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
    if(ImGui::MenuItem("New Group...")) newGroupRequested = true;
    ImGui::PopItemFlag();

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
        if(ImGui::SmallButton("Rename##group")) {
            renameGroupName = group->name;
            renameGroupRequested = true;
        }
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
            if(configChanged) {
                group->resetOSCConnection();
                unsavedChanges = true;
            }

            drawGroupTable(group);
            if(ImGui::Button("New Variable...")) {
                newVariableGroupName = group->name;
                newVariableRequested = true;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    for(const auto& edit : pendingEdits){
        auto group = findGroup(edit.group);
        if(group == nullptr) continue;
        switch(edit.kind){
            case PendingEdit::Rename:{
                const std::string newName = variableName(edit.newName);
                if(newName != edit.parameter && group->renameParameter(edit.parameter, newName)) unsavedChanges = true;
                break;
            }
            case PendingEdit::Retype:
                if(group->setParameterType(edit.parameter, edit.type)) unsavedChanges = true;
                renamingParameter = nullptr;
                break;
            case PendingEdit::Move:
                if(group->moveParameter(edit.parameter, edit.index)) unsavedChanges = true;
                break;
            case PendingEdit::Remove:
                group->removeParameter(edit.parameter);
                renamingParameter = nullptr;
                unsavedChanges = true;
                break;
        }
    }
    pendingEdits.clear();

    if(!groupToDelete.empty()) {
        editablePorts.erase(groupToDelete);
        editableHosts.erase(groupToDelete);
        groups.erase(std::remove_if(groups.begin(), groups.end(), [&](const auto& group) {
            return group->name == groupToDelete;
        }), groups.end());
        unsavedChanges = true;
    }

    drawDialogs();
}

void ofxOceanodeOSCVariablesController::drawGroupTable(const std::shared_ptr<oscVariablesGroup>& group){
    if(group->parameters.empty()){
        ImGui::TextDisabled("No variables");
        return;
    }
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
    if(!ImGui::BeginTable("##variables", 4, flags)) return;
    const float fontSize = ImGui::GetFontSize();
    ImGui::TableSetupColumn("##order", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, fontSize * 12.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, fontSize * 7.0f);
    ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed);
    // Headers take the theme's title colour.
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
    ImGui::TableHeadersRow();
    ImGui::PopStyleColor();

    const int rowCount = int(group->parameters.size());
    for(int row = 0; row < rowCount; row++){
        const auto& parameter = group->parameters[row];
        ofAbstractParameter& absParam = *parameter;
        const std::string parameterName = absParam.getName();
        ImGui::PushID(parameter.get());
        ImGui::TableNextRow();

        // Order.
        ImGui::TableNextColumn();
        PendingEdit move{PendingEdit::Move, group->name, parameterName};
        ImGui::BeginDisabled(row == 0);
        if(ImGui::ArrowButton("##up", ImGuiDir_Up)){ move.index = row - 1; pendingEdits.push_back(move); }
        ImGui::EndDisabled();
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::BeginDisabled(row + 1 == rowCount);
        if(ImGui::ArrowButton("##down", ImGuiDir_Down)){ move.index = row + 1; pendingEdits.push_back(move); }
        ImGui::EndDisabled();

        // Address: the name, edited in place and applied when the field is left.
        ImGui::TableNextColumn();
        const bool editing = renamingParameter == parameter.get();
        char rowName[sizeof(variableNameBuffer)];
        copyName(rowName, sizeof(rowName), parameterName);
        char* nameBuffer = editing ? variableNameBuffer : rowName;
        const std::string proposedName = variableName(std::string(nameBuffer));
        const bool invalidName = editing && proposedName != parameterName &&
            (!group->isParameterNameAvailable(proposedName, parameter.get()) || escapedName(proposedName) == escapedName(parameterName));
        ImGui::TextDisabled("/");
        ImGui::SameLine(0, 0);
        if(invalidName) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##name", nameBuffer, sizeof(variableNameBuffer), ImGuiInputTextFlags_AutoSelectAll);
        if(invalidName) ImGui::PopStyleColor();
        if(ImGui::IsItemActivated()){
            renamingParameter = parameter.get();
            copyName(variableNameBuffer, sizeof(variableNameBuffer), parameterName);
        }
        if(invalidName && ImGui::IsItemHovered()){
            ImGui::SetTooltip("Another variable or pin in this group already uses this name.\nSpaces and symbols count as underscores.");
        }
        if(ImGui::IsItemDeactivated()){
            if(ImGui::IsItemDeactivatedAfterEdit()){
                pendingEdits.push_back({PendingEdit::Rename, group->name, parameterName, std::string(variableNameBuffer)});
            }
            renamingParameter = nullptr;
        }else if(editing && !ImGui::IsItemActive() && !ImGui::IsItemActivated()){
            // The menu closed mid-edit without submitting the field.
            renamingParameter = nullptr;
        }

        // Type: changing it keeps the row and the connections the new type accepts.
        ImGui::TableNextColumn();
        const int type = oscVariableTypeOf(absParam);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::BeginCombo("##type", oscVariableTypeName(type))){
            for(int t = 0; t < oscVariableType_Count; t++){
                if(ImGui::Selectable(oscVariableTypeName(t), t == type) && t != type){
                    PendingEdit edit{PendingEdit::Retype, group->name, parameterName};
                    edit.type = t;
                    pendingEdits.push_back(edit);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextColumn();
        if(ImGui::Button("[-]")) pendingEdits.push_back({PendingEdit::Remove, group->name, parameterName});
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void ofxOceanodeOSCVariablesController::drawDialogs() {
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
    if(renameGroupRequested) {
        copyName(groupNameBuffer, sizeof(groupNameBuffer), renameGroupName);
        ImGui::OpenPopup("Rename OSC Variables Group");
        renameGroupRequested = false;
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

        const string name = ofTrim(string(newGroupName));
        const bool duplicate = !name.empty() && !isGroupNameAvailable(name);
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
            unsavedChanges = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopupModal("Rename OSC Variables Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto group = findGroup(renameGroupName);
        if(group == nullptr) {
            ImGui::TextUnformatted("This group is no longer available.");
            if(ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        } else {
            if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 22.0f);
            const bool enter = ImGui::InputText("Name", groupNameBuffer, sizeof(groupNameBuffer),
                                                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            const string proposedName = ofTrim(string(groupNameBuffer));
            const bool duplicate = !proposedName.empty() && !isGroupNameAvailable(proposedName, group.get());
            const bool canRename = !proposedName.empty() && !duplicate && proposedName != group->name;
            if(duplicate) ImGui::TextDisabled("A group with this name already exists.");
            if(!group->nodes.empty()) {
                const int count = int(group->nodes.size());
                const string prefix = group->oscMode == OscMode::Sender ? "OSC Sender " : "OSC Receiv. ";
                ImGui::Text("%d \"%s\" node%s in the patch will be replaced by \"%s%s\".",
                            count, group->modelName().c_str(), count == 1 ? "" : "s",
                            prefix.c_str(), proposedName.empty() ? "..." : proposedName.c_str());
                ImGui::TextDisabled("Connections are kept. Canvas undo history is cleared, and saved\n"
                                    "presets that use the old name won't find these nodes.");
            }
            ImGui::BeginDisabled(!canRename);
            const bool rename = ImGui::Button("Rename");
            ImGui::EndDisabled();
            if(canRename && (enter || rename)) {
                const string oldName = group->name;
                if(!group->rename(proposedName)) {
                    ofLogWarning("OSC Variables") << "Renamed group " << proposedName << ", but some nodes or connections could not be restored";
                }
                auto portIt = editablePorts.find(oldName);
                if(portIt != editablePorts.end()) {
                    editablePorts[group->name] = portIt->second;
                    editablePorts.erase(oldName);
                }
                auto hostIt = editableHosts.find(oldName);
                if(hostIt != editableHosts.end()) {
                    editableHosts[group->name] = hostIt->second;
                    editableHosts.erase(oldName);
                }
                unsavedChanges = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopupModal("New OSC Variable", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto group = findGroup(newVariableGroupName);
        if(group == nullptr) {
            ImGui::TextUnformatted("This group is no longer available.");
            if(ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        } else {
            ImGui::Text("New variable for %s", group->name.c_str());
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
            ImGui::Combo("Type", &newVariableType, variableTypeNames, oscVariableType_Count);
            if(ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 22.0f);
            const bool enter = ImGui::InputText("Address", newVariableName, sizeof(newVariableName),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
            const string name = variableName(string(newVariableName));
            const bool duplicate = !name.empty() && !group->isParameterNameAvailable(name);
            const bool canCreate = !name.empty() && !duplicate;
            if(duplicate) ImGui::TextDisabled("A variable or pin with this name already exists.");
            ImGui::BeginDisabled(!canCreate);
            const bool create = ImGui::Button("Create");
            ImGui::EndDisabled();
            if(canCreate && (create || enter)) {
                group->addParameter(createOscVariable(newVariableType, name));
                unsavedChanges = true;
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

        // Store parameters, in table order
        ofJson parametersJson = ofJson::array();
        for(const auto& param : group->parameters) {
            const int type = oscVariableTypeOf(*param);
            if(type < 0) continue;
            ofJson paramJson;
            paramJson["name"] = param->getName();
            paramJson["type"] = variableTypeKeys[type];
            parametersJson.push_back(paramJson);
        }
        groupJson["parameters"] = parametersJson;

        json["groups"].push_back(groupJson);
    }

    if(ofSavePrettyJson("oscVars.json", json)) {
        unsavedChanges = false;
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
    renamingParameter = nullptr;
    pendingEdits.clear();

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
                    const int type = typeFromKey(paramJson.value("type", ""));
                    if(!paramName.empty() && type >= 0) {
                        newGroup->addParameter(createOscVariable(type, paramName));
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
    unsavedChanges = false;
}
