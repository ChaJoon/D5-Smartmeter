/*
Smart Meter Software Control
Author : Syazwan
*/

//avr-gcc -mmcu=atmega644p -L../lcdlib -o prog.elf prog.c -llcd
//avr-objcopy -O ihex prog.elf prog.hex
//avrdude -c usbasp -p m644p -U flash:w:prog.hex

/*
Logs : For personal use. Will be deleted in final submission
27/01/2023  : First day of coding,, revising il matto and avr C.
              Using simple calculation and implementation of adc 
              to read the current and display it at PORTB. (Hoping 
              that i'll get and easy TFT display to code instead
              of the one LCD display which is very annoying to 
              implement from scratch)
30/01/2023  : Studying about embedded C and how to use the display
              again. Figuring out how to print new line because the
              tft display will not read \n or \r so need a few logic
              to figure out how to print next line. Adding one more 
              include library which is the string.h to use strlen
              function that can take the length of a string.
31/01/2023  : Realising that i cannot use port a or c for this whole
              thing since al of em are being used by the tft display.
              Modfying the code and adding comments
02/02/2023  : Start to code the ADC conversion function(initialization
              and reading). Debugging on why the fuck the display is
              acting weird(print random stuff). 
              WHY IS IT NOT WORKINGGGGGGGGGGGG >:( 
03/02/2023  : Finally debugged the display. Apparently it wasnt the 
              display. Its that im trying to print double instead of 
              string. Studied a few type casting and trying to make
              it perfect so I dont have to come back to this.
              Changing the printing functions so it takes less space
              Figuring out i/o pins thats need to be connected.
              Defining all the input and output pins for easy reference
              in the future.
04/02/2023  : Redefining the pins since I realise I cannot use PWM and 
              Digital output in the same port.(interference will occur).
              Seperating the digital inout with difference port than 
              analogue inout.
              Declared and defined pwm initialization function and 
              the Dutycyle funtion.
              More research on ADC and PWM.(More in physical logbook) 
              I should start trying to do the comparing and conditions
              tmrw. Im StArvIng......
05/02/2023  : Read the guidebook, theres a part where I can use to 
              approximate the value of current so that I dont have to 
              use shunt resistor to calculate it.
              2 more meetings at tuesday and thursday.
              tuesday 11 - 12 ;
              thursday 10-11 ;
10/02/2023  : DONE ADC NOT GOING BACK. Setup pwm in the same day.
              Need to wait for Jimmy to prepare LabvView simulation in order
              to check the functionality of the algorithm. Need to complete
              the code for control flow while waiting.
11/02/2023  : Rest day. No work is been done today.
12/02/2023  : Setup and completed all the inputs and outputs.
13/02/2023  : The display is back blinking again mainly because
              I did not set the ports correctly. Using the PWM 
              function make the display broke for some reason.
              Probably because a general purpose inout pins (PA7)
              cannot be used as a PWM output.
14/02/2023  : Read the datasheet again and will try to use the
              different pins for the output compare of the timer(PD7)
              This will make it harder to wire the display since the 
              ports available will be diagonal. Will eat more space 
              than ideal.
15/02/2023  : Got the connector needed for the display and got it working 
              port B and C. PWM seems to work based on PuTTY the OCR2A
              value corresponds to the value of load demands that is called.
16/02/2023  : Started working on display design.
19/02/2023  : Still working on the display this whole time. Added a load switch
              responds to the calls.
              Display is blinking again. Tracked the bug by commenting diifferent
              lines. Foudn out I cant use both sprintf and dtosrtf or the display
              blinks. Use sprintf and the display will print '?' for the double 
              type. Solution is to use only dtostrf. Made a colourful TEAMB 
              name on the display. Might need to change this so that it works 
              better with the pixel. Might even delete this if it consume too much 
              power than neccessary.
21/02/2023  : Placed this code in Github so my team can help using source control.
22/02/2023  : in delay function changed i type from int to uint32_t.
              Need a smoother RGB colour flow. Need to work for the second review
              report now.
*/

#include "../lcdlib/lcd.h"
#include "../lcdlib/ili934x.h"  
#include "../lcdlib/avrlcd.h"
#include <avr/io.h>
#include <avr/interrupt.h>
//#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "debug.h"

#define F_CPU 12000000UL
#define VREF 3.3
#define ADCMAX 1023
#define GREY 0xA514

