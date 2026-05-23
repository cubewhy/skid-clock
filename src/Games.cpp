#include "Games.h"
#include "Config.h"

// 游戏中心二级菜单集
const char *gameMenuItems[] = {
    "1. Snake Game",    "2. Gomoku Game", "3. 2048 Game",    "4. Dino Run",
    "5. Brick Breaker", "6. Stack Tower", "7. Naval Battle", "8. < Back"};
const int GAMES_TOTAL = 8;
const int VISIBLE_GAMES_ITEMS = 3;
int currentGamesSelect = 0;
int gamesScrollTop = 0;

// ==========================================
// Game 1: Snake
// ==========================================
#define SNAKE_BLOCK_SIZE 4
#define GRID_WIDTH (SCREEN_WIDTH / SNAKE_BLOCK_SIZE)
#define GRID_HEIGHT (SCREEN_HEIGHT / SNAKE_BLOCK_SIZE)
#define SNAKE_MAX_LEN 64
static Point snake[SNAKE_MAX_LEN];
static int snakeLength = 3;
static SnakeDirection snakeDir = SNAKE_RIGHT;
static Point food;
static bool isGameOver = false;
static unsigned long lastSnakeUpdate = 0;
static const int snakeSpeed = 150;

void spawnFood() {
  food.x = random(0, GRID_WIDTH);
  food.y = random(0, GRID_HEIGHT);
  for (int i = 0; i < snakeLength; i++) {
    if (snake[i].x == food.x && snake[i].y == food.y) {
      food.x = random(0, GRID_WIDTH);
      food.y = random(0, GRID_HEIGHT);
      i = -1;
    }
  }
}
void initSnakeGame() {
  snakeLength = 3;
  snake[0] = {15, 8};
  snake[1] = {14, 8};
  snake[2] = {13, 8};
  snakeDir = SNAKE_RIGHT;
  isGameOver = false;
  spawnFood();
}
void handleSnakeMode(int vry, int vrx, bool clicked) {
  if (isGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 35);
    display.print(F("Score: "));
    display.println(snakeLength - 3);
    display.setCursor(10, 50);
    display.println(F("[Click] return Menu"));
    return;
  }
  if (vrx < 1000 && snakeDir != SNAKE_RIGHT)
    snakeDir = SNAKE_LEFT;
  else if (vrx > 3000 && snakeDir != SNAKE_LEFT)
    snakeDir = SNAKE_RIGHT;
  else if (vry < 1000 && snakeDir != SNAKE_DOWN)
    snakeDir = SNAKE_UP;
  else if (vry > 3000 && snakeDir != SNAKE_UP)
    snakeDir = SNAKE_DOWN;
  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  if (millis() - lastSnakeUpdate > snakeSpeed) {
    lastSnakeUpdate = millis();
    for (int i = snakeLength - 1; i > 0; i--)
      snake[i] = snake[i - 1];
    if (snakeDir == SNAKE_UP)
      snake[0].y--;
    else if (snakeDir == SNAKE_DOWN)
      snake[0].y++;
    else if (snakeDir == SNAKE_LEFT)
      snake[0].x--;
    else if (snakeDir == SNAKE_RIGHT)
      snake[0].x++;

    if (snake[0].x < 0 || snake[0].x >= GRID_WIDTH || snake[0].y < 0 ||
        snake[0].y >= GRID_HEIGHT) {
      isGameOver = true;
      return;
    }
    for (int i = 1; i < snakeLength; i++) {
      if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
        isGameOver = true;
        return;
      }
    }
    if (snake[0].x == food.x && snake[0].y == food.y) {
      if (snakeLength < SNAKE_MAX_LEN)
        snakeLength++;
      spawnFood();
    }
  }
  display.clearDisplay();
  for (int i = 0; i < snakeLength; i++) {
    display.fillRect(snake[i].x * SNAKE_BLOCK_SIZE,
                     snake[i].y * SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE - 1,
                     SNAKE_BLOCK_SIZE - 1, SSD1306_WHITE);
  }
  display.drawRect(food.x * SNAKE_BLOCK_SIZE, food.y * SNAKE_BLOCK_SIZE,
                   SNAKE_BLOCK_SIZE, SNAKE_BLOCK_SIZE, SSD1306_WHITE);
}

