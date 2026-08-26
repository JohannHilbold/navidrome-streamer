#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "Audio.h"
#include "soc/rtc_cntl_reg.h"
#include "MD5Builder.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "display/esp_lcd_sh8601.h"
#include "display/lcd_config.h"
#include "font5x7.h"
#include <TJpg_Decoder.h>

// ---- Configuration ----
const char* WIFI_SSID     = "thebestwifi";
const char* WIFI_PASSWORD = "REDACTED_WIFI_PASSWORD";
const char* NAVIDROME_URL  = "https://your-navidrome-server.example.com";
const char* NAVIDROME_USER = "REDACTED_USER";
const char* NAVIDROME_PASS = "REDACTED_USER";
const char* SUBSONIC_API_VERSION = "1.16.1";
const char* SUBSONIC_CLIENT = "ESP32Player";

// ---- Hardware ----
#define I2S_BCK        39
#define I2S_LRCK       40
#define I2S_DOUT       41
#define I2S_SWITCH_IN  0

// ---- Display ----
static esp_lcd_panel_handle_t lcd_panel = NULL;

// SH8601 init (from Waveshare official demo)
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
  {0xF0,(uint8_t[]){0x28},1,0},{0xF2,(uint8_t[]){0x28},1,0},
  {0x73,(uint8_t[]){0xF0},1,0},{0x7C,(uint8_t[]){0xD1},1,0},
  {0x83,(uint8_t[]){0xE0},1,0},{0x84,(uint8_t[]){0x61},1,0},
  {0xF2,(uint8_t[]){0x82},1,0},{0xF0,(uint8_t[]){0x00},1,0},
  {0xF0,(uint8_t[]){0x01},1,0},{0xF1,(uint8_t[]){0x01},1,0},
  {0xB0,(uint8_t[]){0x56},1,0},{0xB1,(uint8_t[]){0x4D},1,0},
  {0xB2,(uint8_t[]){0x24},1,0},{0xB4,(uint8_t[]){0x87},1,0},
  {0xB5,(uint8_t[]){0x44},1,0},{0xB6,(uint8_t[]){0x8B},1,0},
  {0xB7,(uint8_t[]){0x40},1,0},{0xB8,(uint8_t[]){0x86},1,0},
  {0xBA,(uint8_t[]){0x00},1,0},{0xBB,(uint8_t[]){0x08},1,0},
  {0xBC,(uint8_t[]){0x08},1,0},{0xBD,(uint8_t[]){0x00},1,0},
  {0xC0,(uint8_t[]){0x80},1,0},{0xC1,(uint8_t[]){0x10},1,0},
  {0xC2,(uint8_t[]){0x37},1,0},{0xC3,(uint8_t[]){0x80},1,0},
  {0xC4,(uint8_t[]){0x10},1,0},{0xC5,(uint8_t[]){0x37},1,0},
  {0xC6,(uint8_t[]){0xA9},1,0},{0xC7,(uint8_t[]){0x41},1,0},
  {0xC8,(uint8_t[]){0x01},1,0},{0xC9,(uint8_t[]){0xA9},1,0},
  {0xCA,(uint8_t[]){0x41},1,0},{0xCB,(uint8_t[]){0x01},1,0},
  {0xD0,(uint8_t[]){0x91},1,0},{0xD1,(uint8_t[]){0x68},1,0},
  {0xD2,(uint8_t[]){0x68},1,0},
  {0xF5,(uint8_t[]){0x00,0xA5},2,0},
  {0xDD,(uint8_t[]){0x4F},1,0},{0xDE,(uint8_t[]){0x4F},1,0},
  {0xF1,(uint8_t[]){0x10},1,0},{0xF0,(uint8_t[]){0x00},1,0},
  {0xF0,(uint8_t[]){0x02},1,0},
  {0xE0,(uint8_t[]){0xF0,0x0A,0x10,0x09,0x09,0x36,0x35,0x33,0x4A,0x29,0x15,0x15,0x2E,0x34},14,0},
  {0xE1,(uint8_t[]){0xF0,0x0A,0x0F,0x08,0x08,0x05,0x34,0x33,0x4A,0x39,0x15,0x15,0x2D,0x33},14,0},
  {0xF0,(uint8_t[]){0x10},1,0},{0xF3,(uint8_t[]){0x10},1,0},
  {0xE0,(uint8_t[]){0x07},1,0},{0xE1,(uint8_t[]){0x00},1,0},
  {0xE2,(uint8_t[]){0x00},1,0},{0xE3,(uint8_t[]){0x00},1,0},
  {0xE4,(uint8_t[]){0xE0},1,0},{0xE5,(uint8_t[]){0x06},1,0},
  {0xE6,(uint8_t[]){0x21},1,0},{0xE7,(uint8_t[]){0x01},1,0},
  {0xE8,(uint8_t[]){0x05},1,0},{0xE9,(uint8_t[]){0x02},1,0},
  {0xEA,(uint8_t[]){0xDA},1,0},{0xEB,(uint8_t[]){0x00},1,0},
  {0xEC,(uint8_t[]){0x00},1,0},{0xED,(uint8_t[]){0x0F},1,0},
  {0xEE,(uint8_t[]){0x00},1,0},{0xEF,(uint8_t[]){0x00},1,0},
  {0xF8,(uint8_t[]){0x00},1,0},{0xF9,(uint8_t[]){0x00},1,0},
  {0xFA,(uint8_t[]){0x00},1,0},{0xFB,(uint8_t[]){0x00},1,0},
  {0xFC,(uint8_t[]){0x00},1,0},{0xFD,(uint8_t[]){0x00},1,0},
  {0xFE,(uint8_t[]){0x00},1,0},{0xFF,(uint8_t[]){0x00},1,0},
  {0x60,(uint8_t[]){0x40},1,0},{0x61,(uint8_t[]){0x04},1,0},
  {0x62,(uint8_t[]){0x00},1,0},{0x63,(uint8_t[]){0x42},1,0},
  {0x64,(uint8_t[]){0xD9},1,0},{0x65,(uint8_t[]){0x00},1,0},
  {0x66,(uint8_t[]){0x00},1,0},{0x67,(uint8_t[]){0x00},1,0},
  {0x68,(uint8_t[]){0x00},1,0},{0x69,(uint8_t[]){0x00},1,0},
  {0x6A,(uint8_t[]){0x00},1,0},{0x6B,(uint8_t[]){0x00},1,0},
  {0x70,(uint8_t[]){0x40},1,0},{0x71,(uint8_t[]){0x03},1,0},
  {0x72,(uint8_t[]){0x00},1,0},{0x73,(uint8_t[]){0x42},1,0},
  {0x74,(uint8_t[]){0xD8},1,0},{0x75,(uint8_t[]){0x00},1,0},
  {0x76,(uint8_t[]){0x00},1,0},{0x77,(uint8_t[]){0x00},1,0},
  {0x78,(uint8_t[]){0x00},1,0},{0x79,(uint8_t[]){0x00},1,0},
  {0x7A,(uint8_t[]){0x00},1,0},{0x7B,(uint8_t[]){0x00},1,0},
  {0x80,(uint8_t[]){0x48},1,0},{0x81,(uint8_t[]){0x00},1,0},
  {0x82,(uint8_t[]){0x06},1,0},{0x83,(uint8_t[]){0x02},1,0},
  {0x84,(uint8_t[]){0xD6},1,0},{0x85,(uint8_t[]){0x04},1,0},
  {0x86,(uint8_t[]){0x00},1,0},{0x87,(uint8_t[]){0x00},1,0},
  {0x88,(uint8_t[]){0x48},1,0},{0x89,(uint8_t[]){0x00},1,0},
  {0x8A,(uint8_t[]){0x08},1,0},{0x8B,(uint8_t[]){0x02},1,0},
  {0x8C,(uint8_t[]){0xD8},1,0},{0x8D,(uint8_t[]){0x04},1,0},
  {0x8E,(uint8_t[]){0x00},1,0},{0x8F,(uint8_t[]){0x00},1,0},
  {0x90,(uint8_t[]){0x48},1,0},{0x91,(uint8_t[]){0x00},1,0},
  {0x92,(uint8_t[]){0x0A},1,0},{0x93,(uint8_t[]){0x02},1,0},
  {0x94,(uint8_t[]){0xDA},1,0},{0x95,(uint8_t[]){0x04},1,0},
  {0x96,(uint8_t[]){0x00},1,0},{0x97,(uint8_t[]){0x00},1,0},
  {0x98,(uint8_t[]){0x48},1,0},{0x99,(uint8_t[]){0x00},1,0},
  {0x9A,(uint8_t[]){0x0C},1,0},{0x9B,(uint8_t[]){0x02},1,0},
  {0x9C,(uint8_t[]){0xDC},1,0},{0x9D,(uint8_t[]){0x04},1,0},
  {0x9E,(uint8_t[]){0x00},1,0},{0x9F,(uint8_t[]){0x00},1,0},
  {0xA0,(uint8_t[]){0x48},1,0},{0xA1,(uint8_t[]){0x00},1,0},
  {0xA2,(uint8_t[]){0x05},1,0},{0xA3,(uint8_t[]){0x02},1,0},
  {0xA4,(uint8_t[]){0xD5},1,0},{0xA5,(uint8_t[]){0x04},1,0},
  {0xA6,(uint8_t[]){0x00},1,0},{0xA7,(uint8_t[]){0x00},1,0},
  {0xA8,(uint8_t[]){0x48},1,0},{0xA9,(uint8_t[]){0x00},1,0},
  {0xAA,(uint8_t[]){0x07},1,0},{0xAB,(uint8_t[]){0x02},1,0},
  {0xAC,(uint8_t[]){0xD7},1,0},{0xAD,(uint8_t[]){0x04},1,0},
  {0xAE,(uint8_t[]){0x00},1,0},{0xAF,(uint8_t[]){0x00},1,0},
  {0xB0,(uint8_t[]){0x48},1,0},{0xB1,(uint8_t[]){0x00},1,0},
  {0xB2,(uint8_t[]){0x09},1,0},{0xB3,(uint8_t[]){0x02},1,0},
  {0xB4,(uint8_t[]){0xD9},1,0},{0xB5,(uint8_t[]){0x04},1,0},
  {0xB6,(uint8_t[]){0x00},1,0},{0xB7,(uint8_t[]){0x00},1,0},
  {0xB8,(uint8_t[]){0x48},1,0},{0xB9,(uint8_t[]){0x00},1,0},
  {0xBA,(uint8_t[]){0x0B},1,0},{0xBB,(uint8_t[]){0x02},1,0},
  {0xBC,(uint8_t[]){0xDB},1,0},{0xBD,(uint8_t[]){0x04},1,0},
  {0xBE,(uint8_t[]){0x00},1,0},{0xBF,(uint8_t[]){0x00},1,0},
  {0xC0,(uint8_t[]){0x10},1,0},{0xC1,(uint8_t[]){0x47},1,0},
  {0xC2,(uint8_t[]){0x56},1,0},{0xC3,(uint8_t[]){0x65},1,0},
  {0xC4,(uint8_t[]){0x74},1,0},{0xC5,(uint8_t[]){0x88},1,0},
  {0xC6,(uint8_t[]){0x99},1,0},{0xC7,(uint8_t[]){0x01},1,0},
  {0xC8,(uint8_t[]){0xBB},1,0},{0xC9,(uint8_t[]){0xAA},1,0},
  {0xD0,(uint8_t[]){0x10},1,0},{0xD1,(uint8_t[]){0x47},1,0},
  {0xD2,(uint8_t[]){0x56},1,0},{0xD3,(uint8_t[]){0x65},1,0},
  {0xD4,(uint8_t[]){0x74},1,0},{0xD5,(uint8_t[]){0x88},1,0},
  {0xD6,(uint8_t[]){0x99},1,0},{0xD7,(uint8_t[]){0x01},1,0},
  {0xD8,(uint8_t[]){0xBB},1,0},{0xD9,(uint8_t[]){0xAA},1,0},
  {0xF3,(uint8_t[]){0x01},1,0},{0xF0,(uint8_t[]){0x00},1,0},
  {0x21,(uint8_t[]){0x00},1,0},
  {0x11,(uint8_t[]){0x00},1,120},
  {0x29,(uint8_t[]){0x00},1,0},
  {0x36,(uint8_t[]){0x00},1,0},
};

