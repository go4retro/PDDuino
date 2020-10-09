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
 *  tpdd.cpp: Tandy Portable Disk Drive emulation layer
 *
 */


#include <stdint.h>
#include "config.h"
#include "vfs.h"
#include "logger.h"
#include "powermgmt.h"

#include "tpdd.h"

static VFILEINFO dirinfo;
static byte _checksum =       0;          //Global variable for checksum calculation
static byte _dir_index =      0;          //Current directory block for directory listing
static byte _dir_depth =      0;
static uint8_t _dme_enabled =    false;      //TS-DOS DME mode flag
static sysstate_t _sysstate = SYS_IDLE;
static openmode_t _mode =     OPEN_NONE;

static uint8_t _buffer[DATA_BUFFER_SZ];         //Data buffer for commands
static uint8_t _length;

static char _path[DIRECTORY_SZ] = "/";

// saves 100 bytes of FLASH to have these global
static VFILE _file;                       //Moving file entry for the emulator
static VDIR _dir;                         // information about the current directory item
static char _filename[FILENAME_SZ] = "";  //Reference file name for emulator with no ".<>" if directory
static char _tmpname[FILENAME_SZ] = "";  //Second reference file name for renaming

#define USE_STRING_FUNCS

static uint8_t get_local_path(char *curdir) {
  char ch;
  uint8_t p1 = 0;
  uint8_t p2 = 0;
  uint8_t i;
  uint8_t ret;

  for(i = 0; i < DIRECTORY_SZ; i++) {
    ch = _path[i];
    if(ch == '\0')
      break;
    else if(ch == '/') {
      p2 = p1;
      p1 = i;
    }
  }
  // OK copy into curdir
  if(_path[p2] == '/')
    p2++;   // skip over '/'
  ret = p2;
  i = 0;
  while(p2 < p1 && i < 6 && _path[p2] != '\0') {
    curdir[i++] = _path[p2++];
  }
  curdir[i] = '\0';
  return ret;
}


// Append a string to directory[]
static void append_path(const char* c){
#ifndef USE_STRING_FUNCS
  bool t = false;
  byte i = 0;
  byte j = 0;
#endif

  LOGD_P("%s() entry", __func__);
  LOGD_P("directory (starting) '%s'", _path);

#ifdef USE_STRING_FUNCS
  strcat(_path, c);
#else
  while(_path[i] != '\0') i++;

  while(!t){
    _path[i++] = c[j++];
    t = c[j] == '\0';
  }
#endif

  LOGD_P("directory (ending) '%s'", _path);
  LOGD_P("%s() exit", __func__);
}

// Remove the last path element from directoy[]
static void remove_path(void) {
  uint8_t i = 0;
  uint8_t p1 = 0;
  uint8_t p2 = 0;

  LOGD_P("%s() entry", __func__);
  LOGD_P("directory (starting) '%s'", _path);

  while(_path[i] != '\0') {
    if(_path[i] == '/') {
      p2 = p1;
      p1 = i;
    }
    i++;
  }
  if(p1 + 1 != i) { // filename at end of path
    p2 = p1;
    p1 = i;
  }
  _path[p2 + 1] = '\0';

  LOGD_P("directory (ending) '%s'", _path);
  LOGD_P("%s() exit", __func__);
}


/*
 *
 * TPDD Port misc. routines
 *
 */

static void send_byte(char c){  //Outputs char c to TPDD port and adds to the checksum
  _checksum += c;
  CLIENT.write(c);
  LOGV_P("O:%2.2X(%c)>%2.2X", (uint8_t)c, (c > 0x20 && (uint8_t)c < 0x80 ? c : ' '), _checksum);
}

static void send_buffer(uint8_t *data, uint16_t len) {
  for(uint16_t i = 0; i < len; i++) {
    send_byte(data[i]);
  }
}

static void send_chksum(void) {  //Outputs the checksum to the TPDD port and clears the checksum
  uint8_t chk = _checksum ^ 0xff;
  CLIENT.write(chk);
  LOGV_P("O:%2.2X", chk);
  _checksum = 0;
}

