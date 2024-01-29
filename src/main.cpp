#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "WiFiClientSecure.h"

#include "PubSubClient.h"
#include "ArduinoJson.h"

#include "Electroniccats_PN7150.h"

#include "nfc.h"

#include "iothub.h"

#define START_SCAN_BUTTON 21

bool g_bScanButtonPressed = false;

volatile bool buttonPressed = false;
unsigned long lastInterruptTime = 0;

void IRAM_ATTR scanButtonInterrupt()
{
  if (millis() - lastInterruptTime > 200)
  {
    g_bScanButtonPressed = true;
    lastInterruptTime = millis();
  }
}

#define CAM_SDA0_Pin 19
#define CAM_SCL0_Pin 18

#define NFC_SDA_Pin 15
#define NFC_SCL_Pin 14
#define NFC_IRQ_Pin 13
#define NFC_VEN_Pin 12

#define I2C_Freq 100000
#define I2C_DEV_ADDR 0x10

#define REQUEST_DELAY 0
#define DISPLAY_DELAY 1000

typedef struct
{
  int requestCount;
  byte command;
} __attribute__((packed)) request;

template <class T>
void requestNum(int requestNumber, byte registerNumber, T *response); //string
void requestString(int requestNumber, byte registerNumber, char *response, int length); //string
void I2cTransmit(int requestNumber, byte registerNumber);
template <class T>
void I2cRead(T *response, int length); //string

CNFCHandler g_NFC;
CIoTHub g_IoTHub;

void setup() 
{
  if(!Wire.begin(CAM_SDA0_Pin, CAM_SCL0_Pin, I2C_Freq)) //starting I2C Wire
  {
    Serial.println("I2C Wire Error. Going idle.");
    while(true)
      delay(1);
  }

  g_NFC.setup( &Wire1, NFC_SDA_Pin, NFC_SCL_Pin, NFC_IRQ_Pin, NFC_VEN_Pin);

  Serial.println("Master engaged.");

  setupWiFi();
  initializeTime();

  g_IoTHub.initIoTHub();

  pinMode( START_SCAN_BUTTON, INPUT_PULLUP );
  attachInterrupt(START_SCAN_BUTTON, scanButtonInterrupt, RISING);
}

void informSlave(int requestNumber, byte cmd);

 // Get the data and pack it in a JSON message
String getTelemetryData(char *deviceId, float percentage)
{
  StaticJsonDocument<128> doc; // Create a JSON document we'll reuse to serialize our data into JSON
  String output = "";

	doc["UserID"] = 1;
  doc["Rating"] = percentage;

	doc["DeviceID"] = (String)deviceId;

	serializeJson(doc, output);

	Serial.println(output.c_str());
  return output;
}

long lastTime, currentTime = 0;
int interval = 5000;
void checkTelemetry(float percentage)
{ 
  // Do not block using delay(), instead check if enough time has passed between two calls using millis() 
  currentTime = millis();

  if (currentTime - lastTime >= interval && percentage != 0)
  { 
    // Subtract the current elapsed time (since we started the device) from the last time we sent the telemetry, if the result is greater than the interval, send the data again
    Serial.println("Sending telemetry...");

    String data = getTelemetryData(g_IoTHub.GetDeviceID(), percentage);

    g_IoTHub.sendTelemetryData(data);

    lastTime = currentTime;
  }
}

void loop() 
{
  static int requestCount = 0;
  int responseReqCount;
  char r5[5];
  float flPercentage;

  if( g_bScanButtonPressed )
  {
    informSlave(requestCount, 3);
    requestCount++;
    g_bScanButtonPressed = false;
    Serial.println("Pressed!");
  }

  requestNum<float>(requestCount, 4, &flPercentage);
  //Serial.printf("Request %d, register 4 (len): %f\n",requestCount,flPercentage);
  requestCount++;

  g_IoTHub.loop();

  checkTelemetry(flPercentage);

  g_NFC.loop();
}

void informSlave(int requestNumber, byte cmd)
{
  I2cTransmit(requestNumber, cmd); //send register command
  delay(REQUEST_DELAY);
  if(Wire.requestFrom(I2C_DEV_ADDR,1) == 1)
    Wire.read(); //read old dummy
}

void requestString(int requestNumber, byte cmd, char *response, int length)
{
  informSlave(requestNumber, cmd);
  //again - after ESP32 buffer prefill
  delay(REQUEST_DELAY);
  I2cRead<char>(response, length); //read register
}

template < class T >
void requestNum(int requestNumber, byte cmd, T *response)
{
  informSlave(requestNumber, cmd);
  //again - after ESP32 buffer prefill
  delay(REQUEST_DELAY);
  I2cRead<T>(response, sizeof(T)); //read register
}

void I2cTransmit(int requestNumber, byte cmd)
{
  request command;
  command.command = cmd;
  command.requestCount = requestNumber;
  Wire.beginTransmission(I2C_DEV_ADDR); //start condition
  Wire.write((uint8_t*)&command, sizeof(request)); //write to slave register
  Wire.endTransmission(); // end condition
}

template <class T>
void I2cRead(T *response, int length)
{
  if(Wire.requestFrom(I2C_DEV_ADDR,length) == length) //read from device register
    Wire.readBytes((char *)response, length); //read register
}