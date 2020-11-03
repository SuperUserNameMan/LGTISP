// 20201102 :
// - google-translate chinese comments into english
// - add instructions for compiling to ATmega328p boards
// - code cleanup
// - implemented flash byte reading into universal() which is used by 
//   the "dump flash" command of AVRdude in terminal mode.
// 
// 20 July 2020 David Buezas
// - Bundled and added menu utility
// * When uploading to the programmer,
//   select in the menu: Tools/Arduino as ISP/SERIAL_RX_BUFFER_SIZE to 250)
// * Before using the ISP to program another board, 
//   connect (in the ISP board) the reset pin to gnd via a capacitor,
//   or (at your own risk) short reset to vcc.
// https://github.com/dbuezas/lgt8fx/
//
// author : brother_yan (https://github.com/brother-yan/LGTISP)
//
// LarduinoISP for LGT8FX8P series
// Project fork from
//    - ArduinoISP version 04m3
// Copyright (c) 2008-2011 Randall Bohn
// If you require a license, see 
//     http://www.opensource.org/licenses/bsd-license.php
//
// This sketch turns the Arduino into a AVRISP
// using the following arduino pins:
//
// pin name:    Arduino:          LGT8FX8P:
// slave reset: 10:               PC6/RESET 
// SWD:         12:               PE2/SWD
// SWC:         13:               PE0/SCK
// Make sure to 
// Put an LED (with resistor) on the following pins:
// 9: Heartbeat   - shows the programmer is running
// 8: Error       - Lights up if something goes wrong (use red if that makes sense)
// 7: Programming - In communication with the slave
//
// 23 July 2011 Randall Bohn
// -Address Arduino issue 509 :: Portability of ArduinoISP
// http://code.google.com/p/arduino/issues/detail?id=509
//
// October 2010 by Randall Bohn
// - Write to EEPROM > 256 bytes
// - Better use of LEDs:
// -- Flash LED_PMODE on each flash commit
// -- Flash LED_PMODE while writing EEPROM (both give visual feedback of writing progress)
// - Light LED_ERR whenever we hit a STK_NOSYNC. Turn it off when back in sync.
// - Use pins_arduino.h (should also work on Arduino Mega)
//
// October 2009 by David A. Mellis
// - Added support for the read signature command
// 
// February 2009 by Randall Bohn
// - Added support for writing to EEPROM (what took so long?)
// Windows users should consider WinAVR's avrdude instead of the
// avrdude included with Arduino software.
//
// January 2008 by Randall Bohn
// - Thanks to Amplificar for helping me with the STK500 protocol
// - The AVRISP/STK500 (mk I) protocol is used in the arduino bootloader
// - The SPI functions herein were developed for the AVR910_ARD programmer 
// - More information at http://code.google.com/p/mega-isp

// LarduinoISP for LGTF8FX8P Series
#include "swd_lgt8fx8p.h"

#if SERIAL_RX_BUFFER_SIZE < 250 // 64 bytes的RX缓冲不够大 // 64 bytes of RX buffer is not big enough
#error : Please change the macro SERIAL_RX_BUFFER_SIZE to 250 (In the menu: Tools/Arduino as ISP/SERIAL_RX_BUFFER_SIZE)
#error : If trying to compile for Atmega328p, use the standard Arduino AVR core, and temporarily add "#define SERIAL_RX_BUFFER_SIZE 256" into HardwareSerial.h
#endif

#define RESET	10
#define LED_HB    9
#define LED_ERR   8
#define LED_PMODE 7
#define PROG_FLICKER true

#define HWVER 3
#define SWMAJ 5
#define SWMIN 1

// STK Definitions
#define STK_OK      0x10
#define STK_FAILED  0x11
#define STK_UNKNOWN 0x12
#define STK_INSYNC  0x14
#define STK_NOSYNC  0x15
#define CRC_EOP     0x20 //ok it is a space...

void pulse(int pin, int times);

void setup() 
{
	SWD_init();
	Serial.begin(115200);

	//pinMode(LED_PMODE, OUTPUT);
	//pulse(LED_PMODE, 2);
	
	//pinMode(LED_ERR, OUTPUT);
	//pulse(LED_ERR, 2);
	
	//pinMode(LED_HB, OUTPUT);
	//pulse(LED_HB, 2);
}