static void send_dir_suffix(void) {
  send_byte('.');   //Tack the expected ".<>" to the end of the name
  send_byte('<');
  send_byte('>');
}


/*
 *
 * TPDD Port return routines
 *
 */

static void send_ret_normal(error_t error){ //Sends a normal return to the TPDD port with error code errorCode

  LOGD_P("%s() entry", __func__);
  LOGD_P("R:Norm %2.2X", error);
  send_byte(RET_NORMAL);  //Return type (normal)
  send_byte(1);  //Data size (1)
  send_byte(error); //Error code
  send_chksum(); //Checksum
  LOGD_P("%s() exit", __func__);
}

static void send_ref(const char *name, uint8_t is_dir, uint32_t size ) {
  uint8_t i, j;

  LOGD_P("%s() entry", __func__);
  LOGI_P("N:'%s':D%d-%d", name, is_dir, size);
  send_byte(RET_DIRECTORY);    //Return type (reference)
  // filename + attribute + length + free sector count
#ifdef ENABLE_TPDD_EXTENSIONS
  if(size > 65535)
    send_byte(FILENAME_SZ + 1 + 4 + 1);    //Data size (1E)
  else
#endif
    send_byte(FILENAME_SZ + 1 + 2 + 1);    //Data size (1C)
  if(name == NULL) {
    for(i = 0; i < FILENAME_SZ; i++)
      send_byte('\0');  //Write the reference file name to the TPDD port
  } else {
    if(is_dir && _dme_enabled) {  // handle dirname.
      for(i = 0; (i < 6) && (name[i] != 0); i++)
        send_byte(name[i]);
      for(;i < 6; i++)
        send_byte(' '); // pad out the dir
      send_dir_suffix();
      j = 9;
    } else {
      for(i = 0; (i < 6) && name[i] && (name[i] != '.'); i++) {
        send_byte(name[i]);
      }
      for(j = i; j < 6; j++) {
        send_byte(' ');
      }
      for(; j < FILENAME_SZ && (name[i] != 0); j++) {
        send_byte(name[i++]);  // send the file extension
      }
    }
    for(; j < FILENAME_SZ; j++) {
      send_byte('\0');  // pad out
    }
  }
  // other implementation send 'F' here:
  //tpddWrite(0);  //Attribute, unused
  send_byte((name == NULL ? '\0' : 'F'));
  send_byte((uint8_t)(size >> 8));  //File size most significant byte
  send_byte((uint8_t)(size)); //File size least significant byte
#ifdef ENABLE_TPDD_EXTENSIONS
  if(size > 65535) {
    send_byte('P');  //Free sectors, SD card has more than we'll ever care about
    //tpddWrite(0x80);  //Free sectors, SD card has more than we'll ever care about
    send_byte((uint8_t)(size >> 24));  //File size most significant byte
    send_byte((uint8_t)(size >> 16)); //File size next most significant byte
  } else
#endif
    //send_byte(0x80);  //Free sectors, SD card has more than we'll ever care about
    // Note: ts-dos only uses the value returned on the last dir entry.
    // and that entry is often empty...
    send_byte(MAX_FREE_SECTORS);  //Free sectors, SD card has more than we'll ever care about
  send_chksum(); //Checksum

  LOGD_P("%s() exit", __func__);
}

static void send_normal_ref(VFILEINFO *info) {  //Sends a reference return to the TPDD port

  LOGD_P("%s() entry", __func__);
  send_ref(info->name, info->attr & VATTR_FOLDER, info->size);
  LOGI_P("R:Ref");
  LOGD_P("%s() exit", __func__);
}

static void send_blank_ref(void) {  //Sends a blank reference return to the TPDD port

  LOGD_P("%s() entry", __func__);
  send_ref(NULL, false, 0);
  LOGI_P("R:BRef");
  LOGD_P("%s() exit", __func__);
}

static void send_parent_ref(void) {

  LOGD_P("%s() entry", __func__);
  send_ref("PARENT", true, 0);
  LOGI_P("R:PRef");
  LOGD_P("%s() exit", __func__);
}


/*
 *
 * TPDD Port command handler routines
 *
 */

