#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "main.h"
#include "usiTwiSlave.h"


volatile char positionErkannt = 0;
volatile char i2cmode = 0;
volatile char i2cPos = 0;

const static uint8_t c_LedAnzahl = 254;
const static uint8_t LEDPIN = 4;
//PIN 1 RESET VCC
//PIN 3 LED Strip
//PIN 5 SDA
//PIN 7 SCL

/* EEPROM Layout
	0x10 EEPROM eingestellt
	2 I2C Adresse
	3-6 Farbe
*/

/*
	I2C Positionen
	'm' lese Modus setzten 1 byte von den unten
	'a' adresse 2 byte erstes ist 0xde
	'f' Farbe 4 byte
	's' schlaf enable 1 byte
	'b' Bus erkannt flag setzten 0
	'r' The gesetzte Farbe wieder aufschreiben, Immer mit submode
*/
void main(){

	//PORTB &= ~_BV(4); //Pullups auschalten


	setup();
	uint8_t maske = _BV(3);
	DDRB |= maske; // InfoLed Pin ausgang
	uint8_t farbe[4];
	for(uint8_t i = 0; i < 4;i++){
		farbe[i] = eeprom_read_byte((uint8_t*)(i+3));
	}
	zeichneFarbe(c_LedAnzahl, farbe);
	DDRB &= ~_BV(LEDPIN); // LEDPin auf Input schalten
	PORTB &= ~_BV(LEDPIN); //Pullups auschalten
	usi_onRequestPtr = onRequest;
	usi_onReceiverPtr = onReceive;
	sleep_enable(); // Wir sind fertig und können uns schlafen legen
	sleep_cpu(); //Direkt schlafen gehen
	while(1){
		PORTB ^= maske;
		_delay_ms(200);
	}
}

void onReceive(uint8_t count){

	uint8_t maske = _BV(3);

	i2cmode = usiTwiReceiveByte(); //Neuen Modus setzten
	
	i2cPos = 0;
	if((--count) == 0) return; //Nur der neue Modus wurde gesetzt
	switch (i2cmode)
	{
	case 'a': //adresse neu konfigurieren
		if(usiTwiReceiveByte() != 0xde) // Make sure not every silly stray command may change the adresse
			break;
		eeprom_update_byte((uint8_t*)2, usiTwiReceiveByte());
		usiTwiSlaveInit(eeprom_read_byte((uint8_t*)2));
		break;
	case 's': //Schlafmodus setzten
		if(usiTwiReceiveByte()){
			sleep_enable();
		} else {
			sleep_disable();
		}
		break;
	case 'f': //Farbe neukonfigurieren
		if(count == 4)
			for(uint8_t i = 0; i < 4;i++){
		   		eeprom_update_byte((uint8_t*)(i+3), usiTwiReceiveByte());
			}
		break;
	case 'b': //Setzten ob es schon auf dem Bus lag
		if (usiTwiReceiveByte())
			positionErkannt = 1;
		else
			positionErkannt = 0;
		break;
	case 'r': //Farben neu malen bei 1 und 5 ist das erste byte die länge 4 Byte Farb info
		{
			PORTB ^= maske;
			uint8_t farbe[4];
			uint8_t anzahl = c_LedAnzahl;

			if(count == 1 || count == 5){
				anzahl = usiTwiReceiveByte();	
			}
			count--;
			for(uint8_t i = 0; i < 4;i++){
				if(count != 4){
					farbe[i] = eeprom_read_byte((uint8_t*)(i+3));
				}else {
					farbe[i] = usiTwiReceiveByte();
				}
			}
			count -= 4;
			DDRB |= _BV(LEDPIN); //LED Pin auf Ausgang schalten
			zeichneFarbe(anzahl, farbe);
			DDRB &= ~_BV(LEDPIN); //LED Pin wieder auf Eingang schalten
		}
		break;
	}
}



void onRequest(){

	switch (i2cmode)
	{
		case 'a':
			usiTwiTransmitByte(eeprom_read_byte((uint8_t*)2));
		break;
		case 's':
		case 'r':
			break;
		case 'f':
			usiTwiTransmitByte(eeprom_read_byte((uint8_t*)((i2cPos++) +3)));
			if(i2cPos == 4) 
				i2cPos = 0;
			break;
		case 'b':
		   usiTwiTransmitByte(positionErkannt);
		break;
	}
}

void inline setup(){

	set_sleep_mode(SLEEP_MODE_PWR_DOWN); //Wenn wir schlafen dann richtig

	//configure Interupt on Pin 3
	MCUCR |= 3; //Trigger on a rising edge
	GIMSK = 0x20; // PIN Change
	PCMSK = _BV(4); //PIN 3 aktivieren

	DDRB |= _BV(PINB4); //Output einschalten für LEDS
	PORTB = 0xff & ~ _BV(PINB4); //LED Output anschalten

	if(eeprom_read_byte((uint8_t*) 0x1) != 1){ // Die Standart werte herstellen, da der EEPROM überschrieben wurde
		eeprom_update_byte((uint8_t*)2, 0x30); //I2C Adresse ist 0x30 standart
		eeprom_update_byte((uint8_t*)3, 0x0);  //Rot
		eeprom_update_byte((uint8_t*)4, 0x0);  //Grün
		eeprom_update_byte((uint8_t*)5, 0x0);  //Blau
		eeprom_update_byte((uint8_t*)6, 0xff); //Weiß
		eeprom_update_byte((uint8_t*)0x1, 1); // Default hergestellt
	}
	usiTwiSlaveInit(eeprom_read_byte((uint8_t*)2)); //TWI einschalten
	sei();
}



