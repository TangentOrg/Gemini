
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

/* Table of Contents - system dependent type utility functions:
 *
 *  slng  - copies a LONG to storage, like in a data record
 *  xlng  - copies a value from storage to a LONG and returns it
 *  xct   - returns the COUNT value of a count variable in storage
 *  sct   - copies a COUNT to storage, like in a data record
 *  slngv - copies significant bytes of a LONG to storage
 *  slngv64 - copies significant bytes of a LONG64 to storage
 *  xlngv - returns LONG equivalent of variable length storage
 *  xlngv64 -  returns LONG64 equivalent of variable length storage
 *  sdbl  - copies double to storage
 *  sflt  - Convert double to float and copy to storage.
 *  xdbl  - loads double from storage
 *  xflt  - loads float from storage
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
#include "utspub.h"

/* PROGRAM: slng - copies a LONG to storage, like in a data record
 *
 * RETURNS: DSMVOID
 */
DSMVOID
slng(TEXT	*pt,	/* ptr to storage area for result */
	 LONG	lng)	/* long value to store */
{
    pt += sizeof(LONG);
    *--pt = (TEXT)lng;
    lng >>= 8;
    *--pt = (TEXT)lng;
    lng >>= 8;
    *--pt = (TEXT)lng;
    lng >>= 8;
    *--pt = (TEXT)lng;
}

/* PROGRAM: slng64 - copies a LONG64 to storage, like in a data record
 *
 * RETURNS: DSMVOID
 */
DSMVOID
slng64(TEXT	*pt,	/* ptr to storage area for result */
       LONG64	lng)	/* long value to store */
{
    
    pt += sizeof(LONG64);
    *--pt = (TEXT)lng;
    lng >>= 8;
    *--pt = (TEXT)lng;
    lng >>= 8;
    *--pt = (TEXT)lng;
    lng >>= 8;
    *--pt = (TEXT)lng;
    lng >>= 8;
    *--pt = (TEXT)lng;
    lng >>= 8;
    *--pt = (TEXT)lng;
    lng >>= 8;
    *--pt = (TEXT)lng;
    lng >>= 8;
    *--pt = (TEXT)lng;
}


/* PROGRAM: xlng - copies a value from storage to a LONG and returns it
 *
 * RETURNS: LONG copied from storage
 */
LONG
xlng (TEXT	*pt)	/* storage area holding the LONG */
{
	LONG	 lng;

    lng  = (LONG) *pt++ << 24;
    lng += (LONG) *pt++ << 16;
    lng += (LONG) *pt++ <<  8;
    lng += (LONG) *pt;
    return lng;
}

/* PROGRAM: xlng64 - copies a value from storage to a LONG64 and returns it
 *
 * RETURNS: LONG copied from storage
 */
LONG64
xlng64 (TEXT	*pt)	/* storage area holding the LONG */
{
  LONG64	 lng;
       
  lng =  (LONG64) *pt++ << 56;
  lng += (LONG64) *pt++ << 48;
  lng += (LONG64) *pt++ << 40;
  lng += (LONG64) *pt++ << 32;
  lng += (LONG64) *pt++ << 24;
  lng += (LONG64) *pt++ << 16;
  lng += (LONG64) *pt++ <<  8;
  lng += (LONG64) *pt;
  return lng;
}

/* PROGRAM: xct - returns the COUNT value of a count variable in storage
 *
 * RETURNS: COUNT value of a count variable in storage
 */
COUNT
xct (TEXT	*pt)	/* ptr to storage area holding the COUNT */
{
	COUNT	 ct;

    ct	= (COUNT) *pt++ <<  8;
    ct += (COUNT) *pt;
    return ct;
}


/* PROGRAM: sct - copies a COUNT to storage, like in a data record
 *
 * RETURNS: DSMVOID
 */
DSMVOID
sct (TEXT	*pt,	/* ptr to target storage area for COUNT */
	 COUNT  ct)	    /* value to be stored */
{
    *++pt = (TEXT)ct;
    ct >>= 8;
    *--pt = (TEXT)ct;
}


/* PROGRAM: slngv - copies significant bytes of a LONG to storage
 *
 * RETURNS: int
 */
