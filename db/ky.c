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

#include "dsmpub.h"
#include "kypub.h"
#include "keypub.h"     /* for constants: LOWRANGE ... */
#include "utbpub.h"
#include "utspub.h"

/**************************************************************************
***
***	kyinitdb	initializes a ditem as a key ditem
***	kyins		inserts a data item into a key
***	kycmpdb		compares 2 keys
***	kycount		counts number of equal key comps in 2 keys
**************************************************************************/
/**** LOCAL FUNCTION PROTOTYPE DEFINITIONS ****/

#if 0
LOCALF int kyccnt    ( TEXT *pkeystr);
#endif

/* PROGRAM: kyinitKeydb - 
 *
 * RETURNS: DSMVOID
 */

DSMVOID
kyinitKeydb(
	dsmKey_t	*pkkey,
	COUNT		 ixnum,	
	COUNT		 numcomps,
	GTBOOL		 substrok)
{
    TEXT	*pkeystr;
    COUNT	 i;

#if 1
    TRACE("kyinit")
#else
    TRACE_CB(pcontext, "kyinit")
#endif

    /* initialize the key format part of the ditem */
    pkkey->index	= ixnum;
    pkkey->keycomps	= numcomps;
    pkkey->ksubstr	= substrok;

#if 0
    /* make sure the keystring is long enough for the dummy key */
    /* BUM - assume numcomps always > 0 */
    if ((UCOUNT)(numcomps + 2) > pkditem->dpad)
        FATAL_MSGN_CALLBACK(pcontext, ixFTL003);	/*FATAL*/
#endif

    /* now initialize the dummy keystring */
    pkeystr = pkkey->keystr;
    *(TTINY*)pkeystr = numcomps + 2;
    for (i=0; i<numcomps; i++)	*(TTINY*)++pkeystr = KEY_ILLEGAL;
    *(TTINY*)++pkeystr = numcomps + 2;

    return;

}  /* end kyinitKeydb */


/**********************************************************/
/* PROGRAM: kycmpdb - subr to compare two argument keys
 *
 * RETURNS:
 */
/**********************************************************/
int
kycmpdb(
	TEXT	*pkeystr,
	TEXT	*pixchars)
{
	int	clen;
    	TTINY	kl;
        TTINY	il;
	TTINY	len_k1;
	TTINY	len_k2;
	int	cmpklen;
        int	cmprlen;

    /* get the length of the key */
    len_k1 = *(TTINY*)pkeystr++;  /* advance past length prefix */
    len_k2 = *(TTINY*)pixchars++; /* ...	  		  */
    if( len_k1 < len_k2 )
	cmpklen = len_k1 - 2;   /* subtract 2 for the key length storage */
    else 
	cmpklen = len_k2 - 2;

    /* repeat once for each component of the key	*/
    /* comparing the key comp with index entry loop	*/
    while ( cmpklen > 0 )
    {
	/* save length of the key components, point to 1st byte of key */
	kl = *(TTINY*)pkeystr++;
	il = *(TTINY*)pixchars++;

	/* check for unusual values */
	if ( kl > LKYFLAG || il > LKYFLAG )
	{
	    /* check for range in arg key? */
	    if (kl == LOWRANGE)	return(TOOFAR);
            if (il == SHORTKEY)  return(KEEPGOING);
	    if (kl == HIGHRANGE) return(KEEPGOING);
#if 0
	    /* If the key component is high values, and numcomps > 1, then
	       we have a "value > x" type comparison. In that case, we
	       do not want to return unknown values as satisfying the
	       comparison, so we check the index for MISSING.  */
	       /* NOTE: This code was put in experimentally and may be
		  part of a real fix to this problem at a later time. */
	    if (kl == HIGHRANGE)
	    {
		if (il == HIGHMISSING)
		    return(TOOFAR);
		else
		    return(KEEPGOING);
	    }
#endif
	    if (kl == il)		continue;
	    if (kl == LOWMISSING)	return(TOOFAR);
	    if (kl == HIGHMISSING)	return(KEEPGOING);
	    if (il == LOWMISSING)	return(KEEPGOING);
	    if (il == HIGHMISSING)	return(TOOFAR);
	    if (il == IDXLOCK)		return(KEEPGOING);
	}

	/* we must actually compare them */
	/* find the smallest length
		*** used to be MIN  it generated longer code*/
	if (kl < il) clen = kl;
	else  clen = il;
	cmpklen -= clen + 1;
	for (cmprlen = clen; --clen >= 0; pkeystr++, pixchars++)
	{
	    if (*(UTEXT*)pkeystr == *(UTEXT*)pixchars) continue;
	    /* if arg key > index entry, keep going */
	    if (*(UTEXT*)pkeystr > *(UTEXT*)pixchars) return(KEEPGOING);
	    /* if arg key < index entry, too far */
	    return(TOOFAR);
	}


	if (kl > il)	/* compare lengths */
			 return(KEEPGOING);

	/* do we have a substring match ? */
	if (kl < il)
        {
	    if ( cmpklen == 0 )
		 return(SUBSTRING);
	    else
		 return(TOOFAR);
        }

	/* advance past the characters */
	pkeystr += kl - cmprlen;
	pixchars += il - cmprlen;
    }

    /* fall through bottom means an exact match found */
    return(FOUND);

}  /* end kycmpdb */


#if !NW386
/*************************************************************************/
/* PROGRAM: kycountdb - subr to count equal comps of two argument keys
 *
 * RETURNS: 
 */
/*************************************************************************/
int
kycountdb(
	TEXT	*pkeystr1,
	TEXT	*pkeystr2,
	SMALL	 numcomps)
{
    int	 i;

    pkeystr1++;	/* advance past unnecessary length prefix */
    pkeystr2++;	/* ...					  */

    /* repeat once for each component of the key	*/
    /* comparing key comp1 with key comp2 loop */
    for(i = 0; i < numcomps; i++,pkeystr1++, pkeystr2++)
    {
	/* check for unusual values */
	if ( *(TTINY*)pkeystr1 > LKYFLAG || *(TTINY*)pkeystr2 > LKYFLAG )
        {
	    if ( *(TTINY*)pkeystr1 == *(TTINY*)pkeystr2 )continue;
	    else return i;
        }

	if (*(TTINY*)pkeystr1 != *(TTINY*)pkeystr2)    /* compare lengths */
			 return i;

	/* we must actually compare them */
	if (bufcmp( pkeystr1+1, pkeystr2+1,
			(int)*(TTINY*)pkeystr1) != 0)
	    return i;

	/* advance past the characters */
	pkeystr1 += *(TTINY*)pkeystr1;
	pkeystr2 += *(TTINY*)pkeystr2;
    }

    /* fall through bottom means an exact match found */
    return i;

}  /* end kycountdb */
#endif /* !NW386 */

#if 0
/* PROGRAM: kyccnt - count number of key components
 *
 * RETURNS:
 */
LOCALF int
kyccnt( TEXT *pkeystr)
{
	int	 i;
	TEXT	*endadd;

    endadd = pkeystr + (*(TTINY*)pkeystr) - 1;
    pkeystr++;	/* advance past unnecessary length prefix */

    /* repeat once for each component of the key	*/
    /* comparing key comp1 with key comp2 loop */
    for(i = 0; pkeystr != endadd; i++)
    {
	/* check for unusual values */
	if  (*(TTINY*)pkeystr > LKYFLAG ) { pkeystr++; continue; }
	pkeystr += ((*(TTINY*)pkeystr) + 1);
    }
    return i;

}  /* end kyccnt */
#endif

