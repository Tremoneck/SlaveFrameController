TARGET=LEDFrameController
MCU=attiny85
FCPU=16000000UL
SOURCES=main.c usiTwiSlave.c

PROGRAMMER=arduino
#auskommentieren für automatische Wahl
PORT=-PCOM5
BAUD=-b19200

#Ab hier nichts verändern
OBJECTS=$(SOURCES:.c=.o)
CFLAGS=-c -Os -Wint-to-pointer-cast -DF_CPU=16000000 -g
LDFLAGS=

all: hex eeprom

hex: $(TARGET).hex
	
eeprom: $(TARGET)_eeprom.hex

$(TARGET).hex: $(TARGET).elf
	avr-objcopy -O ihex -j .data -j .text $(TARGET).elf $(TARGET).hex

$(TARGET)_eeprom.hex: $(TARGET).elf
	avr-objcopy -O ihex -j .eeprom --change-section-lma .eeprom=1 $(TARGET).elf $(TARGET)_eeprom.hex

$(TARGET).elf: $(OBJECTS)
	avr-gcc $(LDFLAGS) -mmcu=$(MCU) $(OBJECTS) -o $(TARGET).elf

.c.o:
	avr-gcc $(CFLAGS) -mmcu=$(MCU) $< -o $@

size: hex eeprom
#	avr-size --mcu=$(MCU) -C $(TARGET).elf
	((Get-Item $(TARGET).hex).length)
	(Get-Item $(TARGET)_eeprom.hex).length

program: hex
	avrdude -p$(MCU) -c$(PROGRAMMER) $(PORT) $(BAUD) -Uflash:w:$(TARGET).hex:a

setfuses:
	avrdude -p$(MCU) $(PORT) $(BAUD) -c$(PROGRAMMER) -U lfuse:w:0xF1:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m

read:
	avrdude -p$(MCU) -c$(PROGRAMMER) $(PORT) $(BAUD)

disass: $(TARGET).elf
	avr-objdump.exe -d -S -M intel $(TARGET).elf > $(TARGET)_dump.txt

eeprom_read:
	avrdude -p$(MCU) -c$(PROGRAMMER) $(PORT) $(BAUD) -U eeprom:r:eeprom.hex:i

clean_tmp:
	rm *.o
	rm *.elf

clean:
	rm *.o
	rm *.elf
	rm *.hex