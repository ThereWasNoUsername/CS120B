/*
 * achen115_lab9.c
 *
 * Created: 5/1/2019 2:29:39 PM
 * Author : Alex
 */ 

#include <avr/io.h>


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



int main(void)
{
    /* Replace with your application code */
	DDRA = 0;	PINA = -1;
	DDRB = -1; PORTB = 0;
	PWM_on();
	const unsigned char buttonValues[] = {1, 2, 4};
    while (1)
    {
		char pressed = ~PINA;
		char buttonIndex = -1;
		for(char i = 0; i < 3; i++) {
			if(buttonValues[i] == pressed) {
				buttonIndex = i;
				break;
			}
		}
		if(buttonIndex > -1) {
			const double frequency[] = {261.63, 293.66, 329.63};
			PORTB = buttonIndex;
			set_PWM(frequency[buttonIndex]);
		}

    }
}

