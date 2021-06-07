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
    ofxOceanodeOSCController(std::shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeOSCController();
    
    void draw();
    
    void save();
    
    pair<string, string> addSender(string name);
    
    map<string, ofEvent<string>> hostEvents;
    map<string, ofEvent<string>> portEvents;
private:
    std::shared_ptr<ofxOceanodeContainer> container;
    map<string, string> hosts;
    map<string, string> ports;
    
    ofJson json;
};

#endif /* ofxOceanodeOSCController_h */
