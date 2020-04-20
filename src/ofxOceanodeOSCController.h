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
    ofxOceanodeOSCController(shared_ptr<ofxOceanodeContainer> _container);
    ~ofxOceanodeOSCController(){};
    
    void draw();
private:
    shared_ptr<ofxOceanodeContainer> container;
};

#endif /* ofxOceanodeOSCController_h */
