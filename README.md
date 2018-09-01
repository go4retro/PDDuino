# TeensyPDD
A hardware emulator of the Tandy Portable Disk Drive using an SD card for mass storage.

Based on Jimmy Petit's SD2TPDD

Status:
* Emulate the basic file-access functions of a TPDD1 (not fully working)
* Provide DME directory access

BUGS:
* Writing a file reports success, but doesn't actually happen.


## Requirements
### Hardware
```
Teensy 3.5 or 3.6
RS232<>CMOS level shifter
```

### Software
```
Arduino IDE
TeensyDuino  (add-on for Arduino IDE)
Bill Greiman's SdFat library
```

## Assembly
### Hardware
* Attach the RS232 level shifter to Teensy pins 0(RX1),1(TX1),20(CTS1),21(RTS1),GND,3.3V
* Bridge the DTR and DSR pins on the RS232 connector (Required for TS-DOS)

## Notes
If you plan on using TS-DOS, some versions require that you have a DOS100.CO file on the root of the media. This file can be downloaded from here:
http://www.club100.org/nads/dos100.co

## To-Do
* A protocol expansion allowing access to files greater than 64KB in size
* Full NADSBox compatibility
* A command-line that can be accessed from the computer's terminal emulator for quicker file manipulation
* Teensy built-in RTC
* write-protect?
* teeny/ts-dos injector

## Change-log
### 20180825 bkw
* Ported Jimmy Petit's SD2TPDD to Teensy 3.5/3.6

### V0.2 (7/27/2018)
* Added DME support
* Corrected some file name padding bugs

### V0.1 (7/21/2018)
* Initial testing release with basic TPDD1 emulation
