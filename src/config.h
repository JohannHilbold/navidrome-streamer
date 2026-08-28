#pragma once

extern char* NAVIDROME_URL;
extern char* NAVIDROME_USER;
extern char* NAVIDROME_PASS;
extern const char* SUBSONIC_API_VERSION;
extern const char* SUBSONIC_CLIENT;

// ---- Hardware pins ----
#define I2S_BCK        39
#define I2S_LRCK       40
#define I2S_DOUT       41
#define I2S_SWITCH_IN  0

#define TOUCH_SDA      11
#define TOUCH_SCL      12
#define TOUCH_RST      10
#define TOUCH_IRQ      9

#define ENCODER_A      8
#define ENCODER_B      7

// ---- Colors (RGB565, byte-swapped for big-endian SPI) ----
#define SWAPCOLOR(c) ((((c) >> 8) & 0xFF) | (((c) & 0xFF) << 8))
#define COL_BG     0x0000
#define COL_WHITE  0xFFFF
#define COL_GRAY   SWAPCOLOR(0xC618)
#define COL_DKGRAY SWAPCOLOR(0x8410)
#define COL_CYAN   SWAPCOLOR(0x0677)

#define BATTERY_ADC_PIN 1

// ---- Display layout ----
#define SCREEN_W    360
#define SCREEN_H    360
#define ART_SIZE    200
#define ART_X       ((SCREEN_W - ART_SIZE) / 2)
#define ART_Y       15
#define TITLE_Y     225
#define ARTIST_Y    248
#define ALBUM_Y     271
#define TEXT_SCALE   2
#define MAX_TEXT_PX  320

// ---- Menu layout ----
#define MENU_TITLE_Y       30
#define MENU_FIRST_ITEM_Y  75
#define MENU_ITEM_SPACING  35
#define MENU_VISIBLE       7
#define MENU_TEXT_MARGIN   40
#define MENU_TEXT_MAX_W    280

// ---- Shared types ----
struct MenuItem {
    char id[40];
    char name[52];
};
