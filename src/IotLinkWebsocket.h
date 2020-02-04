#ifndef _IOTLINK_WEBSOCKET_H__
#define _IOTLINK_WEBSOCKET_H__

#if defined ESP8266
  #include <ESP8266WiFi.h>
#endif
#if defined ESP32
  #include <WiFi.h>
#endif

#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "IotLinkDebug.h"
#include "IotLinkConfig.h"
#include "IotLinkInterface.h"
#include "IotLinkSignature.h"

class websocketListener
{
  public:
    typedef std::function<void(void)> wsConnectedCallback;
    typedef std::function<void(void)> wsDisconnectedCallback;

    websocketListener();
    ~websocketListener();

    void begin(String server, String socketAuthToken, String signingKey, std::vector<IotLinkDeviceInterface*> devices, String deviceIds);
    void handle();
    void stop();
    bool isConnected() { return _isConnected; }
    void setRestoreDeviceStates(bool flag) { this->restoreDeviceStates = flag; };

    void sendMessage(String &message);
    void extractTimestamp(JsonDocument &message);
    DynamicJsonDocument prepareResponse(JsonDocument& requestMessage);
    void handleResponse(DynamicJsonDocument& responseMessage);
    void handleRequest(DynamicJsonDocument& requestMessage);

    void onConnected(wsConnectedCallback callback) { _wsConnectedCb = callback; }
    void onDisconnected(wsDisconnectedCallback callback) { _wsDisconnectedCb = callback; }

    void disconnect() { webSocket.disconnect(); }
  private:
    bool _begin = false;
    bool _isConnected = false;
    bool restoreDeviceStates = false;

    WebSocketsClient webSocket;

    wsConnectedCallback _wsConnectedCb;
    wsDisconnectedCallback _wsDisconnectedCb;

    void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
    void setExtraHeaders();
    std::vector<IotLinkDeviceInterface*> devices;
    String deviceIds;
    String socketAuthToken;
    unsigned long getTimestamp() { return baseTimestamp + (millis()/1000); }
    unsigned long baseTimestamp = 0;
    String responseMessageStr = "";
    String signingKey;
};

void websocketListener::setExtraHeaders() {
  String headers  = "appkey:" + socketAuthToken + "\r\n";
         headers += "deviceids:" + deviceIds + "\r\n";
         headers += "restoredevicestates:" + String(restoreDeviceStates?"true":"false") + "\r\n";
  #ifdef ESP8266
         headers += "platform:ESP8266\r\n";
  #endif
  #ifdef ESP32
         headers += "platform:ESP32\r\n";
  #endif
         headers += "version:" + String(IOTLINK_VERSION);
  DEBUG_IOTLINK("[IotLink:Websocket]: headers: \r\n%s\r\n", headers.c_str());
  webSocket.setExtraHeaders(headers.c_str());
}

websocketListener::websocketListener() : _isConnected(false) {}

websocketListener::~websocketListener() {
  stop();
}

void websocketListener::begin(String server, String socketAuthToken, String signingKey, std::vector<IotLinkDeviceInterface*> devices, String deviceIds) {
  if (_begin) return;
  _begin = true;
  this->socketAuthToken = socketAuthToken;
  this->deviceIds = deviceIds;
  this->signingKey = signingKey;
  this->devices = devices;

  DEBUG_IOTLINK("[IotLink:Websocket]: Connecting to WebSocket Server (%s)\r\n", server.c_str());

  if (_isConnected) {
    stop();
  }
  setExtraHeaders();
  webSocket.onEvent([&](WStype_t type, uint8_t * payload, size_t length) { webSocketEvent(type, payload, length); });
  webSocket.enableHeartbeat(WEBSOCKET_PING_INTERVAL, WEBSOCKET_PING_TIMEOUT, WEBSOCKET_RETRY_COUNT);
  webSocket.begin(server, IOTLINK_SERVER_PORT, "/"); // server address, port and URL
}