uint8_t error=0;
uint8_t pmode=0;

// address for reading and writing, set by 'U' command
int address;
uint8_t buff[256]; // global block storage

#define beget16(addr) (*addr * 256 + *(addr+1) )
typedef struct param 
{
	uint8_t devicecode;
	uint8_t revision;
	uint8_t progtype;
	uint8_t parmode;
	uint8_t polling;
	uint8_t selftimed;
	uint8_t lockbytes;
	uint8_t fusebytes;
	uint8_t flashpoll;
	uint16_t eeprompoll;
	uint16_t pagesize;
	uint16_t eepromsize;
	uint32_t flashsize;
	
} parameter_t;

parameter_t param;

// this provides a heartbeat on pin 9, so you can tell the software is running.
uint8_t hbval=128;
uint8_t hbdelta=8;
void heartbeat() 
{
	if ( hbval > 192 )
	{
		hbdelta = -hbdelta;
	}
	
	if ( hbval <  32 )
	{
		hbdelta = -hbdelta;
	}
	
	hbval += hbdelta;
	
	analogWrite(LED_HB, hbval);
	
	delay(40);
}

void loop(void) 
{
	//~ // is pmode active?
	//~ digitalWrite( LED_PMODE, pmode ? HIGH : LOW ); 
	
	//~ // is taddress an error?
	//~ digitalWrite( LED_ERR, error ? HIGH : LOW ); 
	
	//~ // light the heartbeat LED
	//~ heartbeat();
	
	if ( Serial.available() )
	{
		avrisp();
	}
}

uint8_t getch() 
{
	while(!Serial.available());
	
	return Serial.read();
}

void fill(int n) 
{
	for (int x = 0; x < n; x++) 
	{
		buff[x] = getch();
	}
}

#define PTIME 30
void pulse(int pin, int times) 
{
	do 
	{
		digitalWrite(pin, HIGH);
		delay(PTIME);
		digitalWrite(pin, LOW);
		delay(PTIME);
	} 
	while (times--);
}

void prog_lamp(int state) 
{
	if ( PROG_FLICKER )
	{
		digitalWrite(LED_PMODE, state);
	}
}

void empty_reply() 
{
	if ( CRC_EOP == getch() ) 
	{
		Serial.print((char)STK_INSYNC);
		Serial.print((char)STK_OK);
	} 
	else 
	{
		error++;
		Serial.print((char)STK_NOSYNC);
	}
}

void breply(uint8_t b) 
{
	if ( CRC_EOP == getch() ) 
	{
		Serial.print((char)STK_INSYNC);
		Serial.print((char)b);
		Serial.print((char)STK_OK);
	} 
	else 
	{
		error++;
		Serial.print((char)STK_NOSYNC);
	}
}

void get_version(uint8_t c) 
{
	switch(c) 
	{
		case 0x80:
			breply(HWVER);
		break;
		
		case 0x81:
			breply(SWMAJ);
		break;
		
		case 0x82:
			breply(SWMIN);
		break;
		
		case 0x93:
			breply('S'); // serial programmer
		break;
		
		default:
			breply(0);
		break;
	}
}

void set_parameters() 
{
	// call this after reading paramter packet into buff[]
	param.devicecode = buff[0];
	param.revision   = buff[1];
	param.progtype   = buff[2];
	param.parmode    = buff[3];
	param.polling    = buff[4];
	param.selftimed  = buff[5];
	param.lockbytes  = buff[6];
	param.fusebytes  = buff[7];
	param.flashpoll  = buff[8]; 
	// ignore buff[9] (= buff[8])
	// following are 16 bits (big endian)
	param.eeprompoll = beget16(&buff[10]);
	param.pagesize   = beget16(&buff[12]);
	param.eepromsize = beget16(&buff[14]);
	
	// 32 bits flashsize (big endian)
	param.flashsize = 
		  buff[16] * 0x01000000
		+ buff[17] * 0x00010000
		+ buff[18] * 0x00000100
		+ buff[19]
		;
	
}

