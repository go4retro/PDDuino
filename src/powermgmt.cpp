/*
 * sleep.cpp
 *
 *  Created on: Sep 20, 2020
 *      Author: brain
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

unsigned long _idleSince = millis();
  #if !defined(USE_ALP)
const byte wakeInterrupt = digitalPinToInterrupt(WAKE_PIN);
  #endif // !USE_ALP

  #if defined(SLEEP_DELAY)
unsigned long _now = _idleSince;
  #endif // SLEEP_DELAY



void wakeNow (void) {
}

void sleepNow(void) {
 #if SLEEP_DELAY > 0
  _now = millis();
  if ((_now - _idleSince) < SLEEP_DELAY) return;
  _idleSince = _now;
 #endif // SLEEP_DELAY
 #if defined(USE_ALP)
  LowPower.attachInterruptWakeup(WAKE_PIN, wakeNow, CHANGE);
  LowPower.sleep();
 #else
  // if the debug console is enabled, then don't sleep deep enough to power off the usb port
  #if defined LOG_LEVEL && LOG_LEVEL > LOG_NONE
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
#else
#define SLEEP_DELAY 0
#endif


