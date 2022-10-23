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

#define MODE_CC 0
#define MODE_CV 1
#define MODE_PA 2
#define MODE_PV 3
#define MODE_RA 4
#define MODE_RV 5

#define SET_CURRENT 0
#define SET_VOLTAGE 1
#define SET_POWER 2
#define SET_RESISTANCE 3

#define ID_R1 1005
#define ID_R2 38730

int set[4] = {0001,100,100,10};
float vin[4] = {0.0,0.0,0.0,0.0};
const int pwr[5] = {1,10,100,1000,10000};
int numberInputId = 0;
int lastNumberInputId = 0;
int numberValue;
int numberIncrement;
int runMenuId = 1;
int lastRunMenuId = 1;
byte change = 0;
int channel = 0;
int mode = 0;
int lastMode = 0;
bool load = false;
bool lastLoad=true;

void runMode();
void printValue(int index, int value, int row, int col);

// External devices
LiquidCrystal_I2C lcd(0x27,20,4);  // Set the LCD address to 0x27 for a 16 chars and 2 line display
Adafruit_ADS1115 ads;
Adafruit_MCP4725 dac;

// Make some custom characters
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


// ***************************************************************
// Rotatry encoder functions
// ***************************************************************
typedef void (*RotataryHandler) (const int rotation);
RotataryHandler rfunc = 0;

void decodeRotaryEncoder(uint8_t raw) {
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

int decodeSwitch(uint8_t switchState) {
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
  // Fan
  analogWrite(PIN_PWM, 0);
  pinMode(PIN_PWM, OUTPUT);
  
  // Rotary encoder
  pinMode(PIN_A, INPUT);
  pinMode(PIN_A, INPUT);
  pinMode(PIN_SW, INPUT_PULLUP);

  // Load
  digitalWrite(PIN_LOAD_SW, LOW);
  pinMode(PIN_LOAD_SW, OUTPUT);

  // OpAmp
  digitalWrite(PIN_NEGATIVE, LOW);
  pinMode(PIN_NEGATIVE, OUTPUT);
  digitalWrite(PIN_POSITIVE, LOW);
  pinMode(PIN_POSITIVE, OUTPUT);

  // Logging
  Serial.begin(BAUD_RATE);

  // Initialize the LCD
  Serial.println("Initializing LCD");
  lcd.init();
  lcd.createChar(0, degree);
  for (int i = 0; i < 8; i++) {
    lcd.createChar(i + 1, fan[i]);
  }
  lcd.backlight();
  Serial.println("LCD initialized.");

  // Initiaize the dac
  Serial.println("Initializing DAC");
  dac.begin(0x60);
  if (!dac.setVoltage(0, false)) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Failed to initialize DAC");
    delay(100);
    while (1) {
      delay(1000);
    }
  }
  Serial.println("DAC initialized.");

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
  ads.startADCReading(MUX_BY_CHANNEL[channel], false);
  Serial.println("ADS initialized.");

  // Display the splash screen for 3 seconds, 
  // or until the rotary encoder is depressed
  lcd.setCursor(2,0);
  lcd.print("Electronic Load");
  lcd.setCursor(4,1);
  lcd.print("Jason Figge");
  lcd.setCursor(1,2);
  lcd.print("Version 0.7.0");
  lcd.setCursor(1,3);
  lcd.print("Built: Oct 19, 2022");  
  unsigned long SlapshStartTime = millis();
  while (PIN_PORT & SW_BYTE && millis() - SlapshStartTime < 3000) {
    delay(1);
  }
  lcd.clear();
} 

// ***************************************************************
// Load operation
// ***************************************************************
void temperatureChange() {
  int temp = int(vin[VIN_CELCIUS]);
  int fan = 7;
  if (temp > 100) {
    analogWrite(PIN_PWM, 255);
    load = false;
    runMode();
  } else if (temp > 70) {
    analogWrite(PIN_PWM, 255);
  } else if (temp <= 30) {
    analogWrite(PIN_PWM, 0);
    fan = 1 ;
  } else {
    analogWrite(PIN_PWM, (temp - 30) * 255 / 40);
    fan = int((temp - 15) / 8);
  }

  lcd.setCursor(12, 2);
  lcd.print(temp);
  lcd.write((byte)0);
  lcd.print("C ");
  lcd.setCursor(19, 2);
  lcd.write((byte)fan);
}

void updateStatus() {
  if ((change & B00000010) > 0) {
    change &= B11111101;
    temperatureChange();
  }
}

// void displayValues(bool force) {
//   static float last[4];
//   float value = retrieveValue(i);
//   Serial.print("Retrieved value: ");
//   Serial.println(value,3);
//   if (force || last[i] != value) {
//     last[i] = value; 
//     printValue(i, value, int(i / 2) * 10 + 2, i % 2);
//   }
// }

// ***************************************************************
// Processing functions
// ***************************************************************
void wireOpAmp(bool positive) {
  digitalWrite(positive ? PIN_POSITIVE : PIN_NEGATIVE, true);
  delay(5);
  digitalWrite(positive ? PIN_POSITIVE : PIN_NEGATIVE, false);
}

