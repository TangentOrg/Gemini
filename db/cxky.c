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

#include "bkpub.h"
#include "cxmsg.h"
#include "cxpub.h"
#include "cxprv.h"
#include "dsmpub.h"
#include "keypub.h"
#include "ompub.h"
#include "utspub.h"

#if 0
/************************************************************************/
/* PROGRAM: cxIxKeyCompare compares new fomat key (cx) to old format key (ix).
 *
 *      ************** changes to this procedure may require change to
 *              cxKeyToCx.  Both depend on the same new key format
 *              and any change in the key format should be reflected
 *              in both procedures.  For performance reasons
 *              they are not combined. *********
 *
 * RETURNS::    -1  if new < old,
 *              0   if new = old
 *              +1  if new > old
 *              +2  if new > old and old is a substring of new
 */
int
cxIxKeyCompare(
        dsmContext_t  *pcontext,
        TEXT          *pn,      /* to new key */
        int            ln,      /* length of new */
        TEXT          *po,      /* to length byte of 1st component of old key */
        int            lo)      /* length of old */
{
    TEXT        *pendold;       /* points to the end of the old key */
    TEXT        *pendnew;       /* points to the end of the new key */
    int         i;

    pendold = po + lo;
    pendnew = pn + ln;
    pn +=4; /* skip the index number */
    if (pn == pendnew)
        return -1;

    if (po == pendold)
    {
        /* the key is empty, used for default key */
        if (*pn == LOWESCAPE)
        {
            pn++;
            if (pn == pendnew)
                return -1;
            if (*pn == NULLCOMPONENT)
            {
                pn++;
                if (pn == pendnew)
                    return -1;
                if (*pn == COMPSEPAR)
                    return 0;
                else
                    return 1;
            }
            else
                return((*pn < NULLCOMPONENT) ? -1 : 1);
        }
        else
            return((*pn < LOWESCAPE) ? -1 : 1);
    }
    else
    do
    {
        i = *po++;                      /* component's length or flag */
        if (i <= (int)LKYFLAG)          /* it is an ordinary component */
        {
            if (i == 0)                 /* component's size is zero */
            {
                if (*pn == LOWESCAPE)
                {
                    pn++;
                    if (pn == pendnew)
                        return -1;
                    if (*pn != NULLCOMPONENT)
                        return((*pn < NULLCOMPONENT) ? -1 : 1);
                    pn++;               /* to NULL separator */
                }
                else
                    return((*pn < LOWESCAPE) ? -1 : 1);
                /* NULL component */
            }
            else
            for (; i > 0; i--, po++, pn++)
            {
                if (pn == pendnew)      /* new key ended before the old key */
                    return -1;
                if ((TEXT)(*po + 1) < (TEXT)0x03)
                /* the +1 will change 0xff to 0x00, 0x00 to 0x01, 0x01 to 0x02*/
                {
                    if (*po == 0xff)
                    {
                        if (*pn != HIGHESCAPE)
                            return -1;
                        pn++;
                        if (pn == pendnew)
                            return -1;
                        if (*pn != REPLACEFF)
                            return((*pn < REPLACEFF) ? -1 : 1);
                    }
                    else
                    if (*pn != LOWESCAPE)
                        return((*pn < LOWESCAPE) ? -1 : 1);
                    else
                    if (*po == 0x01)
                    {
                        pn++;
                        if (pn == pendnew)
                            return -1;
                        if (*pn != REPLACEONE)
                            return -1;
                    }
                    else
                    {
                        /* *po is 0x00 */
                        pn++;
                        if (pn == pendnew)
                            return -1;
                        if (*pn != REPLACEZERO)
                            return((*pn < REPLACEZERO) ? -1 : 1);
                    }
                }
                else
                {
                    if (*pn != *po)
                        return((*pn < *po) ? -1 : 1);
                }
            }
        }
        else
        {
            /* it is a special flag */
            switch (i)
            {
            case  LOWRANGE:
                return 1;
            case HIGHRANGE:
                return -1;
            case HIGHMISSING:
                if (*pn != HIGHESCAPE)
                    return -1;
                pn++;
                if (pn == pendnew)
                    return -1;
                if (*pn != HIGHMISSING)
                    return((*pn < HIGHMISSING) ? -1 : 1);
                break;
            case LOWMISSING:
                if (*pn != LOWESCAPE)
                    return((*pn < LOWESCAPE) ? -1 : 1);
                pn++;
                if (pn == pendnew)
                    return -1;
                if (*pn != LOWMISSING)
                    return((*pn < LOWMISSING) ? -1 : 1);
                break;
            default:
                FATAL_MSGN_CALLBACK(pcontext, cxFTL011, i);
            }
            pn++;                       /* to NULL separator */
        }

        /* end of old key's component */
        if (pn == pendnew)              /* end of new key */
        {
            if (po == pendold)          /* end of old key, too */
                return 0;
            return -1;                  /* old key is longer */
        }
        if (*pn != COMPSEPAR)           /* new key component didn't end: */
        {                               /* new key component is longer.  */
            /* old key ended, or has an additional LOWRANGE or HIGHRANGE */
            if (po == pendold || (po == pendold - 1 && *po > LKYFLAG))
                return 2;               /* old key is a substring of new key */
            return 1;                   /* old key is smaller */
        }
        pn++;                           /* to next component of new key */
    }
    while (po != pendold);              /* scan the old key components */

    /* end of old key */
    if (pn == pendnew)                  /* end of new key, too */
        return 0;
    return 2;
}
#endif

