#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sfr_defs.h>

#define TIMER_PERIOD 0.016384 // Derived from clkCPU and prescaler.
#define BAR_LEN 3

#define USER_LED_1 PORTB5
#define USER_LED_2 PORTB4

// Button stuff.
#define BUTTON PINB0
#define NOPUSH 0
#define PUSHED 1

// Buzzer stuff.
#define OC2B_PIN PORTD3

typedef enum { STARTED, PAUSED, TESTING } StateProgram;

// Time is divided by TIMER_PERIOD to get number of overflows needed for that
// time, so unit is in number of overflows.
volatile const unsigned long barLen = BAR_LEN / TIMER_PERIOD;
volatile const unsigned long step1 =
    (unsigned long)((BAR_LEN * 1. / 4) / TIMER_PERIOD);
volatile const unsigned long step2 =
    (unsigned long)((BAR_LEN * 1. / 7) / TIMER_PERIOD);
volatile const unsigned long intervLED = 0.1 / TIMER_PERIOD;
volatile unsigned long interv1 = 0;
volatile unsigned long interv2 = 0;
volatile int toggled1 = 0;
volatile int toggled2 = 0;
volatile int offTime1 = 0;
volatile int offTime2 = 0;
volatile unsigned long blinkTimer = 0;
volatile StateProgram state = TESTING;

volatile const unsigned int debounce = 0.5 / TIMER_PERIOD;
volatile int buttonStatus = NOPUSH;
volatile int buttonTimer;

void initPins(void);
void initTimer0(void);
void initTimer2(void);
void doBlinkLEDs(void);
void stateToggle(void);
int buttonInput(void);

ISR(TIMER0_OVF_vect) {
  if (state == STARTED) {
    blinkTimer++;
  }
  // if (PIND & _BV(BUTTON) && buttonStatus == PUSHED) {
  //   buttonTimer++;
  // }
}

int main() {
  initPins();
  initTimer0();
  initTimer2();
  sei();
  StateProgram prevState = state;

  while (1) {
    // if (buttonInput()) {
    //   stateToggle();
    // }
    switch (state) {
    case STARTED:
      if (prevState != state) {
        blinkTimer = 0;
        interv1 = 0;
        interv2 = 0;
        toggled1 = 0;
        toggled2 = 0;
        offTime1 = 0;
        offTime2 = 0;
        prevState = state;
      }
      doBlinkLEDs();
      break;
    case PAUSED:
      if (prevState != state) {
        PORTB &= ~(_BV(USER_LED_1) | _BV(USER_LED_2));
        prevState = state;
      }
      break;
    case TESTING:
      if (PINB & _BV(BUTTON)) {
        PORTB |= _BV(USER_LED_1);
      } else {
        PORTB &= ~_BV(USER_LED_1);
      }
    }
  }
}

void doBlinkLEDs(void) {
  // Controls first LED.
  if (blinkTimer >= interv1) {
    if (!toggled1) {
      PORTB ^= _BV(USER_LED_1);
      toggled1 = 1;
      offTime1 = interv1 + intervLED;
    }
  }
  // Controls second LED.
  if (blinkTimer >= interv2) {
    if (!toggled2) {
      PORTB ^= _BV(USER_LED_2);
      toggled2 = 1;
      offTime2 = interv2 + intervLED;
    }
  }

  // Turns of the LED.
  if (toggled1 && blinkTimer >= offTime1) {
    PORTB ^= _BV(USER_LED_1);
    toggled1 = 0;
    interv1 += step1;
  }
  if (toggled2 && blinkTimer >= offTime2) {
    PORTB ^= _BV(USER_LED_2);
    toggled2 = 0;
    interv2 += step2;
  }
  // Resets the timings.
  if (blinkTimer >= barLen) {
    blinkTimer = 0;
    interv1 = step1;
    interv2 = step2;
    offTime1 = 0;
    offTime2 = 0;
    toggled1 = 0;
    toggled2 = 0;
    PORTB &= ~_BV(USER_LED_1);
    PORTB &= ~_BV(USER_LED_2);
  }
}

void initPins(void) {
  // Pins for LED.
  DDRB |= _BV(DDB5);
  DDRB |= _BV(DDB4);
  DDRB &= ~_BV(DDB0);    // Set pin as input.
  PORTB &= ~_BV(PORTB0); // Disable pull-up.

  // Pin for buzzer.
  DDRD |= (1 << DDD3);
}

void initTimer0(void) {
  TCCR0B = _BV(CS02) | _BV(CS00); // Set clock prescaler.
  TIFR0 = _BV(TOV0);              // Clear overflow flag.
  TIMSK0 = _BV(TOIE0);            // Enable overflow interrupt.
}

void initTimer2(void) {
  // Timer2 PWM mode for the buzzer.
  TCCR2A = (1 << COM2B1) | // Clear OC2B on compare match, set OC2B at BOTTOM.
           (1 << WGM21) | (1 << WGM20); // Fast PWM mode.
  // TCCR2B = (1 << CS21 | 1 << CS00);     // Prescaler = 64.
  TCCR2B = (1 << CS22 | 1 << CS00); // Prescaler = 1024.
  OCR2B = (0xFF - (0xFF >> 3));     // Duty cycle.
  // TIMSK2 = (1 << OCIE2B); // Enable interrupts.
  /* Some notes
  "If one or both of the COM2A1:0 bits are set, the OC2B output
  overrides the normal port functionality of the I/O pin it is connected to."
  */
  // https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf
  // Example code from:
  // https://ww1.microchip.com/downloads/en/Appnotes/Atmel-2505-Setup-and-Use-of-AVR-Timers_ApplicationNote_AVR130.pdf
}

void stateToggle(void) {
  if (state == PAUSED) {
    state = STARTED;
  } else if (state == STARTED) {
    state = PAUSED;
  }
}

// Got idea from https://www.avrfreaks.net/s/topic/a5C3l000000U0lOEAS/t016734
// fourth comment.
int buttonInput(void) {
  if (PIND & _BV(BUTTON)) {
    if (buttonStatus == NOPUSH) {
      buttonTimer = 0;
      buttonStatus = PUSHED;
    }
    if (buttonStatus == PUSHED) {
      // buttonTimer incremented using clock interrupt.
      if (buttonTimer >= debounce) {
        buttonTimer = 0;
        return 1;
      }
    }
  } else {
    if (buttonStatus == PUSHED) {
      buttonStatus = NOPUSH;
      buttonTimer = 0;
    }
  }
  return 0;
}
