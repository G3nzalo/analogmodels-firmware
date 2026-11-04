/* host stub of the FatFs API (types and functions), so the UI code compiles on a
 * PC. test/ff_stdio.c implements the functions with stdio for the pattern file test.
 * SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "stm32f7xx_hal.h"
typedef enum { FR_OK = 0, FR_DISK_ERR, FR_INT_ERR, FR_NOT_READY, FR_NO_FILE, FR_NO_PATH, FR_INVALID_NAME,
	FR_DENIED, FR_EXIST, FR_INVALID_OBJECT, FR_WRITE_PROTECTED, FR_INVALID_DRIVE, FR_NOT_ENABLED,
	FR_NO_FILESYSTEM, FR_MKFS_ABORTED, FR_TIMEOUT, FR_LOCKED, FR_NOT_ENOUGH_CORE, FR_TOO_MANY_OPEN_FILES,
	FR_INVALID_PARAMETER } FRESULT;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef uint32_t DWORD;
typedef uint32_t FSIZE_t;
typedef char TCHAR;
typedef struct { int dummy; } FATFS;
typedef struct { void* fp; } FIL;
typedef struct { void* dir; char pattern[64]; } DIR;
typedef struct { FSIZE_t fsize; char fname[256]; char altname[13]; BYTE fattrib; } FILINFO;
#define FA_READ 0x01
#define FA_WRITE 0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_NEW 0x04
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS 0x10
#define AM_DIR 0x10
#define _FS_RPATH 2
FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode);
FRESULT f_close(FIL* fp);
FRESULT f_read(FIL* fp, void* buff, UINT btr, UINT* br);
FRESULT f_write(FIL* fp, const void* buff, UINT btw, UINT* bw);
FRESULT f_lseek(FIL* fp, FSIZE_t ofs);
FSIZE_t f_tell(FIL* fp);
FRESULT f_opendir(DIR* dp, const TCHAR* path);
FRESULT f_closedir(DIR* dp);
FRESULT f_readdir(DIR* dp, FILINFO* fno);
FRESULT f_findfirst(DIR* dp, FILINFO* fno, const TCHAR* path, const TCHAR* pattern);
FRESULT f_findnext(DIR* dp, FILINFO* fno);
FRESULT f_mkdir(const TCHAR* path);
FRESULT f_unlink(const TCHAR* path);
FRESULT f_rename(const TCHAR* path_old, const TCHAR* path_new);
FRESULT f_stat(const TCHAR* path, FILINFO* fno);
FRESULT f_chdir(const TCHAR* path);
FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt);
#define f_unmount(path) f_mount(0, path, 0)
#define _T(x) x
#define _TEXT(x) x
