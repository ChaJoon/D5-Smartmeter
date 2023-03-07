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
              tests
27/02/2023  : The final scenario is out and I need to rewrite the algorithm.
              Tested the PSU and water test today.
              Changing the name of the variables for labview now.
28/02/2023  : Turns out I didnt change it because Jimmy wants to do it so I leave
              it and work on the algorithm instead.
              Changed while loop to be 'i-counter' instead of 'i>counter' 
03/03/2023  : Fixing PWM using oscilloscope, still not fixed, tried adding prescaler.
              Checked output of digitals. Fixed that discarge and charge pin is the same.
              Fixed pwm, turns out the problem is I didnt initialize the pwm_init() fx
              in the first place. Prescaler is removed since they as high frequency as 
              they can get.
*/

#include "../lcdlib/lcd.h"
#include "../lcdlib/ili934x.h"  
#include "../lcdlib/avrlcd.h"
#include <avr/io.h>
#include <avr/interrupt.h>
//#include <util/delay.h>
#include <stdio.h>
#include <stdint.h>
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
#define CHARGE_BATTERY PD2 
#define DISCHARGE_BATTERY PD3 
#define LOAD1_SWITCH PD4 
#define LOAD2_SWITCH PD5 
#define LOAD3_SWITCH PD6 

/* Double for Mains equired */
double mains_req = 0;

/* Doubles for input values */
double busbar_voltage = 0;
double busbar_current = 0;
double wind_capacity = 0;
double solar_capacity = 0;

/* boolean array for load calls and switches */
// Changed the array to individual variables to match emulator
//bool load_call[3] = {1, 1, 1};
bool LoadCall1 = 0;
bool LoadCall2 = 0;
bool LoadCall3 = 0;
//bool load_switch[3] = {1, 0, 0};
bool LoadSw1 = 0;
bool LoadSw2 = 0;
bool LoadSw3 = 0;

/* boolean for charging and discharging battery */
bool charge_battery = 0;
bool discharge_battery = 0;

/* Total Energy consumption */
double power_consumption = 0 ;

/* Counter can last up until 106751991 days before it overflows */
volatile uint64_t counter ; 
/* Battery counter */
volatile uint32_t battery_counter = 1000*10 ;
/* Team colour counter */
volatile uint16_t teamcolour = 0 ;

/* Battery level */
int battery_lvl = 50 ;

/*** Functions Initialisations ***/
/* Interupt service routine */
ISR(TIMER1_COMPA_vect) ;

/* Initializations */
void timer_init() ;
void adc_init() ;
void digital_init() ;
void pwm_init() ;
rectangle shape_make(int y, double value) ;

/* Measure inputs */
double input_adc_read(uint8_t channel) ;
double input_calculate_rms(uint8_t channel) ;
void input_digital() ;

/* Calculate outputs */
void output_pwm() ;
void output_digital() ;

/* Read and send inputs and outputs */
void read_inputs() ;
void send_outputs() ;

/* Logics */
double check_load_demand(bool call , double current) ;
void algorithm(bool load1_call, bool load2_call, bool load3_call) ;

/* Display graphics */
int display_bool_check(bool load) ;
void display_line() ;
void display_double(int x_position, int y_position, char* name, double value, char* units , int width , int decimal_places) ;
void display_loads(int x_position , int y_position , char* name , bool load_call , bool load_switch ) ;
void display_batt(int x_position , int y_position , char* name , bool charging , bool discharging ) ;
void display_values() ;
void display_pixel_shift(rectangle *shape , int width)  ;
void display_team_name() ;

/* Updates */
void update_lines(rectangle *bar, double value , int y_position) ;
void update_battery_lvl() ;

/* Delays */
void delay_500ms() ;

