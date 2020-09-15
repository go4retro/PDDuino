/*
 * PDDuino - Tandy Portable Disk Drive emulator on Arduino
 * github.com/bkw777/PDDuino
 * Based on github.com/TangentDelta/SD2TPDD
 */

#define SKETCH_NAME __FILE__
#define SKETCH_VERSION "0.4.1"
// 0.4.1 20200913 bkw - more IDE-supplied macros to un-hardcode where possible, more debug output
// 0.4.0 20200828 bkw - sendLoader()
// 0.3.0 20180921 bkw - Teensy & Feather boards, CLIENT & CONSOLE, setLabel(), sleepNow()
// 0.2   20180728 Jimmy Pettit original

// log activity to serial monitor port
// Console is set for 115200 no flow
// 0 or undef = disable serial console, 1 = enabled, least verbose, 2+ = enabled, more verbose
// When disabled, the port itself is disabled, which frees up significant ram and cpu cycles on some hardware.
// Example, on Teensy 3.5/3.6, with the port enabled at all, the sketch must be compiled to run the cpu at a minimum of 24mhz
// but with the port disabled, the sketch can be compiled to run at only 2Mhz. (Tools -> CPU Speed).
// DEBUG will also be disabled if the selected BOARD_* section below doesn't define any CONSOLE port.
#define DEBUG 3     // disable unless actually debugging, uses more battery, prevents full sleep, hangs at boot until connected
//#define DEBUG_LED   // use led to see (some) activity even if no DEBUG or CONSOLE
//#define DEBUG_SLEEP // use DEBUG_LED to debug sleepNow()

// File that sendLoader() will try to send to the client.
// Comment out to disable sendLoader() & ignore DSR_PIN
#define LOADER_FILE "LOADER.DO"

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
//--- configs for some handy boards that are small and have sd readers built in ---

// Detect board/platform from IDE-supplied macros
//
// How to find these values.
// There is no list, and no consistent rule, every manufacturer & board makes up something.
// But in general, find the platform.txt and boards.txt for your board in the locations below (on linux)
// ~/arduino-*/hardware/*/*/boards.txt               Arduino and Teensy
// ~/arduino-*/hardware/*/*/platform.txt
// ~/.arduino*/packages/*/hardware/*/*/boards.txt    Most others, ex: Adafruit, Heltec, etc
// ~/.arduino*/packages/*/hardware/*/*/platform.txt
//
// platform.txt has gcc commandlines with "-D..." defines like "-DARDUINO_{something}"
// boards.txt has "something=AVR_UNO"
// so here we would look for "#if defined(ARDUINO_AVR_UNO)"

#if defined(ARDUINO_AVR_FEATHER32U4)
  #define BOARD_FEATHER32U4
  #define BOARD_NAME "Adafruit Feather 32u4"

#elif defined(ADAFRUIT_FEATHER_M0)
  #define BOARD_FEATHERM0
  #define BOARD_NAME "Adafruit Feather M0"

#elif defined(ARDUINO_TEENSY35)
  #define BOARD_TEENSY35
  #define BOARD_NAME "Teensy 3.5"

#elif defined(ARDUINO_TEENSY36)
  #define BOARD_TEENSY36
  #define BOARD_NAME "Teensy 3.6"

#elif defined(ARDUINO_AVR_UNO)
  #define BOARD_UNO
  #define BOARD_NAME "Arduino UNO"

#elif defined(ARDUINO_AVR_MEGA2560)
  #define BOARD_MEGA
  #define BOARD_NAME "Arduino MEGA"

//#elif defined(ARDUINO_AVR_UNO)          // example to add a new board
//  #define BOARD_UNO
//  #define BOARD_NAME "Arduino UNO"

#else
  #define BOARD_DEFAULT
  #define BOARD_NAME "Default"
#endif

#if defined(BOARD_DEFAULT)
  // User-defined platform details
  #define CONSOLE SERIAL_PORT_MONITOR      // where to send debug messages, if enabled. (Serial)
  #define CLIENT SERIAL_PORT_HARDWARE_OPEN // what serial port is the TPDD client connected to (Serial1)
  #define DTR_PIN 5                         // pin to output DTR, github.com/bkw777/MounT uses pin 5
  #define DSR_PIN 6                        // pin to input DSR, github.com/bkw777/MounT uses pin6
  #define SD_CS_PIN 4                      // sd card reader chip-select pin #, usually automatic
  //#define SD_CD_PIN 7                    // sd card reader card-detect pin #, usually none
  //#define DISABLE_CS 10               // Disable other SPI device on this pin, usully none, assume SD is only SPI device
  //#define USE_SDIO                    // sd card reader is connected by SDIO instead of SPI (Teensy 3.5/3.6/4.1)
  #define ENABLE_SLEEP                  // sleepNow() while idle for power saving
  #define WAKE_PIN 0                    // CLIENT RX pin#, interrupt is attached to wake from sleepNow()
  //#define SLEEP_DELAY 5000            // Delay in ms before sleeping
  //#define USE_ALP                     // Use ArduinoLowPower library for sleepNow(), otherwise use avr/sleep.h
  #define PINMODE_SD_LED_OUTPUT pinMode(LED_BUILTIN,OUTPUT);
  #define SD_LED_ON digitalWrite(LED_BUILTIN,HIGH);
  #define SD_LED_OFF digitalWrite(LED_BUILTIN,LOW);
  #define PINMODE_DEBUG_LED_OUTPUT pinMode(LED_BUILTIN,OUTPUT);
  #define DEBUG_LED_ON digitalWrite(LED_BUILTIN,HIGH);
  #define DEBUG_LED_OFF digitalWrite(LED_BUILTIN,LOW);