#if 0
/****************************************************************************/
/* PROGRAM: cxIxkeyUnknown - check whether an old key contains an UNKNOWN component
 *
 * RETURNS: 0
 *          1
 */
int
cxIxKeyUnknown(DITEM    *poldkey)
{
    TEXT        *po;            /* pointer into the key */
    TEXT        *pend;          /* points to the end of the old key */
    int         i;

    po = poldkey->f.key.keystr;
    pend = po + *po - 1;
    po++;
    while (po < pend)
    {
        i = *po++;              /* to component's length or flag */
        if (i <= (int)LKYFLAG)  /* it is an ordinary component */
            po += i;
        else if (i == HIGHMISSING || i == LOWMISSING)
            return 1;
    }

    return 0;
}
#endif

/***********************************************************************/
/* PROGRAM: cxNewKeyCompare - compares 2 potentially different length Keys.
 *          The length of the strings are given. If one string
 *          is truncated version of other string, the shorter string
 *          will always compare low.
 *
 * RETURNS::    -1  if new < old,
 *              0   if new = old
 *              +1  if new > old
 *              +2  if new > old and old is a substring of new
 */
int
cxNewKeyCompare(TEXT    *pkey1, /* to key */
                int      lenKey1,       
                TEXT    *pkey2, /* to key */
                int      lenKey2)       
{
    TEXT *ptkey1 = pkey1 + 4;
    TEXT *ptkey2 = pkey2 + 4;
    int     min;
    TEXT    *pend;
    TEXT    *p;

    lenKey1 -= 4; /* adjust keys for 4 byte index number */
    lenKey2 -= 4;

    /* HIRANGE and LOWRANGE components should not be compared */
    /* scan the second key and adjust the lenghts to avoid comparing them */
    /* Assuming: only key2 has HIGHRANGE or LOWRANGE */ 
    /* Assuming that only the last component can be HIGHRANGE or LOWRANGE */

    pend = ptkey2 + lenKey2 - 1; /* last byte of the key */
    for ( p = pend; p >= (ptkey2 + 2) ; p--)
    {
        if (( *p == COMPSEPAR) && (p != pend))
            break;  /* last component is not HIGHRANGE */

        if ( *p == HIGHRANGE && *(p - 1) == HIGHESCAPE && *(p - 2) == COMPSEPAR)
        {
            int lng = p - 2 - ptkey2;
            if( lng <= lenKey1 )
            if (*(ptkey1 + lng) != COMPSEPAR)
            {
                lenKey2 = lng;
            }
            break;
        }

        if ( *p == LOWRANGE && *(p - 1) == LOWESCAPE && *(p - 2) == COMPSEPAR)
        {
            int lng = p - 2 - ptkey2;
            if( lng <= lenKey1 )
            if (*(ptkey1 + lng) != COMPSEPAR)
            {
                lenKey2 = lng;
            }
            break;
        }
    }

    if ( lenKey1 <= 0 )
        return (-1);  /* the first key has no text string */ 
    
    /* following code copied from cxIxKeyCompare above and should be reviewed */
    if ( lenKey2 <= 0 )
    {
        /* the key is empty, used for default key */
        if (*ptkey1 == LOWESCAPE)
        {
            ptkey1++;
            lenKey1--;
            if (lenKey1 == 0)
                return -1;
            if (*ptkey1 == NULLCOMPONENT)
            {
                ptkey1++;
                lenKey1--;
                if (lenKey1 == 0)
                    return -1;
                if (*ptkey1 == COMPSEPAR)
                    return 0;
                else
                    return 1;
            }
            else
                return((*ptkey1 < NULLCOMPONENT) ? -1 : 1);
        }
        else
            return((*ptkey1 < LOWESCAPE) ? -1 : 1);
    }

    /* do a byte by byte comparison of the shorter length */
    if ( lenKey1 < lenKey2 )
        min = lenKey1;
    else
        min = lenKey2;

    for (pend = ptkey2 + min; ptkey2 != pend; ptkey1++, ptkey2++)
    {
        /* loop through both strings until inequality found */
        if (*ptkey2 != *ptkey1)
                return *ptkey2 > *ptkey1 ? -1 : 1;
    }
    if (lenKey2 == lenKey1)
        return 0;

    /* if key2 is longer than key 2 cantains key 1 */
    return lenKey2 > lenKey1 ? -1 : 2;
}

