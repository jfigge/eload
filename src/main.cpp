#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MCP4725.h>

// External devices
LiquidCrystal_I2C lcd(0x27,20,4);
Adafruit_ADS1115 ads;
Adafruit_MCP4725 dac;

// Custom LCD characters
uint8_t degree[8] = {140,146,146,140,128,128,128,128};
uint8_t fan[7][8] = {
  {31,17,17,17,17,17,17,31},
  {31,17,17,17,17,17,31,31},
  {31,17,17,17,17,31,21,31},
  {31,17,17,17,31,21,27,31},
  {31,17,17,31,21,27,21,31},
  {31,17,31,21,27,21,27,31},
  {31,27,21,27,21,27,21,31},
};

// Range values
int mode = 0;
double setValues[4] = {.1, 5, 1000, 0.5};
float maxValues[4] = {1.5, 30, 3000, 45};
float incValues[4][4] = { {1, 0, 0.1, 0.01}, {10, 1, 0, 0.1}, {1000, 100, 10, 1}, {10, 1, 0, 0.1} };
String modeNames[4] = {"Current", "Voltage", "Resistance", "Power"};

// Rotary Encoding
#define BTN_UP 0
#define BTN_DOWN 1
#define BTN_SHORT_HOLD 2
#define BTN_LONG_HOLD 3
#define BTN_SHORT_RELEASE 4
#define BTN_LONG_RELEASE 5
typedef void (*RotataryHandler) (const int rotation);
RotataryHandler rfunc = 0;
int decodeSwitch(uint8_t switchState);
void decodeRotaryEncoder(uint8_t raw);

// Analog to Digital conversion
int channel = 0;
int menuIndex = 0;
void updateVoltages();

// Voltage input
float vcc;
int temp;
float loadVcc;
float loadCurrent;
bool load = false;
int overload;

// History
int lastVCC = 0;
int lastCelcius = 0;
int lastVoltage = 0;
bool lastLoad = false; 
int overloadLast;

// Data location
#define DATA_CUR 0
#define DATA_VLT 1
#define DATA_RES 2
#define DATA_PWR 3
#define DATA_RUN 4
#define DATA_TMP 5
#define DATA_FAN 6
int dataCoords[2][7][2] = {
    {{7, 0}, {7, 1}, {7, 2}, {7, 3}, {17, 0}, {17, 1}, {17, 2}},
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {6, 0}, {15, 1}, {15, 2}},
};

