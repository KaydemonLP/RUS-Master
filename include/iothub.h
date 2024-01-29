#pragma once

#include <az_iot_hub_client.h>
#include <az_span.h>
#include <az_core.h>
#include "WiFiClientSecure.h"
#include "PubSubClient.h"
#include <ctime>

class AzIoTSasToken
{
public:
  AzIoTSasToken(
      az_iot_hub_client* client,
      az_span deviceKey,
      az_span signatureBuffer,
      az_span sasTokenBuffer);
  int Generate(unsigned int expiryTimeInMinutes);
  bool IsExpired();
  az_span Get();

private:
  az_iot_hub_client* client;
  az_span deviceKey;
  az_span signatureBuffer;
  az_span sasTokenBuffer;
  az_span sasToken;
  uint32_t expirationUnixTime;
};

#ifndef hehehoho

class CIoTHub
{
public:
  CIoTHub();
  ~CIoTHub();

  bool initIoTHub();

  bool connectMQTT();

  void mqttReconnect();

  void sendTelemetryData(String telemetryData);

  void sendTestMessageToIoTHub();
  
  void EnsureMQTTConnectivity();

  void loop();

  char *GetDeviceID();

private:
  int tokenDuration = 60;

  /* MQTT data for IoT Hub connection */
  int mqttPort = AZ_IOT_DEFAULT_MQTT_CONNECT_PORT;	// Secure MQTT port
  const char* mqttC2DTopic = AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC;	// Topic where we can receive cloud to device messages

  // These three are just buffers - actual clientID/username/password is generated
  // using the SDK functions in initIoTHub()
  char mqttClientId[128];
  char mqttUsername[128];
  char mqttPasswordBuffer[200];
  char publishTopic[200];

/* Auth token requirements */

  uint8_t sasSignatureBuffer[256];  // Make sure it's of correct size, it will just freeze otherwise :/

  az_iot_hub_client client;
  AzIoTSasToken *sasToken;
  /* WiFi things */
  WiFiClientSecure wifiClient;
  PubSubClient *mqttClient;
};

extern void setupWiFi();

// Use pool pool.ntp.org to get the current time
// Define a date on 1.1.2023. and wait until the current time has the same year (by default it's 1.1.1970.)
extern void initializeTime();
#endif