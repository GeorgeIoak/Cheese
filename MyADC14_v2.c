#include "msp432.h"
#include "PhotoDiode_Sensor.h"

//#include <stdlib.h>

#define NUM_SAMPLES			8
#define MEASUREMENT_TIME	3
#define IDLE_TIME			2
#define MV_REF 1200 /* Vref[mV] */
#define ADC_BITS 12 /* Be sure to match w/ ADC config */

int sample_index=0;
int voltage_ave=0;
int millivolts;
int volts;
unsigned int val;
unsigned int min, max;
unsigned int samples[NUM_SAMPLES];
volatile unsigned int displayMode;
unsigned int readingNo;
unsigned char idleCounter = 0;
unsigned char measurementCounter = 0;
unsigned int sleeping, reading;
#define DISPLAY_MODE_CURRENT            0
#define DISPLAY_MODE_MIN                1
#define DISPLAY_MODE_MAX                2
#define DISPLAY_MODE_SLEEP              3

void updateDisplay(void);


int main(void) {
  volatile int i;
  max = 0;
  min = UINT16_MAX;
  sleeping = 0;
  reading = 0;

  diode_sensor_init();

  WDTCTL = WDTPW | WDTHOLD;                 		// Stop WDT

  // GPIO Setup
  /* Configuring P1.0 as output and P1.1 (switch) as input with pull-up resistor*/
  /* Rest of pins are configured as output low */
  P1DIR = ~(uint8_t) BIT1;
  P1OUT = BIT1;
  P1REN = BIT1;                                 // Enable pull-up resistor (P1.1 output high)
  P1SEL0 = 0;
  P1SEL1 = 0;
  P1IFG = 0;                                    // Clear all P1 interrupt flags
  P1IE = BIT1;                                  // Enable interrupt for P1.1
  P1IES = BIT1;                                 // Interrupt on high-to-low transition

  // Enable Port 1 interrupt on the NVIC
  NVIC_ISER1 = 1 << ((INT_PORT1 - 16) & 31);
  NVIC_ISER0 = 1 << ((INT_WDT_A - 16) & 31);

  P1OUT &= ~BIT0;                           		// Clear LED to start
  P1DIR |= BIT0;                            		// Set P1.0/LED to output
  P5SEL1 |= BIT4;                           		// Configure P5.4 for ADC
  P5SEL0 |= BIT4;

  P4DIR |= BIT2;                                // P4.2 output
  P4SEL0 |= BIT2;                               // P4.2 option select
  
  PJSEL0 |= BIT0 | BIT1;                        // set LFXT pin as second function
  
  CSKEY = 0x695A;                               // Unlock CS module for register access
  CSCTL2 |= LFXT_EN;                            // LFXT on
  // Loop until XT1, XT2 & DCO fault flag is cleared
  do
  {
      // Clear XT2,XT1,DCO fault flags
     CSCLRIFG |= CLR_DCORIFG | CLR_HFXTIFG | CLR_LFXTIFG;
     SYSCTL_NMI_CTLSTAT &= ~ SYSCTL_NMI_CTLSTAT_CS_SRC;
  }
  while (SYSCTL_NMI_CTLSTAT & SYSCTL_NMI_CTLSTAT_CS_FLG); // Test oscillator fault flag
  CSCTL1 = CSCTL1 & ~(SELS_M | DIVS_M) | SELA_0; // Select ACLK as LFXTCLK
  CSKEY = 0;                                    // Lock CS module from unintended accesses

  __enable_interrupt();

  readingNo = 0;
  reading = 1;

  diode_sensor_read();
  WDTCTL = WDTPW | WDTSSEL__ACLK | WDTTMSEL | WDTCNTCL | WDTIS_4;  // WDT 250ms, ACLK, interval timer
  while (1) {
      if (diode_sensor_read_result(&samples[readingNo])) {
        // We got a result from the ADC, deal with it
        readingNo = (readingNo + 1) % NUM_SAMPLES;
        if( readingNo == 0) {
          // The result was the last one needed for this sensor, calculate the average
          // of the readings, and update min/max values.
          val = 0;
          for (i=0;i<NUM_SAMPLES;i++) {
            val += samples[i];
          }
          val = val >> 3;
          if (val > max) {
            max = val;
          }
          if (val < min) {
            min = val;
          }

          voltage_ave = 0;
          for (i=0; i<NUM_SAMPLES; i++) {
          voltage_ave = voltage_ave + samples[i];
          }
          voltage_ave = voltage_ave / NUM_SAMPLES; //>> 4; // divide by 4 by right shifting 2
          //theVoltage = (voltage_ave*1.2) / 4096 * 100; //in mV
          millivolts = (voltage_ave * MV_REF) >> ADC_BITS; //shift replaces /4096
          volts = millivolts / 1000;
          for (i=0; i<NUM_SAMPLES; i++) {
            samples[i] = 0;
          }
          P1OUT ^= BIT0;  // Toggle LED0 after every average
          sample_index = 0;
            //  reset reading state and update display
            ADC14CTL0 &= ~(REFON|ADC14ON);
            reading = 0;
            measurementCounter = 0;
            //curVal = 0;
            updateDisplay();
        } else {
          // We need more readings for the current sensor
          diode_sensor_read();
        }
     }  else {
        // No ADC result. We are thus awoken by (not mutual exclusive).
        // a) the user pressing a button.
        // b) idleCounter reached the defined value.
        // c) measurementCounter reached the defined value.
        //
        // a) and b) are both handled in updateDisplay
        updateDisplay();

        if( (measurementCounter >= MEASUREMENT_TIME) && !reading ) {
          // Deal with c) here:
          reading = 1;
          diode_sensor_read();
        }
      }
      if( !reading && sleeping) {
        /* Entering LPM3 with GPIO interrupt */
        /* Setting the sleep deep bit */
        SCB_SCR |= (SCB_SCR_SLEEPDEEP);

        __sleep();
        __no_operation();                   // For debugger
      } else {
        __sleep();
      }
    }
   return 0;
 //   __no_operation();                       // For debugger
}