// ***************************************************************
// ADC functions
// ***************************************************************
void setInputVoltage(float VCC) {
  static unsigned long lastVCCTime = 0;
  if (VCC != lastVCC) {
    if ((millis() - lastVCCTime) > 1000) {
      lastVCC = VCC;
      lastVCCTime = millis();
      lcd.setCursor(dataCoords[menuIndex][DATA_VLT][0], dataCoords[menuIndex][DATA_VLT][1]);
      lcd.print(VCC);
    }
  }
}
void setTemperature(int celcius) {
  static unsigned long lastCelciusTime = 0;
  if (celcius != lastCelcius) {
    if ((millis() - lastCelciusTime) > 1000) {
      lastCelcius = celcius;
      lastCelciusTime = millis();
      int fan = 7;
      if (celcius >= 100) {
        analogWrite(PIN_FAN, 255);
        load = false;
        PORTB |= B00000100; // digitalWrite(PIN_LAOD_SW(D10), LOW);
        //runMode();
      } else if (celcius > 70) {
        analogWrite(PIN_FAN, 255);
      } else if (celcius <= 30) {
        analogWrite(PIN_FAN, 0);
        fan = 1 ;
      } else {
        analogWrite(PIN_FAN, (celcius - 30) * 255 / 40);
        fan = int((celcius - 15) / 8);
      }
      lcd.setCursor(dataCoords[menuIndex][DATA_TMP][0], dataCoords[menuIndex][DATA_TMP][1]);
      lcd.print(celcius);
      lcd.print("  ");
      lcd.setCursor(dataCoords[menuIndex][DATA_FAN][0], dataCoords[menuIndex][DATA_FAN][1]);
      lcd.write((byte)fan);
    }
  }
}
void setLoadVoltage(float voltage) {
  static unsigned long lastVoltageTime = 0;
  if (voltage != lastVoltage) {
    if ((millis() - lastVoltageTime) > 1000) {
      lastVoltage = voltage;
      lastVoltageTime = millis();
      lcd.setCursor(dataCoords[menuIndex][DATA_VLT][0], dataCoords[menuIndex][DATA_VLT][1]);
      lcd.print(voltage);
    }
  }
}
void updateVoltages() {
  if (ads.conversionComplete()) {
    float voltage = ads.computeVolts(ads.getLastConversionResults());
    switch (channel) {
    case 0: 
      vcc = voltage;
      break;
    case 1: {
      setTemperature(int((voltage * 100) - 273.15));
      break;
    }
    case 2: {
      loadVcc = voltage * (ID_R1 + ID_R2)  / ID_R1;
      break;
    }
    case 3:
      loadCurrent = voltage * 10;
      break;
    }
    channel = (channel + 1) % 4;
    ads.startADCReading(MUX_BY_CHANNEL[channel], false);
  }
  overload = (PINB & 2) >> 1; // digitalRead(PIN_LOAD_OVER(D9))
  if (overload != overloadLast) {
    overloadLast = overload;
    lcd.setCursor(dataCoords[menuIndex][DATA_RUN][0], dataCoords[menuIndex][DATA_RUN][1]);
    lcd.print(overload == LOW ? "O" : digitalRead(PIN_FAN) == LOW ? "N" : "Y");
    PORTB &= B11111011; // digitalWrite(PIN_LOAD_SW(D10), LOW);
  }
}

// ***************************************************************
// Stop the processor
// ***************************************************************
void stop() {
  Serial.println("Failed");
  delay(100);
  while (1) {
    delay(1000);
  }
}  

// ***************************************************************
// Setup
// ***************************************************************
void setup() {
  // Fan
  analogWrite(PIN_FAN, 0);
  pinMode(PIN_FAN, OUTPUT);
  
  // Rotary encoder
  pinMode(PIN_A, INPUT);
  pinMode(PIN_A, INPUT);
  pinMode(PIN_SW, INPUT_PULLUP);

  // Load
  PORTB &= B11111011; // digitalWrite(PIN_LOAD_SW(D10), LOW);
  pinMode(PIN_LOAD_SW, OUTPUT);
  pinMode(PIN_LOAD_OVER, INPUT);

  // OpAmp
  PORTB &= B11111110; // digitalWrite(PIN_NEGATIVE(D8), LOW);
  pinMode(PIN_NEGATIVE, OUTPUT);
  PORTD &= B01111111; // digitalWrite(PIN_POSITIVE(D7), LOW);
  pinMode(PIN_POSITIVE, OUTPUT);

  // Beeper
  noTone(PIN_BEEPER);
  pinMode(PIN_BEEPER, OUTPUT);

  // Unused
  pinMode(12, INPUT_PULLUP);
  pinMode(13, INPUT_PULLUP);
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
  pinMode(A6, INPUT_PULLUP);
  pinMode(A7, INPUT_PULLUP);

  // Initialize logging
  Serial.begin(BAUD_RATE);

  // Initialize the LCD
  Serial.print("Initializing LCD... ");
  lcd.init();
  lcd.createChar(0, degree);
  for (int i = 0; i < 8; i++) {
    lcd.createChar(i + 1, fan[i]);
  }
  Serial.println("Success.");

  // Initiaize the DAC
  Serial.print("Initializing DAC... ");
  dac.begin(0x60);
  if (!dac.setVoltage(0 , false)) {
    lcd.clear();
    lcd.setCursor(0,0);
    stop();
  }
  Serial.println("Success.");

  // Initialize the ADC
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 0.125mV
  Serial.print("Initializing ADC... ");
  if (!ads.begin()) {
    lcd.clear();
    lcd.setCursor(0,0);
    stop();
  }
  ads.startADCReading(MUX_BY_CHANNEL[0], false);
  Serial.println("Success.");

  // Display the splash screen for 3 seconds, 
  // or until the rotary encoder is depressed
  lcd.setCursor(2,0);
  lcd.print("Electronic Load");
  lcd.setCursor(4,1);
  lcd.print("Jason Figge");
  lcd.setCursor(1,2);
  lcd.print("Version 0.9.0");
  lcd.setCursor(1,3);
  lcd.print("Built: Oct 23, 2022");  
  lcd.backlight();
  unsigned long SlapshStartTime = millis();
  while (PIN_PORT & SW_BYTE && millis() - SlapshStartTime < 3000) {
    delay(1);
  }
  lcd.clear();
}

