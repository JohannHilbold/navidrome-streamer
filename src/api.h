#pragma once
#include <Arduino.h>
#include "config.h"

String buildApiUrl(const String& ep);
String buildStreamUrl(const String& id);
String apiCall(const String& url);
bool   testConnection();

int apiGetStarredAlbums(MenuItem* out, int maxCount);
int apiGetArtists(MenuItem* out, int maxCount);
int apiGetArtistAlbums(const char* artistId, MenuItem* out, int maxCount);
int apiGetAlbumSongs(const char* albumId, MenuItem* out, int maxCount);
int apiGetPlaylists(MenuItem* out, int maxCount);
int apiGetPlaylistSongs(const char* playlistId, MenuItem* out, int maxCount);
