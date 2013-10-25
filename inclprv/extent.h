/* 
Copyright (C) 2001 NuSphere Corporation, All Rights Reserved.

This program is open source software.  You may not copy or use this 
file, in either source code or executable form, except in compliance 
with the NuSphere Public License.  You can redistribute it and/or 
modify it under the terms of the NuSphere Public License as published 
by the NuSphere Corporation; either version 2 of the License, or 
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
NuSphere Public License for more details.

You should have received a copy of the NuSphere Public License
along with this program; if not, write to NuSphere Corporation
14 Oak Park, Bedford, MA 01730.
*/

#ifndef EXTENT_H
#define EXTENT_H

/*****************************************************/
/* extent.h - Structure definition for Extent Block. */
/*****************************************************/

#include "bkpub.h"  /* for struct bkhdr */

/* NOTE: The position of the dbVersion and extentType is critical and
 *       must always remain as the first 2 COUNTs following the 
 *       block header of all extent blocks.
 *
 *       All values in on disk blocks MUST BE FULLY ALIGNED.
 */

typedef struct extentBlock
{
    struct bkhdr bk_hdr;    /* standard block header */
    ULONG dbVersion;        /* see dbvers.h */
    ULONG blockSize;        /* Block size in this extent  */
    COUNT extentType;       /* type of extent (see list below) */
    COUNT alignment1;       /* Align words */
    COUNT dateVerification; /* determines which validation dates to use.
                             * array subscript (0 or 1)
                             */
    ULONG creationDate[2];  /* database creation date/time for validation */
    ULONG lastOpenDate[2];  /* database most recent open date/time */
} extentBlock_t;

#if 0  /* currently these values are defined in the mstrblk.h
        * They should be removed from there and moved here.
        * When all references to the mstrblk as the extent header
        * have been cleaned up and this new extent header is 
        * referenced, these values should be defined here and 
        * removed from the mstrblk.h.
        */
/* extentType valid values are: */
#define DBDSMVOID          16      /* database is empty of data */
#define DBMSTR          32      /* this is a multi-file structure file */
#define DBMDB           64      /* this is a multi-file data file */
#define DBMBI           128     /* this is a mult-file bi file */
#define DBMAI           256     /* this is a multi-file ai file */
#define DBMTL           512     /* this is a multi-file tl file */
#endif

#endif  /* #ifndef EXTENT_H */
