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
#include "config.h"
#include "Logger.h"
#include "TPDD.h"

#ifndef LOGGER_DECL
  #define LOGGER_DECL
#endif

#if defined(USE_SDIO)
SdFatSdioEX SD;
#else
SdFat SD;
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



sysstate_t _sysstate = SYS_IDLE;

openmode_t _mode = OPEN_NONE;

#define TOKEN_BASIC_END_OF_FILE 0x1A

/*
 *
 * General misc. routines
 *
 */

#if defined LOG_LEVEL && LOG_LEVEL >= LOG_DEBUG
void printDirectory(File dir, byte numTabs) {
  char fileName[FILENAME_SZ] = "";
  uint8_t i;

  SD_LED_ON
  while (entry = dir.openNextFile(O_RDONLY)) {
    if(!entry.isHidden()) {

      entry.getName(fileName,FILENAME_SZ);
      for(i = 0; i < numTabs; i++)
        _buffer[i] = ' ';
      _buffer[i] = '\0';
      if (entry.isDirectory()) {
        LOGD_P("(--------) %s%s/", _buffer, fileName);
        printDirectory(entry, numTabs + 0x01);
      } else {
        LOGD_P("(%8.8lu) %s%s", entry.fileSize(), _buffer, fileName);
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
#endif // ENABLE_SLEEP

void initCard () {

  LOGD_P("%s() entry",__func__);
  while(true) {
    SD_LED_ON
    LOGD_P("Opening SD card..."
#if LOG_LEVEL >= LOG_VERBOSE
  #ifdef USE_SDIO
           "(SDIO)"
  #else
           "(SPI)"
  #endif
#endif
          );
#if defined(USE_SDIO)
    if (SD.begin()) {
#else
 #if defined(SD_CS_PIN) && defined(SD_SPI_MHZ)
    if (SD.begin(SD_CS_PIN,SD_SCK_MHZ(SD_SPI_MHZ))) {
 #elif defined(SD_CS_PIN)
    if (SD.begin(SD_CS_PIN)) {
 #else
    if (SD.begin()) {
 #endif
#endif  // USE_SDIO
      LOGD_P("Card Open");
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
      LOGD_P("No SD card.");
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
#if defined LOG_LEVEL && LOG_LEVEL >= LOG_DEBUG
  LOGD_P("Directory:");
  printDirectory(root,0x00);
  LOGV_P("%lu Bytes Free", SD.vol()->freeClusterCount() * SD.vol()->blocksPerCluster()/2);
#endif
  root.close();

  SD_LED_OFF
  LOGD_P("%s() exit",__func__);
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
#if defined LOG_LEVEL && LOG_LEVEL > LOG_NONE
  int c = 0;
#endif // DEBUG
  File f = SD.open(LOADER_FILE);

  LOGD_P("%s() entry",__func__);
  if (f) {
    SD_LED_ON
    LOGD_P("Sending " LOADER_FILE " ");
      while (f.available()) {
        b = f.read();
        CLIENT.write(b);
#if defined LOG_LEVEL && LOG_LEVEL > LOG_NONE
        if(++c>99) {
          // TODO See if this can be made a little prettier
         LOGD_P(".");
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
    LOGD_P("DONE");
  } else {
    LOGD_P("Could not find " LOADER_FILE " ...");
  }
  LOGD_P("%s() exit",__func__);
  restart(); // go back to normal TPDD emulation mode
}
#endif // LOADER_FILE

// Append a string to directory[]
void directoryAppend(const char* c){
  bool t = false;
  byte i = 0x00;
  byte j = 0x00;

  LOGD_P("%s() entry",__func__);
  LOGD_P("directoryAppend(%s)", c);
  LOGD_P("directory[%s]", directory);

  LOGD_P("->");

  while(directory[i] != 0x00) i++;

  while(!t){
    directory[i++] = c[j++];
    t = c[j] == 0x00;
  }

  LOGD_P("directory[%s]", directory);
  LOGD_P("directoryAppend(%s) end", c);
  LOGD_P("%s() exit",__func__);
}

// Remove the last path element from directoy[]
void upDirectory(){
  byte j = DIRECTORY_SZ;

  LOGD_P("%s() entry",__func__);
  LOGD_P("directory[%s]", directory);

  while(directory[j] == 0x00) j--;
  if(directory[j] == '/' && j!= 0x00) directory[j] = 0x00;
  while(directory[j] != '/') directory[j--] = 0x00;

  LOGD_P("directory[%s]", directory);
  LOGD_P("%s() exit",__func__);
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

  LOGD_P("%s() entry",__func__);
  LOGD_P("setLabel(%s", s);
  LOGD_P("directory[%s]", directory);
  LOGD_P("dmeLabel[%s]", dmeLabel);

  while(s[j] == 0x00) j--;            // seek from end to non-null
  if(s[j] == '/' && j > 0x00) j--;    // seek past trailing slash
  z = j;                              // mark end of name
  while(s[j] != '/' && j > 0x00) j--; // seek to next slash

  // copy 6 chars, up to z or null, space pad
  for(byte i=0x00 ; i<0x06 ; i++) if(s[++j]>0x00 && j<=z) dmeLabel[i] = s[j]; else dmeLabel[i] = 0x20;
  dmeLabel[0x06] = 0x00;

  LOGD_P("dmeLabel[%s]", dmeLabel);
  LOGD_P("%s() exit",__func__);
}



/*
 *
 * TPDD Port misc. routines
 *
 */

void tpddWrite(char c){  //Outputs char c to TPDD port and adds to the checksum
  checksum += c;
  CLIENT.write(c);
  LOGV_P("TPDD Out: %2.2X(%c) | CHK: %2.2X", (uint8_t)c, (c > 0x20 && (uint8_t)c < 0x80 ? c : ' '), checksum);
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

  LOGD_P("%s() entry",__func__);
  LOGD_P("R:Norm %2.2X", errorCode);
  tpddWrite(RET_NORMAL);  //Return type (normal)
  tpddWrite(0x01);  //Data size (1)
  tpddWrite(errorCode); //Error code
  tpddSendChecksum(); //Checksum
  LOGD_P("%s() exit",__func__);
}

void returnReference(const char *name, bool isDir, uint32_t size ) {
  uint8_t i, j;

  LOGD_P("%s() entry",__func__);
  tpddWrite(RET_DIRECTORY);    //Return type (reference)
  // filename + attribute + length + free sector count
  if(size > 65535) {
    tpddWrite(FILENAME_SZ + 1 + 4 + 1);    //Data size (1E)
  } else {
    tpddWrite(FILENAME_SZ + 1 + 2 + 1);    //Data size (1C)
  }
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
  // other implementation send 'F' here:
  //tpddWrite(0);  //Attribute, unused
  tpddWrite((name == NULL ? '\0' : 'F'));
  tpddWrite((uint8_t)(size >> 8));  //File size most significant byte
  tpddWrite((uint8_t)(size)); //File size least significant byte
  if(size > 65535) {
    tpddWrite('P');  //Free sectors, SD card has more than we'll ever care about
    //tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
    tpddWrite((uint8_t)(size >> 24));  //File size most significant byte
    tpddWrite((uint8_t)(size >> 16)); //File size next most significant byte
  } else {
    //tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
    // Note: ts-dos only uses the value returned on the last dir entry.
    // and that entry is often empty...
    tpddWrite(0x9d);  //Free sectors, SD card has more than we'll ever care about
  }
  tpddSendChecksum(); //Checksum

  LOGD_P("%s() exit",__func__);
}

void return_reference(){  //Sends a reference return to the TPDD port

  LOGD_P("%s() entry",__func__);
  entry.getName(tempRefFileName,FILENAME_SZ);  //Save the current file entry's name to the reference file name buffer
  returnReference(tempRefFileName, entry.isDirectory(), entry.fileSize());

  LOGD_P("R:Ref");
  LOGD_P("%s() exit",__func__);
}

void return_blank_reference(){  //Sends a blank reference return to the TPDD port

  LOGD_P("%s() entry",__func__);
  entry.getName(tempRefFileName,FILENAME_SZ);  //Save the current file entry's name to the reference file name buffer
  returnReference(NULL, false, 0);
  LOGD_P("R:BRef");
  LOGD_P("%s() exit",__func__);
}

void return_parent_reference(){

  LOGD_P("%s() entry",__func__);
  returnReference("PARENT", true, 0);
  LOGD_P("%s() exit",__func__);
}

/*
 *
 * TPDD Port command handler routines
 *
 */

void command_reference(){ // File/Dir Reference command handler
  enumtype_t searchForm = (enumtype_t)_buffer[0x19];  //The search form byte exists 0x19 bytes into the command
  byte refIndex = 0x00;  //Reference file name index

  LOGD_P("%s() entry",__func__);

  LOGD_P("SF: %2.2X", searchForm);

  switch(searchForm) {
  case ENUM_PICK:  //Request entry by name
    for(uint8_t i = 0; i < FILENAME_SZ; i++) {  //Put the reference file name into a buffer
      if(_buffer[i] != ' '){ //If the char pulled from the command is not a space character (0x20)...
        refFileName[refIndex++]=_buffer[i];     //write it into the buffer and increment the index.
      }
    }
    refFileName[refIndex] = '\0'; //Terminate the file name buffer with a null character
    _sysstate = SYS_REF;

    LOGV_P("Ref: %s", refFileName);

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
  LOGD_P("%s() exit",__func__);
}

void ref_openFirst(){

  LOGD_P("%s() entry",__func__);
  directoryBlock = 0x00; //Set the current directory entry index to 0
  if(DME && directoryDepth > 0x00) { //Return the "PARENT.<>" reference if we're in DME mode
    SD_LED_OFF
    return_parent_reference();
  }else{
    ref_openNext();    //otherwise we just return the next reference
  }
  LOGD_P("%s() exit",__func__);
}

void ref_openNext(){

  LOGD_P("%s() entry",__func__);
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
  LOGD_P("%s() exit",__func__);
}

/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE if error
 */
void command_open(){  //Opens an entry for reading, writing, or appending
  _mode = (openmode_t)_buffer[0];  //The access mode is stored in the 1st byte of the command payload

  LOGD_P("%s() entry",__func__);

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
              // bug: FILE_WRITE includes O_APPEND, so existing files would be opened
              //      at end.
              //entry = SD.open(directory, FILE_WRITE);

              // open for write, position at beginning of file, create if needed
              entry = SD.open(directory, O_CREAT | O_WRITE);
              _sysstate = SYS_WRITE;
              break;                // Write
            case OPEN_APPEND:
              entry = SD.open(directory, FILE_WRITE | O_APPEND);
              _sysstate = SYS_WRITE;
              break;                // Append
            case OPEN_READ:
            default:
              entry = SD.open(directory, O_READ);
              _sysstate = SYS_READ;
              break;                // Read
            case OPEN_READ_WRITE:   // LaddieAlpha extension
              entry = SD.open(directory, O_CREAT | O_RDWR);
              _sysstate = SYS_READ_WRITE;
              break;
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
  LOGD_P("%s() exit",__func__);
}

/*
 * System State:  Technically, should only run from SYS_READ, SYS_WRITE, of
 *                or SYS_READ_WRITE, but we'll go ahead and be lenient.
 */
void command_close() {  // Closes the currently open entry

  LOGD_P("%s() entry",__func__);
  entry.close();  //Close the entry
  SD_LED_OFF
  _sysstate = SYS_IDLE;
  return_normal(ERR_SUCCESS);  //Normal return with no error
  LOGD_P("%s() exit",__func__);
}

/*
 * System State:  This can only run from SYS_READ or SYS_READ_WRITE,
 *                and goes to SYS_IDLE if error
 */
void command_read(){  //Read a block of data from the currently open entry

  LOGD_P("%s() entry",__func__);
  if((_sysstate == SYS_READ) || (_sysstate == SYS_READ_WRITE)) {
    SD_LED_ON
    byte bytesRead = entry.read(fileBuffer, FILE_BUFFER_SZ); //Try to pull 128 bytes from the file into the buffer
    SD_LED_OFF
    LOGV_P("A: %4X", entry.available());
    if(bytesRead > 0x00){  //Send the read return if there is data to be read
      tpddWrite(RET_READ);  //Return type
      tpddWrite(bytesRead); //Data length
      tpddWriteBuf(fileBuffer, bytesRead);
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
  LOGD_P("%s() exit",__func__);
}

/*
 * System State:  This can only run from SYS_WRITE or SYS_READ_WRITE,
 *                and goes to SYS_IDLE if error
 */
void command_write(){ //Write a block of data from the command to the currently open entry
  int32_t len;

  LOGD_P("%s() entry",__func__);
  if((_sysstate == SYS_WRITE) || (_sysstate == SYS_READ_WRITE)) {
    SD_LED_ON
    len = entry.write(_buffer, _length);
    SD_LED_OFF
    if(len == _length) {
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
  LOGD_P("%s() exit",__func__);
}

/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE afterwards
 */
void command_delete(){  //Delete the currently open entry

  LOGD_P("%s() entry",__func__);

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
  LOGD_P("%s() exit",__func__);
}

void notImplemented(void) {
  _sysstate = SYS_IDLE;
  return_normal(ERR_SUCCESS);
}

void command_format(){  //Not implemented

  LOGD_P("%s() entry",__func__);
  notImplemented();
  LOGD_P("%s() exit",__func__);
}

void command_status(){  //Drive status

  LOGD_P("%s() entry",__func__);
  return_normal(ERR_SUCCESS);
  LOGD_P("%s() exit",__func__);
}

/*
 * System State: This can run from any state, and does not alter state
 */
void command_condition(){

  LOGD_P("%s() entry",__func__);
  tpddWrite(RET_CONDITION);  //Return type (normal)
  tpddWrite(0x01);  //Data size (1)
  tpddWrite(ERR_SUCCESS); //Error code
  tpddSendChecksum(); //Checksum
  LOGD_P("%s() exit",__func__);
}

/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE afterwards
 */
void command_rename(){  //Renames the currently open entry

  LOGD_P("%s() entry",__func__);

  if(_sysstate == SYS_REF) { // we have a file reference to use.

    directoryAppend(refFileNameNoDir);  //Push the current reference name onto the directory buffer

    SD_LED_ON

    if(entry) entry.close(); //Close any currently open entries
    entry = SD.open(directory); //Open the entry
    if(entry.isDirectory()) directoryAppend("/"); //Append a slash to the end of the directory buffer if the reference is a sub-directory

    copyDirectory();  //Copy the directory buffer to the scratchpad directory buffer
    upDirectory();  //Strip the previous directory reference off of the directory buffer

    uint8_t i;
    for(i = 0; i < FILENAME_SZ;i++) {
      if(_buffer[i] == 0 || _buffer[i] == ' ')
        break;
      tempRefFileName[i] = _buffer[i];
    }
    tempRefFileName[i] = '\0'; //Terminate the temporary reference name with a null character

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

    LOGD(directory);
    LOGD(tempDirectory);
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
  LOGD_P("%s() exit",__func__);
}

/*
 * Extended Commands
 */

void command_seek(void) {
  uint32_t pos;

  LOGD_P("%s() entry",__func__);
  if((_sysstate == SYS_WRITE) || (_sysstate == SYS_READ) || (_sysstate == SYS_READ_WRITE)) {
    if((_length == 5)
        && (_buffer[OFFSET_SEEK_TYPE]) // > 0
        && (_buffer[OFFSET_SEEK_TYPE] < SEEKTYPE_MAX)
       ) {
      // handle seek
      pos = (_buffer[1]
            | (_buffer[2] << 8)
            | ((uint32_t)_buffer[3] << 16)
            | ((uint32_t)_buffer[4] << 24)
           );
      switch(_buffer[OFFSET_SEEK_TYPE]) {
      case SEEKTYPE_SET:
        entry.seek(pos);
        break;
      case SEEKTYPE_CUR:
        entry.seekCur(pos);
        break;
      case SEEKTYPE_END:
        entry.seekEnd(pos);
        break;
      }
      return_normal(ERR_SUCCESS);   // Send a normal return to the TPDD port with no error
    } else {
      // return error
      return_normal(ERR_PARM);
    }
  } else {
    return_normal(ERR_NO_NAME);     // no file opened for reading or writing.

  }
  LOGD_P("%s() exit",__func__);
}


void command_tell(void) {
  uint32_t pos;

  LOGD_P("%s() entry",__func__);
  // Only tell if you have a file open
  if((_sysstate == SYS_WRITE) || (_sysstate == SYS_READ) || (_sysstate == SYS_READ_WRITE)) {
    pos = entry.curPosition();
    tpddWrite((uint8_t)pos);
    tpddWrite((uint8_t)(pos >> 8));
    tpddWrite((uint8_t)(pos >> 16));
    tpddWrite((uint8_t)(pos >> 24));
    tpddSendChecksum();
  } else {
    return_normal(ERR_NO_NAME);     // no file opened for reading or writing.
  }
  LOGD_P("%s() exit",__func__);
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

  LOGD_P("%s() entry",__func__);
  LOGD_P("dmeLabel[%s]", dmeLabel);

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
  LOGD_P("%s() exit",__func__);
}

/*
 * Unknown commands
 */

//  https://www.mail-archive.com/m100@lists.bitchin100.com/msg11247.html
void command_unknown_1(void) {

  LOGD_P("%s() entry",__func__);
  tpddWrite(RET_TSDOS_UNK_1);
  tpddWrite(0x01);  //Data size (1)
  tpddWrite(ERR_SUCCESS);
  tpddSendChecksum();
  LOGD_P("%s() exit",__func__);
}

void command_unknown_2(void) {
  const uint8_t data[] = {0x41, 0x10, 0x01, 0x00, 0x50, 0x05, 0x00, 0x02,
                          0x00, 0x28, 0x00, 0xE1, 0x00, 0x00, 0x00
                         };


  LOGD_P("%s() entry",__func__);
  tpddWrite(RET_UNKNOWN_2);
  tpddWrite(sizeof(data));
  tpddWriteBuf((uint8_t *)data, sizeof(data));
  tpddSendChecksum();
  LOGD_P("%s() exit",__func__);
}


/*
 *
 * Power-On
 *
 */

void setup() {
  uint8_t i;


  PINMODE_SD_LED_OUTPUT
  PINMODE_DEBUG_LED_OUTPUT
  //pinMode(WAKE_PIN, INPUT_PULLUP);  // typical, but don't do on RX
  SD_LED_OFF
  DEBUG_LED_OFF
  digitalWrite(LED_BUILTIN,LOW);  // turn standard main led off, besides SD and DEBUG LED macros

  // DSR/DTR
#ifdef DTR_PIN
  pinMode(DTR_PIN, OUTPUT);
  digitalWrite(DTR_PIN,HIGH);  // tell client we're not ready
#endif // DTR_PIN
#ifdef DSR_PIN
  pinMode(DSR_PIN, INPUT_PULLUP);
#endif // DSR_PIN

// if debug console enabled, blink led and wait for console to be attached before proceeding
#if defined LOG_LEVEL && defined(LOGGER)
  LOGGER.begin(115200);
    while(!LOGGER){
      DEBUG_LED_ON
      delay(0x20);
      DEBUG_LED_OFF
      delay(0x200);
    }
  LOGGER.flush();
#endif


  CLIENT.begin(19200);
  CLIENT.flush();

  LOGD_P("%s() entry",__func__);

  LOGD_P("-----------[ " SKETCH_NAME " " SKETCH_VERSION " ]------------");
  LOGD_P("BOARD_NAME: " BOARD_NAME);

#ifdef DTR_PIN
  i = DTR_PIN;
#else
  i = 0;
#endif
  LOGD_P("DTR_PIN: %d", i);
#ifdef DSR_PIN
  i = DSR_PIN;
#else
  i = 0;
#endif
  LOGD_P("DSR_PIN: %d", i);
#ifdef LOADER_FILE
  LOGD_P("DOS Loader File: %s", LOADER_FILE);
#endif

#if defined(DSR_PIN) && defined(LOADER_FILE)
  char state[] = "enabled";
#else
  char state[] = "disabled";
#endif // DSR_PIN && LOADER_FILE
  LOGD_P("sendLoader(): %s", state);

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

  initCard();

#if defined(DTR_PIN)
  digitalWrite(DTR_PIN,LOW); // tell client we're ready
#endif // DTR_PIN

  // TPDD2-style automatic bootstrap.
  // If client is open already at power-on,
  // then send LOADER.BA instead of going into main loop()
#if defined(DSR_PIN) && defined(LOADER_FILE)
  if(!digitalRead(DSR_PIN)) {
    LOGD_P("Client is asserting DSR. Doing sendLoader().");
    sendLoader();
  } else {
    LOGD_P("Client is not asserting DSR. Doing loop().");
  }
#endif // DSR_PIN && LOADER_FILE
  LOGD_P("%s() exit",__func__);
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
  FOUND_OPCODE,
  FOUND_CMD,
  FOUND_LEN,
  FOUND_DATA,
  FOUND_CHK,
  FOUND_MODE,
  FOUND_MODE_DATA,
  FOUND_R
} cmdstate_t;

void loop() {
  cmdstate_t state = IDLE;
  uint8_t i = 0;
  uint8_t data;
  uint8_t cmd = 0; // make the compiler happy
  uint8_t chk = 0;

  LOGD_P("%s() entry",__func__);

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
      data = (uint8_t)CLIENT.read();
      LOGD_P("State: %2.2d | Index: %3.3d | data: %2.2x(%c)", state, i, data, (data > 0x20 && data < 0x7f ? data : ' '));
      // handle this one here, since if someone does ZM, the M should be handled
      // as a new command, same with Za or ZR
      if(state == FOUND_Z) {
        if(data == 'Z') {
          state = FOUND_OPCODE;
        } else
          state = IDLE;
      }
      if(!((state == FOUND_OPCODE) && (data == 'Z'))) { // if the above did not match
        switch (state) {
        case IDLE:
          switch(data) {
          case 'Z':
            state = FOUND_Z;
            break;
          case 'M':
            state = FOUND_MODE;
            break;
          case 'R':
            state = IDLE;  // should be FOUND_R;
            break;
          default:
            //if(data >= 'a' && data <= 'z') {
              // VirtualT goes into command line mode here...
              // TODO This is where one would add the WifiModem stuff..
            //}
            break;
          }
          break;
        case FOUND_Z:
          // this is handled above.
          break;
        case FOUND_OPCODE:
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
          if ((chk ^ 0xff) == data) {
            LOGV_P("T:%2.2X|L:%2.2X|%c", cmd, _length, (DME ? 'D' : '.'));
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
              case CMD_SEEK_EXT: command_seek(); break;
              case CMD_TELL_EXT: command_tell(); break;
              case CMD_TSDOS_UNK_1: command_unknown_1(); break;
              case CMD_TSDOS_UNK_2: command_unknown_2(); break;
              default: return_normal(ERR_PARM); break;  // Send a normal return with a parameter error if the command is not implemented
            }
          } else {
            LOGW_P("Checksum Error: calc(%2.2X) != sent(%2.2X)", chk ^ 0xff, data);
            return_normal(ERR_ID_CRC);  // send back checksum error
          }
          state = IDLE;
          break;
        case FOUND_MODE:
          // 1 = operational mode
          // 0 = FDC emulation mode
          // (ignore for now).
          state = FOUND_MODE_DATA;
          break;
        case FOUND_MODE_DATA:
          // eat the 0x0d that is sent after it.
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
  }
}
