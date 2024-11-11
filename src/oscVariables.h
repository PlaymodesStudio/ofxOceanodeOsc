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
    
    shared_ptr<ofxOceanodeAbstractParameter> addParameter(ofAbstractParameter& param, ofxOceanodeParameterFlags flags = 0);
    std::weak_ptr<oscVariablesGroup> getGroup() {return group;};
    
private:
    std::string name;
    std::weak_ptr<oscVariablesGroup> group;
    std::map<std::string, std::shared_ptr<ofEventListener>> parameterListeners;
};

#endif /* oscVariables_h */
