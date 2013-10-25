
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
/* Table of Contents -  character manipulation utility functions
 *
 * chrcop  - propagate a character thru a buffer
 * dblank  - remove trailing blanks
 * dlblank - remove leading blanks
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
#include "utcpub.h"

/* PROGRAM: chrcop - propagate a character thru a buffer
 *
 * RETURNS: a ptr to the byte after the last char filled.
 */
TEXT * 
chrcop (
        TEXT   *t,
        TEXT    chr,
        int     n)
{
 
#if OPSYS==WIN32API
    memset ( (char *) t, chr, n );
    return t + n;
}
#else
#if OPSYS==VMS
    lib$movc5 (&0, &0, &chr, &n, (char *) t);
    return t + n;
}
#else
  if (n < 20 )                     /* Not enough to use longfill? */
  {                                /* No. Just do byte by byte    */
    while (--n >= 0) *t++ = chr;
 
    return t;
  }
  else                             /* Use Longs to fill */
  {
        LONG    longchar = 0L ;
        int     i;
        TEXT    *end;
 
    /* fill bytes up to a LONG boundary */
    while(   (int)t & (sizeof(LONG)-1)
          && --n >= 0)
    {   *t++ = chr;  }
 
    end = t + (n & ~(sizeof(LONG)-1) );    /* find the end of longs to zero*/
    n = n & (sizeof(LONG)-1);              /* remember what's left to do */
 
    /* Create the long filler */
    for (i=0;i< (int)sizeof(LONG);i++)
        longchar = (longchar<<8) + (LONG) chr; /* Add chr to each byte. */
 
    /* fill all possible LONGs */
    while( t < end )
    {   *(LONG *)t = longchar;
        t += sizeof(LONG);
    }
 
    /* fill any remaining odd bytes */
    while(--n >= 0) {*t++ = chr;}
    return t;
  }
}
#endif
#endif


/* PROGRAM: dblank -- remove trailing blanks
 *
 * RETURNS: int
 */
int
dblank (TEXT *t)
{
    TEXT    *s = t;
 
    while (*s++);
    --s;
    while (s > t)
        if (*--s == ' ') *s = '\0';
        else {++s; break;}
    return (int) ( s - t);

}  /* end dblank */


/* PROGRAM: dlblank -- remove leading blanks
 *
 * RETURNS: int
 */
int
dlblank (TEXT    *t)
{
    FAST TEXT *s = t;
   
   
    while (*s++ == ' ') ;  /* skip lead spaces*/
    s--;                   /* Went one too far! */
    while (*s != '\0')
        *t++ = *s++;
 
    *t-- = '\0';        /* Copy over null termination and backup t for
                           subsequent count calculation  */
 
    return( (int) (t-s) );

}  /* end dlblank */