int
slngv(TEXT	*pt,	/* ptr to target storage area */
	  LONG  lng)	/* value to be stored */
{
	TEXT	b1;
	TEXT	b2;
	TEXT	b3;

    if (lng == 0) return (0);
    if (lng > 0)
    {
	/* Positive number */

	if (lng < 128)
	{
	    /* Fits in 1 byte */

	    *pt = (TEXT)lng;
	    return ((COUNT)1);
	}
	b1 = (TEXT)lng;
	lng = (lng >> 8);
	if (lng < 127)
	{
	    /* Need two bytes */

	    *pt++ = (TEXT)lng;
	    *pt   = b1;
	    return ((COUNT)2);
	}
	b2 = (TEXT)lng;
	lng = (lng >> 8);
	if (lng < 127)
	{
	    /* Need 3 bytes */

	    *pt++ = (TEXT)lng;
	    *pt++ = b2;
	    *pt   = b1;
	    return ((COUNT)3);
	}
	/* Need 4 bytes */

	b3 = (TEXT)lng;

	*pt++ = (TEXT)(lng >> 8);
	*pt++ = b3;
	*pt++ = b2;
	*pt   = b1;
        return ((COUNT)4);
    }
    /* Negative number */

    if (lng > -128)
    {
        /* Fits in 1 byte */

        *pt = (TEXT)lng;
        return ((COUNT)1);
    }
    b1 = (TEXT)lng;
    lng = (lng >> 8);
    if (lng > -128)
    {
        /* Need two bytes */

        *pt++ = (TEXT)lng;
        *pt   = b1;
        return ((COUNT)2);
    }
    b2 = (TEXT)lng;
    lng = (lng >> 8);
    if (lng > -128)
    {
        /* Need 3 bytes */

        *pt++ = (TEXT)lng;
        *pt++ = b2;
        *pt   = b1;
        return ((COUNT)3);
    }
    /* Need 4 bytes */

    b3 = (TEXT)lng;

    *pt++ = (TEXT)(lng >> 8);
    *pt++ = b3;
    *pt++ = b2;
    *pt   = b1;
    return ((COUNT)4);
}

/* PROGRAM: slngv64 - copies significant bytes of a LONG to storage
 *
 * RETURNS: int
 */