// Teensy3.5, Teensy3.6
// https://www.pjrc.com/store/teensy35.html
// https://www.pjrc.com/store/teensy36.html
// Teensy 3.6 has no CD pin and no good alternative way to detect card.
// https://forum.pjrc.com/threads/43422-Teensy-3-6-SD-card-detect
// Might be able to use comment #10 idea.
//   #define CPU_RESTART_ADDR (uint32_t *)0xE000ED0C
//   #define CPU_RESTART_VAL 0x5FA0004
//   #define CPU_RESTART (*CPU_RESTART_ADDR = CPU_RESTART_VAL);
//   ...
//   if (!sd.exists("settings.txt")) { CPU_RESTART }
//
#elif defined(BOARD_TEENSY35) || defined(BOARD_TEENSY36)
  #define CONSOLE SERIAL_PORT_MONITOR
  #define CLIENT SERIAL_PORT_HARDWARE_OPEN
  #define DTR_PIN 5
  #define DSR_PIN 6
  //#define SD_CS_PIN SS
  //#define SD_CD_PIN 7
  //#define DISABLE_CS 10
  #define USE_SDIO
  #define ENABLE_SLEEP
  #define WAKE_PIN 0
  //#define SLEEP_DELAY 5000
  //#define USE_ALP
  // Main LED: PB5
  #define PINMODE_SD_LED_OUTPUT DDRB = DDRB |= 1UL << 5;
  #define SD_LED_ON PORTB |= _BV(5);
  #define SD_LED_OFF PORTB &= ~_BV(5);
  // Main LED: PB5
  #define PINMODE_DEBUG_LED_OUTPUT DDRB = DDRB |= 1UL << 5;
  #define DEBUG_LED_ON PORTB |= _BV(5);
  #define DEBUG_LED_OFF PORTB &= ~_BV(5);

// Adafruit Feather 32u4 Adalogger
// https://learn.adafruit.com/adafruit-feather-32u4-adalogger
//
#elif defined(BOARD_FEATHER32U4)
  #define CONSOLE SERIAL_PORT_MONITOR
  #define CLIENT SERIAL_PORT_HARDWARE_OPEN
  #define DTR_PIN 5
  #define DSR_PIN 6
  #define SD_CS_PIN 4 
  //#define SD_CD_PIN 7
  //#define DISABLE_CS 10
  //#define USE_SDIO
  #define ENABLE_SLEEP
  #define WAKE_PIN 0
  #define SLEEP_DELAY 5000          // Adalogger 32u4 needs a few seconds before sleeping
  //#define USE_ALP
  // Green LED near card reader: PB4
  #define PINMODE_SD_LED_OUTPUT DDRB = DDRB |= 1UL << 4;
  #define SD_LED_ON PORTB |= _BV(4);
  #define SD_LED_OFF PORTB &= ~_BV(4);
  // Main LED: PC7
  #define PINMODE_DEBUG_LED_OUTPUT DDRC = DDRC |= 1UL << 7;
  #define DEBUG_LED_ON PORTC |= _BV(7);
  #define DEBUG_LED_OFF PORTC &= ~_BV(7);

// Adafruit Feather M0 Adalogger
// https://learn.adafruit.com/adafruit-feather-m0-adalogger
//
#elif defined(BOARD_FEATHERM0)
  #define CONSOLE SERIAL_PORT_MONITOR
  #define CLIENT SERIAL_PORT_HARDWARE_OPEN
  #define DTR_PIN 5
  #define DSR_PIN 6
  #define SD_CS_PIN 4
  //#define DISABLE_CS 10
  //#define USE_SDIO
  #define ENABLE_SLEEP
  #define WAKE_PIN 0
  #define SLEEP_DELAY 250
  #define USE_ALP
  // Green LED near card reader: PA6 / pin 8
  #define PINMODE_SD_LED_OUTPUT pinMode(8,OUTPUT);
  #define SD_LED_ON digitalWrite(8,HIGH);
  #define SD_LED_OFF digitalWrite(8,LOW);
  // Main LED: PA17
  #define PINMODE_DEBUG_LED_OUTPUT pinMode(LED_BUILTIN,OUTPUT);
  #define DEBUG_LED_ON digitalWrite(LED_BUILTIN,HIGH);
  #define DEBUG_LED_OFF digitalWrite(LED_BUILTIN,LOW);

#elif defined(BOARD_UNO)
#ifndef HAVE_HWSERIAL1
  #define CONSOLE                   mySerial
  #include <SoftwareSerial.h>
  #define CONSOLE_DECL              SoftwareSerial mySerial(7,8); // RX, TX;
#else
  #define CONSOLE                   SERIAL_PORT_HARDWARE_OPEN
#endif
  #define CLIENT                    SERIAL_PORT_MONITOR
  #define DTR_PIN                   5
  #define DSR_PIN                   6
  #define SD_CS_PIN                 10
  // User-defined platform details
  //#define ENABLE_SLEEP
  //#define WAKE_PIN 0
  //#define SLEEP_DELAY 5000
  //#define USE_ALP
  #define LED_SD_ACT                9
  #define PINMODE_SD_LED_OUTPUT     pinMode(LED_SD_ACT, OUTPUT);
  #define SD_LED_ON                 digitalWrite(LED_SD_ACT,HIGH);
  #define SD_LED_OFF                digitalWrite(LED_SD_ACT,LOW);
  #define LED_DEBUG                 4
  #define PINMODE_DEBUG_LED_OUTPUT  pinMode(LED_DEBUG,OUTPUT);
  #define DEBUG_LED_ON              digitalWrite(LED_DEBUG,HIGH);
  #define DEBUG_LED_OFF             digitalWrite(LED_DEBUG,LOW);

#elif defined(BOARD_MEGA)
  #define CONSOLE                   SERIAL_PORT_HARDWARE_OPEN
  #define CLIENT                    SERIAL_PORT_MONITOR
  #define DTR_PIN                   5
  #define DSR_PIN                   6
  #define SD_CS_PIN                 53
  // User-defined platform details
  //#define ENABLE_SLEEP
  //#define WAKE_PIN 0
  //#define SLEEP_DELAY 5000
  //#define USE_ALP
  #define LED_SD_ACT                9
  #define PINMODE_SD_LED_OUTPUT     pinMode(LED_SD_ACT, OUTPUT);
  #define SD_LED_ON                 digitalWrite(LED_SD_ACT,HIGH);
  #define SD_LED_OFF                digitalWrite(LED_SD_ACT,LOW);
  #define LED_DEBUG                 4
  #define PINMODE_DEBUG_LED_OUTPUT  pinMode(LED_DEBUG,OUTPUT);
  #define DEBUG_LED_ON              digitalWrite(LED_DEBUG,HIGH);
  #define DEBUG_LED_OFF             digitalWrite(LED_DEBUG,LOW);

