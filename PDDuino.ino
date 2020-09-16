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

#define JIM_MODS  // Just so I can find the next item when it is commented out.
#define JIM_NEW_LOOP

File root;  //Root file for filesystem reference
File entry; //Moving file entry for the emulator
File tempEntry; //Temporary entry for moving files

#ifndef JIM_NEW_LOOP
byte head = 0x00;  //Head index
byte tail = 0x00;  //Tail index
byte dataBuffer[DATA_BUFFER_SZ]; //Data buffer for commands
#endif

byte checksum = 0x00;  //Global variable for checksum calculation

bool DME = false; //TS-DOS DME mode flag

uint8_t _buffer[DATA_BUFFER_SZ]; //Data buffer for commands
uint8_t _length;

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

typedef enum command_e {
  CMD_REFERENCE =     0x00,
  CMD_OPEN =          0x01,
  CMD_CLOSE =         0x02,
  CMD_READ =          0x03,
  CMD_WRITE =         0x04,
  CMD_DELETE =        0x05,
  CMD_FORMAT =        0x06,
  CMD_STATUS =        0x07,
  CMD_DMEREQ =        0x08,
  CMD_SEEK =          0x09,
  CMD_TELL =          0x0a,
  CMD_SET_EXT =       0x0b,
  CMD_CONDITION =     0x0c,
  CMD_RENAME =        0x0d,
  CMD_REQ_QUERY_EXT = 0x0e,
  CMD_COND_LIST =     0x0f,

  RET_READ =          0x10,
  RET_DIRECTORY =     0x11,
  RET_NORMAL =        0x12,
  RET_CONDITION =     0x15,
  RET_DIR_EXT =       0x1e, // From LaddieAlpha

  CMD_TSDOS_UNK_2 =   0x23,
  CMD_UNKNOWN_48 =    0x30, // https://www.mail-archive.com/m100@lists.bitchin100.com/msg12129.html
  CMD_TSDOS_UNK_1 =   0x31  // https://www.mail-archive.com/m100@lists.bitchin100.com/msg11244.html
} command_t;

typedef enum error_e {
  ERR_SUCCESS =       0x00,
  ERR_NO_FILE =       0x10,
  ERR_EXISTS =        0x11,
  ERR_NO_NAME =       0x30, // command.tdd calls this EOF
  ERR_DIR_SEARCH =    0x31,
  ERR_BANK =          0x35,
  ERR_PARM =          0x36,
  ERR_FMT_MISMATCH =  0x37,
  ERR_EOF =           0x3f,
  ERR_NO_START =      0x40, // command.tdd calls this IO ERROR (64) (empty name error)
  ERR_ID_CRC =        0x41,
  ERR_SEC_LEN =       0x42,
  ERR_FMT_VERIFY =    0x44,
  ERR_FMT_INTRPT =    0x46,
  ERR_ERASE_OFFSET =  0x47,
  ERR_DATA_CRC =      0x49,
  ERR_SEC_NUM =       0x4a,
  ERR_READ_TIMEOUT =  0x4b,
  ERR_SEC_NUM2 =      0x4d,
  ERR_WRITE_PROTECT = 0x50, // writing to a locked file
  ERR_DISK_NOINIT =   0x5e,
  ERR_DIR_FULL =      0x60, // command.tdd calls this disk full, as does the SW manual
  ERR_DISK_FULL =     0x61,
  ERR_FILE_LEN =      0x6e,
  ERR_NO_DISK =       0x70,
  ERR_DISK_CHG =      0x71
} error_t;

typedef enum openmode_e {
  OPEN_NONE =         0x00,
  OPEN_WRITE =        0x01,
  OPEN_APPEND =       0x02,
  OPEN_READ =         0x03
} openmode_t;

typedef enum sysstate_e {
  SYS_IDLE,
  SYS_ENUM,
  SYS_REF,
  SYS_WRITE,
  SYS_READ,
  SYS_PUSH_FILE,
  SYS_PULL_FILE,
  SYS_PAUSED
} sysstate_t;

typedef enum enumtype_e {
  ENUM_PICK =         0x00,
  ENUM_FIRST =        0x01,
  ENUM_NEXT =         0x02,
  ENUM_PREV =         0x03,
  ENUM_DONE =         0x04
} enumtype_t;

