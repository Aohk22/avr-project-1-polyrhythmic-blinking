make: main.c
	avr-gcc -DF_CPU=16000000UL -mmcu=atmega328p -Wall -Os -c -o build/main.o main.c
	avr-gcc -mmcu=atmega328p build/main.o -o file

hex: file
	avr-objcopy -O ihex -R .eeprom file file.hex

upload:
	avrdude -F -V -c arduino -p ATMEGA328P -P /dev/ttyACM0 -b 115200 -U flash:w:file.hex

clean:
	rm file file.hex

all:
	avr-gcc -DF_CPU=16000000UL -mmcu=atmega328p -Wall -Os -c -o build/main.o main.c
	avr-gcc -mmcu=atmega328p build/main.o -o file
	avr-objcopy -O ihex -R .eeprom file file.hex
	avrdude -F -V -c arduino -p ATMEGA328P -P /dev/ttyACM0 -b 115200 -U flash:w:file.hex
