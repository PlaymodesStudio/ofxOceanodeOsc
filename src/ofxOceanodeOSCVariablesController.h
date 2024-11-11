#ifndef ofxOceanodeOSCVariablesController_h
#define ofxOceanodeOSCVariablesController_h

#include "ofxOceanodeBaseController.h"
#include "ofxOsc.h"

#include <sys/socket.h>
#include <arpa/inet.h>

// Forward declare
class oscVariables;

enum class OscMode {
    Sender,
    Receiver
};

class oscVariablesGroup : public std::enable_shared_from_this<oscVariablesGroup> {
public:
    // Default constructor
    oscVariablesGroup() = default;
    
    // Constructor declaration - make sure this matches the cpp file exactly
    oscVariablesGroup(std::string _name,
                     std::shared_ptr<ofxOceanodeContainer> _container,
                     OscMode mode,
                     int portAux,
                     std::string host);
    
    // Destructor
    ~oscVariablesGroup();
    
    void initializeOSC();
    void registerModule();
    void addNode(oscVariables* node);
    void removeNode(oscVariables* node);
    void addFloatParameter(std::string parameterName, float value = 0);
    void addStringParameter(std::string parameterName, std::string value = "");
    void addFloatVectorParameter(std::string parameterName, std::vector<float> value = std::vector<float>());
    void addStringVectorParameter(std::string parameterName, std::vector<string> value = std::vector<string>());
    void addIntParameter(std::string parameterName, int value = 0);
    void addIntVectorParameter(std::string parameterName, std::vector<int> value = std::vector<int>());
        
    void removeParameter(std::string parameterName);
    void resetOSCConnection();
    void update();
    
//    bool isMyOSCPortAvailable(int port);
    
    std::string name;
    OscMode oscMode;
    
    ofParameter<int> portParam;
    ofParameter<string> ipParam;
    
    ofxOscSender sender;
    ofxOscReceiver receiver;
    
    std::vector<std::shared_ptr<ofAbstractParameter>> parameters;
    std::vector<oscVariables*> nodes;
    std::shared_ptr<ofxOceanodeContainer> container;
    
private:
    std::mutex parameterMutex;
};

//-------------------------------------------------------------------------
class ofxOceanodeOSCVariablesController: public ofxOceanodeBaseController
//-------------------------------------------------------------------------
{
public:
    ofxOceanodeOSCVariablesController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeOSCVariablesController();
    
    void draw();
    
    void save();
    void load();
    
private:
    shared_ptr<ofxOceanodeContainer> container;
    
    std::vector<std::shared_ptr<oscVariablesGroup>> groups;
};

#endif /* ofxOceanodeOSCVariablesController_h */
