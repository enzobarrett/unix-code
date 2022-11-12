// Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include "RTClib.h"
#include "EEPROM.h"
#include <Timezone.h>    // https://github.com/JChristensen/Timezone
#include <avr/sleep.h>
#include <RV-3028-C7.h>
#include "TimeZones.h"
#include <math.h>
RV3028 rtc;

#define NOP __asm__ __volatile__ ("nop\n\t")

// multiplex registers
int latchPin = 8;
int clockPin = A2;
int dataPin = A3;
int OE = 6;

// segment register
int latch2 = 7;
int clock2 = 10;
int data2  = 9;

// sqw pin
int swp = 3;
// startstop pin
int ststp = 4;
// mode pin
int mode = 2;

// mode state
volatile int dispMode = 0;
// array to hold reversed unix time
int unix[10];
// array to hold hex unixtime
char hex[15];
// array to hold year
int yr[4];

// milliseconds for stopwatch
volatile int millies = 0;
int wait = 0;
int stopping = 0;
volatile unsigned long stp = 0;
int stopA[9];
volatile int reset = 0;
volatile int modePressed = 0;
volatile int modeCD = 0;
int stspp = 0;

// set clock index
volatile int index = 0;
volatile int currentSetValue = 0;
volatile int setValue[10] = {0};
volatile uint32_t setResult = 0;
volatile bool setNewTime = false;

// set clock toggle
bool toggle = false;
int toggleTimer = 0;

// GMT
volatile double GMTOffset = TimeZones::getCurrent();
bool setGMT = false;


#define CLOCK_SPEED 16000000

// auto sleep
volatile int secs = 0;
bool sleep = false;

void setup () {
  Serial.begin(115200);

  // multiplex shift register pins
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  // segment shift register pins
  pinMode(latch2, OUTPUT);
  pinMode(clock2, OUTPUT);
  pinMode(data2, OUTPUT);
  pinMode(OE, OUTPUT);

  // make sure segment register isn't outputing
  digitalWrite(OE, HIGH);

  // set appropriate pullups
  pinMode(swp, INPUT_PULLUP);
  pinMode(ststp, INPUT_PULLUP);
  pinMode(mode, INPUT_PULLUP);

  lamptest();

  // set up ints
  noInterrupts();
  attachInterrupt(digitalPinToInterrupt(mode), modeChange, FALLING);
  attachInterrupt(digitalPinToInterrupt(swp), khz, RISING);
  interrupts();

  // for rtc
  Wire.begin();

  // if rtc does not start, halt
  if (! rtc.begin()) {
    Serial.println("rtc not found");
    while (1);
  }

  // try not to blow up the capacitor
  rtc.disableTrickleCharge();

  // don't increment stopwatch
  rtc.disableClockOut();
  rtc.setBackupSwitchoverMode(1);
  millies = 0;

  // auto sleep

  // setup lowest sleep as default
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  // stop interrupts
  cli();

  // clear registers
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1C = 0;

  // set compare register
  // number of /1024 pulses for 1HZ
  OCR1A = ( CLOCK_SPEED / (1024 * 1) ) - 1;

  // turn on CTC mode
  TCCR1B |= 1 << WGM12;

  // set prescaler to slow (/1024)
  TCCR1B |= 1 << CS10;
  //TCCR1B |= 1 << CS11;
  TCCR1B |= 1 << CS12;

  // reset timer
  TCNT1 = 0;

  // enable cmp interrupt
  TIMSK1 |= 1 << OCIE1A;

  // reenable interrupts
  sei();
}

