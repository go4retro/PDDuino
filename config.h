/*
 * config.h
 *
 *  Created on: Sep 18, 2020
 *      Author: brain
 */

#ifndef CONFIG_H
#define CONFIG_H

/*
 * log activity to serial monitor port
 * Logger is set for 115200 no flow
 * LOG_NONE or undefined: disable serial console
 * LOG_ERROR:
 * LOG_WARN:
 * LOG_INFO: major items of note.
 * LOG_DEBUG: some minor items
 * LOG_VERBOSE: Everything
 *
 * When disabled, the port itself is disabled, which frees up significant ram and cpu cycles on some hardware.
 * Example, on Teensy 3.5/3.6, with the port enabled at all, the sketch must be compiled to run the cpu at a minimum of 24mhz
 * but with the port disabled, the sketch can be compiled to run at only 2Mhz. (Tools -> CPU Speed).
 * Logging will also be disabled if the selected BOARD_* section below doesn't define any LOGGER port.
 */

// disable unless actually debugging, uses more battery, prevents full sleep, hangs at boot until connected
#define LOG_LEVEL     LOG_VERBOSE
//#define LOG_LEVEL   LOG_DEBUG

//#define DEBUG_LED   // use led to see (some) activity even if no DEBUG or CONSOLE
//#define DEBUG_SLEEP // use DEBUG_LED to debug sleepNow()

// File that sendLoader() will try to send to the client.
// Comment out to disable sendLoader() & ignore DSR_PIN
#define LOADER_FILE "LOADER.DO"



////////////////////////////////////////////////////////////////////////////////////////////
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
  #define LOGGER SERIAL_PORT_MONITOR      // where to send debug messages, if enabled. (Serial)
  #define CLIENT SERIAL_PORT_HARDWARE_OPEN // what serial port is the TPDD client connected to (Serial1)
  #define DTR_PIN                   5 // pin to output DTR, github.com/bkw777/MounT uses pin 5
  #define DSR_PIN                   6 // pin to input DSR, github.com/bkw777/MounT uses pin6
  #define SD_CS_PIN                 4 // sd card reader chip-select pin #, usually automatic
  //#define SD_CD_PIN 7                    // sd card reader card-detect pin #, usually none
  //#define DISABLE_CS 10               // Disable other SPI device on this pin, usully none, assume SD is only SPI device
  //#define USE_SDIO                    // sd card reader is connected by SDIO instead of SPI (Teensy 3.5/3.6/4.1)
  #define ENABLE_SLEEP                // sleepNow() while idle for power saving
  #define WAKE_PIN                  0 // CLIENT RX pin#, interrupt is attached to wake from sleepNow()
  //#define SLEEP_DELAY 5000            // Delay in ms before sleeping
  //#define USE_ALP                     // Use ArduinoLowPower library for sleepNow(), otherwise use avr/sleep.h
  void led_sd_init(void)            { pinMode(LED_BUILTIN,OUTPUT); }
  void led_sd_on(void)              { digitalWrite(LED_BUILTIN,HIGH); }
  void led_sd_off(void)             { digitalWrite(LED_BUILTIN,LOW); }
  #define PINMODE_DEBUG_LED_OUTPUT  pinMode(LED_BUILTIN,OUTPUT);
  #define DEBUG_LED_ON              digitalWrite(LED_BUILTIN,HIGH);
  #define DEBUG_LED_OFF             digitalWrite(LED_BUILTIN,LOW);

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
  #define LOGGER SERIAL_PORT_MONITOR
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
  #define LED_SD_ACT_PIN            5
  static void led_sd_init(void)     { DDRB |= _BV(LED_SD_ACT_PIN); }
  static void led_sd_on(void)       { PORTB |= _BV(LED_SD_ACT_PIN); }
  static void led_sd_off(void)      { PORTB &= ~_BV(LED_SD_ACT_PIN); }
  // Main LED: PB5
  #define PINMODE_DEBUG_LED_OUTPUT DDRB = DDRB |= 1UL << 5;
  #define DEBUG_LED_ON PORTB |= _BV(5);
  #define DEBUG_LED_OFF PORTB &= ~_BV(5);

// Adafruit Feather 32u4 Adalogger
// https://learn.adafruit.com/adafruit-feather-32u4-adalogger
//
#elif defined(BOARD_FEATHER32U4)
  #define LOGGER SERIAL_PORT_MONITOR
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
  #define LED_SD_ACT_PIN            4
  static void led_sd_init(void)     { DDRB |= _BV(LED_SD_ACT_PIN); }
  static void led_sd_on(void)       { PORTB |= _BV(LED_SD_ACT_PIN); }
  static void led_sd_off(void)      { PORTB &= ~_BV(LED_SD_ACT_PIN); }
  // Main LED: PC7
  #define PINMODE_DEBUG_LED_OUTPUT DDRC = DDRC |= 1UL << 7;
  #define DEBUG_LED_ON PORTC |= _BV(7);
  #define DEBUG_LED_OFF PORTC &= ~_BV(7);