// ---- Audio ----
Audio audio;
int currentVolume = 10;
const char* RADIO_URL = "http://mp3.ffh.de/radioffh/hqlivestream.mp3";

// ---- Track state ----
String currentTitle = "";
String currentArtist = "";
String currentAlbum = "";
String currentCoverArtId = "";

// ---- Colors (RGB565, byte-swapped for big-endian SPI) ----
#define SWAPCOLOR(c) ((((c) >> 8) & 0xFF) | (((c) & 0xFF) << 8))
#define COL_BG     0x0000
#define COL_WHITE  0xFFFF
#define COL_GRAY   SWAPCOLOR(0xC618)
#define COL_DKGRAY SWAPCOLOR(0x8410)

// ---- Layout ----
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

// ---- Subsonic auth ----
String currentSalt, currentToken;

String md5(const String& input) {
    MD5Builder b; b.begin(); b.add(input); b.calculate(); return b.toString();
}

void generateAuth() {
    currentSalt = "";
    for (int i = 0; i < 12; i++) currentSalt += (char)('a' + (esp_random() % 26));
    currentToken = md5(String(NAVIDROME_PASS) + currentSalt);
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

// ---- Display ----
void initDisplay() {
    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(
        EXAMPLE_PIN_NUM_LCD_PCLK, EXAMPLE_PIN_NUM_LCD_DATA0,
        EXAMPLE_PIN_NUM_LCD_DATA1, EXAMPLE_PIN_NUM_LCD_DATA2,
        EXAMPLE_PIN_NUM_LCD_DATA3, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
        EXAMPLE_PIN_NUM_LCD_CS, NULL, NULL);
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io));

    sh8601_vendor_config_t vc = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = { .use_qspi_interface = 1 },
    };
    esp_lcd_panel_dev_config_t pc = {};
    pc.reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST;
    pc.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    pc.bits_per_pixel = 16;
    pc.vendor_config = &vc;
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io, &pc, &lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));

    pinMode(EXAMPLE_PIN_NUM_BK_LIGHT, OUTPUT);
    analogWrite(EXAMPLE_PIN_NUM_BK_LIGHT, 200);
    Serial.println("[display] OK");
}