void loop () {
  //Serial.println(GMTOffset);
  //rtc.updateTime();
  if (dispMode == 0)
    displayNum(rtc.getUNIX());
  else if (dispMode == 1)
    displayHex(rtc.getUNIX());
  else if (dispMode == 2)
    displayMMDDYYYY();
  else if (dispMode == 3) {
    rtc.updateTime();
    displayHHMMSSA();
  } else if (dispMode == 4) {
    if (reset == 1) {
      if (stopping == 1)
        startStop();
      millies = 0;
      stp = 0;
      dispMode = 0;
      reset = 0;
    }

    if (digitalRead(4) == LOW) {
      if (!stspp) {
        startStop();
        stspp = 1;
        wait = 20;
      }
    }

    if (stspp) {
      wait--;
    }

    if (wait == 0)
      stspp = 0;

    displayStop();
  } else if (dispMode == 5) {
    if (toggleTimer == 0) {
      toggle = !toggle;
      toggleTimer = 10;
    } else {
      toggleTimer--;
    }

    displayDigit(index, currentSetValue, toggle);

    if (digitalRead(ststp) == LOW) {
      if (stspp == 0) {
        if (currentSetValue == 9)
          currentSetValue = 0;
        else
          currentSetValue = currentSetValue + 1;

        stspp = 1;
        wait = 15;
      }
    }

    if (stspp)
      wait--;
    if (wait == 0) {
      stspp = 0;
    }
  } else if (dispMode == 6) {
    if (toggleTimer == 0) {
      toggle = !toggle;
      toggleTimer = 10;
    } else {
      toggleTimer--;
    }

    displayGMT(toggle);

    if (digitalRead(ststp) == LOW) {
      if (stspp == 0) {
        GMTOffset = TimeZones::getNext();

        stspp = 1;
        wait = 15;
      }
    }

    if (stspp)
      wait--;
    if (wait == 0) {
      stspp = 0;
    }
  }

  if (modePressed) {
    modeCD--;
  }

  if (modeCD == 0) {
    modePressed = 0;
  }

  if (setNewTime == true) {
    uint32_t UNIX = 1234567890;
    if (rtc.setUNIX(setResult) == false) {
      Serial.println("Something went wrong setting the time");
    }
    rtc.updateTime();
    setNewTime = !setNewTime;
    dispMode = 6;
  }

  if (setGMT == true) {
    TimeChangeRule rule = {"LOCAL", Second, Sun, Mar, 2, (int) (60 * GMTOffset)};
    //Serial.println(60*GMTOffset);
    //TimeChangeRule rule = {"LOCAL", Second, Sun, Mar, 2, 0};
    Timezone zone(rule, rule);
    time_t utc = rtc.getUNIX();
    time_t local = zone.toLocal(utc);

    Serial.println(utc);
    Serial.println(local);
    Serial.println(year(local));

    if (rtc.setTime(second(local) + 1, minute(local), hour(local), weekday(local), day(local), month(local), year(local)) == false)
      Serial.println("Something went wrong setting the time");

    rtc.updateTime();

    setGMT = !setGMT;
    dispMode = 0;
  }
}

void modeChange() {
  // if sleeping, wakeup
  sleep_disable();

  // don't change the mode if awaking
  if (sleep == true) {
    sleep = false;
  }

  if (!modePressed) {
    secs = 0;

    int read = digitalRead(ststp);
    if (read == LOW) {
      dispMode = 5;
      stspp = 1;
      wait = 20;
    } else if (dispMode == 4) {
      dispMode = 0;
    } else if (dispMode == 5) {
      if (index == 9) {
        setValue[index] = currentSetValue;

        setResult = 0;
        for (int i = 0; i < 10; i++) {
          setResult *= 10;
          setResult += setValue[i];
        }

        setNewTime = true;

        index = 0;
      } else {
        setValue[index] = currentSetValue;
        currentSetValue = 0;
        index++;
      }
    } else if (dispMode == 6) {
      setGMT = true;
    } else {
      dispMode++;
    }

    modePressed = 1;
    modeCD = 25;
  }
}

void startStop() {
  if (stopping == 0)
  {
    stopping = 1;
    rtc.enableClockOut(FD_CLKOUT_8192);
  }
  else
  {
    stopping = 0;
    rtc.disableClockOut();
    //rtc.writeSqwPinMode(DS3231_OFF);
  }
}

void displayNum(uint32_t t) {
  int x = 9;
  while (x >= 0)
  {
    unix[x] = t % 10;
    t /= 10;
    x--;
  }
  for (int i = 0; i < 10; i++)
  {
    digitalWrite(latch2, LOW);
    shiftOut(data2, clock2, LSBFIRST, lookup(unix[i]));
    digitalWrite(latch2, HIGH);
    flash();
    pulse();
  }
}

