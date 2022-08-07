// Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include "RTClib.h"
//#include "LowPower.h"
#include "EEPROM.h"
//#define AUTOSLEEP
RTC_DS3231 rtc;

// multiplex registers
int latchPin = 8;
int clockPin = A2;
int dataPin = A3;
int OE = 6;

// segment register
int latch2 = 7;
int clock2 = 10;
int data2  = 9;

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

#ifdef AUTOSLEEP
volatile int sleepCount = 0;
volatile int sleepPending = 0;
#endif

void setup () {

  
  // multiplex shift register pins
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(OE, OUTPUT);

  // segment shift register pins
  pinMode(latch2, OUTPUT);
  pinMode(clock2, OUTPUT);
  pinMode(data2, OUTPUT);


  // make sure segment register isn't outputing
  digitalWrite(OE, HIGH);

  // set appropriate pullups
  pinMode(3, INPUT_PULLUP);
  pinMode(ststp, INPUT_PULLUP);
  pinMode(mode, INPUT_PULLUP);


  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, 0);
  digitalWrite(latchPin, HIGH);
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, 1);
  digitalWrite(latchPin, HIGH);

  // switch dataPin to INPUT so it doesn't mess up with the feedback loop
  pinMode(dataPin, INPUT);
  Serial.begin(19200);

    lamptest();

  noInterrupts();
  attachInterrupt(digitalPinToInterrupt(mode), modeChange, FALLING);
  attachInterrupt(digitalPinToInterrupt(3), khz, RISING);
  interrupts();


  // if rtc does not start, halt
  if (! rtc.begin()) {
    Serial.println("rtc not found");
    while (1);
    
  }
  if (EEPROM.read(4) == 0)
  {
    uint32_t toAdjust = DateTime(F(__DATE__), F(__TIME__)).unixtime() + 8;
    rtc.adjust(DateTime(toAdjust));
    EEPROM.write(4, 1);
  }

  if (rtc.lostPower()) {
    // Set date to last stored in EEPROM
    unsigned long ftime = 0;

    for (int i = 3; i >= 0; i--)
    {
      ftime <<= 8;
      ftime |= EEPROM.read(i);
    }
    rtc.adjust(DateTime(ftime));
  }

  rtc.writeSqwPinMode(DS3231_OFF);


  // Set dispMode to 0 incase it is magically 1
  if (dispMode)
  {
    dispMode = 0;
  }

  // run quick lamp test
  //lamptest();

#ifdef AUTOSLEEP
  // setup timer for auto sleep
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A = 62500;            // compare match register
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS12) | (1 << CS10);    // 1024 prescaler
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();             // enable all interrupts
  TCNT1  = 0;
  sleepCount = 0;
#endif
}

void modeChange()
{
#ifdef AUTOSLEEP
  if (sleepPending) {
    sleepPending = 0;
    modePressed = 1;
    modeCD = 25;
  } else
#endif
    if (!modePressed)
    {
      //Serial.write("modepressed\n");

      if (dispMode == 4)
      {
        //reset = 1;
        dispMode = 0;
      } else {
        dispMode++;
      }
      modePressed = 1;
      modeCD = 25;
    }
#ifdef AUTOSLEEP
  TCNT1  = 0;
  sleepCount = 0;
#endif
}

int lookup(int i)
{
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

int lookupHex(char i)
{
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
    case '-':
      return 253;
    case 'p':
      return 49;
  }
}

void displayHex(uint32_t t)
{
  //clock 2 times
  pulse();
  pulse();
  sprintf(hex, "%lx", t);

  for (int i = 0; i < 8; i++)
  {
    shiftData(lookupHex(hex[i]));

    flash();
    pulse();
  }
}

void startStop()
{
  if (stopping == 0)
  {
    Serial.println("here");
    stopping = 1;
    //rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
    //rtc.writeSqwPinMode(DS3231_SquareWave1kHz);
    //rtc.writeSqwPinMode(DS3231_SquareWave4kHz);
    //rtc.writeSqwPinMode(DS3231_SquareWave8kHz);
    rtc.writeSqwPinMode(DS3231_SquareWave8kHz);
  }
  else
  {
    stopping = 0;
    rtc.writeSqwPinMode(DS3231_OFF);
  }
}

void shiftData(int x)
{
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, x);
  digitalWrite(latch2, HIGH);
}

