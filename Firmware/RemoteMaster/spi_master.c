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

#define BUTTON_UP	PD2	/* Button to send an "Up SPI signal" */	
#define BUTTON_DOWN	PD3	/* Button to send an "Down SPI signal" */	
#define BUTTON_STOP	PD4	/* Button to send an "Stop SPI signal" */	

#define DIAGNOSE_LED	PD5	/* Nur zu debug Zwecken */

unsigned char status = 0;
volatile unsigned char count = 0;

/*ISR names found in avr/iom8.h */
ISR (TIMER1_OVF_vect) {						//Senderoutine
	if (count == 1) {
		count--;
		master_transmit('0');
		PORTD &= ~(1<<DIAGNOSE_LED);
	} else if (count == 0) {
		count++;
		master_transmit('1');
		PORTD |= (1<<DIAGNOSE_LED);
	}
}

void timer1 (void) {
	TIMSK |= (1<<TOIE1);           				//Timer Overflow Interrupt enable
	TCNT1 = 0;                					//Rücksetzen des Timers
	TCCR1B = (1<<CS10) | (1<<CS11);			//8MHz/65536/64 = 1,91Hz --> 0,5s
}

void master_init (void) {
	DDRB = (1<<PB2) | (1<<PB3) | (1<<PB5);		// setze SCK,MOSI,PB2 (SS) als Ausgang
	DDRB &= ~(1<<PB4);							// setze MISO als Eingang
	PORTB = (1<<PB5) | (1<<PB2);				// SCK und PB2 high (ist mit SS am Slave verbunden)
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);	//Aktivierung des SPI, Master, Taktrate fck/16
	status = SPSR;								//Status löschen
}

void master_transmit (unsigned char data) {
	PORTB &= ~(1<<PB2);						//SS am Slave Low --> Beginn der Übertragung
	SPDR = data;								//Schreiben der Daten
	while (!(SPSR & (1<<SPIF)));
	PORTB |= (1<<PB2);							//SS High --> Ende der Übertragung
}

int main (void) {
	DDRD = 0;
	DDRD |= (1<<DIAGNOSE_LED);   // passende Pins als Ausgang markieren
	timer1 ();
	sei ();

	for (;;);
	return 0;
}
