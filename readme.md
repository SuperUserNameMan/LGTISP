# LGTISP
LGT8Fx8p ISP protocol implementation.

## introduction
This is an implementation of LGT8Fx8p ISP protocol. 
You can turn a LGT8Fx8p or an ATmega328p Arduino board into an ISP for LGT8Fx8P.

This https://github.com/SuperUserNameMan/LGTISP version supports the `dump flash` and `dump eeprom` commands of AVRdude in terminal mode.

## supported functions
- Arduino IDE and AVRdude commande line :
   - [x] upload/restore LGT8Fx8p bootloaders from the Arduino IDE
   - [x] upload sketches to LGT8Fx8p board from the Arduino IDE (Ctrl + Shift + U)
- AVRdude terminal mode :
   - [x] `dump flash` command
   - [x] `dump eeprom` command
   - [ ] `write` command
   - [ ] `erase` command
   - [x] `sig` command
   - [x] `part` command

## usage

1. **Turn an other Nano board (ATmega328p or LGT8F328p) into an LGTISP :**
   1. If you want to use a real ATmega328p or ATmega168 Nano board as ISP :
       * select "Tools / Board type / Arduino AVR board / Arduino Nano".
       * select the appropriate "Tools / Processor". 
       * into your Arduino IDE installation directory, edit `hardware/arduino/avr/cores/arduino/HardwareSerial.h`.
       * at the beginning of the file, temporarily add `#define SERIAL_RX_BUFFER_SIZE 256` and save. 
   2. If you want to use a LGT8F328p Nano-like board as ISP :
       * select "Tools / Board type / Logic Green Arduino AVR compatible board / LGT8F328".
       * select the appropriate "Tools / Processor".
       * select "Tools / Arduino as ISP / \[To burn an ISP\] SERIAL_RX_BUFFER_SIZE 250".
   2. Upload this sketch into your Nano board.  
      * If the board is real ATmega328p Nano board, you can now remove the `#define SERIAL_RX_BUFFER_SIZE 256` from `HardwareSerial.h`. 
   3. If you want to avoid bootloader from executing (so you don't reprogram the ISP board accidentally) short `RESET` pin and `VCC` pin of board.
   4. The board has now become a LGTISP ! 
   5. Connect your Nano to the target :   
      | Nano || LGT ||
      |:-:|:-:|:-:|:-:|
      | D13 | -> | SWC ||
      | D12 | -> | SWD ||
      | D10 | -> | RST | (optional)|
      
2. **To burn a bootloader into a LGT8Fx8p board :**
   1. select "Tools / Board type / Logic Green Arduino AVR compatible board / LGT8F328".
   2. select the appropriate "Tools / Processor type".
   3. select "Tools / Programmer / AVR ISP".
   4. click "Tools / Burn Bootloader".
   
3. **To upload a sketch into a LGT8Fx8p board using your ISP :**
   1. select "Tools / Board type / Logic Green Arduino AVR compatible board / LGT8F328".
   2. select the appropriate "Tools / Processor type".
   3. select "Tools / Programmer / AVR ISP".
   4. click "Sketch / Upload using a programmer" or press `Ctrl + Shift + U`.
   
4. **To display the content of the flash using AVRdude :**
   1. open a sketch in the Arduino IDE, and click "Files / Preferences".
   2. check the "Display detailed results while [X] uploading", and press OK.
   3. upload this sketch using the LGTISP.
   4. into the console, look for the white line containing the avrdude commant line, and copy paste it into text editor. It should looks like something like this:
   `<path>/arduino-1.8.13/hardware/tools/avr/bin/avrdude -C<path>/arduino-1.8.13/hardware/tools/avr/etc/avrdude.conf -v -patmega328p -cstk500v1 -P<isp_serial_adapter> -Uflash:w:/tmp/arduino_build_379084/<sketchname>.ino.hex:i`
   5. change it so it looks like this:
   `<path>/arduino-1.8.13/hardware/tools/avr/bin/avrdude -C<path>/arduino-1.8.13/hardware/tools/avr/etc/avrdude.conf -v -patmega328p -cstk500v1 -P<isp_serial_adapter> -t`
   6. `-t` will open AVRdude in terminal mode.
   7. open a terminal, and run this command. AVRdude should open in terminal mode.
   7. command `dump flash <addr> <length>` will let you display the content of the flash.
   8. commant `quit` to quit, and `help` for help.
   

## reference
- Forked from : [brother-yan/LGTISP](https://github.com/brother-yan/LGTISP)
- recommanded LGT8F core for Arduino IDE : [dbuezas/lgt8fx](https://github.com/dbuezas/lgt8fx)
- Atk500v1 protocol : http://ww1.microchip.com/downloads/en/Appnotes/doc2525.pdf 
- AVRdude : http://savannah.nongnu.org/projects/avrdude
- AVR ISP protocol : [AVR910 - In-System-Programming protocol](http://ww1.microchip.com/downloads/en/Appnotes/Atmel-0943-In-System-Programming_ApplicationNote_AVR910.pdf)
