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

#define SET_CURRENT 0
#define SET_VOLTAGE 1
#define SET_RESISTANCE 2
#define SET POWER 3

int set[] = {1234,1625,600,1000};
String names[] = {"Current", "Voltage", "Power", "Resistance"};

LiquidCrystal_I2C lcd(0x27,20,4);  // Set the LCD address to 0x27 for a 16 chars and 2 line display
Adafruit_ADS1115 ads;
Adafruit_MCP4725 dac;

float vin;
float celcius;
float vLoad;
float aLoad;
int change = 0;
int channel = 0;
int mode = 0;
bool load = false;

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

extern void mainMenu();

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
  if (!ads.begin()) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Failed to initialize ADS.");
    delay(100);
    while (1) {
      delay(1000);
    }
  }
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
  lcd.print("Version 0.3.0 Beta");
  lcd.setCursor(1,3);
  lcd.print("Built: Sep 24, 2022");  
  unsigned long SlapshStartTime = millis();
  while (PIN_PORT & SW_BYTE && millis() - SlapshStartTime < 4000) {
    delay(1);
  }
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Voltages:");
} 

void loop(void) {
  mainMenu();
}

void updateVoltages() {
  if (ads.conversionComplete()) {
    float voltage = ads.computeVolts(ads.getLastConversionResults());
    switch (channel) {
    case 0: 
      vin = voltage;
      change |= 1;
      break;
    case 1:
      celcius = (voltage * 100) - 273.15;
      change |= 2;
      break;
    case 2:
      vLoad = voltage;
      change |= 4;
      break;
    case 3:
    aLoad = voltage;
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