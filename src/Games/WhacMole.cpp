#include "Config.h"
#include <cstdint>

struct MoleHole {
  int16_t x, y;
};

// 围绕中央建立 8 方向九宫格空间坐标 (跳过正中央 [1,1])
static const MoleHole holes[8] = {
    {34, 20}, // 0: 左上 (Top-Left)
    {64, 18}, // 1: 正上 (Top)
    {94, 20}, // 2: 右上 (Top-Right)
    {30, 38}, // 3: 正左 (Left)
    {98, 38}, // 4: 正右 (Right)
    {34, 56}, // 5: 左下 (Bottom-Left)
    {64, 58}, // 6: 正下 (Bottom)
    {94, 56}  // 7: 右下 (Bottom-Right)
};

static int8_t activeMoleHole = -1;
static unsigned long moleSpawnTime = 0;
static unsigned long moleDuration = 1200; // 地鼠在洞口停留基础时间(ms)
static int whacScore = 0;
static int whacLives = 3;
static bool whacGameOver = false;
static bool whacJoyReleased = true;
static int8_t playerHammerPos = -1; // 玩家当前摇杆指向的目标洞口

// 辅助工具函数：把摇杆模拟量精确解算映射至 8 个孔洞索引上
static int8_t getWhacDirection(int vrx, int vry) {
  bool left = (vrx < 1200);
  bool right = (vrx > 2800);
  bool up = (vry < 1200);
  bool down = (vry > 2800);

  if (up && left)
    return 0; // 左上
  if (up && right)
    return 2; // 右上
  if (down && left)
    return 5; // 左下
  if (down && right)
    return 7; // 右下
  if (up)
    return 1; // 正上
  if (left)
    return 3; // 正左
  if (right)
    return 4; // 正右
  if (down)
    return 6; // 正下

  return -1; // 摇杆居中空闲
}

void initWhacGame() {
  whacScore = 0;
  whacLives = 3;
  whacGameOver = false;
  whacJoyReleased = true;
  activeMoleHole = -1;
  playerHammerPos = -1;
  moleDuration = 1200;
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

  // 1. 无地鼠时生成新地鼠
  if (activeMoleHole == -1) {
    activeMoleHole = random(0, 8);
    moleSpawnTime = now;
    // 动态难度：随分数提高加快缩回速度，最低保留 450ms 窗口期
    moleDuration = 1200 - (whacScore * 40);
    if (moleDuration < 450)
      moleDuration = 450;
  } else {
    // 2. 地鼠时间结束未被打中，从洞口溜走 (漏地鼠扣血)
    if (now - moleSpawnTime > moleDuration) {
      activeMoleHole = -1;
      whacLives--;
      if (whacLives <= 0) {
        whacGameOver = true;
        return;
      }
    }
  }

  // 3. 摇杆打击逻辑判定
  int8_t currentDir = getWhacDirection(vrx, vry);
  playerHammerPos = currentDir; // 用于准星同步渲染

  if (currentDir != -1) {
    if (whacJoyReleased) {
      whacJoyReleased = false; // 锁定，必须回正才能敲击下一次

      // 精确切中探头的地鼠
      if (currentDir == activeMoleHole) {
        whacScore++;
        activeMoleHole = -1; // 击中，立刻刷新消失
      }
    }
  } else {
    whacJoyReleased = true; // 摇杆回正复位
  }

  // 4. 图形渲染层
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

  // 遍历绘制 8 个地鼠洞及内容
  for (int i = 0; i < 8; i++) {
    // 绘制基本地鼠洞口椭圆基底
    display.drawRoundRect(holes[i].x - 8, holes[i].y - 2, 16, 5, 2,
                          SSD1306_WHITE);

    // 如果该洞当前有地鼠冒出来
    if (i == activeMoleHole) {
      // 绘制冒出的精细地鼠实体（头部小实心圆加五官）
      display.fillCircle(holes[i].x, holes[i].y - 4, 4, SSD1306_WHITE);
      display.fillRect(holes[i].x - 2, holes[i].y - 2, 5, 3,
                       SSD1306_WHITE); // 身体
      display.drawPixel(holes[i].x - 1, holes[i].y - 5,
                        SSD1306_BLACK); // 眼睛 L
      display.drawPixel(holes[i].x + 1, holes[i].y - 5,
                        SSD1306_BLACK); // 眼睛 R
    }

    // 绘制玩家当前摇杆指向的打击准星/框（如果指到该方向）
    if (i == playerHammerPos) {
      display.drawCircle(holes[i].x, holes[i].y - 3, 7, SSD1306_WHITE);
      // 附加两根细线表示准心十字线
      display.drawFastHLine(holes[i].x - 9, holes[i].y - 3, 3, SSD1306_WHITE);
      display.drawFastHLine(holes[i].x + 7, holes[i].y - 3, 3, SSD1306_WHITE);
    }
  }

  // 在中央空地上绘制装饰性质的摇杆状态原点
  display.drawCircle(64, 38, 2, SSD1306_WHITE);
}
