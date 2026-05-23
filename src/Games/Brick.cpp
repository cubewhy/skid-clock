#include "../Config.h"
#include "../Games.h"

static float brickBallX, brickBallY;
static float brickBallVX, brickBallVY;
static int brickPaddleX;
static const int brickPaddleWidth = 24;
static const int brickPaddleHeight = 3;
static const int brickPaddleY = 58;

#define BRICK_ROWS 3
#define BRICK_COLS 6
static bool brickGrid[BRICK_ROWS][BRICK_COLS];
static const int brickWidth = 18;
static const int brickHeight = 4;
static const int brickGapX = 3;
static const int brickGapY = 3;
static const int brickOffsetX = 2;
static const int brickOffsetY = 13;
static int scoreBrick = 0;
static bool gameOverBrick = false;
static bool gameWinBrick = false;
static unsigned long lastBrickUpdate = 0;

static int currentLevel = 1;
#define MAX_DESIGNED_LEVELS 3 // 固定设计的关卡数量，超过后进入无尽随机模式

void setupLevel() {
  // 1. 重置球的位置到中央
  brickBallX = 64.0f;
  brickBallY = 45.0f;

  // 2. 基础速度随关卡略微提升（每关加快 15%）
  float speedMultiplier = 1.0f + (currentLevel - 1) * 0.15f;
  brickBallVX = 1.3f * speedMultiplier;
  brickBallVY = -1.3f * speedMultiplier;

  lastBrickUpdate = millis();

  // 3. 根据当前关卡生成不同的砖块阵型
  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      if (currentLevel == 1) {
        // 【第一关】传统满格阵
        brickGrid[r][c] = true;
      } else if (currentLevel == 2) {
        // 【第二关】棋盘交错阵
        brickGrid[r][c] = ((r + c) % 2 == 0);
      } else if (currentLevel == 3) {
        // 【第三关】V字/金字塔阵型
        brickGrid[r][c] = (c >= r && c < (BRICK_COLS - r));
      } else {
        // 【第四关及以后】随机关卡生成（每个位置 60% 概率有砖块）
        brickGrid[r][c] = (random(0, 10) < 6);
      }
    }
  }

  // 安全检查：防止随机关卡生成了“空地图”
  if (currentLevel > MAX_DESIGNED_LEVELS) {
    bool hasAnyBrick = false;
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < BRICK_COLS; c++) {
        if (brickGrid[r][c])
          hasAnyBrick = true;
      }
    }
    // 如果运气太差全是空，保底塞满第一行
    if (!hasAnyBrick) {
      for (int c = 0; c < BRICK_COLS; c++)
        brickGrid[0][c] = true;
    }
  }
}

void initBrickGame() {
  scoreBrick = 0;
  currentLevel = 1; // 从第一关开始
  gameOverBrick = false;
  gameWinBrick = false;
  setupLevel(); // 调用关卡初始化
}

void handleBrickMode(bool clicked) {
  if (gameOverBrick || gameWinBrick) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 15);
    if (gameWinBrick)
      display.println(F("YOU WIN!"));
    else
      display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 38);
    display.print(F("Score: "));
    display.println(scoreBrick);
    display.setCursor(10, 51);
    display.println(F("[Click] Return"));
    return;
  }
  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }
  int rawPot = analogRead(POT_PIN);
  float normPot = 1.0f - ((float)rawPot / 4095.0f);
  brickPaddleX = (int)(normPot * (SCREEN_WIDTH - brickPaddleWidth));
  brickPaddleX = constrain(brickPaddleX, 0, SCREEN_WIDTH - brickPaddleWidth);

  if (millis() - lastBrickUpdate > 20) {
    lastBrickUpdate = millis();
    brickBallX += brickBallVX;
    brickBallY += brickBallVY;
    if (brickBallX <= 0) {
      brickBallX = 0;
      brickBallVX = -brickBallVX;
    } else if (brickBallX >= SCREEN_WIDTH - 2) {
      brickBallX = SCREEN_WIDTH - 2;
      brickBallVX = -brickBallVX;
    }
    if (brickBallY <= 10) {
      brickBallY = 10;
      brickBallVY = -brickBallVY;
    }
    if (brickBallY > SCREEN_HEIGHT) {
      gameOverBrick = true;
      return;
    }
    if (brickBallY >= brickPaddleY - 2 && brickBallY <= brickPaddleY + 2) {
      if (brickBallX >= brickPaddleX - 1 &&
          brickBallX <= brickPaddleX + brickPaddleWidth + 1 &&
          brickBallVY > 0) {
        brickBallVY = -brickBallVY;
        float hitCenterOffset =
            (brickBallX - brickPaddleX) / (float)brickPaddleWidth;
        brickBallVX = 3.5f * (hitCenterOffset - 0.5f);
      }
    }
    bool hasBricksLeft = false;
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < BRICK_COLS; c++) {
        if (brickGrid[r][c]) {
          hasBricksLeft = true;
          int bx = brickOffsetX + c * (brickWidth + brickGapX);
          int by = brickOffsetY + r * (brickHeight + brickGapY);
          if (brickBallX + 2 >= bx && brickBallX <= bx + brickWidth &&
              brickBallY + 2 >= by && brickBallY <= by + brickHeight) {
            brickGrid[r][c] = false;
            brickBallVY = -brickBallVY;
            scoreBrick += 1;
            brickBallVX *= 1.015f;
            brickBallVY *= 1.015f;
            break;
          }
        }
      }
    }

    if (!hasBricksLeft) {
      currentLevel++;
      setupLevel();
      return;
    }
  }

  // === UI 渲染部分 ===
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(2, 0);
  display.print(F("LVL:"));
  display.print(currentLevel);

  display.setCursor(73, 0);
  display.print(F("SCORE:"));
  display.print(scoreBrick);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BRICK_COLS; c++)
      if (brickGrid[r][c]) {
        int bx = brickOffsetX + c * (brickWidth + brickGapX);
        int by = brickOffsetY + r * (brickHeight + brickGapY);
        display.fillRect(bx, by, brickWidth, brickHeight, SSD1306_WHITE);
      }
  display.fillRect(brickPaddleX, brickPaddleY, brickPaddleWidth,
                   brickPaddleHeight, SSD1306_WHITE);
  display.fillRect((int)brickBallX, (int)brickBallY, 2, 2, SSD1306_WHITE);
}
