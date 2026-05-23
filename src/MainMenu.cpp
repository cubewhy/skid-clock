#include "MainMenu.h"
#include "Config.h"
#include "Pet.h"
#include "TimeTools.h"

void handleMainMenu(int vry, int vrx, bool clicked) {
  bool selectTriggered = clicked;

  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      currentMenuSelect =
          (currentMenuSelect == 0) ? MENU_TOTAL - 1 : currentMenuSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      currentMenuSelect = (currentMenuSelect + 1) % MENU_TOTAL;
      lastJoyAction = millis();
    } else if (vrx < 1000) {
      currentState = STATE_CLOCK;
      lastJoyAction = millis();
      return;
    } else if (vrx > 3000) {
      selectTriggered = true;
      lastJoyAction = millis();
    }

    if (currentMenuSelect < menuScrollTop)
      menuScrollTop = currentMenuSelect;
    else if (currentMenuSelect >= menuScrollTop + VISIBLE_ITEMS)
      menuScrollTop = currentMenuSelect - VISIBLE_ITEMS + 1;
  }

  if (selectTriggered) {
    switch (currentMenuSelect) {
    case 0:
      currentState = STATE_CLOCK;
      break;
    case 1:
      currentState = STATE_TIMERS_MENU;
      break;
    case 2:
      currentState = STATE_GAMES_MENU;
      break;
    case 3:
      currentState = STATE_CALCULATOR;
      break;
    case 4:
      petEnteredViaTimeout = false;
      initPet();
      currentState = STATE_PET;
      break;
    case 5:
      initSettingsMode();
      currentState = STATE_SETTINGS;
      break;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("=== SYSTEM MENU ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  for (int i = 0; i < VISIBLE_ITEMS; i++) {
    int itemIdx = menuScrollTop + i;
    if (itemIdx >= MENU_TOTAL)
      break;
    int yPos = 14 + (i * 12);
    display.setCursor(5, yPos);
    if (itemIdx == currentMenuSelect)
      display.print(F("> "));
    else
      display.print(F("  "));
    display.println(menuItems[itemIdx]);
  }

  int barX = 124, barY = 14, barHeight = 34;
  display.drawFastVLine(barX + 1, barY, barHeight, SSD1306_WHITE);
  int thumbHeight = barHeight * VISIBLE_ITEMS / MENU_TOTAL;
  int thumbY =
      barY + ((barHeight - thumbHeight) * currentMenuSelect / (MENU_TOTAL - 1));
  display.fillRect(barX, thumbY, 3, thumbHeight, SSD1306_WHITE);
  display.drawFastHLine(0, 51, 128, SSD1306_WHITE);

  static int scrollX = 128; // 从屏幕最右侧外部开始
  static unsigned long lastScrollTime = 0;

  // 每 35 毫秒向左平移 1 像素
  if (millis() - lastScrollTime > 35) {
    scrollX--;
    // 当完全滑出左侧边界（字符宽约6px * 18字 = 108px）时重置到右侧
    if (scrollX < -110) {
      scrollX = 128;
    }
    lastScrollTime = millis();
  }

  display.setTextWrap(false);
  display.setCursor(scrollX, 55);
  display.print(GITHUB_URL);
  display.setTextWrap(true);
}