int
slngv64(TEXT	*pt,	/* ptr to target storage area */
      LONG64  lng)	/* value to be stored */
{
	TEXT	b1;
	TEXT	b2;
	TEXT	b3;
	TEXT	b4;
	TEXT	b5;
	TEXT	b6;
	TEXT	b7;

    if (lng == 0) return (0);
    if (lng > 0)
    {
	/* Positive number */

	if (lng < 128)
	{
	    /* Fits in 1 byte */

	    *pt = (TEXT)lng;
	    return ((COUNT)1);
	}
	b1 = (TEXT)lng;
	lng = (lng >> 8);
	if (lng < 127)
	{
	    /* Need two bytes */

	    *pt++ = (TEXT)lng;
	    *pt   = b1;
	    return ((COUNT)2);
	}
	b2 = (TEXT)lng;
	lng = (lng >> 8);
	if (lng < 127)
	{
	    /* Need 3 bytes */

	    *pt++ = (TEXT)lng;
	    *pt++ = b2;
	    *pt   = b1;
	    return ((COUNT)3);
	}

	b3 = (TEXT)lng;
	lng = (lng >> 8);
	if (lng < 127)
	{
	    /* Need 4 bytes */

	    *pt++ = (TEXT)lng;
	    *pt++ = b3;
	    *pt++ = b2;
	    *pt   = b1;
	    return ((COUNT)4);
	}
	b4 = (TEXT)lng;
	lng = (lng >> 8);
	if (lng < 127)
	{
	    /* Need 5 bytes */

	    *pt++ = (TEXT)lng;
	    *pt++ = b4;
	    *pt++ = b3;
	    *pt++ = b2;
	    *pt   = b1;
	    return ((COUNT)5);
	}

	b5 = (TEXT)lng;
	lng = (lng >> 8);
	if (lng < 127)
	{
	    /* Need 6 bytes */

	    *pt++ = (TEXT)lng;
	    *pt++ = b5;
	    *pt++ = b4;
	    *pt++ = b3;
	    *pt++ = b2;
	    *pt   = b1;
	    return ((COUNT)6);
	}

	b6 = (TEXT)lng;
	lng = (lng >> 8);
	if (lng < 127)
	{
	    /* Need 7 bytes */

	    *pt++ = (TEXT)lng;
	    *pt++ = b6;
	    *pt++ = b5;
	    *pt++ = b4;
	    *pt++ = b3;
	    *pt++ = b2;
	    *pt   = b1;
	    return ((COUNT)7);
	}

	/* Need 8 bytes */

	b7 = (TEXT)lng;

	*pt++ = (TEXT)(lng >> 8);
	*pt++ = b7;
	*pt++ = b6;
	*pt++ = b5;
	*pt++ = b4;
	*pt++ = b3;
	*pt++ = b2;
	*pt   = b1;
        return ((COUNT)8);
    }
    /* Negative number */

    if (lng > -128)
    {
        /* Fits in 1 byte */

        *pt = (TEXT)lng;
        return ((COUNT)1);
    }
    b1 = (TEXT)lng;
    lng = (lng >> 8);
    if (lng > -128)
    {
        /* Need two bytes */

        *pt++ = (TEXT)lng;
        *pt   = b1;
        return ((COUNT)2);
    }
    b2 = (TEXT)lng;
    lng = (lng >> 8);
    if (lng > -128)
    {
        /* Need 3 bytes */

        *pt++ = (TEXT)lng;
        *pt++ = b2;
        *pt   = b1;
        return ((COUNT)3);
    }
    b3 = (TEXT)lng;
    lng = (lng >> 8);
    if (lng > -128)
    {
        /* Need 4 bytes */

        *pt++ = (TEXT)lng;
        *pt++ = b3;
        *pt++ = b2;
        *pt   = b1;
        return ((COUNT)4);
    }
    b4 = (TEXT)lng;
    lng = (lng >> 8);
    if (lng > -128)
    {
        /* Need 5 bytes */

        *pt++ = (TEXT)lng;
        *pt++ = b4;
        *pt++ = b3;
        *pt++ = b2;
        *pt   = b1;
        return ((COUNT)5);
    }
    b5 = (TEXT)lng;
    lng = (lng >> 8);
    if (lng > -128)
    {
        /* Need 6 bytes */

        *pt++ = (TEXT)lng;
        *pt++ = b5;
        *pt++ = b4;
        *pt++ = b3;
        *pt++ = b2;
        *pt   = b1;
        return ((COUNT)6);
    }
    b6 = (TEXT)lng;
    lng = (lng >> 8);
    if (lng > -128)
    {
        /* Need 7 bytes */

        *pt++ = (TEXT)lng;
        *pt++ = b6;
        *pt++ = b5;
        *pt++ = b4;
        *pt++ = b3;
        *pt++ = b2;
        *pt   = b1;
        return ((COUNT)7);
    }
    /* Need 8 bytes */

    b7 = (TEXT)lng;

    *pt++ = (TEXT)(lng >> 8);
    *pt++ = b7;
    *pt++ = b6;
    *pt++ = b5;
    *pt++ = b4;
    *pt++ = b3;
    *pt++ = b2;
    *pt   = b1;
    return ((COUNT)8);
}

/* PROGRAM: xlngv - returns LONG equivalent of variable length storage
 *
 * RETURNS: LONG equivalent of variable length storage
 */
LONG
xlngv (TEXT	  *pt,	/* storage area holding value */
	   COUNT  len)	/* number of bytes stored */
{
	LONG	 lng = 0;

    if ((*pt) & (TEXT)128) lng--;

    switch (len)
    {
    case 4: lng = *pt++;
    case 3: lng = (lng << 8) + *pt++;
    case 2: lng = (lng << 8) + *pt++;
    case 1: lng = (lng << 8) + *pt;
    }
    return lng;
}

