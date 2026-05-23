#include "../Config.h"
#include "../Games.h"

#define GOMOKU_SIZE 10
static int8_t gomokuBoard[GOMOKU_SIZE][GOMOKU_SIZE];
static int8_t gomokuCx = 4, gomokuCy = 4;
static bool gomokuIsPvE = true;
static int gomokuDiff = 1;
static int8_t gomokuTurn = 1;
static int8_t gomokuWinner = 0;
int gomokuMenuSelect = 0;

static bool checkGomokuWin(int x, int y, int role) {
  int dx[] = {1, 0, 1, 1};
  int dy[] = {0, 1, 1, -1};
  for (int i = 0; i < 4; i++) {
    int count = 1;
    int tx = x + dx[i], ty = y + dy[i];
    while (tx >= 0 && tx < GOMOKU_SIZE && ty >= 0 && ty < GOMOKU_SIZE &&
           gomokuBoard[tx][ty] == role) {
      count++;
      tx += dx[i];
      ty += dy[i];
    }
    tx = x - dx[i];
    int ty2 = y - dy[i];
    while (tx >= 0 && tx < GOMOKU_SIZE && ty2 >= 0 && ty2 < GOMOKU_SIZE &&
           gomokuBoard[tx][ty2] == role) {
      count++;
      tx -= dx[i];
      ty2 -= dy[i];
    }
    if (count >= 5)
      return true;
  }
  return false;
}

static int evaluateGomokuPoint(int x, int y, int role) {
  int dx[] = {1, 0, 1, 1};
  int dy[] = {0, 1, 1, -1};
  int totalWeight = 0;
  for (int i = 0; i < 4; i++) {
    int count = 1, openEnds = 0;
    int tx = x + dx[i], ty = y + dy[i];
    while (tx >= 0 && tx < GOMOKU_SIZE && ty >= 0 && ty < GOMOKU_SIZE &&
           gomokuBoard[tx][ty] == role) {
      count++;
      tx += dx[i];
      ty += dy[i];
    }
    if (tx >= 0 && tx < GOMOKU_SIZE && ty >= 0 && ty < GOMOKU_SIZE &&
        gomokuBoard[tx][ty] == 0)
      openEnds++;
    tx = x - dx[i];
    int ty2 = y - dy[i];
    while (tx >= 0 && tx < GOMOKU_SIZE && ty2 >= 0 && ty2 < GOMOKU_SIZE &&
           gomokuBoard[tx][ty2] == role) {
      count++;
      tx -= dx[i];
      ty2 -= dy[i];
    }
    if (tx >= 0 && tx < GOMOKU_SIZE && ty2 >= 0 && ty2 < GOMOKU_SIZE &&
        gomokuBoard[tx][ty2] == 0)
      openEnds++;
    if (count >= 5)
      totalWeight += 5000;
    else if (count == 4 && openEnds == 2)
      totalWeight += 1200;
    else if (count == 4 && openEnds == 1)
      totalWeight += 800;
    else if (count == 3 && openEnds == 2)
      totalWeight += 400;
    else if (count == 3 && openEnds == 1)
      totalWeight += 100;
    else if (count == 2 && openEnds == 2)
      totalWeight += 30;
  }
  return totalWeight;
}

