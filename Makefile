all: blink_led1

blink_led1: blink_led1.c Makefile
	avr-gcc -mmcu=atmega328p -o blink_led1 blink_led1.c -Os

blink_led1.hex: blink_led1
	avr-objcopy -O ihex -R .eeprom blink_led1 blink_led1.hex

asm: blink_led1
	avr-objdump -S blink_led1 
