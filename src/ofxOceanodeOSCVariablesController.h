#ifndef ofxOceanodeOSCVariablesController_h
#define ofxOceanodeOSCVariablesController_h

#include "ofxOceanodeBaseController.h"
#include "ofxOsc.h"

#include <array>
#include <map>

#include <sys/socket.h>
#include <arpa/inet.h>

// Forward declare
class oscVariables;

enum class OscMode {
    Sender,
    Receiver
};

// Variable types, in the order the Type column lists them.
enum oscVariableType {
    oscVariableType_Float = 0,
    oscVariableType_FloatVector,
    oscVariableType_Int,
    oscVariableType_IntVector,
    oscVariableType_String,
    oscVariableType_StringVector,
    oscVariableType_Count
};

const char* oscVariableTypeName(int type);
// -1 when the parameter is not one of the variable types.
int oscVariableTypeOf(const ofAbstractParameter& parameter);
// A new variable of the given type holding the type's default value.
std::shared_ptr<ofAbstractParameter> createOscVariable(int type, const std::string& name);

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
    void unregisterModule();
    // The node type, "OSC Sender <name>" or "OSC Receiv. <name>".
    std::string modelName() const;
    void addNode(oscVariables* node);
    void removeNode(oscVariables* node);
    void addParameter(std::shared_ptr<ofAbstractParameter> parameter);
    void addFloatParameter(std::string parameterName, float value = 0);
    void addStringParameter(std::string parameterName, std::string value = "");
    void addFloatVectorParameter(std::string parameterName, std::vector<float> value = std::vector<float>());
    void addStringVectorParameter(std::string parameterName, std::vector<string> value = std::vector<string>());
    void addIntParameter(std::string parameterName, int value = 0);
    void addIntVectorParameter(std::string parameterName, std::vector<int> value = std::vector<int>());

    void removeParameter(std::string parameterName);

    // Variables are matched by escaped name, as node parameters are, so
    // "My Var" and "My_Var" can't both exist, and none may take the name of
    // the node's port or host pin.
    bool isParameterNameAvailable(const std::string& parameterName, const ofAbstractParameter* ignore = nullptr) const;
    // The name is the OSC address, so renaming changes what is sent or matched.
    bool renameParameter(const std::string& oldName, const std::string& newName);
    // Keeps the variable's place in every node and the connections the new
    // type still accepts. The value starts at the new type's default.
    bool setParameterType(const std::string& parameterName, int type);
    // Moves a variable to another row; every node follows the new order.
    bool moveParameter(const std::string& parameterName, int newIndex);
    // Registers the group under its new name and replaces each node of the
    // old type, keeping its place, state and connections.
    bool rename(const std::string& newName);

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
    // In a Sender group each variable sends once per change, at its current name.
    void listen(const std::shared_ptr<ofAbstractParameter>& parameter);

    std::map<const ofAbstractParameter*, std::unique_ptr<of::priv::AbstractEventToken>> senderListeners;
    std::mutex parameterMutex;
};

//-------------------------------------------------------------------------
class ofxOceanodeOSCVariablesController: public ofxOceanodeBaseController
//-------------------------------------------------------------------------
{
public:
    ofxOceanodeOSCVariablesController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeOSCVariablesController();

    void draw() override;
    bool isMenuController() const override { return true; }

    void save();
    void load();

private:
    void drawGroupTable(const std::shared_ptr<oscVariablesGroup>& group);
    // Drawn from inside the menu, so the menu stays open behind them.
    void drawDialogs();
    std::shared_ptr<oscVariablesGroup> findGroup(const std::string& groupName) const;
    bool isGroupNameAvailable(const std::string& groupName, const oscVariablesGroup* ignore = nullptr) const;

    shared_ptr<ofxOceanodeContainer> container;

    std::vector<std::shared_ptr<oscVariablesGroup>> groups;
    std::map<std::string, int> editablePorts;
    std::map<std::string, std::array<char, 16>> editableHosts;

    bool unsavedChanges = false;
    bool newGroupRequested = false;
    bool newVariableRequested = false;
    bool renameGroupRequested = false;
    std::string newVariableGroupName;
    std::string renameGroupName;
    char newGroupName[255] = {};
    char newGroupHost[16] = "127.0.0.1";
    int newGroupPort = 8000;
    int newGroupMode = 0;
    char newVariableName[255] = {};
    char groupNameBuffer[255] = {};
    int newVariableType = 0;

    // Inline variable rename: the row being edited and its text.
    const ofAbstractParameter* renamingParameter = nullptr;
    char variableNameBuffer[255] = {};

    // Edits gathered while drawing, applied once the tables are done.
    struct PendingEdit {
        enum Kind { Rename, Retype, Move, Remove } kind;
        std::string group;
        std::string parameter;
        std::string newName;
        int type = 0;
        int index = 0;
    };
    std::vector<PendingEdit> pendingEdits;
};

#endif /* ofxOceanodeOSCVariablesController_h */