void displayDigit(int position, int num, bool toggle) {
  int i = 0;

  for (i = 0; i < position; i++) {
    shiftData(lookup(setValue[i]));
    flash();
    pulse();
  }

  if (toggle)
    shiftData(0xFF);
  else
    shiftData(lookup(num));
  flash();
  pulse();

  for (i = 10 - position - 1; i > 0; i--) {
    shiftData(0xFF);
    flash();
    pulse();
  }
}

void displayGMT(bool toggle) {
  int i = 0;

  for (i = 0; i < 5; i++) {
    shiftData(0xFF);
    flash();
    pulse();
  }

  shiftData(lookupHex('g'));
  flash();
  pulse();

  if (GMTOffset < 0)
    shiftData(lookupHex('-'));
  else
    shiftData(0xFF);
  flash();
  pulse();

  int posOffset = abs(GMTOffset);
  int digitTwo = posOffset % 10;
  int digitOne = posOffset / 10;

  if (toggle || digitOne == 0)
    shiftData(0xFF);
  else
    shiftData(lookup(digitOne));
  flash();
  pulse();

  if (toggle)
    shiftData(0xFF);
  else
    shiftData(lookup(digitTwo) & 0xFE);
  flash();
  pulse();

  if (toggle)
    shiftData(0xFF);
  else
    shiftData(lookup((int)(fabs(GMTOffset) * 10) % 10));
  flash();
  pulse();
}

#if 1

void displayHex(uint32_t t) {
  Serial.println(t);
  // one-zero-L
  sprintf(hex, "%10lx", t);
  Serial.println(hex);

  for (int i = 0; i < 10; i++)
  {
    shiftData(lookupHex(hex[i]));
    flash();
    pulse();
  }
}

void displayMMDDYYYY() {
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookup(rtc.getMonth() / 10));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookup(rtc.getMonth() % 10));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();

  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookupHex('-'));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookup(rtc.getDate() / 10));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookup(rtc.getDate() % 10));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();

  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookupHex('-'));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();

  int currentYear = rtc.getYear();

  for (int x = 0; x < 4; x++)
  {
    yr[x] = currentYear % 10;
    currentYear /= 10;
  }

  for (int i = 3; i >= 0; i--)
  {
    digitalWrite(latch2, LOW);
    shiftOut(data2, clock2, LSBFIRST, lookup(yr[i]));
    digitalWrite(latch2, HIGH);
    flash();
    pulse();
  }
}

void displayHHMMSSA() {
  pulse();

  int hour = 33;

  if (rtc.getHours() > 12)
    hour = rtc.getHours() - 12;
  else if (rtc.getHours() == 0)
    hour = 12;
  else
    hour = rtc.getHours();

  shiftData(lookup(hour / 10));
  flash();
  pulse();

  shiftData(lookup(hour % 10));
  flash();
  pulse();

  shiftData(lookupHex('-'));
  flash();
  pulse();

  shiftData(lookup(rtc.getMinutes() / 10));
  flash();
  pulse();

  shiftData(lookup(rtc.getMinutes() % 10));
  flash();
  pulse();

  shiftData(lookupHex('-'));
  flash();
  pulse();

  shiftData(lookup(rtc.getSeconds() / 10));
  flash();
  pulse();

  shiftData(lookup(rtc.getSeconds() % 10));
  flash();
  pulse();

  Serial.println(rtc.getHours());

  if (rtc.getHours() >= 12)
    shiftData(lookupHex('p'));
  else
    shiftData(lookupHex('a'));

  flash();
  pulse();
}

void displayStop() {
  //Serial.println(stp);
  long junk = stp;
  int x = 0;
  for (int x = 0; x < 9; x++)
  {
    stopA[x] = junk % 10;
    junk /= 10;
  }

  for (int i = 8; i >= 0; i--)
  {
    if (i == 0)
      shiftData(lookup(stopA[i]) - 1);
    else
      shiftData(lookup(stopA[i]));

    flash(); int ledPin = 13;

    pulse();
  }

  shiftData(lookup(millies / 819));
  flash();
  pulse();

}

