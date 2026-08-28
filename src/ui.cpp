#include "ui.h"
#include "config.h"
#include "display.h"
#include "input.h"
#include "api.h"
#include "player.h"
#include "settings.h"
#include "portal.h"
#include "snake.h"
#include <string.h>
#include <WiFi.h>

enum ScreenType {
    SCREEN_MAIN_MENU,
    SCREEN_STARRED_ALBUMS,
    SCREEN_ARTIST_LIST,
    SCREEN_ARTIST_ALBUMS,
    SCREEN_ALBUM_SONGS,
    SCREEN_PLAYLIST_LIST,
    SCREEN_PLAYLIST_SONGS,
    SCREEN_RADIO_LIST,
    SCREEN_SNAKE,
    SCREEN_NOW_PLAYING,
    SCREEN_SETTINGS,
};

struct ScreenState {
    ScreenType type;
    char contextId[24];
    int cursorIndex;
    int scrollOffset;
};

#define NAV_STACK_DEPTH 6
#define MAX_MENU_ITEMS 500

static ScreenState navStack[NAV_STACK_DEPTH];
static int navDepth = 0;
static MenuItem* menuItems = NULL;
static int menuItemCount = 0;
static bool needsRedraw = true;

static ScreenState& current() { return navStack[navDepth - 1]; }

static void pushScreen(ScreenType type, const char* contextId) {
    if (navDepth >= NAV_STACK_DEPTH) return;
    navStack[navDepth].type = type;
    strlcpy(navStack[navDepth].contextId, contextId, sizeof(navStack[0].contextId));
    navStack[navDepth].cursorIndex = 0;
    navStack[navDepth].scrollOffset = 0;
    navDepth++;
}

static void popScreen() {
    if (navDepth <= 1) return;
    navDepth--;
}

static const char* screenTitle() {
    switch (current().type) {
        case SCREEN_MAIN_MENU:      return "Music";
        case SCREEN_STARRED_ALBUMS: return "Favorites";
        case SCREEN_ARTIST_LIST:    return "Artists";
        case SCREEN_ARTIST_ALBUMS:  return "Albums";
        case SCREEN_ALBUM_SONGS:    return "Songs";
        case SCREEN_PLAYLIST_LIST:  return "Playlists";
        case SCREEN_PLAYLIST_SONGS: return "Songs";
        case SCREEN_RADIO_LIST:     return "Radio";
        case SCREEN_SETTINGS:       return "Settings";
        default: return "";
    }
}

static void renderNowPlaying() {
    if (playerIsRadio()) {
        lcdShowNowPlaying(playerCurrentTitle(), "Web Radio", "");
    } else {
        lcdShowNowPlaying(playerCurrentTitle(), playerCurrentArtist(), "");
        fetchAndDrawCoverArt(playerCurrentCoverArt());
    }
}

static void loadScreenData() {
    menuItemCount = 0;

    switch (current().type) {
        case SCREEN_MAIN_MENU: {
            static const char* names[] = {"Favorites", "Artists", "Playlists", "Radio", "Snake", "Settings"};
            static const char* ids[]   = {"fav", "art", "pls", "rad", "snk", "set"};
            for (int i = 0; i < 6; i++) {
                strlcpy(menuItems[i].id, ids[i], sizeof(menuItems[0].id));
                strlcpy(menuItems[i].name, names[i], sizeof(menuItems[0].name));
            }
            menuItemCount = 6;
            break;
        }
        case SCREEN_STARRED_ALBUMS:
            lcdShowMessage("Loading...", "Favorites");
            menuItemCount = apiGetStarredAlbums(menuItems, MAX_MENU_ITEMS);
            break;
        case SCREEN_ARTIST_LIST:
            lcdShowMessage("Loading...", "Artists");
            menuItemCount = apiGetArtists(menuItems, MAX_MENU_ITEMS);
            break;
        case SCREEN_ARTIST_ALBUMS:
            lcdShowMessage("Loading...", "Albums");
            menuItemCount = apiGetArtistAlbums(current().contextId, menuItems, MAX_MENU_ITEMS);
            break;
        case SCREEN_ALBUM_SONGS:
            lcdShowMessage("Loading...", "Songs");
            menuItemCount = apiGetAlbumSongs(current().contextId, menuItems, MAX_MENU_ITEMS);
            break;
        case SCREEN_PLAYLIST_LIST:
            lcdShowMessage("Loading...", "Playlists");
            menuItemCount = apiGetPlaylists(menuItems, MAX_MENU_ITEMS);
            break;
        case SCREEN_PLAYLIST_SONGS:
            lcdShowMessage("Loading...", "Songs");
            menuItemCount = apiGetPlaylistSongs(current().contextId, menuItems, MAX_MENU_ITEMS);
            break;
        case SCREEN_RADIO_LIST: {
            int count = settingsGetRadioCount();
            for (int i = 0; i < count && i < MAX_MENU_ITEMS; i++) {
                RadioStation r = settingsGetRadio(i);
                snprintf(menuItems[i].id, sizeof(menuItems[0].id), "%d", i);
                strlcpy(menuItems[i].name, r.name, sizeof(menuItems[0].name));
            }
            menuItemCount = count;
            break;
        }
        case SCREEN_SETTINGS: {
            strlcpy(menuItems[0].id, "wifi", sizeof(menuItems[0].id));
            strlcpy(menuItems[0].name, "WiFi Setup", sizeof(menuItems[0].name));

            strlcpy(menuItems[1].id, "ip", sizeof(menuItems[0].id));
            String ipLabel = "Remote: " + WiFi.localIP().toString();
            strlcpy(menuItems[1].name, ipLabel.c_str(), sizeof(menuItems[0].name));

            strlcpy(menuItems[2].id, "reset", sizeof(menuItems[0].id));
            strlcpy(menuItems[2].name, "Reset All", sizeof(menuItems[0].name));

            menuItemCount = 3;
            break;
        }
        default:
            break;
    }
    needsRedraw = true;
}