/* PROGRAM: xlngv64 - returns LONG equivalent of variable length storage
 *
 * RETURNS: LONG64 equivalent of variable length storage
 */
LONG64
xlngv64 (TEXT	  *pt,	/* storage area holding value */
	   COUNT  len)	/* number of bytes stored */
{
	LONG64	 lng = 0;

    if ((*pt) & (TEXT)128) lng--;

    switch (len)
    {
    case 8: lng = *pt++;
    case 7: lng = (lng << 8) + *pt++;
    case 6: lng = (lng << 8) + *pt++;
    case 5: lng = (lng << 8) + *pt++;
    case 4: lng = (lng << 8) + *pt++;
    case 3: lng = (lng << 8) + *pt++;
    case 2: lng = (lng << 8) + *pt++;
    case 1: lng = (lng << 8) + *pt;
    }
    return lng;
}


/* PROGRAM: sdbl  - copies double to storage
 *
 * RETURNS: DSMVOID
 */
void
sdbl(TEXT *pt,
     double d)
{

#if CHIP==INT86 || CHIP==INT386 || OPSYS == MS_DOS || OPSYS == ALPHAOSF
        TEXT *pd = (TEXT *)&d;
        int i;

        for (i = 1; i <= (int)sizeof(double); *pt++ = *(pd + sizeof(double) - i++))
	    ;
#else
#if CHIP==VAXCPU
        int i;

	/* convert vax d_float to ieee double:  bb 3/28/93 */ 

        UCOUNT tmpquad[4];
        COUNT  numwords = 4;

        /* temporary storage */
        bufcop((TEXT *)&tmpquad, (TEXT *)&d, 2*numwords);

        /* right shift 3 bits of mantissa into subsequent WORDS to allow */
        /* 11 bit exponent from 8-bit D_FLOAT  */
        for (i = 3; i>0; i--)
           tmpquad[i] = (( tmpquad[i] >> 3) & 0x1fff) + (tmpquad[i-1] << 13);

        /* sign bit 15, 11-bit exponent in 4-14, 4 bit mantissa in 0-3 */
        /* in first WORD. Rest of mantissa follows in subsequent words */
        tmpquad[0]  =     (tmpquad[0] & 0x8000) +
                       ((((tmpquad[0] & 0x7f80) >> 7) + 894) << 4) +
                         ((tmpquad[0] >> 3) & 0xf) ;

         for ( i = 0; i < numwords; i++)
            tmpquad[i] = (UCOUNT)( xct(&tmpquad[i]));

        bufcop((TEXT *)pt, (TEXT *)&tmpquad[0], 2*numwords);

#else
        /* In the bigendian style */
        bufcop(pt, (TEXT *)&d, sizeof(double));
#endif
#endif
}

/* PROGRAM: sflt - Convert double to float and copy to storage. Put in d only
 *		   values which can be translated to float. Float is not used
 * 		   directly since most compilers, silently convert it to
 *		   causing problems with function prototypes.
 *
 * RETURNS: void
 */
void
sflt(TEXT *pt,
     double d)
{
        float f1 = (float)d;
        TEXT *pf = (TEXT *)&f1;

#if CHIP==INT86 || CHIP==INT386 || OPSYS == MS_DOS || OPSYS == ALPHAOSF
        int i;

        for (i = 1; i <= (int)sizeof(float); *pt++ = *(pf + sizeof(float) - i++))
	    ;
#else
#if CHIP==VAXCPU
        int i;

	/* convert vax f_float to ieee float : bb 3/28/93 */
        UCOUNT tmpflt[2];
        COUNT  numwords = 2;

        /* temporary storage */
        bufcop((TEXT *)&tmpflt, (TEXT *)&f, 2*numwords);

        /* need to change vax 128+ bias to IEEE 127+ bias */
        /* sign bit in 15, 8 bit exponent in 7-14, 7 bit mantissa in 0-6 */
        /* in first WORD, rest of mantissa follow in next word */
        tmpflt[0] =  (tmpflt[0] & 0x8000) +
                  ((((tmpflt[0] & 0x7f80) >> 7) - 2) << 7) +
                     (tmpflt[0] & 0x7f) ;

        /* swap the bytes in COUNT */
        for ( i = 0; i < numwords; i++)
            tmpflt[i] = (UCOUNT)( xct(&tmpflt[i]));

        bufcop((TEXT *)pt, (TEXT *)&tmpflt[0], 2*numwords);

#else
        /* In the bigendian style */
        bufcop(pt, pf, sizeof(float));
#endif
#endif
}

