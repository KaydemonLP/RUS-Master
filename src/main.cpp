#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <AzIoTSasToken.h>
#include <SerialLogger.h>
#include <az_core.h>
#include <azure_ca.h>
#include <ctime>
#include "WiFiClientSecure.h"

#include "DHTesp.h"
#include "PubSubClient.h"
#include "ArduinoJson.h"

#include "Electroniccats_PN7150.h"

#include "nfc.h"

#include "iothub.h"

#define START_SCAN_BUTTON 12

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

#define CAM_SDA0_Pin 13
#define CAM_SCL0_Pin 14
#define NFC_SDA0_Pin 15
#define NFC_SCL0_Pin 19
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

void setup() 
{

  if(!Wire.begin(CAM_SDA0_Pin, CAM_SCL0_Pin, I2C_Freq)) //starting I2C Wire
  {
    Serial.println("I2C Wire Error. Going idle.");
    while(true)
      delay(1);
  }

  if(!Wire1.begin(NFC_SDA0_Pin, NFC_SCL0_Pin, I2C_Freq)) //starting I2C Wire
  {
    Serial.println("I2C Wire1 Error. Going idle.");
    while(true)
      delay(1);
  }

  nfid::setup();

  Serial.println("Master engaged.");

  setupWiFi();
  initializeTime();

  if( initIoTHub() )
  {
    connectMQTT();
    mqttReconnect();
  }

  sendTestMessageToIoTHub();

  pinMode( START_SCAN_BUTTON, INPUT_PULLUP );
  attachInterrupt(START_SCAN_BUTTON, scanButtonInterrupt, RISING);
}

void informSlave(int requestNumber, byte cmd);

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

  if (!mqttClient.connected()) mqttReconnect();
  if (sasToken.IsExpired()) {
    connectMQTT();
  }

  mqttClient.loop();

  checkTelemetry(flPercentage);

   nfid::loop();
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