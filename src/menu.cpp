#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd;
typedef void (*RotataryHandler) (const int rotation);
extern void DecodeRotaryEncoder(byte i);
extern int DecodeSwitch(byte switchState);
extern RotataryHandler rfunc;
extern float set[4];
extern String names[4];

#define BTN_UP 0
#define BTN_DOWN 1
#define BTN_SHORT_HOLD 2
#define BTN_LONG_HOLD 3
#define BTN_SHORT_RELEASE 4
#define BTN_LONG_RELEASE 5

int mainMenuId = 0;
int lastMainMenuId = 0;
void mainMenu();
void mainMenuHandler(const int rotation);

int settingsMenuId = 0;
int lastSettingsMenuId = 0;
void settingsMenu();
void settingsMenuHandler(const int rotation);

int numberInputId = 0;
int lastNumberInputId = 0;
int numberInputDigitId = 0;
float numberInput(String name, float value);

void mainMenuHandler(const int rotation) {
  if (rotation > 0) {
    if (mainMenuId < 1) {
      mainMenuId++;
    }
  } else if (rotation < 0) {
    if (mainMenuId > 0) {
      mainMenuId--;
    }
  }
  if (lastMainMenuId != mainMenuId) {
    lcd.setCursor(0, lastMainMenuId ? 3 : 0);
    lcd.print(" ");
    lcd.setCursor(0,mainMenuId ? 3 : 0);
    lcd.print(">");
    lastMainMenuId = mainMenuId;
  }
};

void mainMenu() {
start: 
  rfunc = mainMenuHandler;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Status:      On");
  lcd.setCursor(0,1);
  lcd.print(" Voltage:     30v");
  lcd.setCursor(0,2);
  lcd.print(" Temperature: 26.2");
  lcd.write((byte)0);
  lcd.print("C");
  lcd.setCursor(0,3);
  lcd.print("");
  lcd.setCursor(0,3);
  lcd.print(" Settings...");
  lcd.setCursor(0, lastMainMenuId ? 3 : 0);
  lcd.print(">");
  
  while(DecodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(1) {
    DecodeRotaryEncoder(PIN_PORT & AB_BYTE);
    switch (DecodeSwitch(PIN_PORT & SW_BYTE)) {
      case 1:
        if (mainMenuId == 0) {
        } else if (mainMenuId == 1) {
          settingsMenu();
          goto start;
        }
    }
  }
}

void settingsMenuHandler(const int rotation) {
  if (rotation > 0) {
    if (settingsMenuId < 3) {
      settingsMenuId++;
    }
  } else if (rotation < 0) {
    if (settingsMenuId > 0) {
      settingsMenuId--;
    }
  }
  if (lastSettingsMenuId != settingsMenuId) {
    lcd.setCursor(0, lastSettingsMenuId);
    lcd.print(" ");
    lcd.setCursor(0,settingsMenuId);
    lcd.print(">");
    lastSettingsMenuId = settingsMenuId;
  }
};

void settingsMenu() {
start:
  rfunc = settingsMenuHandler;
  lcd.clear();
  for (int i = -0; i < 4; i++) {
    lcd.setCursor(0,i);
    lcd.print((i != settingsMenuId ? " " : ">") + names[i]);
  }


  while(DecodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    DecodeRotaryEncoder(PIN_PORT & AB_BYTE);
    switch (DecodeSwitch(PIN_PORT & SW_BYTE)) {
      case BTN_SHORT_RELEASE:
        set[lastSettingsMenuId] = numberInput(names[lastSettingsMenuId], set[lastSettingsMenuId]);
        goto start;

      case BTN_LONG_HOLD:
        settingsMenuId = 0;
        lastSettingsMenuId = 0;
        return;
    }
  }
}

float numberInput(String name, float value) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Set " + name);
    lcd.setCursor(0,1);
    lcd.print(value);

    while(true) {
      switch (DecodeSwitch(PIN_PORT & SW_BYTE)) {
        case BTN_LONG_HOLD:
          return value;
      }
    }
    return value;
}