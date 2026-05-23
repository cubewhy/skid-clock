#include "../Config.h"
#include "../Games.h"

static int16_t board2048[4][4];
static int score2048 = 0;
static bool gameOver2048 = false;

static void spawn2048Tile() {
  int count = 0;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      if (board2048[i][j] == 0)
        count++;
  if (count == 0)
    return;
  int target = random(0, count);
  int current = 0;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (board2048[i][j] == 0) {
        if (current == target) {
          board2048[i][j] = (random(0, 10) == 0) ? 4 : 2;
          return;
        }
        current++;
      }
    }
  }
}

static bool check2048GameOver() {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (board2048[i][j] == 0)
        return false;
      if (i < 3 && board2048[i][j] == board2048[i + 1][j])
        return false;
      if (j < 3 && board2048[i][j] == board2048[i][j + 1])
        return false;
    }
  }
  return true;
}

void init2048Game() {
  memset(board2048, 0, sizeof(board2048));
  score2048 = 0;
  gameOver2048 = false;
  spawn2048Tile();
  spawn2048Tile();
}

void handle2048Mode(int vry, int vrx, bool clicked) {
  if (gameOver2048) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 35);
    display.print(F("Score: "));
    display.println(score2048);
    display.setCursor(10, 50);
    display.println(F("[Click] return Menu"));
    return;
  }
  bool moved = false;
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vrx < 1000) {
      for (int i = 0; i < 4; i++) {
        int target = 0;
        for (int j = 0; j < 4; j++) {
          if (board2048[i][j] != 0) {
            if (target != j) {
              board2048[i][target] = board2048[i][j];
              board2048[i][j] = 0;
              moved = true;
            }
            target++;
          }
        }
        for (int j = 0; j < 3; j++) {
          if (board2048[i][j] != 0 && board2048[i][j] == board2048[i][j + 1]) {
            board2048[i][j] *= 2;
            score2048 += board2048[i][j];
            board2048[i][j + 1] = 0;
            moved = true;
          }
        }
        target = 0;
        for (int j = 0; j < 4; j++) {
          if (board2048[i][j] != 0) {
            if (target != j) {
              board2048[i][target] = board2048[i][j];
              board2048[i][j] = 0;
            }
            target++;
          }
        }
      }
      lastJoyAction = millis();
    } else if (vrx > 3000) {
      for (int i = 0; i < 4; i++) {
        int target = 3;
        for (int j = 3; j >= 0; j--) {
          if (board2048[i][j] != 0) {
            if (target != j) {
              board2048[i][target] = board2048[i][j];
              board2048[i][j] = 0;
              moved = true;
            }
            target--;
          }
        }
        for (int j = 3; j > 0; j--) {
          if (board2048[i][j] != 0 && board2048[i][j] == board2048[i][j - 1]) {
            board2048[i][j] *= 2;
            score2048 += board2048[i][j];
            board2048[i][j - 1] = 0;
            moved = true;
          }
        }
        target = 3;
        for (int j = 3; j >= 0; j--) {
          if (board2048[i][j] != 0) {
            if (target != j) {
              board2048[i][target] = board2048[i][j];
              board2048[i][j] = 0;
            }
            target--;
          }
        }
      }
      lastJoyAction = millis();
    } else if (vry < 1000) {
      for (int j = 0; j < 4; j++) {
        int target = 0;
        for (int i = 0; i < 4; i++) {
          if (board2048[i][j] != 0) {
            if (target != i) {
              board2048[target][j] = board2048[i][j];
              board2048[i][j] = 0;
              moved = true;
            }
            target++;
          }
        }
        for (int i = 0; i < 3; i++) {
          if (board2048[i][j] != 0 && board2048[i][j] == board2048[i + 1][j]) {
            board2048[i][j] *= 2;
            score2048 += board2048[i][j];
            board2048[i + 1][j] = 0;
            moved = true;
          }
        }
        target = 0;
        for (int i = 0; i < 4; i++) {
          if (board2048[i][j] != 0) {
            if (target != i) {
              board2048[target][j] = board2048[i][j];
              board2048[i][j] = 0;
            }
            target++;
          }
        }
      }
      lastJoyAction = millis();
    } else if (vry > 3000) {
      for (int j = 0; j < 4; j++) {
        int target = 3;
        for (int i = 3; i >= 0; i--) {
          if (board2048[i][j] != 0) {
            if (target != i) {
              board2048[target][j] = board2048[i][j];
              board2048[i][j] = 0;
              moved = true;
            }
            target--;
          }
        }
        for (int i = 3; i > 0; i--) {
          if (board2048[i][j] != 0 && board2048[i][j] == board2048[i - 1][j]) {
            board2048[i][j] *= 2;
            score2048 += board2048[i][j];
            board2048[i - 1][j] = 0;
            moved = true;
          }
        }
        target = 3;
        for (int i = 3; i >= 0; i--) {
          if (board2048[i][j] != 0) {
            if (target != i) {
              board2048[target][j] = board2048[i][j];
              board2048[i][j] = 0;
            }
            target--;
          }
        }
      }
      lastJoyAction = millis();
    }
  }
  if (moved) {
    spawn2048Tile();
    if (check2048GameOver())
      gameOver2048 = true;
  }
  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  display.clearDisplay();
  int startX = 2, startY = 2;
  for (int i = 0; i <= 4; i++) {
    display.drawFastHLine(startX, startY + i * 15, 96, SSD1306_WHITE);
    display.drawFastVLine(startX + i * 24, startY, 60, SSD1306_WHITE);
  }
  display.setTextSize(1);
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (board2048[i][j] != 0) {
        int val = board2048[i][j];
        int cx = startX + j * 24;
        int cy = startY + i * 15;
        int offset = 9;
        if (val >= 1000)
          offset = 1;
        else if (val >= 100)
          offset = 3;
        else if (val >= 10)
          offset = 6;
        display.setCursor(cx + offset, cy + 4);
        display.print(val);
      }
    }
  }
  display.setCursor(102, 6);
  display.print(F("2048"));
  display.drawFastHLine(100, 16, 26, SSD1306_WHITE);
  display.setCursor(102, 22);
  display.print(F("SCO:"));
  display.setCursor(102, 34);
  display.print(score2048);
}
