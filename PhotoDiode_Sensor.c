#include "PhotoDiode_Sensor.h"

#include "msp432.h"


unsigned int diode_sensor_result_ready;

void diode_sensor_init(void) {

    // Initialize the shared reference module
	// By default, REFMSTR=1 => REFCTL is used to configure the internal reference
	while(REFCTL0 & REFGENBUSY);              		// If ref generator busy, WAIT
	
	REFCTL0 |= REFVSEL_0 + REFON;             		// Enable internal 1.2V reference
  NVIC_ISER0 = 1 << ((INT_ADC14 - 16) & 31);      // Enable ADC interrupt in NVIC module

  while(!(REFCTL0 & REFGENRDY));	            	// Wait for reference generator
                                          		// to settle

  // Configure ADC14
  ADC14CTL0 = ADC14SHT0_2 | ADC14SHP | ADC14ON;	// Sampling time, S&H=16, ADC14 on
  ADC14CTL1 = ADC14RES_2;                   		// Use sampling timer, 12-bit conversion results

  //ADC14MCTL0 |= ADC14INCH_1;                	// A1 ADC input select; Vref=AVCC
  ADC14MCTL0 =ADC14VRSEL_1 + ADC14INCH_1;      	// A1 ADC input select; Vref=1.2V
  ADC14IER0 |= ADC14IE0;                    		// Enable ADC conv complete interrupt

  SCB_SCR &= ~SCB_SCR_SLEEPONEXIT;           		// Wake up on exit from ISR

  diode_sensor_result_ready = 0;
}

unsigned int diode_sensor_read_result(unsigned int *res) {
  if( diode_sensor_result_ready) {
    diode_sensor_result_ready = 0;
    *res = ADC14MEM0;
    return 1;
  }
  return 0;
}

void diode_sensor_read(void) {
  diode_sensor_result_ready = 0;
  ADC14CTL0 |= ADC14ENC | ADC14SC;        // Start sampling/conversion
}

// ADC14 interrupt service routine

void ADC14IsrHandler(void) {
//    ADC14CTL0 &= ~(ADC14ENC);
    diode_sensor_result_ready = 1;
    __sleep();
  }

