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
        string host = controller->addSender(additionalName);
        
        addParameter(oscHost.set("Host", host), ofxOceanodeParameterFlags_DisableSavePreset);
        addParameter(oscPort.set("Port", "11511"));
        
        sender.setup(oscHost, ofToInt(oscPort));
        
        listeners.push(controller->hostEvents[additionalName].newListener([this](string &s){
            oscHost = s;
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
					if (!disable) {
						ofxOscMessage message;
						message.setAddress("/" + additionalName + "/" + f.getName());
                        message.addFloatArg(ofClamp(f_, f.getMin(), f.getMax()));
						sender.sendMessage(message);
					}
                }));
            }else if(ss[0] == "vf"){
                ofParameter<vector<float>> vf;
                addParameter(vf.set(ss[1], vector<float>(1, ofToFloat(ss[2])), vector<float>(1, ofToFloat(ss[2])), vector<float>(1, ofToFloat(ss[3]))));
                listeners.push(vf.newListener([this, vf](vector<float> &vf_){
					if (!disable) {
						ofxOscMessage message;
						message.setAddress("/" + additionalName + "/" + vf.getName());
						for (auto f : vf_) {
							message.addFloatArg(ofClamp(f, vf.getMin()[0], vf.getMax()[0]));
						}
						sender.sendMessage(message);
					}
                }));
            }
			else if(ss[0] == "i"){
                ofParameter<int> i;
                addParameter(i.set(ss[1], ofToInt(ss[2]), ofToInt(ss[2]), ofToInt(ss[3])));
                listeners.push(i.newListener([this, i](int &i_){
					if (!disable) {
						ofxOscMessage message;
						message.setAddress("/" + additionalName + "/" + i.getName());
						message.addIntArg(ofClamp(i_, i.getMin(), i.getMax()));
						sender.sendMessage(message);
					}
                }));
            }
        }

		disable = false;
    }

	void presetWillBeLoaded() override {
		disable = true;
	}

	void presetHasLoaded() override {
		disable = false;

		for (int i = 0; i < getParameterGroup().size(); i++) {
			ofxOceanodeAbstractParameter &absParam = static_cast<ofxOceanodeAbstractParameter&>(getParameterGroup().get(i));
			if (absParam.valueType() == typeid(float).name())
			{
				auto tempCast = absParam.cast<float>().getParameter();
				ofxOscMessage message;
				message.setAddress("/" + additionalName + "/" + tempCast.getName());
				message.addFloatArg(ofClamp(tempCast, tempCast.getMin(), tempCast.getMax()));
				sender.sendMessage(message);

			}
			else if (absParam.valueType() == typeid(int).name())
			{
				auto tempCast = absParam.cast<int>().getParameter();

				ofxOscMessage message;
				message.setAddress("/" + additionalName + "/" + tempCast.getName());
				message.addIntArg(ofClamp(tempCast, tempCast.getMin(), tempCast.getMax()));
				sender.sendMessage(message);
			}
			else if (absParam.valueType() == typeid(std::vector<float>).name())
			{
				auto tempCast = absParam.cast<std::vector<float>>().getParameter();

				ofxOscMessage message;
				message.setAddress("/" + additionalName + "/" + tempCast.getName());
				for (auto f : tempCast.get()) {
					message.addFloatArg(ofClamp(f, tempCast.getMin()[0], tempCast.getMax()[0]));
				}
				sender.sendMessage(message);
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

	bool disable;
};

#endif /* oscSender_h */
