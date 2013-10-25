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

/* Table of Contents - buffer manipulation utility functions
 *
 *  bufcmp  - compares 2 equal length buffers.
 *  bufcopl - copies exactly n chars from s to t
 *  utblkcp - "quickly" copy a block of LONG's
 *  bufmov  - moves buffer
 *
 */

/*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "utbpub.h"


#define DLLGLOB

#if OPSYS==WIN32API
#define MOVMEM(t, s, n) memmove((char *)(t), (char *)(s), (n))
#define MOVMEMA 1
#endif /* WIN32API */
 
#if OPSYS==VMS
#define MOVMEMA 1
#endif
 

/* PROGRAM: bufcmp - compares 2 equal length buffers.
 *          nulls are treated just like other characters
 *
 * RETURNS: < 0 if t <s
 *          0 if t = s
 *          > 0 if t > s
 */
int 
bufcmp (TEXT *t,
        TEXT *s,
        int   n)
{
    UTEXT  *ut = t;
    UTEXT  *us = s;

    if (n == 0)
        return (0);
    while (*ut++ == *us++)
        if ((--n) == 0)
            return (0);
    return (*--ut > *--us ? 1 : -1);

}  /* end bufcmp */


/* PROGRAM: bufcopl - copies exactly n chars from s to t.
 *
 * RETURNS: DSMVOID
 */
DSMVOID 
bufcopl (
	TEXT	*t,
	TEXT	*s,
	LONG	 n)

#ifdef MOVMEM
{
    if (s > t)
    {
	while ( n >= 32768L )
	{
	    MOVMEM ( t, s, 32767 );
	    s += 32767;
	    t += 32767;
	    n -= 32767;
	}
	if ( n > 0 )
	{
	    MOVMEM ( t, s, (int) n );
	}
    }
    else
    {
	t += n;
	s += n;
	while ( n >= 32768L )
	{
	    s -= 32767;
	    t -= 32767;
	    MOVMEM ( t, s, 32767 );
	    n -= 32767;
	}
	if ( n > 0 )
	{
	    MOVMEM ( t - n, s - n, (int) n );
	}
    }
}
#else
#if OPSYS==VMS
{
	short	 movlen = 65535;

    if (s > t)
    {
	while ( n >= 65536L )
	{   lib$movc3 ( &movlen, (char *) s, (char *) t);
	    s += 65535;
	    t += 65535;
	    n -= 65535;
	}
	if ( n > 0 )
	    lib$movc3 ( &n, (char *) s, (char *) t);
    }
    else
    {
	t += n;
	s += n;
	while ( n >= 65536L )
	{
	    s -= 65535;
	    t -= 65535;
	    lib$movc3 ( &movlen, (char *) s, (char *) t);
	    n -= 65535;
	}
	if ( n > 0 )
	    lib$movc3 ( &n, (char *) s - n, (char *) t - n);
    }
}
#else
{
    if (t < s)	/* left to right */

    {	n++;
	while (--n) *t++ = *s++;
    }
    else
    if (t > s)	/* right to left */
    {	t += n;
	s += n;
	n++;
	while (--n) *--t = *--s;
    }
}
#endif
#endif


/* PROGRAM: utblkcp - "quickly" copy a block of LONG's 
 *
 * RETURNS: DSMVOID
 */
DSMVOID 
utblkcp (
	LONG	*t,
	LONG	*s,
	int	 n) /* number of LONG's, eg BLKSIZE/sizeof(LONG) */
#ifdef MOVMEMA

{
    bufcopl ( (TEXT *) t, (TEXT *) s, (LONG) (n * sizeof(LONG)) );
}

#else
{
    if (t < s)	/* left to right */
    {	n++;
	while (--n) *t++ = *s++;
    }
    else
    if (t > s)	/* right to left */
    {	t += n;
	s += n;
	n++;
	while (--n) *--t = *--s;
    }
}
#endif


/* PROGRAM: bufmov -
 *
 * RETURNS: DSMVOID
 */
DSMVOID 
bufmov (TEXT	*ps,	/* start of buffer */
	    TEXT	*pe,	/* 1 past end of buffer */
	    int	 amt)	    /* shift amount */

#ifdef MOVMEMA
{
    bufcop (ps + amt, ps, pe - ps);
}
#else
{
    TEXT	*pt;	/* target */

    if (amt > 0)
	for (pt = pe + amt; pe > ps; ) *--pt = *--pe;
    else
    if (amt < 0)
	for (pt = ps + amt; ps < pe; ) *pt++ = *ps++;

}  /* end bufmov */
#endif


