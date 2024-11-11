#include "oscVariables.h"
#include "ofxOceanodeOSCVariablesController.h"

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
    parameterListeners.clear();
    
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

shared_ptr<ofxOceanodeAbstractParameter> oscVariables::addParameter(ofAbstractParameter& param, ofxOceanodeParameterFlags flags) {
    auto result = ofxOceanodeNodeModel::addParameter(param, flags);
    if(!result) {
        ofLogError("oscVariables") << "Failed to add parameter to node model: " << param.getName();
        return nullptr;
    }
    
    try {
        // Handle float parameters
        auto* floatParam = dynamic_cast<ofParameter<float>*>(&param);
        if(floatParam != nullptr) {
            auto listener = floatParam->newListener([this, paramName = param.getName()](float& value) {
                auto sharedGroup = group.lock();
                if(sharedGroup && sharedGroup->oscMode == OscMode::Sender) {
                    ofxOscMessage msg;
                    msg.setAddress("/" + paramName);
                    msg.addFloatArg(value);
                    sharedGroup->sender.sendMessage(msg);
                }
            });
            parameterListeners[param.getName()] = std::make_shared<ofEventListener>(std::move(listener));
        }
        auto* intParam = dynamic_cast<ofParameter<int>*>(&param);
        if(intParam != nullptr) {
            auto listener = intParam->newListener([this, paramName = param.getName()](int& value) {
                auto sharedGroup = group.lock();
                if(sharedGroup && sharedGroup->oscMode == OscMode::Sender) {
                    ofxOscMessage msg;
                    msg.setAddress("/" + paramName);
                    msg.addIntArg(value);
                    sharedGroup->sender.sendMessage(msg);
                }
            });
            parameterListeners[param.getName()] = std::make_shared<ofEventListener>(std::move(listener));
        }
        // Handle string parameters
        auto* stringParam = dynamic_cast<ofParameter<string>*>(&param);
        if(stringParam != nullptr) {
            auto listener = stringParam->newListener([this, paramName = param.getName()](string& value) {
                auto sharedGroup = group.lock();
                if(sharedGroup && sharedGroup->oscMode == OscMode::Sender) {
                    ofxOscMessage msg;
                    msg.setAddress("/" + paramName);
                    msg.addStringArg(value);
                    sharedGroup->sender.sendMessage(msg);
                }
            });
            parameterListeners[param.getName()] = std::make_shared<ofEventListener>(std::move(listener));
        }
        
        // Handle vector<float> parameters
        auto* floatVectorParam = dynamic_cast<ofParameter<vector<float>>*>(&param);
        if(floatVectorParam != nullptr) {
            auto listener = floatVectorParam->newListener([this, paramName = param.getName()](vector<float>& values) {
                auto sharedGroup = group.lock();
                if(sharedGroup && sharedGroup->oscMode == OscMode::Sender) {
                    ofxOscMessage msg;
                    msg.setAddress("/" + paramName);
                    for(const auto& value : values) {
                        msg.addFloatArg(value);
                    }
                    sharedGroup->sender.sendMessage(msg);
                }
            });
            parameterListeners[param.getName()] = std::make_shared<ofEventListener>(std::move(listener));
        }
        auto* intVectorParam = dynamic_cast<ofParameter<vector<int>>*>(&param);
        if(intVectorParam != nullptr) {
            auto listener = intVectorParam->newListener([this, paramName = param.getName()](vector<int>& values) {
                auto sharedGroup = group.lock();
                if(sharedGroup && sharedGroup->oscMode == OscMode::Sender) {
                    ofxOscMessage msg;
                    msg.setAddress("/" + paramName);
                    for(const auto& value : values) {
                        msg.addIntArg(value);
                    }
                    sharedGroup->sender.sendMessage(msg);
                }
            });
            parameterListeners[param.getName()] = std::make_shared<ofEventListener>(std::move(listener));
        }
        // Handle vector<string> parameters
        auto* stringVectorParam = dynamic_cast<ofParameter<vector<string>>*>(&param);
        if(stringVectorParam != nullptr) {
            auto listener = stringVectorParam->newListener([this, paramName = param.getName()](vector<string>& values) {
                auto sharedGroup = group.lock();
                if(sharedGroup && sharedGroup->oscMode == OscMode::Sender) {
                    ofxOscMessage msg;
                    msg.setAddress("/" + paramName);
                    for(const auto& value : values) {
                        msg.addStringArg(value);
                    }
                    sharedGroup->sender.sendMessage(msg);
                }
            });
            parameterListeners[param.getName()] = std::make_shared<ofEventListener>(std::move(listener));
        }
        
    } catch(const std::exception& e) {
        ofLogError("oscVariables") << "Exception in addParameter: " << e.what();
        return nullptr;
    }
    
    return result;
}
