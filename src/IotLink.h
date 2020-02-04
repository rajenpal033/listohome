#ifndef _IOTLINK_H_
#define _IOTLINK_H_

#include "IotLinkInterface.h"
#include "IotLinkDeviceInterface.h"
#include "IotLinkWebsocket.h"
#include "IotLinkSignature.h"
#include "IotLinkMessageid.h"


class IotLinkClass : public IotLinkInterface {
  public:
    void begin(String socketAuthToken, String signingKey, String serverURL = IOTLINK_SERVER_URL);
    template <typename DeviceType>
    DeviceType& add(const char* deviceId, unsigned long eventWaitTime = 1000);

    void add(IotLinkDeviceInterface& newDevice);
    void add(IotLinkDeviceInterface* newDevice);
    void handle();
    void stop();
    bool isConnected();

    typedef std::function<void(void)> ConnectedCallbackHandler;

    typedef std::function<void(void)> DisconnectedCallbackHandler;
    void onConnected(ConnectedCallbackHandler cb);
    void onDisconnected(DisconnectedCallbackHandler cb);

    void restoreDeviceStates(bool flag);

    DynamicJsonDocument prepareResponse(JsonDocument& requestMessage);
    DynamicJsonDocument prepareEvent(const char* deviceId, const char* action, const char* cause) override;
    void sendMessage(JsonDocument& jsonMessage) override;

    struct proxy {
      proxy(IotLinkClass* ptr, String deviceId) : ptr(ptr), deviceId(deviceId) {}
      IotLinkClass* ptr;
      String deviceId;
      template <typename DeviceType>
      operator DeviceType&() { return as<DeviceType>(); }
      template <typename DeviceType>
      DeviceType& as() { return ptr->getDeviceInstance<DeviceType>(deviceId); }
    };
    
    proxy operator[](const String deviceId) { return proxy(this, deviceId); }

    // setResponseMessage is is just a workaround until verison 3.x.x will be released
    void setResponseMessage(String &&message) { responseMessageStr = message; }

  private:

    void connect();
    void disconnect();
    void reconnect();

    void onConnect() { DEBUG_IOTLINK("[IotLink]: Connected to \"%s\"!]\r\n", serverURL.c_str()); }
    void onDisconnect() { DEBUG_IOTLINK("[IotLink]: Disconnect\r\n"); }

    bool verifyDeviceId(const char* id);
    bool verifyAppKey(const char* key);
    bool verifyAppSecret(const char* secret);
    void extractTimestamp(JsonDocument &message);

    IotLinkDeviceInterface* getDevice(String deviceId);

    template <typename DeviceType>
    DeviceType& getDeviceInstance(String deviceId);

    std::vector<IotLinkDeviceInterface*> devices;
    String socketAuthToken;
    String signingKey;
    String serverURL;

    websocketListener _websocketListener;

    unsigned long getTimestamp() { return baseTimestamp + (millis()/1000); }
    unsigned long baseTimestamp = 0;

    bool _begin = false;
    String responseMessageStr = "";
};

IotLinkDeviceInterface* IotLinkClass::getDevice(String deviceId) {
  for (auto& device : devices) {
    if (deviceId == String(device->getDeviceId())) return device;
  }
  return nullptr;
}

template <typename DeviceType>
DeviceType& IotLinkClass::getDeviceInstance(String deviceId) { 
  DeviceType* tmp_device = (DeviceType*) getDevice(deviceId);
  if (tmp_device) return *tmp_device;
  
  DEBUG_IOTLINK("[IotLink]: Device \"%s\" does not exist. Creating new device\r\n", deviceId.c_str());
  DeviceType& tmp_deviceInstance = add<DeviceType>(deviceId.c_str());

  if (isConnected()) {
    DEBUG_IOTLINK("[IotLink]: Reconnecting to server.\r\n");
    reconnect();
  }

  return tmp_deviceInstance;
}


