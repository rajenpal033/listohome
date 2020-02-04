#ifndef _IOTLINK_INTERFACE_H_
#define _IOTLINK_INTERFACE_H_

#include "ArduinoJson.h"

class IotLinkInterface {
  public:
    virtual void sendMessage(JsonDocument& jsonEvent);
    virtual DynamicJsonDocument prepareEvent(const char* deviceId, const char* action, const char* cause);
};


#endif