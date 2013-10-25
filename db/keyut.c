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
#include "ditem.h"
#include "keypub.h"
#include "keyprv.h"
#include "cxpub.h"
#include "utspub.h"

/* macro to check that there's enough room in the target to hold next chunk
 * of bytes of keystring  (used in keyConvertIxToCx() )
 */
#define KEY_LENGTH_CHECK(x,restBufferSize,bufferSize)  \
                                if ( (restBufferSize -= (x)) <= 0 \
                                  && (bufferSize != KEY_NO_LENGTH_CHECK ) ) \
                                    return KEY_BUFFER_TOO_SMALL;


/**** Functions with Prototypes in keyprv.h ********************************
 ***
 ***  keyConvertCxToIx         converts keystr: new cx format to old ix format
 ***  keyConvertIxToCx         converts keystr: old ix format to new cx format
      - to replace cxkyut:cxKeyToCxStr
      - also: cxKeyToCx is to be eliminated by 
      -   * changing the comment in cxky.c and cxnxt.c
      -   * add a comment in cxgen.c (used in an "#if 0"-ed code-section)
 ***  keyInitKDitem            initializes a ditem as a key ditem (V6 format)
 ***  keyReplaceSDitemInKDitem inserts a data item into a key (V6 format)
 ***
 ***************************************************************************/


/***************************************************************************/
/***                   KeyServices External Functions                    ***/
/***************************************************************************/
#if 0
/***************************************************************************/
/* PROGRAM: keyConvertCxToIx - converts keystr: new cx format to old ix format
 *
 *      skip first 7 bytes (KS,CS,IS,x00,x00,IxNo)
 *      x'01fe' - will be replaced with escape sequence x'00'
 *      x'01ff' - will be replaced with escape sequence x'01'
 *      x'ff01' - will be replaced with escape sequence x'ff'
 *      put keystr-length into first and last byte
 *
 * RETURNS: DSM_S_SUCCESS
 *          KEY_EXCEEDS_MAX_SIZE
 *          returnCodes from keyConvertCxToIxComponent
 */
/*?????????????????? ix format cann't hold large keys - this proc is used for VST small keys only  */
keyStatus_t
keyConvertCxToIx(
        TEXT *pt,         /* OUT - target area - old format (v6, ix) */
        int   keysize,     /* IN  - Size of source area (incl 3 bytes info */ 
        TEXT *ps)         /* IN  - source area - new format (v7, cx) */
{
    int          ix_key_len = 2;
    int          ix_comp_len;
    int          cx_key_len;
    int          cx_comp_len;
    TEXT        *pCx = ps;
    TEXT        *pIx = pt;
    keyStatus_t  rc = KEY_SUCCESS;

    /* CX --> IX */

/*    pCx       += 7;            Skip CS,KS,IS, 2 nulls & the index number */ 
    pCx += FULLKEYHDRSZ + 4; /* Skip CS,KS,IS, 2 nulls & the index number */
/*    cx_key_len = (int) keysize - 7;  */
    cx_key_len = (int) keysize - (FULLKEYHDRSZ + 4); 

    /* Save space for the length [byte] at the beginning */

    pIx++;

    while (cx_key_len > 0) 
    {
        cx_comp_len = stlen (pCx) + 1;
        
        /* Use cx_comp_len to estimate space required for the IX length
         * byte and the IX component value.  CX value + null byte will
         * always be >= sizeof (IX value) + IX length byte.  IX value may
         * take less space because several two byte escape sequences become 
         * a single byte.
         */

        if ( (rc = keyConvertCxToIxComponent(pIx, pCx)) != KEYSUCCESS)
            return rc;

        pCx        += cx_comp_len;

        ix_comp_len = *pIx + 1;         /* +1 for the length [byte] */
        pIx        += ix_comp_len;
        ix_key_len += ix_comp_len;
        
        cx_key_len -= cx_comp_len;
    }

    if (ix_key_len > KEYSIZE)
        return KEY_EXCEEDS_MAX_SIZE;

    *pt   = (TEXT) ix_key_len; /* fill in the length byte at the beginning */
    *pIx  = (TEXT) ix_key_len; /* ...and the end */
    
    return KEY_SUCCESS;

}  /* end keyConvertCxToIx */