// example to add a new board
//#elif defined(BOARD_UNO)
//  // User-defined platform details
//  #define CONSOLE SERIAL_PORT_MONITOR
//  #define CLIENT SERIAL_PORT_HARDWARE_OPEN
//  #define DTR_PIN 5
//  #define DSR_PIN 6
//  #define SD_CS_PIN 4
//  //#define SD_CD_PIN 7
//  //#define DISABLE_CS 10
//  //#define USE_SDIO
//  #define ENABLE_SLEEP
//  #define WAKE_PIN 0
//  //#define SLEEP_DELAY 5000
//  //#define USE_ALP
//  #define PINMODE_SD_LED_OUTPUT pinMode(LED_BUILTIN,OUTPUT);
//  #define SD_LED_ON digitalWrite(LED_BUILTIN,HIGH);
//  #define SD_LED_OFF digitalWrite(LED_BUILTIN,LOW);
//  #define PINMODE_DEBUG_LED_OUTPUT pinMode(LED_BUILTIN,OUTPUT);
//  #define DEBUG_LED_ON digitalWrite(LED_BUILTIN,HIGH);
//  #define DEBUG_LED_OFF digitalWrite(LED_BUILTIN,LOW);

#endif // BOARD_*

#ifndef CONSOLE_DECL
  #define CONSOLE_DECL
#endif

//
// end of config section
//
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
#include <BlockDriver.h>
#include <FreeStack.h>
#include <MinimumSerial.h>
#if !defined(USE_SDIO)
#include <SPI.h>
#endif
#include <SdFat.h>
#include <sdios.h>
#include <SdFatConfig.h>
#include <SysCall.h>

CONSOLE_DECL

#if defined(USE_SDIO)
SdFatSdioEX SD;
#else
SdFat SD;
#endif

#if !defined(DEBUG)
#define DEBUG 0
#endif

#if !defined(PINMODE_SD_LED_OUTPUT) || !defined(SD_LED_ON) || !defined(SD_LED_OFF)
  #define PINMODE_SD_LED_OUTPUT
  #define SD_LED_ON
  #define SD_LED_OFF
  #define SD_LED 0
#else
  #define SD_LED 1
#endif

#if !defined(PINMODE_DEBUG_LED_OUTPUT)  || !defined(DEBUG_LED_ON) || !defined(DEBUG_LED_OFF)
 #define PINMODE_DEBUG_LED_OUTPUT
 #define DEBUG_LED_ON
 #define DEBUG_LED_OFF
#endif

#if DEBUG && defined(CONSOLE)
 #define DEBUG_PRINT(x)     CONSOLE.print (x)
 #define DEBUG_PRINTI(x,y)  CONSOLE.print (x,y)
 #define DEBUG_PRINTL(x)    CONSOLE.println (x)
 #define DEBUG_PRINTIL(x,y) CONSOLE.println (x,y)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTI(x,y)
 #define DEBUG_PRINTL(x)
 #define DEBUG_PRINTIL(x,y)
#endif

#if !defined(SLEEP_DELAY)
#define SLEEP_DELAY 0
#endif

#if defined(ENABLE_SLEEP)
 #if defined(USE_ALP)
  #include <ArduinoLowPower.h>
 #else
  #include <avr/sleep.h>
 #endif // USE_ALP
#endif // ENABLE_SLEEP

#define DATA_BUFFER_SZ 0x0100  // 256 bytes, 2 full tpdd packets? 0xFF did not work.
#define FILE_BUFFER_SZ 0x80    // 128 bytes at at time to/from files
#define DIRECTORY_SZ 0x40      // size of directory[] which holds full paths
#define FILENAME_SZ 0x18       // TPDD protocol spec 1C, minus 4 for ".<>"+NULL


File root;  //Root file for filesystem reference
File entry; //Moving file entry for the emulator
File tempEntry; //Temporary entry for moving files

byte head = 0x00;  //Head index
byte tail = 0x00;  //Tail index

byte checksum = 0x00;  //Global variable for checksum calculation

byte state = 0x00; //Emulator command reading state
bool DME = false; //TS-DOS DME mode flag

byte dataBuffer[DATA_BUFFER_SZ]; //Data buffer for commands
byte fileBuffer[FILE_BUFFER_SZ]; //Data buffer for file reading

char refFileName[FILENAME_SZ] = "";  //Reference file name for emulator
char refFileNameNoDir[FILENAME_SZ] = ""; //Reference file name for emulator with no ".<>" if directory
char tempRefFileName[FILENAME_SZ] = ""; //Second reference file name for renaming
char entryName[FILENAME_SZ] = "";  //Entry name for emulator
byte directoryBlock = 0x00; //Current directory block for directory listing
char directory[DIRECTORY_SZ] = "/";
byte directoryDepth = 0x00;
char tempDirectory[DIRECTORY_SZ] = "/";
char dmeLabel[0x07] = "";  // 6 chars + NULL


/*
 *
 * General misc. routines
 *
 */

#if DEBUG
void printDirectory(File dir, byte numTabs) {
  char fileName[FILENAME_SZ] = "";

  SD_LED_ON
  while (entry = dir.openNextFile(O_RDONLY)) {
    if(!entry.isHidden()) {

      entry.getName(fileName,FILENAME_SZ);
      for (byte i = 0x00; i < numTabs; i++) DEBUG_PRINT(F("\t"));
      DEBUG_PRINT(fileName);
      if (entry.isDirectory()) {
        DEBUG_PRINTL(F("/"));
        DEBUG_PRINT(F("--- printDirectory(")); DEBUG_PRINT(fileName); DEBUG_PRINT(F(",")); DEBUG_PRINT(numTabs+0x01); DEBUG_PRINTL(F(") start ---"));
        printDirectory(entry, numTabs + 0x01);
        DEBUG_PRINT(F("--- printDirectory(")); DEBUG_PRINT(fileName); DEBUG_PRINT(F(",")); DEBUG_PRINT(numTabs+0x01); DEBUG_PRINTL(F(") end ---"));
      } else {
        DEBUG_PRINT(F("\t\t")); DEBUG_PRINTIL(entry.fileSize(), DEC);
      }
    }
    entry.close();
  }
  SD_LED_OFF
}
#endif // DEBUG

#if defined(ENABLE_SLEEP)
 #if !defined(USE_ALP)
  const byte wakeInterrupt = digitalPinToInterrupt(WAKE_PIN);
 #endif // !USE_ALP

 #if defined(SLEEP_DELAY)
  unsigned long now = millis();
  unsigned long idleSince = now;
 #endif // SLEEP_DELAY

void wakeNow () {
}