// ***************************************************************
// Common purpose functions
// ***************************************************************
void setMenuIndex(int index) {
  menuIndex = index;
  lastVCC = -100;
  lastCelcius = -100;
  overloadLast = -1;
  lcd.clear();
}
void loadOn() {
  overload = (PINB & 2) >> 1; // digitalRead(PIN_LOAD_OVER)
  lcd.setCursor(dataCoords[menuIndex][DATA_RUN][0], dataCoords[menuIndex][DATA_RUN][1]);
  if (overload == HIGH) {
    PORTB |= B00000100; // digitalWrite(PIN_LOAD_SW(D10), HIGH);
    lcd.print("Y");
  } else {
    PORTB &= B11111011; // digitalWrite(PIN_LOAD_SW(D10), LOW);
    lcd.print("O");
  }
  overloadLast = overload;
}
void loadOff() {
  overload = (PINB & 2) >> 1; // digitalRead(PIN_LOAD_OVER(D9))
  lcd.setCursor(dataCoords[menuIndex][DATA_RUN][0], dataCoords[menuIndex][DATA_RUN][1]);
  PORTB &= B11111011; // digitalWrite(PIN_LOAD_SW(D10), LOW);
  if (overload == HIGH) {
    lcd.print("N");
  } else {
    lcd.print("O");
  }
  overloadLast = overload;
}
void beep(int type) {
  switch (type) {
  case 1: // Button down
  case 2: // Button up
    tone(PIN_BEEPER, 400);
    delay(2);
    break;
  case 3: // Error
    tone(PIN_BEEPER, 1000);
    delay(10);
    break;
  default: // Menu
    tone(PIN_BEEPER, 250);
    delay(1);
    tone(PIN_BEEPER, 250);
    delay(1);
    tone(PIN_BEEPER, 250);
    delay(1);
  }
  noTone(PIN_BEEPER);
}
void printValue(int col, int row, int index, double value) {
  char buffer[6];
  switch (index) {
  case 0:
    dtostrf(value, 4, 2, buffer);
    break;
  case 2:
    dtostrf(value, 4, 0, buffer);
    break;
  default:
    dtostrf(value, 4, 1, buffer);
  }
  lcd.setCursor(col, row);
  lcd.print(buffer);
}

// ***************************************************************
// Rotatry encoder functions
// ***************************************************************
uint8_t lastRaw = 12;
uint8_t lastState = 6;
void decodeRotaryEncoder(uint8_t raw) {
  static uint8_t states[] = {0,4,1,0,2,1,1,0,2,3,1,2,2,3,3,0,5,4,4,0,5,4,6,5,5,6,6,0};
  if (lastRaw != raw) {
    lastRaw = raw;
    uint8_t state = states[(raw >> AB_SHIFT) | (lastState << 2)];
    if (rfunc != 0 && state == 0) {
      if (lastState == 6) {
        rfunc(1);
      } else if (lastState == 3) {
        rfunc(-1);
      }
    }
    lastState = state;
  }
}
int decodeSwitch(uint8_t switchState) {
  static long timer;
  static uint8_t lastSwitchState = -1;
  if (lastSwitchState != switchState) {
    lastSwitchState = switchState;
    if (!switchState) {
      timer = millis();
      beep(1);
      return BTN_DOWN;
    } else {
      timer = millis() - timer;
      beep(2);
      return timer > 1000 ? BTN_LONG_RELEASE : BTN_SHORT_RELEASE;
    }
  } else if (!switchState) {
    return millis() - timer > 1000 ? BTN_LONG_HOLD : BTN_SHORT_HOLD;
  }
  return BTN_UP;
}

