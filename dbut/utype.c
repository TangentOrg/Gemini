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

/* Table of Contents - type conversion utility functions:
 *
 * atoin        - converts n chars starting at s to integer and 
 *                returns the integer
 * countl       - converts LONG to COUNT
 * hextoa       - converts hex to ascii
 * intoal       - intoal converts n into characters in s
 * intoall      - intoal converts longlong (64 bit) n into characters in s
 * utintnn      - convert an integer to 2 byte character form
 * utintbbn     - convert an integer to 3 byte character form
 * utitoa       - convert integer to ascii string
 * reversen     - same as reverse, but string length is passed in
 * utConvertBytes - given a value in bytes convert it to kb, mb, gb, or tb
 */

/*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "utspub.h"   /* for stlen */
#include "uttpub.h"

/* PROGRAM: countl - converts LONG to COUNT
 *
 * RETURNS: COUNT
 */
COUNT
countl (LONG    lnum)
{
    return (COUNT)lnum;
}

/* PROGRAM: hextoa - converts hex to ascii
 *
 * RETURNS: DSMVOID
 */
DSMVOID
hextoa(TEXT *ps, 
       TTINY *ph, 
       int len)
{
      TTINY  hdig;
        int  i;
    for (i =0; i < len; i++  )
    {
        hdig = ph[i] >> 4;
        *ps++ = hdig < 10 ? '0' + hdig : 'A' + (hdig - 10);
        hdig  = ph[i] & 0x0f;
        *ps++ = hdig < 10 ? '0' + hdig : 'A' + (hdig - 10);
    }
    *ps = '\0';
}

/* PROGRAM: utintnn - convert an integer to 2 byte character form
 *
 * RETURNS pointer to next byte after converted bytes
 */
TEXT *
utintnn(TEXT *pbuf, /* put result chars here */
        int i)      /* convert this integer  */
{
    *pbuf++ = '0' + (i/10) % 10;
    *pbuf++ = '0' + i % 10;
    return pbuf;
}

/* PROGRAM: utintbbn - convert an integer to 3 byte character form
 *
 * RETURNS pointer to next byte after converted bytes
 */
TEXT *
utintbbn(TEXT    *pbuf,  /* put result chars here */
         int      i)     /* convert this integer  */
{
    if (i>99)
         *pbuf++ = '0' + (i/100) % 10;
    else *pbuf++ = ' ';
 
    if (i>9)
         *pbuf++ = '0' + (i/10) % 10;
    else *pbuf++ = ' ';
 
    *pbuf++ = '0' + i % 10;
    return pbuf;
}

/* PROGRAM: utitoa - convert integer to ascii string
 *
 * RETURNS: DSMVOID
 */
DSMVOID
utitoa(LONG    n,
       TEXT    s[])
{
FAST    COUNT   i,temp;
FAST    LONG   sign;
 
    if ((sign = n) < 0) /* record sign */
        n = -n;         /* make n positive */
    i = 0;
    do {        /* generate digits in reverse order */
        temp = (COUNT) (n % 10) + (COUNT) '0';    /* get next digit */
        s[i++] = (TEXT) temp;
    } while ((n /= 10) > 0); /* delete it */
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    reversen(s,i);
}

/* PROGRAM: reversen - same as reverse, but string length is passed in
 *                     Not double byte enabled.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
reversen (TEXT    *ps,     /* Reverses chars in place */
          int     len)     /* length of string ps */

{
    TEXT    *pt;
    TEXT     c ;
 
    for (pt = ps + len; --pt > ps; ps++)
    {
        c = *ps;
        *ps = *pt;
        *pt = c;
    }
}

/* PROGRAM: atoin converts n chars starting at s to integer and returns
 *          the integer. It stops and returns when non-numeric data found.
 *
 * RETURNS: Integer value
 */
