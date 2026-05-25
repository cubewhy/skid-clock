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

  // 基础难度配置
  int baseTargetBlocks = constrain(8 + (fkLevel / 2), 9, 13);
  int reqBlockers = (fkLevel >= 3) ? 2 : 1;

  bool isMapValid = false;
  int safetyAttempts = 0;

  while (!isMapValid) {
    safetyAttempts++;
    fkBlockCount = 0;

    // 1. 动态自适应降额：如果尝试了 80 次都无法生成超硬核关卡，
    // 说明当前随机种子下的空间太挤。我们开始动态调小目标方块数，确保算法绝对收敛。
    int currentTargetBlocks = baseTargetBlocks;
    if (safetyAttempts > 80)
      currentTargetBlocks = baseTargetBlocks - 1;
    if (safetyAttempts > 160)
      currentTargetBlocks = baseTargetBlocks - 2;
    if (safetyAttempts > 220) {
      currentTargetBlocks = 8; // 保底方块数
      reqBlockers = 1;         // 放宽铁闸限制
    }

    // 限制边界防止数组溢出
    if (currentTargetBlocks > MAX_FK_BLOCKS)
      currentTargetBlocks = MAX_FK_BLOCKS;

    // 2. 放置目标获胜态的钥匙（完美占领 Row 2 的 Col 3,4,5）
    fkBlocks[0] = {3, 2, 3, 1, true, true};
    fkBlockCount = 1;

    // 3. 动态填充：采用 50/50 的严格横纵比随机放置初始方块
    // 注意：这里没有任何硬编码重叠块，所有方块出生即合法
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

      // 防止原地踏步的浅层震荡
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

      // 只有坐标真的发生改变，才算一次有效洗牌步数
      if (b.x != oldX || b.y != oldY) {
        effectiveMoves++;
        stallCounter = 0; // 重置空转计数器
        lastBIdx = bIdx;
        lastDir = dir;
      }
    }

    int verticalBlockers = 0;
    int interlockingPairs = 0;
    int8_t kx = fkBlocks[0].x;

    // A. 审查通路铁闸：扫描钥匙右侧，看有多少根纵向块切断了 Row 2
    for (int i = 1; i < fkBlockCount; i++) {
      FKBlock &b = fkBlocks[i];
      if (b.h > b.w && b.x >= kx + 3) { // 处在钥匙右侧的纵向块
        if (2 >= b.y && 2 < b.y + b.h) {
          verticalBlockers++;

          // B. 审查死锁：这根铁闸的上下两端，是否有横向块顶住它的退路？
          for (int j = 1; j < fkBlockCount; j++) {
            FKBlock &h = fkBlocks[j];
            if (h.w > h.h) { // 横向块
              if (h.x < b.x + b.w && h.x + h.w > b.x) {
                if (h.y == b.y + b.h || h.y + h.h == b.y) {
                  interlockingPairs++; // 产生交叉死锁链
                }
              }
            }
          }
        }
      }
    }

    // C. 综合硬核验证断言：
    // - 钥匙必须被成功逼退到最左侧深处（kx <= 1），拉长解密步长
    // - 钥匙前方的纵向铁闸数必须达标
    // - 全图至少要存在 2 组以上的纵横卡死对（Level 3+ 要求 3 组）
    int reqInterlocks = (fkLevel >= 3) ? 3 : 2;

    if (kx <= 1 && verticalBlockers >= reqBlockers &&
        interlockingPairs >= reqInterlocks) {
      isMapValid = true;
    }
  }

  // 准星初始归位聚焦钥匙
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