/*Analogue input pins*/
#define WIND PA0
#define SOLAR PA1
#define BUSBAR_VOLTAGE PA2
#define BUSBAR_CURRENT PA3

/* Analogue output pins (PWM) */
#define MAIN_CALL PD7 

/* Digital Input pins */
#define LOAD1_CALL PA4 
#define LOAD2_CALL PA5 
#define LOAD3_CALL PA6 

/* Digital output pins */
#define CHARGE_BATTERY PD3 
#define DISCHARGE_BATTERY PD3 
#define LOAD1_SWITCH PD4 
#define LOAD2_SWITCH PD5 
#define LOAD3_SWITCH PD6 

/* Double for Mains equired */
double mains_req = 0;

/* Doubles for input values */
double main_capacity = 0;
double busbar_voltage = 0;
double busbar_current = 0;
double wind_capacity = 0;
double solar_capacity = 0;

/* boolean array for load calls and switches */
bool load_call[3] = {true, true, true};
bool load_switch[3] = {true, false, false};

/* boolean for charging and discharging battery */
bool charge_battery = false;
bool discharge_battery = false;

volatile uint32_t counter ; 
volatile uint16_t teamcolour ; 

/* Function Definintions */

ISR(TIMER1_COMPA_vect)
{
    /* Counter incremented every 1 millisec */
    counter++;

    /* For the coloured team RGB */
    static bool direction = true;
    const uint16_t max_color = 0xFFFC;
    const uint16_t increment = 1;
    if (direction) {
        teamcolour += increment;
        if (teamcolour >= max_color) {
            teamcolour = max_color;
            direction = false;
        }
    } else {
        teamcolour -= increment;
        if (teamcolour <= 255) {
            teamcolour = 255;
            direction = true;
        }
    }
}

void timer_init()
{ 
    TCCR1A |= 0;          //Timer for cycle control. Working at 1ms interval

    TCCR1B |= _BV(WGM12); //Cycle to cycle

    TCCR1B |= _BV(CS11);  //Prescaler set to 8

    OCR1A = 1500;         //Max value to count every 1ms

    TIMSK1 |= _BV(OCIE1A);//Enable interrupt
}

void adc_init()
{
    /*  ADEN = 1 : Enable the ADC
     * ADPS2 = 1 : Configure ADC prescaler
     * ADPS1 = 1 : F_ADC = F_CPU / 64
     * ADPS0 = 0 :       = 187.5 kHz
     */
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1); //Register values 1110 added
}

void digital_init()
{
  /* set inputs */
  DDRA &= ~(_BV(LOAD1_CALL) | _BV(LOAD2_CALL) | _BV(LOAD3_CALL));
  
  PORTA &= ~(_BV(LOAD1_CALL) | _BV(LOAD2_CALL) | _BV(LOAD3_CALL));
  
  /* set outputs */ 
  DDRD |= _BV(LOAD1_SWITCH) | _BV(LOAD2_SWITCH) | _BV(LOAD3_SWITCH);
  
  PORTD &= ~(_BV(LOAD1_SWITCH) | _BV(LOAD2_SWITCH) | _BV(LOAD3_SWITCH));
}

void pwm_init()
{
  /* Set Pin D7 as an output */
  DDRD |= _BV(MAIN_CALL);

  /* Set up timer 2 in Fast PWM mode */
  TCCR2A |= ((1 << WGM20) | (1 << WGM21) | (1 << COM2A1));
  TCCR2B |= ((1 << CS20)); // no prescaler
  OCR2A = 0 ;
}

double input_adc_read(uint8_t channel)
{
  double adcreading = 0;

  ADMUX = channel ;                    //Select the respective channel as ADC input
  
  ADCSRA |= _BV(ADSC);                 //conversion starts
  
  while (ADCSRA & _BV(ADSC));          //Wait for the conversion to finish

  adcreading = ((ADC*VREF) / ADCMAX) ;
   
  return (double) adcreading ;         //return the result of the conversion as a double
}

double input_calculate_rms(uint8_t channel)
{
  /*To find rms, need to find the max voltage at
  1 period. 50 Hz = 20 ms*/
  uint32_t periodEnd = counter + 20 ;
  double val = 0 ;
  double maxval = 0;
  /* in a period of 20ms, find the max amplitude */
  while (counter < periodEnd)
  {
    val = input_adc_read(channel) ;
    if (val > maxval)
      maxval = val ;
      //break;
  }
    maxval = (maxval / 2); // find the rms (due to the wave not being fully rectified, /2 intead of /sqrt(2) )
    return maxval ;

}

