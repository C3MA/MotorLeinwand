/**
 * @file screenctrl.c
 * @brief Logic to control the motor inside the screen.
 * @author pcopfer
 * @author Ollo
 * @version 0.5
 *
 * The Motor is controlled via two pins. There are three possible states, that can be set with this two pins.
 * 
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>


/********** Board layout ***********/

#define PIN_MOTORUP	PC4	/* Motor Up (Use the LED of the evaluation board atm) */
#define PIN_MOTORDOWN	PC5	/* Motor Down (Use hopefully the other LED on the board) */
#define PIN_DEBUG_LED	PC3	/* Debug LED */
#define PORT_MOTOR	PORTC	/**< Port on the Atmega, used by the motor pins */
#define	DDCTRL_MOTOR	DDRC	/**< Data direction control register of the Motor pins */

#define PIN_CTRL_UP	PB5
#define PIN_CTRL_DOWN	PB4
#define PIN_CTRL_STOP	PB3
#define PIN_CTRL	PINB
#define PORT_CTRL	PORTB

/********** Board layout end ********/

#define STATE_UP		1	/**< Motor is pulling the screen up */
#define STATE_DOWN		2	/**< Motor is releasing the screen down */
#define STATE_STOP		3	/**< Motor is stopped */
#define STATE_NONE		4	/**< Beginning of the fsm or signal undecodable */

#define	POSTSCALER_MAXIMUM	11	/**< Maximum, that triggers the timeout */

volatile unsigned char lastState = 0;	/**< Last executed command */
volatile unsigned char postscaler = 0;	/**< Make the timing slower */

/** @fn controlStates
 * @brief Interruped for SPI
 * New received data on SPI is parsed here.
 * So the main logic to control the motor is here.
 */
void handleState(int pinUp, int pinDown, int pinStop)
{
	int i;
	unsigned char data = STATE_NONE;

	/* decode pin states */
	if (pinUp == 0 && pinDown > 0)
	{
		data = STATE_UP;
	}
	else if (pinUp > 0 && pinDown == 0)
	{
		data = STATE_DOWN;
	}
	else if (pinStop == 0)
	{
		data = STATE_STOP;
	}

	/* Reset the timer, if the direction was changed */
	if (lastState != data)
	{
		TCNT1 = 0;                	// Reset the Timer
	}

	switch(data)
	{
	case STATE_UP:
		PORT_MOTOR &= ~(1<<PIN_MOTORDOWN);	/* deactivate down */
		PORT_MOTOR |= (1<<PIN_MOTORUP);		/* activate up */

		/* generate Blink code */
		for(i=0; i < 4; i++)
		{
			PORT_MOTOR |= (1<<PIN_DEBUG_LED);	/* activate LED */
			_delay_ms(50);
			PORT_MOTOR &= ~(1<<PIN_DEBUG_LED);	/* deactivate LED */
			_delay_ms(50);
		}

		break;
	case STATE_DOWN:
		PORT_MOTOR |= (1<<PIN_MOTORDOWN);	/* activate down */
		PORT_MOTOR &= ~(1<<PIN_MOTORUP);	/* deactivate up */

		/* generate Blink code */
		for(i=0; i < 2; i++)
		{
			PORT_MOTOR |= (1<<PIN_DEBUG_LED);	/* activate LED */
			_delay_ms(200);
			PORT_MOTOR &= ~(1<<PIN_DEBUG_LED);	/* deactivate LED */
			_delay_ms(200);
		}
		break;
	case STATE_STOP:
		PORT_MOTOR &= ~(1<<PIN_MOTORDOWN);	/* deactivate down */
		PORT_MOTOR &= ~(1<<PIN_MOTORUP);	/* deactivate up */

		/* generate Blink code */
		for(i=0; i < 1; i++)
		{
			PORT_MOTOR |= (1<<PIN_DEBUG_LED);	/* activate LED */
			_delay_ms(500);
			PORT_MOTOR &= ~(1<<PIN_DEBUG_LED);	/* deactivate LED */
			_delay_ms(100);
		}
		break;
	default:
		/* No changes to be done in the fsm */
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
		PORT_MOTOR &= ~(1<<PIN_MOTORDOWN);	/* deactivate down */
		PORT_MOTOR &= ~(1<<PIN_MOTORUP);	/* deactivate up */
		lastState = STATE_STOP;			/* update the fsm */		

		/* generate Blink code */
		{
			PORT_MOTOR |= (1<<PIN_DEBUG_LED);	/* activate LED */
			_delay_ms(500);
			PORT_MOTOR &= ~(1<<PIN_DEBUG_LED);	/* deactivate LED */
			_delay_ms(100);
		}
		/* reset post scaler */
		postscaler = 0;
	}
	else
	{
		postscaler++;
	}
}

/** @fn void timer_init (void)
 * @brief activiation of the timer.
 * The timer is necessary to trigger a timeout, after which always the motor is stopped.
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
	DDCTRL_MOTOR = 0;
	
	/* Prepare the necessary PINs as outputs */
	DDCTRL_MOTOR |= (1<<PIN_MOTORUP);
	DDCTRL_MOTOR |= (1<<PIN_MOTORDOWN);
	DDCTRL_MOTOR |= (1<<PIN_DEBUG_LED);
	
	/* set pull up for inputs */
	PORT_CTRL |= (1 << PIN_CTRL_UP);
	PORT_CTRL |= (1 << PIN_CTRL_DOWN);
	PORT_CTRL |= (1 << PIN_CTRL_STOP);

	timer_init();
	sei ();
	
	for (;;)
	{
		handleState(
			PIN_CTRL & (1 << PIN_CTRL_UP),
			PIN_CTRL & (1 << PIN_CTRL_DOWN),
			PIN_CTRL & (1 << PIN_CTRL_STOP) );

	}

	return 0;
}
