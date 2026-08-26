#pragma once
#include <Arduino.h>

struct QueueEntry {
    char id[40];
    char title[52];
    char artist[40];
    char coverArt[40];
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