void input_digital()
{
  /* Checks load1 call */
  int inp = PINA&_BV(LOAD1_CALL) ;
  /* Set the array */
  if(inp)
    load_call[0] = true ;
  else
    load_call[0] = false ;

  /* Checks load2 call */
  inp = PINA&_BV(LOAD2_CALL) ;
  /* Set the array */
  if(inp)
    load_call[1] = true ;
  else
    load_call[1] = false ;

  /* Checks load3 call */
  inp = PINA&_BV(LOAD3_CALL) ;
  /* Set the array */
  if(inp)
    load_call[2] = true ;
  else
    load_call[2] = false ;
}

void output_pwm()
{
  /* set the duty cycle of the PWM */
  OCR2A = (mains_req/10) * 255 ; //from handbook
}

void output_digital()
{
  /* Set the load switch 1 high */
  if(load_switch[0]==true)
    PORTD |= _BV(LOAD1_SWITCH) ;
  else
    PORTD &= ~(_BV(LOAD1_SWITCH)) ;
    
  /* Set the load switch 2 high */
  if(load_switch[1]==true)
    PORTD |= _BV(LOAD2_SWITCH) ;
  else
    PORTD &= ~(_BV(LOAD2_SWITCH)) ;
    
  /* Set the load switch 3 high */
  if(load_switch[2]==true)
    PORTD |= _BV(LOAD3_SWITCH) ;
  else
    PORTD &= ~(_BV(LOAD3_SWITCH)) ;

  /* Set the charging battery switch */
  if(charge_battery)
    PORTD |= _BV(CHARGE_BATTERY) ;
  else
    PORTD &= ~(_BV(CHARGE_BATTERY)) ;
  
  /* Set the discharge battery switch */
  if(discharge_battery)
    PORTD |= _BV(DISCHARGE_BATTERY) ;
  else
    PORTD &= ~(_BV(DISCHARGE_BATTERY)) ;
}

void read_inputs()
{
  /* Read the analog values */
  wind_capacity = input_adc_read(0) ;
  solar_capacity = input_adc_read(1) ;
  busbar_voltage = input_calculate_rms(2);
  busbar_current = input_calculate_rms(3);

  /* Read the digital values */
  input_digital() ;
}

void send_outputs()
{
  /* Send the outputs */
  output_pwm() ;
  output_digital() ;
}

double check_load_demand(bool call , double current)
{
  /* if load is calling, we return the value of the
  current required by the load */
  if(call==true)
    return current ;
  else
    return 0 ;
}

void algorithm(bool load1_call, bool load2_call, bool load3_call)
{
  /* Initialize variables */
  mains_req = 0 ;
  charge_battery = 0 ;
  discharge_battery = 0 ;
  int i = 0 ;

  /* Maximum mains current */
  double max_mains_current = 4.0 ;
  
  /* Calculate total load current requirements */
  double load1_current = check_load_demand(load1_call, 0.8) ;
  double load2_current = check_load_demand(load2_call, 1.8) ;
  double load3_current = check_load_demand(load3_call, 1.4) ;
  double total_load_current = load1_current + load2_current + load3_current ;

  for(i = 0 ; i < 3 ; i++ )
  {
    if ( load_call[i] )  
      load_switch[i] = true ;
    else
      load_switch[i] = false ;
  }

  /* calculate total energy available */
  double total_energy = ((wind_capacity + solar_capacity)/2/VREF * 5) ;

  /* Check if we have enough energy */
  if(total_energy >= total_load_current)
  {
    /* Find if we have extra current */
    double excess_current = total_energy - total_load_current ;

    if(excess_current >= 1)
    {
      /* If more than 1A we can charge the batt */
      charge_battery = true ;
      discharge_battery = false ;
    }
    else if ((excess_current < 1) && (excess_current > 0))
    {
      /* Request extra current from main to charge battery */
      mains_req = ((1.0 - excess_current)*(10/max_mains_current)) ;
      charge_battery = true ;
      discharge_battery = false ;
    }
    else
    {
      /* If current from renewable is the same as load, batt in idle state */
      charge_battery = false ;
      discharge_battery = false ;
    }
  }

  else 
  {
    /* If renewable energy is less than load demands */
    double lack_current = total_load_current - total_energy ;
    if((lack_current > 0 ) && (lack_current <= 3))
    {
      /* Request from main and leave batt on idle*/
      mains_req = lack_current * (10/max_mains_current) ;
      charge_battery = false ;
      discharge_battery = false ;
    }
    else if (lack_current > 3)
    {
      /* If lack of current is too high we discharge batt as well as requesting from main */
      double current_needed = lack_current - 1 ;\
      mains_req = current_needed * (10/max_mains_current) ;
      charge_battery = false ;
      discharge_battery = true ;
    }
  }

}