void sleepNow() {
 #if defined(SLEEP_DELAY)
  now = millis();
  if ((now-idleSince)<SLEEP_DELAY) return;
  idleSince = now;
 #endif // SLEEP_DELAY
 #if defined(USE_ALP)
  LowPower.attachInterruptWakeup(WAKE_PIN, wakeNow, CHANGE);
  LowPower.sleep();
 #else
  // if the debug console is enabled, then don't sleep deep enough to power off the usb port
  #if DEBUG
  set_sleep_mode(SLEEP_MODE_IDLE);
  #else
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  #endif // DEBUG
  DEBUG_LED_ON
  attachInterrupt(wakeInterrupt,wakeNow,CHANGE);
  sleep_mode();
  detachInterrupt(wakeInterrupt);
 #endif // USE_ALP
  DEBUG_LED_OFF
}
#endif // ENABLE_SLEEP

void initCard () {
  while(true) {
    DEBUG_PRINT(F("Opening SD card..."));
    SD_LED_ON
#if defined(USE_SDIO)
 #if DEBUG > 2
    DEBUG_PRINT(F("(SDIO)"));
 #endif
    if (SD.begin()) {
#else
 #if DEBUG > 2
    DEBUG_PRINT(F("(SPI)"));
 #endif
 #if defined(SD_CS_PIN) && defined(SD_SPI_MHZ)
    if (SD.begin(SD_CS_PIN,SD_SCK_MHZ(SD_SPI_MHZ))) {
 #elif defined(SD_CS_PIN)
    if (SD.begin(SD_CS_PIN)) {
 #else
    if (SD.begin()) {
 #endif
#endif  // USE_SDIO
      DEBUG_PRINTL(F("OK."));
#if SD_LED
      SD_LED_OFF
      delay (0x60);
      SD_LED_ON
      delay (0x60);
      SD_LED_OFF
      delay (0x60);
      SD_LED_ON
      delay (0x60);
#endif
      break;
    } else {
      DEBUG_PRINTL(F("No SD card."));
#if SD_LED
      SD_LED_OFF
      delay(1000);
#endif
    }
  }

  SD.chvol();

  // TODO - get the FAT volume label and use it in place of rootLabel
  // ...non-trivial

  // Always do this open() & close(), even if we aren't doing the printDirectory()
  // It's needed to get the SdFat library to put the sd card to sleep.
  root = SD.open(directory);
#if DEBUG
  DEBUG_PRINTL(F("--- printDirectory(root,0) start ---"));
  printDirectory(root,0x00);
  DEBUG_PRINTL(F("--- printDirectory(root,0) end ---"));
#endif
  root.close();

  SD_LED_OFF
}


#if defined(LOADER_FILE)
/*
 * sendLoader() and restart()
 *
 * At power-on the Model 100 rs232 port sets all data & control pins to -5v.
 * On RUN "COM:98N1E", pins 4 (RTS) and 20 (DTR) go to +5v.
 * github.com/bkw777/MounT connects GPIO pin 5 through MAX3232 to RS232 DSR (DB25 pin 6) and to RS232 DCD (DB25 pin 8)
 * and connects RS232 DTR (DB25 pin 20) through MAX3232 to GPIO pin 6.
 * To assert DTR (to RS232 DSR), raise or lower GPIO pin 5.
 * To read DSR (from RS232 DTR), read GPIO pin 6.
 */

/* reboot */
void(* restart) (void) = 0;

/* TPDD2-style bootstrap */
void sendLoader() {
  byte b = 0x00;
#if DEBUG
  int c = 0;
#endif // DEBUG
  File f = SD.open(LOADER_FILE);

  DEBUG_PRINTL(F("sendLoader()"));  
  if (f) {
    SD_LED_ON
    DEBUG_PRINT(F("Sending " LOADER_FILE " "));
      while (f.available()) {
        b = f.read();
        CLIENT.write(b);
#if DEBUG
        if(++c>99) {
         DEBUG_PRINT(F("."));
         c = 0;
        }
#endif // DEBUG
        delay(0x05);
      }
    f.close();
    SD_LED_OFF
    CLIENT.write(0x1A);
    CLIENT.flush();
    CLIENT.end();
    DEBUG_PRINTL(F("DONE"));
  } else {
    DEBUG_PRINT(F("Could not find " LOADER_FILE " ..."));
  }
  restart(); // go back to normal TPDD emulation mode
}
#endif // LOADER_FILE

// Append a string to directory[]
void directoryAppend(char* c){
  bool t = false;
  byte i = 0x00;
  byte j = 0x00;

  DEBUG_PRINT(F("directoryAppend(")); DEBUG_PRINT(c); DEBUG_PRINTL(F(")"));
  DEBUG_PRINT(F("directory[")); DEBUG_PRINT(directory); DEBUG_PRINTL(F("]"));

  DEBUG_PRINTL(F("->"));

  while(directory[i] != 0x00) i++;

  while(!t){
    directory[i++] = c[j++];
    t = c[j] == 0x00;
  }

  DEBUG_PRINT(F("directory[")); DEBUG_PRINT(directory); DEBUG_PRINTL(F("]"));
  DEBUG_PRINT(F("directoryAppend(")); DEBUG_PRINT(c); DEBUG_PRINTL(F(") end"));
}

// Remove the last path element from directoy[]
void upDirectory(){
  byte j = DIRECTORY_SZ;

  DEBUG_PRINTL(F("upDirectory()"));
  DEBUG_PRINT(F("directory[")); DEBUG_PRINT(directory); DEBUG_PRINTL(F("]"));

  while(directory[j] == 0x00) j--;
  if(directory[j] == '/' && j!= 0x00) directory[j] = 0x00;
  while(directory[j] != '/') directory[j--] = 0x00;

  DEBUG_PRINT(F("directory[")); DEBUG_PRINT(directory); DEBUG_PRINTL(F("]"));
}

void copyDirectory(){ //Makes a copy of the working directory to a scratchpad
  for(byte i=0x00; i<DIRECTORY_SZ; i++) tempDirectory[i] = directory[i];
}


// Fill dmeLabel[] with exactly 6 chars from s[], space-padded.
// We could just read directory[] directly instead of passng s[]
// but this way we can pass arbitrary values later. For example
// FAT volume label, RTC time, battery level, ...
void setLabel(char* s) {
  byte z = DIRECTORY_SZ;
  byte j = z;

  DEBUG_PRINT(F("setLabel(")); DEBUG_PRINT(s); DEBUG_PRINTL(F(")"));
  DEBUG_PRINT(F("directory[")); DEBUG_PRINT(directory); DEBUG_PRINTL(F("]"));
  DEBUG_PRINT(F("dmeLabel["));  DEBUG_PRINT(dmeLabel);  DEBUG_PRINTL(F("]"));

  while(s[j] == 0x00) j--;            // seek from end to non-null
  if(s[j] == '/' && j > 0x00) j--;    // seek past trailing slash
  z = j;                              // mark end of name
  while(s[j] != '/' && j > 0x00) j--; // seek to next slash

  // copy 6 chars, up to z or null, space pad
  for(byte i=0x00 ; i<0x06 ; i++) if(s[++j]>0x00 && j<=z) dmeLabel[i] = s[j]; else dmeLabel[i] = 0x20;
  dmeLabel[0x06] = 0x00;

  DEBUG_PRINT(F("dmeLabel[")); DEBUG_PRINT(dmeLabel); DEBUG_PRINTL(F("]"));
}



/*
 *
 * TPDD Port misc. routines
 *
 */

void tpddWrite(char c){  //Outputs char c to TPDD port and adds to the checksum
  checksum += c;
  CLIENT.write(c);
}

void tpddWriteString(char* c){  //Outputs a null-terminated char array c to the TPDD port
  byte i = 0x00;
  while(c[i]!=0x00){
    checksum += c[i];
    CLIENT.write(c[i]);
    i++;
  }
}

void tpddSendChecksum(){  //Outputs the checksum to the TPDD port and clears the checksum
  CLIENT.write(checksum^0xFF);
  checksum = 0x00;
}


/*
 *
 * TPDD Port return routines
 *
 */

void return_normal(byte errorCode){ //Sends a normal return to the TPDD port with error code errorCode
  DEBUG_PRINTL(F("return_normal()"));
#if DEBUG > 1
  DEBUG_PRINT("R:Norm ");
  DEBUG_PRINTIL(errorCode, HEX);
#endif
  tpddWrite(0x12);  //Return type (normal)
  tpddWrite(0x01);  //Data size (1)
  tpddWrite(errorCode); //Error code
  tpddSendChecksum(); //Checksum
}

void return_reference(){  //Sends a reference return to the TPDD port
  byte term = 0x06;
  bool terminated = false;
  DEBUG_PRINTL(F("return_reference()"));

  tpddWrite(0x11);  //Return type (reference)
  tpddWrite(0x1C);  //Data size (1C)

  for(byte i=0x00;i<FILENAME_SZ;++i) tempRefFileName[i] = 0x00;

  entry.getName(tempRefFileName,FILENAME_SZ);  //Save the current file entry's name to the reference file name buffer

  if(DME && entry.isDirectory()){ //      !!!Tacks ".<>" on the end of the return reference if we're in DME mode and the reference points to a directory
    for(byte i=0x00; i < 0x07; i++){ //Find the end of the directory's name by looping through the name buffer
      if(tempRefFileName[i] == 0x00) term = i; //and setting a termination index to the offset where the termination is encountered
    }
    tempRefFileName[term++] = '.';  //Tack the expected ".<>" to the end of the name
    tempRefFileName[term++] = '<';
    tempRefFileName[term++] = '>';
    for(byte i=term; i<FILENAME_SZ; i++) tempRefFileName[i] = 0x00;
    term = 0x06; //Reset the termination index to prepare for the next check
  }



  for(byte i=0x00; i<0x06; i++){ //      !!!Pads the name of the file out to 6 characters using space characters
    if(term == 0x06){  //Perform these checks if term hasn't changed
      if(tempRefFileName[i]=='.'){
        term = i;   //If we encounter a '.' character, set the temrination pointer to the current offset and output a space character instead
        tpddWrite(' ');
      }else{
        tpddWrite(tempRefFileName[i]);  //If we haven't encountered a period character, output the next character
      }
    }else{
      tpddWrite(' '); //If we did find a period character, write a space character to pad the reference name
    }
  }

  for(byte i=0x00; i<0x12; i++) tpddWrite(tempRefFileName[i+term]);  //      !!!Outputs the file extension part of the reference name starting at the offset found above

  tpddWrite(0x00);  //Attribute, unused
  tpddWrite((byte)((entry.fileSize()&0xFF00)>>0x08));  //File size most significant byte
  tpddWrite((byte)(entry.fileSize()&0xFF)); //File size least significant byte
  tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum
#if DEBUG > 1
  DEBUG_PRINTL("R:Ref");
#endif
}

void return_blank_reference(){  //Sends a blank reference return to the TPDD port
  DEBUG_PRINTL(F("return_blank_reference()"));
  tpddWrite(0x11);    //Return type (reference)
  tpddWrite(0x1C);    //Data size (1C)

  for(byte i=0x00; i<FILENAME_SZ; i++) tpddWrite(0x00);  //Write the reference file name to the TPDD port

  tpddWrite(0x00);    //Attribute, unused
  tpddWrite(0x00);    //File size most significant byte
  tpddWrite(0x00);    //File size least significant byte
  tpddWrite(0x80);    //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum
#if DEBUG > 1
  DEBUG_PRINTL("R:BRef");
#endif
}

void return_parent_reference(){
  DEBUG_PRINTL(F("return_parent_reference()"));
  tpddWrite(0x11);    // return type
  tpddWrite(0x1C);    // data size

  tpddWriteString("PARENT.<>");
  for(byte i=0x09; i<FILENAME_SZ; i++) tpddWrite(0x00);

  tpddWrite(0x00);    //Attribute, unused
  tpddWrite(0x00);    //File size most significant byte
  tpddWrite(0x00);    //File size least significant byte
  tpddWrite(0x80);    //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum
}

/*
 *
 * TPDD Port command handler routines
 *
 */

void command_reference(){ //Reference command handler
  byte searchForm = dataBuffer[(byte)(tail+0x1D)];  //The search form byte exists 29 bytes into the command
  byte refIndex = 0x00;  //Reference file name index

  DEBUG_PRINTL(F("command_reference()"));

#if DEBUG > 1
  DEBUG_PRINT("SF:");
  DEBUG_PRINTIL(searchForm,HEX);
#endif

  if(searchForm == 0x00){ //Request entry by name
    for(byte i=0x04; i<0x1C; i++){  //Put the reference file name into a buffer
      if(dataBuffer[(tail+i)]!=0x20){ //If the char pulled from the command is not a space character (0x20)...
        refFileName[refIndex++]=dataBuffer[(tail+i)]; //write it into the buffer and increment the index.
      }
    }
    refFileName[refIndex]=0x00; //Terminate the file name buffer with a null character

#if DEBUG > 1
    DEBUG_PRINT("Ref: ");
    DEBUG_PRINTL(refFileName);
#endif

    if(DME){  //        !!!Strips the ".<>" off of the reference name if we're in DME mode
      if(strstr(refFileName, ".<>") != 0x00){
        for(byte i=0x00; i<FILENAME_SZ; i++){  //Copies the reference file name to a scratchpad buffer with no directory extension if the reference is for a directory
          if(refFileName[i] != '.' && refFileName[i] != '<' && refFileName[i] != '>'){
            refFileNameNoDir[i]=refFileName[i];
          }else{
            refFileNameNoDir[i]=0x00; //If the character is part of a directory extension, don't copy it
          }
        }
      }else{
        for(byte i=0x00; i<FILENAME_SZ; i++) refFileNameNoDir[i]=refFileName[i]; //Copy the reference directly to the scratchpad buffer if it's not a directory reference
      }
    }

    directoryAppend(refFileNameNoDir);  //Add the reference to the directory buffer

    SD_LED_ON
    if(SD.exists(directory)){ //If the file or directory exists on the SD card...
      entry=SD.open(directory); //...open it...
      return_reference(); //send a refernce return to the TPDD port with its info...
      entry.close();  //...close the entry
    }else{  //If the file does not exist...
      return_blank_reference();
    }

    upDirectory();  //Strip the reference off of the directory buffer

  }else if(searchForm == 0x01){ //Request first directory block
    SD_LED_ON
    root.close();
    root = SD.open(directory);
    ref_openFirst();
  }else if(searchForm == 0x02){ //Request next directory block
    SD_LED_ON
    root.close();
    root = SD.open(directory);
    ref_openNext();
  }else{  //Parameter is invalid
    return_normal(0x36);  //Send a normal return to the TPDD port with a parameter error
  }
  SD_LED_OFF
}

void ref_openFirst(){
  DEBUG_PRINTL(F("ref_openFirst()"));
  directoryBlock = 0x00; //Set the current directory entry index to 0
  if(DME && directoryDepth>0x00 && directoryBlock==0x00){ //Return the "PARENT.<>" reference if we're in DME mode
    SD_LED_OFF
    return_parent_reference();
  }else{
    ref_openNext();    //otherwise we just return the next reference
  }
}

void ref_openNext(){
  DEBUG_PRINTL(F("ref_openNext()"));
  directoryBlock++; //Increment the directory entry index
  SD_LED_ON
  root.rewindDirectory(); //Pull back to the begining of the directory
  for(byte i=0x00; i<directoryBlock-0x01; i++) root.openNextFile();  //skip to the current entry offset by the index

  //Open the entry
  if(entry = root.openNextFile()) {  //If the entry exists it is returned
    if(entry.isHidden() || (entry.isDirectory() && !DME)) {
      //If it's a directory and we're not in DME mode or file/dir is hidden
      entry.close();  //the entry is skipped over
      ref_openNext(); //and this function is called again
    } else {
      return_reference(); //Send the reference info to the TPDD port
      entry.close();  //Close the entry
    }
    SD_LED_OFF
  } else {
    SD_LED_OFF
    return_blank_reference();
  }
}

void command_open(){  //Opens an entry for reading, writing, or appending
  byte rMode = dataBuffer[(byte)(tail+0x04)];  //The access mode is stored in the 5th byte of the command
  DEBUG_PRINTL(F("command_open()"));
  entry.close();

  if(DME && strcmp(refFileNameNoDir, "PARENT") == 0x00){ //If DME mode is enabled and the reference is for the "PARENT" directory
    upDirectory();  //The top-most entry in the directory buffer is taken away
    directoryDepth--; //and the directory depth index is decremented
  }else{
    directoryAppend(refFileNameNoDir);  //Push the reference name onto the directory buffer
    SD_LED_ON
//    if(DME && (byte)strstr(refFileName, ".<>") != 0x00 && !SD.exists(directory)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
    if(DME && strstr(refFileName, ".<>") != 0x00 && !SD.exists(directory)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
      SD.mkdir(directory);  //create the directory
      upDirectory();
    }else{
      entry=SD.open(directory); //Open the directory to reference the entry
      if(entry.isDirectory()){  //      !!!Moves into a sub-directory
        entry.close();  //If the entry is a directory
        directoryAppend("/"); //append a slash to the directory buffer
        directoryDepth++; //and increment the directory depth index
      }else{  //If the reference isn't a sub-directory, it's a file
        entry.close();
        switch(rMode){
          case 0x01: entry = SD.open(directory, FILE_WRITE); break;             // Write
          case 0x02: entry = SD.open(directory, FILE_WRITE | O_APPEND); break;  // Append
          case 0x03: entry = SD.open(directory, FILE_READ); break;              // Read
        }
        upDirectory();
      }
    }
  }

  if(SD.exists(directory)){ //If the file actually exists...
    SD_LED_OFF
    return_normal(0x00);  //...send a normal return with no error.
  }else{  //If the file doesn't exist...
    SD_LED_OFF
    return_normal(0x10);  //...send a normal return with a "file does not exist" error.
  }
}

void command_close(){ //Closes the currently open entry
  DEBUG_PRINTL(F("command_close()"));
  entry.close();  //Close the entry
  SD_LED_OFF
  return_normal(0x00);  //Normal return with no error
}

void command_read(){  //Read a block of data from the currently open entry
  DEBUG_PRINTL(F("command_read()"));
  SD_LED_ON
  byte bytesRead = entry.read(fileBuffer, FILE_BUFFER_SZ); //Try to pull 128 bytes from the file into the buffer
  SD_LED_OFF
#if DEBUG > 1
  DEBUG_PRINT("A: ");
  DEBUG_PRINTIL(entry.available(),HEX);
#endif
  if(bytesRead > 0x00){  //Send the read return if there is data to be read
    tpddWrite(0x10);  //Return type
    tpddWrite(bytesRead); //Data length
    for(byte i=0x00; i<bytesRead; i++) tpddWrite(fileBuffer[i]);
    tpddSendChecksum();
  }else{
    return_normal(0x3F);  //send a normal return with an end-of-file error if there is no data left to read
  }
}

void command_write(){ //Write a block of data from the command to the currently open entry
  byte commandDataLength = dataBuffer[(byte)(tail+0x03)];

  DEBUG_PRINTL(F("command_write()"));
  SD_LED_ON
  for(byte i=0x00; i<commandDataLength; i++) entry.write(dataBuffer[(byte)(tail+0x04+i)]);
  SD_LED_OFF
  return_normal(0x00);  //Send a normal return to the TPDD port with no error
}

void command_delete(){  //Delete the currently open entry
  DEBUG_PRINTL(F("command_delete()"));
  SD_LED_ON
  entry.close();  //Close any open entries
  directoryAppend(refFileNameNoDir);  //Push the reference name onto the directory buffer
  entry = SD.open(directory, FILE_READ);  //directory can be deleted if opened "READ"

  if(DME && entry.isDirectory()){
    entry.rmdir();  //If we're in DME mode and the entry is a directory, delete it
  }else{
    entry.close();  //Files can be deleted if opened "WRITE", so it needs to be re-opened
    entry = SD.open(directory, FILE_WRITE);
    entry.remove();
  }
  SD_LED_OFF
  upDirectory();
  return_normal(0x00);  //Send a normal return with no error
}

void command_format(){  //Not implemented
  DEBUG_PRINTL(F("command_format()"));
  return_normal(0x00);
}

void command_status(){  //Drive status
  DEBUG_PRINTL(F("command_status()"));
  return_normal(0x00);
}

void command_condition(){ //Not implemented
  DEBUG_PRINTL(F("command_condition()"));
  return_normal(0x00);
}

void command_rename(){  //Renames the currently open entry
  byte refIndex = 0x00;  //Temporary index for the reference name

  DEBUG_PRINTL(F("command_rename()"));

  directoryAppend(refFileNameNoDir);  //Push the current reference name onto the directory buffer

  SD_LED_ON

  if(entry) entry.close(); //Close any currently open entries
  entry = SD.open(directory); //Open the entry
  if(entry.isDirectory()) directoryAppend("/"); //Append a slash to the end of the directory buffer if the reference is a sub-directory

  copyDirectory();  //Copy the directory buffer to the scratchpad directory buffer
  upDirectory();  //Strip the previous directory reference off of the directory buffer

  for(byte i=0x04; i<0x1C; i++){  //Loop through the command's data block, which contains the new entry name
      if(dataBuffer[(byte)(tail+i)]!=0x20 && dataBuffer[(byte)(tail+i)]!=0x00){ //If the current character is not a space (0x20) or null character...
        tempRefFileName[refIndex++]=dataBuffer[(byte)(tail+i)]; //...copy the character to the temporary reference name and increment the pointer.
      }
  }

  tempRefFileName[refIndex]=0x00; //Terminate the temporary reference name with a null character

  if(DME && entry.isDirectory()){ //      !!!If the entry is a directory, we need to strip the ".<>" off of the new directory name
    if(strstr(tempRefFileName, ".<>") != 0x00){
      for(byte i=0x00; i<FILENAME_SZ; i++){
        if(tempRefFileName[i] == '.' || tempRefFileName[i] == '<' || tempRefFileName[i] == '>'){
          tempRefFileName[i]=0x00;
        }
      }
    }
  }

  directoryAppend(tempRefFileName);
  if(entry.isDirectory()) directoryAppend("/");

  DEBUG_PRINTL(directory);
  DEBUG_PRINTL(tempDirectory);
  SD.rename(tempDirectory,directory);  //Rename the entry

  upDirectory();
  entry.close();

  SD_LED_OFF

  return_normal(0x00);  //Send a normal return to the TPDD port with no error
}

/*
 *
 * TS-DOS DME Commands
 *
 */

void command_DMEReq() {  //Send the dmeLabel

  DEBUG_PRINT(F("command_DMEReq(): dmeLabel[")); DEBUG_PRINT(dmeLabel); DEBUG_PRINTL(F("]"));

  if(DME){  // prepend "/" to the root dir label just because my janky-ass setLabel() assumes it
    if (directoryDepth>0x00) setLabel(directory); else setLabel("/SD:   ");
    tpddWrite(0x12);
    tpddWrite(0x0B);
    tpddWrite(0x20);
    for (byte i=0x00 ; i<0x06 ; i++) tpddWrite(dmeLabel[i]);
    tpddWrite('.');
    tpddWrite('<');
    tpddWrite('>');
    tpddWrite(0x20);
    tpddSendChecksum();
  }else{
    return_normal(0x36);
  }
}


/*
 *
 * Power-On
 *
 */

void setup() {
  PINMODE_SD_LED_OUTPUT
  PINMODE_DEBUG_LED_OUTPUT
  //pinMode(WAKE_PIN, INPUT_PULLUP);  // typical, but don't do on RX
  SD_LED_OFF
  DEBUG_LED_OFF
  digitalWrite(LED_BUILTIN,LOW);  // turn standard main led off, besides SD and DEBUG LED macros

  // DSR/DTR
#if defined(DTR_PIN)
  pinMode(DTR_PIN, OUTPUT);
  digitalWrite(DTR_PIN,HIGH);  // tell client we're not ready
#endif // DTR_PIN
#if defined(DSR_PIN)
  pinMode(DSR_PIN, INPUT_PULLUP);
#endif // DSR_PIN

// if debug console enabled, blink led and wait for console to be attached before proceeding
#if DEBUG && defined(CONSOLE)
  CONSOLE.begin(115200);
    while(!CONSOLE){
      DEBUG_LED_ON
      delay(0x20);
      DEBUG_LED_OFF
      delay(0x200);
    }
  CONSOLE.flush();
#endif

  DEBUG_PRINTL(F("setup()"));

  CLIENT.begin(19200);
  CLIENT.flush();

  DEBUG_PRINTL(F("\r\n-----------[ " SKETCH_NAME " " SKETCH_VERSION " ]------------"));
  DEBUG_PRINTL(F("BOARD_NAME: " BOARD_NAME));

  DEBUG_PRINT(F("DSR_PIN: "));
#if defined(DSR_PIN)
  DEBUG_PRINT(DSR_PIN);
#endif // DSR_PIN
  DEBUG_PRINTL("");

  DEBUG_PRINT("DTR_PIN: ");
#if defined(DTR_PIN)
  DEBUG_PRINT(DTR_PIN);
#endif // DTR_PIN
  DEBUG_PRINTL("");

  DEBUG_PRINT("LOADER_FILE: ");
#if defined(LOADER_FILE)
  DEBUG_PRINT(LOADER_FILE);
#endif // LOADER_FILE
  DEBUG_PRINTL("");

  DEBUG_PRINT(F("sendLoader(): "));
#if defined(DSR_PIN) && defined(LOADER_FILE)
  DEBUG_PRINTL(F("enabled"));
#else
  DEBUG_PRINTL(F("disabled"));
#endif // DSR_PIN && LOADER_FILE

  for(byte i=0x00;i<FILE_BUFFER_SZ;++i) dataBuffer[i] = 0x00;

#if defined(USE_SDIO)
 #if DEBUG > 2
 DEBUG_PRINTL(F("Using SDIO"));
 #endif // DEBUG
#else
 #if defined(DISABLE_CS)
  #if DEBUG > 2
   DEBUG_PRINT(F("Disabling SPI device on pin: "));
   DEBUG_PRINTL(DISABLE_CS);
  #endif // DEBUG
   pinMode(DISABLE_CS, OUTPUT);
   digitalWrite(DISABLE_CS, HIGH);
 #else
   #if DEBUG > 2
   DEBUG_PRINTL(F("Assuming the SD is the only SPI device."));  
   #endif // DEBUG
 #endif // DISABLE_CS
 #if DEBUG > 2
  DEBUG_PRINT(F("Using SD chip select pin: "));
  #if defined(SD_CS_PIN)
  DEBUG_PRINT(SD_CS_PIN);
  #endif  // SD_CS_PIN
  DEBUG_PRINTL("");
 #endif  // DEBUG
#endif  // !USE_SDIO

  initCard();

#if defined(DTR_PIN)
  digitalWrite(DTR_PIN,LOW); // tell client we're ready
#endif // DTR_PIN

  // TPDD2-style automatic bootstrap.
  // If client is open already at power-on,
  // then send LOADER.BA instead of going into main loop()
#if defined(DSR_PIN) && defined(LOADER_FILE)
  if(!digitalRead(DSR_PIN)) {
    DEBUG_PRINTL(F("Client is asserting DSR. Doing sendLoader()."));
    sendLoader();
  } else {
    DEBUG_PRINTL(F("Client is not asserting DSR. Doing loop()."));
  }
#endif // DSR_PIN && LOADER_FILE

}


/*
 *
 * Main code loop
 *
 */

void loop() {
  byte rType = 0x00; // Current request type (command type)
  byte rLength = 0x00; // Current request length (command length)
  byte diff = 0x00;  // Difference between the head and tail buffer indexes

  DEBUG_PRINTL(F("loop(): start"));
  state = 0x00; // 0 = waiting for command, 1 = waiting for full command, 2 = have full command

//#if defined(ENABLE_SLEEP)
//  sleepNow();
//#endif // ENABLE_SLEEP
  while(state<0x02){ // While waiting for a command...
#if defined(ENABLE_SLEEP)
    sleepNow();
#endif // ENABLE_SLEEP
    while (CLIENT.available() > 0x00){ // While there's data to read from the TPDD port...
#if defined(ENABLE_SLEEP)
 #if defined(SLEEP_DELAY)
      idleSince = millis();
 #endif // SLEEP_DELAY
#endif // ENABLE_SLEEP
      dataBuffer[head++]=(byte)CLIENT.read();  //...pull the character from the TPDD port and put it into the command buffer, increment the head index...
      if(tail==head)tail++; //...if the tail index equals the head index (a wrap-around has occoured! data will be lost!)
                            //...increment the tail index to prevent the command size from overflowing.

#if DEBUG > 1
      DEBUG_PRINTI((byte)(head-1),HEX);
      DEBUG_PRINT("-");
      DEBUG_PRINTI(tail,HEX);
      DEBUG_PRINTI((byte)(head-tail),HEX);
      DEBUG_PRINT(":");
      DEBUG_PRINTI(dataBuffer[head-1],HEX);
      DEBUG_PRINT(";");
      DEBUG_PRINTL((dataBuffer[head-1]>=0x20)&&(dataBuffer[head-1]<=0x7E)?(char)dataBuffer[head-1]:' ');
#endif
    }

    diff=(byte)(head-tail); //...set the difference between the head and tail index (number of bytes in the buffer)

    if(state == 0x00){ //...if we're waiting for a command...
      if(diff >= 0x04){  //...if there are 4 or more characters in the buffer...
        if(dataBuffer[tail]=='Z' && dataBuffer[(byte)(tail+0x01)]=='Z'){ //...if the buffer's first two characters are 'Z' (a TPDD command)
          rLength = dataBuffer[(byte)(tail+0x03)]; //...get the command length...
          rType = dataBuffer[(byte)(tail+0x02)]; //...get the command type...
          state = 0x01;  //...set the state to "waiting for full command".
        }else if(dataBuffer[tail]=='M' && dataBuffer[(byte)(tail+0x01)]=='1'){ //If a DME command is received
          DME = true; //set the DME mode flag to true
          tail=tail+0x02;  //and skip past the command to the DME request command
        }else{  //...if the first two characters are not 'Z'...
          tail=tail+(tail==head?0x00:0x01); //...move the tail index forward to the next character, stop if we reach the head index to prevent an overflow.
        }
      }
    }

    if(state == 0x01){ //...if we're waiting for the full command to come in...
      if(diff>rLength+0x04){ //...if the amount of data in the buffer satisfies the command length...
          state = 0x02;  //..set the state to "have full command".
        }
    }
  }

#if DEBUG > 1
  DEBUG_PRINTI(tail,HEX); // show the tail index in the buffer where the command was found...
  DEBUG_PRINT("=");
  DEBUG_PRINT("T:"); //...the command type...
  DEBUG_PRINTI(rType, HEX);
  DEBUG_PRINT("|L:");  //...and the command length.
  DEBUG_PRINTI(rLength, HEX);
  DEBUG_PRINTL(DME?'D':'.');
#endif

  switch(rType){  // Select the command handler routine to jump to based on the command type
    case 0x00: command_reference(); break;
    case 0x01: command_open(); break;
    case 0x02: command_close(); break;
    case 0x03: command_read(); break;
    case 0x04: command_write(); break;
    case 0x05: command_delete(); break;
    case 0x06: command_format(); break;
    case 0x07: command_status(); break;
    case 0x08: command_DMEReq(); break; // DME Command
    case 0x0C: command_condition(); break;
    case 0x0D: command_rename(); break;
    default: return_normal(0x36); break;  // Send a normal return with a parameter error if the command is not implemented
  }

#if DEBUG > 1
  DEBUG_PRINTI(head,HEX);
  DEBUG_PRINT(":");
  DEBUG_PRINTI(tail,HEX);
  DEBUG_PRINT("->");
#endif

  tail = tail+rLength+0x05;  // Increment the tail index past the previous command

#if DEBUG > 1
  DEBUG_PRINTIL(tail,HEX);
#endif
}
