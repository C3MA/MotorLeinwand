/**************************************************************
Es soll alle halbe Sekunde im Wechsel 0 bzw. 1 gesendet werden.
Am korrespondierenden Slave soll zur Indikation jeweils die 
LEDs an bzw. aus gehen
Verdrahtung:	MISO(Master) --> MISO(Slave)
				MOSI(Master) --> MOSI(Slave)
				SCK(Master)  --> SCK(Slave)
				PB0(Master)	 --> SS(Slave)
**************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>

volatile unsigned char count = 0;

/*ISR names found in avr/iom8.h */
ISR (TIMER1_OVF_vect) {						//Senderoutine
	if (count == 1) {
		count--;
		PORTD = 0x0;
	} else if (count == 0) {
		count++;
		PORTD = 0xff;
	}
}

void timer1 (void) {
	TIMSK |= (1<<TOIE1);           				//Timer Overflow Interrupt enable
	TCNT1 = 0;                					//Rücksetzen des Timers
	TCCR1B = (1<<CS10) | (1<<CS11);			//8MHz/65536/64 = 1,91Hz --> 0,5s
}

int main (void) {
	DDRD = 0xff;		// PortD als Ausgang nutzen (Chris & Ollo geraten)
	timer1 ();
	sei ();

	for (;;);
	return 0;
}
