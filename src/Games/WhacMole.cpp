#include "Config.h"
#include <cstdint>

struct MoleHole {
  int16_t x, y;
};

static const MoleHole holes[8] = {
    {34, 20}, {64, 18}, {94, 20}, // 0, 1, 2
    {30, 38}, {98, 38},           // 3, 4
    {34, 56}, {64, 58}, {94, 56}  // 5, 6, 7
};

static int8_t activeMoleHole = -1;
static unsigned long moleSpawnTime = 0;
static unsigned long moleDuration = 1200;
static int whacScore = 0;
static int whacLives = 3;
static bool whacGameOver = false;

static int8_t lastJoyDir = -1;
static int8_t playerHammerPos = -1;

static unsigned long moleEmptyStartTime = 0;
static unsigned long moleEmptyDuration = 200;
static bool isWaitingForMole = false;

static int8_t getWhacDirection(int vrx, int vry) {
  bool left = (vrx < 1200);
  bool right = (vrx > 2800);
  bool up = (vry < 1200);
  bool down = (vry > 2800);

  if (up && left)
    return 0;
  if (up && right)
    return 2;
  if (down && left)
    return 5;
  if (down && right)
    return 7;
  if (up)
    return 1;
  if (left)
    return 3;
  if (right)
    return 4;
  if (down)
    return 6;

  return -1;
}

void initWhacGame() {
  whacScore = 0;
  whacLives = 3;
  whacGameOver = false;
  lastJoyDir = -1;
  activeMoleHole = -1;
  playerHammerPos = -1;
  moleDuration = 1200;
  isWaitingForMole = false;
}

void handleWhacGameMode(int vry, int vrx, bool clicked) {
  if (whacGameOver) {
    if (clicked)
      currentState = STATE_GAMES_MENU;
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(15, 14);
    display.println(F("GAME OVER"));
    display.setTextSize(1);
    display.setCursor(15, 38);
    display.print(F("Final Score: "));
    display.print(whacScore);
    display.setCursor(15, 51);
    display.println(F("[Click] Return"));
    return;
  }

  unsigned long now = millis();
  int8_t currentDir = getWhacDirection(vrx, vry);
  playerHammerPos = currentDir;

  // 1. 地鼠生成与生死逻辑（加入出生空档期）
  if (activeMoleHole == -1) {
    if (!isWaitingForMole) {
      // 刚击中或刚漏掉，触发空档等待
      isWaitingForMole = true;
      moleEmptyStartTime = now;
      moleEmptyDuration = random(200, 500); // 随机 200~500ms 的无地鼠安全期
    }

    if (now - moleEmptyStartTime > moleEmptyDuration) {
      // 空档期结束，真正生成地鼠
      activeMoleHole = random(0, 8);
      moleSpawnTime = now;
      moleDuration = 1200 - (whacScore * 40);
      if (moleDuration < 450)
        moleDuration = 450;
      isWaitingForMole = false;
    }
  } else {
    // 地鼠超时未被打中，溜走
    if (now - moleSpawnTime > moleDuration) {
      activeMoleHole = -1;
      whacLives--;
      if (whacLives <= 0) {
        whacGameOver = true;
        return;
      }
    }
  }

  // 2. 【核心修改】改进型摇杆打击判定
  // 只要当前方向有效，且“不等于上一帧的方向”，就视作一次全新的有力挥锤！
  if (currentDir != -1 && currentDir != lastJoyDir) {
    if (currentDir == activeMoleHole) {
      whacScore++;
      activeMoleHole = -1; // 瞬间击中，转入空档期
    }
  }
  lastJoyDir = currentDir; // 更新方向历史

  // 3. 图形渲染层
  display.clearDisplay();

  // 顶部状态栏
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(F("WHAC-A-MOLE"));
  display.setCursor(75, 0);
  display.print(F("S:"));
  display.print(whacScore);
  display.print(F(" L:"));
  display.print(whacLives);
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  // 绘制 8 个洞口
  for (int i = 0; i < 8; i++) {
    display.drawRoundRect(holes[i].x - 8, holes[i].y - 2, 16, 5, 2,
                          SSD1306_WHITE);

    // 绘制地鼠
    if (i == activeMoleHole) {
      display.fillCircle(holes[i].x, holes[i].y - 4, 4, SSD1306_WHITE);
      display.fillRect(holes[i].x - 2, holes[i].y - 2, 5, 3, SSD1306_WHITE);
      display.drawPixel(holes[i].x - 1, holes[i].y - 5, SSD1306_BLACK);
      display.drawPixel(holes[i].x + 1, holes[i].y - 5, SSD1306_BLACK);
    }

    // 绘制准星
    if (i == playerHammerPos) {
      display.drawCircle(holes[i].x, holes[i].y - 3, 7, SSD1306_WHITE);
      display.drawFastHLine(holes[i].x - 9, holes[i].y - 3, 3, SSD1306_WHITE);
      display.drawFastHLine(holes[i].x + 7, holes[i].y - 3, 3, SSD1306_WHITE);
    }
  }

  // 中央装饰原点
  display.drawCircle(64, 38, 2, SSD1306_WHITE);

  // 注：如果主 loop 没有统一调用 display.display()，请在此处加上：
  // display.display();
}