// ==========================================
// Game 2: Gomoku
// ==========================================
#define GOMOKU_SIZE 10
static int8_t gomokuBoard[GOMOKU_SIZE][GOMOKU_SIZE];
static int8_t gomokuCx = 4, gomokuCy = 4;
static bool gomokuIsPvE = true;
static int gomokuDiff = 1;
static int8_t gomokuTurn = 1;
static int8_t gomokuWinner = 0;
int gomokuMenuSelect = 0;

bool checkGomokuWin(int x, int y, int role) {
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
int evaluateGomokuPoint(int x, int y, int role) {
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
void gomokuAIMove() {
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
    if (vry < 1000) {
      gomokuMenuSelect = (gomokuMenuSelect == 0) ? 2 : gomokuMenuSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      gomokuMenuSelect = (gomokuMenuSelect + 1) % 3;
      lastJoyAction = millis();
    }
    if (vrx < 1000 || vrx > 3000) {
      if (gomokuMenuSelect == 0)
        gomokuIsPvE = !gomokuIsPvE;
      if (gomokuMenuSelect == 1 && gomokuIsPvE)
        gomokuDiff = (gomokuDiff == 0) ? 1 : 0;
      lastJoyAction = millis();
    }
  }
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
  display.setCursor(64, 52);
  display.print(F("Exit->Hold"));
}

// ==========================================
// Game 3: 2048
// ==========================================
static int16_t board2048[4][4];
static int score2048 = 0;
static bool gameOver2048 = false;

void spawn2048Tile() {
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
bool check2048GameOver() {
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

// ==========================================
// Game 4: Dino Run
// ==========================================
static float dinoY = 40.0f;
static float dinoVelocityY = 0.0f;
static bool dinoIsJumping = false;
static bool dinoIsDucking = false;
static float scoreDino = 0.0f;
static bool gameOverDino = false;
static unsigned long lastDinoUpdate = 0;
static float dinoGameSpeed = 0.10f;
const float DINO_GRAVITY = 0.00055f;
const float DINO_JUMP_FORCE = -0.17f;
#define MAX_OBSTACLES 2
static Obstacle obstacles[MAX_OBSTACLES];
static unsigned long lastObstacleSpawn = 0;

void initDinoGame() {
  dinoY = 40.0f;
  dinoVelocityY = 0.0f;
  dinoIsJumping = false;
  dinoIsDucking = false;
  scoreDino = 0.0f;
  gameOverDino = false;
  dinoGameSpeed = 0.10f;
  lastDinoUpdate = millis();
  lastObstacleSpawn = 0;
  for (int i = 0; i < MAX_OBSTACLES; i++)
    obstacles[i].active = false;
}
void handleDinoMode(int vry, int vrx, bool clicked) {
  if (gameOverDino) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(10, 35);
    display.print(F("Score: "));
    display.println((int)scoreDino);
    display.setCursor(10, 50);
    display.println(F("[Click] return Menu"));
    return;
  }
  unsigned long now = millis();
  float dt = (float)(now - lastDinoUpdate);
  lastDinoUpdate = now;
  if (dt > 50.0f)
    dt = 50.0f;
  if (dt <= 0.0f)
    dt = 1.0f;
  if (vry > 3000) {
    if (!dinoIsJumping)
      dinoIsDucking = true;
    else
      dinoVelocityY += DINO_GRAVITY * 2.5f * dt;
  } else
    dinoIsDucking = false;
  if (vry < 1000 && !dinoIsJumping && !dinoIsDucking) {
    dinoVelocityY = DINO_JUMP_FORCE;
    dinoIsJumping = true;
  }
  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }

  scoreDino += 0.015f * dt;
  dinoGameSpeed += 0.000002f * dt;
  if (dinoGameSpeed > 0.22f)
    dinoGameSpeed = 0.22f;
  if (dinoIsJumping) {
    dinoY += dinoVelocityY * dt;
    dinoVelocityY += DINO_GRAVITY * dt;
    if (dinoY >= 40.0f) {
      dinoY = 40.0f;
      dinoIsJumping = false;
      dinoVelocityY = 0.0f;
    }
  }

  bool obstacleActive = false;
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      obstacles[i].x -= dinoGameSpeed * dt;
      if (obstacles[i].x < -12.0f)
        obstacles[i].active = false;
      else {
        obstacleActive = true;
        int dw = dinoIsDucking ? 14 : 10;
        int dh = dinoIsDucking ? 6 : 12;
        int dy = dinoIsDucking ? 46 : (int)dinoY;
        int dx = 15;
        int ow =
            (obstacles[i].type == 0) ? 6 : ((obstacles[i].type == 1) ? 8 : 10);
        int oh =
            (obstacles[i].type == 0) ? 10 : ((obstacles[i].type == 1) ? 14 : 6);
        if (dx < (int)obstacles[i].x + ow && dx + dw > (int)obstacles[i].x &&
            dy < obstacles[i].y + oh && dy + dh > obstacles[i].y) {
          gameOverDino = true;
          return;
        }
      }
    }
  }
  uint32_t spawnInterval = 1500 / (dinoGameSpeed / 0.10f);
  if (spawnInterval < 600)
    spawnInterval = 600;
  if (!obstacleActive && (now - lastObstacleSpawn > spawnInterval)) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (!obstacles[i].active) {
        obstacles[i].active = true;
        obstacles[i].x = 128.0f;
        obstacles[i].type = random(0, 3);
        if (obstacles[i].type == 0)
          obstacles[i].y = 42;
        else if (obstacles[i].type == 1)
          obstacles[i].y = 38;
        else
          obstacles[i].y = 32;
        lastObstacleSpawn = now;
        break;
      }
    }
  }
  display.clearDisplay();
  display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
  if (dinoIsDucking)
    display.fillRect(15, 46, 14, 6, SSD1306_WHITE);
  else
    display.fillRect(15, (int)dinoY, 10, 12, SSD1306_WHITE);
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      int ow =
          (obstacles[i].type == 0) ? 6 : ((obstacles[i].type == 1) ? 8 : 10);
      int oh =
          (obstacles[i].type == 0) ? 10 : ((obstacles[i].type == 1) ? 14 : 6);
      if (obstacles[i].type == 2)
        display.drawRect((int)obstacles[i].x, obstacles[i].y, ow, oh,
                         SSD1306_WHITE);
      else
        display.fillRect((int)obstacles[i].x, obstacles[i].y, ow, oh,
                         SSD1306_WHITE);
    }
  }
  display.setTextSize(1);
  display.setCursor(90, 2);
  display.print((int)scoreDino);
}

