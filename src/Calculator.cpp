#include "Calculator.h"
#include "Config.h"

static int calcCx = 0, calcCy = 0;
static char calcInput[32] = "";
static char calcResultStr[24] = "";
static bool calcClearOnNext = false;

const char calcKeys[4][6] = {{'7', '8', '9', '/', '(', 'C'},
                             {'4', '5', '6', '*', ')', 'D'},
                             {'1', '2', '3', '-', '^', 'S'},
                             {'0', '.', '=', '+', 'P', ' '}};

// 前置解析递归声明
double parseExpression(const char *&p);
double parseTerm(const char *&p);
double parsePower(const char *&p);
double parseAtom(const char *&p);

double parseAtom(const char *&p) {
  while (*p == ' ')
    p++;
  if (strncmp(p, "sqrt(", 5) == 0) {
    p += 5;
    double val = parseExpression(p);
    if (*p == ')')
      p++;
    return sqrt(val);
  }
  if (*p == '(') {
    p++;
    double val = parseExpression(p);
    if (*p == ')')
      p++;
    return val;
  }
  char *next;
  double val = strtod(p, &next);
  if (p == next) {
    if (*p != '\0')
      p++;
    return 0;
  }
  p = next;
  return val;
}

double parsePower(const char *&p) {
  double val = parseAtom(p);
  while (*p == ' ')
    p++;
  while (*p == '^') {
    p++;
    val = pow(val, parseAtom(p));
    while (*p == ' ')
      p++;
  }
  return val;
}

double parseTerm(const char *&p) {
  double val = parsePower(p);
  while (*p == ' ')
    p++;
  while (*p == '*' || *p == '/') {
    char op = *p++;
    double next = parsePower(p);
    if (op == '*')
      val *= next;
    else if (next != 0)
      val /= next;
    else
      val = NAN;
    while (*p == ' ')
      p++;
  }
  return val;
}

double parseExpression(const char *&p) {
  double val = parseTerm(p);
  while (*p == ' ')
    p++;
  while (*p == '+' || *p == '-') {
    char op = *p++;
    double next = parseTerm(p);
    if (op == '+')
      val += next;
    else
      val -= next;
    while (*p == ' ')
      p++;
  }
  return val;
}

void handleCalculatorMode(int vry, int vrx, bool clicked) {
  if (millis() - lastJoyAction > JOY_DELAY) {
    if (vry < 1000) {
      calcCy = (calcCy == 0) ? 3 : calcCy - 1;
      lastJoyAction = millis();
    } else if (vry > 3000) {
      calcCy = (calcCy == 3) ? 0 : calcCy + 1;
      lastJoyAction = millis();
    }
    if (vrx < 1000) {
      calcCx = (calcCx == 0) ? 5 : calcCx - 1;
      lastJoyAction = millis();
    } else if (vrx > 3000) {
      calcCx = (calcCx == 5) ? 0 : calcCx + 1;
      lastJoyAction = millis();
    }
  }

  if (clicked) {
    char key = calcKeys[calcCy][calcCx];
    if (calcClearOnNext && key != '=' && key != 'D') {
      calcInput[0] = '\0';
      calcClearOnNext = false;
    }
    if (key == 'C') {
      calcInput[0] = '\0';
      calcResultStr[0] = '\0';
      calcClearOnNext = false;
    } else if (key == 'D') {
      int len = strlen(calcInput);
      if (len > 0) {
        if (len >= 5 && strcmp(&calcInput[len - 5], "sqrt(") == 0)
          calcInput[len - 5] = '\0';
        else
          calcInput[len - 1] = '\0';
      }
    } else if (key == '=') {
      if (strlen(calcInput) > 0) {
        const char *p = calcInput;
        double res = parseExpression(p);

        // 增加 isinf(res) 拦截，防止如 9^999 导致无限大崩溃
        if (isnan(res) || isinf(res)) {
          strcpy(calcResultStr, "= Error");
        } else {
          // 1. 安全边界检查：只有在 long
          // 有效范围内才允许转整型，防止大数引发未定义行为
          if (res >= -2147483648.0 && res <= 2147483647.0 && res == (long)res) {
            snprintf(calcResultStr, sizeof(calcResultStr), "= %ld", (long)res);
          } else {
            // 2. 科学计数法分流：针对超大数（如
            // 2^125）或极小数，用科学计数法输出 占用约 13 个字符（如
            // "= 4.2535e+37"），完美适配 calcResultStr[24]
            if (abs(res) >= 1e10 || (abs(res) < 1e-4 && res != 0)) {
              snprintf(calcResultStr, sizeof(calcResultStr), "= %.4e", res);
            } else {
              // 3. 普通浮点数：将临时缓冲区 temp 扩大至 32 字节，并配合
              // snprintf 限制长度
              char temp[32];
              dtostrf(res, 12, 4, temp);
              int rLen = strlen(temp);
              while (rLen > 0 &&
                     (temp[rLen - 1] == ' ' || temp[rLen - 1] == '0'))
                temp[--rLen] = '\0';
              if (rLen > 0 && temp[rLen - 1] == '.')
                temp[--rLen] = '\0';

              snprintf(calcResultStr, sizeof(calcResultStr), "= %s", temp);
            }
          }
        }
        calcClearOnNext = true;
      }
    } else if (key == 'S') {
      if (strlen(calcInput) + 5 < sizeof(calcInput))
        strcat(calcInput, "sqrt(");
    } else if (key == 'P') {
      if (strlen(calcInput) + 6 < sizeof(calcInput))
        strcat(calcInput, "3.1416");
    } else if (key != ' ') {
      int len = strlen(calcInput);
      if (len + 1 < sizeof(calcInput)) {
        calcInput[len] = key;
        calcInput[len + 1] = '\0';
      }
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(2, 0);
  display.print(calcInput);
  display.setCursor(2, 11);
  display.print(calcResultStr);
  display.drawFastHLine(0, 22, 128, SSD1306_WHITE);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 6; c++) {
      int x = c * 21 + 1;
      int y = 24 + r * 10 + 1;
      char key = calcKeys[r][c];
      if (r == calcCy && c == calcCx) {
        display.fillRect(x - 1, y - 1, 21, 10, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      display.setCursor(x + 4, y + 1);
      if (key == 'D')
        display.print(F("<-"));
      else if (key == 'S')
        display.print(F("sq"));
      else if (key == 'P')
        display.print(F("pi"));
      else if (key != ' ')
        display.print(key);
    }
  }
  display.setTextColor(SSD1306_WHITE);
}
