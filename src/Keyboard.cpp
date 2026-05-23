#include "Keyboard.h"
#include "Config.h"

static const char *kbTitle = "";
static char *kbTargetBuf = nullptr;
static int kbMaxLen = 0;
static void (*kbConfirmCallback)() = nullptr;

static int kbCx = 0, kbCy = 0; // 键盘矩阵光标
static int kbPage = 0;         // 0: 小写, 1: 大写, 2: 数字符号

// 4行8列页矩阵布局定义 ('<'退格, '^'切页, '\n'确认单元)
static const char kbLayout[3][4][8] = {
    {{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'},
     {'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'},
     {'q', 'r', 's', 't', 'u', 'v', 'w', 'x'},
     {'y', 'z', ' ', '<', '^', '\n', '\n', '\n'}},
    {{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'},
     {'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P'},
     {'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X'},
     {'Y', 'Z', ' ', '<', '^', '\n', '\n', '\n'}},
    {{'1', '2', '3', '4', '5', '6', '7', '8'},
     {'9', '0', '-', '=', '[', ']', ';', '\''},
     {',', '.', '/', '\\', '`', '~', '?', '!'},
     {'@', '#', '$', '<', '^', '\n', '\n', '\n'}}};

void initGlobalKeyboard(const char *title, char *targetBuffer, int maxLen,
                        void (*onConfirm)()) {
  kbTitle = title;
  kbTargetBuf = targetBuffer;
  kbMaxLen = maxLen;
  kbConfirmCallback = onConfirm;
  kbCx = 0;
  kbCy = 0;
  kbPage = 0;
}

void handleGlobalKeyboard(int vry, int vrx, bool clicked) {
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      kbCy = (kbCy == 0) ? 3 : kbCy - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      kbCy = (kbCy == 3) ? 0 : kbCy + 1;
      lastJoyAction = millis();
    }
    if (vrx < 1000) {
      kbCx = (kbCx == 0) ? 7 : kbCx - 1;
      lastJoyAction = millis();
    } else if (vrx > 3000) {
      kbCx = (kbCx == 7) ? 0 : kbCx + 1;
      lastJoyAction = millis();
    }
  }

  if (clicked) {
    char key = kbLayout[kbPage][kbCy][kbCx];
    int len = strlen(kbTargetBuf);

    if (key == '\n') { // OK 确认键
      if (kbConfirmCallback != nullptr)
        kbConfirmCallback();
      return;
    } else if (key == '<') { // Backspace
      if (len > 0)
        kbTargetBuf[len - 1] = '\0';
    } else if (key == '^') { // Switch Page
      kbPage = (kbPage + 1) % 3;
    } else { // 普通字符键入
      if (len < kbMaxLen - 1) {
        kbTargetBuf[len] = key;
        kbTargetBuf[len + 1] = '\0';
      }
    }
  }

  // 开始渲染键盘 UI
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(kbTitle);

  // 绘制当前输入框状态底线
  display.setCursor(2, 11);
  display.print(kbTargetBuf);
  display.print(F("_"));
  display.drawFastHLine(0, 21, 128, SSD1306_WHITE);

  // 渲染 4x8 按键小网格
  int startX = 4, startY = 24, cellW = 15, cellH = 10;
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 8; c++) {
      char key = kbLayout[kbPage][r][c];
      int x = startX + c * cellW;
      int y = startY + r * cellH;

      // 如果碰到了连体 '\n' (OK键), 只有第一个网格负责绘制大块高亮
      if (key == '\n' && c > 5)
        continue;

      bool isSelected = (r == kbCy && c == kbCx);
      // 特殊处理：如果光标停在OK区域任何一格，高亮整块OK区域
      if (key == '\n' && kbCy == r && kbCx >= 5)
        isSelected = true;

      int drawW = (key == '\n') ? cellW * 3 : cellW - 1;

      if (isSelected) {
        display.fillRect(x, y, drawW, cellH - 1, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }

      display.setCursor(x + 4, y + 1);
      if (key == '<')
        display.print(F("<"));
      else if (key == '^') {
        if (kbPage == 0)
          display.print(F("A"));
        else if (kbPage == 1)
          display.print(F("#"));
        else
          display.print(F("a"));
      } else if (key == '\n') {
        display.setCursor(x + 14, y + 1);
        display.print(F("OK"));
      } else
        display.print(key);
    }
  }
  display.setTextColor(SSD1306_WHITE);
}
