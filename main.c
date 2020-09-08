/*
 *  Author: Juraj Lazorik
 * 	Login:  xlazor02
 * 	Date:   20.December 2019
 *
 * */

#include "MKL05Z4.h"
#include <time.h>
#include <main.h>


/* Activation of particular LED display (DS1 - DS4) */
#define D1 0x0700
#define D2 0x0B00
#define D3 0x0D00
#define D4 0x0E00


/* Encoding of digits as active segments on specific LED display (DS1 - DS4) */
#define N0 0x0707
#define N1 0x0006
#define N2 0x0B03
#define N3 0x0907
#define N4 0x0C06
#define N5 0x0D05
#define N6 0x0F05
#define N7 0x0007
#define N8 0x0F07
#define N9 0x0D07


/* Bit-level masks that help to enable/disable DP segment on LED display */
#define MASK_DOT_ON 0x0008
#define MASK_DOT_OFF 0xFFF7


#define PB4_ISF_MASK 0x10


int show_dot = 0;
int display_selection =  1;
int number_test = 0;

int number_display1 = 0;
int number_display2 = 0;
int number_display3 = 0;
int number_display4 = 0;

unsigned int sleepCounter = 0;
unsigned int blickCounter = 125;
unsigned int hour_flag = 0;
unsigned int minute_flag = 0;
int ret;

struct tm read_time;


/* Just an ordinary delay loop */
void delay(long long bound) {

  long long i;
  for(i=0;i<bound;i++);
}


/* Let's turn off individual segments on the whole display */
void off() {

  PTB->PDOR = GPIO_PDOR_PDO(0x0000);
  PTA->PDOR = GPIO_PDOR_PDO(D1);
  PTA->PDOR = GPIO_PDOR_PDO(D2);
  PTA->PDOR = GPIO_PDOR_PDO(D3);
  PTA->PDOR = GPIO_PDOR_PDO(D4);

}


/* Basic initialization of GPIO features on PORTA and PORTB */
void ports_init (void)
{
  SIM->COPC = SIM_COPC_COPT(0x00);							   // Just disable the usage of WatchDog feature
  SIM->SCGC5 = (SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK);  // Turn on clocks for PORTA and PORTB

  SIM_CLKDIV1 |= SIM_CLKDIV1_OUTDIV1(0x00); //Frequency div
  SIM->SCGC6 = SIM_SCGC6_RTC_MASK; // Access to RTC module

  /* Set corresponding PORTA pins for GPIO functionality */
  PORTA->PCR[8] = ( 0|PORT_PCR_MUX(0x01) );  // display DS4
  PORTA->PCR[9] = ( 0|PORT_PCR_MUX(0x01) );  // display DS3
  PORTA->PCR[10] = ( 0|PORT_PCR_MUX(0x01) ); // display DS2
  PORTA->PCR[11] = ( 0|PORT_PCR_MUX(0x01) ); // display DS1

  /* Set corresponding PORTA port pins as outputs */
  PTA->PDDR = GPIO_PDDR_PDD( 0x0F00 );  // "1" configures given pin as an output

  NVIC_DisableIRQ(31);  // Disable the eventual generation of the interrupt caused by the control button

  /* Set corresponding PORTB pins for GPIO functionality */
  PORTB->PCR[0] = ( 0|PORT_PCR_MUX(0x01) );   // seg A
  PORTB->PCR[1] = ( 0|PORT_PCR_MUX(0x01) );   // seg B
  PORTB->PCR[2] = ( 0|PORT_PCR_MUX(0x01) );   // seg C
  PORTB->PCR[3] = ( 0|PORT_PCR_MUX(0x01) );   // seg DP
  PORTB->PCR[8] = ( 0|PORT_PCR_MUX(0x01) );   // seg D
  PORTB->PCR[9] = ( 0|PORT_PCR_MUX(0x01) );   // seg E
  PORTB->PCR[10] = ( 0|PORT_PCR_MUX(0x01) );  // seg F
  PORTB->PCR[11] = ( 0|PORT_PCR_MUX(0x01) );  // seg G

  /* Set corresponding PORTB port pins as outputs */
  PTB->PDDR = GPIO_PDDR_PDD( 0x0F0F ); // "1" configures given pin as an input
  PORTB->PCR[4] = ( 0 | PORT_PCR_ISF(1) | PORT_PCR_IRQC(0x0A) | PORT_PCR_MUX(0x01) |
					    PORT_PCR_PE(1) | PORT_PCR_PS(1)); // display SW1

  /* Let's clear any previously pending interrupt on PORTB and allow its subsequent generation */
 NVIC_ClearPendingIRQ(31);
 NVIC_EnableIRQ(31);
}