void lcdFill(uint16_t color) {
    int chunkH = 20;
    size_t n = SCREEN_W * chunkH;
    uint16_t* buf = (uint16_t*)heap_caps_malloc(n * 2, MALLOC_CAP_DMA);
    if (!buf) return;
    for (size_t i = 0; i < n; i++) buf[i] = color;
    for (int y = 0; y < SCREEN_H; y += chunkH)
        esp_lcd_panel_draw_bitmap(lcd_panel, 0, y, SCREEN_W, y + chunkH, buf);
    delay(2);
    free(buf);
}

void lcdFillRect(int x, int y, int w, int h, uint16_t color) {
    int chunkH = 20;
    if (chunkH > h) chunkH = h;
    size_t n = w * chunkH;
    uint16_t* buf = (uint16_t*)heap_caps_malloc(n * 2, MALLOC_CAP_DMA);
    if (!buf) return;
    for (size_t i = 0; i < n; i++) buf[i] = color;
    for (int row = y; row < y + h; row += chunkH) {
        int ch = chunkH;
        if (row + ch > y + h) ch = y + h - row;
        esp_lcd_panel_draw_bitmap(lcd_panel, x, row, x + w, row + ch, buf);
    }
    delay(2);
    free(buf);
}

void lcdDrawText(int x, int y, const char* text, uint16_t fg, uint16_t bg, int scale) {
    int len = strlen(text);
    if (len == 0) return;
    int cw = 6 * scale;
    int ch = 8 * scale;
    int totalW = len * cw;
    if (x + totalW > SCREEN_W) totalW = SCREEN_W - x;
    if (totalW <= 0 || y + ch > SCREEN_H) return;

    size_t bufSz = totalW * ch;
    uint16_t* buf = (uint16_t*)heap_caps_malloc(bufSz * 2, MALLOC_CAP_DMA);
    if (!buf) return;

    for (size_t i = 0; i < bufSz; i++) buf[i] = bg;

    for (int ci = 0; ci < len; ci++) {
        int cx = ci * cw;
        if (cx >= totalW) break;
        char c = text[ci];
        if (c < 32 || c > 126) c = '?';
        const uint8_t* glyph = font5x7[c - 32];

        for (int col = 0; col < 5; col++) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) {
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            int px = cx + col * scale + sx;
                            int py = row * scale + sy;
                            if (px < totalW)
                                buf[py * totalW + px] = fg;
                        }
                    }
                }
            }
        }
    }

    esp_lcd_panel_draw_bitmap(lcd_panel, x, y, x + totalW, y + ch, buf);
    delay(2);
    free(buf);
}

