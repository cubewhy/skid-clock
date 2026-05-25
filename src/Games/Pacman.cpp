#include "../Config.h"
#include "../Games.h"

#define MAP_COLS 21
#define MAP_ROWS 9
#define CELL_SIZE 5 // 5x5 像素

// 地图图块枚举升级
#define TILE_WALL 0
#define TILE_DOT 1
#define TILE_EMPTY 2
#define TILE_BIG_DOT 3

struct PacEntity {
  int8_t x, y;
  int8_t dx, dy;
};

static uint8_t pacMaze[MAP_ROWS][MAP_COLS];
static PacEntity pacman;
static PacEntity ghosts[2];
static int pacScore = 0;
static int pacLives = 3;
static int totalDots = 0;
static bool pacGameOver = false;
static bool pacGameWin = false;
static unsigned long lastPacUpdate = 0;
static const int pacSpeed = 250;

static bool pacPowerMode = false;
static uint16_t pacPowerTimer = 0; // 以游戏帧步长计数（1帧=250ms，32帧=8秒）

// DFS 迷宫雕刻算法
static void carvePacMaze(int r, int c) {
  int dirs[4][2] = {{0, 2}, {0, -2}, {2, 0}, {-2, 0}};
  for (int i = 0; i < 4; i++) {
    int rIdx = random(i, 4);
    int ty = dirs[i][0];
    int tx = dirs[i][1];
    dirs[i][0] = dirs[rIdx][0];
    dirs[i][1] = dirs[rIdx][1];
    dirs[rIdx][0] = ty;
    dirs[rIdx][1] = tx;
  }
  for (int i = 0; i < 4; i++) {
    int nextR = r + dirs[i][0];
    int nextC = c + dirs[i][1];
    if (nextR > 0 && nextR < MAP_ROWS - 1 && nextC > 0 &&
        nextC < MAP_COLS - 1) {
      if (pacMaze[nextR][nextC] == TILE_WALL) {
        pacMaze[nextR][nextC] = TILE_DOT;
        pacMaze[r + dirs[i][0] / 2][c + dirs[i][1] / 2] = TILE_DOT;
        carvePacMaze(nextR, nextC);
      }
    }
  }
}

// 随机地图生成器升级
static void generatePacLevel() {
  totalDots = 0;
  for (int r = 0; r < MAP_ROWS; r++) {
    for (int c = 0; c < MAP_COLS; c++)
      pacMaze[r][c] = TILE_WALL;
  }

  pacMaze[1][1] = TILE_DOT;
  carvePacMaze(1, 1);

  // 破墙扩展
  for (int r = 1; r < MAP_ROWS - 1; r++) {
    for (int c = 1; c < MAP_COLS - 1; c++) {
      if (pacMaze[r][c] == TILE_WALL && random(0, 100) < 22) {
        pacMaze[r][c] = TILE_DOT;
      }
    }
  }

  // 清理实体出生区
  pacMaze[1][1] = TILE_EMPTY;
  pacMaze[MAP_ROWS - 2][MAP_COLS - 2] = TILE_EMPTY;
  pacMaze[MAP_ROWS - 2][1] = TILE_EMPTY;

  // 👈 在固定对称通路上埋藏 4 颗大豆子
  int8_t bigDotCoords[4][2] = {{1, 3},
                               {1, MAP_COLS - 4},
                               {MAP_ROWS - 2, 3},
                               {MAP_ROWS - 2, MAP_COLS - 4}};
  for (int i = 0; i < 4; i++) {
    int8_t by = bigDotCoords[i][0];
    int8_t bx = bigDotCoords[i][1];
    pacMaze[by][bx] = TILE_BIG_DOT;
  }

  // 重新扫描计算通关所需的总豆子基数
  for (int r = 0; r < MAP_ROWS; r++) {
    for (int c = 0; c < MAP_COLS; c++) {
      if (pacMaze[r][c] == TILE_DOT || pacMaze[r][c] == TILE_BIG_DOT)
        totalDots++;
    }
  }
}

void initPacmanGame() {
  pacScore = 0;
  pacLives = 3;
  pacGameOver = false;
  pacGameWin = false;
  pacPowerMode = false;
  pacPowerTimer = 0;

  generatePacLevel();

  pacman = {1, 1, 1, 0};
  ghosts[0] = {MAP_COLS - 2, MAP_ROWS - 2, -1, 0}; // 红鬼
  ghosts[1] = {1, MAP_ROWS - 2, 0, -1};            // 粉鬼
  lastPacUpdate = millis();
}

