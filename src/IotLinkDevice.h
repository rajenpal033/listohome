#ifndef _IOTLINKDEVICE_H_
#define _IOTLINKDEVICE_H_

#include "IotLinkDeviceInterface.h"

class IotLinkDevice : public IotLinkDeviceInterface {
  public:
    IotLinkDevice(const char* newDeviceId, unsigned long eventWaitTime=100);
    virtual ~IotLinkDevice();
    virtual const char* getDeviceId();
    virtual void begin(IotLinkInterface* eventSender);
    virtual void setEventWaitTime(unsigned long eventWaitTime) { if (eventWaitTime<100) {this->eventWaitTime=100;} else { this->eventWaitTime=eventWaitTime;} }


    typedef std::function<bool(const String&, bool&)> PowerStateCallback;
    

    // standard request handler
    virtual bool handleRequest(const char* deviceId, const char* action, JsonObject &request_value, JsonObject &response_value);

    // standard Callbacks
    virtual void onPowerState(PowerStateCallback cb);

    // standard events
    bool sendPowerStateEvent(bool state, String cause = "PHYSICAL_INTERACTION");

  protected:
    virtual bool sendEvent(JsonDocument& event);
    virtual DynamicJsonDocument prepareEvent(const char* deviceId, const char* action, const char* cause);
    char* deviceId;
    PowerStateCallback powerStateCallback;
  private:
    IotLinkInterface* eventSender;
    unsigned long eventWaitTime;
};

IotLinkDevice::IotLinkDevice(const char* newDeviceId, unsigned long eventWaitTime) : 
  powerStateCallback(nullptr),
  eventSender(nullptr),
  eventWaitTime(eventWaitTime) {
  deviceId = strdup(newDeviceId);
  if (this->eventWaitTime < 100) this->eventWaitTime = 100;
}

IotLinkDevice::~IotLinkDevice() {
  if (deviceId) free(deviceId);
}

void IotLinkDevice::begin(IotLinkInterface* eventSender) {
  this->eventSender = eventSender;
}

const char* IotLinkDevice::getDeviceId() {
  return deviceId;
}

bool IotLinkDevice::handleRequest(const char* deviceId, const char* action, JsonObject &request_value, JsonObject &response_value) {
  if (strcmp(deviceId, this->deviceId) != 0) return false;
  DEBUG_IOTLINK("IotLinkDevice::handleRequest()\r\n");
  bool success = false;
  String actionString = String(action);
  //Serial.println(actionString);
  if (actionString == "setPowerState" && powerStateCallback) {
    bool powerState = request_value["state"]=="On"?true:false;
    success = powerStateCallback(String(deviceId), powerState);
    response_value["state"] = powerState?"On":"Off";
    return success;
  }
  return success;
}

DynamicJsonDocument IotLinkDevice::prepareEvent(const char* deviceId, const char* action, const char* cause) {
  if (eventSender) return eventSender->prepareEvent(deviceId, action, cause);
  DEBUG_IOTLINK("[IotLinkDevice:prepareEvent()]: Device \"%s\" isn't configured correctly! The \'%s\' event will be ignored.\r\n", deviceId, action);
  return DynamicJsonDocument(1024);
}


bool IotLinkDevice::sendEvent(JsonDocument& event) {

  String eventName = event["payload"]["action"] | "";

    if (eventSender) {
		//serializeJson(event, Serial);
		eventSender->sendMessage(event);
        return true;
	}

  return false;

}

void IotLinkDevice::onPowerState(PowerStateCallback cb) { 
  powerStateCallback = cb; 
}


bool IotLinkDevice::sendPowerStateEvent(bool state, String cause) {
  DynamicJsonDocument eventMessage = prepareEvent(deviceId, "setPowerState", cause.c_str());
  JsonObject event_value = eventMessage["payload"]["value"];
  event_value["state"] = state?"On":"Off";
  return sendEvent(eventMessage);
}

#endif