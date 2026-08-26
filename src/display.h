#pragma once
#include <Arduino.h>
#include "config.h"

void    displayInit();
void    lcdFill(uint16_t color);
void    lcdFillRect(int x, int y, int w, int h, uint16_t color);
void    lcdDrawText(int x, int y, const char* text, uint16_t fg, uint16_t bg, int scale);
void    lcdDrawTextCentered(int y, const char* text, uint16_t fg, uint16_t bg, int scale);
String  fitText(const String& text, int maxW, int scale);
void    lcdShowMessage(const char* line1, const char* line2);
void    lcdShowNowPlaying(const char* title, const char* artist, const char* album);
void    fetchAndDrawCoverArt(const char* coverArtId);
void    lcdDrawMenuList(const char* title, MenuItem* items, int itemCount,
                        int cursorIndex, int scrollOffset);
void    lcdDrawOverlay(const char* text);
