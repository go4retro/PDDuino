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
 *  main.cpp: primary functions for operation
 *
 */

#include <Arduino.h>
#include "config.h"
#include "logger.h"
#include "tpdd.h"
#include "vfs.h"

#if defined(SD_CD_PIN)
  const byte cdInterrupt = digitalPinToInterrupt(SD_CD_PIN);
#endif // SD_CD_PIN

/*
 *
 * General misc. routines
 *
 */

  /* reboot */
  // This hangs at boot if debug console is enabled. It seems to break the CONSOLE port on the way down,
  // and then on the way back up, it sits in setup() waiting for the CONSOLE port.
  // The fix is probaby to reorganize the sketch to avoid needing to restart:
  // https://forum.arduino.cc/index.php?topic=381083.0
  //void setup() { // leave empty }
  //void loop()
  //{
  //  // ... initialize application here
  //  while (// test application exit condition here )
  //  {
  //    // application loop code here
  //  }
  //}
void(* restart) (void) = 0;

#if defined LOG_LEVEL && LOG_LEVEL >= LOG_DEBUG
void print_dir(const char * path, byte numTabs) {
  VDIR dir;
  VRESULT rc = VR_OK;
  char buffer[20];
  char path2[100];

  VFILEINFO entry; //Moving file entry for the emulator
  uint8_t i;

  led_sd_on();
  if(vfs_opendir(&dir, path) == VR_OK) {
    while ((rc = vfs_readdir(&dir, &entry)) == VR_OK) {

      if(!(entry.attr & VATTR_HIDDEN)) {
        for(i = 0; i < numTabs; i++)
          buffer[i] = ' ';
        buffer[i] = '\0';
        if (entry.attr & VATTR_FOLDER) {
          strcpy(path2, path);
          strcat(path2, entry.name);
          strcat(path2,"/");
          LOGD_P("(--------) %s%s/", buffer, entry.name);
          print_dir(path2, numTabs + 1);
        } else {
          LOGD_P("(%8.8lu) %s%s", entry.size, buffer, entry.name);
        }
      }
    }
    vfs_closedir(&dir);
  }
  led_sd_off();
}
#endif // DEBUG

