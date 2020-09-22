/*
 * vfs.h
 *
 *  Created on: Sep 21, 2020
 *      Author: brain
 */

#ifndef VFS_H_
#define VFS_H_

#include <SdFat.h>
#include "logger.h"

// for now.
#define voflag_t oflag_t

#define USE_NEW_LIB
#ifdef USE_NEW_LIB

class VFile {
private:
  bool empty = true;
  File _file;
public:
  VFile(void) {
  }

  VFile(File f) {
    _file = f;
    char name[30];
    _file.getName(name,30);
    LOGD_P("name:%s", name);

    empty = false;
  }

  VFile(const VFile &vf) {
    empty = vf.empty;
    _file = vf._file;
  }

  VFile& operator=(const VFile &vf) {
    if (this == &vf)
      return *this;

    empty = vf.empty;
    _file = vf._file;
    return *this;
  }

  VFile openNextFile(uint8_t mode = FILE_READ) {
    char name[30];
    File f = _file.openNextFile(mode);
    //_file.getName(name,30);
    //LOGD_P("o:%s", name);
    f.getName(name,30);
    LOGD_P("n:%s", name);

    return *(new VFile(f));
  }

  bool isDirectory(void) {
    return _file.isDir();
  }

  bool isHidden(void) {
    return _file.isHidden();
  }

  bool getName(char * name, uint8_t size) {
    return _file.getName(name, size);
  }

  uint32_t size() {
    return _file.size();
  }

  bool close(void) {
    return _file.close();
  }

  operator bool() {
    if(_file) {
    } else {
      char name[30];
      //_file.getName(name,30);
      //LOGD_P("Checking '%s' bool: %d", name, (_file == true));
      LOGD_P("empty: %d", (_file == true));
    }
      //return (!empty && _file);
    return (_file == true);
    }

  int available(void) {
    return _file.available();
  }

  int read(void) {
    return _file.read();
  }

  bool seek(uint32_t pos) {
    return _file.seek(pos);
  }

  bool seekCur(int32_t pos) {
    //return _file.seek(pos + _file.position());
    return _file.seekCur(pos);
  }

  bool seekEnd(int32_t pos) {
    //return _file.seek(pos + _file.size());
    return _file.seekEnd(pos);
  }

  uint32_t position(void) {
    return _file.position();
  }

  bool remove(void) {
    return _file.remove();
  }

  bool rmdir(void) {
    return _file.rmdir();
  }

  int read(void *buf, size_t len) {
    return _file.read(buf, len);
  }

  int write(void* buf, size_t len) {
    return _file.write(buf, len);
  }

  void rewindDirectory() {
    _file.rewindDirectory();
  }
};


class VFS {
private:
#if defined(USE_SDIO)
SdFatSdioEX _fs;
#else
SdFat _fs;
#endif
public:

  VFS(void);
  bool mount(void);
  VFile open(const char *path, voflag_t mode = FILE_READ);

  bool rename(const char* oldPath, const char* newPath) {
    return _fs.rename(oldPath, newPath);
  }

  bool exists(const char* name) {
    return _fs.exists(name);
  }

  bool mkdir(const char* name, bool flag = true) {
    return _fs.mkdir(name, flag);
  }
};
#else
#define VFile File
#if defined(USE_SDIO)
#define VFS SdFatSdioEX
#else
#define VFS SdFat
#endif
#define mount begin
#endif


#endif /* VFS_H_ */
