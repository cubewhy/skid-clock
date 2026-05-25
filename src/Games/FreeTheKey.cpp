#include "../Config.h"
#include "../Games.h"

#define MAX_FK_BLOCKS 16
#define GRID_SIZE 6
#define CELL_SIZE 9

struct FKBlock {
  int8_t x, y;
  int8_t w, h;
  bool isKey;
  bool active;
};

static FKBlock fkBlocks[MAX_FK_BLOCKS];
static int fkBlockCount = 0;
static int8_t fkSelectedIdx = -1;
static int8_t fkCursorX = 0, fkCursorY = 0;
static bool fkLevelComplete = false;
static int fkLevel = 1;

static int fkMoves = 0;
// 当前选择会话中方块是否真的发生过位移
static bool fkBlockMovedInSession = false;

static bool fkJoyCenteredAfterWin = false;

// 检查绝对网格单元是否被占用
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

// 检查方块放置合法性
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
  fkMoves = 0;
  fkBlockMovedInSession = false;
  fkJoyCenteredAfterWin = false;

  // 基础难度配置
  int baseTargetBlocks = constrain(8 + (fkLevel / 2), 9, 13);
  int reqBlockers = (fkLevel >= 3) ? 2 : 1;

  bool isMapValid = false;
  int safetyAttempts = 0;

  while (!isMapValid) {
    safetyAttempts++;
    fkBlockCount = 0;

    // 1. 动态自适应降额
    int currentTargetBlocks = baseTargetBlocks;
    if (safetyAttempts > 80)
      currentTargetBlocks = baseTargetBlocks - 1;
    if (safetyAttempts > 160)
      currentTargetBlocks = baseTargetBlocks - 2;
    if (safetyAttempts > 220) {
      currentTargetBlocks = 8;
      reqBlockers = 1;
    }

    if (currentTargetBlocks > MAX_FK_BLOCKS)
      currentTargetBlocks = MAX_FK_BLOCKS;

    // 2. 放置目标获胜态的钥匙
    fkBlocks[0] = {3, 2, 3, 1, true, true};
    fkBlockCount = 1;

    // 3. 动态填充
    for (int i = 0; i < 40; i++) {
      if (fkBlockCount >= currentTargetBlocks)
        break;

      bool makeVertical = (random(0, 100) < 50);
      int8_t bw = 1, bh = 1;
      if (makeVertical) {
        bh = random(2, 4);
        bw = 1;
      } else {
        bw = random(2, 4);
        bh = 1;
      }

      int8_t rx = random(0, GRID_SIZE - bw + 1);
      int8_t ry = random(0, GRID_SIZE - bh + 1);

      if (canPlaceBlock(rx, ry, bw, bh, -1)) {
        fkBlocks[fkBlockCount] = {rx, ry, bw, bh, false, true};
        fkBlockCount++;
      }
    }

    int shuffleSteps = 120 + fkLevel * 30;
    int effectiveMoves = 0;
    int8_t lastBIdx = -1;
    int8_t lastDir = 0;
    int stallCounter = 0;

    while (effectiveMoves < shuffleSteps && stallCounter < 400) {
      stallCounter++;
      int bIdx = random(0, fkBlockCount);
      int dir = (random(0, 2) == 0) ? -1 : 1;

      if (bIdx == lastBIdx && dir == -lastDir)
        dir = -dir;

      FKBlock &b = fkBlocks[bIdx];
      int8_t oldX = b.x;
      int8_t oldY = b.y;

      if (b.w > b.h) {
        int8_t nx = b.x + dir;
        if (nx >= 0 && nx + b.w <= GRID_SIZE &&
            canPlaceBlock(nx, b.y, b.w, b.h, bIdx))
          b.x = nx;
      } else {
        int8_t ny = b.y + dir;
        if (ny >= 0 && ny + b.h <= GRID_SIZE &&
            canPlaceBlock(b.x, ny, b.w, b.h, bIdx))
          b.y = ny;
      }

      if (b.x != oldX || b.y != oldY) {
        effectiveMoves++;
        stallCounter = 0;
        lastBIdx = bIdx;
        lastDir = dir;
      }
    }

    int verticalBlockers = 0;
    int interlockingPairs = 0;
    int8_t kx = fkBlocks[0].x;

    for (int i = 1; i < fkBlockCount; i++) {
      FKBlock &b = fkBlocks[i];
      if (b.h > b.w && b.x >= kx + 3) {
        if (2 >= b.y && 2 < b.y + b.h) {
          verticalBlockers++;

          for (int j = 1; j < fkBlockCount; j++) {
            FKBlock &h = fkBlocks[j];
            if (h.w > h.h) {
              if (h.x < b.x + b.w && h.x + h.w > b.x) {
                if (h.y == b.y + b.h || h.y + h.h == b.y) {
                  interlockingPairs++;
                }
              }
            }
          }
        }
      }
    }

    int reqInterlocks = (fkLevel >= 3) ? 3 : 2;
    if (kx <= 1 && verticalBlockers >= reqBlockers &&
        interlockingPairs >= reqInterlocks) {
      isMapValid = true;
    }
  }

  fkCursorX = fkBlocks[0].x;
  fkCursorY = fkBlocks[0].y;
}