/***********************************************************************/
/* PROGRAM: cxKeyCompare - compares 2 potentially different length strings.
 *          The length of the strings are given. If one string
 *          is truncated version of other string, the shorter string
 *          will always compare low.
 *
 * RETURNS: -EQUALLEN if t < s, 0 if t = s, +EQUALLEN if t > s
 *          EQUALLEN is the legth of the common prefix
 */
int
cxKeyCompare (TEXT      *t,
              int        lt, /* length of t */
              TEXT      *s,
              int        ls) /* length of s */
{
                int     min;
                TEXT    *pend;
                TEXT    *savs;

    savs = s;
    min = (ls < lt ? ls : lt);          /* the shorter length */

    for (pend = t + min; t != pend; s++, t++)
    {
        /* loop through both strings until inequality found */
        if (*t != *s)
                return *t > *s ? s - savs : savs - s;
    }
    if (lt == ls)
        return 0;
    return lt > ls ? min : -min;
}
#if 0
/***********************************************************************/
/* PROGRAM: cxKeyFlag - test if last component of a key is a flag.
 *
 * RETURNS: 1 if yes
 *          0 if no.
 */
int
cxKeyFlag(TEXT  *pkey)
{
         TEXT           *p;
         TEXT           *plast;

    p = pkey;
    plast = p + p[0] - 2;       /* point to the last char of last component */
    p++;
    while (p < plast)
    {
        if (*p <= LKYFLAG)
            p += *p;
        p++;
    }
    if (p == plast && *p > LKYFLAG)     /* last element is a flag */
        return 1;
    return 0;
}
#endif
#if 0
/****************************************************************************/
/* PROGRAM: cxKeyLimit - generates a low or high key ditem in 'pnew' from the
 *      data in 'pold' and the key in 'pkey'.
 *
 *      'pold' is a key ditem.
 *      'pnew' points to an empty key ditem, allocated by the caller.
 *          NOTE: the size of its key area must be five (5) bytes longer than
 *          the size of the key in 'pold'.
 *      'pkey' points to the key.  The first byte contains the key size
 *          (excluding the size byte itself).
 *      'flag' indicates whether to build a high or low limit key, and
 *          whether it defines a range (a wildcard).
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxKeyLimit(struct  ditem        *pnew,
           struct  ditem        *pold,
                   TEXT         *pkey,
                   int           flag)
{
         unsigned int   keylen;
         TEXT           *pnk;           /* pointer to the new key */

    keylen = (unsigned int)*pkey + 4;
    if((flag & (CX_HIGHRANGE | CX_WILD)) == (CX_HIGHRANGE | CX_WILD))
        keylen++;               /* wildcard high range needs one more char */

    bufcop(pnew, pold, sizeof(struct ditem));   /* copy the ditem itself */
    pnew->dpad -= keylen;

    pnk = pnew->f.key.keystr;
    pnk[0] = pnk[keylen - 1] = keylen;
    bufcop(pnk + 1, pkey, (unsigned int)*pkey + 1);
    if((flag & (CX_HIGHRANGE | CX_WILD)) == (CX_HIGHRANGE | CX_WILD))
    {                           /* wildcard high range needs one more char */
        pnk[1]++;               /* increment the key size */
        pnk[keylen - 3] = 0XFF;
    }
    pnk[keylen - 2] = ((flag & CX_LOWRANGE) != 0 ? LOWRANGE : HIGHRANGE);
}
#endif
#if 0
/****************************************************************************/
/*     Not yet  ready for large keys??????????????????????????? */
/* PROGRAM: cxKeyLimitNew - generates a low or high key key in 'pnew' from the
 *      data in the cx format 'pold' and the key in 'pkey'.  This will only work
 *      it the key has only one component.
 *
 *      'pold' is a key struct.
 *      'pnew' points to an empty key struct, allocated by the caller.
 *          NOTE: the size of its key area must be five (5) bytes longer than
 *          the size of the key in 'pold'.
 *      'pkey' points to the key.  The first byte contains the key size
 *          (excluding the size byte itself).
 *      'flag' indicates whether to build a high or low limit key, and
 *          whether it defines a range (a wildcard).
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxKeyLimitNew(dsmKey_t  *pnew,
              dsmKey_t  *pold,
              TEXT      *pkey,
              int        flag)
{
         unsigned int   keylen;
         TEXT           *pnk;           /* pointer to the new key */

    keylen = (unsigned int)*pkey + 2;
    if((flag & (CX_HIGHRANGE | CX_WILD)) == (CX_HIGHRANGE | CX_WILD))
        keylen++;               /* wildcard high range needs one more char */

    bufcop(pnew, pold, sizeof(dsmKey_t ));      /* copy the structure itself */

    pnk = pnew->keystr;
    bufcop(pnk, pkey, (unsigned int)*pkey + 3 );
    pnk[0] = pnk[1] = keylen;
    if((flag & (CX_HIGHRANGE | CX_WILD)) == (CX_HIGHRANGE | CX_WILD))
    {                           /* wildcard high range needs one more char */
        pnk[keylen] = 0XFF;
    }
    if ( (flag & CX_LOWRANGE) != 0 )
        pnk[keylen] = 0x00;
    else
        pnk[keylen - 1] = HIGHRANGE;
}

