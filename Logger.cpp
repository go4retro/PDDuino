/*
 * Logger.cpp
 *
 *  Created on: Sep 18, 2020
 *      Author: brain
 */

#include <stdio.h>
#include <Arduino.h>
#include "config.h"
#include "Logger.h"

// TODO  Should move this into a PROGMEM if the platform allows

#if defined LOG_LEVEL && LOG_LEVEL > LOG_NONE
static char levels[] = "NEWIDV";

void LOG(uint8_t lvl, const char* fmt, ...) {
char buf[LOG_BUFSIZE];
va_list args;
char ch;

  ch = (lvl < sizeof(levels) ? levels[lvl] : '?');
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

  ch = (lvl < sizeof(levels) ? levels[lvl] : '?');
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

#endif

