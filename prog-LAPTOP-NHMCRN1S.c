/*
Smart Meter Software Control
Author : Syazwan
*/

//avr-gcc -mmcu=atmega644p -L../lcdlib -o prog.elf prog.c -llcd
//avr-objcopy -O ihex prog.elf prog.hex
//avrdude -c usbasp -p m644p -U flash:w:prog.hex

/*
Logs :
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

//Analogue input pins
#define WIND PA0
#define SOLAR PA1
#define BUSBAR_VOLTAGE PA2
#define BUSBAR_CURRENT PA3

//Analogue output pins (PWM)
#define MAIN_CALL PD7 

//Digital Input pins
#define LOAD1_CALL PA4 
#define LOAD2_CALL PA5 
#define LOAD3_CALL PA6 

//Digital output pins
#define CHARGE_BATTERY PD3 
#define DISCHARGE_BATTERY PD3 
#define LOAD1_SWITCH PD4 
#define LOAD2_SWITCH PD5 
#define LOAD3_SWITCH PD6 

//Double for Mains Required
double mains_req = 0;

//Doubles for input values
double main_capacity = 0;
double busbar_voltage = 0;
double busbar_current = 0;
double wind_capacity = 0;
double solar_capacity = 0;

//boolean array for load calls and switches
bool load_call[3] = {true, true, true};
bool load_switch[3] = {true, false, false};
//boolean for charging and discharging battery
bool charge_battery = false;
bool discharge_battery = false;

volatile uint32_t counter ; 

//Function Definintions :

ISR(TIMER1_COMPA_vect)
{
    //Counter incremented every 1 millisec
    counter++;
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
  //set inputs :
  DDRA &= ~(_BV(LOAD1_CALL) | _BV(LOAD2_CALL) | _BV(LOAD3_CALL));
  
  PORTA &= ~(_BV(LOAD1_CALL) | _BV(LOAD2_CALL) | _BV(LOAD3_CALL));
  
  //set outputs:
  DDRD |= _BV(LOAD1_SWITCH) | _BV(LOAD2_SWITCH) | _BV(LOAD3_SWITCH);
  
  PORTD &= ~(_BV(LOAD1_SWITCH) | _BV(LOAD2_SWITCH) | _BV(LOAD3_SWITCH));
}

void pwm_init()
{
  // Set Pin D7 as an output
  DDRD |= _BV(MAIN_CALL);

  // Set up timer 2 in Fast PWM mode
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
   
  return (double) adcreading ;//return the result of the conversion as a double
}

double input_calculate_rms(uint8_t channel)
{
  /*To find rms, need to find the max voltage at
  1 period. 50 Hz = 20 ms*/
  uint32_t periodEnd = counter + 20 ;
  double val = 0 ;
  double maxval = 0;
  // in a period of 20ms, find the max amplitude
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
  //Checks load1 call
  int inp = PINA&_BV(LOAD1_CALL) ;
  //Set the array
  if(inp)
    load_call[0] = true ;
  else
    load_call[0] = false ;

  //Checks load2 call
  inp = PINA&_BV(LOAD2_CALL) ;
  //Set the array
  if(inp)
    load_call[1] = true ;
  else
    load_call[1] = false ;

  //Checks load3 call
  inp = PINA&_BV(LOAD3_CALL) ;
  //Set the array
  if(inp)
    load_call[2] = true ;
  else
    load_call[2] = false ;
}

void output_pwm()
{
  OCR2A = (mains_req/10) * 255 ; //from handbook
}

void output_digital()
{
  //Set the load switch 1 high
  if(load_switch[0]==true)
    PORTD |= _BV(LOAD1_SWITCH) ;
  else
    PORTD &= ~(_BV(LOAD1_SWITCH)) ;
    
  //Set the load switch 2 high
  if(load_switch[1]==true)
    PORTD |= _BV(LOAD2_SWITCH) ;
  else
    PORTD &= ~(_BV(LOAD2_SWITCH)) ;
    
  //Set the load switch 3 high
  if(load_switch[2]==true)
    PORTD |= _BV(LOAD3_SWITCH) ;
  else
    PORTD &= ~(_BV(LOAD3_SWITCH)) ;

  //Set the charging battery switch
  if(charge_battery)
    PORTD |= _BV(CHARGE_BATTERY) ;
  else
    PORTD &= ~(_BV(CHARGE_BATTERY)) ;
  
  //Set the discharge battery switch
  if(discharge_battery)
    PORTD |= _BV(DISCHARGE_BATTERY) ;
  else
    PORTD &= ~(_BV(DISCHARGE_BATTERY)) ;
}