/* PROGRAM: xdbl - loads double from storage
 *
 * RETURNS: double
 */
double
xdbl(TEXT *pt)
{
        double d;
#if CHIP==INT86 || CHIP==INT386 || OPSYS == MS_DOS || OPSYS == ALPHAOSF
        TEXT *pd = (TEXT *)&d;
        int i;

        for (i = 1; i <= (int)sizeof(double); *pd++ = *(pt + sizeof(double)  - i++))
	    ;
#else
#if CHIP==VAXCPU
        int i;

	/* convert ieee double to vax d_float: bb 3/28/93 */
        UCOUNT tmpquad[4];
        COUNT  numwords = 4;

        /* temporary storage */
        bufcop((TEXT *)&tmpquad, (TEXT *)pt, 2*numwords);

        /* swap bytes of each word */
        for ( i = 0; i < numwords; i++)
            tmpquad[i] = (UCOUNT)( xct(&tmpquad[i]));

        /* left shift 3 bits of mantissa from subsequent WORDS to make */
        /* 8-bit D_FLOAT exponent from 11-bit IEEE exponent  */
        /* sign bit 15, 11-bit exponent in 4-14, 4 bit mantissa in 0-3 */
        /* in first WORD. Rest of mantissa follows in subsequent words */
        tmpquad[0]  =     (tmpquad[0] & 0x8000) +
                       ((((tmpquad[0] >> 4) & 0x7ff) - 894) << 7) +
                         ((tmpquad[0] & 0xf) << 3) +
                         ((tmpquad[1] >> 13) & 0x0007) ;
        tmpquad[1]  =     (tmpquad[1] << 3) + ((tmpquad[2] >> 13) & 0x0007);
        tmpquad[2]  =     (tmpquad[2] << 3) + ((tmpquad[3] >> 13) & 0x0007);
        tmpquad[3]  =     (tmpquad[3] << 3);

        bufcop((TEXT *)&d, (TEXT *)&tmpquad[0], 2*numwords);
#else
        /* In the bigendian style */
        bufcop((TEXT *)&d, pt, sizeof(double));
#endif
#endif
        return d;
}

/* PROGRAM: xflt - loads float from storage
 *
 * RETURNS: float
 */
float
xflt(TEXT *pt)
{
        float f;

#if CHIP==INT86 || CHIP==INT386 || OPSYS == MS_DOS || OPSYS == ALPHAOSF
        TEXT *pf = (TEXT *)&f;
        int i;

        for (i = 1; i <= (int)sizeof(float); *pf++ = *(pt + sizeof(float) - i++))
	    ;
#else
#if CHIP==VAXCPU
        int i;

	/* convert ieee float to vax f_float: bb 3/28/93 */
        UCOUNT tmpflt[2];
        COUNT  numwords = 2;

        /* temporary storage */
        bufcop((TEXT *)&tmpflt, (TEXT *)pt, 2*numwords);

        /* swap the bytes in COUNT */
        for ( i = 0; i < numwords; i++)
            tmpflt[i] = (UCOUNT)( xct(&tmpflt[i]));

        /* need to change IEEE 127+ bias to VAX 128+ bias*/
        /* sign bit in 15, 8 bit exponent in 7-14, 7 bit mantissa in 0-6 */
        /* in first WORD, rest of mantissa follow in next word */
        tmpflt[0] =  (tmpflt[0] & 0x8000) +
                  ((((tmpflt[0] & 0x7f80) >> 7) + 2) << 7) +
                    (tmpflt[0] & 0x7f) ;

        bufcop((TEXT *)&f, (TEXT *)&tmpflt[0], 2*numwords);
#else
        /* In the bigendian style */
        bufcop((TEXT *)&f, pt, sizeof(float));
#endif
#endif
	return f;
}

