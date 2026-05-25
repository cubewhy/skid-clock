#include "TimeTools.h"
#include "Config.h"

// 时间工具菜单内部变量
const char *timerMenuItems[] = {"1. Stopwatch", "2. Countdown", "3. Pomodoro",
                                "4. < Back"};
const int TIMERS_TOTAL = 4;
int currentTimersSelect = 0;

// 秒表变量
unsigned long stopwatchElapsed = 0;
unsigned long stopwatchLastTime = 0;
bool stopwatchRunning = false;

// 倒计时变量
long countdownTotalSec = 300;
unsigned long countdownStartTime = 0;
long countdownRemainingSec = 300;
bool countdownRunning = false;
bool countdownSettingMode = true;
int countdownEditField = 0;

// 番茄钟变量
PomoState pomoState = POMO_PAUSE;
PomoState pomoPrevState = POMO_WORK;
long pomoRemainingSec = 1500;
unsigned long pomoLastTick = 0;
int pomoCompletedCycles = 0;
int pomoConfigWorkMin = 25;

// 调时寄存变量
static EditField currentEditField = FIELD_YEAR;
static int editYear = 2026, editMonth = 1, editDay = 1;
static int editHour = 0, editMinute = 0, editSecond = 0;
static unsigned long lastSettingsTick = 0;

void initSettingsMode() {
  RtcDateTime nowSettings = Rtc.GetDateTime();
  editYear = nowSettings.Year();
  editMonth = nowSettings.Month();
  editDay = nowSettings.Day();
  editHour = nowSettings.Hour();
  editMinute = nowSettings.Minute();
  editSecond = nowSettings.Second();
  currentEditField = FIELD_HOUR;
  lastSettingsTick = millis();
}

void handleClockMode(bool clicked) {
  if (clicked) {
    currentState = STATE_MAIN_MENU;
    return;
  }
  RtcDateTime now = Rtc.GetDateTime();
  display.clearDisplay();
  char dateStr[12];
  sprintf(dateStr, "%04d-%02d-%02d", now.Year(), now.Month(), now.Day());
  display.setTextSize(1);
  display.setCursor(34, 10);
  display.println(dateStr);
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", now.Hour(), now.Minute(), now.Second());
  display.setTextSize(2);
  display.setCursor(16, 26);
  display.println(timeStr);
  display.setTextSize(1);
  display.setCursor(19, 52);
  display.print(F("[Click] to Menu"));
}

