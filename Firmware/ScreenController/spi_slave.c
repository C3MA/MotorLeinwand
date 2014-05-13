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

volatile unsigned char data;
unsigned char status;

/*ISR names found in avr/iom8.h */
ISR (SPI_STC_vect) {
	data = SPDR;
	if (data == '1')	PORTD = 0x00;			//LEDs an
	if (data == '0')	PORTD = 0xff;			//LEDs aus
}

void slave_init (void) {
	DDRB |= (1<<PB4);							//MISO als Ausgang, der Rest als Eingang
	SPCR = (1<<SPE) | (1<<SPIE);				//Aktivierung des SPI + Interrupt
	status = SPSR;								//Status löschen
}

int main (void) {

	DDRD = 0xff;		// PortD als Ausgang nutzen (Chris & Ollo geraten)
	slave_init ();
	sei ();
	
	for (;;);
	return 0;
}