void websocketListener::handle() {
  webSocket.loop();
}

void websocketListener::stop() {
  if (_isConnected) {
    webSocket.disconnect();
  }
  _begin = false;
}

void websocketListener::sendMessage(String &message) {
  //Serial.println(message);
  webSocket.sendTXT(message);
}

void websocketListener::extractTimestamp(JsonDocument &message) {
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

DynamicJsonDocument websocketListener::prepareResponse(JsonDocument& requestMessage) {
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

void websocketListener::handleResponse(DynamicJsonDocument& responseMessage) {
    DEBUG_IOTLINK("[IotLink.handleResponse()]:\r\n");

#ifndef NODEBUG_IOTLINK
    serializeJsonPretty(responseMessage, DEBUG_ESP_PORT);
    Serial.println();
#endif
}

void websocketListener::handleRequest(DynamicJsonDocument& requestMessage) {
    DEBUG_IOTLINK("[IotLink.handleRequest()]: handling request\r\n");
#ifndef NODEBUG_IOTLINK
    serializeJsonPretty(requestMessage, DEBUG_ESP_PORT);
#endif

    DynamicJsonDocument responseMessage = prepareResponse(requestMessage);

    // handle devices
    bool success = false;
    const char* deviceId = requestMessage["payload"]["deviceId"];
    const char* action = requestMessage["payload"]["action"];
    JsonObject request_value = requestMessage["payload"]["value"];
    JsonObject response_value = responseMessage["payload"]["value"];

    for (auto& device : devices) {

        if (strcmp(deviceId, device->getDeviceId()) == 0 && success == false) {
            success = device->handleRequest(deviceId, action, request_value, response_value);
            responseMessage["payload"]["success"] = success;
            if (!success) {
                if (responseMessageStr.length() > 0){
                    responseMessage["payload"]["message"] = responseMessageStr;
                    responseMessageStr = "";
                } else {
                    responseMessage["payload"]["message"] = "Device returned an error while processing the request!";
                }
            }
        }
    }

    String responseString;
    serializeJson(responseMessage, responseString);

    if(isConnected()) {
        //sendMessage(responseString);
    }
}

void websocketListener::webSocketEvent(WStype_t type, uint8_t * payload, size_t length)
{
  switch (type) {
    case WStype_DISCONNECTED:
      if (_isConnected) {
        DEBUG_IOTLINK("[IotLink:Websocket]: disconnected\r\n");
        if (_wsDisconnectedCb) _wsDisconnectedCb();
        _isConnected = false;
      }
      break;
    case WStype_CONNECTED:
      _isConnected = true;
      DEBUG_IOTLINK("[IotLink:Websocket]: connected\r\n");
      if (_wsConnectedCb) _wsConnectedCb();
      if (restoreDeviceStates){
        restoreDeviceStates=false; 
        setExtraHeaders();
      }
      break;
    case WStype_TEXT: {

      delay(200);
      char* request = (char*)payload;
//      DEBUG_IOTLINK("[IotLink:Websocket]: receiving data\r\n");
	  Serial.println((char*)payload);

	  DynamicJsonDocument jsonMessage(1024);
	  deserializeJson(jsonMessage, request);

	  bool sigMatch = false;

	  if (strncmp(request, "{\"timestamp\":", 13) == 0 && strlen(request) <= 26) {
	      sigMatch=true;
	  } else {
	      sigMatch = verifyMessage(signingKey, jsonMessage);
	  }

	  String messageType = jsonMessage["payload"]["type"];

	  if (sigMatch) {
	      extractTimestamp(jsonMessage);
          Serial.println("signature match");
	      if (messageType == "response") handleResponse(jsonMessage);
	      if (messageType == "request") handleRequest(jsonMessage);
	  }else{
          Serial.println("signature not match");
	  }

      break;
    }
    default: break;
  }
}

#endif