/********************************************************** Main function *****************************************************************/
int main(void)
{
  /* TFT display bootup */
  init_lcd();

  /* Setup UART serial comms */
  init_debug_uart0() ;

  /* Setup display orientation */
  orientation o = East ;
  set_orientation(o);

  /* Refresh the screen */
  clear_screen() ;

  /* Setup ADC */
  adc_init() ;

  /* Setup digital Inouts. */
  digital_init() ;

  /* Start the counter */
  timer_init() ;

  /* Setup the PWM */
  pwm_init() ;

  /* Enable interrupts */
  sei() ;

  /* Display the margin */
  display_line() ;

  /* Create the lines */
  rectangle busbar_voltage_bar = shape_make((LCDWIDTH/2) , busbar_voltage) ;
  rectangle busbar_current_bar = shape_make(display.y, busbar_current) ;
  rectangle wind_bar = shape_make(display.y , wind_capacity) ;
  rectangle solar_bar = shape_make(display.y , solar_capacity) ;
  rectangle total_renewable_bar = shape_make(display.y , ((wind_capacity + solar_capacity)/2)) ;
  rectangle mains_req_bar = shape_make(display.y , mains_req) ;

  /* Default background colour */
  display.foreground = WHITE ;
  display.background = BLACK ;

  /* Super Loop */
  for(;;) 
  {
    /* Read all inputs signals */
    read_inputs() ; 

    /* Compute the algorithms */
    algorithm(LoadCall1, LoadCall2 , LoadCall3) ; 

    /* Send all output signals */
    send_outputs() ; 

    /* Display what on screen */
    display_values() ; 

    /* Display team name */
    display_team_name() ;

    /* Update the screen every 100 ms */
    //delay_500ms() ;

    /* Update battery level */
    update_battery_lvl() ;

    /* Updates the line graphics under every data */
    update_lines(&busbar_voltage_bar , busbar_voltage*(sqrt(2)) , (LCDWIDTH/2) ) ;
    update_lines(&busbar_current_bar , busbar_current*(sqrt(2)) , display.y ) ;
    update_lines(&wind_bar , wind_capacity , display.y ) ;
    update_lines(&solar_bar , solar_capacity , display.y) ;
    update_lines(&total_renewable_bar , ((wind_capacity + solar_capacity)/2) , display.y) ;
    update_lines(&mains_req_bar, (mains_req/2)*VREF , display.y) ;
  }

  return 0;
}

/******************************************************************************************************************************************/

/* Functions definitions */

ISR(TIMER1_COMPA_vect)
{
    /* Counter incremented every 1 millisec */
    counter++;

    /* Counter for battery level */
    if((charge_battery) && (battery_counter < 100000))
    battery_counter++ ;
    if((discharge_battery) && (battery_counter > 0))
    battery_counter-- ;
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
  TCCR2A = _BV(COM2A1) | _BV(WGM20) | _BV(WGM21) ;
  /* Set to no prescaler. */
  TCCR2B = _BV(CS20) ;
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
    maxval = (maxval / sqrt(2) ); // find the rms (due to the wave not being fully rectified, /2 intead of /sqrt(2) )
    return maxval ;

}

void input_digital()
{
  /* Checks load1 call */
  int inp = PINA&_BV(LOAD1_CALL) ;
  /* Set the array */
  if(inp)
    LoadCall1 = 1 ;
  else
    LoadCall1 = 0 ;

  /* Checks load2 call */
  inp = PINA&_BV(LOAD2_CALL) ;
  /* Set the array */
  if(inp)
    LoadCall2 = 1 ;
  else
    LoadCall2 = 0 ;

  /* Checks load3 call */
  inp = PINA&_BV(LOAD3_CALL) ;
  /* Set the array */
  if(inp)
    LoadCall3 = 1 ;
  else
    LoadCall3 = 0 ;
}

void output_pwm()
{
  /* set the duty cycle of the PWM */
  OCR2A = (mains_req/VREF) * 255 ; //from handbook
  //OCR2A = 255 ;
  //printf("test ") ;
  //printf(" %d\n " , OCR2A ) ;
}

