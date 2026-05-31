#include "WiFiModule.h"
#include "Config.h"
#include "Keyboard.h"
#include "Options.h"
#include <WiFi.h>
#include <time.h>

static const char *wifiMenuItems[] = {"1. Scan Networks", "2. Add Hidden WiFi",
                                      "3. Disconnect WiFi",
                                      "4. Manual NTP Sync", "5. < Back"};
static const int WIFI_TOTAL = 5;
static const int VISIBLE_WIFI_ITEMS = 4;
static int currentWiFiSelect = 0;
static int wifiScrollTop = 0;

static char wifiSSID[33] = "";
static char wifiPassword[65] = "";
static bool isHiddenMode = false;

static int scanResultCount = -1;
static int scanScrollTop = 0;
static int scanSelectIdx = 0;
static bool scanInProgress = false;

static unsigned long connectStartTime = 0;

static void onPasswordInputConfirm() {
  currentState = STATE_WIFI_CONNECTING;
  connectStartTime = millis();
  WiFi.disconnect(true);
  WiFi.begin(wifiSSID, wifiPassword);
}

static void onHiddenSSIDConfirm() {
  wifiPassword[0] = '\0';
  initGlobalKeyboard("Enter Hidden Pass:", wifiPassword, 65,
                     onPasswordInputConfirm);
  currentState = STATE_WIFI_KEYBOARD;
}

void handleWiFiMenu(int vry, int vrx, bool clicked) {
  bool selectTriggered = clicked;
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      currentWiFiSelect =
          (currentWiFiSelect == 0) ? WIFI_TOTAL - 1 : currentWiFiSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      currentWiFiSelect = (currentWiFiSelect + 1) % WIFI_TOTAL;
      lastJoyAction = millis();
    } else if (vrx < 1000) {
      currentState = STATE_MAIN_MENU;
      lastJoyAction = millis();
      return;
    } else if (vrx > 3000) {
      selectTriggered = true;
      lastJoyAction = millis();
    }

    // 平滑视口上下翻页滚动计算
    if (currentWiFiSelect < wifiScrollTop)
      wifiScrollTop = currentWiFiSelect;
    else if (currentWiFiSelect >= wifiScrollTop + VISIBLE_WIFI_ITEMS)
      wifiScrollTop = currentWiFiSelect - VISIBLE_WIFI_ITEMS + 1;
  }

  if (selectTriggered) {
    if (currentWiFiSelect == 0) {
      scanResultCount = -1;
      scanScrollTop = 0;
      scanSelectIdx = 0;
      scanInProgress = false;
      isHiddenMode = false;
      currentState = STATE_WIFI_SCAN;
    } else if (currentWiFiSelect == 1) {
      isHiddenMode = true;
      wifiSSID[0] = '\0';
      initGlobalKeyboard("Enter Hidden SSID:", wifiSSID, 33,
                         onHiddenSSIDConfirm);
      currentState = STATE_WIFI_KEYBOARD;
    } else if (currentWiFiSelect == 2) {
      // 3. 断开当前 WiFi 连接
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      display.clearDisplay();
      display.setCursor(15, 25);
      display.print(F("WiFi Disconnected"));
      display.display();
      delay(1200);
    } else if (currentWiFiSelect == 3) {
      // 4. 手动触发实时 NTP 校时
      display.clearDisplay();
      display.setCursor(10, 15);
      display.print(F("Syncing NTP..."));
      display.display();
      if (WiFi.status() == WL_CONNECTED) {
        configTime(GMT_OFFSET * 3600, 0, "ntp.ntsc.ac.cn", "pool.ntp.org");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 4000)) {
          RtcDateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                              timeinfo.tm_mday, timeinfo.tm_hour,
                              timeinfo.tm_min, timeinfo.tm_sec);
          Rtc.SetDateTime(ntpTime);
          display.setCursor(10, 35);
          display.print(F("Sync Success OK!"));
        } else {
          display.setCursor(10, 35);
          display.print(F("NTP Sync Timeout"));
        }
      } else {
        display.setCursor(10, 35);
        display.print(F("Error: No Network"));
      }
      display.display();
      delay(1500);
    } else {
      currentState = STATE_MAIN_MENU;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  // 如果已联网，动态在头部实时渲染展示当前 SSID 状态
  if (WiFi.status() == WL_CONNECTED) {
    display.print(F("SSID: "));
    String showSSID = WiFi.SSID();
    if (showSSID.length() > 14)
      showSSID = showSSID.substring(0, 11) + "...";
    display.println(showSSID);
  } else {
    display.println(F("=== WIFI MANAGER ==="));
  }
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // 视口动态范围画出菜单项
  for (int i = 0; i < VISIBLE_WIFI_ITEMS; i++) {
    int itemIdx = wifiScrollTop + i;
    if (itemIdx >= WIFI_TOTAL)
      break;
    display.setCursor(5, 14 + (i * 12));
    if (itemIdx == currentWiFiSelect)
      display.print(F("> "));
    else
      display.print(F("  "));
    display.println(wifiMenuItems[itemIdx]);
  }

  int barX = 124, barY = 12, barHeight = 48;
  display.drawFastVLine(barX + 1, barY, barHeight, SSD1306_WHITE);
  int thumbHeight = barHeight * VISIBLE_WIFI_ITEMS / WIFI_TOTAL;
  int thumbY =
      barY + ((barHeight - thumbHeight) * currentWiFiSelect / (WIFI_TOTAL - 1));
  display.fillRect(barX, thumbY, 3, thumbHeight, SSD1306_WHITE);
}

