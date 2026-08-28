#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "soc/rtc_cntl_reg.h"
#include "config.h"
#include "settings.h"
#include "portal.h"
#include "api.h"
#include "display.h"
#include "player.h"
#include "ui.h"
#include "webconfig.h"

static char _ndUrl[128] = "";
static char _ndUser[33] = "";
static char _ndPass[33] = "";

char* NAVIDROME_URL  = _ndUrl;
char* NAVIDROME_USER = _ndUser;
char* NAVIDROME_PASS = _ndPass;
const char* SUBSONIC_API_VERSION = "1.16.1";
const char* SUBSONIC_CLIENT = "ESP32Player";

static void loadNavidromeConfig() {
    NavidromeConfig nd = settingsGetNavidrome();
    strlcpy(_ndUrl, nd.url, sizeof(_ndUrl));
    strlcpy(_ndUser, nd.username, sizeof(_ndUser));
    strlcpy(_ndPass, nd.password, sizeof(_ndPass));
}

static bool connectWiFiMulti() {
    int count = settingsGetWifiCount();
    if (count == 0) return false;

    WiFiMulti wifiMulti;
    for (int i = 0; i < count; i++) {
        WifiNetwork n = settingsGetWifi(i);
        wifiMulti.addAP(n.ssid, n.password);
        Serial.printf("[wifi] Added: %s\n", n.ssid);
    }

    Serial.print("[wifi] Connecting");
    unsigned long t0 = millis();
    while (wifiMulti.run() != WL_CONNECTED && millis() - t0 < 15000) {
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[wifi] Connected to %s, IP: %s\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("\n[wifi] Failed");
    return false;
}

void rebootToBootloader() {
    Serial.println("Rebooting..."); Serial.flush(); delay(1000);
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_restart();
}

void handleSerialCommand() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n'); cmd.trim();
    if (cmd == "flash") rebootToBootloader();
    else if (cmd == "ping") testConnection();
    else if (cmd == "stop") { playerStop(); lcdShowMessage("Stopped", NULL); }
    else if (cmd == "next") playerPlayNext();
    else if (cmd == "prev") playerPlayPrev();
    else if (cmd == "pause") playerTogglePause();
    else if (cmd == "v+") { playerSetVolume(playerGetVolume()+1); Serial.printf("Vol: %d\n", playerGetVolume()); }
    else if (cmd == "v-") { playerSetVolume(playerGetVolume()-1); Serial.printf("Vol: %d\n", playerGetVolume()); }
    else if (cmd == "setup") { portalStart(); lcdShowMessage("WiFi Setup", "ESP32-Music"); }
    else if (cmd == "reset") { settingsResetAll(); ESP.restart(); }
    else if (cmd == "status") { Serial.printf("Vol:%d WiFi:%s Playing:%d Heap:%d PSRAM:%d Queue:%d/%d\n",
        playerGetVolume(), WiFi.isConnected()?"ok":"no", playerIsPlaying(),
        ESP.getFreeHeap(), ESP.getFreePsram(), playerCurrentIndex(), playerQueueSize()); }
    else if (cmd.length()>0) Serial.println("next|prev|pause|stop|v+|v-|ping|setup|reset|status|flash");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== Navidrome Streamer v7 ===");

    settingsInit();
    displayInit();
    playerInit();

    if (!settingsHasConfig()) {
        lcdShowMessage("Setup", "Connect to");
        lcdDrawTextCentered(225, "ESP32-Music", COL_WHITE, COL_BG, 2);
        lcdDrawTextCentered(255, "WiFi network", COL_GRAY, COL_BG, 2);
        portalStart();
        return;
    }

    lcdShowMessage("Connecting...", NULL);
    loadNavidromeConfig();

    if (connectWiFiMulti() && testConnection()) {
        uiInit();
        webconfigStart();
    } else {
        lcdShowMessage("No WiFi", "Starting setup");
        portalStart();
    }
    Serial.println("Type 'help' for commands.");
}

void loop() {
    if (portalIsActive()) {
        portalLoop();
        handleSerialCommand();
        return;
    }
    playerLoop();
    uiLoop();
    webconfigLoop();
    handleSerialCommand();
}
