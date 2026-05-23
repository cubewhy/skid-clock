#include "../Config.h"
#include "../Games.h"

static uint8_t navalBoardPlayer[8][8];
static uint8_t navalBoardEnemy[8][8];
static int8_t navalCx = 3, navalCy = 3;
static uint8_t navalWinner = 0;

static void placeRandomShips(uint8_t board[8][8]) {
  memset(board, 0, 64);
  int shipSizes[] = {4, 3, 2, 2};
  for (int s = 0; s < 4; s++) {
    int size = shipSizes[s];
    bool placed = false;
    while (!placed) {
      int dir = random(0, 2);
      int x = random(0, 8);
      int y = random(0, 8);
      bool valid = true;
      for (int i = 0; i < size; i++) {
        int tx = x + (dir == 0 ? i : 0);
        int ty = y + (dir == 1 ? i : 0);
        if (tx >= 8 || ty >= 8 || board[tx][ty] != 0) {
          valid = false;
          break;
        }
      }
      if (valid) {
        for (int i = 0; i < size; i++) {
          int tx = x + (dir == 0 ? i : 0);
          int ty = y + (dir == 1 ? i : 0);
          board[tx][ty] = 1;
        }
        placed = true;
      }
    }
  }
}

static bool checkNavalWin(uint8_t board[8][8]) {
  for (int x = 0; x < 8; x++)
    for (int y = 0; y < 8; y++)
      if (board[x][y] == 1)
        return false;
  return true;
}

static void navalAIMove() {
  while (true) {
    int rx = random(0, 8);
    int ry = random(0, 8);
    uint8_t val = navalBoardPlayer[rx][ry];
    if (val == 0 || val == 1) {
      if (val == 1)
        navalBoardPlayer[rx][ry] = 3;
      else
        navalBoardPlayer[rx][ry] = 2;
      if (checkNavalWin(navalBoardPlayer))
        navalWinner = 2;
      break;
    }
  }
}

void initNavalGame() {
  navalWinner = 0;
  navalCx = 3;
  navalCy = 3;
  placeRandomShips(navalBoardPlayer);
  placeRandomShips(navalBoardEnemy);
}

void handleNavalPlay(int vry, int vrx, bool clicked) {
  if (navalWinner != 0) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(12, 12);
    if (navalWinner == 1)
      display.println(F("YOU WIN!"));
    else
      display.println(F("AI WIN!"));
    display.setTextSize(1);
    display.setCursor(12, 45);
    display.println(F("[Click] Return Menu"));
    return;
  }
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000 && navalCy > 0) {
      navalCy--;
      lastJoyAction = millis();
    } else if (vry > 3000 && navalCy < 7) {
      navalCy++;
      lastJoyAction = millis();
    }
    if (vrx < 1000 && navalCx > 0) {
      navalCx--;
      lastJoyAction = millis();
    } else if (vrx > 3000 && navalCx < 7) {
      navalCx++;
      lastJoyAction = millis();
    }
  }
  if (clicked) {
    uint8_t val = navalBoardEnemy[navalCx][navalCy];
    if (val == 0 || val == 1) {
      if (val == 1)
        navalBoardEnemy[navalCx][navalCy] = 3;
      else
        navalBoardEnemy[navalCx][navalCy] = 2;
      if (checkNavalWin(navalBoardEnemy)) {
        navalWinner = 1;
        return;
      }
      navalAIMove();
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("NAVAL BATTLE"));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  int startY = 14, cellS = 5;
  int startX1 = 4;
  display.drawRect(startX1 - 1, startY - 1, 8 * cellS + 2, 8 * cellS + 2,
                   SSD1306_WHITE);
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      uint8_t val = navalBoardPlayer[x][y];
      int cx = startX1 + x * cellS;
      int cy = startY + y * cellS;
      if (val == 1)
        display.drawRect(cx + 1, cy + 1, cellS - 2, cellS - 2, SSD1306_WHITE);
      else if (val == 2)
        display.drawPixel(cx + 2, cy + 2, SSD1306_WHITE);
      else if (val == 3)
        display.fillRect(cx + 1, cy + 1, cellS - 2, cellS - 2, SSD1306_WHITE);
    }
  }
  int startX2 = 68;
  display.drawRect(startX2 - 1, startY - 1, 8 * cellS + 2, 8 * cellS + 2,
                   SSD1306_WHITE);
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      uint8_t val = navalBoardEnemy[x][y];
      int cx = startX2 + x * cellS;
      int cy = startY + y * cellS;
      if (val == 2)
        display.drawPixel(cx + 2, cy + 2, SSD1306_WHITE);
      else if (val == 3)
        display.fillRect(cx + 1, cy + 1, cellS - 2, cellS - 2, SSD1306_WHITE);
    }
  }
  int curX = startX2 + navalCx * cellS;
  int curY = startY + navalCy * cellS;
  display.drawRect(curX, curY, cellS, cellS, SSD1306_WHITE);
  display.setCursor(4, 56);
  display.print(F("MY FLEET"));
  display.setCursor(68, 56);
  display.print(F("EN RADAR"));
}