void start_pmode(uint8_t chip_erase) 
{
	digitalWrite(RESET, HIGH);
	pinMode(RESET, OUTPUT);
	delay(20);
	digitalWrite(RESET, LOW);
	
	SWD_init();
	SWD_Idle(10);
	
	pmode = SWD_UnLock(chip_erase);
	if ( ! pmode )
	{
		pmode = SWD_UnLock(chip_erase);
	}
}

void end_pmode()
{
	SWD_exit();
	pmode = 0;
	
	digitalWrite(RESET, HIGH);
	pinMode(RESET, INPUT);
}

void universal() 
{
	fill(4);
	
	if( buff[0] == 0x30 && buff[1] == 0x00 ) // signature
	{
		switch( buff[2] ) 
		{
			case 0x00:
				breply(0x1e);
			break;
			
			case 0x01:
				breply(0x95);
			break;
			
			case 0x02:
				breply(0x0f);
			break;
			
			default:
				breply(0xff);
			break;  
		}
	} 
	else 
	if( buff[0] == 0xf0 ) 
	{
		breply(0x00);
	} 
	else
	if ( buff[0] == 0x20 || buff[0] == 0x28 )
	{
		// Read one byte from flash memory.
		//!\ Atmega32 and AVRdude use addr for 16 bits data
		//!\ LGT8Fx8p needs addr for 32 bits data
		
		// 0x20 AVR_OP_READ_HI
		// 0x28 AVR_OP_READ_LO
		
		// buff[0] : 0x20-0x28
		// buff[1] : addr_hi >> 1
		// buff[2] : addr_lo >> 1
		// buff[3] : 0
		
		uint16_t addr = ( ( ( buff[1] << 8 ) | buff[2] ) << 1 ) + ( buff[0] == 0x28 ? 1 : 0 ); // 8bits data addr
		
		
		SWD_EEE_CSEQ(0x00, 0x01);
		uint32_t data = SWD_EEE_Read( addr / 4 ); // LGT8Fx8p uses addr for 32bits data
		SWD_EEE_CSEQ(0x00, 0x01);
		
		breply( ((uint8_t*)&data)[ addr & 0x3 ] );
	}
	else 
	{
		breply(0xff);
	}
}

void write_flash(int length) 
{
	fill(length);
	if ( CRC_EOP == getch() ) 
	{
		Serial.print((char) STK_INSYNC);
		Serial.print((char) write_flash_pages(length));
	}
	else 
	{
		error++;
		Serial.print((char) STK_NOSYNC);
	}
}

uint8_t write_flash_pages(int length)
{
	int addr = address / 2;
	/*
	 * lgt8fx8p的flash是按4字节编址的，而avr是按2字节编址的，
	 * avrdude传过来的是按2字节编址的address
	 * avrisp()函数中也有证实：
	 * 		case 'U': // set address (word)
	 */
	/*
	 * The flash of lgt8fx8p is addressed by 4 bytes, while avr is 
	 * addressed by 2 bytes, and the address passed by avrdude is by 2 bytes.
	 * There is also confirmation in the avrisp() function:
	 * 		case'U': // set address (word)
	 */
	
	SWD_EEE_CSEQ(0x00, addr);
	SWD_EEE_CSEQ(0x84, addr);
	SWD_EEE_CSEQ(0x86, addr);
	
	for (int i = 0; i < length; i += 4)
	{
		SWD_EEE_Write(*((uint32_t *)(&buff[i])), addr);
		++addr;
	}
	
	SWD_EEE_CSEQ(0x82, addr - 1);
	SWD_EEE_CSEQ(0x80, addr - 1);
	SWD_EEE_CSEQ(0x00, addr - 1);
	
	return STK_OK;
}

#define EECHUNK (32)
uint8_t write_eeprom(int length) 
{
	// address is a word address, get the byte address
	int start = address * 2;
	int remaining = length;
	
	if (length > param.eepromsize) 
	{
		error++;
		return STK_FAILED;
	}
	
	while (remaining > EECHUNK) 
	{
		write_eeprom_chunk(start, EECHUNK);
		start += EECHUNK;
		remaining -= EECHUNK;
	}
	
	write_eeprom_chunk(start, remaining);
	
	return STK_OK;
}