void IotLinkClass::begin(String socketAuthToken, String signingKey, String serverURL) {
  bool success = true;
//  if (!verifyAppKey(socketAuthToken.c_str())) {
//    DEBUG_IOTLINK("[IotLink:begin()]: App-Key \"%s\" is invalid!! Please check your app-key!! IotLink will not work!\r\n", socketAuthToken.c_str());
//    success = false;
//  }
//  if (!verifyAppSecret(signingKey.c_str())) {
//    DEBUG_IOTLINK("[IotLink:begin()]: App-Secret \"%s\" is invalid!! Please check your app-secret!! IotLink will not work!\r\n", signingKey.c_str());
//    success = false;
//    return;
//  }

  if(!success) {
    _begin = false;
    return;
  }

  this->socketAuthToken = socketAuthToken;
  this->signingKey = signingKey;
  this->serverURL = serverURL;
  _begin = true;
}

template <typename DeviceType>
DeviceType& IotLinkClass::add(const char* deviceId, unsigned long eventWaitTime) {
  DeviceType* newDevice = new DeviceType(deviceId, eventWaitTime);
  //if (verifyDeviceId(deviceId)){
    DEBUG_IOTLINK("[IotLink:add()]: Adding device with id \"%s\".\r\n", deviceId);
    newDevice->begin(this);
    //if (verifyAppKey(socketAuthToken.c_str()) && verifyAppSecret(signingKey.c_str()))
    _begin = true;
//  } else {
//    DEBUG_IOTLINK("[IotLink:add()]: DeviceId \"%s\" is invalid!! Device will be ignored and will NOT WORK!\r\n", deviceId);
//  }
  devices.push_back(newDevice);
  return *newDevice;
}

__attribute__ ((deprecated("Please use DeviceType& myDevice = IotLink.add<DeviceType>(DeviceId);")))
void IotLinkClass::add(IotLinkDeviceInterface* newDevice) {
  //if (!verifyDeviceId(newDevice->getDeviceId())) return;
  newDevice->begin(this);
  devices.push_back(newDevice);
}

__attribute__ ((deprecated("Please use DeviceType& myDevice = IotLink.add<DeviceType>(DeviceId);")))
void IotLinkClass::add(IotLinkDeviceInterface& newDevice) {
  //if (!verifyDeviceId(newDevice.getDeviceId())) return;
  newDevice.begin(this);
  devices.push_back(&newDevice);
}

void IotLinkClass::handle() {
  static bool begin_error = false;
  if (!_begin) {
    if (!begin_error) {
      DEBUG_IOTLINK("[IotLink:handle()]: ERROR! IotLink.begin() failed or was not called prior to event handler\r\n");
      DEBUG_IOTLINK("[IotLink:handle()]:    -Reasons include an invalid app-key, invalid app-secret or no valid deviceIds)\r\n");
      DEBUG_IOTLINK("[IotLink:handle()]:    -IotLink is disabled! Check earlier log messages for details.\r\n");
      begin_error = true;
    }
    return;
  }


  if (!isConnected()) connect();
  _websocketListener.handle();
}

void IotLinkClass::connect() {
  String deviceList;
  int i = 0;
  for (auto& device : devices) {
    const char* deviceId = device->getDeviceId();
    //if (verifyDeviceId(deviceId)) {
      if (i>0) deviceList += ':';
      deviceList += String(deviceId);
      i++;
    //}
  }
  if (i==0) { // no device have been added! -> do not connect!
    _begin = false;
    DEBUG_IOTLINK("[IotLink]: ERROR! No valid devices available. Please add a valid device first!\r\n");
    return;
  }

  _websocketListener.begin(serverURL, socketAuthToken, signingKey, devices, deviceList);
}


void IotLinkClass::stop() {
  DEBUG_IOTLINK("[IotLink:stop()\r\n");
  _websocketListener.stop();
}

bool IotLinkClass::isConnected() {
  return _websocketListener.isConnected();
};


void IotLinkClass::onConnected(ConnectedCallbackHandler cb) {
  _websocketListener.onConnected(cb);
}

void IotLinkClass::onDisconnected(DisconnectedCallbackHandler cb) {
  _websocketListener.onDisconnected(cb);
}