static void gomokuAIMove() {
  int bestX = -1, bestY = -1, maxScore = -1;
  if (gomokuDiff == 1) {
    for (int x = 0; x < GOMOKU_SIZE; x++) {
      for (int y = 0; y < GOMOKU_SIZE; y++) {
        if (gomokuBoard[x][y] == 0) {
          int totalScore =
              evaluateGomokuPoint(x, y, 2) + evaluateGomokuPoint(x, y, 1) * 1.2;
          if (totalScore > maxScore) {
            maxScore = totalScore;
            bestX = x;
            bestY = y;
          }
        }
      }
    }
  } else {
    for (int x = 0; x < GOMOKU_SIZE; x++)
      for (int y = 0; y < GOMOKU_SIZE; y++)
        if (gomokuBoard[x][y] == 0 && evaluateGomokuPoint(x, y, 2) >= 1000) {
          bestX = x;
          bestY = y;
          goto doMove;
        }
    for (int x = 0; x < GOMOKU_SIZE; x++)
      for (int y = 0; y < GOMOKU_SIZE; y++)
        if (gomokuBoard[x][y] == 0 && evaluateGomokuPoint(x, y, 1) >= 1000) {
          bestX = x;
          bestY = y;
          goto doMove;
        }
    while (true) {
      int rx = random(0, GOMOKU_SIZE), ry = random(0, GOMOKU_SIZE);
      if (gomokuBoard[rx][ry] == 0) {
        bestX = rx;
        bestY = ry;
        break;
      }
    }
  }
doMove:
  if (bestX != -1 && bestY != -1) {
    gomokuBoard[bestX][bestY] = 2;
    gomokuCx = bestX;
    gomokuCy = bestY;
    if (checkGomokuWin(bestX, bestY, 2))
      gomokuWinner = 2;
  }
}

void initGomokuGame() {
  memset(gomokuBoard, 0, sizeof(gomokuBoard));
  gomokuCx = GOMOKU_SIZE / 2;
  gomokuCy = GOMOKU_SIZE / 2;
  gomokuTurn = 1;
  gomokuWinner = 0;
}