/* Display double type on screen */
void display_double(int x_position, int y_position, char* name, double value, char* units)
{
  /* Set font colour as grey */
  display.foreground = GREY ;

  /* Set the position on the screen */
  display.x = x_position ; 
  display.y = y_position ; 
  
  /* Temporary value for converting double to string */
  char temp_str[10] ;
  //int temp ;
  
  /* Converting double to string */
  dtostrf( value , 4, 2, temp_str );
  //sprintf(temp_str,"%s",temp) ;
  
  /* Print the converted double as string */
  display_string(name) ;
  display_string(temp_str) ;
  display_string(units) ;
  
  /* Set new line */
  display.y += 20 ;
} 

int display_bool_check(bool load)
{
  /* Check for boolean. If the logic is high then we set the textx to print in green,
    if false we will print the text in red. */
  if(load==true)
    return GREEN ;
  else
    return RED ;
}

void display_loads(int x_position , int y_position , char* name , bool load_call , bool load_switch )
{
  /* Set the position on the display. */
  display.x = x_position ;
  display.y = y_position ;

  /* Start to print the strings. */
  display_string(name) ;
  display.foreground = display_bool_check(load_call) ;
  display_string(" CALL ") ;
  display.foreground = display_bool_check(load_switch) ;
  display_string(" SWITCH ") ;
  display.foreground = GREY ;
  display.y += 20 ;
}

void display_batt(int x_position , int y_position , char* name , bool charging , bool discharging )
{
  /* Set the position on the display */
  display.x = x_position ;
  display.y = y_position ;

  /* start print on the display */
  display_string(name) ;

  /* New line */
  display.x = x_position ;
  display.y += 10 ;

  /* Display current state of the battery */
  if(charging==true)
  {
    display.foreground = GREEN ;
    display_string(" CHARGING  ") ;
  }
  else if(discharging)
  {
    display.foreground = RED ;
    display_string(" DISCHARGE ") ;
  }
  else
  {
    display_string(" IDLE      ") ;
  }

  /* Set the font colour back to grey */
  display.foreground = GREY ;
}

void display_values()
{
  /* Set the position on the display */
  display.x = 10; 
  display.y = 10;

  /* Display all values and states on the screen */
  display_double(10 , (LCDWIDTH/2) + 10 , "Busbar voltage = " , (busbar_voltage/VREF)*200 , " V ") ;
  display_double(10 , display.y , "Busbar current = " , (busbar_current/VREF)*5 , " A ") ;
  display_double(10 , display.y , "Wind capacity = " , (wind_capacity/VREF)*5 , " A ") ;
  display_double(10 , display.y , "Solar capacity = " , (solar_capacity/VREF)*5 , " A ") ;
  display_double(10 , display.y , "Total Renewable = " , ((wind_capacity + solar_capacity)/VREF) *5 , " A ") ;
  display_loads((LCDHEIGHT/2) + 15 , (LCDWIDTH/2) + 10 , "Load 1 = " , load_call[0] , load_switch[0]) ;
  display_loads((LCDHEIGHT/2) + 15 , display.y , "Load 2 = " , load_call[1] , load_switch[1]) ;
  display_loads((LCDHEIGHT/2) + 15 , display.y , "Load 3 = " , load_call[2] , load_switch[2]) ;
  display_batt((LCDHEIGHT/2) + 15 , display.y , "Batt Status : " , charge_battery , discharge_battery) ;
}

void display_pixel_shift(rectangle *shape) 
{
  (*shape).top =  display.y ;
  (*shape).left = display.x ;
  (*shape).bottom = (*shape).top + 10 ;
  (*shape).right = (*shape).left + 10 ;
  fill_rectangle(*shape , teamcolour) ;
}