#endif
/***********************************************************************/
/* PROGRAM: keyConvertCxToIxComponent - converts a component from cx format 
 *                                      to ix format
 * 
 *          converts one component (NULL terminated) from new cx format
 *          back to the old ix component (1st byte is the length)
 *
 * RETURNS: DSM_S_SUCCESS - success
 */
keyStatus_t
keyConvertCxToIxComponent(
        TEXT  *pt,    /* to target area - old format */
        TEXT  *ps)    /* to source area - new format */
{
        int      len=0;
        TEXT    *plenbyte;
 
    for ( plenbyte = pt++; *ps; ps++, pt++, len++)
    {
        if (*ps == LOWESCAPE)
        {
            ps++;
            if(*ps == REPLACEZERO)
                *pt = 0;
            else
            if(*ps == NULLCOMPONENT)    /* null component */
            {
                pt--;
                len--;
            }
            else
            if(*ps == REPLACEONE)
                *pt = 1;
            else
            if (*ps == LOWMISSING)
                *pt = LOWMISSING;
        }
        else
        if (*ps == HIGHESCAPE)
        {
            ps++;
            if (*ps == REPLACEFF)
                *pt = 0xff;
            if (*ps == HIGHMISSING)
                *pt = HIGHMISSING;
        }
        else
        {
            *pt = *ps;
        }
    }

    *plenbyte = len;

    return KEY_SUCCESS;

}  /* end keyConvertCxToIxComponent */


/***********************************************************************/
/* PROGRAM: keyConvertIxToCx - converts keystr: old ix format to new cx format
 *
 *      This function overwrites only the following members of the dsmKey_t 
 *      structure, and leaves all the other members alone:
 *      * keystr
 *      * keyLen
 *      * unknown_comp
 *
 *      New Format:
 *      1st byte - total length of key and info.
 *      2nd byte - size of the key (ks)
 *      3rd byte - size of the info (is)
 *      followed by the key, followed by the info.
 *      
 *      The new cx format uses some special characters.
 *      The special characters are as follows:
 *
 *      x'00' - component separator, will sort lower that any value.
 *      x'01' - low escape character.
 *      x'ff' - High escape character.
 *
 *      When a special character is found in a regular component of a key,
 *      it will be replaced with an escape sequence that will preserve the
 *      collating sequence, as follows:
 *
 *      x'00' - will be replaced with escape sequence x'01fe'
 *      x'01' - will be replaced with escape sequence x'01ff'
 *      x'ff' - will be replaced with escape sequence x'ff01'
 *
 *      Additional escape sequences are:
 *
 *      x'0101' LOW-RANGE value, it will sort higher than component separator
 *                      but lower than any other value except.
 *                      NOTICE: a LOWRANGE in the old index, if not in the
 *                      1st component will cause the following conversion:
 *                      the last byte of the previous component will be
 *                      decremented by 1, and a HIGHRANGE value will be
 *                      concatennated to it.
 *      x'01f0' NULL component like "" char value, it will sort higher than
 *                      component separator and higher than LOW-RANGE, but
 *                      less than any other value.
 *      x'01fb' LOW-UNKNOWN value, it will sort higher than component separator
 *                                      but lower than any other value.
 *      x'fffc' HIGH-UNKNOWN value, it will sort higher than component separator
 *                      and higher than any other value except HIGH-RANGE.
 *      x'ffff' HIGH-RANGE value, it will sort higher than component separator
 *                      but higher than any other value.
 *
 * RETURNS: DSM_S_SUCCESS
 *          KEY_BUFFER_TOO_SMALL
 *          KEY_ILLEGAL  (-0xfa) if an illegal component was found.
 */