void handleGomokuMenu(int vry, int vrx, bool clicked) {
  if (millis() - lastJoyAction > JOY_DELAY) {
    // 纵向 Y 轴：切换选项
    if (vry < 1000) {
      gomokuMenuSelect = (gomokuMenuSelect == 0) ? 2 : gomokuMenuSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      gomokuMenuSelect = (gomokuMenuSelect + 1) % 3;
      lastJoyAction = millis();
    }

    // 横向 X 轴：左右进退及配置切换
    if (vrx < 1000) { // 向左移动摇杆
      if (gomokuMenuSelect == 0) {
        gomokuIsPvE = !gomokuIsPvE;
        lastJoyAction = millis();
      } else if (gomokuMenuSelect == 1 && gomokuIsPvE) {
        gomokuDiff = (gomokuDiff == 0) ? 1 : 0;
        lastJoyAction = millis();
      } else if (gomokuMenuSelect == 2) {
        currentState =
            STATE_GAMES_MENU; // ✨ 处于 [START GAME] 时向左拨动返回游戏菜单
        lastJoyAction = millis();
        return;
      }
    } else if (vrx > 3000) { // 向右移动摇杆
      if (gomokuMenuSelect == 0) {
        gomokuIsPvE = !gomokuIsPvE;
        lastJoyAction = millis();
      } else if (gomokuMenuSelect == 1 && gomokuIsPvE) {
        gomokuDiff = (gomokuDiff == 0) ? 1 : 0;
        lastJoyAction = millis();
      } else if (gomokuMenuSelect == 2) {
        initGomokuGame();
        currentState =
            STATE_GOMOKU_PLAY; // ✨ 处于 [START GAME] 时向右拨动直接启动游戏
        lastJoyAction = millis();
        return;
      }
    }
  }

  // 物理中心键按下同样保持原生兼容
  if (clicked) {
    if (gomokuMenuSelect == 2) {
      initGomokuGame();
      currentState = STATE_GOMOKU_PLAY;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("=== GOMOKU CONFIG ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  display.setCursor(10, 18);
  display.print(gomokuMenuSelect == 0 ? F("> Mode: ") : F("  Mode: "));
  display.println(gomokuIsPvE ? F("VS Computer") : F("Dual Player"));
  display.setCursor(10, 32);
  display.print(gomokuMenuSelect == 1 ? F("> AI:   ") : F("  AI:   "));
  if (gomokuIsPvE)
    display.println(gomokuDiff == 0 ? F("Easy") : F("Hard"));
  else
    display.println(F("N/A"));
  display.setCursor(10, 50);
  display.println(gomokuMenuSelect == 2 ? F("> [ START GAME ]")
                                        : F("  [ START GAME ]"));
}

void handleGomokuPlay(int vry, int vrx, bool clicked) {
  if (gomokuWinner != 0) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(12, 12);
    if (gomokuWinner == 1)
      display.println(F("P1 WIN!"));
    else if (gomokuWinner == 2)
      display.println(gomokuIsPvE ? F("AI WIN!") : F("P2 WIN!"));
    else
      display.println(F("DRAW GAME"));
    display.setTextSize(1);
    display.setCursor(12, 45);
    display.println(F("[Click] Return Menu"));
    return;
  }
  if (gomokuTurn == 1 || (gomokuTurn == 2 && !gomokuIsPvE)) {
    if (millis() - lastJoyAction > JOY_DELAY) {
      if (vry < 1000 && gomokuCy > 0) {
        gomokuCy--;
        lastJoyAction = millis();
      } else if (vry > 3000 && gomokuCy < GOMOKU_SIZE - 1) {
        gomokuCy++;
        lastJoyAction = millis();
      }
      if (vrx < 1000 && gomokuCx > 0) {
        gomokuCx--;
        lastJoyAction = millis();
      } else if (vrx > 3000 && gomokuCx < GOMOKU_SIZE - 1) {
        gomokuCx++;
        lastJoyAction = millis();
      }
    }
    if (clicked && gomokuBoard[gomokuCx][gomokuCy] == 0) {
      gomokuBoard[gomokuCx][gomokuCy] = gomokuTurn;
      if (checkGomokuWin(gomokuCx, gomokuCy, gomokuTurn)) {
        gomokuWinner = gomokuTurn;
        return;
      }
      bool full = true;
      for (int i = 0; i < GOMOKU_SIZE; i++)
        for (int j = 0; j < GOMOKU_SIZE; j++)
          if (gomokuBoard[i][j] == 0)
            full = false;
      if (full) {
        gomokuWinner = 3;
        return;
      }
      gomokuTurn = (gomokuTurn == 1) ? 2 : 1;
    }
  }
  if (gomokuWinner == 0 && gomokuTurn == 2 && gomokuIsPvE) {
    gomokuAIMove();
    gomokuTurn = 1;
  }
  display.clearDisplay();
  int startX = 4, startY = 5, space = 6;
  for (int i = 0; i < GOMOKU_SIZE; i++) {
    display.drawFastHLine(startX, startY + i * space,
                          (GOMOKU_SIZE - 1) * space + 1, SSD1306_WHITE);
    display.drawFastVLine(startX + i * space, startY,
                          (GOMOKU_SIZE - 1) * space + 1, SSD1306_WHITE);
  }
  for (int x = 0; x < GOMOKU_SIZE; x++) {
    for (int y = 0; y < GOMOKU_SIZE; y++) {
      if (gomokuBoard[x][y] == 1)
        display.fillRect(startX + x * space - 2, startY + y * space - 2, 5, 5,
                         SSD1306_WHITE);
      else if (gomokuBoard[x][y] == 2)
        display.drawRect(startX + x * space - 2, startY + y * space - 2, 5, 5,
                         SSD1306_WHITE);
    }
  }
  display.drawRect(startX + gomokuCx * space - 3, startY + gomokuCy * space - 3,
                   7, 7, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(64, 4);
  display.print(F("GOMOKU"));
  display.drawFastHLine(64, 14, 64, SSD1306_WHITE);
  display.setCursor(64, 20);
  display.print(F("Mode:"));
  display.print(gomokuIsPvE ? F("PvE") : F("PvP"));
  display.setCursor(64, 34);
  display.print(F("Turn:"));
  display.print(gomokuTurn == 1 ? F("P1[X]")
                                : (gomokuIsPvE ? F("AI[O]") : F("P2[O]")));
}
