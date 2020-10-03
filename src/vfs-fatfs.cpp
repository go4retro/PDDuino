#include <SdFat.h>
#include "config.h"
#include "logger.h"
#include "vfs.h"

#ifndef NULL
#define NULL null
#endif

#if defined(USE_SDIO)
SdFatSdioEX _fs;
#else
SdFat _fs;
#endif

VRESULT vfs_mount(void) {
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
      return VR_OK;
    }
    return VR_FAILURE;
}


VRESULT vfs_open(VFILE* file, const char *filename, const uint8_t mode) {
  // convert modes
  if(file != NULL) {
    file->file = _fs.open(filename, mode);
    if(file->file)
      return VR_OK;
  }
  return VR_FAILURE;
}


VRESULT vfs_close(VFILE* file) {
  if(file != NULL) {
    return (file->file.close() ? VR_OK : VR_FAILURE);
  }
  return VR_FAILURE;
}


VRESULT vfs_read(VFILE* file, void* data, const uint32_t len, uint32_t *read) {
  int32_t i;

  if(file != NULL) {
    i = file->file.read(data, len);
    if(i >= 0) {
      file->size = file->file.size();
      file->pos = file->file.position();
      return VR_OK;
    } else
      *read = 0;
  }
  return VR_FAILURE;
}

VRESULT vfs_write(VFILE* file, const void* data, const uint32_t len, uint32_t *read) {
  int32_t i;

  if(file != NULL) {
    i = file->file.write(data, len);
    if(i >= 0) {
      *read = i;
      file->size = file->file.size();
      file->pos = file->file.position();
      return VR_OK;
    } else
      *read = 0;
  }
  return VR_FAILURE;
}


VRESULT vfs_seek(VFILE* file, const uint32_t pos) {
  bool rc;

  if(file != NULL) {
    rc = file->file.seek(pos);
    file->pos = file->file.position();
    return (rc ? VR_OK : VR_FAILURE);
  }
  return VR_FAILURE;

}


VRESULT vfs_opendir (VDIR *dir, const char *path) {
  if(dir != NULL) {
    dir->dirfile =  _fs.open(path);
  }
  if(dir->dirfile) {
    return VR_OK;
  }
  return VR_FAILURE;
}


VRESULT vfs_readdir (VDIR *dir, VFILEINFO* info) {
  if(dir != NULL) {
    dir->entry = dir->dirfile.openNextFile();
    if(dir->entry) {
      if(info != NULL) {
        info->size = dir->entry.size();
        //info.attr = dir->direentry.fileAttr();
        dir->entry.getName(info->name,VFS_NAMELEN);
        info->attr = VATTR_NONE;
        if(dir->entry.isDirectory())
          info->attr |= VATTR_FOLDER;
        if(dir->entry.isHidden())
          info->attr |= VATTR_HIDDEN;
        return VR_OK;
      }
    }
  }
  return VR_FAILURE;
}

VRESULT vfs_closedir(VDIR *dir) {
  if(dir->entry)
    dir->entry.close();
  dir->dirfile.close();
  return VR_OK;
}

VRESULT vfs_mkdir(const char *path) {
  _fs.mkdir(path);
  return VR_OK;
}

VRESULT vfs_rename(const char *old_name, const char *new_name) {
  _fs.rename(old_name, new_name);
  return VR_OK;
}

VRESULT vfs_delete(const char *name) {
  File f;
  f = _fs.open(name, O_WRITE);
  if(f)
    f.remove();
  return VR_OK;
}

