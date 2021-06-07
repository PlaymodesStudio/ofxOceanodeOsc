//
//  oscSender.h
//  Oceanode
//
//  Created by Eduard Frigola Bagué on 17/02/2021.
//

#ifndef oscSender_h
#define oscSender_h

#include "ofxOceanodeNodeModel.h"
#include "ofxOsc.h"
#include "ofxOceanodeOSCController.h"

class oscSender : public ofxOceanodeNodeModel{
public:
    oscSender(string name, string config, shared_ptr<ofxOceanodeOSCController> _controller) : additionalName(name), configuration(config), controller(_controller), ofxOceanodeNodeModel("Osc Sender " + name){
        controller->addSender(additionalName);
    };
    
    void setup(){
        auto senderInfo = controller->addSender(additionalName);
        
        addParameter(oscHost.set("Host", senderInfo.first), ofxOceanodeParameterFlags_DisableSavePreset);
        addParameter(oscPort.set("Port", senderInfo.second), ofxOceanodeParameterFlags_DisableSavePreset);
        
        sender.setup(oscHost, ofToInt(oscPort));
        
        listeners.push(controller->hostEvents[additionalName].newListener([this](string &s){
            oscHost = s;
        }));
        listeners.push(controller->portEvents[additionalName].newListener([this](string &s){
            oscPort = s;
        }));
        
        listeners.push(oscHost.newListener([this](string &s){
            sender.setup(oscHost, ofToInt(oscPort));
        }));
        
        listeners.push(oscPort.newListener([this](string &s){
            sender.setup(oscHost, ofToInt(oscPort));
        }));
        
        vector<string> splittedConfig = ofSplitString(configuration, ", ");
        for(string &s : splittedConfig){
            vector<string> ss = ofSplitString(s, ":");
            if(ss[0] == "f"){
                ofParameter<float> f;
                addParameter(f.set(ss[1], ofToFloat(ss[2]), ofToFloat(ss[2]), ofToFloat(ss[3])));
                listeners.push(f.newListener([this, f](float &f_){
                    ofxOscMessage message;
                    message.setAddress("/" + additionalName + "/" + f.getName());
                    message.addFloatArg(f_);
                    sender.sendMessage(message);
                }));
            }else if(ss[0] == "vf"){
                ofParameter<vector<float>> vf;
                addParameter(vf.set(ss[1], vector<float>(1, ofToFloat(ss[2])), vector<float>(1, ofToFloat(ss[2])), vector<float>(1, ofToFloat(ss[3]))));
                listeners.push(vf.newListener([this, vf](vector<float> &vf_){
                    ofxOscMessage message;
                    message.setAddress("/" + additionalName + "/" + vf.getName());
                    for(auto f : vf_){
                        message.addFloatArg(f);
                    }
                    sender.sendMessage(message);
                }));
            }
        }
    }
    
private:
    string additionalName;
    string configuration;
    shared_ptr<ofxOceanodeOSCController> controller;
    
    ofxOscSender sender;
    
    ofParameter<string> oscHost;
    ofParameter<string> oscPort;
    
    ofEventListeners listeners;
};

#endif /* oscSender_h */
