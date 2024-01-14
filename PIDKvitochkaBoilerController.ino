#include <GyverTM1637.h>
#include <TimerMs.h>
#include <EEPROM.h>
#include <DS_raw.h>
#include <microDS18B20.h>
#include <microOneWire.h>
#include <EncButton.h>

EncButton eb(12, 10, 9);
MicroDS18B20<5> sensor;
#define BUZZ_PIN 2
#define BTN_PIN 4
#define RELE_PIN 2
#define CLK 7 //Display
#define DIO 8 //Display
#define BLU_LED 9 // Blu led intocator

uint8_t temp;
uint8_t potVal = 40;
uint8_t gstVal = 3;
uint32_t timerRele = 0;
uint32_t periodWorkRele = 14400000; // 4 Години в міліснкундах
int periodWorkReleH = (periodWorkRele/1000/60/60);
//uint16_t periodWorkRele = 1000*15; // 15 c перевірка
bool profilePot = 0;
bool profileGst = 0;
bool warningTemp = 0;
bool lockMode = 1;
bool startPeriodTimerRele = 0;
bool offMode = 0;

TimerMs timerNTC(1000, 1, 0);
TimerMs timerPOT(300, 1, 0);
TimerMs timerControlWarningTemp(2000, 1, 0);
TimerMs timerPID(750, 1, 0);
TimerMs timerDisplayTemp(1100, 1, 0);


GyverTM1637 disp(CLK, DIO);



void setup() {
  Serial.begin(9600);

  disp.clear();
  disp.brightness(7);  // яркость, 0 - 7 (минимум - максимум)
  pinMode(BLU_LED, OUTPUT);
  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, HIGH);
  eb.setBtnLevel(LOW);
    eb.setClickTimeout(500);
    eb.setDebTimeout(50);
    eb.setHoldTimeout(600);
    eb.setStepTimeout(200);

    eb.setEncReverse(1);
    eb.setEncType(EB_STEP4_LOW);
    eb.setFastTimeout(30);

    // сбросить счётчик энкодера
    eb.counter = 0;
  
  EEPROM.get(0, potVal);
  EEPROM.get(1, periodWorkReleH);
  periodWorkRele = (periodWorkReleH*1000*60*60);
  if (periodWorkRele == 0) {
    periodWorkRele = 1000*60*60; // 1 Година
  }
  Serial.println("Start system");
  Serial.println( periodWorkRele );
}

void loop() {
  eb.tick();
  lockTick();
  bHelper();
  tempTick();
  if (offMode) {
    releControl(0);
    displayOff();
  } else {
    
    
    controlWarnibfTempTick();
    displayTick();
    potTick();
    gstTick();
    pidTick();
    periodWorkReleTick();
  }
  
  
}

void lockTick() {
  if (lockMode) {
    disp.brightness(1);
  } else {
    disp.brightness(6);
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
    byte inc = 1;
   if (eb.fast()) {
    inc =  2 ;
   }
   if (eb.right() && potVal <= 95) {
      potVal = potVal + inc;
   }
   if (eb.left() && potVal >= 5){
    potVal = potVal - inc;
   }
   if (eb.click()) {
    EEPROM.put(0, potVal);
    profilePot = 0;
    profileGst = 0;
   }
}

void tempTick() {
  if (timerNTC.tick()) {
    temp = getTemoDs18b20();
//    Serial.println(temp);
  }
}

void displayTemp(int temp) {
  if (!profilePot && !profileGst) {
//    disp.clear();
//    disp.brightness(2);
    if (temp != 255) {
      disp.displayInt(temp);
    } else {
      byte troll[4] = {_E, _r, _r, _t};
      disp.scrollByte(troll, 100);
    }
    
  }
}

void gstTick() {
  if (profileGst) {
    
    if (eb.left() && periodWorkReleH >= 1) {
      periodWorkReleH = periodWorkReleH - 1;
    }
    if (eb.right() && periodWorkReleH <= 9) {
      periodWorkReleH = periodWorkReleH + 1;
    }
    if (eb.click()) {
      EEPROM.put(1, periodWorkReleH);
      profileGst = 0;
      profilePot = 0;
    }
  }
}

void displayPot() {
  if (profilePot && !profileGst) {
//    disp.brightness(5);
    int temp = potVal;
    if (temp >= 10 && temp <= 99) {
      disp.display(2, int(temp / 10));
      disp.display(3, int(temp % 10));
    } else if (temp >= 0 && temp <= 9) {
      disp.display(2, 0);
      disp.display(3, int(temp));
    } 
     disp.displayByte(0, _t);
     disp.displayByte(1, _r);
  }
}

void displayGst() {
  
  if (!profilePot && profileGst) {
    
//    Serial.print("ReleTimeout GST:");
//    Serial.println(periodWorkReleH);
//    Serial.print("ReleTimeout:");
//    Serial.println(periodWorkRele);
//    disp.brightness(5);
//    temp = map(temp, 30, 85, 1, 10);
    if (periodWorkReleH >= 0 && periodWorkReleH <= 9) {
      disp.display(2, 0);
      disp.display(3, int(periodWorkReleH));
    } 
    disp.displayByte(0, _H);
    disp.displayByte(1, _t);
    
  }
}

byte getByteToDigit(byte d) {
  switch(d) {
    case 0:
      return _0;
    case 1:
      return _1;
    case 2:
      return _2;
    case 3:
      return _3;
    case 4:
      return _4;
    case 5:
      return _5;
    case 6:
      return _6;
    case 7:
      return _7;
    case 8:
      return _8;
    case 9:
      return _9;
  }
  return _empty;
}

void displayOff() {
//    disp.brightness(1);
    byte welcome_banner[] = {_O, _F, _empty, _empty};
    if (temp >= 10 && temp <= 99) {
    welcome_banner[2] = getByteToDigit(int(temp / 10));
    welcome_banner[3] = getByteToDigit(int(temp % 10));
  } else if (temp >= 0 && temp <= 9) {
    welcome_banner[2] = getByteToDigit(0);
    welcome_banner[3] = getByteToDigit(int(temp));
  } 
  disp.displayByte(welcome_banner);
}

void bHelper() {
  uint8_t btnState = (bool)digitalRead(BTN_PIN);
//  profilePot = (bool)digitalRead(BTN_PIN);
//  if (profilePot ) {
//    EEPROM.get(0, potVal);
//  }
  if (offMode && eb.click()) {
    offMode = 0;
  }
   if (eb.hold()) {
    offMode = 1;
   }
  if (eb.hasClicks(2) && !lockMode) {
    EEPROM.get(0, potVal);
    profileGst = 0;
    profilePot = 1;
    
  }
  if (eb.hasClicks(3) && !lockMode) {
    profileGst = 1;
    profilePot = 0;
  }
  if (lockMode && btnState) {
    if (eb.hasClicks(3)) {
      lockMode = 0;
      Serial.println("Unlock!");
    }
  }
  if (!lockMode && btnState && eb.hasClicks(4)) {
    lockMode = 1;
    Serial.println("Lock!");
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
  gst = 1;
  if (temp >= (maxTemp + gst)) {
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
    warningTemp = (temp > 90 || temp <= -10) ;
  }
}

void periodWorkReleTick() {
  if (startPeriodTimerRele && millis() - timerRele >= periodWorkRele) {
    offMode = 1;
  }
}


int getTemoDs18b20() {
  uint16_t tempDs = 0;
  sensor.requestTemp();
  if (sensor.readTemp()) tempDs = int(sensor.getTemp());
  else tempDs = -273;
  return tempDs;
}