void displayHHMMSSA(DateTime n)
{
  pulse();

  int hour = 33;

  if (n.hour() > 12)
    hour = n.hour() - 12;
  else if (n.hour() == 0)
    hour = 12;
  else
    hour = n.hour();

  shiftData(lookup(hour / 10));
  flash();
  pulse();

  shiftData(lookup(hour % 10));
  flash();
  pulse();

  shiftData(lookupHex('-'));
  flash();
  pulse();

  shiftData(lookup(n.minute() / 10));
  flash();
  pulse();

  shiftData(lookup(n.minute() % 10));
  flash();
  pulse();

  shiftData(lookupHex('-'));
  flash();
  pulse();

  shiftData(lookup(n.second() / 10));
  flash();
  pulse();

  shiftData(lookup(n.second() % 10));
  flash();
  pulse();

  if (n.hour() > 12)
    shiftData(lookupHex('p'));
  else
    shiftData(lookupHex('a'));

  flash();
  pulse();
}

void displayMMDDYYYY(DateTime n)
{
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookup(n.month() / 10));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookup(n.month() % 10));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();


  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookupHex('-'));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookup(n.day() / 10));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();
  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookup(n.day() % 10));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();

  digitalWrite(latch2, LOW);
  shiftOut(data2, clock2, LSBFIRST, lookupHex('-'));
  digitalWrite(latch2, HIGH);
  flash();
  pulse();

  int currentYear = n.year();

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

void pulse()
{
  digitalWrite(latchPin, LOW);
  digitalWrite(clockPin, HIGH);
  digitalWrite(clockPin, LOW);
  digitalWrite(latchPin, HIGH);
}

void flash()
{
  digitalWrite(OE, LOW);
  delay(1);
  digitalWrite(OE, HIGH);

}

void khz() {
  //Serial.println(millies);
  millies++;
  if (millies % 32000 == 0)
  {
    Serial.println(stp);
    stp++;
    millies = 0;
  }
}
void displayNum(uint32_t t)
{
  int x = 9;
  while (x >= 0)
  {
    unix[x] = t % 10;
    t /= 10;
    x--;
  }
  for (int i = 0; i < 10; i++)
  {

    //Serial.println(lookup(unix[i]));
    digitalWrite(OE, HIGH);
    digitalWrite(latch2, LOW);
    shiftOut(data2, clock2, LSBFIRST, lookup(unix[i]));
    digitalWrite(latch2, HIGH);
    flash();
    pulse();
  }



}

void displayStop()
{
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
  Serial.println(millies / 320);
  shiftData(lookup(millies / 3200));
  flash();
  pulse();

}

void lamptest() {
  //Serial.println("lamp testing...");
  for (int i = 0; i < 200; i++)
  {
    for (int x = 0; x < 10; x++)
    {
      //Serial.println(lookup(unix[i]));
      digitalWrite(OE, HIGH);
      digitalWrite(latch2, LOW);
      shiftOut(data2, clock2, LSBFIRST, 0);
      digitalWrite(latch2, HIGH);
      flash();
      pulse();
    }
  }
}

#ifdef AUTOSLEEP
// sleep interrupt
ISR(TIMER1_COMPA_vect)          // timer compare interrupt service routine
{
  if (sleepCount < 2)
  {
    sleepCount++;
  } else {
    sleepPending = 1;
    sleepCount = 0;
  }
}
#endif


void loop () {
  DateTime now = rtc.now();
  //Serial.println(now.unixtime());
  if (dispMode == 0)
    displayNum(now.unixtime() + 25200);
  else if (dispMode == 1)
    displayHex(now.unixtime() + 25200);
  else if (dispMode == 2)
    displayMMDDYYYY(now);
  else if (dispMode == 3)
    displayHHMMSSA(now);
  else if (dispMode == 4)
  {
    if (reset == 1)
    {
      if (stopping == 1)
        startStop();
      millies = 0;
      stp = 0;
      dispMode = 0;
      reset = 0;
    }
    if (digitalRead(4) == LOW)
    {
      if (!stspp)
      {
        startStop();
        stspp = 1;
        wait = 20;
      }
    }
    if (stspp)
    {
      wait--;
    }
    if (wait == 0)
      stspp = 0;



    displayStop();

  }
  //Serial.println(stp);
  if (modePressed)
  {
    modeCD--;
  }
  if (modeCD == 0) {
    modePressed = 0;
  }

#ifdef AUTOSLEEP
  if (sleepPending) {
    uint32_t junkunix = now.unixtime();
    for (int i = 0; i < 4; i++)
    {
      EEPROM.write(i, junkunix & 255);
      junkunix >>= 8;
    }
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

  }
#endif
}
