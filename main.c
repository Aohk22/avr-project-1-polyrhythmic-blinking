#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sfr_defs.h>
#include <stddef.h>

// Derived from clkCPU and prescaler.
// Hertz.
#define FTICK 977

// Button stuff.
#define PIN_BUTTON PINC2
#define NOPUSH 0
#define PUSHED 1

// Buzzer stuff.
#define OC1A_PIN PORTB1

typedef enum { STARTED, PAUSED, TESTING } StateProgram;
typedef struct {
  const char port;
  const int interval; // Formula: (bar_length * 1/x) * FTICK.
  unsigned int pointInTime;
  unsigned char toggled;
  unsigned int offTime;
} LEDTiming;

/* LED timing variables. */
// LEDs should be attached to port D.
volatile const unsigned long barLen = 3 * FTICK;
volatile const unsigned long intervLED = 1 * FTICK / 10; // i.e 0.1 seconds.
volatile unsigned long blinkTimer = 0;
volatile LEDTiming timingLED1 = {1 << PORTD1, 977, 0, 0, 0};
volatile LEDTiming timingLED2 = {1 << PORTD3, 733, 0, 0, 0};
volatile LEDTiming timingLED3 = {1 << PORTD5, 586, 0, 0, 0};
volatile LEDTiming timingLED4 = {1 << PORTD7, 419, 0, 0, 0};
volatile LEDTiming *timings[4] = {&timingLED1, &timingLED2, &timingLED3,
                                  &timingLED4};
volatile const unsigned char LEDArrSize = sizeof(timings) / sizeof(timings[0]);

/* Program state. */
volatile StateProgram state = STARTED;

/* Button variables */
volatile const unsigned int debounce = 5 * FTICK / 10; // i.e 0.5 seconds.
volatile int buttonStatus = NOPUSH;
volatile int buttonTimer;

void initPins(void);
void initTimer0(void);
void initTimer2(void);

void doBlinkLEDs(void);
void doStateToggle(void);

int buttonInput(void);

ISR(TIMER0_OVF_vect) {
  if (state == STARTED) {
    blinkTimer++;
  }

  // If button is pushed and remains pushed, increase button timer.
  if (PINC & (1 << PIN_BUTTON) && buttonStatus == PUSHED) {
    buttonTimer++;
  }
}

int main() {
  initPins();
  initTimer0();
  sei();
  StateProgram prevState = state;

  while (1) {
    if (buttonInput()) {
      doStateToggle();
    }
    switch (state) {
    case STARTED:
      if (prevState != state) {
        prevState = state;
        blinkTimer = 0;
        for (int i = 0; i < LEDArrSize; i++) {
          timings[i]->pointInTime = 0;
          timings[i]->toggled = 0;
          timings[i]->offTime = 0;
        }
      }
      doBlinkLEDs();
      break;

    case PAUSED:
      if (prevState != state) {
        prevState = state;
        for (int i = 0; i < LEDArrSize; i++) {
          PORTD &= ~(timings[i]->port);
        }
      }
      break;

    case TESTING:
      // if (PINC & (1 << PIN_BUTTON)) {
      //   PORTD |= timings[0]->port;
      // } else {
      //   PORTD &= ~timings[0]->port;
      // }
      if (buttonInput()) {
        PORTD ^= timings[0]->port;
      }
      break;
    }
  }
}

void doBlinkLEDs(void) {
  for (int i = 0; i < LEDArrSize; i++) {
    if (blinkTimer >= timings[i]->pointInTime) {
      if (!timings[i]->toggled) {
        PORTD ^= timings[i]->port;
        timings[i]->toggled = 1;
        timings[i]->offTime = timings[i]->pointInTime + intervLED;
      }
    }
  }

  // LED off if it's turned on and blinkTimer has exceeded LEDs off
  // time (which is a point in time).
  for (int i = 0; i < LEDArrSize; i++) {
    if (timings[i]->toggled && blinkTimer >= timings[i]->offTime) {
      PORTD ^= timings[i]->port;
      timings[i]->toggled = 0;
      timings[i]->pointInTime += timings[i]->interval;
    }
  }
  // Resets the timings.
  if (blinkTimer >= barLen) {
    blinkTimer = 0;
    for (int i = 0; i < LEDArrSize; i++) {
      timings[i]->pointInTime = 0;
      timings[i]->offTime = 0;
      timings[i]->toggled = 0;
      PORTD &= ~timings[i]->port;
    }
  }
}

void initPins(void) {
  // Pins for LED.
  for (int i = 0; i < LEDArrSize; i++) {
    DDRD |= timings[i]->port;
  }

  DDRC &= ~(1 << PIN_BUTTON);  // Set pin as input.
  PORTC &= ~(1 << PIN_BUTTON); // Disable pull-up.

  // Pin for buzzer.
  // DDRD |= (1 << DDD3);
}

void initTimer0(void) {
  TCCR0B = (1 << CS01 | 1 << CS00); // Set clock prescaler.
  TIFR0 = (1 << TOV0);              // Clear overflow flag.
  TIMSK0 = (1 << TOIE0);            // Enable overflow interrupt.
}

// void initTimer2(void) {
//   // Timer2 PWM mode for the buzzer.
//   TCCR2A = (1 << COM2B1) | // Clear OC2B on compare match, set OC2B at
//   BOTTOM.
//            (1 << WGM21) | (1 << WGM20); // Fast PWM mode.
//   // TCCR2B = (1 << CS21 | 1 << CS00);     // Prescaler = 64.
//   TCCR2B = (1 << CS22 | 1 << CS00); // Prescaler = 1024.
//   OCR2B = (0xFF - (0xFF >> 3));     // Duty cycle.
//   // TIMSK2 = (1 << OCIE2B); // Enable interrupts.
//   /* Some notes
//   "If one or both of the COM2A1:0 bits are set, the OC2B output
//   overrides the normal port functionality of the I/O pin it is connected to."
//   */
//   //
//   https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf
//   // Example code from:
//   //
//   https://ww1.microchip.com/downloads/en/Appnotes/Atmel-2505-Setup-and-Use-of-AVR-Timers_ApplicationNote_AVR130.pdf
// }

void doStateToggle(void) {
  if (state == PAUSED) {
    state = STARTED;
  } else if (state == STARTED) {
    state = PAUSED;
  }
}

// Got idea from https://www.avrfreaks.net/s/topic/a5C3l000000U0lOEAS/t016734
// fourth comment.
int buttonInput(void) {
  if (PINC & (1 << PIN_BUTTON)) {
    if (buttonStatus == NOPUSH) {
      buttonTimer = 0;
      buttonStatus = PUSHED;
    }
    if (buttonStatus == PUSHED) {
      // `buttonTimer` incremented using clock interrupt.
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