void read_inputs()
{
  //Read the analog values
  wind_capacity = input_adc_read(0) ;
  solar_capacity = input_adc_read(1) ;
  busbar_voltage = input_calculate_rms(2);
  busbar_current = input_calculate_rms(3);

  //Read the digital values
  input_digital() ;
}

void send_outputs()
{
  //Send the outputs
  output_pwm() ;
  output_digital() ;
}

double check_load_demand(bool call , double current)
{
  //if load is calling, we return the value of the
  //current required by the load
  if(call==true)
    return current ;
  else
    return 0 ;
}

void algorithm(bool load1_call, bool load2_call, bool load3_call)
{
  //Initialize variables
  mains_req = 0 ;
  charge_battery = 0 ;
  discharge_battery = 0 ;

  //Maximum mains current
  double max_mains_current = 3.0 ;
  
  //Calculate total load current requirements
  double load1_current = check_load_demand(load1_call, 0.5) ;
  double load2_current = check_load_demand(load2_call, 1.0) ;
  double load3_current = check_load_demand(load3_call, 3.0) ;
  double total_load_current = load1_current + load2_current + load3_current ;

  //calculate total energy available
  double total_energy = ((wind_capacity + solar_capacity)/2/VREF * 5) ;

  //Check if we have enough energy
  if(total_energy >= total_load_current)
  {
    double excess_current = total_energy - total_load_current ;

    if(excess_current >= 1)
    {
      charge_battery = true ;
      discharge_battery = false ;
    }
    else if ((excess_current < 1) && (excess_current > 0))
    {
      //Request extra current from main to charge battery
      mains_req = ((1.0 - excess_current)*(10/max_mains_current)) ;
      charge_battery = true ;
      discharge_battery = false ;
    }
    else
    {
      charge_battery = false ;
      discharge_battery = false ;
    }
  }
  else 
  {
    double lack_current = total_load_current - total_energy ;
    if((lack_current > 0 ) && (lack_current <= 3))
    {
      mains_req = lack_current * (10/max_mains_current) ;
      charge_battery = false ;
      discharge_battery = false ;
    }
    else if (lack_current > 3)
    {
      double current_needed = lack_current - 1 ;

      mains_req = current_needed * (10/max_mains_current) ;

      charge_battery = false ;
      discharge_battery = true ;
    }


  }

}

void display_double(int x_position, int y_position, char* name, double value, char* units)
{
  display.foreground = GREY ;
  display.x = x_position; //Set position on screen
  display.y = y_position;
  char temp_str[50] ;
  int temp ;
  dtostrf( value , 4, 2, temp );
  sprintf(temp_str,"%s",temp) ;
  display_string(name) ;
  display_string(temp_str) ;
  display_string(units) ;
  display.y += 20 ;
} 

int display_bool_check(bool load)
{
  if(load==true)
    return GREEN ;
  else
    return RED ;
}

void display_loads(int x_position , int y_position , char* name , bool load_call , bool load_switch )
{ 
  display.x = x_position ;
  display.y = y_position ;
  display_string(name) ;
  display.foreground = display_bool_check(load_call) ;
  display_string(" CALL ") ;
  display.foreground = display_bool_check(load_switch) ;
  display_string(" SWITCH ") ;
  display.foreground = GREY ;
}

void display_batt(int x_position , int y_position , char* name , bool charging , bool discharging )
{
  display.x = x_position ;
  display.y = y_position ;
  display_string(name) ;
  display.x = x_position ;
  display.y += 10 ;
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
  display.foreground = GREY ;
}