#endif

/***************************************************************************/
/* PROGRAM: cxKeyToStruct - take a pointer to a key (new format)
 *                          and build a dsmKey_t structure 
 *
 * RETURNS: 0
 */
int
cxKeyToStruct (dsmKey_t *pkkey, /* points to the ditem, must be big enough */
              TEXT      *pkey)    /* points to the key which will be extracted  */
{
/*#ifdef LARGE_INDEX_ENTRIES_DISABLED */
/*    pkkey->index = xct(pkey + ENTHDRSZ + 2); */
/*    bufcop(pkkey->keystr, pkey, *(TTINY*)pkey); */
/*#else */
    pkkey->index = xct(pkey + FULLKEYHDRSZ + 2);
    bufcop(pkkey->keystr, pkey, (int)xct(&pkey[TS_OFFSET]));
/*#endif   LARGE_INDEX_ENTRIES_DISABLED */ 
    return(0);
}

#if 0                                  
/**********************************************************************/
     Not yet  ready for long keys??????????????????????????? */
/* PROGRAM: cxKeyTruncate - check if a key will become longer than maxkey after
    conversion to the new format, and return the max no. of characters
    that can be used from it.

    Used for truncation word keys.

    Returns: no. of chars of the key to use.
*/
int
cxKeyTruncate(TTINY       *pw,            /* points to the word.  1'st char is length */
         int         maxkey)
{
    unsigned    wlen;
    TTINY       *p;
    TTINY       *plast;

    wlen = 0;
    p = pw;
    plast = &p[*p];
    while (p < plast)
    {
        p++;
        wlen++;
        if ((TTINY)(*p + 1) <= (TTINY)2)
            wlen++;
        if (wlen > (unsigned)maxkey)
            return ((int)(p - pw) - 1);
    }
    return (int)(pw[0]);        /* the whole key fits */

}
#endif