void output_digital()
{
  /* Set the load switch 1 high */
  if(LoadSw1==1)
    PORTD |= _BV(LOAD1_SWITCH) ;
  else
    PORTD &= ~(_BV(LOAD1_SWITCH)) ;
    
  /* Set the load switch 2 high */
  if(LoadSw2==1)
    PORTD |= _BV(LOAD2_SWITCH) ;
  else
    PORTD &= ~(_BV(LOAD2_SWITCH)) ;
    
  /* Set the load switch 3 high */
  if(LoadSw3==1)
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
  if(call==1)
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
  bool check_load_1 ;
  bool check_load_2 ;
  bool check_load_3 ;

  /* Maximum mains current */
  double max_mains_current = 2.0 ;
  
  /* Calculate total load current requirements */
  double load1_current = check_load_demand(load1_call, 1.2) ;
  double load2_current = check_load_demand(load2_call, 2.0) ;
  double load3_current = check_load_demand(load3_call, 0.8) ;
  double total_load_current = load1_current + load2_current + load3_current ;

  /*for(i = 0 ; i < 3 ; i++ )
  {
    if ( load_call[i] )  
      load_switch[i] = 1 ;
    else
      load_switch[i] = 0 ;
  }*/

  /* calculate total energy available */
  double total_energy = wind_capacity + solar_capacity ;

  /* Check if we have enough energy */
  if(total_energy >= total_load_current)
  {
    /* Find if we have extra current */
    double excess_current = total_energy - total_load_current ;

    if(excess_current >= 1)
    {
      /* If more than 1A we can charge the batt */
      charge_battery = 1 ;
      discharge_battery = 0 ;
      check_load_1 = 1 ;
      check_load_2 = 1 ;
      check_load_3 = 1 ;
    }
    else if ((excess_current < 1) && (excess_current > 0))
    {
      /* Request extra current from main to charge battery */
      mains_req = 1.0 - excess_current ;
      charge_battery = 1 ;
      discharge_battery = 0 ;
      check_load_1 = 1 ;
      check_load_2 = 1 ;
      check_load_3 = 1 ;
    }
    else
    {
      /* Default state in case the loop fails. */
      charge_battery = 0 ;
      discharge_battery = 0 ;
      check_load_1 = 1 ;
      check_load_2 = 1 ;
      check_load_3 = 1 ;
    }
  }

  else 
  {
    /* If renewable energy is less than load demands */
    double lack_current = total_load_current - total_energy ;
    if(lack_current<=(max_mains_current-1))
    {
      /* Request from main and leave batt on idle*/
      mains_req = lack_current + 1 ;
      charge_battery = 1 ;
      discharge_battery = 0 ;
      check_load_1 = 1 ;
      check_load_2 = 1 ;
      check_load_3 = 1 ;
    }
    else if (lack_current <= (max_mains_current) )
    {
      /* If lack of current is too high we discharge batt as well as requesting from main */
      mains_req = lack_current  ;
      charge_battery = 0 ;
      discharge_battery = 0 ;
      check_load_1 = 1 ;
      check_load_2 = 1 ;
      check_load_3 = 1 ;
    }
    /* If support from main is not enough. We need to discharge battery  */
    else if (lack_current <= (max_mains_current+1) )
    {
      mains_req = lack_current - 1 ;
      charge_battery = 0 ;
      discharge_battery = 1 ;
      check_load_1 = 1 ;
      check_load_2 = 1 ;
      check_load_3 = 1 ;
    }
    /* If renewable is too low and exceeds max main request and battery 
       Load1,Load2 and Load3 calls */
    else if (load3_current)
    {
      /* If we have at least 0.2 renewables we can cancel load3 for highest survival rate. */
      if (total_energy>=0.2)
      {
        mains_req = max_mains_current ;
        discharge_battery = 1;
        charge_battery = 0 ;
        check_load_1 = 1 ;
        check_load_2 = 1 ;
        check_load_3 = 0 ;
        total_load_current -= 0.8 ;
      }
      /* If not then cancel load2 */
      else 
      {
        mains_req = 2.0 - total_energy ;
        discharge_battery = 1 ;
        charge_battery = 0 ;
        check_load_1 = 1 ;
        check_load_2 = 0 ;
        check_load_3 = 1 ;
        total_load_current -= 2.0 ;
      }
    }
    /* Load1 and Load2 calls, cancels load2 calls */
    else
    {
      mains_req = load2_current - total_energy ;
      charge_battery = 0 ;
      discharge_battery = 0 ;
      check_load_1 = 1 ;
      check_load_2 = 0 ;
      check_load_3 = 1 ;
      total_load_current -= 2.0 ;
    }

  }

  /* Set Load Switch 1 */
  if((check_load_1 == 1) && (LoadCall1 ==  1) )
    LoadSw1 = 1 ;
  else 
    LoadSw1 = 0 ;
  /* Set Load Switch 2 */
  if((check_load_2 == 1) && (LoadCall2 == 1) )
    LoadSw2 = 1 ;
  else
    LoadSw2 = 0 ;
  /* Set Load Switch 3 */
  if( (check_load_3 == 1) && (LoadCall3 == 1) )
    LoadSw3 = 1 ;
  else 
    LoadSw3 = 0 ;

    /* Calculate power consumption in kW/h */
    power_consumption = (busbar_voltage*(400/VREF)*total_load_current)/1000 ;
}

