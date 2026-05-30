#include "../Config.h"
#include "../Games.h"
#include "FKLevels.h"

// Game board physical constants matching your pre-built levels configuration
#define MAX_FK_BLOCKS 12
#define GRID_SIZE 6
#define CELL_SIZE 9

// Static game states and variables
static FKBlock fkBlocks[MAX_FK_BLOCKS];
static int fkBlockCount = 0;
static int8_t fkSelectedIdx = -1;
static int8_t fkCursorX = 0, fkCursorY = 0;
static bool fkLevelComplete = false;
static int fkLevel = 1;

static int fkMoves = 0;
static bool fkBlockMovedInSession = false;
static bool fkJoyCenteredAfterWin = false;

// Helper function to check if a specific grid cell is occupied by any active
// block
static bool isCellOccupied(int8_t x, int8_t y, int8_t excludeIdx) {
  if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE)
    return true;
  for (int i = 0; i < fkBlockCount; i++) {
    if (!fkBlocks[i].active || i == excludeIdx)
      continue;
    if (x >= fkBlocks[i].x && x < fkBlocks[i].x + fkBlocks[i].w &&
        y >= fkBlocks[i].y && y < fkBlocks[i].y + fkBlocks[i].h) {
      return true;
    }
  }
  return false;
}

// Helper function to check if a block can be safely placed at coordinates
static bool canPlaceBlock(int8_t x, int8_t y, int8_t w, int8_t h,
                          int8_t excludeIdx) {
  for (int8_t r = 0; r < h; r++) {
    for (int8_t c = 0; c < w; c++) {
      if (isCellOccupied(x + c, y + r, excludeIdx))
        return false;
    }
  }
  return true;
}

void initFreeKeyGame() {
  fkLevelComplete = false;
  fkSelectedIdx = -1;
  fkBlockMovedInSession = false;
  fkJoyCenteredAfterWin = false;

  // 100 level circular wrap-around
  int levelIdx = (fkLevel - 1) % 100;

  // Copy level data directly from flash memory (PROGMEM)
  fkBlockCount = pgm_read_byte(&(PREBUILT_LEVELS[levelIdx].blockCount));
  fkMoves = 0; // Reset moves count for the new level

  // Copy blocks configuration into memory securely
  memcpy_P(fkBlocks, PREBUILT_LEVELS[levelIdx].blocks,
           sizeof(FKBlock) * MAX_FK_BLOCKS);

  // Set the initial cursor position to the key piece (index 0)
  fkCursorX = fkBlocks[0].x;
  fkCursorY = fkBlocks[0].y;
}