void handleWiFiScan(int vry, int vrx, bool clicked) {
  if (!scanInProgress && scanResultCount == -1) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.scanNetworks(true);
    scanInProgress = true;
  }

  if (scanInProgress) {
    int16_t status = WiFi.scanComplete();
    if (status >= 0) {
      scanResultCount = status;
      scanInProgress = false;
    } else {
      display.clearDisplay();
      display.setCursor(10, 20);
      display.print(F("Scanning WiFi..."));
      display.fillRect(10, 36, (millis() / 10) % 100, 4, SSD1306_WHITE);
      return;
    }
  }

  if (scanResultCount == 0) {
    if (clicked || (millis() - lastJoyAction > JOY_DELAY && vrx < 1000)) {
      currentState = STATE_WIFI_MENU;
      lastJoyAction = millis();
    }
    display.clearDisplay();
    display.setCursor(10, 20);
    display.print(F("No Networks Found."));
    display.setCursor(10, 40);
    display.print(F("[Back] to return"));
    return;
  }

  bool actionTriggered = false;
  if (clicked) {
    actionTriggered = true;
  }

  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      scanSelectIdx =
          (scanSelectIdx == 0) ? scanResultCount - 1 : scanSelectIdx - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      scanSelectIdx = (scanSelectIdx + 1) % scanResultCount;
      lastJoyAction = millis();
    } else if (vrx < 1000) {
      currentState = STATE_WIFI_MENU;
      lastJoyAction = millis();
      return;
    } else if (vrx > 3000) {
      actionTriggered = true;
      lastJoyAction = millis();
    }

    if (scanSelectIdx < scanScrollTop)
      scanScrollTop = scanSelectIdx;
    else if (scanSelectIdx >= scanScrollTop + 3)
      scanScrollTop = scanSelectIdx - 3 + 1;
  }

  if (actionTriggered) {
    strncpy(wifiSSID, WiFi.SSID(scanSelectIdx).c_str(), 32);
    wifiPassword[0] = '\0';

    if (WiFi.encryptionType(scanSelectIdx) == WIFI_AUTH_OPEN) {
      currentState = STATE_WIFI_CONNECTING;
      connectStartTime = millis();
      WiFi.disconnect(true);
      WiFi.begin(wifiSSID);
    } else {
      initGlobalKeyboard("Enter Password:", wifiPassword, 65,
                         onPasswordInputConfirm);
      currentState = STATE_WIFI_KEYBOARD;
    }
    return;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(F("Select Network ("));
  display.print(scanResultCount);
  display.println(F("):"));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  for (int i = 0; i < 3; i++) {
    int curIdx = scanScrollTop + i;
    if (curIdx >= scanResultCount)
      break;
    int yPos = 14 + (i * 12);
    display.setCursor(5, yPos);
    if (curIdx == scanSelectIdx)
      display.print(F("> "));
    else
      display.print(F("  "));

    String name = WiFi.SSID(curIdx);
    bool isOpen = (WiFi.encryptionType(curIdx) == WIFI_AUTH_OPEN);

    int maxDisplayLen = isOpen ? 8 : 12;
    if (name.length() > maxDisplayLen)
      name = name.substring(0, maxDisplayLen - 1) + "~";
    display.print(name);
    if (isOpen) {
      display.print(F(" [O]"));
    }

    display.setCursor(102, yPos);
    display.print(WiFi.RSSI(curIdx));
  }
}

void handleWiFiConnectingLoop() {
  display.clearDisplay();
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    display.setCursor(10, 6);
    display.print(F("CONNECTED SUCCESS!"));
    display.setCursor(10, 18);
    display.print(F("IP:"));
    display.print(WiFi.localIP());

    static bool ntpExecuted = false;
    if (!ntpExecuted) {
      display.setCursor(10, 32);
      display.print(F("NTP Syncing..."));
      display.display();
      configTime(8 * 3600, 0, "ntp.ntsc.ac.cn", "pool.ntp.org");
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 5000)) {
        RtcDateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                            timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
                            timeinfo.tm_sec);
        Rtc.SetDateTime(ntpTime);
        display.setCursor(10, 44);
        display.print(F("RTC Synced OK!"));
      } else {
        display.setCursor(10, 44);
        display.print(F("NTP Timeout!"));
      }
      ntpExecuted = true;
    }
    display.setCursor(10, 56);
    display.print(F("[Back] to Exit"));
  } else if (status == WL_CONNECT_FAILED ||
             (millis() - connectStartTime > 15000)) {
    display.setCursor(10, 20);
    display.print(F("CONNECTION FAILED"));
    display.setCursor(10, 40);
    display.print(F("[Back] to retry"));
  } else {
    display.setCursor(10, 15);
    display.print(F("Connecting to:"));
    display.setCursor(10, 27);
    display.print(wifiSSID);
    display.drawRect(10, 44, 100, 6, SSD1306_WHITE);
    int progress = map((millis() - connectStartTime) % 3000, 0, 3000, 0, 96);
    display.fillRect(12, 46, progress, 2, SSD1306_WHITE);
  }
}
