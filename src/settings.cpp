#include "settings.h"
#include <Preferences.h>

static Preferences prefs;

void settingsInit() {
    prefs.begin("music", false);
    Serial.printf("[settings] WiFi networks: %d, Navidrome: %s\n",
                  settingsGetWifiCount(),
                  settingsHasConfig() ? "configured" : "not set");
}

bool settingsHasConfig() {
    return settingsGetWifiCount() > 0 && prefs.isKey("nd_url") && prefs.getString("nd_url").length() > 0;
}

int settingsGetWifiCount() {
    return prefs.getInt("wifi_count", 0);
}

WifiNetwork settingsGetWifi(int index) {
    WifiNetwork n = {};
    if (index < 0 || index >= settingsGetWifiCount()) return n;
    char keyS[12], keyP[12];
    snprintf(keyS, sizeof(keyS), "ws_%d", index);
    snprintf(keyP, sizeof(keyP), "wp_%d", index);
    strlcpy(n.ssid, prefs.getString(keyS, "").c_str(), sizeof(n.ssid));
    strlcpy(n.password, prefs.getString(keyP, "").c_str(), sizeof(n.password));
    return n;
}

void settingsAddWifi(const char* ssid, const char* password) {
    int count = settingsGetWifiCount();
    if (count >= MAX_WIFI_NETWORKS) return;

    for (int i = 0; i < count; i++) {
        WifiNetwork n = settingsGetWifi(i);
        if (strcmp(n.ssid, ssid) == 0) {
            char keyP[12];
            snprintf(keyP, sizeof(keyP), "wp_%d", i);
            prefs.putString(keyP, password);
            Serial.printf("[settings] Updated WiFi: %s\n", ssid);
            return;
        }
    }

    char keyS[12], keyP[12];
    snprintf(keyS, sizeof(keyS), "ws_%d", count);
    snprintf(keyP, sizeof(keyP), "wp_%d", count);
    prefs.putString(keyS, ssid);
    prefs.putString(keyP, password);
    prefs.putInt("wifi_count", count + 1);
    Serial.printf("[settings] Added WiFi: %s (%d total)\n", ssid, count + 1);
}

void settingsRemoveWifi(int index) {
    int count = settingsGetWifiCount();
    if (index < 0 || index >= count) return;

    for (int i = index; i < count - 1; i++) {
        WifiNetwork next = settingsGetWifi(i + 1);
        char keyS[12], keyP[12];
        snprintf(keyS, sizeof(keyS), "ws_%d", i);
        snprintf(keyP, sizeof(keyP), "wp_%d", i);
        prefs.putString(keyS, next.ssid);
        prefs.putString(keyP, next.password);
    }

    char keyS[12], keyP[12];
    snprintf(keyS, sizeof(keyS), "ws_%d", count - 1);
    snprintf(keyP, sizeof(keyP), "wp_%d", count - 1);
    prefs.remove(keyS);
    prefs.remove(keyP);
    prefs.putInt("wifi_count", count - 1);
    Serial.printf("[settings] Removed WiFi #%d (%d remaining)\n", index, count - 1);
}

NavidromeConfig settingsGetNavidrome() {
    NavidromeConfig c = {};
    strlcpy(c.url, prefs.getString("nd_url", "").c_str(), sizeof(c.url));
    strlcpy(c.username, prefs.getString("nd_user", "").c_str(), sizeof(c.username));
    strlcpy(c.password, prefs.getString("nd_pass", "").c_str(), sizeof(c.password));
    return c;
}

void settingsSetNavidrome(const char* url, const char* user, const char* pass) {
    prefs.putString("nd_url", url);
    prefs.putString("nd_user", user);
    prefs.putString("nd_pass", pass);
    Serial.printf("[settings] Navidrome: %s user=%s\n", url, user);
}

void settingsResetAll() {
    prefs.clear();
    Serial.println("[settings] All settings cleared");
}