void IotLinkClass::reconnect() {
  DEBUG_IOTLINK("IotLink:reconnect(): disconnecting\r\n");
  stop();
  DEBUG_IOTLINK("IotLink:reconnect(): connecting\r\n");
  connect();
}

bool IotLinkClass::verifyDeviceId(const char* id) {
  if (strlen(id) != 24) return false;
  int tmp; char tmp_c;
  return sscanf(id, "%4x%4x%4x%4x%4x%4x%c", 
         &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp_c) == 6;
}

bool IotLinkClass::verifyAppKey(const char* key) {
  if (strlen(key) != 36) return false;
  int tmp; char tmp_c;
  return sscanf(key, "%4x%4x-%4x-%4x-%4x-%4x%4x%4x%c",
         &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp_c) == 8;
}

bool IotLinkClass::verifyAppSecret(const char* secret) {
  if (strlen(secret) != 73) return false;
  int tmp; char tmp_c;
  return sscanf(secret, "%4x%4x-%4x-%4x-%4x-%4x%4x%4x-%4x%4x-%4x-%4x-%4x-%4x%4x%4x%c",
         &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp_c) == 16;
}


void IotLinkClass::extractTimestamp(JsonDocument &message) {
  unsigned long tempTimestamp = 0;
  // extract timestamp from timestamp message right after websocket connection is established
  tempTimestamp = message["timestamp"] | 0;
  if (tempTimestamp) {
    baseTimestamp = tempTimestamp - (millis() / 1000);
    DEBUG_IOTLINK("[IotLink:extractTimestamp(): Got Timestamp %lu\r\n", tempTimestamp);
    return;
  }

  // extract timestamp from request message
  tempTimestamp = message["payload"]["createdAt"] | 0;
  if (tempTimestamp) {
    DEBUG_IOTLINK("[IotLink:extractTimestamp(): Got Timestamp %lu\r\n", tempTimestamp);
    baseTimestamp = tempTimestamp - (millis() / 1000);
    return;
  }
}


void IotLinkClass::sendMessage(JsonDocument& jsonMessage) {

    jsonMessage["payload"]["createdAt"] = getTimestamp();
    signMessage(signingKey, jsonMessage);

    String messageString;

    serializeJson(jsonMessage, messageString);

    if(isConnected()) {
      _websocketListener.sendMessage(messageString);
    }

}


void IotLinkClass::restoreDeviceStates(bool flag) { 
  _websocketListener.setRestoreDeviceStates(flag);
}

DynamicJsonDocument IotLinkClass::prepareResponse(JsonDocument& requestMessage) {
  DynamicJsonDocument responseMessage(1024);
  JsonObject header = responseMessage.createNestedObject("header");
  header["payloadVersion"] = 2;
  header["signatureVersion"] = 1;

  JsonObject payload = responseMessage.createNestedObject("payload");
  payload["action"] = requestMessage["payload"]["action"];
  payload["clientId"] = requestMessage["payload"]["clientId"];
  payload["createdAt"] = 0;
  payload["deviceId"] = requestMessage["payload"]["deviceId"];
  payload["message"] = "OK";
  payload["replyToken"] = requestMessage["payload"]["replyToken"];
  payload["success"] = false;
  payload["type"] = "response";
  payload.createNestedObject("value");
  return responseMessage;
}


DynamicJsonDocument IotLinkClass::prepareEvent(const char* deviceId, const char* action, const char* cause) {
  DynamicJsonDocument eventMessage(1024);
  JsonObject header = eventMessage.createNestedObject("header");
  header["payloadVersion"] = 2;
  header["signatureVersion"] = 1;

  JsonObject payload = eventMessage.createNestedObject("payload");
  payload["action"] = action;
  payload["cause"].createNestedObject("type");
  payload["cause"]["type"] = cause;
  payload["createdAt"] = 0;
  payload["deviceId"] = deviceId;
  payload["replyToken"] = MessageID().getID();
  payload["type"] = "event";
  payload.createNestedObject("value");
  return eventMessage;
}

#ifndef NOIOTLINK_INSTANCE

IotLinkClass IotLink;
#endif

#endif
