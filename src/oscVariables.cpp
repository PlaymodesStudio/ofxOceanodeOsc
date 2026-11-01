#include "oscVariables.h"
#include "ofxOceanodeOSCVariablesController.h"
#include "ofxOceanodeContainer.h"

#include <set>

oscVariables::oscVariables(const std::string& name, const std::weak_ptr<oscVariablesGroup>& group)
: ofxOceanodeNodeModel(
    [&group, &name]() -> string {
        auto sharedGroup = group.lock();
        return sharedGroup ? (sharedGroup->oscMode == OscMode::Sender ?
            "OSC Sender " + name :
            "OSC Receiv. " + name) :
            "OSC " + name;
    }())
, name(name)
, group(group)
{
    description = "Contains all the variables stored globally for the " + name + " category";
}

oscVariables::~oscVariables() {
    std::shared_ptr<oscVariablesGroup> sharedGroup = group.lock();
    if(sharedGroup) {
        sharedGroup->removeNode(this);
    }
}

void oscVariables::setup() {
    std::shared_ptr<oscVariablesGroup> sharedGroup = group.lock();
    if(sharedGroup) {
        sharedGroup->addNode(this);
    }
}

void oscVariables::update(ofEventArgs& e)
{
    if(auto groupPtr = group.lock())
    {
        groupPtr->update();
    }
}

shared_ptr<ofxOceanodeAbstractParameter> oscVariables::addVariableParameter(ofAbstractParameter& param, ofxOceanodeParameterFlags flags) {
    auto sharedGroup = group.lock();
    if(sharedGroup && sharedGroup->oscMode == OscMode::Receiver) {
        flags |= outputParameterFlags;
    }
    auto result = addParameter(param, flags);
    if(!result) {
        ofLogError("oscVariables") << "Failed to add parameter to node model: " << param.getName();
    }
    return result;
}

void oscVariables::removeVariableParameter(const std::string& parameterName) {
    if(!getParameterGroup().contains(parameterName)) return;
    static_cast<ofxOceanodeAbstractParameter&>(getParameterGroup().get(parameterName)).removeAllConnections();
    ofxOceanodeNodeModel::removeParameter(parameterName);
    parameterGroupChanged.notify();
}

void oscVariables::replaceVariableParameter(const std::string& parameterName, ofAbstractParameter& newParameter) {
    auto& nodeParameters = getParameterGroup();
    if(!nodeParameters.contains(parameterName)) return;

    struct Link { ofxOceanodeAbstractParameter* other; bool persistent; };
    std::vector<Link> outLinks;
    std::vector<Link> inLinks;
    auto& oldParameter = static_cast<ofxOceanodeAbstractParameter&>(nodeParameters.get(parameterName));
    for(auto* connection : oldParameter.getOutConnections()) {
        outLinks.push_back({&connection->getSinkParameter(), connection->getIsPersistent()});
    }
    for(auto* connection : oldParameter.getInConnections()) {
        inLinks.push_back({&connection->getSourceParameter(), connection->getIsPersistent()});
    }
    std::vector<std::string> order;
    for(int i = 0; i < nodeParameters.size(); i++) order.push_back(nodeParameters.get(i).getName());

    removeVariableParameter(parameterName);
    auto pin = addVariableParameter(newParameter, ofxOceanodeParameterFlags_DisableSavePreset);
    nodeParameters.reorder(order);

    // Connections the new type can't take are dropped.
    auto* host = getHostContainer();
    if(host != nullptr && pin != nullptr) {
        for(const auto& link : outLinks) {
            auto* connection = host->createConnection(*pin, *link.other);
            if(connection != nullptr) connection->setIsPersistent(link.persistent);
        }
        for(const auto& link : inLinks) {
            auto* connection = host->createConnection(*link.other, *pin);
            if(connection != nullptr) connection->setIsPersistent(link.persistent);
        }
    }
    parameterGroupChanged.notify();
}

void oscVariables::sortVariables(const std::vector<std::shared_ptr<ofAbstractParameter>>& groupParameters) {
    auto& nodeParameters = getParameterGroup();
    std::vector<std::string> rows;
    std::set<std::string> variablePins;
    for(const auto& parameter : groupParameters) {
        if(!nodeParameters.contains(parameter->getName())) continue;
        rows.push_back(parameter->getName());
        variablePins.insert(parameter->getEscapedName());
    }

    // Refill the slots the variable pins hold, so the port and host pins stay put.
    std::vector<std::string> order;
    size_t next = 0;
    for(int i = 0; i < nodeParameters.size(); i++) {
        auto& parameter = nodeParameters.get(i);
        if(variablePins.count(parameter.getEscapedName()) != 0 && next < rows.size()) {
            order.push_back(rows[next++]);
        } else {
            order.push_back(parameter.getName());
        }
    }
    nodeParameters.reorder(order);
    parameterGroupChanged.notify();
}