void handleFreeKeyMode(int vry, int vrx, bool clicked) {
  if (fkLevelComplete) {
    // 实时读取判断摇杆当前是否处于物理中心安全区 (1000 ~ 3000)
    bool isCentered =
        (vrx >= 1000 && vrx <= 3000 && vry >= 1000 && vry <= 3000);

    if (isCentered) {
      fkJoyCenteredAfterWin = true; // 只要松开过一次，立刻永久解锁当前结算状态
    }

    // 只有在摇杆释放解锁后，再次拨动摇杆（!isCentered）或点击中键，才会切入下一关
    if (fkJoyCenteredAfterWin) {
      bool proceedNext =
          clicked || (!isCentered && (millis() - lastJoyAction > JOY_DELAY));
      if (proceedNext) {
        fkLevel++;
        initFreeKeyGame();
        lastJoyAction = millis();
        return;
      }
    }

    // 动态 UI 显示
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(12, 12);
    display.println(F("ESCAPE OK"));
    display.setTextSize(1);
    display.setCursor(12, 40);

    if (!fkJoyCenteredAfterWin) {
      // 摇杆还按着呢，死锁并提示释放
      display.print(F("[Release Joystick]"));
    } else {
      // 已经释放过，进入标准就绪态
      display.print(F("[Joy/Click] Next Lvl"));
    }
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
            if (b.isKey && nx == 4) { // 钥匙突围成功
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

        if (b.x != oldX || b.y != oldY) {
          fkBlockMovedInSession = true;
        }
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
      if (fkBlockMovedInSession) {
        fkMoves++;
      }
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

  display.drawFastHLine(startX - 1, startY - 1, bSize + 2, SSD1306_WHITE);
  display.drawFastHLine(startX - 1, startY + bSize, bSize + 2, SSD1306_WHITE);
  display.drawFastVLine(startX - 1, startY - 1, bSize + 2, SSD1306_WHITE);

  display.drawFastVLine(startX + bSize, startY - 1, 2 * CELL_SIZE + 1,
                        SSD1306_WHITE);
  display.drawFastVLine(startX + bSize, startY + 3 * CELL_SIZE,
                        3 * CELL_SIZE + 1, SSD1306_WHITE);

  display.setCursor(startX + bSize + 2, startY + 2 * CELL_SIZE + 1);
  display.print(F(">"));

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
      display.setTextSize(1);
      display.setCursor(bx + 4, by + 1);
      display.print(F("KEY"));
    } else {
      if (i == fkSelectedIdx) {
        display.fillRect(bx, by, bw, bh, SSD1306_WHITE);
        display.drawRect(bx + 1, by + 1, bw - 2, bh - 2, SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
        display.drawRect(bx, by, bw, bh, SSD1306_WHITE);
        if (b.w > b.h) {
          display.drawFastHLine(bx + 2, by + bh / 2, bw - 4, SSD1306_WHITE);
        } else {
          display.drawFastVLine(bx + bw / 2, by + 2, bh - 4, SSD1306_WHITE);
        }
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
