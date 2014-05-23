/**************************************************************
Es soll alle halbe Sekunde im Wechsel 0 bzw. 1 gesendet werden.
Am korrespondierenden Slave soll zur Indikation jeweils die 
LEDs an bzw. aus gehen
Verdrahtung:	MISO(Master) --> MISO(Slave)
				MOSI(Master) --> MOSI(Slave)
				SCK(Master)  --> SCK(Slave)
				PB2(Master)	 --> SS(Slave)
**************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>

#define PIN_UP		PD3 /* Taster Hoch */
#define PIN_DOWN	PD4 /* Taster Runter */
#define PIN_STOP	PD2 /* Taster Stop (INT0) */

#define LED		PD5 /* LED auf Dev Board / Spaeter PC3 */

unsigned char status = 0;
volatile unsigned char count = 0;

void int0_init (void);
void timer1 (void);
void master_init (void);
void master_transmit (unsigned char data);

/*ISR names found in avr/iom8.h */
ISR (SPI_STC_vect) {
	return;
}

/*ISR names found in avr/iom8.h */
ISR (TIMER1_OVF_vect) {						//Senderoutine
	if (PIND & (1<<PIN_STOP))
	{
		master_transmit ('S');
	} else if (status == 0) 
	{
		status = 1;
		switch(PIND & ((1<<PIN_UP)|(1<<PIN_DOWN)))
		{
		case (1<<PIN_UP):
			PORTD |= (1<<LED);
			master_transmit ('U');
			count = 0;
			break;
		case (1<<PIN_DOWN):
			PORTD |= (1<<LED);
			master_transmit ('D');
			count = 0;
			break;
		case ((1<<PIN_UP)|(1<<PIN_DOWN)):
			master_transmit ('S');
			count = 0;
			break;
		default:
			PORTD &= ~(1<<LED);
			break;
		}
	} else if (status == 1 && (PIND & ((1<<PIN_UP)|(1<<PIN_DOWN))) == 0) {
		status = 0;
	}
}

ISR (INT0_vect) {
	PORTD |= (1<<LED);
//	PORTD &= ~(1<<LED);
	master_transmit ('S');
}

void timer1 (void) {
	TIMSK |= (1<<TOIE1);    //Timer Overflow Interrupt enable
	TCNT1 = 0;       	//Rücksetzen des Timers
	TCCR1B = (1<<CS10);	//8MHz/65536/64 = 1,91Hz --> 0,5s
}




void master_init (void) {
	DDRD = 0;		// PortD alles Eingang
	DDRD |= (1<<LED);	// LED als Ausgang (Dev)
	DDRB = (1<<PB2) | (1<<PB3) | (1<<PB5);		// setze SCK,MOSI,PB2 (SS) als Ausgang
	DDRB &= ~(1<<PB4);				// setze MISO als Eingang
	PORTB = (1<<PB5) | (1<<PB2);			// SCK und PB2 high (ist mit SS am Slave verbunden)
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);	//Aktivierung des SPI, Master, Taktrate fck/16
	status = SPSR;								//Status löschen
}

void master_transmit (unsigned char data) {
	PORTB &= ~(1<<PB2);			//SS am Slave Low --> Beginn der Übertragung
	SPDR = data;				//Schreiben der Daten
	while (!(SPSR & (1<<SPIF)));
	PORTB |= (1<<PB2);			//SS High --> Ende der Übertragung
}

void int0_init (void) {
	MCUCR |= (1<<ISC01) | (1<<ISC00);
	GICR |= (1<<INT0);
}

int main (void) {
	master_init ();
	int0_init ();
	timer1 ();
	sei ();

	for (;;);
	return 0;
}