// TODO need to remove dirinfo as a global parm
static void ret_next_ref(VDIR *dir, int8_t offset) {
  VRESULT rc;

  LOGD_P("%s() entry", __func__);
  if(_sysstate == SYS_ENUM) {               // We are enumerating the directory
    // TODO we might be able to optimize this for forward motion...
    _dir_index = _dir_index + offset;  // Inc/Dec the directory entry index
    led_sd_on();
    rc = vfs_opendir(dir, _path); //Pull back to the begining of the directory
    for(uint8_t i = 0; (rc == VR_OK) && (i < _dir_index); i++) {
      rc = vfs_readdir(dir, &dirinfo);  //skip to the current entry offset by the index
    }
    while(rc == VR_OK) {  //If the entry exists it is returned
      //Open the entry
      if((dirinfo.attr & VATTR_HIDDEN) || ((dirinfo.attr & VATTR_FOLDER) && !_dme_enabled)) {
        //If it's a directory and we're not in DME mode or file/dir is hidden
        // skip this entry
        rc = vfs_readdir(dir, &dirinfo);  // grab a new one
      } else {
        break;
      }
    } // at this point, we should be at end or at good entry
    vfs_closedir(dir);  //Close the entry
    if(rc == VR_OK) {
      send_normal_ref(&dirinfo); //Send the reference info to the TPDD port
      led_sd_off();
    } else {
      led_sd_off();
      send_blank_ref();
    }
  } else {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_DIR_SEARCH);
  }
  LOGD_P("%s() exit", __func__);
}

static void ret_first_ref(VDIR *dir) {

  LOGD_P("%s() entry", __func__);
  _dir_index = 0;               //Set the current directory entry index to 0
  if(_dme_enabled && _dir_depth > 0) {   //Return the "PARENT.<>" reference if we're in DME mode
    led_sd_off();
    send_parent_ref();
  } else {
    ret_next_ref(dir, 1);    //otherwise we just return the next reference
  }
  LOGD_P("%s() exit", __func__);
}


static bool exists(const char *path) {
  VRESULT rc;
  VFILE f;

  LOGD_P("%s() entry", __func__);

  rc = vfs_open(&f, path, VMODE_READ);
  vfs_close(&f);
  LOGD_P("%s() exit", __func__);
  return (rc == VR_OK || rc == VR_IS_DIRECTORY);
}

static bool fmt_name(char* buf, char* name, uint8_t strip_dir) {
  uint8_t i;
  uint8_t j = 0;
  uint8_t is_dir = false;

  for(i = 0; i < FILENAME_SZ; i++) {  //Put the reference file name into a buffer
    if(strip_dir && i < (FILENAME_SZ - 3) && buf[i] == '.' && buf[i + 1] == '<' && buf[i + 2] == '>') {
      // we're a dir listing, strip off ".<>" and set flag
      is_dir = true;
      break;
    }
    if(buf[i] != ' ') { //If the char pulled from the command is not a space character (0x20)...
      name[j++] = buf[i];     //write it into the buffer and increment the index.
    } else if(buf[i] == '\0') {
      break;
    }
  }
  name[j] = '\0'; //Terminate the file name buffer with a null character
  return is_dir;
}


