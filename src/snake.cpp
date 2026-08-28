#include "snake.h"
#include "config.h"
#include "display.h"
#include <Arduino.h>

#define GRID_W    20
#define GRID_H    18
#define CELL_SIZE 16
#define GRID_X    ((SCREEN_W - GRID_W * CELL_SIZE) / 2)
#define GRID_Y    50
#define MAX_BODY  (GRID_W * GRID_H)

#define COL_SNAKE_HEAD SWAPCOLOR(0x07E0)
#define COL_SNAKE_BODY SWAPCOLOR(0x04A0)
#define COL_FOOD       SWAPCOLOR(0xF800)
#define COL_GRID       SWAPCOLOR(0x18E3)

enum Dir { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
enum GameState { STATE_PLAYING, STATE_PAUSED, STATE_OVER };

struct Cell { int8_t x, y; };

static Cell body[MAX_BODY];
static int bodyLen;
static Dir dir;
static Dir nextDir;
static Cell food;
static int score;
static GameState state;
static unsigned long lastTick;
static int speed;
static bool active = false;

static void placeFood() {
    bool ok;
    do {
        ok = true;
        food.x = esp_random() % GRID_W;
        food.y = esp_random() % GRID_H;
        for (int i = 0; i < bodyLen; i++) {
            if (body[i].x == food.x && body[i].y == food.y) { ok = false; break; }
        }
    } while (!ok);
}

static void drawCell(int gx, int gy, uint16_t color) {
    int px = GRID_X + gx * CELL_SIZE;
    int py = GRID_Y + gy * CELL_SIZE;
    lcdFillRect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2, color);
}

static void drawScore() {
    char buf[24];
    snprintf(buf, sizeof(buf), "Score: %d", score);
    lcdFillRect(0, 10, SCREEN_W, 20, COL_BG);
    lcdDrawTextCentered(12, buf, COL_WHITE, COL_BG, 2);
}

static void drawFullGrid() {
    lcdFill(COL_BG);
    lcdFillRect(GRID_X, GRID_Y, GRID_W * CELL_SIZE, GRID_H * CELL_SIZE, COL_GRID);

    for (int x = 0; x < GRID_W; x++)
        for (int y = 0; y < GRID_H; y++)
            drawCell(x, y, COL_BG);

    for (int i = 0; i < bodyLen; i++)
        drawCell(body[i].x, body[i].y, i == 0 ? COL_SNAKE_HEAD : COL_SNAKE_BODY);

    drawCell(food.x, food.y, COL_FOOD);
    drawScore();
}

void snakeStart() {
    bodyLen = 3;
    int cx = GRID_W / 2;
    int cy = GRID_H / 2;
    for (int i = 0; i < bodyLen; i++) {
        body[i].x = cx - i;
        body[i].y = cy;
    }
    dir = DIR_RIGHT;
    nextDir = DIR_RIGHT;
    score = 0;
    speed = 200;
    state = STATE_PLAYING;
    active = true;
    lastTick = millis();

    placeFood();
    drawFullGrid();
}

static void snakeTick() {
    dir = nextDir;

    Cell newHead = body[0];
    switch (dir) {
        case DIR_UP:    newHead.y--; break;
        case DIR_DOWN:  newHead.y++; break;
        case DIR_LEFT:  newHead.x--; break;
        case DIR_RIGHT: newHead.x++; break;
    }

    if (newHead.x < 0 || newHead.x >= GRID_W ||
        newHead.y < 0 || newHead.y >= GRID_H) {
        state = STATE_OVER;
        return;
    }

    for (int i = 0; i < bodyLen; i++) {
        if (body[i].x == newHead.x && body[i].y == newHead.y) {
            state = STATE_OVER;
            return;
        }
    }

    bool ate = (newHead.x == food.x && newHead.y == food.y);

    if (!ate) {
        drawCell(body[bodyLen - 1].x, body[bodyLen - 1].y, COL_BG);
    }

    if (!ate) {
        for (int i = bodyLen - 1; i > 0; i--)
            body[i] = body[i - 1];
    } else {
        if (bodyLen < MAX_BODY) {
            for (int i = bodyLen; i > 0; i--)
                body[i] = body[i - 1];
            bodyLen++;
        }
        score++;
        if (speed > 80) speed -= 5;
        placeFood();
        drawCell(food.x, food.y, COL_FOOD);
        drawScore();
    }

    body[0] = newHead;

    if (bodyLen > 1)
        drawCell(body[1].x, body[1].y, COL_SNAKE_BODY);
    drawCell(body[0].x, body[0].y, COL_SNAKE_HEAD);
}

void snakeLoop() {
    if (!active || state != STATE_PLAYING) return;

    unsigned long now = millis();
    if (now - lastTick >= (unsigned long)speed) {
        lastTick = now;
        snakeTick();

        if (state == STATE_OVER) {
            lcdDrawTextCentered(160, "GAME OVER", COL_WHITE, COL_BG, 3);
            char buf[24];
            snprintf(buf, sizeof(buf), "Score: %d", score);
            lcdDrawTextCentered(200, buf, COL_GRAY, COL_BG, 2);
            lcdDrawTextCentered(240, "Tap to exit", COL_DKGRAY, COL_BG, 2);
        }
    }
}

bool snakeHandleInput(InputAction action) {
    if (!active) return false;

    if (state == STATE_OVER) {
        if (action == INPUT_TAP) {
            active = false;
            return true;
        }
        return false;
    }

    if (action == INPUT_TAP) {
        if (state == STATE_PLAYING) {
            state = STATE_PAUSED;
            lcdDrawTextCentered(160, "PAUSED", COL_WHITE, COL_BG, 3);
            lcdDrawTextCentered(200, "Tap to resume", COL_DKGRAY, COL_BG, 2);
        } else if (state == STATE_PAUSED) {
            state = STATE_PLAYING;
            lastTick = millis();
            drawFullGrid();
        }
        return false;
    }

    if (state != STATE_PLAYING) return false;

    switch (action) {
        case INPUT_SWIPE_UP:    if (dir != DIR_DOWN)  nextDir = DIR_UP;    break;
        case INPUT_SWIPE_DOWN:  if (dir != DIR_UP)    nextDir = DIR_DOWN;  break;
        case INPUT_SWIPE_LEFT:  if (dir != DIR_RIGHT) nextDir = DIR_LEFT;  break;
        case INPUT_SWIPE_RIGHT: if (dir != DIR_LEFT)  nextDir = DIR_RIGHT; break;
        default: break;
    }
    return false;
}

bool snakeIsActive() { return active; }
bool snakeIsGameOver() { return active && state == STATE_OVER; }
