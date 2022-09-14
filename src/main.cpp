#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MCP4725.h>

// #define PIN_SCL 5
// #define PIN_SDA 4
// #define PIN_A 2
// #define PIN_B 1
// #define PIN_SW 0
#define PIN_SCL A5
#define PIN_SDA A4
#define PIN_A 3
#define PIN_B 2
#define PIN_SW 4


LiquidCrystal_I2C lcd(0x27,20,4);  // Set the LCD address to 0x27 for a 16 chars and 2 line display
Adafruit_ADS1115 ads;
Adafruit_MCP4725 dac;

float vin;
float ain1;
uint16_t dacv = -1;

int switchState = HIGH;
volatile int count = 500;
volatile int last_count = 0;

// Make some custom characters
uint8_t degree[8]  = {140,146,146,140,128,128,128,128};

typedef void (*RotataryHandler) (const int rotation);
RotataryHandler rfunc = 0;
void REUpdateISR() {
  static uint8_t states[] = {0,1,4,0, 0,1,1,2, 2,1,3,2, 0,3,3,2, 0,4,4,5, 5,6,4,5, 0,6,6,5};
  static uint8_t last_state = 0;
  uint8_t state = states[((PIND & B00001100) >> 2) | (last_state << 2)];
  if (rfunc != 0 && state == 0) {
    if (last_state == 6) {
      rfunc(-1);
    } else if (last_state == 3) {
      rfunc(1);
    }
  }
  last_state = state;
}

void rotaryHandler(const int rotation) {
  if (rotation > 0) {
    if (count < 999) {
      count++;
    }
  } else if (rotation < 0) {
    if (count > 0) {
      count--;
    }
  }
}

void setup() {
  // *********************
  // Debugging
  // *********************
  Serial.begin(BAUD_RATE);

  dac.begin(0x60);
  dac.setVoltage(0, false);

  // Print a message to the LCD.
  lcd.init();                      // initialize the lcd 
  lcd.createChar(0, degree);
  lcd.backlight();
  lcd.setCursor(2,0);
  lcd.print("Electronic Load");
  lcd.setCursor(4,1);
  lcd.print("Jason Figge");
  lcd.setCursor(1,2);
  lcd.print("Version 0.0.3 Beta");
  lcd.setCursor(1,3);
  lcd.print("Built: Sep 11, 2022");


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
    lcd.setCursor(0,0);
    lcd.print("Failed to initialize ADS.");
    while (1);
  }

  pinMode(PIN_A, INPUT);
  pinMode(PIN_A, INPUT);
  pinMode(PIN_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_A), REUpdateISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_B), REUpdateISR, CHANGE);
  rfunc = rotaryHandler;

  // Display the splash screen for 4 seconds, or until the rotary
  // encoder is depressed
  unsigned long SlapshStartTime = millis();
  while (PIND & B00010000 && millis() - SlapshStartTime < 4000) {
    delay(1);
  }
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Voltages:");
} 

float kelvin;
uint16_t tmp;
void loop(void) {
  static long last_update = millis();
  static uint8_t last_switchState = switchState;
  
  if (millis() - last_update > 500) {
    for(int i=0; i < 4; i++) {
      int16_t adc = ads.readADC_SingleEnded(i);
      lcd.setCursor(9,i);
      switch (i) {
        case 0:
          vin = ads.computeVolts(adc);
          lcd.print(ads.computeVolts(adc));
          lcd.write('V');
          break;
        case 1:
          ain1 = ads.computeVolts(adc);
          lcd.print(ain1);
          lcd.write('V');
          lcd.write((byte)B01111110);
          tmp = (ain1 * 4096) / vin;
          if (tmp != dacv) {
            dac.setVoltage(tmp, false, 400);
            dacv = tmp;
          }
          break;
        case 2:
          lcd.print(ads.computeVolts(adc));
          lcd.write('V');
          lcd.write((byte)B01111111);
          break;
        case 3: {
          kelvin = ads.computeVolts(adc) * 100;
        }
      }
    }

    float celsius=kelvin - 273.15;
    lcd.setCursor(0,3);
    lcd.print(celsius);
    lcd.write((byte)0);
    lcd.print("C");
  }

  switchState = PIND & B00010000 ? HIGH : LOW;
  if (last_switchState != switchState) {
    last_switchState = switchState;
    lcd.setCursor(0,1);
    lcd.print(switchState == LOW ? "Down" : "Up  ");
  }

  if (last_count != count) {
    lcd.setCursor(0,2);
    char buffer[3];
    sprintf(buffer, "%3d", count);
    lcd.print(buffer);
    last_count = count;
  }
}