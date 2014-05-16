/**
 * @file spi_slave.c
 * @brief Logic to control the motor inside the screen.
 * @author pcopfer
 * @author Ollo
 * @version 0.2
 *
 * The Motor is controlled via SPI. There are three possible commands.
 * 'U' for up, 'D' for down and 'S' to stop the screen.
LEDs an bzw. aus gehen
Verdrahtung:	MISO(Master) --> MISO(Slave)
				MOSI(Master) --> MOSI(Slave)
				SCK(Master)  --> SCK(Slave)
				PB0(Master)	 --> SS(Slave)
**************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>

#define PIN_MOTORUP	PD5	/* Motor Up (Use the LED of the evaluation board atm) */
#define PIN_MOTORDOWN	PD6	/* Motor Down (Use hopefully the other LED on the board) */

/* The SPI commands */
#define SPI_DOWN	'D'
#define SPI_UP		'U'
#define SPI_STOP	'S'

volatile unsigned char data;		/**< Actual received command via SPI */
volatile unsigned char lastState = 0;	/**< Last executed command */
unsigned char status;			/*FIXME old variable from the example */

/** @fn ISR (SPI_STC_vect)
 * @brief Interruped for SPI
 * New received data on SPI is parsed here.
 * So the main logic to control the motor is here.
 */
ISR (SPI_STC_vect)
{
	data = SPDR;

	/* Reset the timer, if the direction was changed */
	if (lastState != data)
	{
		TCNT1 = 0;                	// Reset the Timer
	}

	switch(data)
	{
	case SPI_UP:
		PORTD &= ~(1<<PIN_MOTORDOWN);	/* deactivate down */
		PORTD |= (1<<PIN_MOTORUP);	/* activate up */
		break;
	case SPI_DOWN:
		PORTD |= (1<<PIN_MOTORDOWN);	/* activate down */
		PORTD &= ~(1<<PIN_MOTORUP);	/* deactivate up */
		break;
	case SPI_STOP:
	default:
		PORTD &= ~(1<<PIN_MOTORDOWN);	/* deactivate down */
		PORTD &= ~(1<<PIN_MOTORUP);	/* deactivate up */
		break;
	}
	
	/* remember the last state, to detect changes */
	lastState = data;
}

/** @fn ISR (TIMER1_OVF_vect)
 * @brief Overflow interrupt of the timer
 * Logic to stop the motor (release the relais) after a timeout
 */
ISR (TIMER1_OVF_vect)
{
	/* The motor has rolled out (or up) the complete screen.
	 * So switch into the "Stop-State"
	 */
	PORTD &= ~(1<<PIN_MOTORDOWN);	/* deactivate down */
	PORTD &= ~(1<<PIN_MOTORUP);	/* deactivate up */
}

/** @fn void slave_init (void)
 * @brief intialize the SPI slave
 */
void slave_init (void)
{
	DDRB |= (1<<PB4);            //MISO als Ausgang, der Rest als Eingang
	SPCR = (1<<SPE) | (1<<SPIE); //Aktivierung des SPI + Interrupt
}

/** @fn void timer_init (void)
 * @brief activiation of the timer.
 */
void timer_init (void)
{
	TIMSK |= (1<<TOIE1);		// Timer Overflow Interrupt enable
	TCNT1 = 0;                	// Rücksetzen des Timers
	TCCR1B = (1<<CS10) | (1<<CS11);	// 8MHz/65536/64 = 1,91Hz --> 0,5s FIXME this ist TOOO fast
}

/** @fn int main (void)
 * @brief main entry point
 * Initializing the hardware IO, the timer and the SPI logic.
 */
int main (void)
{
	DDRD = 0;
	/* Prepare the necessary PINs as outputs */
	DDRD |= (1<<PIN_MOTORUP);
	DDRD |= (1<<PIN_MOTORDOWN);
	slave_init ();
	timer_init();
	sei ();
	
	for (;;);
	return 0;
}