void display_values()
{
  display.x = 10; 
  display.y = 10;
  char str[50] ;
  int temp ;
  display_double(10 , display.y , "Busbar voltage = " , (busbar_voltage/VREF)*400 , " V ") ;
  display_double(10 , display.y , "Busbar current = " , (busbar_current/VREF)*10 , " A ") ;
  display_double(10 , display.y , "Wind capacity = " , (wind_capacity/VREF)*5 , " A ") ;
  display_double(10 , display.y , "Solar capacity = " , (solar_capacity/VREF)*5 , " A ") ;
  display_double(10 , display.y , "Total Renewable = " , ((wind_capacity + solar_capacity)/VREF) *5 , " A ") ;
  display_loads((LCDHEIGHT/2) + 15 , 10 , "Load 1 = " , load_call[0] , load_switch[0]) ;
  display_loads((LCDHEIGHT/2) + 15 , 30 , "Load 2 = " , load_call[1] , load_switch[1]) ;
  display_loads((LCDHEIGHT/2) + 15 , 50 , "Load 3 = " , load_call[2] , load_switch[2]) ;
  display_batt((LCDHEIGHT/2) + 15 , 70 , "Batt Status : " , charge_battery , discharge_battery) ;
}

void update_lines(rectangle *bar, double value)
{
  int colour = WHITE ;
  double temp = 0 ;
  fill_rectangle(*bar,BLACK) ;
  temp = (value/VREF) * (LCDHEIGHT/2 - 22) ; 
  (*bar).right = 12 + temp ;
  if(temp < LCDHEIGHT/6)
  colour = RED ;
  else
  colour = GREEN ; 
  fill_rectangle(*bar,colour) ;
  printf("%d\n" , (*bar).right) ;
}

rectangle shape_make(int y, double value)
{
  rectangle shape ;
  shape.left = 10 ;
  shape.right = 12 + ((value) * (LCDHEIGHT/2 - 22)) ;
  shape.top = y ;
  shape.bottom = y+1 ;
  return shape ;
}

void display_line()
{
    rectangle line ;
    line.top = LCDWIDTH/2 ;
    line.bottom = line.top + 1 ;
    line.left = 0 ;
    line.right = LCDHEIGHT ;
    fill_rectangle(line,0xA514) ; 
    line.top = 0 ;
    line.bottom = LCDWIDTH/2 ;
    line.left = LCDHEIGHT/2 + 8 ;
    line.right = line.left + 2 ;
    fill_rectangle(line , 0x2965) ; 
}

int main(void)
{
  //TFT display bootup
  init_lcd();

  //Setup UART serial comms
  init_debug_uart0() ;

  //Setup display orientation
  orientation o = East ;
  set_orientation(o);

  //Setup ADC
  adc_init() ;

  //Setup digital Inouts.
  digital_init() ;

  //Start the counter
  timer_init() ;

  //Enable interrupts
  sei() ;

  display_line() ;

  rectangle busbar_voltage_bar = shape_make(20, busbar_voltage) ;
  rectangle busbar_current_bar = shape_make(40, busbar_current) ;
  rectangle wind_bar = shape_make(60 , wind_capacity) ;
  rectangle solar_bar = shape_make(80 , solar_capacity) ;
  rectangle total_renewable_bar = shape_make(100 , ((wind_capacity + solar_capacity)/2)) ;
  display.foreground = GREY ;

  for(;;) // forever loop
  {
    read_inputs() ; //read all inputs signals

    algorithm(load_call[0], load_call[1] , load_call[2]) ; //compute the algorithm

    send_outputs() ; //send all outputs signals

    display_values() ; // Show readings in LCD display

    update_lines(&wind_bar , wind_capacity) ;
    update_lines(&solar_bar , solar_capacity) ;
    update_lines(&busbar_voltage_bar , busbar_voltage*2) ;
    update_lines(&busbar_current_bar , busbar_current*2) ;
    update_lines(&total_renewable_bar , ((wind_capacity + solar_capacity)/2)) ;
  }
  //test
  return 0;
}