void updateVoltages() {
  if (ads.conversionComplete()) {
    float voltage = ads.computeVolts(ads.getLastConversionResults());
    switch (channel) {
    case 0: 
      if (vin[VIN_VOLTAGE] != voltage) {
        vin[VIN_VOLTAGE] = voltage;
        printValue(0, voltage, int(0 / 2) * 10 + 2, 0 % 2);
      }
      break;
    case 1: {
      int x = int((voltage * 100) - 273.15);
      if (x != vin[VIN_CELCIUS]) {
        vin[VIN_CELCIUS] = x;
        change |= B00000010;
        printValue(0, voltage, int(0 / 2) * 10 + 2, 0 % 2);
      }
      break;
    }
    case 2: {
      float v = ((voltage * (ID_R1 + ID_R2))  / ID_R1);
      if (vin[VIN_VLOAD] != v) {
        vin[VIN_VLOAD] = v;
        change |= B00000100;
        printValue(0, voltage, int(0 / 2) * 10 + 2, 0 % 2);
      }
      break;
    }
    case 3:
      if (vin[VIN_ALOAD] != voltage) {
        vin[VIN_ALOAD] = voltage;
        change |= B00001000;
        printValue(0, voltage, int(0 / 2) * 10 + 2, 0 % 2);
      }
      break;
    }
    channel = (channel + 1) % 4;
    ads.startADCReading(MUX_BY_CHANNEL[channel], false);
    if (change) {
      updateStatus();
    }
  }
}

void configureMode() {
  lcd.setCursor(0,2);
  load = false;
  switch (mode) {
    case MODE_CC: 
      Serial.println("Constant current    "); 
      wireOpAmp(true);
      break;
    case MODE_CV: 
      Serial.println("Constant voltage    "); 
      wireOpAmp(false);
      break;
    case MODE_PA: 
      Serial.println("Constant power by current "); 
      wireOpAmp(true);
      break;
    case MODE_PV: 
      Serial.println("Constant power by voltage "); 
      wireOpAmp(false);
      break;
    case MODE_RA: 
      Serial.println("Constant resist by current"); 
      wireOpAmp(true);
      break;
    case MODE_RV: 
      Serial.println("Constant resist by voltage"); 
      wireOpAmp(false);
      break;
  }
}

void runMode() {
  if (lastLoad != load) {
    Serial.print("Load: ");
    Serial.println(load ? "On" : "Off");
    lcd.setCursor(2,2);
    lcd.print(load ? "On" : "Off");
  }
    digitalWrite(PIN_LOAD_SW, load ? HIGH : LOW);
}

float retrieveValue(int value) {
  switch(value) {
    case SET_CURRENT:
      return mode == MODE_CC ?  float(set[SET_CURRENT]) / 1000.0 : vin[VIN_ALOAD];
      break;
    case SET_VOLTAGE: 
      return mode == MODE_CV ? set[SET_VOLTAGE] : vin[VIN_VLOAD];
    case SET_POWER:
      return (mode == MODE_PA || mode == MODE_PV) ? set[SET_POWER] : vin[VIN_VLOAD] * vin[VIN_ALOAD];
    case SET_RESISTANCE:
      return (mode == MODE_RA || mode == MODE_RA) ? set[SET_RESISTANCE] : vin[VIN_VLOAD] / vin[VIN_ALOAD];
  }
  Serial.print("Unexpected retrieveValue parameter called: ");
  Serial.println(value);
  lcd.clear();
  lcd.setCursor(0,0);
  Serial.print("Unexpected retrieve-");
  lcd.setCursor(0,1);
  Serial.print("parameter called:");
  lcd.setCursor(0,3);
  Serial.print("Halted");
  Serial.print(value);
  while (true) delay(1000);
}

// ***************************************************************
// User Interface functions
// ***************************************************************
void printValue(int index, int value, int row, int col) {
  char buf[10];  
  switch (index) {
  case 0:
    snprintf_P(buf, sizeof(buf), PSTR("%01d.%03dA"), value / 1000, value % 1000);
    break;
  case 1:
    snprintf_P(buf, sizeof(buf), PSTR("%02d.%02dV"), value / 100, value % 100);
    break;
  case 2:
    snprintf_P(buf, sizeof(buf), PSTR("%03d.%01dW"), value / 10, value % 10);
    break;
  case 3:
    snprintf_P(buf, sizeof(buf), PSTR("%05dR"), value);
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

  while(decodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    decodeRotaryEncoder(PIN_PORT & AB_BYTE);
    if (lastNumberInputId != numberInputId) {
      lastNumberInputId = numberInputId;
      printValue(index, numberValue, int(index / 2) * 10 + 2, index % 2);
      lcd.setCursor(int(index / 2) * 10 + digitId + (digitId <= index ? 2 : 3), index % 2);
    }
    switch (decodeSwitch(PIN_PORT & SW_BYTE)) {
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

void printRunMenuSelector() {
  if (lastRunMenuId != runMenuId) {
    lastRunMenuId = runMenuId;
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
  rfunc = runMenuHandler;
  lcd.setCursor(1,3);
  lcd.print(" CC CV Pa Pv Ra Rv");
  temperatureChange();
start:
  runMenuPosition(0, 1);
  lcd.print("*");
  runMode();
  printRunMenuSelector();

  while(decodeSwitch(PIN_PORT & SW_BYTE) != 0);
  while(true) {
    updateVoltages();
    decodeRotaryEncoder(PIN_PORT & AB_BYTE);
    switch (decodeSwitch(PIN_PORT & SW_BYTE)) {
      case BTN_SHORT_RELEASE:
        switch (runMenuId) {
        case 0:
          load = false;
          switch (mode) {
            case 4: case 5: numberInput(3); break;
            case 3: numberInput(2); break;
            default: numberInput(mode);
          }
          goto start;
        case 1:
          load = !load;
          goto start;
        default:
          if (mode != runMenuId - 2) {
            mode = runMenuId - 2;
            configureMode();
          }
          goto start;return;
        }
      case BTN_LONG_HOLD:
        runMenuId = 1;
        lastRunMenuId = 1;
        goto start;
    }
  }
}