// ***************************************************************
// Debug menu
// ***************************************************************
int debugMenuId = 0;
int lastDebugMenuId = 0;
void debugMenuHandler(const int rotation) {
  if (rotation > 0) {
    if (debugMenuId < 2) {
      beep(0);
      debugMenuId++;
    } else {
      beep(3);
    }
  } else if (rotation < 0) {
    if (debugMenuId > 0) {
      beep(0);
      debugMenuId--;
    } else {
      beep(3);
    }
  }
  if (lastDebugMenuId != debugMenuId) {
    lcd.setCursor(0, lastDebugMenuId);
    lcd.print(" ");
    lcd.setCursor(0, debugMenuId);
    lcd.print(">");
    lastDebugMenuId = debugMenuId;
  }
}
void debugMenu() {
  setMenuIndex(1);
  rfunc = debugMenuHandler;
  lcd.setCursor(0,0);
  lcd.print(">Load:");
  lcd.setCursor(0,1);
  lcd.print(" N-tive");
  lcd.setCursor(0,2);
  lcd.print(" P-tive");

  lcd.setCursor(9,0);
  lcd.print("VCC:");
  lcd.setCursor(9,1);
  lcd.print("Temp:");
  lcd.setCursor(9,2);
  lcd.print("Load:");
  
  while(decodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    updateVoltages();
    decodeRotaryEncoder(PIN_PORT & AB_BYTE);
    switch (decodeSwitch(PIN_PORT & SW_BYTE)) {
      case BTN_SHORT_RELEASE:
        Serial.print("Short Press: ");
        Serial.println(debugMenuId);
        switch (debugMenuId) {
        case 0:
          if (overloadLast == HIGH ) {
            if ((PINB & 4) >> 2 == LOW) { // if (digitalRead(10) == LOW)
              loadOn();
            } else {
              loadOff();
            }

          }
          break;
        case 1:
          PORTB |= B00000001; // digitalWrite(PIN_NEGATIVE(D8), HIGH);
          delay(50);
          PORTB &= B11111110; // digitalWrite(PIN_NEGATIVE(D8), LOW);
          lcd.setCursor(1,1);
          lcd.print("N");
          lcd.setCursor(1,2);
          lcd.print("p");
          break;
        case 2:
          PORTD |= B10000000; // digitalWrite(PIN_POSITIVE(D8), HIGH);
          delay(50);
          PORTD &= B01111111; // digitalWrite(PIN_POSITIVE(D8), LOW);
          lcd.setCursor(1,1);
          lcd.print("n");
          lcd.setCursor(1,2);
          lcd.print("P");
          break;
        }
        break;
      case BTN_LONG_HOLD:
        beep(1);
        Serial.print("Long Hold: ");
        loadOff();
        return;
    }
  }
}

// ***************************************************************
// Number editor
// ***************************************************************
int numberInputId;
void numberEditorHandler(const int rotation) {
  double lastValue = setValues[mode];
  if (rotation > 0) {
    setValues[mode] = setValues[mode] - incValues[mode][numberInputId];
    if (setValues[mode] < 0) {
      setValues[mode] = 0;
    }
  } else if (rotation < 0) {
    setValues[mode] = setValues[mode] + incValues[mode][numberInputId];
    if (setValues[mode] > maxValues[mode]) {
      setValues[mode] = maxValues[mode];
    }
  }

  if (lastValue != setValues[mode]) {
    printValue(6, mode, mode, setValues[mode]);
    lcd.setCursor(6 + numberInputId, mode);
    beep(0);
  } else {
    beep(3);
  }
}
void numberEditorMenu() {
  rfunc = numberEditorHandler;
  loadOff();
  numberInputId = 3;
  lcd.setCursor(0, mode);
  lcd.print("  Set");
  lcd.setCursor(6 + numberInputId, mode);
  lcd.cursor_on();

  while(decodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    decodeRotaryEncoder(PIN_PORT & AB_BYTE);
    switch (decodeSwitch(PIN_PORT & SW_BYTE)) {
      case BTN_SHORT_RELEASE:
        numberInputId--;
        if (numberInputId < 0) {
          numberInputId = 3;
        } else if (incValues[mode][numberInputId] == 0) {
          numberInputId--;
        }
        lcd.setCursor(6 + numberInputId, mode);
        break;
      case BTN_LONG_HOLD:
        beep(1);
        Serial.print("Long Hold: ");
        lcd.cursor_off();
        return;
    }
  }
}

