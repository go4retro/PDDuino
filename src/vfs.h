/*
 * vfs.h
 *
 *  Created on: Sep 21, 2020
 *      Author: brain
 */

#ifndef VFS_H_
#define VFS_H_

#include <SdFat.h>

#define VFS_NAMELEN 20

#define  VATTR_NONE     0
#define  VATTR_FOLDER   1
#define  VATTR_HIDDEN   2

typedef struct _VFILE {
  File file;
  uint32_t size;
  uint32_t pos;
} VFILE;

typedef struct _VDIR {
  File dirfile;
  File entry;
} VDIR;

typedef struct _VFILEINFO {
  uint32_t size;
  //WORD fdate;
  //WORD ftime;
  uint8_t attr;
  char name[VFS_NAMELEN];
} VFILEINFO;

typedef enum _VRESULT {
  VR_OK = 0,
  VR_FAILURE = 255
} VRESULT;

#define VMODE_CREATE  O_CREAT
#define VMODE_APPEND  O_APPEND
#define VMODE_READ    O_READ
#define VMODE_WRITE   O_WRITE
#define VMODE_RDWR    O_RDWR


VRESULT vfs_mount(void);
VRESULT vfs_open(VFILE* file, const char *filename, const uint8_t mode);
VRESULT vfs_close(VFILE* file);
VRESULT vfs_read(VFILE* file, void* data, const uint32_t len, uint32_t *read);
VRESULT vfs_write(VFILE* file, const void* data, const uint32_t len, uint32_t *read);
VRESULT vfs_seek(VFILE* file, const uint32_t pos);
VRESULT vfs_opendir (VDIR *dir, const char *path);
VRESULT vfs_readdir (VDIR *dir, VFILEINFO* info);
VRESULT vfs_closedir(VDIR *dir);
VRESULT vfs_mkdir(const char *path);
VRESULT vfs_rename(const char *old_name, const char *new_name);
VRESULT vfs_delete(const char *name);

#endif /* VFS_H_ */
