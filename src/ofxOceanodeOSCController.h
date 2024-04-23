//
//  ofxOceanodeOSCController.h
//  PLedNodes
//
//  Created by Eduard Frigola on 09/05/2018.
//

#ifndef ofxOceanodeOSCController_h
#define ofxOceanodeOSCController_h

#include "ofxOceanodeBaseController.h"

class ofxOceanodeOSCController : public ofxOceanodeBaseController{
public:
    ofxOceanodeOSCController(ofParameter<int> & _receiverPort);
    ~ofxOceanodeOSCController();
    
    void draw();
    
    void save();
    
    string addSender(string name);
    
    map<string, ofEvent<string>> hostEvents;
private:
    shared_ptr<ofParameter<int>> receiverPortParam;
    map<string, string> hosts;
    
    ofJson json;
};

#endif /* ofxOceanodeOSCController_h */