void display_team_name()//Need to redo this so that it looks nicer
{
  rectangle shape ;
  int i = 0 ;
  int topmost = 10 ;
  int leftmost = 10 ;
  int width = 13 ;
  /* T */
  display.y = topmost ;
  display.x = leftmost ;
  display_pixel_shift(&shape) ;
  for(i = 0 ; i < 2 ; i++)
  {
    display.x += width ;
    display_pixel_shift(&shape) ;
  }
  display.x -= width ;
  for(i = 1 ; i < 5 ; i ++ )
  {
    display.y += width ;
    display_pixel_shift(&shape) ;
  }
  /* E */
  display.y = topmost - width ;
  display.x += 3*width ;
  for(i = 0 ; i < 5 ; i++)
  {
    display.y += width ;
    display_pixel_shift(&shape) ;
  }
  for(i = 0 ; i < 2 ; i ++)
  {
    display.x += width ;
    display_pixel_shift(&shape) ;
  }
  display.y -= 2*width ;
  display.x -= width ;
  display_pixel_shift(&shape) ;
  display.y -= 2*width ;
  display_pixel_shift(&shape) ;
  for(i = 0 ; i < 1 ; i++ )
  {
    display.x += width ;
    display_pixel_shift(&shape) ;
  }
  /* A */
  display.y  = topmost ;
  display.x += 2*width ;
  for(i = 0 ; i < 4 ; i++ )
  {
    display.y += width ;
    display_pixel_shift(&shape) ;
  }
  display.y = topmost ;
  for(i = 0 ; i < 2 ; i++)
  {
    display.x += width ;
    display_pixel_shift(&shape) ;
  }
  display.y += 2*width ;
  display.x += width ;
  for(i = 0 ; i < 2  ; i++ )
  {
    display.x -= width ;
    display_pixel_shift(&shape) ;
  }
  display.y = topmost ;
  display.x += 2*width ;
  for(i = 0 ; i < 4 ; i++ )
  {
    display.y += width ;
    display_pixel_shift(&shape) ;
  }
  /* M */
  display.y = topmost - width ;
  display.x += 2*width ;
  for( i = 0 ; i < 5 ; i++)
  {
    display.y += width ;
    display_pixel_shift(&shape) ;
  }
  display.y -= 3*width ;
  display.x += width ;
  display_pixel_shift(&shape) ;
  display.y += width ;
  display.x += width ;
  display_pixel_shift(&shape) ;
  display.y -= width ;
  display.x += width ;
  display_pixel_shift(&shape) ;
  display.y = topmost - width ; 
  display.x += width ;
  for( i = 0 ; i < 5 ; i++)
  {
    display.y += width ;
    display_pixel_shift(&shape) ;
  }
  /* B */
  display.y = topmost - width ;
  display.x += 2*width ;
  for(i = 0 ; i < 5 ; i++)
  {
    display.y += width ;
    display_pixel_shift(&shape) ;
  }
  for(i = 0 ; i < 2 ; i++)
  {
    display.x += width ;
    display_pixel_shift(&shape) ;
  }
  display.y -= 2*width ;
  display.x += width ;
  for(i = 0 ; i < 2 ; i++)
  {
    display.x -= width ;
    display_pixel_shift(&shape) ;
  }
  display.y -= 2*width ;
  display.x -= width ;
  for(i = 0 ; i < 2 ; i++)
  {
    display.x += width ;
    display_pixel_shift(&shape) ;
  }
  display.x += width ;
  display.y -= width ;
  for(i = 0 ; i < 2 ; i++)
  {
    display.y += 2*width ;
    display_pixel_shift(&shape) ;
  }
}

void update_lines(rectangle *bar, double value , int y_position)
{
  /* Update the lines under the values */

  /* Default */
  int colour = WHITE ;
  double temp = 0 ;
  display.y = y_position ;
  /* Delete previous line for a new one */
  fill_rectangle(*bar,BLACK) ;
  temp = (value/VREF) * (LCDHEIGHT/2 - 22) ; 
  (*bar).right = 12 + temp ;
  (*bar).top = y_position ;
  (*bar).bottom = y_position + 1 ;

  /* If line is too short, colour is red. Green otherwise. */
  if(temp < LCDHEIGHT/6)
  colour = RED ;
  else
  colour = GREEN ; 

  display.y += 20 ;
  /* Update a new line */
  fill_rectangle(*bar,colour) ;
}