keyStatus_t
keyConvertIxToCx(
        dsmKey_t       *pdsmKey,        /* OUT storamge manager key structure */
        dsmIndex_t      indexNumber,    /* IN  object-number of index*/
        dsmBoolean_t    ksubstr _UNUSED_, /* IN  substring ok bit */
        dsmBoolean_t    wordIndex,      /* IN  is this for a word index (i.e.
                                         *     keyString is list of words)
                                         */
        TEXT            keyString[],    /* IN  keyString in IX format */
        LONG            bufferSize)     /* IN  max possible length for keystr */
{

    LONG         availableBuffer = 0x0fff;  /* for check before adding more
                                             * bytes to keystr
                                             */
    TEXT        *pce;           /* points to the end of a source component */
    TEXT        *pend;          /* points to the end of the source key */
    TEXT        *pfrom;         /* points to the source key */
    TEXT        *pto;           /* points to the target key */
    TEXT         unknown    = 0;
    TEXT        *p_start_to;
    TEXT        *pWordStart = 0;
    
 
    pdsmKey->unknown_comp = 0;
    pto                   = pdsmKey->keystr;
    if ( bufferSize != KEY_NO_LENGTH_CHECK )
        availableBuffer = bufferSize - keyString[0];

/******* this proc is used for small keys only in MySQL****/    
    if ( wordIndex )
    {       /* word-index */
/*#ifdef LARGE_INDEX_ENTRIES_DISABLED */
        pfrom = keyString + 1;
        pend  = pfrom + *pfrom + 1; /* word list len does not include trailing
                                     * length byte.
                                     */
/*#else */
      /*** ????????????????????????????***/
/*#endif   LARGE_INDEX_ENTRIES_DISABLED */ 
    }       /* word-index */
    else
    {       /* !word-index */
        pfrom = keyString;
/*#ifdef LARGE_INDEX_ENTRIES_DISABLED */
	/*        KEY_LENGTH_CHECK(6,availableBuffer,bufferSize); */
	/*        pto   += 3;  skip the 1st byte which is used for the total size */ 
                    /* skip the 2nd byte which is used for the key size */
                    /* skip the 3nd byte which is used for the info size */
/*#else */
        KEY_LENGTH_CHECK(FULLKEYHDRSZ+4,availableBuffer,bufferSize);
        pto   += FULLKEYHDRSZ;   /* skip header with all the sizes */
/*#endif   LARGE_INDEX_ENTRIES_DISABLED */ 
 
        *pto++ = 0; /* mark the new format & define the type of the index = 0 */
        *pto++ = 0; /* mark the new format */
        sct(pto, indexNumber);  /* index number */
        pto += 2;
 
/*#ifdef LARGE_INDEX_ENTRIES_DISABLED */
        pend = pfrom + *pfrom - 1;  /* the key lng byte at the end */
    }       /* !word-index */

    pfrom++;    /* to 1st component */
/*#else */
/*       ?????? */
/*#endif   LARGE_INDEX_ENTRIES_DISABLED */ 

    if (pfrom == pend)          /* the key is empty, used for default key */
    {
        KEY_LENGTH_CHECK(2,availableBuffer,bufferSize);
        *pto++ = LOWESCAPE;
        *pto++ = NULLCOMPONENT;
        *pto++ = 0;
        goto done;
    }
 
    p_start_to = pto;           /* start of new key */

    do
    {   /* while (pfrom != pend); / * scan the key components and convert */

        if ( wordIndex )
        {       /* word-index */
            /* For each word in the ditem we create a whole key string.    */
            pWordStart = pto;
/*#ifdef LARGE_INDEX_ENTRIES_DISABLED */
            KEY_LENGTH_CHECK(6,availableBuffer,bufferSize);
            pto +=3; /* skip the 1st byte which is used for the total size */
                     /* skip the 2nd byte which is used for the key size */
                     /* skip the 2nd byte which is used for the info size */
/*#else */
/*            KEY_LENGTH_CHECK(FULLKEYHDRSZ+4,availableBuffer,bufferSize); */
/*            pto   += FULLKEYHDRSZ;    skip header with all the sizes */ 
/*#endif   LARGE_INDEX_ENTRIES_DISABLED */ 
 
            *pto++ =0; /* mark new format & define the type of the index = 0 */
            *pto++ =0; /* mark the new format */
            sct(pto, indexNumber);      /* index number */
            pto += 2;
        }       /* word-index */
 
        /* scan all the key components and convert */
        if (*pfrom <= LKYFLAG)          /* it is an ordinary component */
        {
            if (*pfrom == 0)            /* a null component */
            {
                KEY_LENGTH_CHECK(1,availableBuffer,bufferSize);
                *pto++ = LOWESCAPE;
                *pto++ = NULLCOMPONENT;
                pfrom++;
            }
            else
            for (pce = pfrom + *pfrom, pfrom++; pfrom <= pce; pfrom++, pto++)
            {
                /* NOTE: the following checks whether ch equals 0xff, 0 or 1 */
                /* It relies on overflow to change the range to 0, 1, 2 so   */
                /* that it can be done in one if statement.   This is rather */
                /* dirty, but since the key conversion is called frequently, */
                /* it seems to be worth it */
                if ((TEXT)(*pfrom + 1) > (TEXT)2)
                {
                    *pto = *pfrom;
                }
                
                else if (*pfrom < (TEXT)2)
                {
                    KEY_LENGTH_CHECK(1,availableBuffer,bufferSize);
                    *pto++ = LOWESCAPE;
                    *pto = REPLACEZERO + *pfrom; /* REPLACEZERO or REPLACEONE */
                }
                else            /* *pfrom must be 0xff */
                {
                    KEY_LENGTH_CHECK(1,availableBuffer,bufferSize);
                    *pto++ = HIGHESCAPE;
                    *pto = REPLACEFF;
                }

            }   /* for each byte in the string */

        }                               /* it is an ordinary component */

        else                            /* it is a special flag */
        {
            switch (*pfrom)
            {
            case  LOWRANGE:
                if (pto != p_start_to)  /* not 1'st component */
                    pto--;
                break;
            case HIGHRANGE:
                KEY_LENGTH_CHECK(1,availableBuffer,bufferSize);
                *pto++ = HIGHESCAPE;
                *pto++ = HIGHRANGE;
                break;
            case HIGHMISSING:
                KEY_LENGTH_CHECK(1,availableBuffer,bufferSize);
                *pto++ = HIGHESCAPE;
                *pto++ = HIGHMISSING;
                unknown=1;
                break;
            case LOWMISSING:
                KEY_LENGTH_CHECK(1,availableBuffer,bufferSize);
                *pto++ = LOWESCAPE;
                *pto++ = LOWMISSING;
                unknown=1;
                break;
            case KEY_ILLEGAL:
                return KEY_ILLEGAL; /* illegal key */
            default:
                return KEY_ILLEGAL; /* illegal key */
            }

            pfrom++;

        }                            /* it is a special flag */

        /* end of component */

        *pto++ = COMPSEPAR;     /* null separator */

        if ( wordIndex )
        {
            /* Add in the length for a word list key    */
/*#ifdef LARGE_INDEX_ENTRIES_DISABLED */
            *pWordStart = pto - pWordStart - 3;
            *(pWordStart + 1) = pto - pWordStart - 3;
/*#else */
/*            sct(pWordStart + TS_OFFSET, pto - pWordStart - FULLKEYHDRSZ); */
/*            sct(pWordStart + KS_OFFSET, pto - pWordStart - FULLKEYHDRSZ); */
/*#endif   LARGE_INDEX_ENTRIES_DISABLED */ 
        }
 
    }  while (pfrom != pend); /* scan the key components and convert */
 
  done:
 
/*#ifdef LARGE_INDEX_ENTRIES_DISABLED */
    /*    pdsmKey->keyLen = pto - pdsmKey->keystr - 3; */
    /*    if ( !wordIndex ) */
    /*    { */
    /*  pdsmKey->keystr[0] = (TEXT)pdsmKey->keyLen; */
    /*  pdsmKey->keystr[1] = (TEXT)pdsmKey->keyLen; */
    /*} */
/*#else */
    pdsmKey->keyLen = pto - pdsmKey->keystr - FULLKEYHDRSZ;
    if ( !wordIndex )
    {
        sct(&pdsmKey->keystr[TS_OFFSET], pdsmKey->keyLen);
        sct(&pdsmKey->keystr[KS_OFFSET], pdsmKey->keyLen);
    }
/*#endif   LARGE_INDEX_ENTRIES_DISABLED */ 
    
    if (unknown)
        pdsmKey->unknown_comp = 1;
    else
        pdsmKey->unknown_comp = 0;
 
    return KEY_SUCCESS;

}  /* end keyConvertIxToCx */


