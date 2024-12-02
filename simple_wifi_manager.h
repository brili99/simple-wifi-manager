#ifndef _SIMPLE_WIFI_MANAGER_H_
#define _SIMPLE_WIFI_MANAGER_H_

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <CRC32.h>

#define FS_PATH_CONFIG_STA "/config_sta.bin"
#define FS_PATH_CONFIG_AP "/config_ap.bin"

struct Config_Wifi
{
    char ssid[50];
    char pass[50];
};

struct Storage_Config
{
    Config_Wifi config_wifi;
    uint32_t crc;
};

void wifi_manager_setup(char *default_ap_ssid, char *default_ap_pass, char *default_sta_ssid, char *default_sta_pass);

void onWifiConnect(const WiFiEventStationModeGotIP &event);
void onWifiDisconnect(const WiFiEventStationModeDisconnected &event);
void onSoftApConnected(const WiFiEventSoftAPModeStationConnected &event);

void wifi_manager_loop();

void handle_not_found();
void handle_root();
void handle_get_status();
void handle_update_wifi();

bool get_config(Storage_Config &config, const char *path);
bool save_config(Config_Wifi &config_wifi, const char *path);

uint32_t calculate_crc_storage_config(Storage_Config &storage_config);

extern ESP8266WebServer server;
extern Config_Wifi config_sta;
extern Config_Wifi config_ap;

#endif
