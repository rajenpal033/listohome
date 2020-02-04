#ifndef _IOTLINKCONTROL_H_
#define _IOTLINKCONTROL_H_

#include "IotLinkDevice.h"

class IotLinkControl :  public IotLinkDevice {
  public:
    IotLinkControl(const char* deviceId, unsigned long eventWaitTime=60000);

    // event
    bool sendSensorEvent(JsonObject value, const char* action, String cause = "PERIODIC_POLL");

  private:
};

IotLinkControl::IotLinkControl(const char* deviceId, unsigned long eventWaitTime) : IotLinkDevice(deviceId, eventWaitTime) {}


bool IotLinkControl::sendSensorEvent(JsonObject value, const char* action, String cause) {

  DynamicJsonDocument eventMessage = prepareEvent(deviceId, action, cause.c_str());
  //JsonObject event_value = eventMessage["payload"]["value"];
  //event_value = value;
  eventMessage["payload"]["value"] = value;
  serializeJson(eventMessage, Serial);
  //Serial.println(event_value);
  return sendEvent(eventMessage);

}

#endif