void handleFreeKeyMode(int vry, int vrx, bool clicked) {
  if (fkLevelComplete) {
    bool isCentered =
        (vrx >= 1000 && vrx <= 3000 && vry >= 1000 && vry <= 3000);
    if (isCentered)
      fkJoyCenteredAfterWin = true;

    if (fkJoyCenteredAfterWin) {
      bool proceedNext =
          clicked || (!isCentered && (millis() - lastJoyAction > JOY_DELAY));
      if (proceedNext) {
        fkLevel++;
        initFreeKeyGame(); // Load next level instantly
        lastJoyAction = millis();
        return;
      }
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(12, 12);
    display.println(F("ESCAPE OK"));
    display.setTextSize(1);
    display.setCursor(12, 40);

    if (!fkJoyCenteredAfterWin)
      display.print(F("[Release Joystick]"));
    else
      display.print(F("[Joy/Click] Next"));
    return;
  }

  int8_t dx = 0, dy = 0;
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vrx < 1000)
      dx = -1;
    else if (vrx > 3000)
      dx = 1;
    if (vry < 1000)
      dy = -1;
    else if (vry > 3000)
      dy = 1;

    if (dx != 0 || dy != 0) {
      lastJoyAction = millis();
      if (fkSelectedIdx == -1) {
        fkCursorX = constrain(fkCursorX + dx, 0, GRID_SIZE - 1);
        fkCursorY = constrain(fkCursorY + dy, 0, GRID_SIZE - 1);
      } else {
        FKBlock &b = fkBlocks[fkSelectedIdx];
        int8_t oldX = b.x;
        int8_t oldY = b.y;

        if (b.w > b.h) {
          if (dx != 0) {
            int8_t nx = b.x + dx;
            // Dynamically check win conditions based on the width of the key
            // piece
            if (b.isKey && nx == (GRID_SIZE - b.w)) {
              b.x = nx;
              fkMoves++;
              fkLevelComplete = true;
              lastJoyAction = millis();
              return;
            }
            if (nx >= 0 && nx + b.w <= GRID_SIZE &&
                canPlaceBlock(nx, b.y, b.w, b.h, fkSelectedIdx)) {
              b.x = nx;
              fkCursorX = b.x;
            }
          }
        } else {
          if (dy != 0) {
            int8_t ny = b.y + dy;
            if (ny >= 0 && ny + b.h <= GRID_SIZE &&
                canPlaceBlock(b.x, ny, b.w, b.h, fkSelectedIdx)) {
              b.y = ny;
              fkCursorY = b.y;
            }
          }
        }
        if (b.x != oldX || b.y != oldY)
          fkBlockMovedInSession = true;
      }
    }
  }

  if (clicked) {
    if (fkSelectedIdx == -1) {
      for (int i = 0; i < fkBlockCount; i++) {
        FKBlock &b = fkBlocks[i];
        if (fkCursorX >= b.x && fkCursorX < b.x + b.w && fkCursorY >= b.y &&
            fkCursorY < b.y + b.h) {
          fkSelectedIdx = i;
          fkBlockMovedInSession = false;
          break;
        }
      }
    } else {
      if (fkBlockMovedInSession)
        fkMoves++;
      fkSelectedIdx = -1;
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(66, 4);
  display.print(F("FREE KEY"));
  display.drawFastHLine(66, 13, 58, SSD1306_WHITE);
  display.setCursor(66, 17);
  display.print(F("Lvl: "));
  display.print(fkLevel);
  display.setCursor(66, 29);
  display.print(F("Step:"));
  display.print(fkMoves);
  display.setCursor(66, 45);
  display.print(fkSelectedIdx >= 0 ? F("[MOVING]") : F("[SELECT]"));

  int startX = 4, startY = 5;
  int bSize = GRID_SIZE * CELL_SIZE;

  display.drawRect(startX - 1, startY - 1, bSize + 2, bSize + 2, SSD1306_WHITE);
  display.drawFastVLine(startX + bSize, startY + 2 * CELL_SIZE, CELL_SIZE,
                        SSD1306_BLACK);

  for (int i = 0; i < fkBlockCount; i++) {
    FKBlock &b = fkBlocks[i];
    int bx = startX + b.x * CELL_SIZE + 1;
    int by = startY + b.y * CELL_SIZE + 1;
    int bw = b.w * CELL_SIZE - 1;
    int bh = b.h * CELL_SIZE - 1;

    if (bx >= startX + bSize)
      continue;

    if (b.isKey) {
      display.fillRect(bx, by, bw, bh, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);

      if (b.w >= 3) {
        // Standard GFX font char width is 5px + 1px spacing = 6px. 3 chars
        // ("KEY") = 17px wide.
        int16_t xOffset = (bw - 17) / 2;
        display.setCursor(bx + xOffset, by + 1);
        display.print(F("KEY"));
      } else {
        // Standard single char "K" is 5px wide.
        int16_t xOffset = (bw - 5) / 2;
        display.setCursor(bx + xOffset, by + 1);
        display.print(F("K"));
      }
    } else {
      if (i == fkSelectedIdx) {
        display.fillRect(bx, by, bw, bh, SSD1306_WHITE);
        display.drawRect(bx + 1, by + 1, bw - 2, bh - 2, SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
        display.drawRect(bx, by, bw, bh, SSD1306_WHITE);
        if (b.w > b.h)
          display.drawFastHLine(bx + 2, by + bh / 2, bw - 4, SSD1306_WHITE);
        else
          display.drawFastVLine(bx + bw / 2, by + 2, bh - 4, SSD1306_WHITE);
      }
    }
  }

  display.setTextColor(SSD1306_WHITE);
  if (fkSelectedIdx == -1) {
    int cx = startX + fkCursorX * CELL_SIZE;
    int cy = startY + fkCursorY * CELL_SIZE;
    display.drawRect(cx, cy, CELL_SIZE, CELL_SIZE, SSD1306_WHITE);
  }
}