/* Display double type on screen */
void display_double(int x_position, int y_position, char* name, double value, char* units , int width , int decimal_places)
{
  /* Set font colour as grey */
  display.foreground = WHITE ;

  /* Set the position on the screen */
  display.x = x_position ; 
  display.y = y_position ; 
  
  /* Temporary value for converting double to string */
  char temp_str[10] ;
  //int temp ;
  
  /* Converting double to string */
  dtostrf( value , width , decimal_places, temp_str );
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
    if 0 we will print the text in red. */
  if(load==1)
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
  display.foreground = WHITE ;
  display.y += 20 ;
}

void display_batt(int x_position , int y_position , char* name , bool charging , bool discharging )
{
  /* Set the position on the display */
  display.x = x_position ;
  display.y = y_position ;

  /* start print on the display */
  display_string(name) ;
  display_double(display.x , display.y , "" , battery_lvl , "% " , 2 , 0) ;//Display battery level

  /* New line */
  display.x = x_position ;
  display.y -= 10 ;

  /* Display current state of the battery */
  if(charging==1)
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
  display.foreground = WHITE ;
}

void display_values()
{
  /* Set the default position on the display */
  display.x = 10; 
  display.y = 10;

  /* Display all values and states on the screen */
  display_double(LCDHEIGHT/2 + 45 , (LCDWIDTH/2) - 30 , "Elapsed = " , counter/60000 , "m " , 2 , 0) ;//Display minutes
  display_double(display.x , (LCDWIDTH/2) - 30 , "" , (counter/1000)%60 , "s " , 2, 0) ;//Display seconds
  display_double(10 , (LCDWIDTH/2) - 10 , "Busbar Voltage = " , (busbar_voltage)*(400/VREF) , " V " , 4, 2 ) ;//Display actual value of busbarV
  display_double(10 , display.y , "Busbar Current = " , (busbar_current)*(10/VREF) , " A " , 4 , 2) ;//Display actual value of busbarI
  display_double(10 , display.y , "Wind Capacity = " , (wind_capacity/VREF)*5 , " A " , 4 , 2) ;//Display actual value of windI
  display_double(10 , display.y , "Solar Capacity = " , (solar_capacity/VREF)*5 , " A " , 4 , 2) ;//Display actual value of SolarI
  display_double(10 , display.y , "Total Renewable = " , ((wind_capacity + solar_capacity)/VREF) *5 , " A " , 4 , 2) ;//Display Total renewable energy
  display_double(10 , display.y , "Main Request = " , mains_req , " A " , 4 , 2) ;//Display Main reque st
  display_loads((LCDHEIGHT/2) + 15 , (LCDWIDTH/2) - 10 , "Load 1 = " , LoadCall1 , LoadSw1) ;//Display load states
  display_loads((LCDHEIGHT/2) + 15 , display.y , "Load 2 = " , LoadCall2 , LoadSw2) ;
  display_loads((LCDHEIGHT/2) + 15 , display.y , "Load 3 = " , LoadCall3 , LoadSw3) ;
  display_batt((LCDHEIGHT/2) + 15 , display.y , "Batt Status : " , charge_battery , discharge_battery) ;// Display battery status
  display_double((LCDHEIGHT/2) + 15 , display.y += 20, "Power = " , power_consumption , "kW/h " , 3 , 2) ;//Display power consumption
}

void display_pixel_shift(rectangle *shape , int width) 
{
  (*shape).top =  display.y ;
  (*shape).left = display.x ;
  (*shape).bottom = (*shape).top + width ;
  (*shape).right = (*shape).left + width ;
  fill_rectangle(*shape , teamcolour) ;
}

