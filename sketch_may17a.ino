/*
 * Author - Raymond M Booth
 * 
 * Description - This was my final CS122A arduno project that was an alarm that the user can
 *                and detect if their door was left open for 9 seconds or longer. After which
 *                the passive buzzer will play a melody to alert those near by that the door 
 *                should be closed
 */
#include <TimerOne.h>
#include "pitches.h"
#define button 8
//Ultrasonic sensor
#define trig_pin 4
#define echo_pin 3

#define passive_buz 7

int latchPin = 11; // connect to the ST_CP of 74HC595 (pin 3,latch pin)
int clockPin = 9;  // connect to the SH_CP of 74HC595 (pin 4, clock pin)
int dataPin = 12;  // connect to the DS of 74HC595 (pin 2)

bool wait_button;
bool countdown = false;
int digit;
int max_time = 10;
int closed_distance;//distance of door on closed state

int timer_flag = 0;
unsigned long int period = 1000;// 1 millisecond
//The next 3 varaible and code relating to the 7 segment display was learned and part gotten from https://youtu.be/gkjsA180xs4
byte sevenSegDigits[10] = { B01111011   ,  // = 0
                            B00001001   ,  // = 1
                            B10110011   ,  // = 2
                            B10011011   ,  // = 3
                            B11001001   ,  // = 4
                            B11011010   ,  // = 5
                            B11111000   ,  // = 6
                            B00001011   ,  // = 7
                            B11111011   ,  // = 8
                            B11001011      // = 9
                           };
byte sevenSegDP = B00000100;  // = DP

byte sevenSegAlpha[] = {  B11101011   , // = A
                          B11111000   , // = b
                          B01110010   , // = C
                          B10111001   , // = d
                          B11110010   , // = E
                          B11100010   , // = F
                          B11011011   , // = g
                          B11101000   , // = h
                          B01100000     // = I   
};

void TimerISR(void){
  timer_flag = 1;
}

//////////////////////////////////////////////////////////////////
// notes in the melody:
int melody[] = {
  NOTE_D3, NOTE_A3, NOTE_G3, NOTE_G3, NOTE_F3, NOTE_D3, NOTE_F3, NOTE_G3, NOTE_C3, NOTE_C3

};
/*
 *SM is for controlling the passive busser to play a cusome melody when the time reaches 0
 */
int melody_size = 10, cnt = 0;
unsigned long int buzz_period = period * 100;
unsigned long int BZ_elapsed_time = buzz_period;
enum BZ_states { BZ_SMStart, BZ_on, BZ_off } BZ_state;
void TickFct_BuzzSpeek() {
  switch(BZ_state){
    case BZ_SMStart:
      if(digit < 1)
        BZ_state = BZ_on;
      else
        BZ_state = BZ_SMStart;
    break;
    case BZ_on:
      if(digit < 1)
        BZ_state = BZ_off;
      else
        BZ_state = BZ_SMStart;
    break;
    case BZ_off:
      if(digit < 1)
        BZ_state = BZ_on;
      else
        BZ_state = BZ_SMStart;
    break;
  }
  switch(BZ_state){
    case BZ_SMStart:
      noTone(passive_buz);
      cnt = 0; 
    break;
    case BZ_on:
      if(cnt >= melody_size)
        cnt = 0;
      tone(passive_buz, melody[cnt]);
      cnt++;
    break;
    case BZ_off:
      noTone(passive_buz);
    break;
  }
}

/////////////////////////////////////////////////////////////////////
// this fuciton from https://youtu.be/gkjsA180xs4
// display a alpha, binary value, or number on the digital segment display
void sevenSegWrite(byte digit, bool bDP = false, char switchValue='D') {
    /*       digit = array pointer or binary value, as a byte 
     *         bDP = true-include decimal point, as boolean
     * switchValue = 'A' alpha
     *               'B' binary
     *               'D' digits <default>, as char           */
    
    // set the latchPin to low potential, before sending data
    digitalWrite(latchPin, LOW);
     
    // the data (bit pattern)
    if (switchValue=='A'){
        // alpha
        shiftOut(dataPin, clockPin, MSBFIRST, sevenSegAlpha[digit]+(sevenSegDP*bDP)); 
    } else if (switchValue=='B'){
        // binary
        shiftOut(dataPin, clockPin, MSBFIRST, digit+(sevenSegDP*bDP));
    } else {
        // digits
        shiftOut(dataPin, clockPin, MSBFIRST, sevenSegDigits[digit]+(sevenSegDP*bDP));   
    }
 
    // set the latchPin to high potential, after sending data
    digitalWrite(latchPin, HIGH);
}
/////////////////////////////////////////////////////////////////////
//this function from https://youtu.be/gkjsA180xs4
void sevenSegBlank(){
    // set the latchPin to low potential, before sending data
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, B00000000);  
    // set the latchPin to high potential, after sending data
    digitalWrite(latchPin, HIGH);
}
/////////////////////////////////////////////////////////////////////
/*
 * SM for taking the input of the button to start the whole system and
 * toggle from pause to start
 */