/***********************************************************************/
/* PROGRAM: keyInitStructure - initializes dsmKey Structure
 * 
 * RETURNS:
 *      KEY_SUCCESS             dsmKey structure contains keystring 
 *      KEY_BAD_PARAMETER       pKey is NULL
 *      
 */
keyStatus_t
keyInitStructure(
        COUNT           keyMode _UNUSED_, /* IN     SVC???KEY */
        dsmIndex_t      indexNumber,    /* IN     object-number of index*/
        COUNT           keycomps,       /* IN     number of key components */
        COUNT           bufferLength,   /* IN     Length of key string */
        dsmBoolean_t    descending_key, /* IN     substring ok bit */
        dsmBoolean_t    ksubstr,        /* IN     substring ok bit */
        dsmKey_t       *pKey)           /* IN/OUT structure to initialize*/
#if 0
/* the following elements don't get passed in and assigned, becasue they are
 * opaque to the keyServices. There would need to be a dsm-function to
 * initialize them - is there?  (th)
 */
	dsmArea_t       area,           /* IN area the index is in     */
        dsmDbkey_t      root,           /* IN dbkey of root of the index   */	
        dsmBoolean_t    unique_index,   /* IN substring ok bit */
        dsmBoolean_t    word_index,     /* IN is this a word index */
