#ifndef _IOTLINKDEVICEINTERFACE_
#define _IOTLINKDEVICEINTERFACE_

#include <IotlinkInterface.h>

class IotLinkDeviceInterface {
  public:
    virtual bool handleRequest(const char* deviceId, const char* action, JsonObject &request_value, JsonObject &response_value) = 0;
    virtual const char* getDeviceId() = 0;
    virtual void begin(IotLinkInterface* eventSender) = 0;
  protected:
    virtual bool sendEvent(JsonDocument& event) = 0;
    virtual DynamicJsonDocument prepareEvent(const char* deviceId, const char* action, const char* cause) = 0;
};

#endif