void init_card (void) {

  LOGD_P("%s() entry", __func__);
  while(true) {
    led_sd_on();
    LOGD_P("Opening SD card..."
#if LOG_LEVEL >= LOG_VERBOSE
  #ifdef USE_SDIO
           "(SDIO)"
  #else
           "(SPI)"
  #endif
#endif
          );
    if(vfs_mount() == VR_OK) {
      LOGD_P("Card Open");
#if LED_SD
      led_sd_off();
      delay (0x60);
      led_sd_on();
      delay (0x60);
      led_sd_off();
      delay (0x60);
      led_sd_on();
      delay (0x60);
#endif
      break;
    } else {
      LOGD_P("No SD card.");
#if LED_SD
      led_sd_off();
      delay(1000);
#endif
    }
  }


  // TODO - get the FAT volume label and use it in place of rootLabel
  // ...non-trivial

  // Always do this open() & close(), even if we aren't doing the printDirectory()
  // It's needed to get the SdFat library to put the sd card to sleep.
  #if defined LOG_LEVEL && LOG_LEVEL >= LOG_DEBUG
  LOGD_P("Directory:");
  print_dir("/", 0);
  // The below takes FOREVER!
  //LOGV_P("%lu Bytes Free", fs.vol()->freeClusterCount() * fs.vol()->blocksPerCluster()/2);
  #endif
  led_sd_off();
  LOGD_P("%s() exit", __func__);
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


/* TPDD2-style bootstrap */
void send_loader(void) {
  VFILE f;
  uint8_t buffer[FILE_BUFFER_SZ];
  uint32_t len;
  uint32_t read;
  uint32_t sent = 0;
  uint8_t i;
  VRESULT rc = VR_OK;
  LOGD_P("Address %8.8X", &f);

  LOGD_P("%s() entry", __func__);
  if (vfs_open(&f, LOADER_FILE, VMODE_READ) == VR_OK) {
    led_sd_on();
    len = f.size;
    LOGD_P("Sending " LOADER_FILE ": %d bytes ", len);
    while ((rc = vfs_read(&f, buffer, FILE_BUFFER_SZ, &read)) == VR_OK) {
      for(i = 0; i < read; i++) {
        CLIENT.write(buffer[i]);
        sent++;
        delay(LOADER_SEND_DELAY);
      }
      LOGD_P("%d%% complete", (uint8_t)((sent * 100) / len));
      if(read != sizeof(buffer))
        break;
    }
    if(rc != VR_OK) // error
      LOGE_P(LOADER_FILE " send returned error %d", rc);
    vfs_close(&f);
    led_sd_off();
    CLIENT.flush();
    CLIENT.write(TOKEN_BASIC_END_OF_FILE);
#if defined(BOARD_FEATHERM0)                // no idea why but it works
    // TODO JLB  I'm worried about this.  Seems like a bug...
    delay(250);                             // delay alone before 0x1A didn't work
    CLIENT.write(TOKEN_BASIC_END_OF_FILE);  // needs the double-tap
#endif // BOARD_FEATHERM0
    CLIENT.flush();
    //CLIENT.end();
    LOGD_P("DONE");
  } else {
    LOGD_P("Could not find " LOADER_FILE " ...");
  }
  LOGD_P("%s() exit", __func__);
  //restart(); // go back to normal TPDD emulation mode
}
#endif // LOADER_FILE



/*
 *
 * Power-On
 *
 */

void setup() {
  uint8_t i;
  board_init();

  led_sd_init();
  led_debug_init();
  led_sd_off();
  led_debug_off();

  // interrupt pins
  // pinMode(CLIENT_RX_PIN, INPUT_PULLUP);  // don't do on a HardwareSerial uart pin
#if defined(SD_CD_PIN)
  pinMode(SD_CD_PIN,INPUT_PULLUP);  // card-detect
#endif // SD_CD_PIN

  // DSR/DTR
  dtr_init();
  dsr_init();

  // if debug console enabled, blink led and wait for console to be attached before proceeding
  LOG_INIT();


  CLIENT.begin(19200);
  CLIENT.flush();

  LOGD_P("%s() entry", __func__);

  LOGD_P("----- " APP_NAME " %d.%d.%d on board: " BOARD_NAME, APP_VERSION, APP_RELEASE, APP_REVISION);

  LOGD_P("DTR_PIN: %d", DTR_PIN);
  LOGD_P("DSR_PIN: %d", DSR_PIN);

#if DSR_PIN > -1 && defined(LOADER_FILE)
 #define LOADER_STATE "enabled, " LOADER_FILE
#else
 #define LOADER_STATE "disabled"
#endif // DSR_PIN && LOADER_FILE
  LOGD_P("sendLoader(): " LOADER_STATE);

#if defined(ENABLE_SLEEP)
  LOGD_P("sleep: enabled, delay:" SLEEP_DELAY " ms, "
 #if defined(USE_ALP)
  "ArduinoLowPower.h"
 #else
  "avr/sleep.h"
 #endif // USE_ALP
  );
#else
  LOGD_P("sleep: disabled");
#endif // ENABLE_SLEEP

  LOGD_P("Card-Detect: "
#if defined(SD_CD_PIN)
  "enabled, pin:"
  SD_CD_PIN
  DEBUG_PRINT(F(", intr:"
  DEBUG_PRINT(cdInterrupt
  ", state: "
  digitalRead(SD_CD_PIN)
#else
  "not enabled"
#endif // SD_CD_PIN
  );

#if defined(USE_SDIO)
    LOGV_P("Using SDIO");
#else
  #if defined(DISABLE_CS)
    LOGV_P("Disabling SPI device on pin: %d", DISABLE_CS);
    pinMode(DISABLE_CS, OUTPUT);
    digitalWrite(DISABLE_CS, HIGH);
  #else
    LOGV_P("Assuming the SD is the only SPI device.");
  #endif // DISABLE_CS

  #if defined(SD_CS_PIN)
    i = SD_CS_PIN;
  #else
    i = 0;
  #endif  // SD_CS_PIN
    LOGV_P("Using SD chip select pin: %d", i);
#endif  // !USE_SDIO

  init_card();

  // If the card is ejected, immediately restart, so that we:
  //   go off-line (DTR_PIN)
  //   wait for a card to re-appear
  //   open the new card
  //   finally go back on-line.
  // This comes after initCard(), so that while ejected, we wait in initCard() rather than in a boot loop.
  #if defined(SD_CD_PIN)
    attachInterrupt(cdInterrupt, restart, LOW);
  #endif

  dtr_ready(); // tell client we're ready

  // TPDD2-style automatic bootstrap.
  // If client is open already at power-on,
  // then send LOADER.BA instead of going into main loop()
#if DSR_PIN > -1 && defined(LOADER_FILE)
  if(dsr_is_ready()) {
    LOGD_P("DSR: LOW. Sending DOS loader");
    send_loader();
  } else {
    LOGD_P("DSR HIGH. Run application");
  }
#endif // DSR_PIN && LOADER_FILE
  LOGD_P("%s() exit", __func__);
}


/*
 *
 * Main code loop
 *
 */

void loop() {

  LOGD_P("%s() entry", __func__);
  tpdd_scan();
}