void handleSettingsMode(int vry, int vrx, bool clicked) {
  unsigned long nowMs = millis();
  if (nowMs - lastSettingsTick >= 1000) {
    int passedSeconds = (nowMs - lastSettingsTick) / 1000;
    lastSettingsTick += passedSeconds * 1000;

    editSecond += passedSeconds;
    if (editSecond >= 60) {
      editMinute += editSecond / 60;
      editSecond %= 60;
      if (editMinute >= 60) {
        editHour += editMinute / 60;
        editMinute %= 60;
        if (editHour >= 24) {
          editDay += editHour / 24;
          editHour %= 24;
          int maxD = getMaxDay(editYear, editMonth);
          while (editDay > maxD) {
            editDay -= maxD;
            editMonth++;
            if (editMonth > 12) {
              editYear++;
              editMonth = 1;
            }
            maxD = getMaxDay(editYear, editMonth);
          }
        }
      }
    }
  }

  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vrx < 1000) {
      if (currentEditField == FIELD_MONTH)
        currentEditField = FIELD_YEAR;
      else if (currentEditField == FIELD_DAY)
        currentEditField = FIELD_MONTH;
      else if (currentEditField == FIELD_HOUR)
        currentEditField = FIELD_DAY;
      else if (currentEditField == FIELD_MINUTE)
        currentEditField = FIELD_HOUR;
      else if (currentEditField == FIELD_SECOND)
        currentEditField = FIELD_MINUTE;
      lastJoyAction = millis();
    } else if (vrx > 3000) {
      if (currentEditField == FIELD_YEAR)
        currentEditField = FIELD_MONTH;
      else if (currentEditField == FIELD_MONTH)
        currentEditField = FIELD_DAY;
      else if (currentEditField == FIELD_DAY)
        currentEditField = FIELD_HOUR;
      else if (currentEditField == FIELD_HOUR)
        currentEditField = FIELD_MINUTE;
      else if (currentEditField == FIELD_MINUTE)
        currentEditField = FIELD_SECOND;
      lastJoyAction = millis();
    }

    if (vry < 1000) {
      if (currentEditField == FIELD_YEAR)
        editYear++;
      else if (currentEditField == FIELD_MONTH) {
        editMonth++;
        if (editMonth > 12)
          editMonth = 1;
      } else if (currentEditField == FIELD_DAY) {
        editDay++;
        if (editDay > getMaxDay(editYear, editMonth))
          editDay = 1;
      } else if (currentEditField == FIELD_HOUR) {
        editHour++;
        if (editHour > 23)
          editHour = 0;
      } else if (currentEditField == FIELD_MINUTE) {
        editMinute++;
        if (editMinute > 59)
          editMinute = 0;
      } else if (currentEditField == FIELD_SECOND) {
        editSecond++;
        if (editSecond > 59)
          editSecond = 0;
      }
      lastJoyAction = millis();
    } else if (vry > 3000) {
      if (currentEditField == FIELD_YEAR)
        editYear--;
      else if (currentEditField == FIELD_MONTH) {
        editMonth--;
        if (editMonth < 1)
          editMonth = 12;
      } else if (currentEditField == FIELD_DAY) {
        editDay--;
        if (editDay < 1)
          editDay = getMaxDay(editYear, editMonth);
      } else if (currentEditField == FIELD_HOUR) {
        editHour--;
        if (editHour < 0)
          editHour = 23;
      } else if (currentEditField == FIELD_MINUTE) {
        editMinute--;
        if (editMinute < 0)
          editMinute = 59;
      } else if (currentEditField == FIELD_SECOND) {
        editSecond--;
        if (editSecond < 0)
          editSecond = 59;
      }
      lastJoyAction = millis();
    }

    int maxDaysInCurrentMonth = getMaxDay(editYear, editMonth);
    if (editDay > maxDaysInCurrentMonth) {
      editDay = maxDaysInCurrentMonth;
    }
  }

  if (clicked) {
    RtcDateTime updated(editYear, editMonth, editDay, editHour, editMinute,
                        editSecond);
    Rtc.SetDateTime(updated);
    currentState = STATE_MAIN_MENU;
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("[EDIT] X:Move  Y:+-"));
  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  char dateBuf[22];
  char timeBuf[22];
  sprintf(dateBuf, "DATE: %04d-%02d-%02d", editYear, editMonth, editDay);
  sprintf(timeBuf, "TIME: %02d:%02d:%02d", editHour, editMinute, editSecond);

  display.setCursor(10, 22);
  display.print(dateBuf);
  display.setCursor(10, 38);
  display.print(timeBuf);

  if (currentEditField == FIELD_YEAR)
    display.fillRect(46, 31, 24, 2, SSD1306_WHITE);
  else if (currentEditField == FIELD_MONTH)
    display.fillRect(76, 31, 12, 2, SSD1306_WHITE);
  else if (currentEditField == FIELD_DAY)
    display.fillRect(94, 31, 12, 2, SSD1306_WHITE);
  else if (currentEditField == FIELD_HOUR)
    display.fillRect(46, 47, 12, 2, SSD1306_WHITE);
  else if (currentEditField == FIELD_MINUTE)
    display.fillRect(64, 47, 12, 2, SSD1306_WHITE);
  else if (currentEditField == FIELD_SECOND)
    display.fillRect(82, 47, 12, 2, SSD1306_WHITE);
}

