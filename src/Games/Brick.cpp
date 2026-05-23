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

void initBrickGame() {
  brickBallX = 64.0f;
  brickBallY = 45.0f;
  brickBallVX = 1.3f;
  brickBallVY = -1.3f;
  scoreBrick = 0;
  gameOverBrick = false;
  gameWinBrick = false;
  lastBrickUpdate = millis();
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BRICK_COLS; c++)
      brickGrid[r][c] = true;
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
            scoreBrick += 10;
            brickBallVX *= 1.015f;
            brickBallVY *= 1.015f;
            break;
          }
        }
      }
    }
    if (!hasBricksLeft) {
      gameWinBrick = true;
      return;
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("BRICKS"));
  display.setCursor(75, 0);
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
