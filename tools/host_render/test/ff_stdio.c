/*
 * ff_stdio.c - the FatFs functions the preset manager uses, on top of stdio and
 * a folder of the PC as the SD card (ffhost_root), for the pattern file test.
 * PC only, not part of the firmware.
 *
 * Part of a custom firmware for the Erica Synths LXR-02 (not an official
 * Erica Synths release). Added 2026.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <unistd.h>
/* FatFs and POSIX both call their directory type DIR */
#define DIR FF_DIR
#include "ff.h"
#undef DIR
#include <dirent.h>

FATFS SDFatFS;
FIL SDFile;
char SDPath[4];

const char* ffhost_root = ".";		/* the SD card */
static char ffhost_cwd[256] = "/";	/* current folder on the card */

static void ffhost_path(char* out, size_t n, const TCHAR* path)
{
	if (path[0] == '/')
		snprintf(out, n, "%s%s", ffhost_root, path);
	else if (path[0] == 0)
		snprintf(out, n, "%s%s", ffhost_root, ffhost_cwd);
	else
		snprintf(out, n, "%s%s%s%s", ffhost_root, ffhost_cwd, ffhost_cwd[strlen(ffhost_cwd) - 1] == '/' ? "" : "/", path);
}

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) { (void)fs; (void)path; (void)opt; return FR_OK; }

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode)
{
	char p[512];
	ffhost_path(p, sizeof p, path);
	FILE* f;
	if (mode & FA_CREATE_ALWAYS) f = fopen(p, "w+b");
	else if (mode & FA_WRITE) { f = fopen(p, "r+b"); if (!f && (mode & FA_OPEN_ALWAYS)) f = fopen(p, "w+b"); }
	else f = fopen(p, "rb");
	fp->fp = f;
	return f ? FR_OK : FR_NO_FILE;
}

FRESULT f_close(FIL* fp)
{
	if (fp->fp) fclose((FILE*)fp->fp);
	fp->fp = NULL;
	return FR_OK;
}

FRESULT f_read(FIL* fp, void* buff, UINT btr, UINT* br)
{
	*br = 0;
	if (!fp->fp) return FR_INVALID_OBJECT;
	*br = (UINT)fread(buff, 1, btr, (FILE*)fp->fp);
	return FR_OK;
}

FRESULT f_write(FIL* fp, const void* buff, UINT btw, UINT* bw)
{
	*bw = 0;
	if (!fp->fp) return FR_INVALID_OBJECT;
	*bw = (UINT)fwrite(buff, 1, btw, (FILE*)fp->fp);
	return *bw == btw ? FR_OK : FR_DISK_ERR;
}

FRESULT f_lseek(FIL* fp, FSIZE_t ofs) { return (fp->fp && !fseek((FILE*)fp->fp, (long)ofs, SEEK_SET)) ? FR_OK : FR_INT_ERR; }
FSIZE_t f_tell(FIL* fp) { return fp->fp ? (FSIZE_t)ftell((FILE*)fp->fp) : 0; }

FRESULT f_opendir(FF_DIR* dp, const TCHAR* path)
{
	char p[512];
	ffhost_path(p, sizeof p, path);
	dp->dir = opendir(p);
	dp->pattern[0] = 0;
	return dp->dir ? FR_OK : FR_NO_PATH;
}

FRESULT f_closedir(FF_DIR* dp)
{
	if (dp->dir) closedir((DIR*)dp->dir);
	dp->dir = NULL;
	return FR_OK;
}

FRESULT f_readdir(FF_DIR* dp, FILINFO* fno)
{
	fno->fname[0] = 0;
	if (!dp->dir) return FR_INVALID_OBJECT;
	struct dirent* e;
	while ((e = readdir((DIR*)dp->dir))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
		if (dp->pattern[0] && fnmatch(dp->pattern, e->d_name, FNM_CASEFOLD)) continue;
		snprintf(fno->fname, sizeof fno->fname, "%s", e->d_name);
		fno->fattrib = (e->d_type == DT_DIR) ? AM_DIR : 0;
		return FR_OK;
	}
	return FR_OK;
}

FRESULT f_findfirst(FF_DIR* dp, FILINFO* fno, const TCHAR* path, const TCHAR* pattern)
{
	FRESULT r = f_opendir(dp, path);
	if (r != FR_OK) { fno->fname[0] = 0; return r; }
	snprintf(dp->pattern, sizeof dp->pattern, "%s", pattern);
	return f_readdir(dp, fno);
}

FRESULT f_findnext(FF_DIR* dp, FILINFO* fno) { return f_readdir(dp, fno); }

FRESULT f_mkdir(const TCHAR* path)
{
	char p[512];
	ffhost_path(p, sizeof p, path);
	return mkdir(p, 0777) ? FR_EXIST : FR_OK;
}

FRESULT f_unlink(const TCHAR* path)
{
	char p[512];
	ffhost_path(p, sizeof p, path);
	return (!unlink(p) || !rmdir(p)) ? FR_OK : FR_NO_FILE;
}

FRESULT f_rename(const TCHAR* path_old, const TCHAR* path_new)
{
	char a[512], b[512];
	struct stat st;
	ffhost_path(a, sizeof a, path_old);
	ffhost_path(b, sizeof b, path_new);
	if (stat(a, &st)) return FR_NO_FILE;
	if (!stat(b, &st)) return FR_EXIST;	/* like FatFs: no replacing */
	return rename(a, b) ? FR_DENIED : FR_OK;
}

FRESULT f_stat(const TCHAR* path, FILINFO* fno)
{
	char p[512];
	struct stat st;
	ffhost_path(p, sizeof p, path);
	if (stat(p, &st)) return FR_NO_FILE;
	if (fno) { fno->fsize = (FSIZE_t)st.st_size; fno->fattrib = S_ISDIR(st.st_mode) ? AM_DIR : 0; }
	return FR_OK;
}

FRESULT f_chdir(const TCHAR* path)
{
	char n[256];
	if (path[0] == '/') snprintf(n, sizeof n, "%s", path);
	else snprintf(n, sizeof n, "%s%s%s", ffhost_cwd, ffhost_cwd[strlen(ffhost_cwd) - 1] == '/' ? "" : "/", path);
	char p[512];
	snprintf(p, sizeof p, "%s%s", ffhost_root, n);
	struct stat st;
	if (stat(p, &st) || !S_ISDIR(st.st_mode)) return FR_NO_PATH;
	snprintf(ffhost_cwd, sizeof ffhost_cwd, "%s", n);
	return FR_OK;
}
