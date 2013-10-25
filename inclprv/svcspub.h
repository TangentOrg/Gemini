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

#ifndef SVCSPUB_H
#define SVCSPUB_H

/* key modes */
#define SVCV8IDXKEY     0x001   /* Index key format v7 or v8 */
#define SVCV9IDXKEY     0x002   /* Index key format v9 */
#define SVCV8SORTKEY    0x003   /* Sort key format v4 thru v8 */

/* Base date value for storage/external format */
#define SVCDATEBASE 2433405L 


typedef struct svcI18N          /* I18N related input */
{
    TEXT        *pCollMaps;     /* Collating tables */
    TEXT        *pCpAttrib;     /* Code Page info */
} svcI18N_t;

typedef struct svcByteString                /* a Byte string */
{
       TEXT     *pbyte;   /* pointer to byte string */
       ULONG     size;    /* size of byte string (used only for extracting) */
       ULONG     length;  /* length of formatted string (to store or extracted) */
} svcByteString_t;

typedef TINY svcTiny_t;

typedef COUNT svcSmall_t;

typedef GTBOOL svcBool_t;

typedef LONG svcLong_t;

#if 0
typedef struct svcBig                      /* Big integer (LONGLONG) */
{
        LONG    ll;     /* Low long */
        LONG    hl;     /* High long */
} svcBig_t;
#else
typedef LONG64 svcBig_t;
#endif

typedef float svcFloat_t;                  /* a Float */

typedef DOUBLE svcDouble_t;

typedef struct svcDate                     /* A date  */
{
        COUNT       year;      /* year:  1 to 9999 */
        TEXT        month;     /* month: 1 to 12   */
        TEXT        day;       /* day: 1 to max # days in month */
        TEXT        mode;      /* g = Gregorian, j = Julian */
} svcDate_t;

typedef struct svcTime                      /* A time  */
{
        TEXT        hour;       /* hours:        0 to 23  */
        TEXT        minute;     /* minutes:      0 to 59  */
        TEXT        second;     /* seconds:      0 to 59  */
        COUNT       msecond;    /* milliseconds: 0 to 999 */
} svcTime_t;

typedef struct svcTimeStamp                /* A time stamp  */
{
        svcDate_t   tsDate;
        svcTime_t   tsTime;
} svcTimeStamp_t;

typedef struct svcDecimal                  /* a Decimal */
{
        GTBOOL        plus;           /* number is positive */
        TTINY        totalDigits;    /* number of digits */
        TTINY        decimals;       /* number of decimal digits */
        TTINY        *pdigit;        /* leading zero suppressed digits */
} svcDecimal_t;

#define SVCMAXDIGIT 50             /* max number of decimal digits */
#define SVCMAXDECS  15             /* max decimal precision */


typedef union svcFormat
{
    svcByteString_t svcByteString;
    svcTiny_t       svcTiny;
    svcSmall_t      svcSmall;
    svcBool_t       svcBool;
    svcLong_t       svcLong;
    svcBig_t        svcBig;
    svcFloat_t      svcFloat;
    svcDouble_t     svcDouble;
    svcDate_t       svcDate;
    svcTime_t       svcTime;
    svcTimeStamp_t  svcTimeStamp;
    svcDecimal_t    svcDecimal;
} svcFormat_t;

typedef struct svcDataType
{
       ULONG        type;        /* value type - see definitions below */
       ULONG        indicator;   /* indicator flags - see definitions below */
       svcFormat_t  f;           /* values */

} svcDataType_t;


/*** svcDataType type flag values: ***/

#define SVCBYTESTRING  1  /* retired: use SVCCHARACTER or SVCBINARY instead */
#define SVCTINY        2
#define SVCSMALL       3
#define SVCBOOL        4
#define SVCLONG        5
#define SVCBIG         6
#define SVCFLOAT       7
#define SVCDOUBLE      8
#define SVCDATE        9
#define SVCTIME       10
#define SVCTIMESTAMP  11
#define SVCDECIMAL    12
#define SVCCHARACTER  13
#define SVCBINARY     14
#define SVCBLOB       15 

/*** indicator flag values: ***/

#define SVCBADTYPE       0x0001     /* datatype & size/value not compatiable */
#define SVCBADVALUE      0x0002     /* datatype & size/value not compatiable */
#define SVCCASESENSITIVE 0x0004
#define SVCDESCENDING    0x0008     /* this component is descending */
#define SVCHIGHRANGE     0x0010     /* for index-braketing */
#define SVCLOWRANGE      0x0020     /* for index-braketing */
#define SVCSQLNULL       0x0040     /* May be treated differently from UNKNOWN */
#define SVCTRUNCVALUE    0x0080     /* Value has been truncated */
#define SVCUNKNOWN       0x0100     /* value unknown                        */
#define SVCTODAY         0x0200     /* value of today                       */
#define SVCMISSING       SVCUNKNOWN /* alias -- used to include NA          */

#endif  /* ifndef SVCSPUB_H */


