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
static bool fkBlockMovedInSession =
    false; // 当前选择会话中方块是否真的发生过位移

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

  // A. 空间控制：关卡越高，方块越多，留空越少。满图意味着每动一步都需要精妙计算
  int maxBlocks = constrain(7 + (fkLevel / 2), 8, MAX_FK_BLOCKS);

  // B. 障碍深度：钥匙前方的路径上，必须被多少个独立的垂直长条“截断”
  int requiredPathBlockers = 1;
  if (fkLevel >= 3)
    requiredPathBlockers = 2; // 第3关起，双重铁闸
  if (fkLevel >= 6)
    requiredPathBlockers = 3; // 第6关起，三重铁闸（极难）

  // C. 冲刷烈度：大幅提升高级关卡的洗牌深度
  int shuffleSteps = 100 + fkLevel * 50;

  bool isMapValid = false;
  int safetyAttempts = 0;

  // 验证循环：不生成出高难度合格的地图决不罢休
  while (!isMapValid && safetyAttempts < 300) {
    safetyAttempts++;
    fkBlockCount = 0;

    // 1. 放置目标获胜态的钥匙（1x3横向，位于最右侧出口边缘占满 X=3,4,5）
    fkBlocks[0] = {3, 2, 3, 1, true, true};
    fkBlockCount = 1;

    // 2. 在出口左侧的上/下方战略性预埋 vertical 种子方块
    // 当钥匙在洗牌中后退时，这些种子方块会如同机关一样“扎入”Row 2 形成锁死结构
    fkBlocks[fkBlockCount++] = {3, 0, 1, 2, false, true}; // 挂在列3上方的1x2
    fkBlocks[fkBlockCount++] = {4, 3, 1, 3, false, true}; // 站在列4下方的1x3
    if (maxBlocks > 9) {
      fkBlocks[fkBlockCount++] = {5, 0, 1, 2, false, true}; // 挂在列5上方的1x2
    }

    // 3. 动态填充干扰块：关卡越高，生成垂直块（纵向移动）的权重越恐怖
    int verticalWeight = constrain(35 + fkLevel * 6, 35, 80);

    for (int i = 0; i < 40; i++) {
      if (fkBlockCount >= maxBlocks)
        break;

      bool makeVertical = (random(0, 100) < verticalWeight);
      int8_t bw = 1, bh = 1;

      if (makeVertical) {
        bh = random(2, 4); // 随机生成 1x2 或 1x3 垂直长条
        bw = 1;
      } else {
        bw = random(2, 4); // 随机生成 2x1 或 3x1 水平长条
        bh = 1;
      }

      int8_t rx = random(0, GRID_SIZE - bw + 1);
      int8_t ry = random(0, GRID_SIZE - bh + 1);

      if (canPlaceBlock(rx, ry, bw, bh, -1)) {
        fkBlocks[fkBlockCount] = {rx, ry, bw, bh, false, true};
        fkBlockCount++;
      }
    }

    // 4. 高强度反向解谜冲刷
    for (int s = 0; s < shuffleSteps; s++) {
      int bIdx = random(0, fkBlockCount);
      int dir = (random(0, 2) == 0) ? -1 : 1;
      FKBlock &b = fkBlocks[bIdx];

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
    }

    int pathBlockerCount = 0;
    int8_t keyFrontX = fkBlocks[0].x + fkBlocks[0].w;

    // 从当前洗开后的钥匙前端，一直向右扫描到出口边际
    for (int8_t tx = keyFrontX; tx < GRID_SIZE; tx++) {
      // 检查当前列的 Row 2 是否有垂直方块穿过
      for (int i = 1; i < fkBlockCount; i++) {
        FKBlock &b = fkBlocks[i];
        if (tx >= b.x && tx < b.x + b.w && 2 >= b.y && 2 < b.y + b.h) {
          if (b.h >
              b.w) { // 必须是垂直长条卡在路中央（横向块不算，因为会被钥匙直接推走）
            pathBlockerCount++;
            break;
          }
        }
      }
    }

    // 严苛通过断言：
    // 1. 阻挡钥匙的垂直长闸数量必须达标
    // 2. 钥匙自身必须被逼退到足够靠左的位置（增加了底层解密所需的总步长）
    if (pathBlockerCount >= requiredPathBlockers &&
        fkBlocks[0].x <= (4 - requiredPathBlockers)) {
      isMapValid = true;
    }
  }

  // 极度安全的降级保护：万一在超高难度下随机死循环，保底将钥匙推回最左侧起步
  if (!isMapValid) {
    fkBlocks[0].x = 0;
  }

  // 准星聚焦钥匙
  fkCursorX = fkBlocks[0].x;
  fkCursorY = fkBlocks[0].y;
}