/* Basic initialization of RTC module and oscillator */
void RTCInit() {

    RTC_CR |= RTC_CR_SWR_MASK;  // SWR = 1, reset all RTC's registers
    RTC_CR &= ~RTC_CR_SWR_MASK; // SWR = 0

    RTC_TCR = 0x0000; // reset CIR and TCR - 1 sec

    RTC_CR |= RTC_CR_OSCE_MASK; // enable 32.768 kHz oscillator

    delay(0x300000); //oscillator startup time 3-4 sec

    RTC_SR &= ~RTC_SR_TCE_MASK; // RTC off

    RTC_TSR = 0x00000000; // MIN value in 32bit register start value
    RTC_TAR = 0xFFFFFFFF; // MAX value in 32bit register alarm

    RTC_SR |= RTC_SR_TCE_MASK; // RTC on
}

/* Service routine invoked upon the press of a control button */
void PORTB_IRQHandler( void )
{
	delay(100);
	unsigned int press_time_flag = 0;
	sleepCounter = 0;

	if (PORTB->ISFR & GPIO_PDIR_PDI(PB4_ISF_MASK)) {
		 off();
	  if (!(PTB->PDIR & GPIO_PDIR_PDI(PB4_ISF_MASK))) {
	    while(!(PTB->PDIR & GPIO_PDIR_PDI(PB4_ISF_MASK))){
	    	press_time_flag++;
	    }
	  }
	  PORTB->PCR[4] |= PORT_PCR_ISF(0x01);  // Confirmation of interrupt after button press
	}

	if(press_time_flag > 1500000){ //Long press
		press_time_flag = 0;
		blickCounter = 125;
		if (hour_flag == 1 && minute_flag == 1)
		{
			read_time.tm_sec = 0;
			hour_flag = 0;
			minute_flag = 0;
		}
		else
		{
			if (hour_flag == 1)
			{
				minute_flag = 1;
			}
			else
			{
				hour_flag = 1;
			}
		}
	}
	else //short press
	{
		  press_time_flag = 0;
		  blickCounter = 125;
		  if (hour_flag == 1 && minute_flag == 0){
			  //add hour
			  addHour();
		  }
		  if (hour_flag == 1 && minute_flag == 1){
		  	//add minutes
			  addMinute();
		  }
	}

}

/* Single digit shown on a particular section of the display  */
void sn(int number, uint32_t display) {

  uint32_t n;

  switch (number) {
    case 0:
      n = N0; break;
    case 1:
      n = N1; break;
    case 2:
      n = N2; break;
    case 3:
      n = N3; break;
    case 4:
      n = N4; break;
    case 5:
      n = N5; break;
    case 6:
      n = N6; break;
    case 7:
      n = N7; break;
    case 8:
      n = N8; break;
    case 9:
      n = N9; break;
    default:
      n = N0;
  }

  if (show_dot)
    n |= MASK_DOT_ON;
  else
    n &= MASK_DOT_OFF;

  PTA->PDOR = GPIO_PDOR_PDO(display);
  PTB->PDOR = GPIO_PDOR_PDO(n);

  delay(10); //10
}


void Display_number() {
  PTA->PDOR = GPIO_PDOR_PDO(0x0000);
  PTB->PDOR = GPIO_PDOR_PDO(0x0000);

  uint32_t n;

  switch (display_selection) {
      case 0:
      case 1:
        n = D1; break;
      case 2:
        n = D2; break;
      case 3:
        n = D3; break;
      case 4:
        n = D4; break;
      default:
        n = D1;
    }

  sn(number_test, n);
  delay(3000);
  off();
}