LONG
atoin (TEXT    *ps,
       int      n)
{
        GBOOL     sign = 0;
FAST    LONG     ival = 0;
 
    while((*ps == ' ' || *ps == '\n' || *ps == '\t') && (--n > 0))
        ++ps;
 
    if (n > 0)
    switch(*ps)
    {case '-':  sign = 1;
     case '+':  ps++;
                --n;
    }
 
    while((--n >= 0) && (*ps >= '0') && (*ps <= '9'))
        ival = 10 * ival + *ps++ - '0';
 
    if (sign) ival = -ival;
 
    return ival;
}

/* PROGRAM: intoal converts n into characters in s. s must be big enough.
 *          It also adds the null term char.
 *
 * RETURNS:
 */
int
intoal(LONG        n,
       TEXT       *s)
{
    FAST TEXT   *ps = s;
    FAST GBOOL    minus;
   
    if (n < -2147483647)
    {
        /* Maximum negative number.  Changing the
         * sign will not work so handle it explicitly
         */
        ps = (TEXT *)stcopy(ps, (TEXT *)"-2147483648");
        *ps = '\0';
        return ((int) (ps - s));
    }
 
    if ((minus = (n < 0))) n = -n;
    do {
        *ps++ = n % 10 + '0';
    }
#if SYS5REL2
   while ((n = n / 10) > 0);
#else
   while ((n /= 10) > 0);
#endif
    if (minus) *ps++ = '-';
    *ps = '\0';
    reversen(s,stlen(s));
    return (int)(ps - s);
}

/* PROGRAM: intoall converts n into characters in s. s must be big enough.
 *          It also adds the null term char.
 *          For 64-bit signed values (dbkey).
 *
 * RETURNS:
 */
int
intoall(LONG64 n,
        TEXT  *s)
{
    FAST TEXT   *ps = s;
    FAST GBOOL    minus;
   
    if (n == 0x8000000000000)
    {
        /* Maximum negative number.  Changing the
         * sign will not work so handle it explicitly
         */
        ps = (TEXT *)stcopy(ps, (TEXT *)"-9223372036854775808");
        *ps = '\0';
        return ((int) (ps - s));
    }
 
    if ((minus = (n < 0))) n = -n;
    do {
        *ps++ = (TEXT)(n % 10 + '0');
    }
#if SYS5REL2
   while ((n = n / 10) > 0);
#else
   while ((n /= 10) > 0);
#endif
    if (minus) *ps++ = '-';
    *ps = '\0';
    reversen(s,stlen(s));
    return (int)(ps - s);
}

/* PROGRAM: utConvertBytes - given a value in bytes convert it to
 *          kb, mb, gb, tb based on the following:
 *           if value < KBYTE, display in bytes
 *           if value >= KBYTE and < MBYTE, display in KBYTE
 *           if value >= MBYTE and < GBYTE, display in MBYTE
 *           if value >= GBYTE and < TBYTE, display in GBYTE
 *           otherwise display in TBYTE
 *          Pass back the converted value and an identifierin
 *
 * RETURNS: 0 on success
 *         -1 on failure
 */
int
utConvertBytes(DOUBLE bytesin,
               DOUBLE *bytesout,
               TEXT *identifier)
{
    /* if < KBYTE, display in bytes */
    if (bytesin < KBYTE)
    {
        *identifier = 'B';
        *bytesout = bytesin;
    }

    /* if < MBYTE, display in KBYTE */
    else if (bytesin < MBYTE)
    {
        *identifier = 'K';
        *bytesout = bytesin / KBYTE;
    }

    /* if < GBYTE, display in MBYTE */
    else if (bytesin < GBYTE)
    {
        *identifier = 'M';
        *bytesout = bytesin / MBYTE;
    }

    /* if < TBYTE, display in GBYTE */
    else if (bytesin < TBYTE)
    {
        *identifier = 'G';
        *bytesout = bytesin / GBYTE;
    }

    /* display in TBYTE */
    else
    {
        *identifier = 'T';
        *bytesout = bytesin / TBYTE;
    }

    return(0);
} /* end upConvertBytes */