// ***************************************************************
// Main menu
// ***************************************************************
int mainCoords[6][2] = { {0,0}, {0,1}, {0,2}, {0,3}, {12,0}, {12,3} };
int mainMenuId = 0;
int lastMainMenuId = 0;
void setMode(int newMode) {
  if (newMode != mode) {
    if (newMode >=0 && newMode <= 3) {
      lcd.setCursor(mainCoords[mode][0] + 1, mainCoords[mode][1]);
      lcd.print(" ");
      mode = newMode;
      lcd.setCursor(mainCoords[mode][0] + 1, mainCoords[mode][1]);
      lcd.print("*");
    } else {
      beep(3);
    }
  }
}
void mainMenuHandler(const int rotation) {
  if (rotation > 0) {
    if (mainMenuId < 5) {
      beep(0);
      mainMenuId++;
    } else {
      beep(3);
    }
  } else if (rotation < 0) {
    if (mainMenuId > 0) {
      beep(0);
      mainMenuId--;
    } else {
      beep(3);
    }
  }
  if (lastMainMenuId != mainMenuId) {
    lcd.setCursor(mainCoords[lastMainMenuId][0], mainCoords[lastMainMenuId][1]);
    lcd.print(" ");
    lcd.setCursor(mainCoords[mainMenuId][0], mainCoords[mainMenuId][1]);
    lcd.print(">");
    lastMainMenuId = mainMenuId;
  }
}
void mainMenu() {
reset:
  setMenuIndex(0);
  rfunc = mainMenuHandler;
  lcd.setCursor(0,0);
  lcd.print("  Cur:    a  Run:");
  lcd.setCursor(0,1);
  lcd.print("  Vlt:    v  Tmp:");
  lcd.setCursor(0,2);
  lcd.print("  Res:    r  Fan:");
  lcd.setCursor(17,2);
  lcd.write((byte)1);
  lcd.setCursor(0,3);
  lcd.print("  Pwr:    w  Monitor");
  lcd.setCursor(mainCoords[mainMenuId][0], mainCoords[mainMenuId][1]);
  lcd.print(">");
  lcd.setCursor(mainCoords[mode][0] + 1, mainCoords[mode][1]);
  lcd.print("*");

  for (int i = 0; i < 4; i++) {
    printValue(6, i, i, setValues[i]);
  }
 
  while(decodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    updateVoltages();
    decodeRotaryEncoder(PIN_PORT & AB_BYTE);
    switch (decodeSwitch(PIN_PORT & SW_BYTE)) {
      case BTN_SHORT_RELEASE:
        switch (mainMenuId) {
          case 0 ... 3:
            Serial.print("Mode selection: ");
            Serial.println(modeNames[mainMenuId]);
            setMode(mainMenuId);
            break;
          case 4: // Run
            Serial.println("Clicked run");
            break;
          case 5: // Monitor
            Serial.println("Clicked monitor");
            debugMenu();
            goto reset;
            break;
        }
        break;
      case BTN_LONG_HOLD:
        if (mainMenuId == mode) {
          numberEditorMenu();
          goto reset;
        } else {
          beep(3);
          while(decodeSwitch(PIN_PORT & SW_BYTE) != 0);
        }
        
    }
  }
}

void loop() {
  mainMenu();
}