void handlePacmanMode(int vry, int vrx, bool clicked) {
  if (pacGameOver || pacGameWin) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(12, 12);
    display.println(pacGameWin ? F("YOU WIN!") : F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(12, 38);
    display.print(F("Score: "));
    display.println(pacScore);
    display.setCursor(12, 50);
    display.println(F("[Click] Return Menu"));
    return;
  }

  // 摇杆控制锁存
  if (vrx < 1000) {
    pacman.dx = -1;
    pacman.dy = 0;
  } else if (vrx > 3000) {
    pacman.dx = 1;
    pacman.dy = 0;
  } else if (vry < 1000) {
    pacman.dx = 0;
    pacman.dy = -1;
  } else if (vry > 3000) {
    pacman.dx = 0;
    pacman.dy = 1;
  }

  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  // 250ms 帧同步更新
  if (millis() - lastPacUpdate > pacSpeed) {
    lastPacUpdate = millis();

    // 👈 能量药丸时效倒计时
    if (pacPowerMode) {
      if (pacPowerTimer > 0)
        pacPowerTimer--;
      else
        pacPowerMode = false;
    }

    // 1. 移动吃豆人
    int8_t nextX = pacman.x + pacman.dx;
    int8_t nextY = pacman.y + pacman.dy;

    if (nextX >= 0 && nextX < MAP_COLS && nextY >= 0 && nextY < MAP_ROWS &&
        pacMaze[nextY][nextX] != TILE_WALL) {
      pacman.x = nextX;
      pacman.y = nextY;

      if (pacMaze[pacman.y][pacman.x] == TILE_DOT) {
        // 吃普通豆
        pacMaze[pacman.y][pacman.x] = TILE_EMPTY;
        pacScore += 10;
        totalDots--;
      } else if (pacMaze[pacman.y][pacman.x] == TILE_BIG_DOT) {
        pacMaze[pacman.y][pacman.x] = TILE_EMPTY;
        pacScore += 50;
        totalDots--;
        pacPowerMode = true;
        pacPowerTimer = 32; // 32帧 * 250ms = 8秒无敌反噬时间
      }

      if (totalDots <= 0) {
        pacGameWin = true;
        return;
      }
    }

    // 2. 移动鬼魂
    for (int i = 0; i < 2; i++) {
      PacEntity &g = ghosts[i];
      int8_t gNextDirs[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
      int8_t bestDx = g.dx, bestDy = g.dy;

      if (i == 0) {
        // 【红鬼 AI 分支升级】
        int targetDist =
            pacPowerMode
                ? -1
                : 999; // 恐惧状态下求最大曼哈顿距离（逃跑），普通状态求最小（追击）

        for (int d = 0; d < 4; d++) {
          int8_t tX = g.x + gNextDirs[d][0];
          int8_t tY = g.y + gNextDirs[d][1];
          if (tX >= 0 && tX < MAP_COLS && tY >= 0 && tY < MAP_ROWS &&
              pacMaze[tY][tX] != TILE_WALL) {
            if (gNextDirs[d][0] == -g.dx && gNextDirs[d][1] == -g.dy)
              continue; // 禁止走回头路

            int dist = abs(tX - pacman.x) + abs(tY - pacman.y);
            if (pacPowerMode) {
              // 逃跑模式：选择离吃豆人最远的方向
              if (dist > targetDist) {
                targetDist = dist;
                bestDx = gNextDirs[d][0];
                bestDy = gNextDirs[d][1];
              }
            } else {
              // 追击模式：选择离吃豆人最近的方向
              if (dist < targetDist) {
                targetDist = dist;
                bestDx = gNextDirs[d][0];
                bestDy = gNextDirs[d][1];
              }
            }
          }
        }
      } else {
        // 【粉鬼 AI 分支升级】
        // 恐惧模式下全额随机乱走乱晃，普通模式 35% 几率随缘变向
        int8_t validDirs[4][2];
        int validCount = 0;
        for (int d = 0; d < 4; d++) {
          int8_t tX = g.x + gNextDirs[d][0];
          int8_t tY = g.y + gNextDirs[d][1];
          if (tX >= 0 && tX < MAP_COLS && tY >= 0 && tY < MAP_ROWS &&
              pacMaze[tY][tX] != TILE_WALL) {
            if (gNextDirs[d][0] == -g.dx && gNextDirs[d][1] == -g.dy)
              continue;
            validDirs[validCount][0] = gNextDirs[d][0];
            validDirs[validCount][1] = gNextDirs[d][1];
            validCount++;
          }
        }
        if (validCount > 0 &&
            (g.dx == 0 && g.dy == 0 || pacPowerMode || random(0, 100) < 35)) {
          int rD = random(0, validCount);
          bestDx = validDirs[rD][0];
          bestDy = validDirs[rD][1];
        }
      }

      if (pacMaze[g.y + bestDy][g.x + bestDx] != TILE_WALL) {
        g.dx = bestDx;
        g.dy = bestDy;
        g.x += g.dx;
        g.y += g.dy;
      } else {
        g.dx = -g.dx;
        g.dy = -g.dy;
      }

      // 3. 接触判定（升级支持双向吞噬判定）
      if (g.x == pacman.x && g.y == pacman.y) {
        if (pacPowerMode) {
          pacScore += 200;
          if (i == 0) {
            g.x = MAP_COLS - 2;
            g.y = MAP_ROWS - 2;
            g.dx = -1;
            g.dy = 0;
          } else {
            g.x = 1;
            g.y = MAP_ROWS - 2;
            g.dx = 0;
            g.dy = -1;
          }
        } else {
          // 普通丧命逻辑
          pacLives--;
          if (pacLives <= 0) {
            pacGameOver = true;
            return;
          }
          pacman.x = 1;
          pacman.y = 1;
          pacman.dx = 1;
          pacman.dy = 0;
          ghosts[0] = {MAP_COLS - 2, MAP_ROWS - 2, -1, 0};
          ghosts[1] = {1, MAP_ROWS - 2, 0, -1};
          break;
        }
      }
    }
  }

  // === 屏幕图形渲染层 ===
  display.clearDisplay();

  // 顶层数据展示栏（加入无敌状态闪烁标志 [P]）
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("SCORE:"));
  display.print(pacScore);

  if (pacPowerMode && (pacPowerTimer > 8 || (millis() / 150) % 2 == 0)) {
    display.setCursor(64, 0); // 在中央闪烁提示魔王状态
    display.print(F("[POWER]"));
  }

  display.setCursor(105, 0);
  display.print(F("V:"));
  display.print(pacLives);
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  int startX = 11, startY = 14;

  for (int r = 0; r < MAP_ROWS; r++) {
    for (int c = 0; c < MAP_COLS; c++) {
      int bx = startX + c * CELL_SIZE;
      int by = startY + r * CELL_SIZE;

      if (pacMaze[r][c] == TILE_WALL) {
        display.fillRect(bx + 1, by + 1, CELL_SIZE - 1, CELL_SIZE - 1,
                         SSD1306_WHITE);
      } else if (pacMaze[r][c] == TILE_DOT) {
        display.drawPixel(bx + CELL_SIZE / 2, by + CELL_SIZE / 2,
                          SSD1306_WHITE);
      } else if (pacMaze[r][c] == TILE_BIG_DOT) {
        display.fillRect(bx + 1, by + 1, 3, 3, SSD1306_WHITE);
      }
    }
  }

  // 绘制吃豆人
  int px = startX + pacman.x * CELL_SIZE + CELL_SIZE / 2;
  int py = startY + pacman.y * CELL_SIZE + CELL_SIZE / 2;
  display.drawCircle(px, py, 2, SSD1306_WHITE);
  if (pacman.dx == 1)
    display.drawPixel(px + 2, py, SSD1306_BLACK);
  else if (pacman.dx == -1)
    display.drawPixel(px - 2, py, SSD1306_BLACK);
  else if (pacman.dy == 1)
    display.drawPixel(px, py + 2, SSD1306_BLACK);
  else if (pacman.dy == -1)
    display.drawPixel(px, py - 2, SSD1306_BLACK);

  // 绘制鬼魂们
  for (int i = 0; i < 2; i++) {
    int gx = startX + ghosts[i].x * CELL_SIZE;
    int gy = startY + ghosts[i].y * CELL_SIZE;

    if (pacPowerMode) {
      // 恐惧状态：渲染为特殊的闪烁交叉“X”字形，提示玩家可以安全吞噬
      if (pacPowerTimer > 8 || (millis() / 200) % 2 == 0) {
        display.drawLine(gx + 1, gy + 1, gx + 3, gy + 3, SSD1306_WHITE);
        display.drawLine(gx + 1, gy + 3, gx + 3, gy + 1, SSD1306_WHITE);
      } else {
        // 时效即将结束时，闪烁变为空心线框
        display.drawRect(gx + 1, gy, CELL_SIZE - 1, CELL_SIZE - 1,
                         SSD1306_WHITE);
      }
    } else {
      // 正常状态
      if (i == 0)
        display.drawRect(gx + 1, gy, CELL_SIZE - 1, CELL_SIZE - 1,
                         SSD1306_WHITE);
      else {
        display.drawRect(gx + 1, gy, CELL_SIZE - 1, CELL_SIZE - 1,
                         SSD1306_WHITE);
        display.fillRect(gx + 2, gy + 1, CELL_SIZE - 3, CELL_SIZE - 3,
                         SSD1306_WHITE);
      }
    }
  }
}