/* Display number on individual displays*/
void Display_all(){

	if (display_selection == 1)
	{
		number_test = number_display1;
		Display_number();
	}
	if (display_selection == 2)
	{
		number_test = number_display2;
		show_dot = 1;
		Display_number();
	}
	if (display_selection == 3)
	{
		number_test = number_display3;
		Display_number();
	}
	if (display_selection == 4)
	{
		number_test = number_display4;
		Display_number();
	}

	if (display_selection < 4)
		display_selection++;
	else
		display_selection = 1;
	show_dot = 0;

}

/* Convert time from 32bit number to human readable format and put into structure read_time */
void time_convert(unsigned int actual_time) {

	time_t t = actual_time;

	(void) localtime_r(&t, &read_time);

}

/* Minutes start blink (when we are setting minutes)*/
void SetMinutes(){

	time_convert(RTC_TSR);

		number_display1 = read_time.tm_hour / 10;
		number_display2 = read_time.tm_hour % 10;

		number_display3 = read_time.tm_min / 10;
		number_display4 = read_time.tm_min % 10;

		if(blickCounter < 125){ //Display only hours for short period - blink

			if (display_selection == 1)
			{
				number_test = number_display1;
				Display_number();
			}
			if (display_selection == 2)
			{
				number_test = number_display2;
				show_dot = 1;
				Display_number();
			}

			if (display_selection < 2)
				display_selection++;
			else
				display_selection = 1;
			show_dot = 0;
		}
		else //Display minutes and hours for short period - blink
		{
			if(blickCounter >250){
				blickCounter = 0;
			}
			else
			{
				Display_all();
			}
		}
}

/* Hours start blink (when we are setting hours)*/
void SetHours(){

	time_convert(RTC_TSR);

	number_display1 = read_time.tm_hour / 10;
	number_display2 = read_time.tm_hour % 10;

	number_display3 = read_time.tm_min / 10;
	number_display4 = read_time.tm_min % 10;

	if(blickCounter < 125){ //Display only minutes for short period - blink

		if (display_selection == 3)
		{
			number_test = number_display3;
			Display_number();
		}
		if (display_selection == 4)
		{
			number_test = number_display4;
			Display_number();
		}

		if (display_selection < 4)
			display_selection++;
		else
			display_selection = 3;

	}
	else //Display minutes and hours for short period - blink
	{
		if(blickCounter >250){
			blickCounter = 0;
		}
		else
		{
			Display_all();
		}

	}
}

/* Add +1 hour*/
void addHour(){

	read_time.tm_hour++;

	ret = mktime(&read_time); //change read_time structure to 32bit number

	RTC_SR &= ~RTC_SR_TCE_MASK; // RTC off
	RTC_TSR = ret;
	RTC_SR |= RTC_SR_TCE_MASK; // RTC on

	ret = 0;
	time_convert(RTC_TSR);
}

/* Add +1 minute*/
void addMinute(){

	if(read_time.tm_min == 59) //after 60 minutes we don't want add +1 hour
	{
		read_time.tm_hour--;
	}

	read_time.tm_min++;

	ret = mktime(&read_time); //change read_time structure to 32bit number

	RTC_SR &= ~RTC_SR_TCE_MASK; // RTC off
	RTC_TSR = ret;
	RTC_SR |= RTC_SR_TCE_MASK; // RTC on

	ret = 0;
	time_convert(RTC_TSR);
}

/* Enable and activate VLPS(very low power stop) mode */
void SleepMode(){

	sleepCounter = 0;

	SMC_PMPROT = 0x20;
	SMC_PMCTRL = 0x02;
	__WFI();
}


int main(void)
{
	ports_init();
	RTCInit();

	for (;;) {

		if(minute_flag == 1)
		{
			blickCounter++;
			SetMinutes();
			continue;
		}

		if(hour_flag == 1)
		{
			blickCounter++;
			SetHours();
			continue;
		}

		sleepCounter++;
		if(sleepCounter > 750){
			SleepMode();
		}

		time_convert(RTC_TSR);

		number_display1 = read_time.tm_hour / 10;
		number_display2 = read_time.tm_hour % 10;

		number_display3 = read_time.tm_min / 10;
		number_display4 = read_time.tm_min % 10;

		Display_all();
	}

	return 0;
}
////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////