String fitText(const String& text, int maxW, int scale) {
    int maxChars = maxW / (6 * scale);
    if ((int)text.length() <= maxChars) return text;
    if (maxChars < 3) return text.substring(0, maxChars);
    return text.substring(0, maxChars - 2) + "..";
}

void lcdDrawTextCentered(int y, const char* text, uint16_t fg, uint16_t bg, int scale) {
    int w = strlen(text) * 6 * scale;
    int x = (SCREEN_W - w) / 2;
    if (x < 0) x = 0;
    lcdDrawText(x, y, text, fg, bg, scale);
}

void lcdShowMessage(const char* line1, const char* line2) {
    lcdFill(COL_BG);
    if (line1) {
        String t = fitText(line1, MAX_TEXT_PX, 3);
        lcdDrawTextCentered(155, t.c_str(), COL_WHITE, COL_BG, 3);
    }
    if (line2) {
        String t = fitText(line2, MAX_TEXT_PX, 2);
        lcdDrawTextCentered(190, t.c_str(), COL_GRAY, COL_BG, 2);
    }
    Serial.printf("[display] %s %s\n", line1, line2 ? line2 : "");
}

void lcdShowNowPlaying() {
    lcdFill(COL_BG);

    // Title
    String t = fitText(currentTitle, MAX_TEXT_PX, TEXT_SCALE);
    lcdDrawTextCentered(TITLE_Y, t.c_str(), COL_WHITE, COL_BG, TEXT_SCALE);

    // Artist
    t = fitText(currentArtist, MAX_TEXT_PX, TEXT_SCALE);
    lcdDrawTextCentered(ARTIST_Y, t.c_str(), COL_GRAY, COL_BG, TEXT_SCALE);

    // Album
    if (currentAlbum.length() > 0) {
        t = fitText(currentAlbum, MAX_TEXT_PX, TEXT_SCALE);
        lcdDrawTextCentered(ALBUM_Y, t.c_str(), COL_DKGRAY, COL_BG, TEXT_SCALE);
    }

    Serial.printf("[display] Now: %s - %s\n", currentArtist.c_str(), currentTitle.c_str());
}