/******************************************************************/
/* PROGRAM: cxKeyUnknown - check if a new key has an UNKNOWN component
 *
 * RETURNS: 0
 *          1
 */
int
cxKeyUnknown(TEXT *pnew)        /* ditem with the key in new format */
{
        TEXT    *ptr = 0;
        TEXT    *pend = 0;

    ptr = pnew;
/*    pend = ptr + ptr[1] + 3; */
/*    ptr += 3 + 4;        to start of key (1st component) */ 
    pend = ptr + xct(&ptr[KS_OFFSET]) + FULLKEYHDRSZ;
    ptr += FULLKEYHDRSZ + 4;       /* to start of key (1st component) */
    while (ptr <  pend)
    {
        if (*ptr == HIGHESCAPE)
        {
            ptr++;
            if (ptr == pend)
                return 0;
            if (*ptr == HIGHMISSING)
                return 1;
        }
        else
        if (*ptr == LOWESCAPE)
        {
            ptr++;
            if (ptr == pend)
                return 0;
            if (*ptr == LOWMISSING)
                return 1;
        }
        /* this component is ok skip to the next one */
        while (*ptr != 0)
        {
            if (ptr++ == pend)
                return 0;
        }
        if (ptr++ == pend)
            return 0;
    }
    return 0;
}

#if 0
/******************************************************************/
/* PROGRAM: cxConvertMode - convert mode to new mode
 *
 * RETURNS: DSMFINDFIRST 
 *          DSMFINDLAST
 */
int
cxConvertMode(int mode, int *type)      /* ditem with the key in new format */
{
    if ( mode == 0 ) 
    {
        /* this is a default mode used in dbfnd.  Seting the values this
           way insures that they will work as they did before by default.
        */
        *type = DSMUNIQUE;
        return ( DSMFINDFIRST );
    }

    /* find an exact key including the dbkey */
    if (( mode & FNDDBKEY ) == FNDDBKEY )
    {
        *type = DSMDBKEY;
        return ( DSMFINDFIRST );
    }
    else
    /* find unique */
    if (( mode & FINDQ ) == FINDQ )
    {
        *type = DSMUNIQUE;
        if (mode & (FINDFRST+FNDFRST+FNDNEXT ))
            return ( DSMFINDFIRST );
        else
            return ( DSMFINDLAST );
    }
    else
    {
        /* defaule to partial */
        *type = DSMPARTIAL;
        if (mode & (FNDFRST+FNDNEXT))
            return ( DSMFINDFIRST );
        else
            return ( DSMFINDLAST );
    }
 
}
#endif