static void req_reference(VDIR *dir, char *name, uint8_t *is_dir) { // File/Dir Reference command handler
  VFILE f;
  enumtype_t enumtype = (enumtype_t)_buffer[OFFSET_SEARCH_FORM];  //The search form byte exists 0x19 bytes into the command
  VRESULT rc = VR_OK;

  LOGD_P("%s() entry", __func__);

  *is_dir = false;

  LOGD_P("Search Form: %2.2X", enumtype);

  switch(enumtype) {
  case ENUM_PICK:  //Request entry by name
    *is_dir = fmt_name((char *)_buffer, name, _dme_enabled); // remove spaces.

    _sysstate = SYS_REF;

    LOGV_P("Ref: %s", name);

    append_path(name);  //Add the reference to the directory buffer

    led_sd_on();
    rc = vfs_open(&f, _path, VMODE_READ);
    if(rc == VR_OK) {
      send_ref(name, false, f.size);  // it's a file
      vfs_close(&f);
    } else if(rc == VR_IS_DIRECTORY) {
      vfs_close(&f);
      // TODO Should we send back the NoDir version, or the regular version with the ".<>"?
      send_ref(name, true, 0);            // it's a dir
    } else {  //If the file does not exist...
      send_blank_ref();
    }

    remove_path();  //Strip the reference off of the directory buffer

    break;
  case ENUM_FIRST:  //Request first directory block
    _sysstate = SYS_ENUM;
    led_sd_on();
    ret_first_ref(dir);
    break;
  case ENUM_NEXT:   //Request next directory block
    led_sd_on();
    //vfs_closedir(&direntry);
    //vfs_opendir(&direntry, directory);
    ret_next_ref(dir, +1);
    break;
  case ENUM_PREV:
    ret_next_ref(dir, -1);
    //_sysstate = SYS_IDLE;
    //send_ret_normal(ERR_PARM);  //  For now, send some error back
    break;
  default:          //Parameter is invalid
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_PARM);  //Send a normal return to the TPDD port with a parameter error
    break;
  case ENUM_DONE:
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_SUCCESS);  //Send a normal return to the TPDD port with a parameter error
  }
  led_sd_off();
  LOGD_P("%s() exit", __func__);
}


/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE if error
 */
static void req_open(VFILE *f, char *name, uint8_t is_dir) {  //Opens an entry for reading, writing, or appending
  VRESULT rc = VR_OK;
  sysstate_t state;
  uint8_t attr;
  _mode = (openmode_t)_buffer[OFFSET_OPEN_MODE];  //The access mode is stored in the 1st byte of the command payload

  LOGD_P("%s() entry", __func__);

  if(_sysstate == SYS_REF) {
    //vfs_closedir(&direntry);

    if(_dme_enabled && strcmp(name, "PARENT") == 0) { //If DME mode is enabled and the reference is for the "PARENT" directory
      LOGD_P("CHDIR ..");
      remove_path();  //The top-most entry in the directory buffer is taken away
      _dir_depth--; //and the directory depth index is decremented
    } else {
      led_sd_on();
      append_path(name);  //Push the reference name onto the directory buffer
      if(_dme_enabled && is_dir && !exists(_path)){ //If the reference is for a directory and the directory buffer points to a directory that does not exist
        LOGD_P("MKDIR");
        rc = vfs_mkdir(_path);  //create the directory
        // TODO should check rc
        remove_path();
      } else { // something exists.
        rc = vfs_open(f, _path, VMODE_READ);
        vfs_close(f);
        if(rc == VR_IS_DIRECTORY) { // open the directory to reference the entry
          LOGD_P("CHDIR");
          append_path("/"); //append a slash to the directory buffer
          _dir_depth++; //and increment the directory depth index
          rc = VR_OK;
        } else {  //If the reference isn't a sub-directory, it's a file
          LOGD_P("OPEN");
          switch(_mode){
            case OPEN_WRITE:
              // open for write, position at beginning of file, create if needed
              attr =  VMODE_CREATE | VMODE_WRITE;
              state = SYS_WRITE;
              break;                // Write
            case OPEN_APPEND:
              attr =  VMODE_WRITE | VMODE_APPEND;
              state = SYS_WRITE;
              break;                // Append
            case OPEN_READ:
            default:
              attr =  VMODE_READ;
              state = SYS_READ;
              break;                // Read
#ifdef ENABLE_TPDD_EXTENSIONS
            case OPEN_READ_WRITE:   // LaddieAlpha/VirtuaT extension
              attr =  VMODE_CREATE | VMODE_RDWR;
              state = SYS_READ_WRITE;
              break;
#endif
          }
          rc = vfs_open(f, _path, attr);
          if(rc == VR_OK)
            _sysstate = state;
          remove_path();
        }
      }
    }
    led_sd_off();
    send_ret_normal(rc == VR_OK ? ERR_SUCCESS : ERR_NO_FILE);
  } else {  // wrong system state
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_NO_FILE);  //...send a normal return with a "file does not exist" error.

  }
  LOGD_P("%s() exit", __func__);
}