ISR(SIG_PIN_CHANGE){ //Erkennen ob PIN 3 Verändert wurde
	positionErkannt = 1; //Auf dem BUS wurde veränderung erkannt
	PCMSK = 0x00; //Interupt auf PIN 3 ausschalten damit wir nicht in jeder animation aufwachen
	sei();
	sleep_cpu(); //CPU schlafen legen, wenn im Schlafmodus
	cli();
	PCMSK = _BV(LEDPIN); //Reaktivieren, damit einschlafen funktioniert
}

void zeichneFarbe(uint8_t anzahl, uint8_t* farbe){

	uint8_t bit  = 8;
	uint8_t index = 4;
	uint8_t* start = farbe;
	uint8_t cByte = *farbe++;
	cli(); //Interrupts ausschalten

	/* VOn Hand getunter Assembler. Taktung mit ein (T = 2), ggf aus (T = 7) und aus (T = 15) muss gewart werden dammit das Programm funktioniert
		Der sprunghafte Code kommt aus Zeitgründen, da wir viel zu erledigen haben wenn wir zur nächsten LED springen
		Es sind 3 schleifen vorhanden, die erste zählt jedes Bit runter und wird immer auf 8 zurück gesetzt.
		Die 2. zählt von 4 runter um zu wissen wann wir alles zurück setzten müssen, für die nächste LED
		Die 3. zählt die anzahl an LED runter
	*/
	asm volatile(
	"head20:"                   "\n\t"      // Clk  Pseudocode           (T =  0)
		"sbi 0x18, 4"           "\n\t"      // 2 PORTB PIN 4 an          (T = 2)
		"nop"                   "\n\t"      // 1     nop               (T = 3)
		"dec %[bit]"            "\n\t"      // 1     bit--               (T = 4)
		"sbrs %[cByte], 7"        "\n\t"      // 1     Ignorieren falls an (T = 5)
		"cbi 0x18, 4"           "\n\t"      // 2     PORTB PIN 4 aus     (T = 7)
		"brne sameByte20"       "\n\t"      // 1-2   if(bit != 0) (from dec above)(T = 8)
		"ldi %[bit], 8"         "\n\t"      // 1 bit = 8                 (T = 9)
		"subi %[index], 1"      "\n\t"      // 1 index--                 (T = 10)
		"brne sameLED20"        "\n\t"      // 1-2 index != 0 same Led   (T = 11)
		"movw %[ptr], %[start]" "\n\t"      // 1 Pointer zurücksetzten   (T = 12)
		"ldi %[index], 4"       "\n\t"      // 1 index wieder auf 4 zurück setzen (T = 13)
		"cbi 0x18, 4"           "\n\t"      // 2 PORTB PIN 4 aus         (T = 15)
		"ld %[cByte], %a[ptr]+" "\n\t"      // 2                         (T = 17)
		"subi %[count],1 "      "\n\t"      // 1    i--                  (T = 18)
		"brne head20"           "\n\t"      // 1-2  i != 0 ==> weiter
		"rjmp ende20"           "\n\t"      //ende kann für Platzersparnisse optimiert werden
	"sameByte20:"
		"rjmp .+0"               "\n\t"      // 2     nop nop             (T = 11)
		"rjmp .+0"               "\n\t"      // 2     nop nop             (T = 13)
		"cbi 0x18, 4"           "\n\t"      // 2     PORTB PIN 4 aus     (T = 15)
		"rol %[cByte]"          "\n\t"      // 1     b <<= 1             (T = 16)
		"rjmp .+0"               "\n\t"      // 2     nop nop             (T = 18)
		"rjmp head20"           "\n\t"      // 2    -> head20            (T = 20 / 0)
	"sameLED20:"                            // (T = 12)
		"cbi 0x18, 4"           "\n\t"      // 2 PORTB PIN 4 aus         (T = 15)
		"nop"                   "\n\t"      // 1     nop               (T = 16)
		"ld %[cByte], %a[ptr]+" "\n\t"      // 2                         (T = 18)
		"rjmp head20"           "\n"        // 2    if(i != 0) -> (next byte)
	"ende20:"
		://Output
		[cByte] "+r"  (cByte),
		[bit] "+r" (bit),
		[count] "+r" (anzahl),
		[index] "+r" (index),
		[ptr] "+e" (farbe)
		://Input
		[start] "w" (start)
		);
	sei(); //INterrupts wieder einschalten


}

void showByte(uint8_t b){
	uint8_t maske = _BV(3);
	for(int i = 0; i < 8; i++){
		
		PORTB |= maske;
		_delay_ms(25);
		PORTB &= ~maske;
		_delay_ms(25);
		if((b >> (7-i)) & 1)
	  		PORTB |= maske;
		else
			PORTB &= ~maske;
		_delay_ms(100);
	}

}