sysstate_t _sysstate = SYS_IDLE;

openmode_t _mode = OPEN_NONE;

#define TOKEN_BASIC_END_OF_FILE 0x1A

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

unsigned long idleSince = millis();
#if defined(ENABLE_SLEEP)
 #if !defined(USE_ALP)
  const byte wakeInterrupt = digitalPinToInterrupt(WAKE_PIN);
 #endif // !USE_ALP

 #if defined(SLEEP_DELAY)
  unsigned long now = idleSince;
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
    CLIENT.write(TOKEN_BASIC_END_OF_FILE);
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
void directoryAppend(const char* c){
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
void setLabel(const char* s) {
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
  DEBUG_PRINT(F("Sent:"));
  DEBUG_PRINTIL((byte)c,HEX);

}

void tpddWriteBuf(uint8_t *data, uint16_t len) {
  for(uint16_t i = 0; i < len; i++) {
    tpddWrite(data[i]);
  }
}

void tpddWriteString(char* c){  //Outputs a null-terminated char array c to the TPDD port
  byte i = 0x00;
  while(c[i] != '\0'){
    tpddWrite(c[i++]);
  }
}

void tpddSendChecksum(){  //Outputs the checksum to the TPDD port and clears the checksum
  CLIENT.write(checksum ^ 0xFF);
  checksum = 0x00;
}


/*
 *
 * TPDD Port return routines
 *
 */

void return_normal(error_t errorCode){ //Sends a normal return to the TPDD port with error code errorCode
  DEBUG_PRINTL(F("return_normal()"));
#if DEBUG > 1
  DEBUG_PRINT("R:Norm ");
  DEBUG_PRINTIL(errorCode, HEX);
#endif
  tpddWrite(RET_NORMAL);  //Return type (normal)
  tpddWrite(0x01);  //Data size (1)
  tpddWrite(errorCode); //Error code
  tpddSendChecksum(); //Checksum
}

void returnReference(const char *name, bool isDir, uint16_t size ) {
  uint8_t i, j;

  DEBUG_PRINTL(F("returnReference()"));

  tpddWrite(RET_DIRECTORY);    //Return type (reference)
  tpddWrite(0x1C);    //Data size (1C)
  if(name == NULL) {
    for(i = 0; i < FILENAME_SZ; i++)
      tpddWrite(0x00);  //Write the reference file name to the TPDD port
  } else {
    if(isDir && DME) { // handle dirname.
      for(i = 0; (i < 6) && (name[i] != 0); i++)
        tpddWrite(name[i]);
      for(;i < 6; i++)
        tpddWrite(' '); // pad out the dir
      tpddWrite('.');  //Tack the expected ".<>" to the end of the name
      tpddWrite('<');
      tpddWrite('>');
      j = 9;
    } else {
      for(i = 0; (i < 6) && (name[i] != '.'); i++) {
        tpddWrite(name[i]);
      }
      for(j = i; j < 6; j++) {
        tpddWrite(' ');
      }
      for(; j < FILENAME_SZ && (name[i] != 0); j++) {
        tpddWrite(name[i++]);  // send the file extension
      }
    }
    for(; j < FILENAME_SZ; j++) {
      tpddWrite(0);  // pad out
    }
  }
  tpddWrite(0);  //Attribute, unused
  tpddWrite((uint8_t)(size >> 8));  //File size most significant byte
  tpddWrite((uint8_t)(size & 0xFF)); //File size least significant byte
  tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum

}

#define JIM_NEW_RETURN_REF

void return_reference(){  //Sends a reference return to the TPDD port
#ifdef JIM_NEW_RETURN_REF
  DEBUG_PRINTL(F("return_reference()"));
  entry.getName(tempRefFileName,FILENAME_SZ);  //Save the current file entry's name to the reference file name buffer
  returnReference(tempRefFileName, entry.isDirectory(), entry.fileSize());
#else
  byte term = 0x06;
  bool terminated = false;
  DEBUG_PRINTL(F("return_reference()"));

  tpddWrite(RET_DIRECTORY);  //Return type (reference)
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
#endif
#if DEBUG > 1
  DEBUG_PRINTL("R:Ref");
#endif
}

void return_blank_reference(){  //Sends a blank reference return to the TPDD port
  DEBUG_PRINTL(F("return_blank_reference()"));
#ifdef JIM_NEW_RETURN_REF
  entry.getName(tempRefFileName,FILENAME_SZ);  //Save the current file entry's name to the reference file name buffer
  returnReference(NULL, false, 0);
#else
  tpddWrite(RET_DIRECTORY);    //Return type (reference)
  tpddWrite(0x1C);    //Data size (1C)

  for(byte i=0x00; i<FILENAME_SZ; i++) tpddWrite(0x00);  //Write the reference file name to the TPDD port

  tpddWrite(0x00);    //Attribute, unused
  tpddWrite(0x00);    //File size most significant byte
  tpddWrite(0x00);    //File size least significant byte
  tpddWrite(0x80);    //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum
#endif
#if DEBUG > 1
  DEBUG_PRINTL("R:BRef");
#endif
}

void return_parent_reference(){
  DEBUG_PRINTL(F("return_parent_reference()"));
#ifdef JIM_NEW_RETURN_REF
  returnReference("PARENT", true, 0);
#else
  tpddWrite(0x11);    // return type
  tpddWrite(0x1C);    // data size

  tpddWriteString("PARENT.<>");
  for(byte i=0x09; i<FILENAME_SZ; i++) tpddWrite(0x00);

  tpddWrite(0x00);    //Attribute, unused
  tpddWrite(0x00);    //File size most significant byte
  tpddWrite(0x00);    //File size least significant byte
  tpddWrite(0x80);    //Free sectors, SD card has more than we'll ever care about
  tpddSendChecksum(); //Checksum
#endif
}

/*
 *
 * TPDD Port command handler routines
 *
 */

void command_reference(){ // File/Dir Reference command handler
#ifdef JIM_NEW_LOOP
  enumtype_t searchForm = (enumtype_t)_buffer[0x19];  //The search form byte exists 0x19 bytes into the command
#else
  enumtype_t searchForm = dataBuffer[(byte)(tail+0x1D)];  //The search form byte exists 29 bytes into the command
#endif
  byte refIndex = 0x00;  //Reference file name index

  DEBUG_PRINTL(F("command_reference()"));

#if DEBUG > 1
  DEBUG_PRINT("SF:");
  DEBUG_PRINTIL(searchForm,HEX);
#endif

  switch(searchForm) {
  case ENUM_PICK:  //Request entry by name
#ifdef JIM_NEW_LOOP
    for(uint8_t i = 0; i < FILENAME_SZ; i++) {  //Put the reference file name into a buffer
      if(_buffer[i] != ' '){ //If the char pulled from the command is not a space character (0x20)...
        refFileName[refIndex++]=_buffer[i];     //write it into the buffer and increment the index.
      }
    }
    refFileName[refIndex] = '\0'; //Terminate the file name buffer with a null character
#else
    for(byte i=0x04; i<0x1C; i++){  //Put the reference file name into a buffer
      if(dataBuffer[(tail+i)]!=0x20){ //If the char pulled from the command is not a space character (0x20)...
        refFileName[refIndex++]=dataBuffer[(tail+i)]; //write it into the buffer and increment the index.
      }
    }
    refFileName[refIndex]=0x00; //Terminate the file name buffer with a null character
#endif
    _sysstate = SYS_REF;

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

    break;
  case ENUM_FIRST:  //Request first directory block
    _sysstate = SYS_ENUM;
    SD_LED_ON
    root.close();
    root = SD.open(directory);
    ref_openFirst();
    break;
  case ENUM_NEXT:   //Request next directory block
    SD_LED_ON
    root.close();
    root = SD.open(directory);
    ref_openNext();
    break;
  case ENUM_PREV:
    // TODO really should back up the dir one...
    _sysstate = SYS_IDLE;
    return_normal(ERR_PARM);  //  For now, send some error back
    break;
  default:          //Parameter is invalid
    _sysstate = SYS_IDLE;
    return_normal(ERR_PARM);  //Send a normal return to the TPDD port with a parameter error
    break;
  case ENUM_DONE:
    _sysstate = SYS_IDLE;
    return_normal(ERR_SUCCESS);  //Send a normal return to the TPDD port with a parameter error
  }
  SD_LED_OFF
}

void ref_openFirst(){
  DEBUG_PRINTL(F("ref_openFirst()"));
  directoryBlock = 0x00; //Set the current directory entry index to 0
  if(DME && directoryDepth > 0x00) { //Return the "PARENT.<>" reference if we're in DME mode
    SD_LED_OFF
    return_parent_reference();
  }else{
    ref_openNext();    //otherwise we just return the next reference
  }
}

void ref_openNext(){
  DEBUG_PRINTL(F("ref_openNext()"));

  if(_sysstate == SYS_ENUM) {   // We are enumerating the directory
    directoryBlock++; //Increment the directory entry index
    SD_LED_ON
    root.rewindDirectory(); //Pull back to the begining of the directory
    for(uint8_t i = 0x00; i < directoryBlock - 0x01; i++)
      root.openNextFile();  //skip to the current entry offset by the index

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
  } else {
    _sysstate = SYS_IDLE;
    return_normal(ERR_DIR_SEARCH);
  }
}

/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE if error
 */
void command_open(){  //Opens an entry for reading, writing, or appending
#ifdef JIM_NEW_LOOP
  _mode = (openmode_t)_buffer[0];  //The access mode is stored in the 1st byte of the command payload
#else
  _mode = (openmode_t)dataBuffer[(byte)(tail+0x04)];  //The access mode is stored in the 5th byte of the command
#endif
  DEBUG_PRINTL(F("command_open()"));

  if(_sysstate == SYS_REF) {
    entry.close();

    if(DME && strcmp(refFileNameNoDir, "PARENT") == 0x00) { //If DME mode is enabled and the reference is for the "PARENT" directory
      upDirectory();  //The top-most entry in the directory buffer is taken away
      directoryDepth--; //and the directory depth index is decremented
    } else {
      directoryAppend(refFileNameNoDir);  //Push the reference name onto the directory buffer
      SD_LED_ON
  //    if(DME && (byte)strstr(refFileName, ".<>") != 0x00 && !SD.exists(directory)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
      if(DME && strstr(refFileName, ".<>") != 0x00 && !SD.exists(directory)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
        SD.mkdir(directory);  //create the directory
        upDirectory();
      } else {
        entry = SD.open(directory); //Open the directory to reference the entry
        if(entry.isDirectory()){  //      !!!Moves into a sub-directory
          entry.close();  //If the entry is a directory
          directoryAppend("/"); //append a slash to the directory buffer
          directoryDepth++; //and increment the directory depth index
        } else {  //If the reference isn't a sub-directory, it's a file
          entry.close();
          switch(_mode){
            case OPEN_WRITE:
              entry = SD.open(directory, FILE_WRITE);
              _sysstate = SYS_WRITE;
              break;                // Write
            case OPEN_APPEND:
              entry = SD.open(directory, FILE_WRITE | O_APPEND);
              _sysstate = SYS_WRITE;
              break;                // Append
            case OPEN_READ:
            default:
              entry = SD.open(directory, FILE_READ);
              _sysstate = SYS_READ;
              break;                // Read
          }
          upDirectory();
        }
      }
    }

    if(SD.exists(directory)) { //If the file actually exists...
      SD_LED_OFF
      return_normal(ERR_SUCCESS);  //...send a normal return with no error.
    } else {  //If the file doesn't exist...
      SD_LED_OFF
      return_normal(ERR_NO_FILE);  //...send a normal return with a "file does not exist" error.
    }
  } else {  // wrong system state
    _sysstate = SYS_IDLE;
    return_normal(ERR_NO_FILE);  //...send a normal return with a "file does not exist" error.

  }
}

/*
 * System State:  Technically, should only run from SYS_READ or SYS_WRITE, but
 *                we'll go ahead and be lenient.
 */
void command_close() {  // Closes the currently open entry
  DEBUG_PRINTL(F("command_close()"));
  entry.close();  //Close the entry
  SD_LED_OFF
  _sysstate = SYS_IDLE;
  return_normal(ERR_SUCCESS);  //Normal return with no error
}

/*
 * System State: This can only run from SYS_READ, and goes to SYS_IDLE if error
 */
void command_read(){  //Read a block of data from the currently open entry
  DEBUG_PRINTL(F("command_read()"));
  if(_sysstate == SYS_READ) {
    SD_LED_ON
    byte bytesRead = entry.read(fileBuffer, FILE_BUFFER_SZ); //Try to pull 128 bytes from the file into the buffer
    SD_LED_OFF
  #if DEBUG > 1
    DEBUG_PRINT("A: ");
    DEBUG_PRINTIL(entry.available(),HEX);
  #endif
    if(bytesRead > 0x00){  //Send the read return if there is data to be read
      tpddWrite(RET_READ);  //Return type
      tpddWrite(bytesRead); //Data length
      tpddWriteBuf(fileBuffer,bytesRead);
      tpddSendChecksum();
    } else { //send a normal return with an end-of-file error if there is no data left to read
      return_normal(ERR_EOF);
    }
  } else if(_sysstate == SYS_WRITE) {
    _sysstate = SYS_IDLE;
    return_normal(ERR_FMT_MISMATCH);  // trying to read from a write/append file.
  } else {
    _sysstate = SYS_IDLE;
    return_normal(ERR_NO_NAME);       // no file to reference
  }
}

/*
 * System State: This can only run from SYS_WRITE, and goes to SYS_IDLE if error
 */
void command_write(){ //Write a block of data from the command to the currently open entry
  int32_t len;

#ifndef JIM_NEW_LOOP
  byte commandDataLength = dataBuffer[(byte)(tail+0x03)];
#endif
  DEBUG_PRINTL(F("command_write()"));
  if(_sysstate == SYS_WRITE) {
    SD_LED_ON
    #ifdef JIM_NEW_LOOP
      len = entry.write(_buffer, _length);
    #else
      len = entry.write(&(dataBuffer[tail+0x04]), commandDataLength);
      //for(byte i=0x00; i<commandDataLength; i++) entry.write(dataBuffer[(byte)(tail+0x04+i)]);
    #endif
    SD_LED_OFF
#ifdef JIM_NEW_LOOP
    if(len == _length) {
#else
    if(len == commandDataLength) {
#endif
      return_normal(ERR_SUCCESS);   // Send a normal return to the TPDD port with no error
    } else if(len < 0) { // general error
      return_normal(ERR_DATA_CRC);  // Pick someting for general error
    } else { // didn't store as many bytes as received.
      return_normal(ERR_SEC_NUM);   // Send Sector Number Error (bogus, but...)
    }
  } else if(_sysstate == SYS_READ) {
    _sysstate = SYS_IDLE;
    return_normal(ERR_FMT_MISMATCH); // trying to write a file opened for reading.
  } else {
    _sysstate = SYS_IDLE;
    return_normal(ERR_NO_NAME); // no file to reference
  }
}

/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE afterwards
 */
void command_delete(){  //Delete the currently open entry
  DEBUG_PRINTL(F("command_delete()"));

  if(_sysstate == SYS_REF) {
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
    _sysstate = SYS_IDLE;
    return_normal(ERR_SUCCESS);  //Send a normal return with no error
  } else {
    _sysstate = SYS_IDLE;
    return_normal(ERR_NO_FILE);
  }
}

void notImplemented(void) {
  _sysstate = SYS_IDLE;
  return_normal(ERR_SUCCESS);
}

//  https://www.mail-archive.com/m100@lists.bitchin100.com/msg11247.html
void command_unknown(void) {
  return_normal(ERR_SUCCESS);
}

void command_format(){  //Not implemented
  DEBUG_PRINTL(F("command_format()"));
  notImplemented();
}

void command_status(){  //Drive status
  DEBUG_PRINTL(F("command_status()"));
  return_normal(ERR_SUCCESS);
}

void command_condition(){ //Not implemented
  DEBUG_PRINTL(F("command_condition()"));
  notImplemented();
}

/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE afterwards
 */
void command_rename(){  //Renames the currently open entry
#ifndef JIM_NEW_LOOP
  byte refIndex = 0x00;  //Temporary index for the reference name
#endif

  DEBUG_PRINTL(F("command_rename()"));

  if(_sysstate == SYS_REF) { // we have a file reference to use.

    directoryAppend(refFileNameNoDir);  //Push the current reference name onto the directory buffer

    SD_LED_ON

    if(entry) entry.close(); //Close any currently open entries
    entry = SD.open(directory); //Open the entry
    if(entry.isDirectory()) directoryAppend("/"); //Append a slash to the end of the directory buffer if the reference is a sub-directory

    copyDirectory();  //Copy the directory buffer to the scratchpad directory buffer
    upDirectory();  //Strip the previous directory reference off of the directory buffer

  #ifdef JIM_NEW_LOOP
    uint8_t i;
    for(i = 0; i < FILENAME_SZ;i++) {
      if(_buffer[i] == 0 || _buffer[i] == ' ')
        break;
      tempRefFileName[i] = _buffer[i];
    }
    tempRefFileName[i] = '\0'; //Terminate the temporary reference name with a null character
  #else
    for(byte i=0x04; i<0x1C; i++){  //Loop through the command's data block, which contains the new entry name
      if(dataBuffer[(byte)(tail+i)]!=0x20 && dataBuffer[(byte)(tail+i)]!=0x00){ //If the current character is not a space (0x20) or null character...
        tempRefFileName[refIndex++]=dataBuffer[(byte)(tail+i)]; //...copy the character to the temporary reference name and increment the pointer.
      }
    }
    tempRefFileName[refIndex]=0x00; //Terminate the temporary reference name with a null character
  #endif


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

    _sysstate = SYS_IDLE;
    return_normal(ERR_SUCCESS);   // Send a normal return to the TPDD port with no error
  } else { // wrong system state
    _sysstate = SYS_IDLE;
    return_normal(ERR_NO_FILE);    // No file to rename
  }
}

/*
 *
 * TS-DOS DME Commands
 *
 */

/*
 * System State: This can run from any state, and does not alter state
 */
void command_DMEReq() {  //Send the dmeLabel

  DEBUG_PRINT(F("command_DMEReq(): dmeLabel[")); DEBUG_PRINT(dmeLabel); DEBUG_PRINTL(F("]"));

  /* as per
   * http://bitchin100.com/wiki/index.php?title=Desklink/TS-DOS_Directory_Access#TPDD_Service_discovery
   * The mere inclusion of this command implies Directory Mode Extensions, so enable
   */
  DME = true;
  // prepend "/" to the root dir label just because my janky-ass setLabel() assumes it
  if (directoryDepth>0x00) setLabel(directory); else setLabel("/SD:   ");
  tpddWrite(RET_NORMAL);
  tpddWrite(0x0B);
  tpddWrite(0x20);
  for (byte i=0x00 ; i<0x06 ; i++) tpddWrite(dmeLabel[i]);
  tpddWrite('.');
  tpddWrite('<');
  tpddWrite('>');
  tpddWrite(' ');
  tpddSendChecksum();
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

#ifndef JIM_NEW_LOOP
  for(byte i=0x00;i<FILE_BUFFER_SZ;++i) dataBuffer[i] = 0x00;
#endif

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

// 0 = waiting for command, 1 = waiting for full command, 2 = have full command
typedef enum cmdstate_s {
  IDLE,
  CMD_STARTED,
  CMD_COMPLETE,
  FOUND_Z,
  FOUND_Z2,
  FOUND_CMD,
  FOUND_LEN,
  FOUND_DATA,
  FOUND_CHK,
  FOUND_MODE,
} cmdstate_t;

void loop() {
#ifdef JIM_NEW_LOOP
  cmdstate_t state = IDLE;
  uint8_t i = 0;
  uint8_t data;
  uint8_t cmd = 0; // make the compiler happy
  uint8_t chk;

  DEBUG_PRINTL(F("loop(): start"));

  while(true) {
  #if defined(ENABLE_SLEEP)
    sleepNow();
  #endif // ENABLE_SLEEP
    if(millis() - idleSince > 500) { // interval between cmd element > ./5s
      state = IDLE; // go back to IDLE state.
      idleSince = millis();
    }
    // should check for a timeout...
    while(CLIENT.available()) {
      idleSince = millis();  // reset timer.
      #if defined(ENABLE_SLEEP)
       #if defined(SLEEP_DELAY)
        idleSince = millis();
       #endif // SLEEP_DELAY
      #endif // ENABLE_SLEEP
      data = (uint8_t)CLIENT.read();
      #if DEBUG > 1
        DEBUG_PRINTI((uint8_t)state, HEX);
        DEBUG_PRINT(":");
        DEBUG_PRINTI((uint8_t)i, HEX);
        DEBUG_PRINT(" - ");
        DEBUG_PRINTI((uint8_t)data, HEX);
        if(data > 0x20 && data < 0x7f) {
          DEBUG_PRINT(";");
          DEBUG_PRINT((char)data);
        }
        DEBUG_PRINTL("");
      #endif
      switch (state) {
      case IDLE:
        if(data == 'Z')
          state = FOUND_Z;
        else if(data == 'M')
          state = FOUND_MODE;
        break;
      case FOUND_Z:
        if(data == 'Z') {
          state = FOUND_Z2;

        } else
          state = IDLE;
        break;
      case FOUND_Z2:
        cmd = data;
        chk = data;
        state = FOUND_CMD;
        break;
      case FOUND_CMD:
        _length = data;
        chk += data;
        i = 0;
        if(_length)
          state = FOUND_LEN;
        else
          state = FOUND_DATA;
        break;
      case FOUND_LEN:
        if(i < _length) {
          chk += data;
          _buffer[i++] = data;
        }
        if(i == _length) {  // cmd is complete.  get checksum
          state = FOUND_DATA;
        }
        break;
      case FOUND_DATA: // got checksum.  Check and exec
        if (chk ^ 0xff == data) {
          #if DEBUG > 1
            DEBUG_PRINT("T:"); //...the command type...
            DEBUG_PRINTI(cmd, HEX);
            DEBUG_PRINT("|L:");  //...and the command length.
            DEBUG_PRINTI(_length, HEX);
            DEBUG_PRINTL(DME ? 'D' : '.');
          #endif
          switch(cmd){  // Select the command handler routine to jump to based on the command type
            case CMD_REFERENCE: command_reference(); break;
            case CMD_OPEN:      command_open(); break;
            case CMD_CLOSE:     command_close(); break;
            case CMD_READ:      command_read(); break;
            case CMD_WRITE:     command_write(); break;
            case CMD_DELETE:    command_delete(); break;
            case CMD_FORMAT:    command_format(); break;
            case CMD_STATUS:    command_status(); break;
            case CMD_DMEREQ:    command_DMEReq(); break; // DME Command
            case CMD_CONDITION: command_condition(); break;
            case CMD_RENAME: command_rename(); break;
            case CMD_TSDOS_UNK_1: command_unknown(); break;
            default: return_normal(ERR_PARM); break;  // Send a normal return with a parameter error if the command is not implemented
          }
        } else {
          #if DEBUG > 1
            DEBUG_PRINT("Checksum Error: calc="); //...the command type...
            DEBUG_PRINTI(chk ^ 0xff, HEX);
            DEBUG_PRINT(", sent=");  //...and the command length.
            DEBUG_PRINTI(data, HEX);
          #endif
        }
        state = IDLE;
        break;
      case FOUND_MODE:
        // 1 = operational mode
        // 0 = FDC emulation mode
        // (ignore for now).
        _sysstate = SYS_IDLE;
        state = IDLE;
        break;
      default:
        // not sure how you'd get here, but...
        _sysstate = SYS_IDLE;
        state = IDLE;
        break;
      }
    }
  }
#else
  command_t rType = CMD_REFERENCE; // Current request type (command type)
  byte rLength = 0x00; // Current request length (command length)
  byte diff = 0x00;  // Difference between the head and tail buffer indexes

  DEBUG_PRINTL(F("loop(): start"));

//#if defined(ENABLE_SLEEP)
//  sleepNow();
//#endif // ENABLE_SLEEP
  while(state != CMD_COMPLETE){ // While waiting for a command...
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
    case CMD_REFERENCE: command_reference(); break;
    case CMD_OPEN:      command_open(); break;
    case CMD_CLOSE:     command_close(); break;
    case CMD_READ:      command_read(); break;
    case CMD_WRITE:     command_write(); break;
    case CMD_DELETE:    command_delete(); break;
    case CMD_FORMAT:    command_format(); break;
    case CMD_STATUS:    command_status(); break;
    case CMD_DMEREQ:    command_DMEReq(); break; // DME Command
    case CMD_CONDITION: command_condition(); break;
    case CMD_RENAME: command_rename(); break;
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
#endif
}
