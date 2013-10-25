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

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "utdpub.h"

/* FILE: utdate.c - contains utility functions for date/time conversions
 */

INTERN COUNT upto[13] = {0,31,59,90,120,151,181,
                        212,243,273,304,334,365
                        };

/* PROGRAM: utDateToLong - convert date structure to long store date
 *
 * RETURNS: number of days since base date (05/02/50) in pstoreDate
 *       BADMODE     mode is not GREGORIAN or JULIAN
 *       BADYEAR     year is 0
  *      BADMON      month is < 1 or > 12
 *       BADDAY      day is < 1 or > #days in month
 */
int
utDateToLong(
        LONG *pstoreDate,  /* OUT */
        utDate_t *peDate)      /* IN */
{
    LONG     storeDate;
    COUNT    year;
    COUNT    i;
    COUNT    endMonth;
    COUNT    dayInYear;
    GBOOL     leap;
    GBOOL     mode = peDate->mode;
 
    if ((year = peDate->year) == 0)
        return BADYEAR;
 
    if (year < 1582 ||
        (year == 1582 &&
         (peDate->month < 10 ||
          (peDate->month == 10 && peDate->day < 15))))
        mode = JULIAN;              /* force Julian */
 
    leap = 0;
    if (year < 0)
        ++year;
 
    switch(mode)
    {
      case GREGORIAN:
        storeDate = GB1600;
        while(year >= 2000)
        {   storeDate += G400;
            year -= 400;
        }
        while(year < 1600)
        {   storeDate -= G400;
            year += 400;
        }
        if (year == 1600)
            leap = 1;
        else
        {   ++storeDate;
            while (year >= 1700)
            {   storeDate += G100;
                year -= 100;
            }
            if (year > 1600)
                --storeDate;
        }
        break;
 
      case JULIAN:
        storeDate = JB1600;
        while (year >= (1600 + 512))
        {   storeDate += J512;
            year -= 512;
        }
        while (year < 1600)
        {   storeDate -= J512;
            year += 512;
        }
        break;
 
      default:
        return BADMODE;
    }
 
    if ((mode == JULIAN) || (year > 1600))
    {   while (year >= 1616)
        {   storeDate += J16;
            year -= 16;
        }
        while (year >= 1604)
        {   storeDate += J4;
            year -= 4;
        }
        if (year == 1600)
            leap = 1;
        else
        {   ++storeDate;
            while (--year >= 1600) storeDate += 365;
        }
    }
    /* compute month/day additions */
    if (peDate->month < 1 || peDate->month > 12)
        return BADMON;
    if (peDate->day < 1)
        return BADDAY;
    i = upto[peDate->month - 1];
    endMonth = upto[peDate->month] - i;
    dayInYear = i + peDate->day;
    if (leap)
    {   if (peDate->month == 2)
            endMonth++;
        if (peDate->month > 2)
            dayInYear++;
    }
    *pstoreDate = storeDate + dayInYear;
    if (peDate->day > endMonth)
        return BADDAY;

    return 0;
 
}  /* end utDateToLong */
 

/* PROGRAM: utLongToDate - take long date and fill in date struct
 *
 * NOTE: This is basically a copy of fmxdate
 *
 * RETURNS:
 *         SUCCESS
 *         SDAYSIZE    sday is out of range
 *         BADMODE     mode is not JULIAN or GREGORIAN
 */
int
utLongToDate(
        LONG       storeDate,
        utDate_t *peDate)
{
    COUNT    year;
    COUNT    i;
    COUNT    endMonth;
    COUNT    *pendMonth;
    COUNT    dayInYear;
    GBOOL     leap;
 
    if ((storeDate < MINSDAY) || (storeDate > MAXSDAY))
        return BADDAYSIZE;
    if (storeDate < JGSPLIT)
        peDate->mode = JULIAN;
 
    leap = 0;
    year = 1600;
 
    switch (peDate->mode)
    {
      case GREGORIAN:
        storeDate -= GB1600;
        while (storeDate > G400)
        {   storeDate -= G400;
            year += 400;
        }
        while (storeDate <= 0)
        {   storeDate += G400;
            year -= 400;
        }
        if (storeDate <= 366)
            leap = 1;
        else
        {
            --storeDate;
            while (storeDate > G100)
            {   storeDate -= G100;
                year += 100;
            }
            if (storeDate > 365)
                ++storeDate;
        }
        break;
 
      case JULIAN:
        storeDate -= JB1600;
        while (storeDate > J512)
        {   storeDate -= J512;
            year += 512;
        }
        while (storeDate <= 0)
        {   storeDate += J512;
            year -= 512;
        }
        break;
 
      default:
        return BADMODE;
    }
    if ((peDate->mode == JULIAN) || (storeDate > 366))
    {   while (storeDate > J16)
        {   storeDate -= J16;
            year += 16;
        }
        while (storeDate > J4)
        {   storeDate -= J4;
            year += 4;
        }
        if (storeDate <= 366)
            leap = 1;
        else
        {   --storeDate;
            while (storeDate > 365)
            {   storeDate -= 365;
                ++year;
            }
        }
    }
 
    if (year <= 0)
        --year;
    peDate->year = year;
    dayInYear = (COUNT)storeDate;
    if (leap && storeDate > 59)
        --storeDate;
    for (i = 1, pendMonth = &upto[1];
         *pendMonth < storeDate;
         ++i, ++pendMonth);
    peDate->month = (UTEXT)i;
    endMonth = *pendMonth;
    endMonth -= *--pendMonth;
    if (leap && (i == 2))
    {   endMonth++;
        peDate->day = dayInYear - (COUNT) 31;
    }
    else
        peDate->day = (UTEXT)(storeDate - *pendMonth);
 
    return 0;
 
}  /* end utLongToDate */


/* PROGRAM: utTimeToLong
 *
 * convert recTime_t to a LONG containing # of milliseconds since midnight
 * (time 0).
 *
 *       # milliseconds since midnight    =
 *       hour    * # milliseconds/hour    +
 *       minute  * # milliseconds/minutes +
 *       second  * # milliseconds/second  +
 *                 # milliseconds
 *
 * RETURNS: number of milliseconds since midnight
 */
int
utTimeToLong(LONG *psTime, utTime_t  *pValue)
{
    *psTime = (pValue->hour    * 3600000) +
              (pValue->minute  *   60000) +
              (pValue->second  *    1000) +
               pValue->msecond;
 
    return 0;
 
}  /* end utTimeToLong */
 

/* PROGRAM: utLongToTime - convert long value to # mill sec since midnight
 *
 * RETURNS:
 */
int
utLongToTime(
        LONG          sTime,
        utTime_t    *pfieldValue)
{
    pfieldValue->hour     = sTime / 3600000;
    sTime                 = sTime % 3600000;
    pfieldValue->minute   = sTime /   60000;
    sTime                 = sTime %   60000;
    pfieldValue->second   = sTime /    1000;
    pfieldValue->msecond  = sTime %    1000;
 
    return 0;
 
}  /* end utLongToTime */