// ==========================================
// Game 5: Brick Breaker
// ==========================================
static float brickBallX, brickBallY;
static float brickBallVX, brickBallVY;
static int brickPaddleX;
const int brickPaddleWidth = 24;
const int brickPaddleHeight = 3;
const int brickPaddleY = 58;
#define BRICK_ROWS 3
#define BRICK_COLS 6
static bool brickGrid[BRICK_ROWS][BRICK_COLS];
const int brickWidth = 18;
const int brickHeight = 4;
const int brickGapX = 3;
const int brickGapY = 3;
const int brickOffsetX = 2;
const int brickOffsetY = 13;
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

// ==========================================
// Game 6: Stack Tower
// ==========================================
#define MAX_STACK_LAYERS 40
#define STACK_BLOCK_HEIGHT 5
static float stackBlockX = 0.0f;
static int stackBlockWidth = 45;
static float stackBlockSpeedX = 1.4f;
static bool stackIsFalling = false;
static float stackBlockY = 12.0f;
static int towerLayerCount = 0;
static int scoreStack = 0;
static bool gameOverStack = false;
static unsigned long lastStackTick = 0;
static int towerWidths[MAX_STACK_LAYERS];
static int towerXs[MAX_STACK_LAYERS];
static int cameraViewOffsetY = 0;
static bool joyMoveLatched = false;

