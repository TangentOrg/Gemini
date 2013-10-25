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

/* Table of Contents - system time utility functions:
 *
 * uttime - given a LONG date/time value, convert it to a
 *          printable ASCII form, using the local time zone.
 * uthms  - get the current time formatted as HH:MM:SS
 * utSysTime - get the current time from the time system call
 *             and apply adjustments if necessary.
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
#include "utspub.h"    /* for stncop */
#include "uttpub.h"    /* for uttime(), uthms() */

#if HPUX
#include <sys/time.h>
#endif /* HPUX */


/* PROGRAM: uttime - given a LONG date/time value, convert it to a
 *              printable ASCII form, using the local time zone.
 *
 * RETURNS: a pointer to a null terminated string, WITHOUT the nl.
 */
TEXT *
uttime (
        time_t *ptime,         /* date/time value */
        TEXT   *ptimeString,
        int     timeLength)  /* length of time string buffer */
{

#if defined(_REENTRANT) && OPSYS!=WIN32API
    /* convert date/time value to ascii string */
#if IBMRIOS || UNIX486V4 || ALPHAOSF || SCO || LINUX || \
    (HPUX && !defined(_PTHREADS_DRAFT4) && !defined(HPUX1020))
    ctime_r(ptime, (psc_rtlchar_t *)ptimeString);
#else
    ctime_r(ptime, (psc_rtlchar_t *)ptimeString, timeLength);
#endif

    /* smash the nl character */
    *(ptimeString + 24) = '\0'; 
#else
   TEXT *ptimeStringOut;

    /* convert date/time value to ascii string */
    ptimeStringOut = (TEXT *)ctime(ptime);

    stncop(ptimeString, ptimeStringOut, timeLength);

    /* smash the nl character */
    *(ptimeString + strlen((psc_rtlchar_t *)ptimeString)-1) = '\0'; 
#endif

    return ptimeString;

}  /* end uttime */


/* PROGRAM: uthms - get the current time formatted as HH:MM:SS
 *
 * RETURNS: a LONG encoded version of today, useful to know if
 *	    today has changed
 */
LONG
uthms(TEXT *hmsbuf) /* must point to 8 char buffer */
{
	time_t		 clock;
	LONG		 longret;
	struct tm	*ptm;

    /* current hh, mm, ss */
    time(&clock);
#if defined(_REENTRANT) && OPSYS!=WIN32API
    {
        struct tm tm;
#if HPUX && (defined(_PTHREADS_DRAFT4) || defined(HPUX1020))
        longret = localtime_r(&clock, &tm);
        ptm = &tm;
#else
        ptm = localtime_r(&clock, &tm);
#endif
    }
#else
    ptm = localtime(&clock);
#endif

    hmsbuf = utintnn(hmsbuf, ptm->tm_hour);
    *hmsbuf++ = ':';
    hmsbuf = utintnn(hmsbuf, ptm->tm_min);
    *hmsbuf++ = ':';
    utintnn(hmsbuf, ptm->tm_sec);
    longret = ((LONG)ptm->tm_year)<<16;
    longret += ptm->tm_yday;

    return longret;

}  /* end uthms */

/* PROGRAM: utSysTime - get the current time from the system time call
 *                      and check to see if any adjustments
 *                      to the time need to be made.
 *
 *          NOTE: adjustments allow us to test the effects of time
 *                alterations due to time daemons, daylight savings,
 *                and Y2k testing.
 *
 * RETURNS:  LONG time value
 *	    
 */
void
utSysTime( LONG adjustment, LONG* psystemTime ) 
{
 
    time(psystemTime);

    if ( adjustment )
        *psystemTime += adjustment;

    return;
}
