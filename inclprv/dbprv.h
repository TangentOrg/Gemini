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

#ifndef DBPRV_H
#define DBPRV_H

#include "rlpub.h"
/**********************************************************/
/* dbprv.h - Private function prototypes for DB subsystem. */
/**********************************************************/

/*** BUM CONSTANT DEFINITIONS (from syscall.h - for compilation purposes) ***/
/* The following definitions were extracted directly from syscall.h */
/* This needs cleanup, desperately. */

#if OPSYS==UNIX
#define OPENMODE  2     /* open for read/write random access */
#define OPENREAD  0     /* open for read only, random access */
#define CREATMODE 0666
#endif  /* UNIX */

#if OPSYS==WIN32API
#if MSC || WC386
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>
#define OPENMODE  O_RDWR | O_BINARY
#define OPENREAD  O_RDONLY | O_BINARY /* open for read only, random access */
#define CREATMODE S_IREAD | S_IWRITE
#endif  /*  MSC || WC386 */
#endif  /* WIN32API */

#define DB_KEYSIZE 400
/*** BUM TEMPORARY FUNCTION PROTOTYPES ***/


/**********************************************************/
/* dbtable.c - function prototypes  */
/**********************************************************/

/**********************************************************/
/* dbtable.c - structures and defines  */
/**********************************************************/
typedef struct dbAutoIncrementNote
{
  RLNOTE  rlnote;
}dbAutoIncrementNote_t;

typedef struct dbAutoIncrementSetNote
{
  RLNOTE  rlnote;
  LONG64  value;
}dbAutoIncrementSetNote_t;


#endif  /* #ifndef DBPRV_H */