void handleTimersMenu(int vry, int vrx, bool clicked) {
  bool selectTriggered = clicked;
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      currentTimersSelect = (currentTimersSelect == 0)
                                ? TIMERS_TOTAL - 1
                                : currentTimersSelect - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      currentTimersSelect = (currentTimersSelect + 1) % TIMERS_TOTAL;
      lastJoyAction = millis();
    } else if (vrx < 1000) {
      currentState = STATE_MAIN_MENU;
      lastJoyAction = millis();
      return;
    } else if (vrx > 3000) {
      selectTriggered = true;
      lastJoyAction = millis();
    }
  }

  if (selectTriggered) {
    switch (currentTimersSelect) {
    case 0:
      stopwatchElapsed = 0;
      stopwatchRunning = false;
      currentState = STATE_STOPWATCH;
      break;
    case 1:
      countdownRunning = false;
      countdownSettingMode = true;
      countdownEditField = 0;
      currentState = STATE_COUNTDOWN;
      break;
    case 2:
      pomoState = POMO_PAUSE;
      pomoPrevState = POMO_WORK;
      pomoRemainingSec = pomoConfigWorkMin * 60;
      currentState = STATE_POMODORO;
      break;
    case 3:
      currentState = STATE_MAIN_MENU;
      break;
    }
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("=== TIME TOOLS ==="));
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  for (int i = 0; i < TIMERS_TOTAL; i++) {
    display.setCursor(5, 16 + (i * 11));
    if (i == currentTimersSelect)
      display.print(F("> "));
    else
      display.print(F("  "));
    display.println(timerMenuItems[i]);
  }
}

void handleStopwatch(int vry, int vrx, bool clicked) {
  if (clicked) {
    stopwatchRunning = !stopwatchRunning;
    if (stopwatchRunning)
      stopwatchLastTime = millis();
  }
  if (millis() - lastJoyAction > JOY_DELAY && vry > 3000) {
    stopwatchRunning = false;
    stopwatchElapsed = 0;
    lastJoyAction = millis();
  }
  if (stopwatchRunning) {
    unsigned long now = millis();
    stopwatchElapsed += (now - stopwatchLastTime);
    stopwatchLastTime = now;
  }
  unsigned long totalSec = stopwatchElapsed / 1000;
  int ms100 = (stopwatchElapsed % 1000) / 100;
  int sec = totalSec % 60;
  int min = (totalSec / 60) % 60;
  int hour = totalSec / 3600;

  // 获取当前系统时间
  RtcDateTime nowTime = Rtc.GetDateTime();
  char timeBuf[9];
  sprintf(timeBuf, "%02d:%02d:%02d", nowTime.Hour(), nowTime.Minute(),
          nowTime.Second());

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(stopwatchRunning ? F("SW:RUN") : F("SW:PAUS"));
  display.setCursor(80, 0); // 右侧对齐显示系统当前实时时间
  display.print(timeBuf);
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  char buf[16];
  sprintf(buf, "%02d:%02d:%02d", hour * 60 + min, sec, ms100);
  display.setTextSize(2);
  display.setCursor(16, 25);
  display.print(buf);
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print(F("[Click]Paus  [Joy-D]Rst"));
}