void handleFreeKeyMode(int vry, int vrx, bool clicked) {
  if (fkLevelComplete) {
    if (clicked) {
      fkLevel++;
      initFreeKeyGame();
    }
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(12, 15);
    display.println(F("ESCAPE OK"));
    display.setTextSize(1);
    display.setCursor(12, 42);
    display.print(F("[Click] Next Level"));
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
        // 模式 1：移动准星光标
        fkCursorX = constrain(fkCursorX + dx, 0, GRID_SIZE - 1);
        fkCursorY = constrain(fkCursorY + dy, 0, GRID_SIZE - 1);
      } else {
        // 模式 2：移动锁定的方块
        FKBlock &b = fkBlocks[fkSelectedIdx];
        int8_t oldX = b.x;
        int8_t oldY = b.y;

        if (b.w > b.h) {
          if (dx != 0) {
            int8_t nx = b.x + dx;
            if (b.isKey && nx == 4) { // 钥匙成功突围
              b.x = nx;
              fkMoves++; // 突围的最后一步自动计入
              fkLevelComplete = true;
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

        // 👈 如果位置真的变了，标记这轮选择是有合法位移的
        if (b.x != oldX || b.y != oldY) {
          fkBlockMovedInSession = true;
        }
      }
    }
  }

  if (clicked) {
    if (fkSelectedIdx == -1) {
      // 尝试抓取方块
      for (int i = 0; i < fkBlockCount; i++) {
        FKBlock &b = fkBlocks[i];
        if (fkCursorX >= b.x && fkCursorX < b.x + b.w && fkCursorY >= b.y &&
            fkCursorY < b.y + b.h) {
          fkSelectedIdx = i;
          fkBlockMovedInSession = false; // 👈 抓取时重置移动标记
          break;
        }
      }
    } else {
      // 👈 松开方块：只有当抓取期间对方块做过实际位移，才算一次有效操作
      if (fkBlockMovedInSession) {
        fkMoves++;
      }
      fkSelectedIdx = -1;
    }
  }

  display.clearDisplay();

  // 右侧留白控制面板信息渲染
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(66, 4);
  display.print(F("FREE KEY"));
  display.drawFastHLine(66, 13, 58, SSD1306_WHITE);

  display.setCursor(66, 17);
  display.print(F("Lvl: "));
  display.print(fkLevel);

  // 👈 新增：步数实时显示
  display.setCursor(66, 29);
  display.print(F("Step:"));
  display.print(fkMoves);

  display.setCursor(66, 45);
  display.print(fkSelectedIdx >= 0 ? F("[MOVING]") : F("[SELECT]"));

  // 华容道地图主骨架绘制
  int startX = 4, startY = 5;
  int bSize = GRID_SIZE * CELL_SIZE;

  display.drawFastHLine(startX - 1, startY - 1, bSize + 2, SSD1306_WHITE);
  display.drawFastHLine(startX - 1, startY + bSize, bSize + 2, SSD1306_WHITE);
  display.drawFastVLine(startX - 1, startY - 1, bSize + 2, SSD1306_WHITE);

  // 右边框第 2 行留出开放缺口
  display.drawFastVLine(startX + bSize, startY - 1, 2 * CELL_SIZE + 1,
                        SSD1306_WHITE);
  display.drawFastVLine(startX + bSize, startY + 3 * CELL_SIZE,
                        3 * CELL_SIZE + 1, SSD1306_WHITE);

  display.setCursor(startX + bSize + 2, startY + 2 * CELL_SIZE + 1);
  display.print(F(">"));

  // 渲染所有方块
  for (int i = 0; i < fkBlockCount; i++) {
    FKBlock &b = fkBlocks[i];
    int bx = startX + b.x * CELL_SIZE + 1;
    int by = startY + b.y * CELL_SIZE + 1;
    int bw = b.w * CELL_SIZE - 1;
    int bh = b.h * CELL_SIZE - 1;

    if (bx >= startX + bSize)
      continue;

    if (b.isKey) {
      // 🔑【修改点：钥匙块标志性重构】
      // 采用大反白高亮打底，并在正中心写入反色黑字 "KEY"！辨识度瞬间拉满
      display.fillRect(bx, by, bw, bh, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setTextSize(1);
      // 居中打印简易 Key 文本标示 (1x3 块有 26 像素宽，字体占 17
      // 像素宽，左右留白 4 像素)
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

  // 恢复出厂白色画图环境
  display.setTextColor(SSD1306_WHITE);

  // 渲染准星框
  if (fkSelectedIdx == -1) {
    int cx = startX + fkCursorX * CELL_SIZE;
    int cy = startY + fkCursorY * CELL_SIZE;
    display.drawRect(cx, cy, CELL_SIZE, CELL_SIZE, SSD1306_WHITE);
  }
}