/* PROGRAM: cxGetIndexInfo - Access object cache to get root and
                             index attributes for the index.         */
dsmStatus_t
cxGetIndexInfo(dsmContext_t *pContext, dsmObject_t tableNum, dsmKey_t *pKey)
{
    dsmStatus_t     rc;

    omDescriptor_t  indexInfo;
    
    rc = omFetch(pContext,DSMOBJECT_MIXINDEX, pKey->index, tableNum, &indexInfo);

    if( rc )
        return rc;

    pKey->area = indexInfo.area;
    pKey->root = indexInfo.objectRoot;
    pKey->unique_index = (dsmBoolean_t)indexInfo.objectAttributes &
                                       DSMINDEX_UNIQUE;
    pKey->word_index   = (dsmBoolean_t)indexInfo.objectAttributes &
                                       DSMINDEX_WORD;

    return rc;
}


/* PROGRAM: cxDsmKeyComponetToLong - convert stored key value
 *
 * NOTE: We are assuming NEW STYLE KEY format where only the
 *       first key component is of interest.
 *
 * Attempt to convert from sorted value back to "real value"
 * (The sorted value was created by sortint().)
 *
 * The converted key value from cxConvertToIx() is in the ix format of:
 * keystr[0] is the length of the key data value
 * keystr[1] is the start of the stored key data value (no terminator)
 *
 * The new key format is:
 * keystr[0] is the length of the entire sort key value
 * keystr[1] is the length of the key data value
 * keystr[2] is garbage at this point
 * keystr[3] is zero
 * keystr[4] is zero
 * keystr[5-6] is the index number - note however that negative
 *             index numbers are not properly padded, so get
 *             the index number from dsmkey->index.
 * keystr[7] start of 1st null terminated component
 *
 * 1. Convert new key to old key format
 * 2. Special case length of 1 to be value of 0
 # 3. Determine if positive or negative value
 * 4. Perform the reverse of sortint().
 * 5. Return converted value.
 *
 * RETURNS: "external" dbkey data value extracted from dsmKey_t structure
 *          -1 on error (if not key type ditem
 */
DBKEY
cxDsmKeyComponentToLong( TEXT *pdsmKeyStr /* Pointer to component */ ) 
{
  LONG value;       /* converted value */
  TEXT keystr[KEYSIZE] = {0};
  TEXT pos_test;    /* temp var used in pos/neg test */
  TEXT neg_test;    /* temp var used in pos/neg test */

  /* Convert new key structure back to old format */
  keyConvertCxToIxComponent(keystr,pdsmKeyStr);

  /* special case key length of 1 to be value of 0 */
  if ( (keystr[0] == 0) ||
       ((keystr[0] == 1) && (keystr[1] == 0xff || keystr[1] == 0x00)) )
     return 0;

  value = 0;
  pos_test = (keystr[0] + 4) << 4;
  if ( (keystr[1] & pos_test) == pos_test)
  {
    /* POSITIVE VALUE */
    keystr[1] &= (pos_test ^ 0xff);
  }
  else
  {
    /* the following ensures that a non-positive # is a negative # */
    neg_test = (5 - keystr[0]) << 4;
    if ( (keystr[1] & neg_test) == neg_test)
    {
        /* set up mask depending on length of variable length stored value
         * (Assuming 8 bits/byte and 4 bits/nibble)
         */
        value = ((LONG)-1 >> ((keystr[0] * 8) - 4) ) <<
                  ((keystr[0] * 8) - 4);
     }
  }

  /* get long value from ditem's variable length storage */
  value |= xlngv(&keystr[1], keystr[0]);

  return ((DBKEY)value);

}  /* end cxDsmKeyComponetToLong */


