#include <GyverTM1637.h>
#include <AnalogKey.h>
#include <GyverButton.h>
#include <GyverNTC.h>
#include <TimerMs.h>
#include <EEPROM.h>

#define BUZZ_PIN 2
#define BTN_PIN 3
#define RELE_PIN 4
#define CLK 5 //Display
#define DIO 6 //Display
#define BLU_LED 9 // Blu led intocator

uint8_t temp;
uint8_t potVal;
uint8_t gstVal = 3;
uint32_t timerRele = 0;
uint16_t periodWorkRele = ((1000 * 60) * 60) * 4; // 4 Години в міліснкундах
//uint16_t periodWorkRele = 10000*15; // 4 Години в міліснкундах
bool profilePot = 0;
bool profileGst = 0;
bool warningTemp = 0;
bool startPeriodTimerRele = 0;
bool offMode = 0;

TimerMs timerNTC(1000, 1, 0);
TimerMs timerControlWarningTemp(2000, 1, 0);
TimerMs timerPID(750, 1, 0);
TimerMs timerDisplayTemp(1100, 1, 0);
TimerMs timerPotVal(250, 1, 0);
TimerMs timerHold(3000, 1, 0);
GButton butt1(BTN_PIN);
GyverNTC therm(1, 10000, 3450);
GyverTM1637 disp(CLK, DIO);



void setup() {
  Serial.begin(9600);
  disp.clear();
  disp.brightness(7);  // яркость, 0 - 7 (минимум - максимум)
  pinMode(BLU_LED, OUTPUT);
  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, HIGH);
  butt1.setDebounce(50);        // настройка антидребезга (по умолчанию 80 мс)
  butt1.setTimeout(1000);        // настройка таймаута на удержание (по умолчанию 500 мс)
  butt1.setClickTimeout(600);   // настройка таймаута между кликами (по умолчанию 300 мс)
  butt1.setType(HIGH_PULL);
  butt1.setDirection(NORM_OPEN);
  tone(BUZZ_PIN, 2100, 750);
  Serial.println("Start system");
}

void loop() {
  butt1.tick();
  btnHelper();
  if (offMode) {
    releControl(0);
    analogWrite(BLU_LED, 80);
    displayOff();
    
  } else {
    digitalWrite(BLU_LED, LOW);
    tempTick();
    controlWarnibfTempTick();
    displayTick();
    potTick();
    pidTick();
    periodWorkReleTick();
  }
  
  
}

void displayTick() {
  if (timerDisplayTemp.tick()) {
    displayTemp(temp);
    displayGst();
    displayPot();
  }
}

void potTick() {
  if (timerPotVal.tick()) {
    potVal = map(analogRead(A0), 0, 1023, 40, 80);
    potVal = constrain(potVal, 40, 80);
  }
}

void tempTick() {
  if (timerNTC.tick()) {
    temp = therm.getTempAverage();
  }
}

void displayTemp(int temp) {
  if (!profilePot && !profileGst) {
    disp.clear();
    if (temp >= 10 && temp <= 99) {
      disp.display(2, int(temp / 10));
      disp.display(3, int(temp % 10));
    } else if (temp >= 0 && temp <= 9) {
      disp.display(2, 0);
      disp.display(3, int(temp));
    } 
     disp.displayByte(0, _t);
  }
}

void displayPot() {
  if (profilePot && !profileGst) {
    disp.clear();
    int temp = potVal;
    if (temp >= 10 && temp <= 99) {
      disp.display(2, int(temp / 10));
      disp.display(3, int(temp % 10));
    } else if (temp >= 0 && temp <= 9) {
      disp.display(2, 0);
      disp.display(3, int(temp));
    } 
     disp.displayByte(0, _F);
  }
}

void displayGst() {
  if (!profilePot && profileGst) {
    disp.clear();
    int temp = potVal;
    temp = map(temp, 40, 80, 2, 7);
    if (temp >= 0 && temp <= 9) {
      disp.display(2, 0);
      disp.display(3, int(temp));
    } 
    disp.displayByte(0, _P);
  }
}

void displayOff() {
    disp.displayByte(0, _0);
    disp.displayByte(1, _F);
    disp.displayByte(2, _F);
}

void btnHelper() {
  if (butt1.isClick() ) {
    piBuzz();
    startPeriodTimerRele = 0;
    offMode = 0;
    if (profilePot) {
      EEPROM.put(0, potVal);
      profilePot = 0;
    }
    if (profileGst) {
      EEPROM.put(1, gstVal);
      profileGst = 0;
    }
  }

  if (butt1.isDouble()) {
    profilePot = 1;
    profileGst = 0;
  }
  if (butt1.isTriple()) {
    profilePot = 0;
    profileGst = 1;
  }

  if (timerHold.tick()) {
    if (butt1.isHold()) {
      offMode = 1;
      
    }
  }
}

void piBuzz() {
  tone(BUZZ_PIN, 2100, 100);
}

void pidTick() {
  if (timerPID.tick()) {
    pidReleControl();
  }
}

void pidReleControl() {
  byte maxTemp, gst;
  EEPROM.get(0, maxTemp);
  EEPROM.get(1, gst);
  if (temp >= maxTemp) {
    releControl(0);
  }
  if ( temp <= (maxTemp - gst) ) {
    releControl(1);
  }
}

void releControl(bool status) {
  if (warningTemp) {
    digitalWrite(RELE_PIN, HIGH);
    startPeriodTimerRele = 0;
  } else {
    if (status) {
      digitalWrite(RELE_PIN, LOW);
      if (!startPeriodTimerRele) {
        startPeriodTimerRele = 1;
        timerRele = millis();
      }
    } else {
      digitalWrite(RELE_PIN, HIGH);
      startPeriodTimerRele = 0;
    }
  }
}

void controlWarnibfTempTick() {
  if (timerControlWarningTemp.tick()) {
    warningTemp = (temp > 80 || temp <= -5) ;
  }
}

void periodWorkReleTick() {
  if (startPeriodTimerRele && millis() - timerRele >= periodWorkRele) {
    offMode = 1;
  }
}
