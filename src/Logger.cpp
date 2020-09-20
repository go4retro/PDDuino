/*
 * Logger.cpp
 *
 *  Created on: Sep 18, 2020
 *      Author: brain
 */

#include <stdio.h>
#include <Arduino.h>
#include <avr/pgmspace.h>
#include "config.h"
#include "Logger.h"

#ifndef LOGGER_DECL
  #define LOGGER_DECL
#endif

//#define USE_FLASH
// Using Flash saves 8 bytes of RAM, but costs 10 bytes of Flash
#ifdef USE_FLASH
  #define MEM_ATTR    PROGMEM
  #define GET_LVL     pgm_read_byte(levels + lvl)
#else
  #define MEM_ATTR
  #define GET_LVL     levels[lvl]
#endif

#if defined LOG_LEVEL && LOG_LEVEL > LOG_NONE

static const MEM_ATTR char levels[] = "NEWIDV";

// I tried to combine these functions, or pull out the common code, but
// that all increased code size.

void LOG(uint8_t lvl, const char* fmt, ...) {
char buf[LOG_BUFSIZE];
va_list args;
char ch;

  ch = (lvl < sizeof(levels) ? GET_LVL : '?');
#ifdef LOGGER_TIMESTAMP
  snprintf(buf, LOG_BUFSIZE,"%c (%lu) ", ch, millis());
#else
  snprintf(buf, LOG_BUFSIZE,"%c ", ch);
#endif
  LOGGER.print(buf);
  va_start(args, fmt);
  vsnprintf(buf, LOG_BUFSIZE, fmt, args);
  va_end(args);
  LOGGER.println(buf);
}

void LOG_P(uint8_t lvl, const char* fmt, ...) {
char buf[LOG_BUFSIZE];
va_list args;
char ch;

  ch = (lvl < sizeof(levels) ? GET_LVL : '?');
#ifdef LOGGER_TIMESTAMP
  snprintf(buf, LOG_BUFSIZE,"%c (%lu) ", ch, millis());
#else
  snprintf(buf, LOG_BUFSIZE,"%c ", ch);
#endif
  LOGGER.print(buf);
  va_start(args, fmt);
  vsnprintf_P(buf, LOG_BUFSIZE, fmt, args);
  va_end(args);
  LOGGER.println(buf);
}

void LOG_INIT(void) {
  LOGGER.begin(115200);
    while(!LOGGER){
      led_debug_on();
      delay(0x20);
      led_debug_off();
      delay(0x200);
    }
  LOGGER.flush();
}

#endif

