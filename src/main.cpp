#include "Calculator.h"
#include "Config.h"
#include "DeveloperTools.h"
#include "Games.h"
#include "IRrecv.h"
#include "Keyboard.h"
#include "LobstersReader.h"
#include "MainMenu.h"
#include "Options.h"
#include "Pet.h"
#include "TimeTools.h"
#include "WiFiModule.h"

static int lastBackState = HIGH;
static unsigned long backBtnDownTime = 0;
static bool longPressTriggered = false;

IRrecv irrecv(IR_RECV_PIN);

/**
 * @brief 统一红外输入解析器：将红外远程控制报文转换为系统原生操控总线信号
 */
void processIRInput(int &vrx, int &vry, bool &clicked, bool &shortPress,
                    bool &anyInput) {
  decode_results results;

  if (irrecv.decode(&results)) {
    // 过滤掉红外协议中的长按连续重复帧 (repeat)，保证单次触发的稳定性
    if (!results.repeat) {
      // 在新库中，results.command 存储了解码后的指令数值
      uint32_t cmd = results.command;

      switch (cmd) {
      case IR_REMOTE_CMD_UP:
        vry = 500; // 模拟物理摇杆推向上极限
        anyInput = true;
        break;
      case IR_REMOTE_CMD_DOWN:
        vry = 3500; // 模拟物理摇杆推向下极限
        anyInput = true;
        break;
      case IR_REMOTE_CMD_LEFT:
        vrx = 500; // 模拟物理摇杆推向左极限
        anyInput = true;
        break;
      case IR_REMOTE_CMD_RIGHT:
        vrx = 3500; // 模拟物理摇杆推向右极限
        anyInput = true;
        break;
      case IR_REMOTE_CMD_OK:
        clicked = true; // 模拟物理摇杆下压 Click
        anyInput = true;
        break;
      case IR_REMOTE_CMD_BACK:
        shortPress = true; // 模拟物理 Back 按键触发
        anyInput = true;
        break;
      default:
        break;
      }
    }
    // 释放接收锁，使能下一次硬件 RMT 脉冲捕获
    irrecv.resume();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(JOY_VRX, INPUT);
  pinMode(JOY_VRY, INPUT);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);

  analogSetAttenuation(ADC_11db);
  Wire.begin(OLED_SDA, OLED_SCL);

  irrecv.enableIRIn();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  Rtc.Begin();
  if (!Rtc.GetIsRunning())
    Rtc.SetIsRunning(true);
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid())
    Rtc.SetDateTime(compiled);

  randomSeed(analogRead(POT_PIN) + Rtc.GetDateTime().Second());
  lastActivityTime = millis();
}

