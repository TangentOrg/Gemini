
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

/* Table of Contents - platform memory utilities
 *
 * utmalloc  - Wrapper platform malloc
 * utfree    - Wrapper platform free
 *
 */

/*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include <stdlib.h>

#include "dbconfig.h"
#include "utmpub.h"     /* utmalloc(), utfree() */

/* these entrypoints are #defined to make them unusable elsewhere */
/* in the system.   This defeats that for this routine only       */
#ifdef malloc
#undef malloc
#undef free
#undef calloc
#undef realloc
#endif
 
/* set it to 0 in case anyone #if's */
/* default is off, turn it on in $RDL/local/rh/machine.h for     */
/* central dev environment and during the EARLY stages of a port */
#ifndef STERILIZE
#define STERILIZE 0
#endif
 
#define ROUND8(x) (((x) + (sizeof(double)-1)) & ~(sizeof(double)-1) )

/* Round STZPFX size to a DOUBLE here so that the resulting
 * users's data area stays double word aligned.
 */
#if STERILIZE
#define STZPFX    unsigned
#define STZPFXLEN ((unsigned)ROUND8(sizeof(STZPFX)))
#else
#define STZPFXLEN ((unsigned)0)
#endif
 
#if STERILIZE
DSMVOID
do_sterilize(TEXT       *ptr,
             int         chr,
             unsigned    len)
{
    while(len--) { *ptr++ = chr;}
}
#endif

/* PROGRAM: utmalloc - calls malloc             */
/* RETURNS: a pointer to the allocated storage, */
/*          NULL if not enough is available     */
TEXT *
utmalloc(unsigned  len)    /* size of requested storage */
{
    TEXT     *ptr;
 
#if ALPHAOSF
    /* SMM #94-11-01-010. All civilized versions of malloc let */
    /* you request zero bytes, but not ERGO DPM16. 11/4/94     */
    if ((len + STZPFXLEN) == 0)
        len++;
#endif
 
    ptr = (TEXT *) malloc(len + STZPFXLEN);
#if STERILIZE
    if (ptr==PNULL) return PNULL;
    do_sterilize(ptr, 0xFF, len + STZPFXLEN);
    *(STZPFX *)ptr = len;
    ptr += STZPFXLEN;
#endif
    return ptr;
}
 
/* PROGRAM: utfree - calls the free() routine */
DSMVOID
utfree(TEXT    *ptr)   /* a pointer to the storage to be freed */
{
#if STERILIZE
    ptr -= STZPFXLEN;
    do_sterilize(ptr, 0xFF, (*(STZPFX *)(ptr)) + STZPFXLEN);
#endif
    free(ptr);
}
