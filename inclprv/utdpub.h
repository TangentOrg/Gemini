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

#ifndef UTDPUB_H
#define UTDPUB_H

/**** Date constants ****/
 
/* value for DATEBASE taken from bffld.h */
/* Dates are offset by this value (to maintain compatiblity with enterprise)
 * the date portion of timestamps ARE NOT offset by this value.
 */
#define DATEBASE 2433405L       /* 5/2/50 (caz) */

#define MINSDAY -10247087L      /* 1/1/-32768 */        /* bug 92-04-20-013 */
#define MAXSDAY 13689326L       /* 12/31/32767 */
 
#define GREGORIAN 'g'
#define JULIAN    'j'
 
#define JGSPLIT 2299162L        /* 10/15/1582 Julian/Gregorian split */
 
#define J4 (LONG) (365 * 4 + 1)
#define J16 (LONG) (4 * J4)
#define J512 (LONG) (32 * J16)
#define G100 (LONG) (25 * J4 - 1)
#define G400 (LONG) (4 * G100 + 1)
 
#define GB1600 2305448L /* 12/31/1599 Gregorian */
#define JB1600 2305458L /* 12/31/1599 Julian */
 
/**** end Date Constants ****/

/* Return Codes */
#define DATEOK     0 
#define BADMODE    1
#define BADDAYSIZE 2 
#define BADYEAR    3
#define BADMON     4
#define BADDAY     5

typedef struct utDate                     /* A date  */
{
        COUNT       year;      /* year:  1 to 9999 */
        TEXT        month;     /* month: 1 to 12   */
        TEXT        day;       /* day: 1 to max # days in month */
        TEXT        mode;      /* g = Gregorian, j = Julian */
} utDate_t;

typedef struct utTime                      /* A time  */
{
        TEXT        hour;       /* hours:        0 to 23  */
        TEXT        minute;     /* minutes:      0 to 59  */
        TEXT        second;     /* seconds:      0 to 59  */
        COUNT       msecond;    /* milliseconds: 0 to 999 */
} utTime_t;


/* Public function prototypes for utdate.c */

DLLEXPORT int utDateToLong(LONG *pstoreDate, utDate_t *peDate);

DLLEXPORT int utLongToDate(LONG storeDate, utDate_t *peDate);

DLLEXPORT int utTimeToLong(LONG *psTime, utTime_t  *pValue);

DLLEXPORT int utLongToTime(LONG sTime, utTime_t *pfieldValue);

#endif /*  UTDPUB_H */
