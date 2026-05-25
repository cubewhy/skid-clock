#include "../Config.h"
#include "../Games.h"

#define BOARD_COLS 10
#define BOARD_ROWS 20

// 5x3 像素非对称大视窗网格配置
#define BLOCK_W 5
#define BLOCK_H 3

static const int8_t TETRIS_SHAPES[7][4][4][2] = {
    {{{0, 1}, {1, 1}, {2, 1}, {3, 1}},
     {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
     {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
     {{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
    {{{1, 1}, {2, 1}, {1, 2}, {2, 2}},
     {{1, 1}, {2, 1}, {1, 2}, {2, 2}},
     {{1, 1}, {2, 1}, {1, 2}, {2, 2}},
     {{1, 1}, {2, 1}, {1, 2}, {2, 2}}},
    {{{1, 1}, {0, 2}, {1, 2}, {2, 2}},
     {{1, 1}, {1, 2}, {2, 2}, {1, 3}},
     {{0, 2}, {1, 2}, {2, 2}, {1, 3}},
     {{1, 1}, {0, 2}, {1, 2}, {1, 3}}},
    {{{2, 1}, {0, 2}, {1, 2}, {2, 2}},
     {{1, 1}, {1, 2}, {1, 3}, {2, 3}},
     {{0, 2}, {1, 2}, {2, 2}, {0, 3}},
     {{0, 1}, {1, 1}, {1, 2}, {1, 3}}},
    {{{0, 1}, {0, 2}, {1, 2}, {2, 2}},
     {{1, 1}, {2, 1}, {1, 2}, {1, 3}},
     {{0, 2}, {1, 2}, {2, 2}, {2, 3}},
     {{1, 1}, {1, 2}, {0, 3}, {1, 3}}},
    {{{1, 1}, {2, 1}, {0, 2}, {1, 2}},
     {{1, 1}, {1, 2}, {2, 2}, {2, 3}},
     {{1, 2}, {2, 2}, {0, 3}, {1, 3}},
     {{0, 1}, {0, 2}, {1, 2}, {1, 3}}},
    {{{0, 1}, {1, 1}, {1, 2}, {2, 2}},
     {{2, 1}, {1, 2}, {2, 2}, {1, 3}},
     {{0, 2}, {1, 2}, {1, 3}, {2, 3}},
     {{1, 1}, {0, 2}, {1, 2}, {0, 3}}}};

static uint8_t tetrisBoard[BOARD_ROWS][BOARD_COLS];

static int8_t curPieceType = 0;
static int8_t curRotation = 0;
static int8_t curPieceX = 3;
static int8_t curPieceY = -1;
static int8_t nextPieceType = 0;

static int tetrisScore = 0;
static int tetrisLines = 0;
static int tetrisLevel = 1;
static bool tetrisGameOver = false;

static unsigned long lastDropTime = 0;

static bool isPieceValid(int8_t px, int8_t py, int8_t pr) {
  for (int i = 0; i < 4; i++) {
    int8_t tx = px + TETRIS_SHAPES[curPieceType][pr][i][0];
    int8_t ty = py + TETRIS_SHAPES[curPieceType][pr][i][1];

    if (tx < 0 || tx >= BOARD_COLS || ty >= BOARD_ROWS)
      return false;
    if (ty >= 0 && tetrisBoard[ty][tx] != 0)
      return false;
  }
  return true;
}

static void spawnNextPiece() {
  curPieceType = nextPieceType;
  curRotation = 0;
  curPieceX = 3;
  curPieceY = -1;
  nextPieceType = random(0, 7);

  if (!isPieceValid(curPieceX, curPieceY, curRotation)) {
    tetrisGameOver = true;
  }
}

void initTetrisGame() {
  memset(tetrisBoard, 0, sizeof(tetrisBoard));
  tetrisScore = 0;
  tetrisLines = 0;
  tetrisLevel = 1;
  tetrisGameOver = false;

  curPieceType = random(0, 7);
  nextPieceType = random(0, 7);
  spawnNextPiece();

  lastDropTime = millis();
}

static void lockAndClearLines() {
  for (int i = 0; i < 4; i++) {
    int8_t tx = curPieceX + TETRIS_SHAPES[curPieceType][curRotation][i][0];
    int8_t ty = curPieceY + TETRIS_SHAPES[curPieceType][curRotation][i][1];
    if (ty >= 0 && ty < BOARD_ROWS) {
      tetrisBoard[ty][tx] = 1;
    }
  }

  int clearedInThisTurn = 0;
  for (int r = BOARD_ROWS - 1; r >= 0; r--) {
    bool isRowFull = true;
    for (int c = 0; c < BOARD_COLS; c++) {
      if (tetrisBoard[r][c] == 0) {
        isRowFull = false;
        break;
      }
    }

    if (isRowFull) {
      clearedInThisTurn++;
      for (int moveR = r; moveR > 0; moveR--) {
        for (int c = 0; c < BOARD_COLS; c++) {
          tetrisBoard[moveR][c] = tetrisBoard[moveR - 1][c];
        }
      }
      for (int c = 0; c < BOARD_COLS; c++)
        tetrisBoard[0][c] = 0;
      r++;
    }
  }

  if (clearedInThisTurn == 1)
    tetrisScore += 100 * tetrisLevel;
  else if (clearedInThisTurn == 2)
    tetrisScore += 300 * tetrisLevel;
  else if (clearedInThisTurn == 3)
    tetrisScore += 600 * tetrisLevel;
  else if (clearedInThisTurn == 4)
    tetrisScore += 1000 * tetrisLevel;

  tetrisLines += clearedInThisTurn;
  tetrisLevel = (tetrisLines / 10) + 1;

  spawnNextPiece();
}

void handleTetrisMode(int vry, int vrx, bool clicked) {
  if (tetrisGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(15, 12);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(15, 38);
    display.print(F("Score: "));
    display.println(tetrisScore);
    display.setCursor(15, 50);
    display.println(F("[Click] to Exit"));
    return;
  }

  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  static bool hardDropLatched = false;

  // 1. 摇杆方向性滤镜
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vrx < 1000) {
      if (isPieceValid(curPieceX - 1, curPieceY, curRotation)) {
        curPieceX--;
        lastJoyAction = millis();
      }
    } else if (vrx > 3000) {
      if (isPieceValid(curPieceX + 1, curPieceY, curRotation)) {
        curPieceX++;
        lastJoyAction = millis();
      }
    }

    if (vry < 1000) {
      int8_t nextR = (curRotation + 1) % 4;
      if (isPieceValid(curPieceX, curPieceY, nextR)) {
        curRotation = nextR;
        lastJoyAction = millis();
      }
    }
  }

  if (vry > 4000) {
    if (!hardDropLatched) {
      while (isPieceValid(curPieceX, curPieceY + 1, curRotation)) {
        curPieceY++;
      }
      lockAndClearLines();
      hardDropLatched = true;
      lastDropTime = millis();
    }
  } else if (vry < 2000) {
    // 只有当摇杆推回安全中心区域，才释放下落锁
    hardDropLatched = false;
  }

  // 3. 自然下落重力控制器
  unsigned long currentGravityInterval =
      constrain(850 - (tetrisLevel * 80), 100, 900);
  if (millis() - lastDropTime > currentGravityInterval) {
    lastDropTime = millis();
    if (isPieceValid(curPieceX, curPieceY + 1, curRotation)) {
      curPieceY++;
    } else {
      lockAndClearLines();
    }
  }

  // === 4. UI 视窗渲染层 ===
  display.clearDisplay();

  int startX = 4, startY = 2;
  display.drawRect(startX - 1, startY - 1, BOARD_COLS * BLOCK_W + 2,
                   BOARD_ROWS * BLOCK_H + 2, SSD1306_WHITE);

  // 渲染大盘面上已经固化的死块
  for (int r = 0; r < BOARD_ROWS; r++) {
    for (int c = 0; c < BOARD_COLS; c++) {
      if (tetrisBoard[r][c] != 0) {
        display.fillRect(startX + c * BLOCK_W, startY + r * BLOCK_H,
                         BLOCK_W - 1, BLOCK_H - 1, SSD1306_WHITE);
      }
    }
  }

  // 计算落点阴影
  int8_t ghostY = curPieceY;
  while (isPieceValid(curPieceX, ghostY + 1, curRotation)) {
    ghostY++;
  }

  // 分布的“全连通像素点阵烟雾”绘制空心辅助线 这种交错编织纹理能够在 monochrome
  // 单色屏上完美表现出“半透明”和“虚线框”质感
  for (int i = 0; i < 4; i++) {
    int8_t tx = curPieceX + TETRIS_SHAPES[curPieceType][curRotation][i][0];
    int8_t ty = ghostY + TETRIS_SHAPES[curPieceType][curRotation][i][1];
    if (ty >= 0) {
      int bx = startX + tx * BLOCK_W;
      int by = startY + ty * BLOCK_H;
      // 编织 4 点边界空心线框：仅点亮四个绝对顶点，中轴留空
      display.drawPixel(bx, by, SSD1306_WHITE);
      display.drawPixel(bx + BLOCK_W - 2, by, SSD1306_WHITE);
      display.drawPixel(bx, by + BLOCK_H - 2, SSD1306_WHITE);
      display.drawPixel(bx + BLOCK_W - 2, by + BLOCK_H - 2, SSD1306_WHITE);
    }
  }

  // 渲染当前下落的活动实心块（会完美覆盖覆盖重叠处的网格点）
  for (int i = 0; i < 4; i++) {
    int8_t tx = curPieceX + TETRIS_SHAPES[curPieceType][curRotation][i][0];
    int8_t ty = curPieceY + TETRIS_SHAPES[curPieceType][curRotation][i][1];
    if (ty >= 0) {
      display.fillRect(startX + tx * BLOCK_W, startY + ty * BLOCK_H,
                       BLOCK_W - 1, BLOCK_H - 1, SSD1306_WHITE);
    }
  }

  // 右侧留白控制面板（X=58处精细排版）
  int panelX = 58;
  display.setTextSize(1);
  display.setCursor(panelX + 4, 2);
  display.print(F("TETRIS"));
  display.drawFastHLine(panelX, 11, 70, SSD1306_WHITE);

  display.setCursor(panelX + 2, 15);
  display.print(F("Sco:"));
  display.print(tetrisScore);
  display.setCursor(panelX + 2, 25);
  display.print(F("Lin:"));
  display.print(tetrisLines);
  display.setCursor(panelX + 2, 35);
  display.print(F("Lvl:"));
  display.print(tetrisLevel);

  // “NEXT” 下一个方块微缩视窗预览
  display.setCursor(panelX + 2, 48);
  display.print(F("NXT:"));
  int nextXAnchor = panelX + 30, nextYAnchor = 48;
  for (int i = 0; i < 4; i++) {
    int8_t nx = TETRIS_SHAPES[nextPieceType][0][i][0];
    int8_t ny = TETRIS_SHAPES[nextPieceType][0][i][1];
    display.fillRect(nextXAnchor + nx * 4, nextYAnchor + ny * 3, 3, 2,
                     SSD1306_WHITE);
  }
}
