# TeensyPDD
A hardware emulator of the Tandy Portable Disk Drive using an SD card for mass storage.

Based on Jimmy Petit's SD2TPDD

Status:
* Emulate the basic file-access functions of a TPDD1
* Provide DME directory access

BUGS:
* Works with real TS-DOS, but file transfers don't actually work with TpddTool.py
: This seems to be due to mis-matches in handling the space-padding in the filenames?
* Some kind of working directory initial/default state issue where:
: TS-DOS can successfully load a file like DOS100.CO from the root dir,
: but Ultimate Rom 2 usually can not load that same file, but sometimes it can.
: update:
: If you try to use TS-DOS from UR2 after a fresh power-cycle of both TeensyPDD and the M100,
: It doesn't work. But If you load TS-DOS (for example, use a REX to switch roms from UR2 to TS-DOS)
: use TS-DOS to read the directory listing once, then switch roms back to UR-2, then the TS-DOS
: menu entry in UR-2 works (successfully loads DOS100.CO from the disk).
: So TS-DOS does some kind of initialization that UR-2 isn't doing. UR-2 works fine
: with a real TPDD/TPDD2 and dlplus, so there must be some sort of default condition that
: we should be resetting to.
* If you use UR-2 to load TS-DOS in ram from the TeensyPDD,
: and switch to a subdirectory like /Games while in TS-DOS, then exit TS-DOS,
: You can't use TS-DOS any more, because the next time UR-2 tries to load DOS100.CO
: from disk, TeensyPDD looks for /Games/DOS100.CO, which does not exist.
: Power-cycling the TeensyPDD usually doesn't work either. I have not yet figured out
: what makes UR-2 work sometimes but not others.
: If you have DOS100.CO in ram, then UR-2 works because it will use that copy if available.
: And TS-DOS (rom or ram) seems to be working pretty well all the time.
* When UR-2 loads DOS100.CO, sucessfully or not, the LED doesn't shut off after.
* Diesplays PARENT.<> entry even when you are already in the root dir.
: Goes away if you try to open PARENT.<> when you're already in root.


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
* Attach the RS232 level shifter to Teensy pins 0(RX1),1(TX1),GND,3.3V
* Bridge the DTR and DSR pins on the RS232 connector (Required for TS-DOS)
* You can ignore RTS/CTS entirely. (Don't have to connect them or even short them)

## Notes
If you plan on using TS-DOS, some versions require that you have a DOS100.CO file on the root of the media. This file can be downloaded from here:
http://www.club100.org/nads/dos100.co

## To-Do
* A protocol expansion allowing access to files greater than 64KB in size
* Full NADSBox compatibility
* A command-line that can be accessed from the computer's terminal emulator for quicker file manipulation
* Teensy built-in RTC
* write-protect?
* teeny/ts-dos injector (dlplus looks for YY preamble and sends loader.ba)
* Translate line-endings for .DO files?
* Fixup filenames each time one is handled, to add or remove the space-padding as needed.
* Instead of waiting for sd card in setup(), detect missing sdcard and send back proper error in loop(), so that TS-DOS doesn't lock up.
* Make optional instructions to hook up DSR/DTR/DCD to a real pin, maybe via the otherwise un-used RTS/CTS pins on the rs232 shifter, so that the Teensy uses it to actually signal drive-ready instead of just being shorted.


## Change-log
### 20180825 bkw
* Ported Jimmy Petit's SD2TPDD to Teensy 3.5/3.6

### V0.2 (7/27/2018)
* Added DME support
* Corrected some file name padding bugs

### V0.1 (7/21/2018)
* Initial testing release with basic TPDD1 emulation
