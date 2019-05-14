/*
 * achen115_lab9.c
 *
 * Created: 5/1/2019 2:29:39 PM
 * Author : Alex
 */ 

#include <avr/io.h>
#include "io.c"
#include <avr/io.h>
#include <avr/interrupt.h>

volatile unsigned char TimerFlag = 0; // TimerISR() sets this to 1. C programmer should clear to 0.

// Internal variables for mapping AVR's ISR to our cleaner TimerISR model.
unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks

void TimerOn() {
	// AVR timer/counter controller register TCCR1
	TCCR1B = 0x0B;// bit3 = 0: CTC mode (clear timer on compare)
	// bit2bit1bit0=011: pre-scaler /64
	// 00001011: 0x0B
	// SO, 8 MHz clock or 8,000,000 /64 = 125,000 ticks/s
	// Thus, TCNT1 register will count at 125,000 ticks/s

	// AVR output compare register OCR1A.
	OCR1A = 125;    // Timer interrupt will be generated when TCNT1==OCR1A
	// We want a 1 ms tick. 0.001 s * 125,000 ticks/s = 125
	// So when TCNT1 register equals 125,
	// 1 ms has passed. Thus, we compare to 125.
	// AVR timer interrupt mask register
	TIMSK1 = 0x02; // bit1: OCIE1A -- enables compare match interrupt

	//Initialize avr counter
	TCNT1=0;

	_avr_timer_cntcurr = _avr_timer_M;
	// TimerISR will be called every _avr_timer_cntcurr milliseconds

	//Enable global interrupts
	SREG |= 0x80; // 0x80: 1000000
}

void TimerOff() {
	TCCR1B = 0x00; // bit3bit1bit0=000: timer off
}

void TimerISR() {
	TimerFlag = 1;
}

// In our approach, the C programmer does not touch this ISR, but rather TimerISR()
ISR(TIMER1_COMPA_vect) {
	// CPU automatically calls when TCNT1 == OCR1 (every 1 ms per TimerOn settings)
	_avr_timer_cntcurr--; // Count down to 0 rather than up to TOP
	if (_avr_timer_cntcurr == 0) { // results in a more efficient compare
		TimerISR(); // Call the ISR that the user uses
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

// Set TimerISR() to tick every M ms
void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

// 0.954 hz is lowest frequency possible with this function,
// based on settings in PWM_on()
// Passing in 0 as the frequency will stop the speaker from generating sound
void set_PWM(double frequency) {
	static double current_frequency; // Keeps track of the currently set frequency
	// Will only update the registers when the frequency changes, otherwise allows
	// music to play uninterrupted.
	if (frequency != current_frequency) {
		if (!frequency) { TCCR0B &= 0x08; } //stops timer/counter
		else { TCCR0B |= 0x03; } // resumes/continues timer/counter
		
		// prevents OCR3A from overflowing, using prescaler 64
		// 0.954 is smallest frequency that will not result in overflow
		if (frequency < 0.954) { OCR0A = 0xFFFF; }
		
		// prevents OCR0A from underflowing, using prescaler 64                    // 31250 is largest frequency that will not result in underflow
		else if (frequency > 31250) { OCR0A = 0x0000; }
		
		// set OCR3A based on desired frequency
		else { OCR0A = (short)(8000000 / (128 * frequency)) - 1; }

		TCNT0 = 0; // resets counter
		current_frequency = frequency; // Updates the current frequency
	}
}
void PWM_on() {
	TCCR0A = (1 << WGM02) | (1 << WGM00) | (1 << COM0A0);
	// COM3A0: Toggle PB3 on compare match between counter and OCR0A
	TCCR0B = (1 << WGM02) | (1 << CS01) | (1 << CS00);
	// WGM02: When counter (TCNT0) matches OCR0A, reset counter
	// CS01 & CS30: Set a prescaler of 64
	set_PWM(0);
}

void PWM_off() {
	TCCR0A = 0x00;
	TCCR0B = 0x00;
}

void play() {
	const double G3 = 196.0, A3 = 220.0, B3 = 246.94, C4 = 261.63, Db4 = 277.18, D4 = 293.66, Eb4 = 311.13, E4 = 329.63, F4 = 349.23, Gb4 = 369.99, G4 = 392, Ab4 = 415.30, A4 = 440, Bb4 = 466.16, B4 = 494.88, C5 = 523.25, Db5 = 544.37, D5 = 587.33;
	const short whole = 1000;
	const short halfdot = whole * 3 / 4;
	const short half = whole/2;
	const short quarter = half/2;
	const double notes[] = {
		E4, B3, C4,
		D4, C4, B3,
		A4, A4, C4,
		E4, D4, C4,
		B3, B3, C4,
		D4, E4,
		C4, A4,
		A4, 0,

		D4, F4, A4, G4,
		F4, E4, 0, C4,
		E4, D4, C4, B3,

		B3, C4,
		D4,
		E4,
		C4,
		A4, A4, 0,

			E4, B3, C4,
			D4, C4, B3,
			A4, A4, C4,
			E4, D4, C4,
			B3, B3, C4,
			D4, E4,
			C4, A4,
			A4, 0,

			D4, F4, A4, G4,
			F4, E4, 0, C4,
			E4, D4, C4, B3,

			B3, C4,
			D4,
			E4,
			C4,
			A4, A4, 0,
			/*
				D4, F4, A4, G4,
				F4, E4, 0, C4,
				E4, D4, C4, B3,

				B3, C4,
				D4,
				E4,
				C4,
				A4, A4, 0,
				*/

		E4, C4, D4, B4, C4, A4,
		Ab4, E4, F4, E4, 

		E4, C4, D4, B4, C4, A4,
		Ab4, E4, F4, E4
	};
	const short times[] = {
		half, quarter, quarter,
		half, quarter, quarter,
		half, quarter, quarter,
		half, quarter, quarter,
		half, quarter, quarter,
		half, half,
		half, half,
		whole, quarter,

		half, quarter, half, quarter,
		quarter, half, quarter, quarter,
		half, quarter, quarter, half,

		quarter, quarter,
		half,
		half,
		half,
		half, half, half,

			half, quarter, quarter,
			half, quarter, quarter,
			half, quarter, quarter,
			half, quarter, quarter,
			half, quarter, quarter,
			half, half,
			half, half,
			whole, quarter,

			half, quarter, half, quarter,
			quarter, half, quarter, quarter,
			half, quarter, quarter, half,

			quarter, quarter,
			half,
			half,
			half,
			half, half, half,

				half, quarter, half, quarter,
				quarter, half, quarter, quarter,
				half, quarter, quarter, half,

				quarter, quarter,
				half,
				half,
				half,
				half, half, half,

		whole, whole, whole, whole, whole, whole,
		half, half, half, half,

		whole, whole, whole, whole, whole, whole,
		half, half, half, half

	};
	const short length = 102;//122;


	static short time = 0;
	static short note = -1;
	if(--time > 0)
		return;
	if(++note >= length)
		note = 0;

	set_PWM(notes[note]);
	time = times[note];
}
int main(void)
{
    /* Replace with your application code */
	DDRA = 0;	PINA = -1;
	DDRB = 0x1F; PORTB = 0;
	PWM_on();
	TimerSet(1);
	TimerOn();
	while(1) {
		play();
		while(!TimerFlag);
		TimerFlag = 0;
	}
}

