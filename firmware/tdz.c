/*
 * tdz.c
 *
 * Created: 14.06.2013 00:23:12
 *  Author: alex
 */


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/cpufunc.h>
#include <util/delay.h>
#include <string.h>

#define D	1<<PB0
#define E	1<<PB1
#define C	1<<PB2
#define G	1<<PB3
#define B	1<<PB4
#define F	1<<PB5
#define A	1<<PB7

#define S_0	(A|B|C|D|E|F)
#define S_1	(B|C)
#define S_2	(A|B|D|E|G)
#define S_3	(A|B|C|D|G)
#define S_4	(B|C|F|G)
#define S_5	(A|C|D|F|G)
#define S_6	(A|C|D|E|F|G)
#define S_7	(A|B|C)
#define S_8	(A|B|C|D|E|F|G)
#define S_9	(A|B|C|D|F|G)
#define S_E	(A|D|E|F|G)

const uint8_t symbols[] PROGMEM = {S_0,S_1,S_2,S_3,S_4,S_5,S_6,S_7,S_8,S_9};

#define T1_PRESCALER		1024
#define SECONDS_PER_50_C	300
#define BUZZER_THRESHOLD	60
#define DISPLAY_COUNT		4

volatile struct {
	uint16_t countdown;
	uint8_t display_digits[DISPLAY_COUNT];
}globals;


/*
 * Scans all 4 7-segment displays once
 * Run as often as possible
 */
void update7Seg(void){
	cli();																//We can't have this function interrupted or we risk destroying the displays
	for (uint8_t i=0;i<DISPLAY_COUNT;i++){
		if (globals.display_digits[i]>=sizeof(symbols)){
			//TODO: Possible error handling for invalid symbols
		}
		PORTB = (PORTB | 0b10111111) ^ (pgm_read_byte(symbols + globals.display_digits[i]));
		PORTC = (PORTC | 0b00001111) ^ (1<<i);
		_delay_us(10);
		PORTC |= 0b00001111;
		PORTB = 0b10111111;
	}
	PORTC |= 0b00001111;
	sei();
}


void start_timer1(void){
	TCNT1=0;
	TIMSK=1<<OCIE1A;
	OCR1A=F_CPU/T1_PRESCALER;
	TCCR1B=1<<CS12|1<<CS10|1<<WGM12;
}

/*
 * GPIO Assignments:
 * 	PORTB0-5,7	Output	Display Cathodes(active low)
 * 	PORTB6		Input	Unused	(Pullups on)
 *
 * 	PORTC0-3	Output	Display Anodes(active low)
 * 	PORTC4-5	Input	Unused	(Pullups on)
 *
 * 	PORTD0		Input	Unused	(Pullups on)
 * 	PORTD1		Output	INHIBIT	(active high)
 * 	PORTD2		Input	Credit	(Pullups on)
 * 	PORTD3		Input	Counter	(Pullups on)
 * 	PORTD4		Output	Power Off(active high)
 * 	PORTD5-7	Input	Unused	(Pullups on)
 */
inline void init_gpio(void){
	PORTB=0xFF;
	PORTC=0xFF;
	PORTD=0b11111101;
	DDRB=0b10111111;
	DDRC=0b00001111;
	DDRD=0b00010010;
}

inline void init_ext_interrupt(void){
	MCUCR=1<<ISC11;
	GICR=1<<INT1;
}

int main(void)
{
	init_gpio();
	init_ext_interrupt();
	memset(&globals,0,sizeof(globals));
	sei();
	while(1)
	{
		update7Seg();
	}
}

// Runs on the falling edge of the Counter signal
ISR(INT1_vect){
	// Check the interrupt pin multiple times to make sure it isn't just a glitch
	for(uint8_t i=20;i>0;i--){
		if (PIND & 1<<PD3){
			return;
		}
	}
	globals.countdown+=SECONDS_PER_50_C;
	PORTD&=~(1<<PD4);	//Enable the heater
	start_timer1();
}


// Runs every 1 second. Used to update the countdown timer and its BCD representation.
ISR(TIMER1_COMPA_vect){
	uint16_t minutes,seconds,countdown_temp;

	globals.countdown--;
	countdown_temp=globals.countdown;
	if (countdown_temp == 0){
		PORTD|=1<<PD4;	//Disable the heater
		TCCR1B=0;		//Deinit the timer
		TIMSK&=~(OCIE1A);
	}
//	else if (countdown_temp < BUZZER_THRESHOLD){
//		//TODO: Buzzer code here
//	}

	//Convert the numbers to full BCD
	minutes = countdown_temp/60;
	seconds = countdown_temp%60;
	if (minutes > 99){
		minutes = 99;
	}

	globals.display_digits[3]=seconds%10;
	globals.display_digits[2]=seconds/10;
	globals.display_digits[1]=minutes%10;
	globals.display_digits[0]=minutes/10;
}
