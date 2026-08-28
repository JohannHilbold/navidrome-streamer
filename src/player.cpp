#include "player.h"
#include "config.h"
#include "api.h"
#include "Audio.h"
#include <ArduinoJson.h>

static Audio audio;
static int currentVolume = 10;
static bool paused = false;
static volatile bool eofFlag = false;

#define MAX_QUEUE 200
static QueueEntry* queue = NULL;
static int queueSize = 0;
static int queueIdx = -1;

static bool radioMode = false;
static char radioName[52] = "";
static char radioUrl[128] = "";

void playerInit() {
    queue = (QueueEntry*)ps_malloc(MAX_QUEUE * sizeof(QueueEntry));
    if (!queue) queue = (QueueEntry*)malloc(50 * sizeof(QueueEntry));

    pinMode(I2S_SWITCH_IN, OUTPUT);
    digitalWrite(I2S_SWITCH_IN, HIGH);
    audio.setPinout(I2S_BCK, I2S_LRCK, I2S_DOUT);
    audio.setVolume(currentVolume);
}

void playerLoop() {
    audio.loop();
}

void playerLoadQueue(QueueEntry* entries, int count, int startIndex) {
    int n = (count > MAX_QUEUE) ? MAX_QUEUE : count;
    memcpy(queue, entries, n * sizeof(QueueEntry));
    queueSize = n;
    queueIdx = startIndex;
}

void playerClearQueue() {
    queueSize = 0;
    queueIdx = -1;
}

int playerQueueSize() { return queueSize; }
int playerCurrentIndex() { return queueIdx; }

void playerPlayCurrent() {
    if (queueIdx < 0 || queueIdx >= queueSize) return;
    radioMode = false;
    QueueEntry& t = queue[queueIdx];
    paused = false;
    eofFlag = false;
    String url = buildStreamUrl(String(t.id));
    Serial.printf("[player] %s - %s\n", t.artist, t.title);
    audio.connecttohost(url.c_str());
}

void playerPlayNext() {
    if (queueSize == 0) return;
    queueIdx++;
    if (queueIdx >= queueSize) queueIdx = 0;
    playerPlayCurrent();
}

void playerPlayPrev() {
    if (queueSize == 0) return;
    queueIdx--;
    if (queueIdx < 0) queueIdx = queueSize - 1;
    playerPlayCurrent();
}

void playerTogglePause() {
    paused = !paused;
    audio.pauseResume();
    Serial.printf("[player] %s\n", paused ? "paused" : "resumed");
}

void playerStop() {
    audio.stopSong();
    paused = false;
    Serial.println("[player] stopped");
}

void playerSetVolume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 21) vol = 21;
    currentVolume = vol;
    audio.setVolume(currentVolume);
}

int  playerGetVolume() { return currentVolume; }
bool playerIsPlaying() { return audio.isRunning(); }

bool playerCheckEof() {
    if (eofFlag) {
        eofFlag = false;
        return true;
    }
    return false;
}

const char* playerCurrentTitle() {
    if (queueIdx >= 0 && queueIdx < queueSize) return queue[queueIdx].title;
    return "";
}

const char* playerCurrentArtist() {
    if (queueIdx >= 0 && queueIdx < queueSize) return queue[queueIdx].artist;
    return "";
}

const char* playerCurrentCoverArt() {
    if (queueIdx >= 0 && queueIdx < queueSize) return queue[queueIdx].coverArt;
    return "";
}

void playerPlayRadio(const char* name, const char* url) {
    audio.stopSong();
    queueSize = 0;
    queueIdx = -1;
    radioMode = true;
    strlcpy(radioName, name, sizeof(radioName));
    strlcpy(radioUrl, url, sizeof(radioUrl));
    paused = false;
    eofFlag = false;
    Serial.printf("[player] Radio: %s\n", name);
    audio.connecttohost(url);
}

bool playerIsRadio() { return radioMode; }

static bool loadQueueFromJson(const String& response, const char* parentKey,
                               const char* arrayKey, int startIndex) {
    JsonDocument doc;
    if (deserializeJson(doc, response)) return false;

    JsonObject parent = doc["subsonic-response"][parentKey];
    JsonArray songs = parent[arrayKey];
    if (songs.isNull()) return false;

    const char* albumCoverArt = parent["coverArt"] | "";
    const char* albumArtist = parent["artist"] | "";

    QueueEntry* entries = (QueueEntry*)ps_malloc(MAX_QUEUE * sizeof(QueueEntry));
    if (!entries) return false;

    int count = 0;
    for (JsonObject s : songs) {
        if (count >= MAX_QUEUE) break;
        strlcpy(entries[count].id, s["id"] | "", sizeof(entries[0].id));
        strlcpy(entries[count].title, s["title"] | "", sizeof(entries[0].title));
        strlcpy(entries[count].artist, s["artist"] | albumArtist, sizeof(entries[0].artist));
        strlcpy(entries[count].coverArt, s["coverArt"] | albumCoverArt, sizeof(entries[0].coverArt));
        count++;
    }

    playerLoadQueue(entries, count, startIndex);
    free(entries);
    playerPlayCurrent();
    return true;
}

bool playerLoadAlbum(const char* albumId, int startIndex) {
    String r = apiCall(buildApiUrl("getAlbum.view") + "&id=" + albumId);
    if (r.length() == 0) return false;
    return loadQueueFromJson(r, "album", "song", startIndex);
}

bool playerLoadPlaylist(const char* playlistId, int startIndex) {
    String r = apiCall(buildApiUrl("getPlaylist.view") + "&id=" + playlistId);
    if (r.length() == 0) return false;
    return loadQueueFromJson(r, "playlist", "entry", startIndex);
}

// Audio callbacks (weak-linked by the Audio library)
void audio_info(const char* info) { Serial.printf("[audio] %s\n", info); }
void audio_eof_stream(const char* info) {
    Serial.println("[stream end]");
    eofFlag = true;
}