static void loadQueueAndPlay(ScreenType type, const char* contextId, int startIndex) {
    lcdShowMessage("Loading...", NULL);

    bool ok;
    if (type == SCREEN_ALBUM_SONGS)
        ok = playerLoadAlbum(contextId, startIndex);
    else
        ok = playerLoadPlaylist(contextId, startIndex);

    if (!ok) return;

    pushScreen(SCREEN_NOW_PLAYING, "");
    renderNowPlaying();
}

static void selectItem() {
    int idx = current().cursorIndex;
    if (idx < 0 || idx >= menuItemCount) return;

    switch (current().type) {
        case SCREEN_MAIN_MENU:
            if (strcmp(menuItems[idx].id, "fav") == 0) {
                pushScreen(SCREEN_STARRED_ALBUMS, "");
                loadScreenData();
            } else if (strcmp(menuItems[idx].id, "art") == 0) {
                pushScreen(SCREEN_ARTIST_LIST, "");
                loadScreenData();
            } else if (strcmp(menuItems[idx].id, "pls") == 0) {
                pushScreen(SCREEN_PLAYLIST_LIST, "");
                loadScreenData();
            } else if (strcmp(menuItems[idx].id, "rad") == 0) {
                pushScreen(SCREEN_RADIO_LIST, "");
                loadScreenData();
            } else if (strcmp(menuItems[idx].id, "snk") == 0) {
                pushScreen(SCREEN_SNAKE, "");
                snakeStart();
            } else if (strcmp(menuItems[idx].id, "set") == 0) {
                pushScreen(SCREEN_SETTINGS, "");
                loadScreenData();
            }
            break;

        case SCREEN_STARRED_ALBUMS:
        case SCREEN_ARTIST_ALBUMS:
            pushScreen(SCREEN_ALBUM_SONGS, menuItems[idx].id);
            loadScreenData();
            break;

        case SCREEN_ARTIST_LIST:
            pushScreen(SCREEN_ARTIST_ALBUMS, menuItems[idx].id);
            loadScreenData();
            break;

        case SCREEN_PLAYLIST_LIST:
            pushScreen(SCREEN_PLAYLIST_SONGS, menuItems[idx].id);
            loadScreenData();
            break;

        case SCREEN_ALBUM_SONGS:
        case SCREEN_PLAYLIST_SONGS:
            loadQueueAndPlay(current().type, current().contextId, idx);
            break;

        case SCREEN_RADIO_LIST: {
            int stationIdx = atoi(menuItems[idx].id);
            RadioStation r = settingsGetRadio(stationIdx);
            if (strlen(r.url) > 0) {
                playerPlayRadio(r.name, r.url);
                pushScreen(SCREEN_NOW_PLAYING, "");
                renderNowPlaying();
            }
            break;
        }

        case SCREEN_SETTINGS:
            if (strcmp(menuItems[idx].id, "wifi") == 0) {
                portalStart();
                lcdShowMessage("WiFi Setup", "Connect to");
                lcdDrawTextCentered(225, "ESP32-Music", COL_WHITE, COL_BG, 2);
                lcdDrawTextCentered(255, "WiFi network", COL_GRAY, COL_BG, 2);
            } else if (strcmp(menuItems[idx].id, "reset") == 0) {
                lcdShowMessage("Resetting...", NULL);
                settingsResetAll();
                delay(500);
                ESP.restart();
            }
            break;

        default:
            break;
    }
}

