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

#ifndef UTDATE_H
#define UTDATE_H

#include "svcspub.h"

/**** Date constants ****/
 
/* value for DATEBASE taken from bffld.h */
/* Dates are offset by this value (to maintain compatiblity with enterprise)
 * the date portion of timestamps ARE NOT offset by this value.
 */
#define DATEBASE SVCDATEBASE   /* 5/2/50 (caz) */

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


/* Public function prototypes for utdate.c */

int utDateToLong(svcLong_t *pstoreDate, svcDate_t *peDate);

int utLongToDate(LONG storeDate, svcDate_t *peDate);

int utTimeToLong(svcLong_t *psTime, svcTime_t  *pValue);

int utLongToTime(LONG sTime, svcTime_t *pfieldValue);

#endif /*  UTDATE_H */
