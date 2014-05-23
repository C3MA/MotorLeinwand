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
#include <util/delay.h>

#define PIN_MOTORUP	PC4	/* Motor Up (Use the LED of the evaluation board atm) */
#define PIN_MOTORDOWN	PC5	/* Motor Down (Use hopefully the other LED on the board) */
#define PIN_DEBUG_LED	PC3	/* Debug LED */

/* The SPI commands */
#define SPI_DOWN	'D'
#define SPI_UP		'U'
#define SPI_STOP	'S'


#define	POSTSCALER_MAXIMUM	11	/**< Maximum, that triggers the timeout */

volatile unsigned char data;		/**< Actual received command via SPI */
volatile unsigned char lastState = 0;	/**< Last executed command */
volatile unsigned char postscaler = 0;	/**< Make the timing slower */
unsigned char status;			/*FIXME old variable from the example */

/** @fn ISR (SPI_STC_vect)
 * @brief Interruped for SPI
 * New received data on SPI is parsed here.
 * So the main logic to control the motor is here.
 */
ISR (SPI_STC_vect)
{
	int i;
	data = SPDR;
	
	/* Reset the timer, if the direction was changed */
	if (lastState != data)
	{
		TCNT1 = 0;                	// Reset the Timer
	}

	switch(data)
	{
	case SPI_UP:
		PORTC &= ~(1<<PIN_MOTORDOWN);	/* deactivate down */
		PORTC |= (1<<PIN_MOTORUP);	/* activate up */

		/* generate Blink code */
		for(i=0; i < 3; i++)
		{
			PORTC |= (1<<PIN_DEBUG_LED);	/* activate LED */
			_delay_ms(600);
			PORTC &= ~(1<<PIN_DEBUG_LED);	/* deactivate LED */
			_delay_ms(600);
		}

		break;
	case SPI_DOWN:
		PORTC |= (1<<PIN_MOTORDOWN);	/* activate down */
		PORTC &= ~(1<<PIN_MOTORUP);	/* deactivate up */

		/* generate Blink code */
		for(i=0; i < 2; i++)
		{
			PORTC |= (1<<PIN_DEBUG_LED);	/* activate LED */
			_delay_ms(200);
			PORTC &= ~(1<<PIN_DEBUG_LED);	/* deactivate LED */
			_delay_ms(200);
		}
		break;
	case SPI_STOP:
	default:
		PORTC &= ~(1<<PIN_MOTORDOWN);	/* deactivate down */
		PORTC &= ~(1<<PIN_MOTORUP);	/* deactivate up */
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

	if (postscaler > POSTSCALER_MAXIMUM)
	{ 
		/* The motor has rolled out (or up) the complete screen.
		 * So switch into the "Stop-State"
		 */
		PORTC &= ~(1<<PIN_MOTORDOWN);	/* deactivate down */
		PORTC &= ~(1<<PIN_MOTORUP);	/* deactivate up */
		
		/* reset post scaler */
		postscaler = 0;
	}
	else
	{
		postscaler++;
	}
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
	TCCR1B = (1<<CS10) | (1<<CS12);	// 8MHz/65536/1024 = 0,11Hz --> 8,38s
}


/** @fn int main (void)
 * @brief main entry point
 * Initializing the hardware IO, the timer and the SPI logic.
 */
int main (void)
{
	DDRC = 0;
	
	/* Prepare the necessary PINs as outputs */
	DDRC |= (1<<PIN_MOTORUP);
	DDRC |= (1<<PIN_MOTORDOWN);
	DDRC |= (1<<PIN_DEBUG_LED);
	slave_init ();
	timer_init();
	sei ();
	
	for (;;)
	{
		PORTC |= (1<<PIN_DEBUG_LED);	/* activate LED */
		_delay_ms(50);
		PORTC &= ~(1<<PIN_DEBUG_LED);	/* deactivate LED */
		_delay_ms(50);
	}

	return 0;
}
