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
	if (pinStop == 0)
	{
		data = STATE_STOP;
	}
	else if (pinUp == 0 && pinDown > 0)
	{
		data = STATE_UP;
	}
	else if (pinUp > 0 && pinDown == 0)
	{
		data = STATE_DOWN;
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


/**
 *****************************************************************************
 * @brief USART used from http://maxembedded.com/2013/09/the-usart-of-the-avr/ */

// define some macros
#define BAUD 9600                                   // define baud
#define BAUDRATE ((F_CPU)/(BAUD*16UL)-1)            // set baud rate value for UBRR
  
// function to initialize UART
void uart_init (void)
{
    UBRRH = (BAUDRATE>>8);                      // shift the register right by 8 bits
    UBRRL = BAUDRATE;                           // set baud rate
    UCSRB|= (1<<TXEN)|(1<<RXEN);                // enable receiver and transmitter
    UCSRC|= (1<<URSEL)|(1<<UCSZ0)|(1<<UCSZ1);   // 8bit data format
}

// function to send data
void uart_transmit (unsigned char data)
{
    while (!( UCSRA & (1<<UDRE)));                // wait while register is free
    UDR = data;                                   // load data in the register
}

/** @fn  void uart_write(unsigned char *string)
 * @brief send a string over uart
 * @param string    text to send, expected to be closed with \0 to mark end
 */
void uart_write(char *string)
{
    unsigned char* c= (unsigned char *)string;
    while ((*c) != '\0' )
    {
        uart_transmit(*c);
        c++;
    }
}

#define printf(txt)  uart_write(txt "\r\n\0")

/** @fn int main (void)
 * @brief main entry point
 * Initializing the hardware IO, the timer and the SPI logic.
 */
int main (void)
{
    int btnUp      = 0;
    int btnDown    = 0;
    int btnStop    = 0;
    int oldBtnUp   = 0;
    int oldBtnDown = 0;
    int oldBtnStop = 0;

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
    uart_init();
    printf("Setup complete");
	
	for (;;)
	{
        /* Retrieve the actual button states */
	    btnUp   = PIN_CTRL & (1 << PIN_CTRL_UP);
		btnDown = PIN_CTRL & (1 << PIN_CTRL_DOWN);
		btnStop = PIN_CTRL & (1 << PIN_CTRL_STOP);

        /* Handle the buttons if they are stable for two cycles */
        if ((btnUp == oldBtnUp) && (btnDown == oldBtnDown) && (btnStop == oldBtnStop))
        {
		    handleState(btnUp, btnDown, btnStop);
        }

        /* delay main loop, in order to ignore EVM problems */
        PORTC |= (1<<PIN_DEBUG_LED);	/* activate LED */
		_delay_ms(50);
		PORTC &= ~(1<<PIN_DEBUG_LED);	/* deactivate LED */
		_delay_ms(50);
        


        /* store the actual state of the buttons */
        oldBtnUp = btnUp;
        oldBtnDown = btnDown;
        oldBtnStop = btnStop;
	}

	return 0;
}
