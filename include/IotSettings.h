#ifndef IOTSETTINGS_H
#define IOTSETTINGS_H

/* Wifi, connect to a local router*/
// WIFI Name
const char *ssid = "The nasty Kay demon";
// WIFI PASSWORD
const char *pass = "trollfacegymnastics";

/* Azure auth data */
// Azure Primary key for device
// IOT hub->Devices->Bilo koji device->Primary key
char *deviceKey = "Bski4L+vR0o3Bh5AzFa+pY6kwFnEPJZq0AIoTOOKDtg=";	
// IOT hub->Devices->Bilo koji device, ime
char *deviceId = "LabDevice1";  // Device ID as specified in the list of devices on IoT Hub
//[Azure IoT host name].azure-devices.net
// IOT hub->Overview ( prva stranica )->Hostname
char *iotHubHost = "HUB-RUS-KAY-21.azure-devices.net";
#endif