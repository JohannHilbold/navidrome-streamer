#pragma once
#include <Arduino.h>

struct QueueEntry {
    char id[40];
    char title[52];
    char artist[40];
    char coverArt[40];
    uint32_t duration;
};

void    playerInit();
void    playerLoop();

void    playerLoadQueue(QueueEntry* entries, int count, int startIndex);
void    playerClearQueue();
int     playerQueueSize();
int     playerCurrentIndex();

void    playerPlayCurrent();
void    playerPlayNext();
void    playerPlayPrev();
void    playerTogglePause();
void    playerStop();
void    playerSetVolume(int vol);
int     playerGetVolume();
bool    playerIsPlaying();
bool    playerCheckEof();

const char* playerCurrentTitle();
const char* playerCurrentArtist();
const char* playerCurrentCoverArt();
uint32_t    playerGetElapsed();
uint32_t    playerGetDuration();

void    playerPlayRadio(const char* name, const char* url);
bool    playerIsRadio();

bool    playerLoadAlbum(const char* albumId, int startIndex);
bool    playerLoadPlaylist(const char* playlistId, int startIndex);