void handleCountdown(int vry, int vrx, bool clicked) {
  if (countdownSettingMode) {
    if (millis() - lastJoyAction > JOY_DELAY) {
      if (vrx < 1000) {
        countdownEditField = 0;
        lastJoyAction = millis();
      } else if (vrx > 3000) {
        countdownEditField = 1;
        lastJoyAction = millis();
      }
      int delta = (vry < 1000) ? 1 : ((vry > 3000) ? -1 : 0);
      if (delta != 0) {
        if (countdownEditField == 0)
          countdownTotalSec += delta * 60;
        else
          countdownTotalSec += delta;
        if (countdownTotalSec < 10)
          countdownTotalSec = 10;
        countdownTotalSec = constrain(countdownTotalSec, 10, 5999);
        lastJoyAction = millis();
      }
    }
    if (clicked) {
      countdownRemainingSec = countdownTotalSec;
      countdownRunning = true;
      countdownSettingMode = false;
      countdownStartTime = millis();
    }
  } else {
    if (clicked) {
      countdownRunning = !countdownRunning;
      if (countdownRunning)
        countdownStartTime = millis();
    }
    if (countdownRunning) {
      unsigned long now = millis();
      if (now - countdownStartTime >= 1000) {
        long passedSec = (now - countdownStartTime) / 1000;
        countdownRemainingSec -= passedSec;
        countdownStartTime += passedSec * 1000;
        if (countdownRemainingSec <= 0) {
          countdownRemainingSec = 0;
          countdownRunning = false;
        }
      }
    }
    if (millis() - lastJoyAction > JOY_DELAY && vry > 3000) {
      countdownRunning = false;
      countdownSettingMode = true;
      lastJoyAction = millis();
    }
  }

  // 获取当前系统时间
  RtcDateTime nowTime = Rtc.GetDateTime();
  char timeBuf[9];
  sprintf(timeBuf, "%02d:%02d:%02d", nowTime.Hour(), nowTime.Minute(),
          nowTime.Second());

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("COUNTDOWN"));
  display.setCursor(80, 0); // 显示系统当前实时时间
  display.print(timeBuf);
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  long dispSec =
      countdownSettingMode ? countdownTotalSec : countdownRemainingSec;
  int m = dispSec / 60;
  int s = dispSec % 60;
  char buf[8];
  sprintf(buf, "%02d:%02d", m, s);
  display.setTextSize(2);
  display.setCursor(34, 25);
  display.print(buf);
  display.setTextSize(1);
  display.setCursor(0, 55);
  if (countdownSettingMode) {
    display.print(F("X:Switch Y:+- [Click]Go"));
    if (countdownEditField == 0)
      display.fillRect(34, 41, 22, 2, SSD1306_WHITE);
    else
      display.fillRect(70, 41, 22, 2, SSD1306_WHITE);
  } else {
    if (countdownRemainingSec == 0)
      display.print(F("TIME UP! [Joy-D] Return"));
    else
      display.print(countdownRunning ? F("[Click]Paus   [Joy-D]Stop")
                                     : F("[Click]Run    [Joy-D]Stop"));
  }
}

void handlePomodoro(int vry, int vrx, bool clicked) {
  if (clicked) {
    if (pomoState == POMO_PAUSE) {
      pomoState = pomoPrevState;
      pomoLastTick = millis();
    } else {
      pomoPrevState = pomoState;
      pomoState = POMO_PAUSE;
    }
  }
  if (pomoState != POMO_PAUSE) {
    unsigned long now = millis();
    if (now - pomoLastTick >= 1000) {
      long passed = (now - pomoLastTick) / 1000;
      pomoRemainingSec -= passed;
      pomoLastTick += passed * 1000;
      if (pomoRemainingSec <= 0) {
        if (pomoState == POMO_WORK) {
          pomoCompletedCycles++;
          pomoState = POMO_BREAK;
          pomoRemainingSec = 300;
        } else {
          pomoState = POMO_WORK;
          pomoRemainingSec = pomoConfigWorkMin * 60;
        }
        pomoPrevState = pomoState;
        pomoState = POMO_PAUSE;
      }
    }
  }

  // 获取当前系统时间
  RtcDateTime nowTime = Rtc.GetDateTime();
  char timeBuf[9];
  sprintf(timeBuf, "%02d:%02d:%02d", nowTime.Hour(), nowTime.Minute(),
          nowTime.Second());

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  if (pomoState == POMO_PAUSE)
    display.print(F("POMO:PAUS"));
  else
    display.print(pomoState == POMO_WORK ? F("POMO:FOC") : F("POMO:BRK"));
  display.setCursor(80, 0); // 显示系统当前实时时间
  display.print(timeBuf);
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  int m = pomoRemainingSec / 60;
  int s = pomoRemainingSec % 60;
  char buf[8];
  sprintf(buf, "%02d:%02d", m, s);
  display.setTextSize(2);
  display.setCursor(34, 20);
  display.print(buf);
  long totalDenom = (pomoState == POMO_BREAK ||
                     (pomoState == POMO_PAUSE && pomoPrevState == POMO_BREAK))
                        ? 300
                        : (pomoConfigWorkMin * 60);
  int barWidth =
      map(constrain(pomoRemainingSec, 0, totalDenom), 0, totalDenom, 0, 100);
  display.drawRect(14, 40, 100, 5, SSD1306_WHITE);
  display.fillRect(14, 40, barWidth, 5, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print(F("Done:"));
  display.print(pomoCompletedCycles);
  display.setCursor(60, 55);
  display.print(F("[Click] Start/Paus"));
}
