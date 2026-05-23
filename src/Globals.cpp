#include "Config.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
ThreeWire myWire(RTC_DAT, RTC_CLK, RTC_RST);
RtcDS1302<ThreeWire> Rtc(myWire);

SystemState currentState = STATE_CLOCK;
unsigned long lastActivityTime = 0;
unsigned long lastButtonPress = 0;
unsigned long lastJoyAction = 0;
const int DEBOUNCE_DELAY = 250;
const int JOY_DELAY = 200;

int currentMenuSelect = 0;
int menuScrollTop = 0;
const char *menuItems[] = {"1. Realtime Clock", "2. Time Tools",
                           "3. Arcade Games",   "4. Calculator",
                           "5. Desktop Pet",    "6. Adjust Settings"};
const int MENU_TOTAL = 6;
const int VISIBLE_ITEMS = 3;
bool petEnteredViaTimeout = false;

int getMaxDay(int y, int m) {
  if (m == 2) {
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))
      return 29;
    return 28;
  }
  if (m == 4 || m == 6 || m == 9 || m == 11)
    return 30;
  return 31;
}
