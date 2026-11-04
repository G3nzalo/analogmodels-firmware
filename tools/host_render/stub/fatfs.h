/* host stub of the FatFs glue (FATFS/App/fatfs.h), only so the UI code compiles on a PC
 * SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "ff.h"
extern FATFS SDFatFS;
extern FIL SDFile;
extern char SDPath[4];
