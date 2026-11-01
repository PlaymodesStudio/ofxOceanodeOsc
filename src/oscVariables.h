// oscVariables.h
#ifndef oscVariables_h
#define oscVariables_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOceanodeOSCVariablesController.h" // Include the full definition instead of forward declaration

class oscVariables : public ofxOceanodeNodeModel {
public:
    oscVariables(const std::string& name, const std::weak_ptr<oscVariablesGroup>& group);
    ~oscVariables();

    void setup() override;
    void update(ofEventArgs& e) override; // Add the update method

    // A variable pin of the group, sharing the group's parameter, so a rename
    // of the group parameter renames the pin. In a Receiver group the node
    // writes it from incoming OSC (oscVariablesGroup::update), so it is an
    // output, with the same flags addOutputParameter sets. In a Sender group
    // it is an input.
    shared_ptr<ofxOceanodeAbstractParameter> addVariableParameter(ofAbstractParameter& param, ofxOceanodeParameterFlags flags = 0);
    void removeVariableParameter(const std::string& parameterName);
    // Swaps the pin for the group's new parameter of another type at the same
    // place, reconnecting whatever the new type accepts.
    void replaceVariableParameter(const std::string& parameterName, ofAbstractParameter& newParameter);
    // Puts the variable pins in the group's row order.
    void sortVariables(const std::vector<std::shared_ptr<ofAbstractParameter>>& groupParameters);
    std::weak_ptr<oscVariablesGroup> getGroup() {return group;};

private:
    std::string name;
    std::weak_ptr<oscVariablesGroup> group;
};

#endif /* oscVariables_h */