void loop() {
  int swState = digitalRead(JOY_SW);
  int vrxVal = analogRead(JOY_VRX);
  int vryVal = analogRead(JOY_VRY);
  bool anyInputDetected = false;
  bool isClicked = false;

  if (swState == LOW && (millis() - lastButtonPress > DEBOUNCE_DELAY)) {
    lastButtonPress = millis();
    isClicked = true;
    anyInputDetected = true;
  }
  if (vrxVal < 1000 || vrxVal > 3000 || vryVal < 1000 || vryVal > 3000) {
    anyInputDetected = true;
  }

  int currentBackState = digitalRead(BTN_BACK);
  bool shortPress = false;
  bool longPress = false;

  if (currentBackState != lastBackState) {
    if (currentBackState == HIGH) {
      if (!longPressTriggered && (millis() - backBtnDownTime >= 50)) {
        shortPress = true;
        anyInputDetected = true;
      }
      longPressTriggered = false;
    } else {
      backBtnDownTime = millis();
    }
    lastBackState = currentBackState;
  } else if (currentBackState == LOW && !longPressTriggered) {
    if (millis() - backBtnDownTime > 800) {
      longPress = true;
      longPressTriggered = true;
      anyInputDetected = true;
    }
  }

  if (currentState != STATE_IR_DEBUGGER) {
    processIRInput(vrxVal, vryVal, isClicked, shortPress, anyInputDetected);
  }

  if (currentState == STATE_BRICK || currentState == STATE_PET) {
    static int lastPotVal = 0;
    int currentPotVal = analogRead(POT_PIN);
    if (abs(currentPotVal - lastPotVal) > 40)
      anyInputDetected = true;
    lastPotVal = currentPotVal;
  }

  if (anyInputDetected) {
    lastActivityTime = millis();
  } else {
    if ((currentState == STATE_CLOCK || currentState == STATE_MAIN_MENU) &&
        (millis() - lastActivityTime > IDLE_TIMEOUT)) {
      petEnteredViaTimeout = true;
      initPet();
      currentState = STATE_PET;
      lastActivityTime = millis();
    }
  }

  if (longPress) {
    currentState = STATE_MAIN_MENU;
  } else if (shortPress) {

    if (currentState == STATE_GOMOKU_PLAY)
      currentState = STATE_GOMOKU_MENU;
    else if (currentState == STATE_GOMOKU_MENU || currentState == STATE_SNAKE ||
             currentState == STATE_2048 || currentState == STATE_DINO ||
             currentState == STATE_BRICK || currentState == STATE_STACK ||
             currentState == STATE_NAVAL_PLAY ||
             currentState == STATE_GOLDMINER ||
             currentState == STATE_FREE_THE_KEY ||
             currentState == STATE_PACMAN || currentState == STATE_SHOOTER ||
             currentState == STATE_TARGET_RANGE ||
             currentState == STATE_TETRIS || currentState == STATE_3D_RACING ||
             currentState == STATE_3D_RUNNER ||
             currentState == STATE_TANK_TROUBLE ||
             currentState == STATE_UNDERTALE ||
             currentState == STATE_JUMPJUMP || currentState == STATE_FLAPPY)
      currentState = STATE_GAMES_MENU;
    else if (currentState == STATE_PET)
      currentState = petEnteredViaTimeout ? STATE_CLOCK : STATE_MAIN_MENU;
    else if (currentState == STATE_GAMES_MENU ||
             currentState == STATE_CALCULATOR ||
             currentState == STATE_SETTINGS || currentState == STATE_CLOCK ||
             currentState == STATE_TIMERS_MENU ||
             currentState == STATE_LOBSTERS_READER ||
             currentState == STATE_WIFI_MENU ||
             currentState == STATE_IR_DEBUGGER)
      currentState = STATE_MAIN_MENU;
    else if (currentState == STATE_MAIN_MENU)
      currentState = STATE_CLOCK;
    else if (currentState == STATE_STOPWATCH ||
             currentState == STATE_COUNTDOWN || currentState == STATE_POMODORO)
      currentState = STATE_TIMERS_MENU;
    else if (currentState == STATE_WIFI_SCAN ||
             currentState == STATE_WIFI_KEYBOARD ||
             currentState == STATE_WIFI_CONNECTING)
      currentState = STATE_WIFI_MENU;
  }

  switch (currentState) {
  case STATE_MAIN_MENU:
    handleMainMenu(vryVal, vrxVal, isClicked);
    break;
  case STATE_CLOCK:
    if (isClicked ||
        (vrxVal < 1000 || vrxVal > 3000 || vryVal < 1000 || vryVal > 3000)) {
      if (isClicked || (millis() - lastJoyAction > JOY_DELAY))
        currentState = STATE_MAIN_MENU;
    }
    handleClockMode(false);
    break;
  case STATE_CALCULATOR:
    handleCalculatorMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_LOBSTERS_READER:
    handleLobstersReader(vryVal, vrxVal, isClicked);
    break;
  case STATE_GAMES_MENU:
    handleGamesMenu(vryVal, vrxVal, isClicked);
    break;
  case STATE_SETTINGS:
    handleSettingsMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_SNAKE:
    handleSnakeMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_GOMOKU_MENU:
    handleGomokuMenu(vryVal, vrxVal, isClicked);
    break;
  case STATE_GOMOKU_PLAY:
    handleGomokuPlay(vryVal, vrxVal, isClicked);
    break;
  case STATE_2048:
    handle2048Mode(vryVal, vrxVal, isClicked);
    break;
  case STATE_DINO:
    handleDinoMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_BRICK:
    handleBrickMode(isClicked);
    break;
  case STATE_STACK:
    handleStackMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_NAVAL_PLAY:
    handleNavalPlay(vryVal, vrxVal, isClicked);
    break;
  case STATE_GOLDMINER:
    handleGoldMinerMode(vrxVal, vryVal, isClicked);
    break;
  case STATE_FREE_THE_KEY:
    handleFreeKeyMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_PACMAN:
    handlePacmanMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_SHOOTER:
    handleShooterMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_TETRIS:
    handleTetrisMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_TARGET_RANGE:
    handleTargetMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_3D_RACING:
    handleRacing3DMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_3D_RUNNER:
    handleRunner3DMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_TANK_TROUBLE:
    handleTankTroubleMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_UNDERTALE:
    handleUndertaleMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_JUMPJUMP:
    handleJumpJumpMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_FLAPPY:
    handleFlappyMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_PET:
    handlePetMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_IR_DEBUGGER:
    handleIRDebuggerMode(vryVal, vrxVal, isClicked);
    break;
  case STATE_TIMERS_MENU:
    handleTimersMenu(vryVal, vrxVal, isClicked);
    break;
  case STATE_STOPWATCH:
    handleStopwatch(vryVal, vrxVal, isClicked);
    break;
  case STATE_COUNTDOWN:
    handleCountdown(vryVal, vrxVal, isClicked);
    break;
  case STATE_POMODORO:
    handlePomodoro(vryVal, vrxVal, isClicked);
    break;
  case STATE_WIFI_MENU:
    handleWiFiMenu(vryVal, vrxVal, isClicked);
    break;
  case STATE_WIFI_SCAN:
    handleWiFiScan(vryVal, vrxVal, isClicked);
    break;
  case STATE_WIFI_KEYBOARD:
    handleGlobalKeyboard(vryVal, vrxVal, isClicked);
    break;
  case STATE_WIFI_CONNECTING:
    handleWiFiConnectingLoop();
    break;
  }

  display.display();
  delay(10);
}
