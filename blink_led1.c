#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/cpufunc.h>
#include <string.h>


#define F_CPU_david 16000000
#define BAUD 9600
#define BRC ((F_CPU_david/16/BAUD)-1)
#define TX_BUFFER_SIZE 128

// Use stty -F /dev/ttyUSB0 -echo -icanon 0 min 1?

volatile char serialBuffer[TX_BUFFER_SIZE];		// Added volatile here bc I saw it on a forum post 
volatile uint8_t serialReadPos = 0; // global variables don't need to be set to 0
volatile uint8_t serialWritePos = 0;

uint16_t timerFrequencyGlobal = 65000; // I think i need to make this volatile 

uint8_t adc_read();
void serialWrite(char c[]);

void itoa(int n, char *s) //avr stdlib.h gives itoa
{
	int i, sign;
	if ((sign = n) < 0)
		n = -n;
	i=0;
	do {
		s[i++] = n % 10 + '0';
	} while ((n /= 10) > 0);
	if (sign < 0)
		s[i++] = '-';
	//s[i] = '\0';

	//char temp[4];
	//temp[3] = s[0];
	//temp[2] = s[1];
	//temp[1] = s[2];
	//temp[0] = s[3];

	//*s = temp; //can't assign four things at the same time 
}

void port_init()
{
	DDRB |= _BV(PORTB5);
}

void timer1_init()
{
	TCCR1B |= (1 << WGM12)|(1 << CS12)|(1 << CS10); // Set up timer with prescaler = 256 (1024) and CTC mode
	TCNT1 = 0; 					// Initialize compare value
	OCR1A = 65000;					// Enable compare value with this 16 bit register 
	TIMSK1 |= (1 << OCIE1A);			// Enable global interrupts
	sei();
}

void ADC_init()
{
	DDRC = 0b00000000;				// Configure PORT C bit 0 to an input
	ADMUX = 0b01100000;				// Configure ADC to be left justified (this is a 12 bit ADC left justified to be 8 bits)
							// ...Use AVCC as reference, and select ADC0 and ADC input
	ADCSRA = 0b10000111;				// Enable the ADC and set the prescaler to max value (128)
	sei();
}

void UART_init()
{
	// need to disable interupts 
	UBRR0H = (BRC >> 8);
	UBRR0L = BRC;	
	UCSR0B = (1 << TXEN0) | (1 << TXCIE0);	
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
	sei();
}

void appendSerial(char c) //two different behaviors depending on whether this is the first character or not also the name isn't accurate "put_char" might be better
{
	cli();
	if(UCSR0A & (1 << UDRE0))
	{
		UDR0 = c; 
		return;
	}

	if ((serialWritePos + 1) % TX_BUFFER_SIZE == serialReadPos)
	{
		return;
	}

	serialBuffer[serialWritePos] = c;
	serialWritePos++;

	if(serialWritePos >= TX_BUFFER_SIZE)
	{
		serialWritePos = 0;
	}
	sei();
}

void serialWrite(char *c)
{
	//cli();
	for(uint8_t i = 0; i < strlen(c); i++)
	{
		appendSerial(c[i]); //*(c+i)
	}
	//sei();
}

ISR(USART_TX_vect)
{
	if(serialReadPos != serialWritePos)
	{
		UDR0 = serialBuffer[serialReadPos];
		serialReadPos++;
			
		if(serialReadPos >= TX_BUFFER_SIZE)
		{
			serialReadPos = 0; // this was a bug bc ++
		}
	}
}

ISR(TIMER1_COMPA_vect)
{
	PORTB ^= (1 << 5);
	OCR1A = timerFrequencyGlobal; 
}

void blink_speak()
{
	PORTB ^= (1 << 5);
	for (int32_t i=0; i< 1000000; i++)
	{
		_NOP();
	}
	char *my_char = "abc123\n"; // string in double quotes is a constant can't change
	//int i = sizeof my_char; //i will be 100*1 (1 is the size of a character)
	//my_char[0] = '\n'; 
	//uint8_t my_int = 128; //adc_read();
	//itoa(my_int,my_char_2,size);
	
	serialWrite(my_char);
	//serialWrite("\n");
	//UDR0 = '\n';
	//char *my_char = "\n";i
	//using double quotes gives you a string and that will be null terminated single quotes gives a character 
}

void blink()
{
	PORTB ^= (1 << 5);
	for (uint32_t i=0; i < 1000000; i++)
	{
		_NOP();
	}
}

uint8_t linearize(uint8_t value)
{
	int temp = value;
	if (temp > 127)
	{
		temp = 127;
	}
	temp = temp + 64;
	value = temp; 
	return value;
}

void blink_adc()
{
	PORTB ^= (1 << 5);
		
	ADCSRA = ADCSRA | (1 << ADSC);
	while(ADCSRA & (1 << ADSC));

	
for (int i=0; i < (256 - linearize(ADCH)); i++)
	{	
		for (int j=0; j < 12500; j++)
		{
			_NOP();
		}
 	}
}

void adc_read_threshold()
{
	uint8_t	TRIGPOINT = 128;
	ADCSRA = ADCSRA | (1 << ADSC);
		
	// Wait until the ADSC bit has been cleared
	while(ADCSRA & (1 << ADSC));

	if(ADCH < TRIGPOINT)
	{
		// Turn LED on
		PORTB = PORTB | (1 << 5);
	}
	else
	{
		// Turn LED off
		PORTB = PORTB & ~(1 << 5);
	}
}

uint8_t adc_read()
{
	ADCSRA = ADCSRA | (1 << ADSC);		// Start and ADC conversion by setting ADSC bit (bit 6)
	while(ADCSRA & (1 << ADSC));		// Wait until the ADSC bit has been cleared (this is bitwise &)
	return ADCH;
}

void timer_set(uint8_t setpoint)
{
	OCR1A = setpoint << 6;
	if (TCNT1 > OCR1A)
	{
		TCNT1 = 0;	
	}
}

void main (void) 
{
	port_init(); 
	ADC_init();
	UART_init();	
	//timer1_init();
	while(1)
	{
		blink_speak();
	}	
}


