#include "api.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "MD5Builder.h"

static String currentSalt, currentToken;

static String md5hash(const String& input) {
    MD5Builder b; b.begin(); b.add(input); b.calculate(); return b.toString();
}

static void generateAuth() {
    currentSalt = "";
    for (int i = 0; i < 12; i++) currentSalt += (char)('a' + (esp_random() % 26));
    currentToken = md5hash(String(NAVIDROME_PASS) + currentSalt);
}

String buildApiUrl(const String& ep) {
    generateAuth();
    return String(NAVIDROME_URL) + "/rest/" + ep + "?u=" + NAVIDROME_USER +
        "&t=" + currentToken + "&s=" + currentSalt + "&v=" + SUBSONIC_API_VERSION +
        "&c=" + SUBSONIC_CLIENT + "&f=json";
}

String buildStreamUrl(const String& id) {
    generateAuth();
    return String(NAVIDROME_URL) + "/rest/stream.view?id=" + id +
        "&format=mp3&maxBitRate=192&u=" + NAVIDROME_USER +
        "&t=" + currentToken + "&s=" + currentSalt + "&v=" + SUBSONIC_API_VERSION +
        "&c=" + SUBSONIC_CLIENT;
}

String apiCall(const String& url) {
    WiFiClientSecure c; c.setInsecure();
    HTTPClient http;
    if (!http.begin(c, url)) return "";
    int code = http.GET();
    String p = (code == 200) ? http.getString() : "";
    if (code != 200) Serial.printf("[api] HTTP %d\n", code);
    http.end();
    return p;
}

bool testConnection() {
    String r = apiCall(buildApiUrl("ping.view"));
    if (r.length() == 0) return false;
    JsonDocument doc; deserializeJson(doc, r);
    const char* s = doc["subsonic-response"]["status"];
    bool ok = s && String(s) == "ok";
    Serial.printf("[navidrome] Ping: %s\n", ok ? "OK" : "FAILED");
    return ok;
}

int apiGetStarredAlbums(MenuItem* out, int maxCount) {
    String r = apiCall(buildApiUrl("getStarred2.view"));
    if (r.length() == 0) return 0;
    JsonDocument doc;
    if (deserializeJson(doc, r)) return 0;
    JsonArray albums = doc["subsonic-response"]["starred2"]["album"];
    if (albums.isNull()) return 0;
    int count = 0;
    for (JsonObject a : albums) {
        if (count >= maxCount) break;
        strlcpy(out[count].id, a["id"] | "", sizeof(out[0].id));
        snprintf(out[count].name, sizeof(out[0].name), "%s - %s",
                 (const char*)(a["artist"] | ""), (const char*)(a["name"] | ""));
        count++;
    }
    Serial.printf("[api] starred albums: %d\n", count);
    return count;
}

int apiGetArtists(MenuItem* out, int maxCount) {
    String r = apiCall(buildApiUrl("getArtists.view"));
    if (r.length() == 0) return 0;
    JsonDocument doc;
    if (deserializeJson(doc, r)) return 0;
    JsonArray indexes = doc["subsonic-response"]["artists"]["index"];
    if (indexes.isNull()) return 0;
    int count = 0;
    for (JsonObject idx : indexes) {
        JsonArray artists = idx["artist"];
        if (artists.isNull()) continue;
        for (JsonObject a : artists) {
            if (count >= maxCount) break;
            strlcpy(out[count].id, a["id"] | "", sizeof(out[0].id));
            strlcpy(out[count].name, a["name"] | "", sizeof(out[0].name));
            count++;
        }
    }
    Serial.printf("[api] artists: %d\n", count);
    return count;
}

int apiGetArtistAlbums(const char* artistId, MenuItem* out, int maxCount) {
    String r = apiCall(buildApiUrl("getArtist.view") + "&id=" + artistId);
    if (r.length() == 0) return 0;
    JsonDocument doc;
    if (deserializeJson(doc, r)) return 0;
    JsonArray albums = doc["subsonic-response"]["artist"]["album"];
    if (albums.isNull()) return 0;
    int count = 0;
    for (JsonObject a : albums) {
        if (count >= maxCount) break;
        strlcpy(out[count].id, a["id"] | "", sizeof(out[0].id));
        int year = a["year"] | 0;
        if (year > 0)
            snprintf(out[count].name, sizeof(out[0].name), "%s (%d)",
                     (const char*)(a["name"] | ""), year);
        else
            strlcpy(out[count].name, a["name"] | "", sizeof(out[0].name));
        count++;
    }
    Serial.printf("[api] artist albums: %d\n", count);
    return count;
}

int apiGetAlbumSongs(const char* albumId, MenuItem* out, int maxCount) {
    String r = apiCall(buildApiUrl("getAlbum.view") + "&id=" + albumId);
    if (r.length() == 0) return 0;
    JsonDocument doc;
    if (deserializeJson(doc, r)) return 0;
    JsonArray songs = doc["subsonic-response"]["album"]["song"];
    if (songs.isNull()) return 0;
    int count = 0;
    for (JsonObject s : songs) {
        if (count >= maxCount) break;
        strlcpy(out[count].id, s["id"] | "", sizeof(out[0].id));
        int track = s["track"] | 0;
        if (track > 0)
            snprintf(out[count].name, sizeof(out[0].name), "%d. %s",
                     track, (const char*)(s["title"] | ""));
        else
            strlcpy(out[count].name, s["title"] | "", sizeof(out[0].name));
        count++;
    }
    Serial.printf("[api] album songs: %d\n", count);
    return count;
}

int apiGetPlaylists(MenuItem* out, int maxCount) {
    String r = apiCall(buildApiUrl("getPlaylists.view"));
    if (r.length() == 0) return 0;
    JsonDocument doc;
    if (deserializeJson(doc, r)) return 0;
    JsonArray playlists = doc["subsonic-response"]["playlists"]["playlist"];
    if (playlists.isNull()) return 0;
    int count = 0;
    for (JsonObject p : playlists) {
        if (count >= maxCount) break;
        strlcpy(out[count].id, p["id"] | "", sizeof(out[0].id));
        snprintf(out[count].name, sizeof(out[0].name), "%s (%d)",
                 (const char*)(p["name"] | ""), (int)(p["songCount"] | 0));
        count++;
    }
    Serial.printf("[api] playlists: %d\n", count);
    return count;
}

int apiGetPlaylistSongs(const char* playlistId, MenuItem* out, int maxCount) {
    String r = apiCall(buildApiUrl("getPlaylist.view") + "&id=" + playlistId);
    if (r.length() == 0) return 0;
    JsonDocument doc;
    if (deserializeJson(doc, r)) return 0;
    JsonArray entries = doc["subsonic-response"]["playlist"]["entry"];
    if (entries.isNull()) return 0;
    int count = 0;
    for (JsonObject e : entries) {
        if (count >= maxCount) break;
        strlcpy(out[count].id, e["id"] | "", sizeof(out[0].id));
        snprintf(out[count].name, sizeof(out[0].name), "%s - %s",
                 (const char*)(e["artist"] | ""), (const char*)(e["title"] | ""));
        count++;
    }
    Serial.printf("[api] playlist songs: %d\n", count);
    return count;
}