unsigned long int PR_period = period * 100;
unsigned long int PR_elapsed_time = PR_period;
bool on = false;
bool change_sytem_state = false;
bool button_state;
enum PR_states { PR_SMStart, PR_on, PR_update, PR_hold, PR_release} PR_state;
void TickFct_button_press() {
  switch(PR_state){
    case PR_SMStart:
      PR_state = PR_on;
      button_state = digitalRead(button);
    break;
    case PR_on:
      button_state = digitalRead(button);
      if(button_state == 0)
        PR_state = PR_update;
      else
        PR_state = PR_on;
    break;
    case PR_update:
        PR_state = PR_hold;
    break;
    case PR_hold:
      button_state = digitalRead(button);
      if(button_state == 1)
        PR_state = PR_release;
      else
        PR_state = PR_hold;
    break;
    case PR_release:
      button_state = digitalRead(button);
      if(button_state == 0)
        PR_state = PR_update;
      else
        PR_state = PR_release;
    break;
  }
  switch(PR_state){
    case PR_update:
      change_sytem_state = !change_sytem_state;
      on = change_sytem_state;
    break;
  }
}
/////////////////////////////////////////////////////////////////////
/*
 * SM for controlling the segment display to show the count down in
 * seconds till the alarm goes off for the door being open for too
 * long
 */
unsigned long int LED_period = period * 1000;
unsigned long int LED_elapsed_time = LED_period;
enum LED_states { LED_SMStart, LED_on} LED_state;
void TickFct_LEDcount() {
  switch(LED_state){
    case LED_SMStart:
      if(!countdown)
        LED_state = LED_on;
      else
        LED_state = LED_SMStart;
    break;
    case LED_on:
      if(!countdown)
        LED_state = LED_on;
      else
        LED_state = LED_SMStart;
  }
  switch(LED_state){
    case LED_SMStart:
      digit = max_time - 1; 
      sevenSegWrite(digit, on);
    break;
    case LED_on:
      digit = digit - 1;
      if(digit < 0)
        digit = 0;
      sevenSegWrite(digit, on);
    break;
  }
}
/////////////////////////////////////////////////////////////////////
//function for getting the raw data from the ultrasonic and calculating
//the distance and returning that
int utlrasonic(){
  float dur;
  int dist;
  
  digitalWrite(trig_pin, LOW); 
  delayMicroseconds(2);
  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig_pin, LOW);
  
  //mesures the response from the echo pin
  dur = pulseIn(echo_pin, HIGH);
  //used speed of sould as 343mps to help calculates distance
  dist = (int)((dur / 2) * 0.0343);
  /*if (dist >= 400 || dist <= 2){
    Serial.print("Distance = ");
    Serial.println("Out of range");
  }
  else {
    Serial.print("Distance = ");
    Serial.print(dist);
    Serial.println(" cm");
    Serial.println(countdown);
  }*/
  
  return dist;
}
/////////////////////////////////////////////////////////////////////
/*
 * SM for getting a reading on the ultrasonic and setting the first reading
 * as the door closed state and any reading that is less than 20% of that
 * target distance is considered the door open
*/
unsigned long int US_period = period * 100;
unsigned long int US_elapsed_time = US_period;
enum US_states {US_SMStart, US_on, US_check} US_state;
void TickFct_Ultrasonic() {
  double pos_10;
  double minus_20;
  double temp;
  switch(US_state){
    case US_SMStart:
      US_state = US_on;
    break;
    case US_on:
      US_state = US_check;
    break;
    case US_check:
      US_state = US_check;
    break;
  }
  switch(US_state){
    case US_on:
      closed_distance = utlrasonic();
      minus_20 = closed_distance - (closed_distance * 0.20);
      //break;
    case US_check:
      temp = utlrasonic();
      if(temp > minus_20)
        countdown = true;
      else
        countdown = false;
    break;
  }
}
/////////////////////////////////////////////////////////////////////


void setup() {
    // Set latchPin, clockPin, dataPin as output
    pinMode(latchPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(dataPin, OUTPUT);

    pinMode(button, INPUT_PULLUP);

    wait_button = false;
    digit = max_time;

    //ultrasonic stuff
    Serial.begin (9600);
    pinMode(trig_pin, OUTPUT);
    pinMode(echo_pin, INPUT);

    pinMode(passive_buz, OUTPUT);
    //for starting the timer and starting the SM's
    Timer1.initialize(period);//period 10 milliseconds
    BZ_state = BZ_SMStart;
    LED_state = LED_SMStart;
    PR_state = PR_SMStart;
    US_state = US_SMStart;
    Timer1.attachInterrupt(TimerISR);

}
/////////////////////////////////////////////////////////////////////

void loop() {

    if(timer_flag){
      if(PR_elapsed_time >= PR_period){
        TickFct_button_press();
        PR_elapsed_time = 0;
      }
      //Serial.println(change_sytem_state);
      if(change_sytem_state){
        if(US_elapsed_time >= US_period){
          TickFct_Ultrasonic();
          US_elapsed_time = 0;
        } 
        
        if(BZ_elapsed_time >= buzz_period){
          TickFct_BuzzSpeek();
          BZ_elapsed_time = 0;
        }
        if(LED_elapsed_time >= LED_period){
          TickFct_LEDcount();
          LED_elapsed_time = 0;
        }
        BZ_elapsed_time += period;
        LED_elapsed_time += period;
        US_elapsed_time += period; 
      }
      PR_elapsed_time += period;
      timer_flag = 0;
    }
}
