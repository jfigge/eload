#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MCP4725.h>

#define BTN_UP 0
#define BTN_DOWN 1
#define BTN_SHORT_HOLD 2
#define BTN_LONG_HOLD 3
#define BTN_SHORT_RELEASE 4
#define BTN_LONG_RELEASE 5

#define VIN_VOLTAGE 0
#define VIN_CELCIUS 1
#define VIN_VLOAD 2
#define VIN_ALOAD 3

int set[4] = {1234,1625,600,1000};
float vin[4] = {0,0,0,0};
const int pwr[5] = {1,10,100,1000,10000};
int numberInputId = 0;
int lastNumberInputId = 0;
int numberValue;
int numberIncrement;
int runMenuId = 1;
int lastRunMenuId = 1;
int change = 0;
int channel = 0;
int mode = 0;
bool load = false;

// External devices
LiquidCrystal_I2C lcd(0x27,20,4);  // Set the LCD address to 0x27 for a 16 chars and 2 line display
Adafruit_ADS1115 ads;
Adafruit_MCP4725 dac;

// Make some custom characters
uint8_t degree[8]  = {140,146,146,140,128,128,128,128};

// ***************************************************************
// Rotatry encoder
// ***************************************************************
typedef void (*RotataryHandler) (const int rotation);
RotataryHandler rfunc = 0;

void DecodeRotaryEncoder(uint8_t raw) {
  static uint8_t states[] = {0,4,1,0,2,1,1,0,2,3,1,2,2,3,3,0,5,4,4,0,5,4,6,5,5,6,6,0};
  static uint8_t lastRaw = -1;
  static uint8_t lastState = -1;
  if (lastRaw != raw) {
    lastRaw = raw;
    uint8_t state = states[(raw >> AB_SHIFT) | (lastState << 2)];
    if (rfunc != 0 && state == 0) {
      if (lastState == 6) {
        rfunc(-1);
      } else if (lastState == 3) {
        rfunc(1);
      }
    }
    lastState = state;
  }
}

int DecodeSwitch(uint8_t switchState) {
  static long timer;
  static uint8_t lastSwitchState = -1;
  if (lastSwitchState != switchState) {
    lastSwitchState = switchState;
    if (!switchState) {
      timer = millis();
      return BTN_DOWN;
    } else {
      timer = millis() - timer;
      return timer > 1000 ? BTN_LONG_RELEASE : BTN_SHORT_RELEASE;
    }
  } else if (!switchState) {
    return millis() - timer > 1000 ? BTN_LONG_HOLD : BTN_SHORT_HOLD;
  }
  return BTN_UP;
}

// ***************************************************************
// Setup
// ***************************************************************
void setup() {
  // *********************
  // Debugging
  // *********************
  Serial.begin(BAUD_RATE);

  dac.begin(0x60);
  dac.setVoltage(0, false);

  // initialize the lcd 
  lcd.init();
  lcd.createChar(0, degree);
  lcd.backlight();

  // The ADC input range (or gain) can be changed via the following
  // functions, but be careful never to exceed VDD +0.3V max, or to
  // exceed the upper and lower limits if you adjust the input range!
  // Setting these values incorrectly may destroy your ADC!
  //                                                                ADS1015  ADS1115
  //                                                                -------  -------
  // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
  // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
  // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
  // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
  // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
  // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
  Serial.println("Initializing ADS");
  if (!ads.begin()) {
    lcd.clear();
    lcd.setCursor(0,0);
    Serial.println("Failed to initialize ADS.");
    lcd.print("Failed to initialize ADS.");
    delay(100);
    while (1) {
      delay(1000);
    }
  }
  Serial.println("ADS initialized.");
  ads.startADCReading(MUX_BY_CHANNEL[channel], false);
  pinMode(PIN_PWM, OUTPUT);
  digitalWrite(PIN_PWM, LOW);
  pinMode(PIN_A, INPUT);
  pinMode(PIN_A, INPUT);
  pinMode(PIN_SW, INPUT_PULLUP);
  digitalWrite(PIN_RELAY, LOW);
  pinMode(PIN_RELAY, OUTPUT);

  // Display the splash screen for 4 seconds, or until the rotary
  // encoder is depressed
  lcd.setCursor(2,0);
  lcd.print("Electronic Load");
  lcd.setCursor(4,1);
  lcd.print("Jason Figge");
  lcd.setCursor(1,2);
  lcd.print("Version 0.4.0");
  lcd.setCursor(1,3);
  lcd.print("Built: Sep 24, 2022");  
  unsigned long SlapshStartTime = millis();
  while (PIN_PORT & SW_BYTE && millis() - SlapshStartTime < 4000) {
    delay(1);
  }
  lcd.clear();

} 

// ***************************************************************
// Processing functions
// ***************************************************************
void updateVoltages() {
  if (ads.conversionComplete()) {
    float voltage = ads.computeVolts(ads.getLastConversionResults());
    switch (channel) {
    case 0: 
      vin[VIN_VOLTAGE] = voltage;
      change |= 1;
      break;
    case 1:
      vin[VIN_CELCIUS] = (voltage * 100) - 273.15;
      change |= 2;
      break;
    case 2:
      vin[VIN_VLOAD] = voltage;
      change |= 4;
      break;
    case 3:
    vin[VIN_ALOAD] = voltage;
      change |= 8;
      break;
    }
    channel = (channel + 1) % 4;
    ads.startADCReading(MUX_BY_CHANNEL[channel], false);
  }
}

void configureMode() {
  lcd.setCursor(0,2);
  switch (mode) {
    case 0: Serial.println("Constant current    "); break;
    case 1: Serial.println("Constant voltage    "); break;
    case 2: Serial.println("Constant power by current "); break;
    case 3: Serial.println("Constant power by voltage "); break;
    case 4: Serial.println("Constant resist by current"); break;
    case 5: Serial.println("Constant resist by voltage"); break;
  }
}

void runMode() {
    Serial.print("Load: ");
    Serial.println(load ? "On" : "Off");
}

// ***************************************************************
// User Interface
// ***************************************************************
void printValue(int index, int value, int row, int col) {
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

// ***************************************************************
// Main loop
// ***************************************************************
void loop(void) {
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
  lcd.print(vin[VIN_CELCIUS]);
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
          return;
        case 1:
          load = !load;
          runMode();
          return;
        default:
          mode = runMenuId - 2;
          configureMode();
          return;
        }
      case BTN_LONG_HOLD:
        runMenuId = 1;
        lastRunMenuId = 1;
        return;
    }
  }
}