// ---- TJpg_Decoder callback ----
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (x + w > SCREEN_W || y + h > SCREEN_H) return true;
    esp_lcd_panel_draw_bitmap(lcd_panel, x, y, x + w, y + h, bitmap);
    return true;
}

void fetchAndDrawCoverArt() {
    if (currentCoverArtId.length() == 0) return;

    String url = buildApiUrl("getCoverArt.view") +
                 "&id=" + currentCoverArtId +
                 "&size=" + String(ART_SIZE);

    WiFiClientSecure c;
    c.setInsecure();
    HTTPClient http;
    if (!http.begin(c, url)) return;

    int code = http.GET();
    if (code != 200) {
        Serial.printf("[art] HTTP %d\n", code);
        http.end();
        return;
    }

    int contentLen = http.getSize();
    size_t maxLen = (contentLen > 0) ? (size_t)contentLen : 200000;
    uint8_t* buf = (uint8_t*)ps_malloc(maxLen);
    if (!buf) { http.end(); return; }

    WiFiClient* stream = http.getStreamPtr();
    size_t total = 0;
    unsigned long t0 = millis();
    while (http.connected() && total < maxLen && millis() - t0 < 10000) {
        size_t avail = stream->available();
        if (avail) {
            size_t n = stream->readBytes(buf + total, min(avail, maxLen - total));
            total += n;
            if (contentLen > 0 && total >= (size_t)contentLen) break;
        } else {
            delay(1);
        }
    }
    http.end();

    if (total > 0) {
        uint16_t jpgW, jpgH;
        TJpgDec.getJpgSize(&jpgW, &jpgH, buf, total);
        int x = (SCREEN_W - jpgW) / 2;
        int y = ART_Y + (ART_SIZE - jpgH) / 2;
        Serial.printf("[art] %dx%d (%d bytes)\n", jpgW, jpgH, total);
        TJpgDec.drawJpg(x, y, buf, total);
    }

    free(buf);
}