// Adafruit Feather M0 Adalogger
// https://learn.adafruit.com/adafruit-feather-m0-adalogger
//
#elif defined(BOARD_FEATHERM0)
  #define LOGGER SERIAL_PORT_MONITOR
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
  #define LED_SD_ACT_PIN            8
  static void led_sd_init(void)     { pinMode(LED_SD_ACT_PIN,OUTPUT); }
  static void led_sd_on(void)       { digitalWrite(LED_SD_ACT_PIN,HIGH); }
  static void led_sd_off(void)      { digitalWrite(LED_SD_ACT_PIN,LOW); }
  // Main LED: PA17
  #define PINMODE_DEBUG_LED_OUTPUT pinMode(LED_BUILTIN,OUTPUT);
  #define DEBUG_LED_ON digitalWrite(LED_BUILTIN,HIGH);
  #define DEBUG_LED_OFF digitalWrite(LED_BUILTIN,LOW);

#elif defined(BOARD_UNO)
#ifndef HAVE_HWSERIAL1
  #define LOGGER                    mySerial
  #include <SoftwareSerial.h>
  #define LOGGER_DECL               SoftwareSerial mySerial(7,8); // RX, TX;
#else
  #define LOGGER                    SERIAL_PORT_HARDWARE_OPEN
  #define LOGGER                    SERIAL_PORT_HARDWARE_OPEN
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
  #define LED_SD_ACT_PIN            9
  static void led_sd_init(void)     { pinMode(LED_SD_ACT_PIN, OUTPUT); }
  static void led_sd_on(void)       { digitalWrite(LED_SD_ACT_PIN,HIGH); }
  static void led_sd_off(void)      { digitalWrite(LED_SD_ACT_PIN,LOW); }
  #define LED_DEBUG                 4
  #define PINMODE_DEBUG_LED_OUTPUT  pinMode(LED_DEBUG,OUTPUT);
  #define DEBUG_LED_ON              digitalWrite(LED_DEBUG,HIGH);
  #define DEBUG_LED_OFF             digitalWrite(LED_DEBUG,LOW);

#elif defined(BOARD_MEGA)
  #define LOGGER                    SERIAL_PORT_HARDWARE_OPEN
  #define CLIENT                    SERIAL_PORT_MONITOR
  #define DTR_PIN                   5
  #define DSR_PIN                   6
  #define SD_CS_PIN                 53
  // User-defined platform details
  //#define ENABLE_SLEEP
  //#define WAKE_PIN 0
  //#define SLEEP_DELAY 5000
  //#define USE_ALP
  #define LED_SD_ACT_PIN            9
  static void led_sd_init(void)     { pinMode(LED_SD_ACT_PIN, OUTPUT); }
  static void led_sd_on(void)       { digitalWrite(LED_SD_ACT_PIN,HIGH); }
  static void led_sd_off(void)      { digitalWrite(LED_SD_ACT_PIN,LOW); }
  #define LED_DEBUG                 4
  #define PINMODE_DEBUG_LED_OUTPUT  pinMode(LED_DEBUG,OUTPUT);
  #define DEBUG_LED_ON              digitalWrite(LED_DEBUG,HIGH);
  #define DEBUG_LED_OFF             digitalWrite(LED_DEBUG,LOW);

// example to add a new board
//#elif defined(BOARD_UNO)
//  // User-defined platform details
//  #define LOGGER SERIAL_PORT_MONITOR
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
//  void led_sd_on(void) { digitalWrite(LED_BUILTIN,HIGH); }
//  void led_sd_off(void) { digitalWrite(LED_BUILTIN,LOW); }
//  #define SD_LED_ON digitalWrite(LED_BUILTIN,HIGH);
//  #define SD_LED_OFF digitalWrite(LED_BUILTIN,LOW);
//  #define PINMODE_DEBUG_LED_OUTPUT pinMode(LED_BUILTIN,OUTPUT);
//  #define DEBUG_LED_ON digitalWrite(LED_BUILTIN,HIGH);
//  #define DEBUG_LED_OFF digitalWrite(LED_BUILTIN,LOW);

#endif // BOARD_*

#if !defined(PINMODE_SD_LED_OUTPUT) || !defined(SD_LED_ON) || !defined(SD_LED_OFF)
  #define PINMODE_SD_LED_OUTPUT
  #define led_sd_on() do {} while(0)
  #define led_sd_off() do {} while(0)
  #define SD_LED_ON
  #define SD_LED_OFF
  #define LED_SD 0
  #define SD_LED 0
#else
  #define LED_SD 1
  #define SD_LED 1
#endif

#if !defined(PINMODE_DEBUG_LED_OUTPUT)  || !defined(DEBUG_LED_ON) || !defined(DEBUG_LED_OFF)
  #define PINMODE_DEBUG_LED_OUTPUT
  #define led_debug_on() do {} while(0)
  #define led_debug_off() do {} while(0)
  #define DEBUG_LED_ON
  #define DEBUG_LED_OFF
#endif


#endif /* CONFIG_H_ */
