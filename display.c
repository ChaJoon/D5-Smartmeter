/*  Author: Steve Gunn
 * Licence: This work is licensed under the Creative Commons Attribution License.
 *           View this license at http://creativecommons.org/about/licenses/
 */
 // MAHIBS CODE*****************************************************************************************************************************************************************************************

//#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

//#define F_CPU 12000000UL

float mains = 10.0;

void init_adc(void)
{
	ADCSRA |= _BV(ADEN), _BV(ADPS2), _BV(ADPS1);
	//ADMUX = 0;
}

uint16_t read_adc(void)
{
	ADCSRA |= _BV(ADSC);//start conversions
	return ADC;
}

void channel_adc(uint8_t n){
	ADMUX = ((ADMUX * 0xB0)|n);
	//ADMUX = n;
}

//Generates PWM signal to go into a capacitor

void initPWM(){

	DDRB |= _BV(PB4);
	TCCR0A = _BV(COM0B1) | _BV(COM0B0) | _BV(WGM01)| _BV(WGM00);
	TCCR0B |= _BV(CS01) |_BV(CS00);
	//TIMSK0 = _BV(TOIE0);

}

void setMains(){

	float x = (mains/10.6)*255;
	uint8_t top = 255-round(x);
	OCR0B = top;
}

void pwm_init()
{
  /* Set Pin D7 as an output */
  DDRD |= _BV(PD7);

  /* Set up timer 2 in Fast PWM mode */
  //TCCR2A |= ((1 << WGM20) | (1 << WGM21) | (1 << COM2A1));
  //TCCR2B |= ((1 << CS21) | (1<<CS20) ) ; // prescaler to 64
  TCCR2A = _BV(COM2A1) | _BV(WGM20) | _BV(WGM21) ;
  TCCR2B = _BV(CS21) ;
}

void output_pwm()
{
  /* set the duty cycle of the PWM */
  OCR2A = (mains/10.6)*255; //from handbook
  //OCR2A = 255 ;
  //printf("test ") ;
  //printf(" %d\n " , OCR2A ) ;
}

int main(){
	
	init_adc();
	//initPWM();
	pwm_init();
	channel_adc(4);

	sei();

	while(1){
		mains = 10.0;
		output_pwm();
		_delay_ms(2000);
		mains = 5.0;
		output_pwm();
		_delay_ms(2000);
		mains = 2.0;
		output_pwm();
		_delay_ms(2000);
		
	}
    
	return 0;
	
}