#endif
{

    if ( pKey == NULL )
        return KEY_BAD_PARAMETER;
    
    pKey->index         = indexNumber;
    pKey->keycomps      = keycomps;
    pKey->keyLen        = 0;
    pKey->unknown_comp  = NULL;
    pKey->descending_key= descending_key;
    pKey->ksubstr       = ksubstr;
    
    if ( bufferLength != KEY_NO_LENGTH_CHECK )
        stnclr (pKey->keystr, bufferLength);

#if 0
    pKey->area          = area;
    pKey->root          = root;
    pKey->unique_index  = unique_index;
    pKey->word_index    = word_index;
#endif
    
    return KEY_SUCCESS;

}  /* end keyInitStructure */


/***************************************************************************/
/***                   KeyServices Internal Functions                    ***/
/***************************************************************************/

/*****************************************************************/
/* PROGRAM: keyInitKDitem - initialize a KDitem for key
 *
 * RETURNS: DSM_S_SUCCESS
 *          KEY_BUFFER_TOO_SMALL
 */
keyStatus_t
keyInitKDitem(
        struct ditem    *pKDitem,
        COUNT            indexNumber,
        COUNT            numberComponents,
        GTBOOL            substringOk)
{
TEXT    *pKeyString;
COUNT    i;
 
    TRACE("keyInitKDitem")
 
    /* initialize the ditem fields */
    pKDitem->type = TY_KEY;
    pKDitem->flag = 0;
    pKDitem->dflag2 = 0;
 
    /* initialize the key format part of the ditem */
    pKDitem->f.key.kidx         = indexNumber;
    pKDitem->f.key.kcomps       = numberComponents;
    pKDitem->f.key.ksubstr      = substringOk;
 
    /* make sure the keystring is long enough for the dummy key */
    /* BUM - assume numberComponents always > 0 */
    if ((UCOUNT)(numberComponents + 2) > pKDitem->dpad)
    {
        /* we can't fatal here! return error to caller instead. */
        return KEY_BUFFER_TOO_SMALL;
    }
 
    /* now initialize the dummy keystring */
    pKeyString = pKDitem->f.key.keystr;
    *(TTINY*)pKeyString = numberComponents + 2;
    for (i=0; i<numberComponents; i++)  *(TTINY*)++pKeyString = KYILLEGAL;
    *(TTINY*)++pKeyString = numberComponents + 2;
 
    return KEY_SUCCESS;
 
}  /* end keyInitKDitem */


/*****************************************************************/
/* PROGRAM: keyReplaceSDitemInKDitem - replaces/inserts an SDitem 
 *                                     into a key (V6 format)
 *
 * RETURNS: DSM_S_SUCCESS
 *          KEY_BUFFER_TOO_SMALL
 *          KEY_EXCEEDS_MAX_SIZE
 */