void updateDisplay(void) {
  unsigned int i;

  if( sleeping && displayMode == DISPLAY_MODE_SLEEP) {
    // We are sleeping, and noone is waking us
    return;
  } else if( sleeping ) {
    // Time to wake up:
    sleeping = false;
    /*P1SEL |= PWM_PIN;
    TACTL |= MC_1;
    lcd_backlight(true);
    lcd_on();
    lcd_clear();*/
  } else if( displayMode == DISPLAY_MODE_SLEEP) {
    // Time to sleep:
    sleeping = true;
    /*P1SEL &= ~PWM_PIN;
    P1OUT &= ~PWM_PIN;
    TACTL &= ~(MC0|MC1);
    lcd_backlight(false);
    lcd_clear();
    lcd_off();*/
    return;
  }
}

/* Watchdog Timer interrupt service routine */
void WdtIsrHandler(void)
{
    if( !sleeping) {
    idleCounter++;
  }

  if( !reading ) {
    measurementCounter++;
    if( measurementCounter >= MEASUREMENT_TIME) {
      ADC14CTL0 |= (REFON|ADC14ON);
      __sleep();
    }
  }

  if( idleCounter >= IDLE_TIME ) {
    displayMode = DISPLAY_MODE_SLEEP;
    idleCounter = 0;
    __sleep();
  }
}

/* Port1 ISR */
void Port1Handler(void)
{
    volatile uint32_t i, status;
    /* Toggling the output on the LED */
    if(P1IFG & BIT1)
        P1OUT ^= BIT0;

    /* Delay for switch debounce */
    for(i = 0; i < 10000; i++)

    P1IFG &= ~BIT1;

}
