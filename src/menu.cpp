#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd;
typedef void (*RotataryHandler) (const int rotation);
extern void DecodeRotaryEncoder(byte i);
extern int DecodeSwitch(byte switchState);
extern RotataryHandler rfunc;
extern int set[];
extern String names[4];
int pwr[5] = {1,10,100,1000,10000};

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
int numberIncrement;
int numberValue;
int printValue(int index, int value, int row, int col);
int numberInput(int index);

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
}

void settingsMenu() {
start:
  rfunc = settingsMenuHandler;
  lcd.clear();
  for (int i = 0; i < 4; i++) {
    lcd.setCursor(0,i);
    lcd.print((i != settingsMenuId ? " " : ">") + names[i]);
    printValue(i, set[i], 13, i);
  }


  while(DecodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    DecodeRotaryEncoder(PIN_PORT & AB_BYTE);
    switch (DecodeSwitch(PIN_PORT & SW_BYTE)) {
      case BTN_SHORT_RELEASE:
        set[lastSettingsMenuId] = numberInput(settingsMenuId);
        goto start;

      case BTN_LONG_HOLD:
        settingsMenuId = 0;
        lastSettingsMenuId = 0;
        return;
    }
  }
}

void numberInputHandler(const int rotation) {
  if (rotation > 0) {
    numberInputId++;
    numberValue += numberIncrement;
    if (numberInputId >= 10) {
      numberInputId = 0;
    }
  } else if (rotation < 0) {
    numberInputId--;
    numberValue -= numberIncrement;
    if (numberInputId <= 0) {
      numberInputId = 9;
    }
  }
}

int printValue(int index, int value, int row, int col) {
  char buf[10];  
  switch (index) {
  case 0:
    sprintf( buf, "%01d.%03dA", value / 1000, value % 1000);
    break;
  case 1:
    sprintf( buf, "%02d.%02dV", value / 100, value % 100);
    break;
  case 2:
    sprintf( buf, "%03d.%01dW", value / 10, value % 10);
    break;
  case 3:
    sprintf( buf, "%05dR", value);
    break;
  }
  lcd.setCursor(row,col);
  lcd.print(buf);
}

int numberInput(int index) {
  int digitId;
  int digits = 4;

  rfunc = numberInputHandler;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Set " + names[index]);
  
  numberValue = set[index];
  digitId = index;
  numberIncrement = pwr[digits - 1 - digitId];
  numberInputId = int(numberValue / pwr[digits - 1 - digitId]) % 10;
  lastNumberInputId = numberInputId;

  printValue(index, numberValue, 1, 1);
  lcd.setCursor(digitId + (digitId <= index ? 1 : 2), 1);
  lcd.cursor();

  while(DecodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    DecodeRotaryEncoder(PIN_PORT & AB_BYTE);
    if (lastNumberInputId != numberInputId) {
      lastNumberInputId = numberInputId;
      printValue(index, numberValue, 1, 1);
      lcd.setCursor(digitId + (digitId <= index ? 1 : 2), 1);
    }
    switch (DecodeSwitch(PIN_PORT & SW_BYTE)) {
      case BTN_DOWN:
        digitId = (digitId + 1) % digits;
        lcd.setCursor(digitId + (digitId <= index ? 1 : 2), 1);
        numberIncrement = pwr[digits - 1 - digitId];
        numberInputId = int(numberValue / numberIncrement) % 10;
        lastNumberInputId = numberInputId;
        break;
      case BTN_LONG_HOLD:
        lcd.noCursor();
        return numberValue;
    }
  }
  return numberValue;
}