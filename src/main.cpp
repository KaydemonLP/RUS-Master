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
/*
TwoWire Wire2(2);
*/
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

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 g_Screen(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1); //OLED

void setup() 
{
  Serial.begin(115200);

  if(!Wire.begin(CAM_SDA0_Pin, CAM_SCL0_Pin, I2C_Freq)) //starting I2C Wire
  {
    Serial.println("I2C Wire Error. Going idle.");
  }

  g_NFC.setup( &Wire1, NFC_SDA_Pin, NFC_SCL_Pin, NFC_IRQ_Pin, NFC_VEN_Pin);

  if(!g_Screen.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("Screen not present.");
    while(1)
      delay(1);
  }
  g_Screen.clearDisplay();
  g_Screen.setTextColor(WHITE);
  g_Screen.setTextSize(0);
  g_Screen.display();

  Serial.println("Master engaged.");

  setupWiFi();
  initializeTime();

  g_IoTHub.initIoTHub();

  pinMode( START_SCAN_BUTTON, INPUT_PULLUP );
  attachInterrupt(START_SCAN_BUTTON, scanButtonInterrupt, RISING);
}

void informSlave(int requestNumber, byte cmd);

 // Get the data and pack it in a JSON message
String createTelemetryData(char *deviceId, menu menu, float rating)
{
  StaticJsonDocument<128> doc; // Create a JSON document we'll reuse to serialize our data into JSON
  String output = "";

	doc["UserID"] = menu.iUserID;
  doc["Rating"] = rating;

	doc["DeviceID"] = (String)deviceId;

  auto menuArray = doc.createNestedArray("Menu");
  for( int i = 0; i < menu.iMenuLen; i++ )
    menuArray.add(menu.iaMenu[i]);

	serializeJson(doc, output);

	Serial.println(output.c_str());
  return output;
}

float g_Percentage = -1;

enum {
  CMD_IS_SCANNING = 2,
  CMD_START_SCAN,
  CMD_GET_RESULT
};

enum {
  STATE_IDLE = 0,
  STATE_CONFIRM_SCAN,
  STATE_SCANNING,
  STATE_CONFIRM_RESULT
};

int state = STATE_IDLE;

menu currMenu;

const char *rating [] = 
{
  ":))",
  ":)",
  ":|",
  ":(",
  ":(("
};

void loop() 
{
  g_Screen.clearDisplay();
  g_Screen.setCursor(0, 1);

  static int requestCount = 0;
  int responseReqCount;
  float flPercentage = -1;
  bool bIsScanning = false;

  requestNum<float>(requestCount, CMD_GET_RESULT, &flPercentage);
  //Serial.printf("Request %d, command 4 (len): %f\n",requestCount,flPercentage);
  requestCount++;

  requestNum<bool>(requestCount, CMD_IS_SCANNING, &bIsScanning);
  requestCount++;
  //Serial.printf("Request %d, command 2 (len): %d\n",requestCount, bIsScanning);

  g_IoTHub.loop();

  switch( state )
  {
    case STATE_IDLE:
    {
      // Debounce scan unless we're in confirm scan
      g_bScanButtonPressed = false;
      g_Screen.println("Prislonite karticu.");

      bool bCardExists = g_NFC.CheckCard();
      if( bCardExists )
      {
        int iRetCode = g_NFC.ReadMenu(currMenu);
        if( iRetCode != 0 )
        {
          g_Screen.clearDisplay();
          g_Screen.setCursor(0, 1);
          switch( iRetCode )
          {
            case 1:
              g_Screen.println("Pogreska kod detekcije kartice!\nOdmaknite kartu.");
            break;
            default:
            case 2:
              g_Screen.println("Pogreska kod citanja kartice!\nOdmaknite kartu.");
            break;
            case 3:
              g_Screen.println("Neispravna vrsta kartice!\nOdmaknite kartu.");
            break;
          }
          
          g_Screen.display();
          g_NFC.WaitForRemoval();

          currMenu.Clear();
        }
        else
        {
          state = STATE_CONFIRM_SCAN;

          g_Screen.clearDisplay();
          g_Screen.setCursor(0, 1);
          g_Screen.println("Uspjeh!\nOdmaknite kartu.");
          g_Screen.display();
          g_NFC.WaitForRemoval();
        }
      }
      g_NFC.Reset();
    }
    break;
    case STATE_CONFIRM_SCAN:
    {
      g_Screen.printf("Pozdrav korisnik: %d.\n", currMenu.iUserID);
      g_Screen.println("Pritisnite gumb za pocetak skena.");
      g_Screen.println("Prislonite ponovno karticu za prekid.");

      bool bCardExists = g_NFC.CheckCard();
      if( bCardExists )
      {
        state = STATE_IDLE;

        g_Screen.clearDisplay();
        g_Screen.setCursor(0, 1);
        g_Screen.println("Prekid uspjesan!\nOdmaknite kartu.");
        g_Screen.display();
        g_NFC.WaitForRemoval();
        g_NFC.Reset();
        break;
      }
      g_NFC.Reset();

      if( g_bScanButtonPressed )
      {
        informSlave(requestCount, CMD_START_SCAN);
        requestCount++;
        g_bScanButtonPressed = false;
        Serial.println("Pressed!");
        state = STATE_SCANNING;
      }
    }
    break;
    case STATE_SCANNING:
    {
      g_Screen.println("Skeniranje...");

      if( !bIsScanning )
      {
        if( flPercentage != -1 )
        {
          g_Percentage = flPercentage;
          state = STATE_CONFIRM_RESULT;
          break;
        }
      }
    }
    break;
    case STATE_CONFIRM_RESULT:
    {
      int iRating = 0;

      if( g_Percentage >= 0.6f )
        iRating = 0;
      else if( g_Percentage >= 0.48 )
        iRating = 1;
      else if( g_Percentage >= 0.36 )
        iRating = 2;
      else if( g_Percentage >= 0.24 )
        iRating = 3;
      else
        iRating = 4;

      g_Screen.printf("Rating: %s\n", rating[iRating] );
      g_Screen.println("Prislonite karticu za potvrdu.");

      bool bCardExists = g_NFC.CheckCard();
      if( bCardExists )
      {
        state = STATE_IDLE;
        g_Screen.clearDisplay();
        g_Screen.setCursor(0, 1);
        g_Screen.println("Uspjeh!\nOdmaknite kartu za slanje podataka.");
        g_Screen.display();
        g_NFC.WaitForRemoval();
        g_NFC.Reset();
        Serial.println("Sending telemetry...");
        String data = createTelemetryData(g_IoTHub.GetDeviceID(), currMenu, g_Percentage);
        g_IoTHub.sendTelemetryData(data);

        currMenu.Clear();

        g_Screen.clearDisplay();
        g_Screen.setCursor(0, 1);
        g_Screen.println("Podatci uspjesno poslani!");
        g_Screen.display();
        delay(3000);
        break;
      }
    }
    break;
  }
  
  g_Screen.display();
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