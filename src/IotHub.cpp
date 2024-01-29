#include <Arduino.h>
#include <iothub.h>

#include <azure_ca.h>

#include "IotSettings.h"

// MQTT is a publish-subscribe based, therefore a callback function is called whenever something is published on a topic that device is subscribed to
void callback(char *topic, byte *payload, unsigned int length)
{
    // It's also a binary-safe protocol, therefore instead of transfering text,
    // bytes are transfered and they aren't null terminated - so we need ot add \0 to terminate the string 
    payload[length] = '\0'; 
     // After it's been terminated, it can be converted to String
    String message = String((char *)payload);

    Serial.printf("Callback: %s: %s\n", topic, message.c_str());
}

CIoTHub::CIoTHub()
{
    // Authentication token for our specific device
    sasToken = new AzIoTSasToken(
	&client, az_span_create_from_str(const_cast<char*>(deviceKey)),
	AZ_SPAN_FROM_BUFFER(sasSignatureBuffer),
	AZ_SPAN_FROM_BUFFER(
		mqttPasswordBuffer));

    mqttClient = new PubSubClient(wifiClient);
}

CIoTHub::~CIoTHub()
{
    if( sasToken )
        delete sasToken;

    if( mqttClient )
        delete mqttClient;
}

bool CIoTHub::initIoTHub()
{
    // We are using TLS to secure the connection, therefore we need to supply a certificate (in the SDK)
    wifiClient.setCACert((const char*)ca_pem); 

    // Get a default instance of IoT Hub client options
    az_iot_hub_client_options options = az_iot_hub_client_options_default(); 
    // Create an instnace of IoT Hub client for our IoT Hub's host and the current device
    if (az_result_failed(az_iot_hub_client_init( 
            &client,
            az_span_create((unsigned char *)iotHubHost, strlen(iotHubHost)),
            az_span_create((unsigned char *)deviceId, strlen(deviceId)),
            &options)))
    {
        Serial.println("ERROR: Failed initializing Azure IoT Hub client");
        return false;
    }

    size_t client_id_length;
    if (az_result_failed(az_iot_hub_client_get_client_id(
            &client, mqttClientId, sizeof(mqttClientId) - 1, &client_id_length))) // Get the actual client ID (not our internal ID) for the device
    {
        Serial.println("ERROR: Failed getting client id");
        return false;
    }

    size_t mqttUsernameSize;
    if (az_result_failed(az_iot_hub_client_get_user_name(
            &client, mqttUsername, sizeof(mqttUsername), &mqttUsernameSize))) // Get the MQTT username for our device
    {
        Serial.println("ERROR: Failed to get MQTT username ");
        return false;
    }

    Serial.println("Great success");
    Serial.printf("Client ID: %s\n", mqttClientId);
    Serial.printf("Username: %s\n", mqttUsername);

    connectMQTT();
    mqttReconnect();

    sendTestMessageToIoTHub();

    return true;
}

bool CIoTHub::connectMQTT()
{
    // The default size is defined in MQTT_MAX_PACKET_SIZE to be 256 bytes, which is too small for Azure MQTT messages,
    //therefore needs to be increased or it will just crash without any info
    mqttClient->setBufferSize(1024); 

    // SAS tokens need to be generated in order to generate a password for the connection
    if (sasToken->Generate(tokenDuration) != 0) 
    {
        Serial.println("Error: Failed generating SAS token");
        return false;
    }
    else
        Serial.println("SAS token generated");

    mqttClient->setServer(iotHubHost, mqttPort);
    mqttClient->setCallback(callback);

    return true;
}

void CIoTHub::mqttReconnect() 
{
    while (!mqttClient->connected())
    {
        Serial.println("Attempting MQTT connection...");
        // Just in case that the SAS token has been regenerated since the last MQTT connection, get it again
        const char *mqttPassword = (const char *)az_span_ptr(sasToken->Get()); 
        Serial.println(mqttClientId);
        Serial.println(mqttUsername);
        Serial.println(mqttPassword);
        // Either connect or wait for 5 seconds (we can block here since without IoT Hub connection we can't do much)
        if (mqttClient->connect(mqttClientId, mqttUsername, mqttPassword)) 
        {
            Serial.println("MQTT connected");
            // If connected, (re)subscribe to the topic where we can receive messages sent from the IoT Hub 
            mqttClient->subscribe(mqttC2DTopic); 
        } 
        else 
        {
            Serial.println("Trying again in 5 seconds");
            delay(5000);
        }
    }
}

void CIoTHub::sendTelemetryData(String telemetryData)
{
    mqttClient->publish(publishTopic, telemetryData.c_str());
}


void CIoTHub::sendTestMessageToIoTHub()
{
    Serial.println("Sending...");
     // The receive topic isn't hardcoded and depends on chosen properties, therefore we need to use az_iot_hub_client_telemetry_get_publish_topic()
    az_result res = az_iot_hub_client_telemetry_get_publish_topic(&client, NULL, publishTopic, 200, NULL );
    Serial.println(publishTopic);

    // Use https://github.com/Azure/azure-iot-explorer/releases to read the telemetry
    mqttClient->publish(publishTopic, deviceId); 
}

void CIoTHub::EnsureMQTTConnectivity()
{
    if( !mqttClient->connected() )
        mqttReconnect();

    if( sasToken->IsExpired() )
        connectMQTT();
}

void CIoTHub::loop()
{
    EnsureMQTTConnectivity();

    mqttClient->loop();
}

char *CIoTHub::GetDeviceID()
{
    return deviceId;
}

short timeoutCounter = 0;

void setupWiFi()
{
	Serial.println("Connecting to WiFi");

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass);

	while (WiFi.status() != WL_CONNECTED)
    { // Wait until we connect...
		Serial.print(".");
		delay(500);

		timeoutCounter++;
		if (timeoutCounter >= 20) ESP.restart(); // Or restart if we waited for too long, not much else can you do
	}

	Serial.println("WiFi connected");
}

void initializeTime()
{
    // MANDATORY or SAS tokens won't generate
  Serial.println("Setting time using SNTP");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(NULL);
  std::tm tm{};
  tm.tm_year = 2023; // Define a date on 1.1.2023. and wait until the current time has the same year (by default it's 1.1.1970.)

  while (now < std::mktime(&tm)) // Since we are using an Internet clock, it may take a moment for clocks to sychronize
  {
    delay(500);
    Serial.print(".");
    now = time(NULL);
  }
}