#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd;
typedef void (*RotataryHandler) (const int rotation);
extern void updateVoltages();
extern void DecodeRotaryEncoder(byte i);
extern int DecodeSwitch(byte switchState);
extern void configureMode();
extern void runMode();
extern RotataryHandler rfunc;
extern float vin;
extern float celcius;
extern int set[];
extern int mode;
extern bool load;
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
//void mainMenuHandler(const int rotation);

int runMenuId = 1;
int lastRunMenuId = 1;
void runMenu();
void runMenuHandler(const int rotation);

int numberInputId = 0;
int lastNumberInputId = 0;
int numberIncrement;
int numberValue;
int printValue(int index, int value, int row, int col);
int numberInput(int index);

void mainMenuHandler(const int rotation) {
  if (rotation > 0) {
    if (mainMenuId < 0) {
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
  updateVoltages();
  rfunc = mainMenuHandler;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Load:        Off");
  lcd.setCursor(0,1);
  lcd.print(" Voltage:     ");
  lcd.print(vin);
  lcd.print("V  ");
  lcd.setCursor(0,2);
  lcd.print(" Temperature: 26.2");
  lcd.write((byte)0);
  lcd.print("C");
  lcd.setCursor(0,3);
  lcd.print("");
  // lcd.setCursor(0,3);
  // lcd.print(" Settings...");
  lcd.setCursor(0, lastMainMenuId ? 3 : 0);
  lcd.print(">");

  float lastVin = vin;
  while(DecodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    updateVoltages();
    DecodeRotaryEncoder(PIN_PORT & AB_BYTE);
    switch (DecodeSwitch(PIN_PORT & SW_BYTE)) {
      case 1:
        if (mainMenuId == 0) {
          runMenu();
          goto start;
        } else if (mainMenuId == 1) {
          //settingsMenu();
          goto start;
        }
    }
    if (vin != lastVin) {
      lastVin = vin;
      lcd.setCursor(14, 1);
      lcd.print(vin);
      lcd.print("V ");
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
  int digits = index < 3 ? 4 : 5;
  lcd.clear();

  rfunc = numberInputHandler;
  
  numberValue = set[index];
  digitId = index;
  numberIncrement = pwr[digits - 1 - digitId];
  numberInputId = int(numberValue / pwr[digits - 1 - digitId]) % 10;
  lastNumberInputId = numberInputId;

  printValue(index, set[index], int(index / 2) * 10 + 2, index % 2);
  lcd.setCursor(int(index / 2) * 10 + digitId + (digitId <= index ? 2 : 3), index % 2);
  lcd.cursor();

  while(DecodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    updateVoltages();
    DecodeRotaryEncoder(PIN_PORT & AB_BYTE);
    if (lastNumberInputId != numberInputId) {
      lastNumberInputId = numberInputId;
      printValue(index, numberValue, int(index / 2) * 10 + 2, index % 2);
      lcd.setCursor(int(index / 2) * 10 + digitId + (digitId <= index ? 2 : 3), index % 2);
    }
    switch (DecodeSwitch(PIN_PORT & SW_BYTE)) {
      case BTN_DOWN:
        digitId = (digitId + 1) % digits;
        lcd.setCursor(int(index / 2) * 10 + digitId + (digitId <= index ? 2 : 3), index % 2);
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

void runMenuPosition(int index, int offset) {
  switch (index) {
    case 0:
      switch (mode) {
        case 0: lcd.setCursor(offset, 0); break;
        case 1: lcd.setCursor(offset, 1); break;
        case 2:
        case 3: lcd.setCursor(10 + offset, 0); break;
        case 4:
        case 5: lcd.setCursor(10 + offset, 1); break;
      }
      break;
    case 1:
      lcd.setCursor(offset, 2);
      break;
    default:
      lcd.setCursor(-5 + index * 3, 3);
      break;
  }
}


void runMenuHandler(const int rotation) {
  if (rotation > 0) {
    if (runMenuId < 7) {
      runMenuId++;
    }
  } else if (rotation < 0) {
    if (runMenuId > 0) {
      runMenuId--;
    }
  }
  if (lastRunMenuId != runMenuId) {
    runMenuPosition(lastRunMenuId, 0);
    lcd.print(" ");
    runMenuPosition(runMenuId, 0);
    lcd.print(">");
    lastRunMenuId = runMenuId;
  }
}

void runMenu() {
start:
  lcd.clear();
  rfunc = runMenuHandler;
  for (int i = 0; i < 4; i++) {
    printValue(i, set[i], int(i / 2) * 10 + 2, i % 2);
  }
  runMenuPosition(0, 1);
  lcd.print("*");
  lcd.setCursor(2,2);
  lcd.print(load ? "On" : "Off");
  lcd.setCursor(12, 2);
  lcd.print(celcius);
  lcd.write((byte)0);
  lcd.print("C");
  lcd.setCursor(1,3);
  lcd.print(" CC CV Pa Pv Ra Rv");
  runMenuPosition(lastRunMenuId, 0);
  lcd.print(" ");
  runMenuPosition(runMenuId, 0);
  lcd.print(">");
  lastRunMenuId = runMenuId;

  while(DecodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    updateVoltages();
    DecodeRotaryEncoder(PIN_PORT & AB_BYTE);
    switch (DecodeSwitch(PIN_PORT & SW_BYTE)) {
      case BTN_SHORT_RELEASE:
        switch (runMenuId) {
        case 0:
          load = false;
          runMode();
          switch (mode) {
            case 4: case 5: numberInput(3); break;
            case 3: numberInput(2); break;
            default: numberInput(mode);
          }
          goto start;
        case 1:
          load = !load;
          runMode();
          goto start;
        default:
          mode = runMenuId - 2;
          configureMode();
          goto start;
        }
      case BTN_LONG_HOLD:
        runMenuId = 1;
        lastRunMenuId = 1;
        return;
    }
  }
}