/*
 * System State:  Technically, should only run from SYS_READ, SYS_WRITE, of
 *                or SYS_READ_WRITE, but we'll go ahead and be lenient.
 */
static void req_close(VFILE *f) {  // Closes the currently open entry

  LOGD_P("%s() entry", __func__);
  vfs_close(f);  //Close the entry
  led_sd_off();
  _sysstate = SYS_IDLE;
  send_ret_normal(ERR_SUCCESS);  //Normal return with no error
  LOGD_P("%s() exit", __func__);
}

/*
 * System State:  This can only run from SYS_READ or SYS_READ_WRITE,
 *                and goes to SYS_IDLE if error
 */
static void req_read(VFILE *f){  //Read a block of data from the currently open entry
  VRESULT rc;
  uint32_t read;

  LOGD_P("%s() entry", __func__);
  if((_sysstate == SYS_READ) || (_sysstate == SYS_READ_WRITE)) {
    led_sd_on();
    rc = vfs_read(f, _buffer, FILE_BUFFER_SZ, &read);  //Try to pull 128 bytes from the file into the buffer
    led_sd_off();
    LOGV_P("A: %4X", f->size - f->pos);
    if((rc == VR_OK) && read){  //Send the read return if there is data to be read
      send_byte(RET_READ);  //Return type
      send_byte(read); //Data length
      send_buffer(_buffer, read);
      send_chksum();
    } else { //send a normal return with an end-of-file error if there is no data left to read
      send_ret_normal(ERR_EOF);
    }
  } else if(_sysstate == SYS_WRITE) {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_FMT_MISMATCH);  // trying to read from a write/append file.
  } else {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_NO_NAME);       // no file to reference
  }
  LOGD_P("%s() exit", __func__);
}

/*
 * System State:  This can only run from SYS_WRITE or SYS_READ_WRITE,
 *                and goes to SYS_IDLE if error
 */
static void req_write(VFILE *f){ //Write a block of data from the command to the currently open entry
  VRESULT rc;
  uint32_t written;

  LOGD_P("%s() entry", __func__);
  if((_sysstate == SYS_WRITE) || (_sysstate == SYS_READ_WRITE)) {
    led_sd_on();
    rc = vfs_write(f, _buffer, _length, &written);
    led_sd_off();
    if((rc == VR_OK) && (written == _length)) {
      send_ret_normal(ERR_SUCCESS);   // Send a normal return to the TPDD port with no error
    } else if(rc != VR_OK) { // general error
      send_ret_normal(ERR_DATA_CRC);  // Pick someting for general error
    } else { // didn't store as many bytes as received.
      send_ret_normal(ERR_SEC_NUM);   // Send Sector Number Error (bogus, but...)
    }
  } else if(_sysstate == SYS_READ) {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_FMT_MISMATCH); // trying to write a file opened for reading.
  } else {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_NO_NAME); // no file to reference
  }
  LOGD_P("%s() exit", __func__);
}


/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE afterwards
 */
static void req_delete(VFILE *f, char *name) {  //Delete the currently open entry
  VRESULT rc;

  LOGD_P("%s() entry", __func__);

  if(_sysstate == SYS_REF) {
    led_sd_on();
    vfs_close(f);  //Close any open entries
    append_path(name);  //Push the reference name onto the directory buffer
    rc = vfs_delete(_path);
    // handle error, if any
    // TODO do we need to worry if the name was a dir and DME mode is off?
    led_sd_off();
    remove_path();
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_SUCCESS);  //Send a normal return with no error
  } else {
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_NO_FILE);
  }
  LOGD_P("%s() exit", __func__);
}

static void ret_not_impl(void) {
  _sysstate = SYS_IDLE;
  send_ret_normal(ERR_SUCCESS);
}

static void req_format(void){  //Not implemented

  LOGD_P("%s() entry", __func__);
  ret_not_impl();
  LOGD_P("%s() exit", __func__);
}

static void req_status(void){  //Drive status

  LOGD_P("%s() entry", __func__);
  send_ret_normal(ERR_SUCCESS);
  LOGD_P("%s() exit", __func__);
}

/*
 * System State: This can run from any state, and does not alter state
 */