/*****************************************************************/
keyStatus_t
keyReplaceSDitemInKDitem(
        struct ditem    *pKDitem,          /* IN/OUT  Ditem holding Key */
        struct ditem    *pSDitem,          /* IN      Ditem holding 1 component */
        COUNT            componentNumber) /* IN      number of comp to replace */
{
    TEXT       *pKeyString;
    COUNT       oldLength;
    COUNT       newLength;
    COUNT       totalLength;
    COUNT       newTotalLength;
 
 
    TRACE2("keyReplaceSDitemInKDitem: index=%d, componentNumber=%d", pKDitem->f.key.kidx, componentNumber);
    TRACE2("keyReplaceSDitemInKDitem: ditem.type=%d, flag=%d", pSDitem->type, pSDitem->flag);
    /* find out how big the key is right now */
    pKeyString  = pKDitem->f.key.keystr;
    totalLength = *(TTINY*)pKeyString;
 
    /* decide how big the new data item to be inserted is */
    if ( pSDitem->flag & (LORANGE + HIRANGE + MISSING + LOW + HIGH + SQLNULL) )
         newLength = 1;                       /* length of flag byte */
    else
    {    
        newLength = pSDitem->SRTVLEN + 1;  /* length of data plus pfix */
        if (  (pSDitem->type == DSRTFRM) 
          &&  (pSDitem->flag & DSAFF)
          && !(pKDitem->flag & DESCIDX) )
            ++newLength;
    }
 
    /* skip key components to get to the component to be replaced */
    oldLength = 1;         /* seed to advance past key prefix byte */
    do 
    {
        pKeyString += oldLength; /* advance to next (first) component */
        oldLength   = 1;         /* allow for pfix byte of component  */
        if ( *(TTINY*)pKeyString < LKYFLAG )  /* if not a flag, then it's */
            oldLength += *(TTINY*)pKeyString; /*   the component's length */
    } while (--componentNumber > 0); /* go to correct component */
 

    /* check for overflow */
    newTotalLength = totalLength + newLength - oldLength;
    /* +5 because cx adds some additional overhead so here we are
       limited to five less than KEYSIZE  */
    if ( (newTotalLength+5) > (COUNT)pKDitem->dpad )
        return KEY_BUFFER_TOO_SMALL;
    if ( (newTotalLength+5) > KEYSIZE              )
        return KEY_EXCEEDS_MAX_SIZE;
 
    /* shove stuff over to make room */
    bufcop( pKeyString + newLength
          , pKeyString + oldLength
          , (int) ( totalLength 
                  - oldLength
                  - (pKeyString - pKDitem->f.key.keystr)
                  )
          );
 
    /* insert the new key component */
    if (pSDitem->flag & (LORANGE + HIRANGE + MISSING + LOW + HIGH + SQLNULL))
    {   /* special translate ditem flag value to key comp flag value */

        if (pSDitem->flag & LORANGE )
            *(TTINY*)pKeyString = LOWRANGE;

        else if (pSDitem->flag & HIRANGE)
            *(TTINY*)pKeyString = HIGHRANGE;

        else if (pSDitem->flag & LOW)
        {
            if (pSDitem->type == ASRTFRM)
                *(TTINY*)pKeyString = LOWMISSING;
            else 
                *(TTINY*)pKeyString = HIGHMISSING;
        }
        else /* HIGH | SQLNULL */
        {
            if (pSDitem->type == ASRTFRM)
                *(TTINY*)pKeyString = HIGHMISSING;
            else 
                *(TTINY*)pKeyString = LOWMISSING;
        }

    }   /* special translate ditem flag value to key comp flag value */

    else
    {   /* copy the character string in */
        *(TTINY*)pKeyString++ = (TEXT)(--newLength); /* length pfx byte */
        bufcop(pKeyString, pSDitem->f.srt.vch, (int) pSDitem->SRTVLEN);

        if (pSDitem->type == DSRTFRM)
        {   
            pKeyString += newLength;
            if (   pSDitem->flag & DSAFF
              && !(pKDitem->flag & DESCIDX)
               )
            {   
                *--pKeyString = (TEXT) 0xff;  /* pad x'ff' */
                --newLength;
            }
            while (--newLength >= 0)
                *--pKeyString ^= (TEXT) 0xff; /* toggle all bits */
        }

    }   /* copy the character string in */
 
    /* patch the key length prefix and suffix */ 
    /* NOTE: assume newTotalLength < 256 */
    pKDitem->f.key.keystr[0]                = (TEXT)newTotalLength;
    pKDitem->f.key.keystr[newTotalLength-1] = (TEXT)newTotalLength;
 
    return KEY_SUCCESS;
 
}  /* end keyReplaceSDitemInKDitem */
 

/***************************************************************************/
/***                           Local Functions                           ***/
/***************************************************************************/

/***********************************************************************/