// ---- Navidrome ----
bool testConnection() {
    String r = apiCall(buildApiUrl("ping.view"));
    if (r.length() == 0) return false;
    JsonDocument doc; deserializeJson(doc, r);
    const char* s = doc["subsonic-response"]["status"];
    bool ok = s && String(s) == "ok";
    Serial.printf("[navidrome] Ping: %s\n", ok ? "OK" : "FAILED");
    return ok;
}

void playRandomSong() {
    Serial.println("[navidrome] Fetching random song...");
    String r = apiCall(buildApiUrl("getRandomSongs.view") + "&size=1");
    if (r.length() == 0) return;
    JsonDocument doc;
    if (deserializeJson(doc, r)) return;
    const char* s = doc["subsonic-response"]["status"];
    if (!s || String(s) != "ok") return;
    JsonObject song = doc["subsonic-response"]["randomSongs"]["song"][0];
    const char* id = song["id"];
    if (!id) return;

    currentTitle = song["title"] | "Unknown";
    currentArtist = song["artist"] | "Unknown";
    currentAlbum = song["album"] | "";
    currentCoverArtId = song["coverArt"] | "";
    Serial.printf("[navidrome] %s - %s (%s)\n", currentArtist.c_str(), currentTitle.c_str(), currentAlbum.c_str());

    lcdShowNowPlaying();
    audio.connecttohost(buildStreamUrl(String(id)).c_str());
    fetchAndDrawCoverArt();
}

