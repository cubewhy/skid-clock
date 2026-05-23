#include "Pet.h"
#include "Config.h"

static PetState petState = PET_IDLE;
static int petX = 54;
static int petY = 40;
static int petDir = 1;
static unsigned long lastPetStateChange = 0;
static unsigned long lastPetAnimUpdate = 0;
static int petFrame = 0;
static int lastPetPotVal = 0;

void initPet() {
  petState = PET_IDLE;
  petX = 54;
  petY = 40;
  petFrame = 0;
  lastPetStateChange = millis();
  lastPetAnimUpdate = millis();
  lastPetPotVal = analogRead(POT_PIN);
}

void handlePetMode(int vry, int vrx, bool clicked) {
  if (clicked) {
    currentState = petEnteredViaTimeout ? STATE_CLOCK : STATE_MAIN_MENU;
    return;
  }
  unsigned long now = millis();

  int currentPotVal = analogRead(POT_PIN);
  if (abs(currentPotVal - lastPetPotVal) > 80) {
    petState = PET_PETTED;
    lastPetStateChange = now;
    lastPetPotVal = currentPotVal;
  }
  if (petState == PET_PETTED && (now - lastPetStateChange > 1500)) {
    petState = PET_IDLE;
    lastPetStateChange = now;
  }
  if (petState != PET_PETTED && (now - lastPetStateChange > 5000)) {
    lastPetStateChange = now;
    int r = random(0, 3);
    if (r == 0)
      petState = PET_IDLE;
    else if (r == 1) {
      petState = PET_WALK;
      petDir = (random(0, 2) == 0) ? 1 : -1;
    } else
      petState = PET_SLEEP;
  }

  int animInterval = (petState == PET_WALK) ? 140 : 350;
  if (now - lastPetAnimUpdate > animInterval) {
    lastPetAnimUpdate = now;
    petFrame = (petFrame + 1) % 4;
    if (petState == PET_WALK) {
      petX += petDir * 2;
      if (petX < 5) {
        petX = 5;
        petDir = 1;
      }
      if (petX > 107) {
        petX = 107;
        petDir = -1;
      }
    }
  }

  display.clearDisplay();
  display.drawFastHLine(0, 56, 128, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  if (petState == PET_PETTED)
    display.print(F("So Comfortable~"));
  else
    display.print(F("Cat Life~"));

  RtcDateTime rtcNow = Rtc.GetDateTime();
  char petTimeStr[9];
  sprintf(petTimeStr, "%02d:%02d:%02d", rtcNow.Hour(), rtcNow.Minute(),
          rtcNow.Second());
  display.setCursor(80, 2);
  display.print(petTimeStr);

  int x = petX;
  int y = petY;
  if (petState == PET_SLEEP) {
    display.fillRoundRect(x, y + 6, 16, 10, 4, SSD1306_WHITE);
    display.drawFastHLine(x + 11, y + 10, 3, SSD1306_BLACK);
    display.fillTriangle(x + 9, y + 6, x + 11, y + 2, x + 13, y + 6,
                         SSD1306_BLACK);
    display.drawTriangle(x + 9, y + 6, x + 11, y + 2, x + 13, y + 6,
                         SSD1306_WHITE);
    if (petFrame == 0) {
      display.setCursor(x + 19, y + 2);
      display.print(F("z"));
    } else if (petFrame == 1) {
      display.setCursor(x + 22, y - 1);
      display.print(F("zZ"));
    } else if (petFrame == 2) {
      display.setCursor(x + 24, y - 5);
      display.print(F("ZzZ"));
    }
  } else if (petState == PET_PETTED) {
    display.fillRoundRect(x + 2, y + 4, 12, 9, 3, SSD1306_WHITE);
    int hx = x + 3;
    display.fillRoundRect(hx, y - 1, 9, 8, 2, SSD1306_WHITE);
    display.fillTriangle(hx + 1, y - 1, hx + 2, y - 4, hx + 4, y - 1,
                         SSD1306_BLACK);
    display.drawTriangle(hx + 1, y - 1, hx + 2, y - 4, hx + 4, y - 1,
                         SSD1306_WHITE);
    display.fillTriangle(hx + 5, y - 1, hx + 7, y - 4, hx + 8, y - 1,
                         SSD1306_BLACK);
    display.drawTriangle(hx + 5, y - 1, hx + 7, y - 4, hx + 8, y - 1,
                         SSD1306_WHITE);
    display.drawPixel(hx + 2, y + 2, SSD1306_BLACK);
    display.drawPixel(hx + 6, y + 2, SSD1306_BLACK);
    display.drawPixel(hx + 4, y + 4, SSD1306_BLACK);
    display.drawFastVLine(x + 4, y + 13, 3, SSD1306_WHITE);
    display.drawFastVLine(x + 6, y + 13, 3, SSD1306_WHITE);
    display.drawFastVLine(x + 10, y + 13, 3, SSD1306_WHITE);
    display.drawFastVLine(x + 12, y + 13, 3, SSD1306_WHITE);
    int tailWiggle = (petFrame % 2 == 0) ? -3 : 0;
    display.drawLine(x + 14, y + 6, x + 16, y + 2 + tailWiggle, SSD1306_WHITE);
    if (petFrame % 2 == 0) {
      display.setCursor(hx + 1, y - 12);
      display.print(F("<3"));
    } else {
      display.drawFastHLine(hx + 1, y - 9, 6, SSD1306_WHITE);
    }
  } else {
    display.fillRoundRect(x + 2, y + 4, 12, 9, 3, SSD1306_WHITE);
    int hx = (petDir == 1) ? x + 9 : x - 3;
    display.fillRoundRect(hx, y - 1, 9, 8, 2, SSD1306_WHITE);
    if (petDir == 1) {
      display.fillTriangle(hx + 1, y - 1, hx + 3, y - 4, hx + 5, y - 1,
                           SSD1306_BLACK);
      display.drawTriangle(hx + 1, y - 1, hx + 3, y - 4, hx + 5, y - 1,
                           SSD1306_WHITE);
      display.fillTriangle(hx + 5, y - 1, hx + 7, y - 4, hx + 8, y - 1,
                           SSD1306_BLACK);
      display.drawTriangle(hx + 5, y - 1, hx + 7, y - 4, hx + 8, y - 1,
                           SSD1306_WHITE);
      display.drawPixel(hx + 6, y + 2, SSD1306_BLACK);
    } else {
      display.fillTriangle(hx + 1, y - 1, hx + 2, y - 4, hx + 4, y - 1,
                           SSD1306_BLACK);
      display.drawTriangle(hx + 1, y - 1, hx + 2, y - 4, hx + 4, y - 1,
                           SSD1306_WHITE);
      display.fillTriangle(hx + 4, y - 1, hx + 6, y - 4, hx + 8, y - 1,
                           SSD1306_BLACK);
      display.drawTriangle(hx + 4, y - 1, hx + 6, y - 4, hx + 8, y - 1,
                           SSD1306_WHITE);
      display.drawPixel(hx + 2, y + 2, SSD1306_BLACK);
    }
    if (petState == PET_WALK) {
      if (petFrame % 2 == 0) {
        display.drawLine(x + 4, y + 13, x + 2, y + 16, SSD1306_WHITE);
        display.drawLine(x + 7, y + 13, x + 7, y + 16, SSD1306_WHITE);
        display.drawLine(x + 10, y + 13, x + 9, y + 16, SSD1306_WHITE);
        display.drawLine(x + 13, y + 13, x + 14, y + 16, SSD1306_WHITE);
      } else {
        display.drawLine(x + 4, y + 13, x + 5, y + 16, SSD1306_WHITE);
        display.drawLine(x + 7, y + 13, x + 6, y + 16, SSD1306_WHITE);
        display.drawLine(x + 10, y + 13, x + 11, y + 16, SSD1306_WHITE);
        display.drawLine(x + 13, y + 13, x + 12, y + 16, SSD1306_WHITE);
      }
    } else {
      display.drawFastVLine(x + 4, y + 13, 3, SSD1306_WHITE);
      display.drawFastVLine(x + 6, y + 13, 3, SSD1306_WHITE);
      display.drawFastVLine(x + 10, y + 13, 3, SSD1306_WHITE);
      display.drawFastVLine(x + 12, y + 13, 3, SSD1306_WHITE);
    }
    int tx = (petDir == 1) ? x + 2 : x + 14;
    int tailWiggle = (petFrame % 2 == 0) ? -2 : 1;
    display.drawLine(tx, y + 6, tx - (petDir * 4), y + 2 + tailWiggle,
                     SSD1306_WHITE);
  }
}