static void handleMenuInput(InputAction action) {
    ScreenState& scr = current();

    switch (action) {
        case INPUT_KNOB_CW:
            if (menuItemCount > 0) {
                scr.cursorIndex++;
                if (scr.cursorIndex >= menuItemCount) {
                    scr.cursorIndex = 0;
                    scr.scrollOffset = 0;
                } else if (scr.cursorIndex >= scr.scrollOffset + MENU_VISIBLE) {
                    scr.scrollOffset = scr.cursorIndex - MENU_VISIBLE + 1;
                }
                needsRedraw = true;
            }
            break;

        case INPUT_KNOB_CCW:
            if (menuItemCount > 0) {
                scr.cursorIndex--;
                if (scr.cursorIndex < 0) {
                    scr.cursorIndex = menuItemCount - 1;
                    scr.scrollOffset = max(0, menuItemCount - MENU_VISIBLE);
                } else if (scr.cursorIndex < scr.scrollOffset) {
                    scr.scrollOffset = scr.cursorIndex;
                }
                needsRedraw = true;
            }
            break;

        case INPUT_SWIPE_DOWN:
        case INPUT_TAP:
            selectItem();
            break;

        case INPUT_SWIPE_UP:
            if (navDepth > 1) {
                popScreen();
                loadScreenData();
            }
            break;

        default:
            break;
    }
}

static unsigned long overlayUntil = 0;

static void showOverlay(const char* text, unsigned long ms) {
    lcdDrawOverlay(text);
    overlayUntil = millis() + ms;
}

static void handleNowPlayingInput(InputAction action) {
    switch (action) {
        case INPUT_TAP:
            playerTogglePause();
            showOverlay(playerIsPlaying() ? "Playing" : "Paused", 1500);
            break;

        case INPUT_SWIPE_RIGHT:
            if (!playerIsRadio()) {
                playerPlayNext();
                renderNowPlaying();
            }
            break;

        case INPUT_SWIPE_LEFT:
            if (!playerIsRadio()) {
                playerPlayPrev();
                renderNowPlaying();
            }
            break;

        case INPUT_KNOB_CW: {
            playerSetVolume(playerGetVolume() + 1);
            char buf[16];
            snprintf(buf, sizeof(buf), "Vol %d", playerGetVolume());
            showOverlay(buf, 1200);
            break;
        }

        case INPUT_KNOB_CCW: {
            playerSetVolume(playerGetVolume() - 1);
            char buf[16];
            snprintf(buf, sizeof(buf), "Vol %d", playerGetVolume());
            showOverlay(buf, 1200);
            break;
        }

        case INPUT_SWIPE_UP:
            if (navDepth > 1) {
                popScreen();
                loadScreenData();
            }
            break;

        default:
            break;
    }
}

static void renderCurrentScreen() {
    if (current().type == SCREEN_NOW_PLAYING) {
        renderNowPlaying();
        return;
    }
    lcdDrawMenuList(screenTitle(), menuItems, menuItemCount,
                    current().cursorIndex, current().scrollOffset);
}

void uiInit() {
    menuItems = (MenuItem*)ps_malloc(MAX_MENU_ITEMS * sizeof(MenuItem));
    if (!menuItems)
        menuItems = (MenuItem*)malloc(50 * sizeof(MenuItem));

    inputInit();
    pushScreen(SCREEN_MAIN_MENU, "");
    loadScreenData();
}

void uiLoop() {
    InputAction action = inputPoll();

    if (current().type == SCREEN_SNAKE) {
        if (action != INPUT_NONE) {
            bool exit = snakeHandleInput(action);
            if (exit) {
                popScreen();
                loadScreenData();
            }
        }
        snakeLoop();
        return;
    }

    if (action != INPUT_NONE) {
        if (current().type == SCREEN_NOW_PLAYING)
            handleNowPlayingInput(action);
        else
            handleMenuInput(action);
    }

    if (playerCheckEof()) {
        playerPlayNext();
        if (current().type == SCREEN_NOW_PLAYING)
            renderNowPlaying();
    }

    if (overlayUntil > 0 && millis() > overlayUntil) {
        overlayUntil = 0;
        if (current().type == SCREEN_NOW_PLAYING)
            renderNowPlaying();
    }

    if (needsRedraw) {
        needsRedraw = false;
        renderCurrentScreen();
    }
}
