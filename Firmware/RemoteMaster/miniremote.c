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

/* 
  ISR für Timer 1, parametriert in timer1()
*/
ISR (TIMER1_OVF_vect) {	
	if (PIND & (1<<PIN_STOP))				// Sende S wenn Stop gedrückt wurde
	{
		master_transmit ('S');
	}
	else if (status == 0) 				// Wenn beim letzten durchlauf der ISR kein Button gedrückt wurde
	{
		/* Mask the two buttons 'up'' and 'down' on the register */
		switch(PIND & ((1<<PIN_UP)|(1<<PIN_DOWN)))	// Prüfen ob Taster hoch oder Runter betätigt wurde
		{
		case (1<<PIN_UP):				// Wenn Taster hoch betätigt wurde
			PORTD |= (1<<LED);			// Aktiviere LED
			master_transmit ('U');			// Sende U (hoch) an Master
			status = 1;				// Taster wurde betätigt	
			break;
		case (1<<PIN_DOWN):				// Wenn Taster runter betätigt wurde
			PORTD |= (1<<LED);			// s.o.
			master_transmit ('D');
			status = 1;					
			break;
		default:
			PORTD &= ~(1<<LED);			// Wenn kein Taster betätigt wurde, deaktiviere LED
			break;
		}
	} else if (status == 1 && (PIND & ((1<<PIN_UP)|(1<<PIN_DOWN))) == 0) {	// Wenn bei letztem Durchlauf Tasterbetätigt wurden, aber es nichtmehr sind
		status = 0;							// Setze Status zurück
	}
}

/*
  ISR für Taster INT0 (PIN 4), Taster STOP 
*/
ISR (INT0_vect) {
	PORTD |= (1<<LED);					// Aktiviere LED
//	PORTD &= ~(1<<LED);
	master_transmit ('S');					// Sende STOP an Leinwand
}

/*
  Initialisierung für Timer1
*/
void timer1 (void) {
	TIMSK |= (1<<TOIE1);           				//Timer Overflow Interrupt enable
	TCNT1 = 0;                				//Rücksetzen des Timers
	TCCR1B = (1<<CS10);					//8MHz/65536 = 1,2kHz --> 0,0008192 s --> 0,82 ms
}

/*
  Initialisierung der Ein und Ausgänge
*/
void master_init (void) {
	DDRD |= (1<<LED);					// LED als Ausgang (Dev)
	DDRB = (1<<PB2) | (1<<PB3) | (1<<PB5);			// setze SCK,MOSI,PB2 (SS) als Ausgang
	DDRB &= ~(1<<PB4);					// setze MISO als Eingang
	PORTB = (1<<PB5) | (1<<PB2);				// SCK und PB2 high (ist mit SS am Slave verbunden)
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);		//Aktivierung des SPI, Master, Taktrate fck/16
	status = SPSR;						//Status löschen
}

/*
  Senden der Daten via SPI
*/
void master_transmit (unsigned char data) {
	PORTB &= ~(1<<PB2);					//SS am Slave Low --> Beginn der Übertragung
	SPDR = data;						//Schreiben der Daten
	while (!(SPSR & (1<<SPIF)));				//warten bis die Übertragung beendet ist
	PORTB |= (1<<PB2);					//SS High --> Ende der Übertragung
}

/*
  Initialisieren des Intterupts für INT0 (Taster Stop)
*/
void int0_init (void) {
	MCUCR |= (1<<ISC01) | (1<<ISC00);			// Triggern auf Steigende Flanke
	GICR |= (1<<INT0);					// Aktivieren des Interrupts
}

int main (void) {
	master_init ();						// Initialisieren
	int0_init ();						// INT0 (Taster Stop) Initialisieren
	timer1 ();						// Timer1 Initialisieren
	sei ();							// Aktiveren des Interrupts

	for (;;);						// Endlosschleife
	return 0;
}
