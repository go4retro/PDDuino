#include <SdFat.h>
#include "config.h"
#include "logger.h"
#include "vfs.h"

#ifdef USE_NEW_LIB

VFS::VFS(void) {
}

bool VFS::mount(void) {
  File root;  //Root file for filesystem reference
#if defined(USE_SDIO)
    if (_fs.begin()) {
#else
 #if defined(SD_CS_PIN) && defined(SD_SPI_MHZ)
    if (_fs.begin(SD_CS_PIN,SD_SCK_MHZ(SD_SPI_MHZ))) {
 #elif defined(SD_CS_PIN)
    if (_fs.begin(SD_CS_PIN)) {
 #else
    if (_fs.begin()) {
 #endif
#endif  // USE_SDIO
      _fs.chvol();
      // Always do this open() & close(), even if we aren't doing the printDirectory()
      // It's needed to get the SdFat library to put the sd card to sleep.
      root = _fs.open("/");
      root.close();
      return true;
    }
    return false;
  }

VFile VFS::open(const char *path, voflag_t mode) {
  return *(new VFile(_fs.open(path, mode)));
}

#endif