void initStackGame() {
  scoreStack = 0;
  towerLayerCount = 0;
  gameOverStack = false;
  stackBlockWidth = 45;
  stackBlockX = (SCREEN_WIDTH - stackBlockWidth) / 2;
  stackBlockY = 12.0f;
  stackBlockSpeedX = 1.4f;
  stackIsFalling = false;
  cameraViewOffsetY = 0;
  joyMoveLatched = false;
  memset(towerWidths, 0, sizeof(towerWidths));
  memset(towerXs, 0, sizeof(towerXs));
  towerWidths[0] = 50;
  towerXs[0] = (SCREEN_WIDTH - towerWidths[0]) / 2;
  towerLayerCount = 1;
  lastStackTick = millis();
}
void handleStackMode(int vry, int vrx, bool clicked) {
  if (gameOverStack) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(12, 10);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(12, 35);
    display.print(F("Layers: "));
    display.println(towerLayerCount - 1);
    display.setCursor(12, 50);
    display.println(F("[Click] return Menu"));
    return;
  }
  if (clicked) {
    currentState = STATE_GAMES_MENU;
    return;
  }
  bool joyMoved = (vrx < 1000 || vrx > 3000 || vry < 1000 || vry > 3000);
  if (joyMoved) {
    if (!joyMoveLatched && !stackIsFalling) {
      stackIsFalling = true;
      joyMoveLatched = true;
    }
  } else
    joyMoveLatched = false;

  if (millis() - lastStackTick > 25) {
    lastStackTick = millis();
    if (!stackIsFalling) {
      stackBlockX += stackBlockSpeedX;
      if (stackBlockX <= 0 || stackBlockX + stackBlockWidth >= SCREEN_WIDTH)
        stackBlockSpeedX = -stackBlockSpeedX;
    } else {
      stackBlockY += 3.5f;
      int targetTopY =
          58 - (towerLayerCount * STACK_BLOCK_HEIGHT) + cameraViewOffsetY;
      if (stackBlockY >= targetTopY) {
        stackBlockY = targetTopY;
        stackIsFalling = false;
        int baseLeft = towerXs[towerLayerCount - 1];
        int baseRight = baseLeft + towerWidths[towerLayerCount - 1];
        int curLeft = (int)stackBlockX;
        int curRight = curLeft + stackBlockWidth;
        int overlapLeft = max(baseLeft, curLeft);
        int overlapRight = min(baseRight, curRight);
        if (overlapRight > overlapLeft) {
          towerXs[towerLayerCount] = overlapLeft;
          towerWidths[towerLayerCount] = overlapRight - overlapLeft;
          stackBlockWidth = towerWidths[towerLayerCount];
          scoreStack += 10;
          towerLayerCount++;
          if (towerLayerCount >= MAX_STACK_LAYERS) {
            initStackGame();
            return;
          }
          int currentHighestY =
              58 - (towerLayerCount * STACK_BLOCK_HEIGHT) + cameraViewOffsetY;
          if (currentHighestY < 24)
            cameraViewOffsetY += STACK_BLOCK_HEIGHT;
          stackBlockX = random(0, SCREEN_WIDTH - stackBlockWidth);
          stackBlockY = 12.0f;
          stackBlockSpeedX = (stackBlockSpeedX > 0 ? 1.0f : -1.0f) *
                             (1.2f + (towerLayerCount * 0.12f));
          if (abs(stackBlockSpeedX) > 4.5f)
            stackBlockSpeedX = (stackBlockSpeedX > 0 ? 4.5f : -4.5f);
        } else
          gameOverStack = true;
      }
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("STACK: "));
  display.print(towerLayerCount - 1);
  display.setCursor(80, 0);
  display.print(F("S:"));
  display.print(scoreStack);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);
  for (int i = 0; i < towerLayerCount; i++) {
    int drawY = 58 - ((i + 1) * STACK_BLOCK_HEIGHT) + cameraViewOffsetY;
    if (drawY > 64 || drawY < 10)
      continue;
    display.fillRect(towerXs[i], drawY, towerWidths[i], STACK_BLOCK_HEIGHT - 1,
                     SSD1306_WHITE);
  }
  display.drawRect((int)stackBlockX, (int)stackBlockY, stackBlockWidth,
                   STACK_BLOCK_HEIGHT - 1, SSD1306_WHITE);
}