void searchAndPlay(const String& q) {
    String r = apiCall(buildApiUrl("search3.view") + "&query=" + q + "&songCount=1&albumCount=0&artistCount=0");
    if (r.length() == 0) return;
    JsonDocument doc; deserializeJson(doc, r);
    JsonObject song = doc["subsonic-response"]["searchResult3"]["song"][0];
    const char* id = song["id"];
    if (!id) { Serial.printf("[navidrome] No results for '%s'\n", q.c_str()); return; }
    currentTitle = song["title"] | "Unknown";
    currentArtist = song["artist"] | "Unknown";
    currentAlbum = song["album"] | "";
    currentCoverArtId = song["coverArt"] | "";
    Serial.printf("[navidrome] %s - %s\n", currentArtist.c_str(), currentTitle.c_str());

    lcdShowNowPlaying();
    audio.connecttohost(buildStreamUrl(String(id)).c_str());
    fetchAndDrawCoverArt();
}

// ---- Utilities ----
void rebootToBootloader() {
    Serial.println("Rebooting..."); Serial.flush(); delay(1000);
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_restart();
}

void connectWiFi() {
    Serial.printf("Wi-Fi: %s", WIFI_SSID);
    WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int a = 0;
    while (WiFi.status() != WL_CONNECTED && a < 30) { delay(500); Serial.print("."); a++; }
    if (WiFi.status() == WL_CONNECTED) Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
    else Serial.println("\nFAILED");
}

void handleSerialCommand() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n'); cmd.trim();
    if (cmd == "flash") rebootToBootloader();
    else if (cmd == "wifi") connectWiFi();
    else if (cmd == "ping") testConnection();
    else if (cmd == "random") playRandomSong();
    else if (cmd.startsWith("search ")) searchAndPlay(cmd.substring(7));
    else if (cmd == "radio") { audio.connecttohost(RADIO_URL); }
    else if (cmd == "stop") { audio.stopSong(); lcdShowMessage("Stopped", NULL); }
    else if (cmd == "next") { audio.stopSong(); playRandomSong(); }
    else if (cmd == "v+") { currentVolume = min(21, currentVolume+1); audio.setVolume(currentVolume); Serial.printf("Vol: %d\n", currentVolume); }
    else if (cmd == "v-") { currentVolume = max(0, currentVolume-1); audio.setVolume(currentVolume); Serial.printf("Vol: %d\n", currentVolume); }
    else if (cmd == "status") { Serial.printf("Vol:%d WiFi:%s Run:%d Heap:%d PSRAM:%d\n", currentVolume, WiFi.isConnected()?"ok":"no", audio.isRunning(), ESP.getFreeHeap(), ESP.getFreePsram()); }
    else if (cmd == "switch") { int c=digitalRead(I2S_SWITCH_IN); digitalWrite(I2S_SWITCH_IN,!c); Serial.printf("I2S GPIO0=%s\n",!c?"H":"L"); }
    else if (cmd.length()>0) Serial.println("random|next|search <q>|radio|stop|v+|v-|ping|status|switch|flash");
}

// ---- Audio callbacks ----
void audio_info(const char *info) { Serial.printf("[audio] %s\n", info); }
void audio_showstreamtitle(const char *info) {
    if (String(info).length() > 0) { currentTitle = info; Serial.printf("[now] %s\n", info); }
}
void audio_eof_stream(const char *info) { Serial.println("[stream end]"); playRandomSong(); }

// ---- Setup & Loop ----
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== Navidrome Streamer v4 ===");

    // I2S switch
    pinMode(I2S_SWITCH_IN, OUTPUT);
    digitalWrite(I2S_SWITCH_IN, HIGH);

    // Display
    initDisplay();
    lcdShowMessage("Connecting...", NULL);

    // JPEG decoder
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);

    // Wi-Fi
    connectWiFi();

    // Audio
    audio.setPinout(I2S_BCK, I2S_LRCK, I2S_DOUT);
    audio.setVolume(currentVolume);

    if (WiFi.isConnected() && testConnection()) {
        lcdShowMessage("Ready", "Type 'random'");
    } else {
        lcdShowMessage("No connection", NULL);
    }
    Serial.println("Type 'help' for commands.");
}

void loop() {
    audio.loop();
    handleSerialCommand();
}
