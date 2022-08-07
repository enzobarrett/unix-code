#include <avr/sleep.h>

#define CLOCK_SPEED 16000000

volatile int secs = 0;

// handle cmp interuppt
ISR(TIMER1_COMPA_vect) {
  secs++;

  // sleep after 30 secs
  if (secs > 30) {
    // sleep
    sleep_mode();
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);

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

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second
}