static void req_condition(void){

  LOGD_P("%s() entry", __func__);
  send_byte(RET_CONDITION);  //Return type (normal)
  send_byte(1);  //Data size (1)
  send_byte(ERR_SUCCESS); //Error code
  send_chksum(); //Checksum
  LOGD_P("%s() exit", __func__);
}

/*
 * System State: This can only run from SYS_REF, and goes to SYS_IDLE afterwards
 */
static void req_rename(char *name) {  //Renames the currently open entry

  LOGD_P("%s() entry", __func__);

  if(_sysstate == SYS_REF) { // we have a file reference to use.

    fmt_name((char *)_buffer, _tmpname, _dme_enabled);

    append_path(name);  //Push the current reference name onto the directory buffer

    led_sd_on();

#ifdef USE_STRING_FUNCS
    strcpy((char *)_buffer, _path);
#else
    for(byte i = 0; i < DIRECTORY_SZ; i++)
      _buffer = _path[i];
#endif
    remove_path();  //Strip the previous directory reference off of the directory buffer

    append_path(_tmpname);
    if(dirinfo.attr & VATTR_FOLDER)
      append_path("/");

    LOGD(_path);
    LOGD((char *)_buffer);
    vfs_rename((char *)_buffer,_path);  //Rename the entry

    remove_path();

    led_sd_off();

    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_SUCCESS);   // Send a normal return to the TPDD port with no error
  } else { // wrong system state
    _sysstate = SYS_IDLE;
    send_ret_normal(ERR_NO_FILE);    // No file to rename
  }
  LOGD_P("%s() exit", __func__);
}

/*
 * Extended Commands
 */
#ifdef ENABLE_TPDD_EXTENSIONS
static void req_seek(VFILE *f) {
  VRESULT rc;
  uint32_t pos = 0;
  int32_t offset;

  LOGD_P("%s() entry", __func__);
  if((_sysstate == SYS_WRITE) || (_sysstate == SYS_READ) || (_sysstate == SYS_READ_WRITE)) {
    if((_length == 5)
        && (_buffer[OFFSET_SEEK_TYPE]) // > 0
        && (_buffer[OFFSET_SEEK_TYPE] < SEEKTYPE_MAX)
       ) {
      // handle seek
      offset = (_buffer[1]
                | (_buffer[2] << 8)
                | ((uint32_t)_buffer[3] << 16)
                | ((uint32_t)_buffer[4] << 24)
               );
      switch(_buffer[OFFSET_SEEK_TYPE]) {
      case SEEKTYPE_SET:
        pos = offset;
        break;
      case SEEKTYPE_CUR:
        pos = f->pos + offset;
        break;
      case SEEKTYPE_END:
        pos = f->size + pos;
        break;
      }
      rc = vfs_seek(f, pos);
      send_ret_normal(ERR_SUCCESS);   // Send a normal return to the TPDD port with no error
    } else {
      // return error
      send_ret_normal(ERR_PARM);
    }
  } else {
    send_ret_normal(ERR_NO_NAME);     // no file opened for reading or writing.

  }
  LOGD_P("%s() exit", __func__);
}


static void req_tell(VFILE *f) {
  uint32_t pos;

  LOGD_P("%s() entry", __func__);
  // Only tell if you have a file open
  if((_sysstate == SYS_WRITE) || (_sysstate == SYS_READ) || (_sysstate == SYS_READ_WRITE)) {
    pos = f->pos;
    send_byte((uint8_t)pos);
    send_byte((uint8_t)(pos >> 8));
    send_byte((uint8_t)(pos >> 16));
    send_byte((uint8_t)(pos >> 24));
    send_chksum();
  } else {
    send_ret_normal(ERR_NO_NAME);     // no file opened for reading or writing.
  }
  LOGD_P("%s() exit", __func__);
}
#endif

// scan dir forwards to get local dir
// scanning backwards also works, but most times, dir path will be short.

/*
 *
 * TS-DOS DME Commands
 *
 */


/*
 * System State: This can run from any state, and does not alter state
 *
 * It appears the .<> at the end need not be send, not the last ' ' char.
 */