// write (length) bytes, (start) is a byte address
uint8_t write_eeprom_chunk(int start, int length) 
{
	// this writes byte-by-byte,
	// page writing may be faster (4 bytes at a time)
	fill(length);
	prog_lamp(LOW);
	for (int x = 0; x < length; x++) 
	{
		int addr = start+x;
		// do e2prom program here
		// donothing for lgt8fx8d series
		delay(45); 
	}
	prog_lamp(HIGH); 
	return STK_OK;
}

void program_page() 
{
	char result = (char) STK_FAILED;
	
	// get length
	uint16_t length = getch() << 8;
	length += getch();
	
	char memtype = getch();
	
	// flash memory @address, (length) bytes
	if (memtype == 'F') 
	{
		write_flash(length);
		return;
	}
	
	if (memtype == 'E') 
	{
		result = (char)write_eeprom(length);
		if (CRC_EOP == getch()) 
		{
			Serial.print((char) STK_INSYNC);
			Serial.print(result);
		} 
		else 
		{
			error++;
			Serial.print((char) STK_NOSYNC);
		}
		return;
	}
	
	Serial.print((char)STK_FAILED);
	return;
}

char flash_read_page(int length)
{
	int addr = address / 2;
	/*
	 * lgt8fx8p的flash是按4字节编址的，而avr是按2字节编址的，
	 * avrdude传过来的是按2字节编址的address
	 * avrisp()函数中也有证实：
	 * 		case 'U': // set address (word)
	 */
	/*
	 * The flash of lgt8fx8p is addressed by 4 bytes, while avr is 
	 * addressed by 2 bytes, and the address passed by avrdude is by 2 bytes.
	 * There is also confirmation in the avrisp() function:
	 * 		case'U': // set address (word)
	 */
	
	SWD_EEE_CSEQ(0x00, 0x01);
	
	uint32_t data;
	for ( int i = 0; i < length; ++i )
	{
		if (i % 4 == 0)
		{
			data = SWD_EEE_Read(addr);
			++addr;
		}
		Serial.print((char)((uint8_t *)&data)[i % 4]);
	}
	
	SWD_EEE_CSEQ(0x00, 0x01);
	
	return STK_OK;
}

char eeprom_read_page(uint16_t length) 
{
	// address again we have a word address
	uint16_t start = address * 2;
	for (int x = 0; x < length; x++) 
	{
		uint16_t addr = start + x;
		// do ep2rom read here
		// but donothing for lgt8fx8d series (by now...)
		Serial.print((char) 0xff); // TODO
	}
	return STK_OK;
}

void read_page() 
{
	char result = (char)STK_FAILED;
	
	uint16_t length = getch() << 8;
	length += getch();

	char memtype = getch();
	
	if ( CRC_EOP != getch() ) 
	{
		error++;
		Serial.print((char) STK_NOSYNC);
		return;
	}
	
	Serial.print((char) STK_INSYNC);
	
	if ( memtype == 'F' ) 
	{
		result = flash_read_page(length);
	}
	else
	if ( memtype == 'E' )
	{
		result = eeprom_read_page(length);
	}
	
	Serial.print( result );
	
	return;
}

void read_signature() 
{
	if ( CRC_EOP != getch() ) 
	{
		error++;
		Serial.print((char) STK_NOSYNC);
		return;
	}
	
	Serial.print((char) STK_INSYNC);
	Serial.print((char) 0x1e);
	Serial.print((char) 0x95);
	Serial.print((char) 0x0a);
	Serial.print((char) STK_OK);
}

//////////////////////////////////////////
//////////////////////////////////////////


////////////////////////////////////
////////////////////////////////////

volatile uint8_t chip_erased;