#endif

int lookup(int i) {
  switch (i) {
    case 0:
      return 3;
    case 1:
      return 159;
    case 2:
      return 37;
    case 3:
      return 13;
    case 4:
      return 153;
    case 5:
      return 73;
    case 6:
      return 65;
    case 7:
      return 31;
    case 8:
      return 1;
    case 9:
      return 25;
  }
}

int lookupHex(char i) {
  switch (i) {
    case '0':
      return 3;
    case '1':
      return 159;
    case '2':
      return 37;
    case '3':
      return 13;
    case '4':
      return 153;
    case '5':
      return 73;
    case '6':
      return 65;
    case '7':
      return 31;
    case '8':
      return 1;
    case '9':
      return 25;
    case 'a':
      return 17;
    case 'b':
      return 193;
    case 'c':
      return 99;
    case 'd':
      return 133;
    case 'e':
      return 97;
    case 'f':
      return 113;
    case 'g':
      return 67;
    case '-':
      return 253;
    case 'p':
      return 49;
    case '.':
      return 254;
    default:
      return 3;
  }
}

void shiftData(int x) {
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, x);
  digitalWrite(latch2, HIGH);
}

int pulseCount = 9;

void pulse() {
  if (pulseCount == 9) {
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, 1);
    digitalWrite(dataPin, LOW);
    digitalWrite(latchPin, HIGH);
    pulseCount = 0;
  } else {
    pulseMultiplexer();
    pulseCount++;
  }
}

void pulseMultiplexer() {
  digitalWrite(latchPin, LOW);
  digitalWrite(clockPin, HIGH);
  digitalWrite(clockPin, LOW);
  digitalWrite(latchPin, HIGH);
}

void flash() {
  digitalWrite(OE, LOW);
  //delay(1);

  for (int i = 0; i < 2200; i++)
    NOP;

  digitalWrite(OE, HIGH);
}

void lamptest() {
  // this is test method 1, all 1s in multiplex

  // shift all 0s into segement register
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, 0);
  digitalWrite(latch2, HIGH);

  // shift all 1s into multiplex register
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, 255);
  shiftOut(dataPin, clockPin, MSBFIRST, 255);
  digitalWrite(latchPin, HIGH);

  // display
  digitalWrite(OE, LOW);
  delay(200);
  digitalWrite(OE, HIGH);

  // test 2, multiplexing the shift register

  // clear multiplex register
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, 0);
  shiftOut(dataPin, clockPin, MSBFIRST, 0);
  digitalWrite(latchPin, HIGH);

  // slowly multiplex
  digitalWrite(OE, LOW);
  delay(10);
  for (int i = 0; i < 20; ++i) {
    pulse();
    delay(10);
  }
  digitalWrite(OE, HIGH);
  pulse();
}

void lampclear() {
  for (int x = 0; x < 10; x++) {
    digitalWrite(latch2, LOW);
    shiftOut(data2, clock2, LSBFIRST, 0xFF);
    digitalWrite(latch2, HIGH);
    flash();
    pulse();
  }
}

void khz() {
  //Serial.println(millies);
  millies++;
  if (millies % 8192 == 0)
  {
    //Serial.println(stp);
    stp++;
    millies = 0;
  }
}

// auto sleep
// handle cmp interuppt
ISR(TIMER1_COMPA_vect) {
  if (dispMode != 4)
    secs++;

  // sleep after 30 secs
  if (secs > 30) {
    secs = 0;

    // reset the LCDs
    lampclear();

    // sleep
    noInterrupts();          // make sure we don't get interrupted before we sleep
    sleep_enable();          // enables the sleep bit in the mcucr register
    sleep = true;           // set so we know to not change the mode
    modePressed = 1; // debounce
    modeCD = 25;
    interrupts();           // interrupts allowed now, next instruction WILL be executed
    sleep_cpu();            // here the device is put to sleep
  }
}