// ==========================================
// Game 7: Naval Battle
// ==========================================
static uint8_t navalBoardPlayer[8][8];
static uint8_t navalBoardEnemy[8][8];
static int8_t navalCx = 3, navalCy = 3;
static uint8_t navalWinner = 0;

void placeRandomShips(uint8_t board[8][8]) {
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
bool checkNavalWin(uint8_t board[8][8]) {
  for (int x = 0; x < 8; x++)
    for (int y = 0; y < 8; y++)
      if (board[x][y] == 1)
        return false;
  return true;
}
void navalAIMove() {
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

// ==========================================
// 游戏二级菜单中心驱动
// ==========================================
void handleGamesMenu(int vry, int vrx, bool clicked) {
  bool selectTriggered = clicked;
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      currentGamesSelect =
          (currentGamesSelect == 0) ? GAMES_TOTAL - 1 : currentGamesSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      currentGamesSelect = (currentGamesSelect + 1) % GAMES_TOTAL;
      lastJoyAction = millis();
    } else if (vrx < 1000) {
      currentState = STATE_MAIN_MENU;
      lastJoyAction = millis();
      return;
    } else if (vrx > 3000) {
      selectTriggered = true;
      lastJoyAction = millis();
    }
    if (currentGamesSelect < gamesScrollTop)
      gamesScrollTop = currentGamesSelect;
    else if (currentGamesSelect >= gamesScrollTop + VISIBLE_GAMES_ITEMS)
      gamesScrollTop = currentGamesSelect - VISIBLE_GAMES_ITEMS + 1;
  }

  if (selectTriggered) {
    switch (currentGamesSelect) {
    case 0:
      initSnakeGame();
      currentState = STATE_SNAKE;
      break;
    case 1:
      gomokuMenuSelect = 0;
      currentState = STATE_GOMOKU_MENU;
      break;
    case 2:
      init2048Game();
      currentState = STATE_2048;
      break;
    case 3:
      initDinoGame();
      currentState = STATE_DINO;
      break;
    case 4:
      initBrickGame();
      currentState = STATE_BRICK;
      break;
    case 5:
      initStackGame();
      currentState = STATE_STACK;
      break;
    case 6:
      initNavalGame();
      currentState = STATE_NAVAL_PLAY;
      break;
    case 7:
      currentState = STATE_MAIN_MENU;
      break;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("=== GAME CENTER ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  for (int i = 0; i < VISIBLE_GAMES_ITEMS; i++) {
    int itemIdx = gamesScrollTop + i;
    if (itemIdx >= GAMES_TOTAL)
      break;
    int yPos = 14 + (i * 12);
    display.setCursor(5, yPos);
    if (itemIdx == currentGamesSelect)
      display.print(F("> "));
    else
      display.print(F("  "));
    display.println(gameMenuItems[itemIdx]);
  }
  int barX = 124, barY = 14, barHeight = 34;
  display.drawFastVLine(barX + 1, barY, barHeight, SSD1306_WHITE);
  int thumbHeight = barHeight * VISIBLE_GAMES_ITEMS / GAMES_TOTAL;
  int thumbY = barY + ((barHeight - thumbHeight) * currentGamesSelect /
                       (GAMES_TOTAL - 1));
  display.fillRect(barX, thumbY, 3, thumbHeight, SSD1306_WHITE);
  display.drawFastHLine(0, 51, 128, SSD1306_WHITE);
  display.setCursor(2, 55);
  display.print(F("Back to Menu -> [Back]"));
}