static void req_dme_label(void) {  //Send the dmeLabel
  uint8_t i;
  char label[DME_LENGTH] = "v" TOSTRING(APP_VERSION) "." TOSTRING(APP_RELEASE) "." TOSTRING(APP_REVISION);

  LOGD_P("%s() entry", __func__);

  /* as per
   * http://bitchin100.com/wiki/index.php?title=Desklink/TS-DOS_Directory_Access#TPDD_Service_discovery
   * The mere inclusion of this command implies Directory Mode Extensions, so enable
   */
  _dme_enabled = true;
  if (_dir_depth > 0) {
    get_local_path(label);
  }
  send_byte(RET_NORMAL);
  send_byte(11);
  send_byte(0x20);  // not sure if this is a ' ' or magic value
  for (i = 0; i < 6 ; i++)
    send_byte(label[i] ? label[i] : ' ');
  send_dir_suffix();
  send_byte(' ');
  send_chksum();
  LOGD_P("%s() exit", __func__);
}

/*
 * Unknown commands
 */

#ifdef ENABLE_TPDD_EXTENSIONS
//  https://www.mail-archive.com/m100@lists.bitchin100.com/msg11247.html
static void req_unknown_1(void) {

  LOGD_P("%s() entry", __func__);
  send_byte(RET_TSDOS_UNK_1);
  send_byte(0x01);  //Data size (1)
  send_byte(ERR_SUCCESS);
  send_chksum();
  LOGD_P("%s() exit", __func__);
}

static void req_unknown_2(void) {
  const uint8_t data[] = {0x41, 0x10, 0x01, 0x00, 0x50, 0x05, 0x00, 0x02,
                          0x00, 0x28, 0x00, 0xE1, 0x00, 0x00, 0x00
                         };


  LOGD_P("%s() entry", __func__);
  send_byte(RET_UNKNOWN_2);
  send_byte(sizeof(data));
  send_buffer((uint8_t *)data, sizeof(data));
  send_chksum();
  LOGD_P("%s() exit", __func__);
}
#endif



void tpdd_scan(void) {
  cmdstate_t state = IDLE;
  uint8_t i = 0;
  uint8_t data;
  uint8_t cmd = 0; // make the compiler happy
  uint8_t chk = 0;
  unsigned long idleSince = 0;
  uint8_t is_dir = false;

  LOGD_P("%s() entry", __func__);
  dtr_ready();
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
      LOGV_P("S:%2.2d|I:%3.3d|D:%2.2x(%c)", state, i, data, (data > 0x20 && data < 0x7f ? data : ' '));
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
            LOGV_P("T:%2.2X|L:%2.2X|%c", cmd, _length, (_dme_enabled ? 'D' : '.'));
            switch(cmd){  // Select the command handler routine to jump to based on the command type
              case CMD_REFERENCE:   req_reference(&_dir, _filename, &is_dir); break;
              case CMD_OPEN:        req_open(&_file, _filename, is_dir); break;
              case CMD_CLOSE:       req_close(&_file); break;
              case CMD_READ:        req_read(&_file); break;
              case CMD_WRITE:       req_write(&_file); break;
              case CMD_DELETE:      req_delete(&_file, _filename); break;
              case CMD_FORMAT:      req_format(); break;
              case CMD_STATUS:      req_status(); break;
              case CMD_DMEREQ:      req_dme_label(); break; // DME Command
              case CMD_CONDITION:   req_condition(); break;
              case CMD_RENAME:      req_rename(_filename); break;
#ifdef ENABLE_TPDD_EXTENSIONS
              case CMD_SEEK_EXT:    req_seek(&_file); break;
              case CMD_TELL_EXT:    req_tell(&_file); break;
              case CMD_TSDOS_UNK_1: req_unknown_1(); break;
              case CMD_TSDOS_UNK_2: req_unknown_2(); break;
#endif
              default:              send_ret_normal(ERR_PARM); break;  // Send a normal return with a parameter error if the command is not implemented
            }
          } else {
            LOGW_P("Checksum Error: calc(%2.2X) != sent(%2.2X)", chk ^ 0xff, data);
            send_ret_normal(ERR_ID_CRC);  // send back checksum error
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