rectangle shape_make(int y, double value)
{
  /* Lines that correspond to amount of value we have */
  rectangle shape ;
  shape.left = 10 ;
  shape.right = 12 + ((value) * (LCDHEIGHT/2 - 22)) ;
  shape.top = y ;
  shape.bottom = y+1 ;
  display.y += 20 ;
  return shape ;
}

void display_line()
{
  /* Create lines that seperate parts on the display */
  /* Initialize the shape*/
  rectangle line ;
  /* Drawing the boxes at the bottom */
  line.top = LCDWIDTH/2 ;
  line.bottom = line.top + 1 ;
  line.left = 5 ;
  line.right = LCDHEIGHT - 5 ;
  fill_rectangle(line,0xA514) ;
  line.top = LCDWIDTH - 7 ;
  line.bottom = line.top + 2 ;
  line.left = 5 ;
  line.right = LCDHEIGHT - 5 ;
  fill_rectangle(line,0xA514) ;
  line.top = LCDWIDTH/2 ;
  line.bottom = LCDWIDTH - 5 ;
  line.left = LCDHEIGHT/2 + 8 ;
  line.right = line.left + 2 ;
  fill_rectangle(line , 0xA514) ; 
  line.top = LCDWIDTH/2 ;
  line.bottom = LCDWIDTH - 5 ;
  line.left = 5 ;
  line.right = line.left + 2 ;
  fill_rectangle(line , 0xA514) ; 
  line.top = LCDWIDTH/2 ;
  line.bottom = LCDWIDTH - 5 ;
  line.left = LCDHEIGHT - 7 ;
  line.right = line.left + 2 ;
  fill_rectangle(line , 0xA514) ; 
}

void delay_100ms()
{
  /* wait until counter is counts 100ms. i and counter must be
   the same type so that it still works when counter is overflowing */
  uint32_t i = counter + 100 ;
  while(i>counter) ;
}

void delay_6000ms()
{
  /* Boot up loading time */
  uint32_t i = counter + 6000 ;
  while(i>counter) ;
}

int main(void)
{
  /* Start the counter */
  timer_init() ;

  /* Enable interrupts */
  sei() ;

  /* Give time for PSU to supply the il matto */
  delay_6000ms() ;

  /* TFT display bootup */
  init_lcd();

  /* Setup UART serial comms */
  init_debug_uart0() ;

  /* Setup display orientation */
  orientation o = West ;
  set_orientation(o);

  /* Setup ADC */
  adc_init() ;

  /* Setup digital Inouts. */
  digital_init() ;

  /* Display the margin */
  display_line() ;

  /* Create the line */
  rectangle busbar_voltage_bar = shape_make((LCDWIDTH/2) + 20, busbar_voltage) ;
  rectangle busbar_current_bar = shape_make(display.y, busbar_current) ;
  rectangle wind_bar = shape_make(display.y , wind_capacity) ;
  rectangle solar_bar = shape_make(display.y , solar_capacity) ;
  rectangle total_renewable_bar = shape_make(display.y , ((wind_capacity + solar_capacity)/2)) ;
  /* Default background colour */
  display.foreground = GREY ;

  /* Super Loop */
  for(;;) 
  {
    /* Read all inputs signals */
    read_inputs() ; 

    /* Compute the algorithms */
    algorithm(load_call[0], load_call[1] , load_call[2]) ; 

    /* Send all output signals */
    send_outputs() ; 

    /* Display what on screen */
    display_values() ; 

    /* Display team name */
    display_team_name() ;

    /* Update the screen every 100 ms */
    //delay_100ms() ;

    /* Updates graphic */
    update_lines(&busbar_voltage_bar , busbar_voltage*2 , (LCDWIDTH/2) + 20 ) ;
    update_lines(&busbar_current_bar , busbar_current*2 , display.y ) ;
    update_lines(&wind_bar , wind_capacity , display.y ) ;
    update_lines(&solar_bar , solar_capacity , display.y) ;
    update_lines(&total_renewable_bar , ((wind_capacity + solar_capacity)/2) , display.y) ;
  }
  //test
  return 0;
}