#pragma once
#include <Arduino.h>

#define MAX_WIFI_NETWORKS 5
#define MAX_RADIO_STATIONS 20

struct WifiNetwork {
    char ssid[33];
    char password[65];
};

struct NavidromeConfig {
    char url[128];
    char username[33];
    char password[33];
};

struct RadioStation {
    char name[52];
    char url[128];
};

void             settingsInit();
bool             settingsHasConfig();

int              settingsGetWifiCount();
WifiNetwork      settingsGetWifi(int index);
void             settingsAddWifi(const char* ssid, const char* password);
void             settingsRemoveWifi(int index);

NavidromeConfig  settingsGetNavidrome();
void             settingsSetNavidrome(const char* url, const char* user, const char* pass);

int              settingsGetRadioCount();
RadioStation     settingsGetRadio(int index);
void             settingsAddRadio(const char* name, const char* url);
void             settingsRemoveRadio(int index);

void             settingsResetAll();
