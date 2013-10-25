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

#ifndef UTTPUB_H
#define UTTPUB_H

/* Used in utConvertBytes */
#define KBYTE          1024.0
#define MBYTE       1048576.0
#define GBYTE    1073741824.0
#define TBYTE 1099511627776.0

#include <time.h>

#define TIMESIZE 32

/* prototypes for uttime.c */

DLLEXPORT TEXT *uttime (time_t *date, TEXT *ptimeString, int timeLength);

DLLEXPORT DSMVOID utdeadp (DSMVOID);      /* called when a SIGPIPE interrupt has */
				    /* been received.                      */

DLLEXPORT DSMVOID utwaitp (int *ppid,
			int  maxwait);  /* waits for indicated subprocess to */
				        /* terminate.                        */

DLLEXPORT LONG uthms(TEXT *hmsbuf); /* must point to 8 char buffer */

DLLEXPORT TEXT * utintnn(TEXT *pbuf, /* put result chars here */
                      int i);      /* convert this integer  */
DLLEXPORT DSMVOID utSysTime( LONG adjustment, LONG *psystemTime );

/* utype.c */
/* data type conversion utilities */

typedef struct dbutTime                      /* A time  */
{
        TEXT        hour;       /* hours:        0 to 23  */
        TEXT        minute;     /* minutes:      0 to 59  */
        TEXT        second;     /* seconds:      0 to 59  */
        COUNT       msecond;    /* milliseconds: 0 to 999 */
} dbutTime_t;

typedef struct dbutDate                     /* A date  */
{
        COUNT       year;      /* year:  1 to 9999 */
        TEXT        month;     /* month: 1 to 12   */
        TEXT        day;       /* day: 1 to max # days in month */
        TEXT        mode;      /* g = Gregorian, j = Julian */
} dbutDate_t;

DLLEXPORT LONG atoin (TEXT *ps, int n);

DLLEXPORT int intoal(LONG n, TEXT *s);
DLLEXPORT int intoall(LONG64 n, TEXT *s);

DLLEXPORT DSMVOID reversen           (TEXT *ps, int len);

DLLEXPORT COUNT countl (LONG    lnum);

DLLEXPORT DSMVOID hextoa(TEXT *ps, 
            TTINY *ph, 
            int len);

DLLEXPORT TEXT *utintnn(TEXT *pbuf, /* put result chars here */
              int i);      /* convert this integer  */

DLLEXPORT TEXT *utintbbn(TEXT    *pbuf,  /* put result chars here */
               int      i);     /* convert this integer  */

DLLEXPORT DSMVOID utitoa(LONG    n,
            TEXT    s[]);

DLLEXPORT int utConvertBytes(DOUBLE     bytesin,      /* size value in */
                             DOUBLE    *bytesout,     /* size value out */
                             TEXT      *identifier);  /* size identifier out */

/****** uttimr.c ******/
/* NOTE: Don't rename uttimr.c functions - they are shared with the client
 *
 *       ...with the exception of utsleep which is being defined in
 *       dbdbctl.c. In order to enable the client to load rocket as a
 *       DLL functions are being renamed on the client side and included
 *       in a function table. It's not feasable to rename all of the
 *       occurances of utsleep in the client. Others may also be considered
 *       in the future.
 */

DLLEXPORT DSMVOID utsleep            (int secs);
 
DLLEXPORT DSMVOID utnap              (int milliSeconds); 
 
DLLEXPORT LONG utevt              (int secs, GTBOOL *pflag);

DLLEXPORT DSMVOID utcncel            (GTBOOL *pflag);

#if OPSYS==UNIX
 
DLLEXPORT DSMVOID utalsig            (int signum);

DLLEXPORT DSMVOID utwait             (GTBOOL *pflag);
 
#endif /* OPSYS==UNIX */

#if HAS_US_TIME
#if SUN4 || HP825

DLLEXPORT ULONG utrdusclk ();

#endif  /* SUN4 || HP825 */
#endif  /* HAS_US_TIME */

 
#define MAXTIMR 20      /* maximum number of pending time intervals */

#endif /* UTTPUB_H */
