/*
 *  PDDuino - Arduino-based Tandy Portable Disk Drive emulator
 *  github.com/bkw777/PDDuino
 *  Based on github.com/TangentDelta/SD2TPDD
 *
 *  Copyright (C) 2020  Brian K. White
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 *  powermgmt.cpp: Power management functionality
 *
 */

#include <Arduino.h>
#if defined(ENABLE_SLEEP)
  #if defined(USE_ALP)
    #include <ArduinoLowPower.h>
  #else
    #include <avr/sleep.h>
  #endif // USE_ALP
  #include "config.h"
  #include "powermgmt.h"

#if defined(SLEEP_DELAY)
 unsigned long _now = millis();
 unsigned long _idleSince = now;
#endif // SLEEP_DELAY
#if !defined(USE_ALP)
 const byte rxInterrupt = digitalPinToInterrupt(CLIENT_RX_PIN);
#endif // !USE_ALP

void wakeNow (void) {
}

void sleepNow(void) {
 #if SLEEP_DELAY > 0
  _now = millis();
  if ((_now - _idleSince) < SLEEP_DELAY) return;
  _idleSince = _now;
 #endif // SLEEP_DELAY
 #if defined(DEBUG_SLEEP)
  led_debug_on();
 #endif
 #if defined(USE_ALP)
  LowPower.attachInterruptWakeup(CLIENT_RX_PIN, wakeNow, CHANGE);
 #if defined LOG_LEVEL
  LowPower.idle();  // .idle() .sleep() .deepSleep()
 #else
  LowPower.sleep();  // .idle() .sleep() .deepSleep()
 #endif
 #else
  // if the debug console is enabled, then don't sleep deep enough to power off the usb port
  #if defined LOG_LEVEL && LOG_LEVEL > LOG_NONE
  set_sleep_mode(SLEEP_MODE_STANDBY);  // power down breaks usb serial connection
  #else
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // lightest to deepest: _IDLE _ADC _PWR_SAVE _STANDBY _PWR_DOWN
  #endif // DEBUG
  DEBUG_LED_ON
  attachInterrupt(rxInterrupt,wakeNow,CHANGE);
  sleep_mode();
  detachInterrupt(rxInterrupt);
 #endif // USE_ALP
  DEBUG_LED_OFF
}
#else
#define SLEEP_DELAY 0
#endif