int avrisp() 
{ 
	uint8_t data, low, high;
	uint8_t ch = getch();
	switch (ch) 
	{
		case 0x30 : // '0' Cmnd_STK_GET_SYNC
			error = 0;
			empty_reply();
		break;
		
		case 0x31 : // '1' Cmnd_STK_GET_SIGN_ON
			if ( getch() == CRC_EOP ) 
			{
				Serial.print((char) STK_INSYNC);
				Serial.print("AVR ISP");
				Serial.print((char) STK_OK);
			} 
			else 
			{
				error++;
				Serial.print((char) STK_NOSYNC);
			}
		break;
		
		// case 0x40 : // '@' Cmnd_STK_SET_PARAMETER
		
		case 0x41 : // 'A' Cmnd_STK_GET_PARAMETER
			get_version(getch());
		break;
		
		case 0x42 : // 'B' Cmnd_STK_SET_DEVICE, optional for lgt8fx8d series
			fill(20);
			set_parameters();
			empty_reply();
		break;
		
		case 0x45 : // 'E' Cmnd_SET_DEVICE_EXT
			// ignore for now
			fill(5);
			empty_reply();
		break;
		
		case 0x50 : // 'P' Cmnd_STK_ENTER_PROGMODE
			if (pmode) 
			{
				pulse(LED_ERR, 3);
			} 
			else 
			{
				start_pmode(0);
				chip_erased = 0;
			}
			
			if ( pmode )
			{
				empty_reply();
			}
			else
			{
				if ( CRC_EOP == getch() ) 
				{
					Serial.print((char)STK_INSYNC);
					Serial.print((char)STK_FAILED);
				} 
				else 
				{
					error++;
					Serial.print((char)STK_NOSYNC);
				}
			}
		break;
		
		case 0x51 : // 'Q' Cmnd_STK_LEAVE_PROGMODE
			error=0;
			end_pmode();
			empty_reply();
		break;
		
		// case 0x52 : // 'R' Cmnd_STK_CHIP_ERASE
		// case 0x53 : // 'S' Cmnd_STK_CHECK_AUTOINC
		
		case 0x55 : // 'U' Cmnd_STK_LOAD_ADDRESS : set address (word)
			address = getch();
			address += (getch() << 8);
			empty_reply();
		break;
		
		case 0x56 : // 'V' Cmnd_STK_UNIVERSAL
			// Universal command is used to send a generic 32-bit 
			// data/command stream directly to the SPI interface of the 
			// current device. 
			universal();
		break;
		
		//case 0x57 : // 'W' Cmnd_STK_UNIVERSAL_MULTI
		
		case 0x60 : // '`' Cmnd_STK_PROG_FLASH : Program one word in FLASH memory
			low = getch();
			high = getch();
			empty_reply();
		break;
		
		case 0x61 : // 'a' Cmnd_STK_PROG_DATA : Program one byte in EEPROM memory
			data = getch();
			empty_reply();
		break;
		
		// case 0x62 : // 'b' Cmnd_STK_PROG_FUSE : Program fuse bits
		// case 0x63 : // 'c' Cmnd_STK_PROG_LOCK
		
		case 0x64 : // 'd' Cmnd_STK_PROG_PAGE
			// Download a block of data to the starterkit and program it 
			// in FLASH or EEPROM of thecurrent device. The data block 
			// size should not be larger than 256 bytes
			if ( ! chip_erased )
			{
				error = 0;
				end_pmode();
				start_pmode(1);
				chip_erased = 1;
			}
			program_page();
		break;
		
		// case 0x65 : // 'e' Cmnd_STK_PROG_FUSE_EXT
		
		// case 0x70 : // 'p' Cmnd_STK_READ_FLASH : Read one word from FLASH memory
		
		// case 0x71 : // 'q' Cmnd_STK_READ_DATA : Read one word from EEPROM memory
		// case 0x72 : // 'r' Cmnd_STK_READ_FUSE 
		// case 0x73 : // 's' Cmnd_STK_READ_LOCK
		
		case 0x74 : // 't' Cmnd_STK_READ_PAGE 
			read_page();
		break;
		
		case 0x75 : // 'u' Cmnd_STK_READ_SIGN : Read signature bytes
			read_signature();
		break;
		
		// case 0x76 : // 'v' Cmnd_STK_READ_OSCCAL : Read Oscillator calibration byte
		// case 0x77 : // 'w' Cmnd_STK_READ_FUSE_EXT
		// case 0x78 : // 'x' Cmnd_STK_READ_OSCCAL_EXT
		
		case CRC_EOP: // expecting a command, not CRC_EOP
			// this is how we can get back in sync
			error++;
			Serial.print((char) STK_NOSYNC);
		break;
		
		default: // anything else we will return STK_UNKNOWN
			error++;
			if ( CRC_EOP == getch() )
			{
				Serial.print((char)STK_UNKNOWN);
			}
			else
			{
				Serial.print((char)STK_NOSYNC);
			}
		break;
		
	} // end_switch
	
	return 0;
}