void display_team_name()
{
  teamcolour++ ;
  rectangle shape ;
  int i = 0 ;
  int topmost = 10 ;
  int leftmost = 10 ;
  int width = 10 ;
  /* T */
  display.y = topmost ;
  display.x = leftmost ;
  display_pixel_shift(&shape , width) ;
  for(i = 0 ; i < 2 ; i++)
  {
    display.x += width ;
    display_pixel_shift(&shape , width) ;
  }
  display.x -= width ;
  for(i = 1 ; i < 5 ; i ++ )
  {
    display.y += width ;
    display_pixel_shift(&shape , width) ;
  }
  /* E */
  display.y = topmost - width ;
  display.x += 3*width ;
  for(i = 0 ; i < 5 ; i++)
  {
    display.y += width ;
    display_pixel_shift(&shape , width) ;
  }
  for(i = 0 ; i < 2 ; i ++)
  {
    display.x += width ;
    display_pixel_shift(&shape , width) ;
  }
  display.y -= 2*width ;
  display.x -= width ;
  display_pixel_shift(&shape , width) ;
  display.y -= 2*width ;
  display_pixel_shift(&shape , width) ;
  for(i = 0 ; i < 1 ; i++ )
  {
    display.x += width ;
    display_pixel_shift(&shape , width) ;
  }
  /* A */
  display.y  = topmost ;
  display.x += 2*width ;
  for(i = 0 ; i < 4 ; i++ )
  {
    display.y += width ;
    display_pixel_shift(&shape , width) ;
  }
  display.y = topmost ;
  for(i = 0 ; i < 2 ; i++)
  {
    display.x += width ;
    display_pixel_shift(&shape , width) ;
  }
  display.y += 2*width ;
  display.x += width ;
  for(i = 0 ; i < 2  ; i++ )
  {
    display.x -= width ;
    display_pixel_shift(&shape , width) ;
  }
  display.y = topmost ;
  display.x += 2*width ;
  for(i = 0 ; i < 4 ; i++ )
  {
    display.y += width ;
    display_pixel_shift(&shape , width) ;
  }
  /* M */
  display.y = topmost - width ;
  display.x += 2*width ;
  for( i = 0 ; i < 5 ; i++)
  {
    display.y += width ;
    display_pixel_shift(&shape , width) ;
  }
  display.y -= 3*width ;
  display.x += width ;
  display_pixel_shift(&shape , width) ;
  display.y += width ;
  display.x += width ;
  display_pixel_shift(&shape , width) ;
  display.y -= width ;
  display.x += width ;
  display_pixel_shift(&shape , width) ;
  display.y = topmost - width ; 
  display.x += width ;
  for( i = 0 ; i < 5 ; i++)
  {
    display.y += width ;
    display_pixel_shift(&shape , width) ;
  }
  /* B */
  display.y = topmost - width ;
  display.x += 2*width ;
  for(i = 0 ; i < 5 ; i++)
  {
    display.y += width ;
    display_pixel_shift(&shape , width) ;
  }
  for(i = 0 ; i < 2 ; i++)
  {
    display.x += width ;
    display_pixel_shift(&shape , width) ;
  }
  display.y -= 2*width ;
  display.x += width ;
  for(i = 0 ; i < 2 ; i++)
  {
    display.x -= width ;
    display_pixel_shift(&shape , width) ;
  }
  display.y -= 2*width ;
  display.x -= width ;
  for(i = 0 ; i < 2 ; i++)
  {
    display.x += width ;
    display_pixel_shift(&shape , width) ;
  }
  display.x += width ;
  display.y -= width ;
  for(i = 0 ; i < 2 ; i++)
  {
    display.y += 2*width ;
    display_pixel_shift(&shape , width) ;
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

void update_battery_lvl()
{
  /* Battery need to charge for at least 5 second before increasing the level \
    %101 so that it will not equal to 0 when at 100%*/
  battery_lvl = (battery_counter/5000)%101 ;
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
  //0xA514 is colour hex for grey.
  line.top = LCDWIDTH/2 - 20 ;
  line.bottom = line.top + 1 ;
  line.left = 5 ;
  line.right = LCDHEIGHT - 5 ;
  fill_rectangle(line,0xA514) ;
  line.top = LCDWIDTH - 7 ;
  line.bottom = line.top + 2 ;
  line.left = 5 ;
  line.right = LCDHEIGHT - 5 ;
  fill_rectangle(line,0xA514) ;
  line.top = LCDWIDTH/2 - 20 ;
  line.bottom = LCDWIDTH - 5 ;
  line.left = LCDHEIGHT/2 + 8 ;
  line.right = line.left + 2 ;
  fill_rectangle(line , 0xA514) ; 
  line.top = LCDWIDTH/2 -20 ;
  line.bottom = LCDWIDTH - 5 ;
  line.left = 5 ;
  line.right = line.left + 2 ;
  fill_rectangle(line , 0xA514) ; 
  line.top = LCDWIDTH/2 - 20 ;
  line.bottom = LCDWIDTH - 5 ;
  line.left = LCDHEIGHT - 7 ;
  line.right = line.left + 2 ;
  fill_rectangle(line , 0xA514) ; 
}

void delay_500ms()
{
  /* wait until counter is counts 100ms. i and counter must be
   the same type so that it still works when counter is overflowing */
  uint32_t i = counter + 500 ;
  /*Only exit the while loop if counter and integer is the same number */
  while(i-counter) ;
}
