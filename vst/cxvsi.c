
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

#include "dbconfig.h"   /* standard dbmgr header */

#include "bkpub.h"      /* TODO: there is order dependency here */
#include "dsmpub.h"     /* private interface */
#include "cxpub.h"      /* public interface */
#include "cxprv.h"      /* private interface */
#include "cxmsg.h"      /* messages */
#include "dbpub.h"      /* dbmgr public interface */
#include "scasa.h"      /* TODO - where is correct place for these */
#include "utspub.h"     /* for xlngv */
#include "vstpub.h"     /* for vstGetLastKey */

#if VST_DEBUG
#define MSGD MSGD_CALLBACK
#endif

/* Macro to determine if high values exist in last component of key
 * This is used throughout in determining if GT/LT or GE/LE bracket.
 */
#define HIGHVALUE(key)   ( xlngv(&(key)->keystr[(key)->keyLen-1], \
                             sizeof(LONG)) == 0x00ffff00 )

/*** LOCAL FUNCTION PROTOTYPES ***/

LOCALF int      cxValidVirtual(COUNT index);

#if 0  /* DO NOT DELETE - Save for merge back into V8 baseline */
LOCALF LONG     cxIxDitemToExt(struct ditem *pditem);
#endif

/*** END LOCAL FUNCTION PROTOTYPES ***/

/* PROGRAM: cxFindVirtual - find an entry in a virtual index
 *
 * Simulates access to a virtual index.  All virtual indexes are unique and
 * contain a single component - the recid.  (In some cases the index will
 * contain a second component - that being high values - which is used in 
 * determining if the bracket is a GT/LT or GE/LT bracket.)
 * The index key is always the same as the recid associated with it which
 * allows us to resolve all index operations without storing anything on disk.
 * 
 * Virtual indexes use the cursor->blknum as a way to keep track of the
 * current position (rather than keeping track of a key).  One way to think of
 * it is that one virtual records always fits in a single virtual block.
 *
 * Several liberties have been taken:
 * 1. The valid range of index values are from 1 to lastKey.
 * 2. The key values are an uninterrupted sequential series incremented by
 *    1 throughout their valid range.
 *
 * RETURNS:   FNDOK if all is well
 *            DSM_S_FNDFAIL for failure
 *            The dbkey returned through a parameter
 */
int
cxFindVirtual(
        dsmContext_t    *pcontext,
        CXCURSOR *pcursor,      /* virtual cursor */
        DBKEY    *pdbkey,       /* ptr to return record's dbkey */
        dsmKey_t *pdlo_key,     /* low range key */
        dsmKey_t *pdhi_key,     /* high range key */
        int       fndmode,      /* find mode bit map */
        int       lockmode _UNUSED_) /* ignored for now */
{
    LONG  lastKey    = 0;
    LONG  high_value = 0;
    LONG  low_value  = 0;
    int   ret;

#ifdef VST_ENABLED
    /* need a cursor, this must be for vsi and current blknum must be valid */
    if (pcursor && cxValidVirtual(pcursor->ixnum))
    {
        /* perform the VSI find */

        lastKey = vstGetLastKey(pcontext, pcursor->ixnum);

        /* get upper value out of pdhi_key */
        if (pdhi_key)
            high_value = cxDsmKeyComponentToLong (&pdhi_key->keystr[7]);

        /* get the low value out of pdlo_key */
        if (pdlo_key)
            low_value = cxDsmKeyComponentToLong (&pdlo_key->keystr[7]);

        if (fndmode == DSMFINDFIRST )
        {
            if (!high_value)  /* FIND first > bracket or >= bracket */
            {
                if (low_value < 1)
                    low_value = 1;  /* assume lastKey always >= 1 */

                else if (HIGHVALUE(pdlo_key))
                {
                    /* If high value then this is a find first > low bracket
                     * so get next one if possible
                     */
                    low_value++;
                }

                /* validate new value within high bracket */
                if (low_value > lastKey)
                     return DSM_S_FNDFAIL;

                pcursor->blknum = low_value;
                ret = FNDOK;
            }
            else if (!low_value)  /* FIND first < bracket or <= bracket */
            {
                /* validate value within low bracket */
                if (high_value < 1)
                    return DSM_S_FNDFAIL;

                pcursor->blknum = 1;  /* find first record */
                ret = FNDOK;
            }
            else if (low_value && high_value) /* two sided bracketed AND */
            {
                /* validate invalid high range and case where x == -1 */
                if (high_value < 1)
                    return DSM_S_FNDFAIL;

                if (low_value < 1)
                    low_value = 1;  /* assume lastKey always >= 1 */

                else if (HIGHVALUE(pdlo_key))
                {
                    /* If high value then this is a find first > bracket
                     * so get next one if possible
                     */
                    low_value++;
                }

                /* verify new value within upper bracket */
                if (HIGHVALUE(pdhi_key))
                {
                   /* low_value must be <= high_value */
                   if (low_value > high_value)
                       return DSM_S_FNDFAIL;
                }
                else 
                {
                   /* low_value must be < high_value */
                   if (low_value >= high_value)
                        return DSM_S_FNDFAIL;
                }

                /* validate new value within high bracket */
                if (low_value > lastKey)
                    return DSM_S_FNDFAIL;

                pcursor->blknum = low_value;
                ret = FNDOK;
            }
            else
            {
                return DSM_S_FNDFAIL;   /* should never get here! */
            }
        }
        else if (fndmode == DSMFINDLAST)
        {
            if (!low_value)  /* FIND last > bracket or >= bracket */
            {
                /* validate value within high bracket */
                if (high_value > lastKey)
                    return DSM_S_FNDFAIL;

                pcursor->blknum = lastKey;
                ret = FNDOK;
            }
            else if (!high_value)   /* FIND last <  bracket or <= bracket */
            {
                if (low_value > lastKey)
                    low_value = lastKey;
                else if (!HIGHVALUE(pdlo_key))
                {
                    /* If NOT high values then this is a find last < bracket
                     * so get prev one if possible.
                     */
                    low_value--;
                }

                /* validate new value within low bracket */
                if (low_value < 1)
                    return DSM_S_FNDFAIL;

                pcursor->blknum = low_value;
                ret = FNDOK;
            }
            else if (low_value && high_value) /* two sided bracketed AND */
            {
                if (low_value > lastKey)
                    low_value = lastKey;

                else if (!HIGHVALUE(pdlo_key))
                {
                    /* If NOT high values then this is a find last < bracket
                     * so get prev one if possible.
                     */
                    low_value--;
                }

                /* validate new value within lower bracket */
                if (HIGHVALUE(pdhi_key))
                {
                    /* low_value must be > low_value */
                    if (low_value <= high_value)
                        return DSM_S_FNDFAIL;
                }
                else
                {
                    /* low_value must be >= high_value */
                    if (low_value < high_value)
                        return DSM_S_FNDFAIL;
                }

                /* validate new value within valid range of values */
                if (low_value < 1)
                    return DSM_S_FNDFAIL;

                pcursor->blknum = low_value;
                ret = FNDOK;
            }
            else
            {
                return DSM_S_FNDFAIL;   /* should never get here! */
            }
        }
        else
        {
#if VST_DEBUG
            MSGD(pcontext,
                 "%LcxFindVirtual: INVALID find request. fndmode=%i", fndmode);
#endif
            return DSM_S_FNDFAIL;
        }

        /* return the required dbkey */
        *pdbkey = (DBKEY) pcursor->blknum;

    }
    else
#endif  /* ifdef VST_ENABLED */
    {
        /* invalid request */
        return DSM_S_FNDFAIL;
    }
        
    return ret;

}  /* end cxFindVirtual */

    
/* PROGRAM: cxNextVirtual - find next entry in a virtual index
 *
 * Simulates access to a virtual index.  All virtual indexes are unique and
 * contain a single component - the recid.  (In some cases the index will
 * contain a second component - that being high values - which is used in 
 * determining if the bracket is a GT/LT or GE/LT bracket.)
 * The index key is always the same as the recid associated with it.
 *
 * pcursor->blknum maintains the current cursor position.  It is manipulated
 * depending on the search criteria.  In the event that invalid search
 * criteria is given, the value of pcursor->blknum must not be altered.
 *
 * Several liberties have been taken:
 * 1. The valid range of index values are from 1 to lastKey.
 * 2. The key values are an uninterrupted sequential series incremented by
 *    1 throughout their valid range.
 *
 * RETURNS:   FNDOK if all is well
 *            FNDFAIL for failure
 *            AHEADORNG - FNDPREV failure ?
 *            ENDLOOP - FNDNEXT gone past end
 *            The dbkey returned through a parameter
 */
int
cxNextVirtual(
        dsmContext_t    *pcontext,
        CXCURSOR *pcursor,      /* virtual cursor */
        DBKEY *pdbkey,  /* ptr to return record's dbkey */
        dsmKey_t *pdlo_key,     /* low range key */
        dsmKey_t *pdhi_key,     /* high range key */
        int fndmode,            /* find mode bit map */
        int lockmode _UNUSED_)  /* ignored for now */
{
    LONG  lastKey    = 0;
    LONG  high_value = 0;
    LONG  low_value  = 0;
    int   ret; /* return value */
    
#ifdef VST_ENDABLED
    /* need a cursor, this must be for vsi and current blknum must be valid */
    if (pcursor &&
        cxValidVirtual(pcursor->ixnum) &&
        (pcursor->blknum <= 
          (ULONG)(lastKey = vstGetLastKey(pcontext, pcursor->ixnum))) )
    {
        /* perform the VSI next */

        /* get upper value out of pdhi_key  */
        if (pdhi_key)
            high_value = cxDsmKeyComponentToLong (&pdhi_key->keystr[7]);

        /* get the low value out of pdlo_key */
        if (pdlo_key)
            low_value = cxDsmKeyComponentToLong (&pdlo_key->keystr[7]);

        if (fndmode & CXNEXT)
        {
            if (!high_value) /* FIND next >  bracket or >= bracket */
            {
                if (low_value <= (LONG)pcursor->blknum)
                    low_value = pcursor->blknum + 1;

                else if (HIGHVALUE(pdlo_key))
                {
                    /* If high values then this is a find > bracket.
                     * so get next one if possible
                     */
                    low_value++;
                }

                /* validate new value within high bracket */
                if (low_value > lastKey)
                     return DSM_S_ENDLOOP;

                pcursor->blknum = low_value;
                ret = FNDOK;
            }
            else if (!low_value) /* FIND next < bracket or <= bracket */
            {
                if ( (high_value < 1) || (++pcursor->blknum > (ULONG)lastKey) )
                    return DSM_S_ENDLOOP;

                /* validate new value within high bracket */
                if (HIGHVALUE(pdhi_key))
                {
                    /* If high values then this is a find next <= bracket
                     * so new value must be <= high bracket
                     */
                    if ( (LONG)pcursor->blknum > high_value)
                        return DSM_S_ENDLOOP;
                }
                else
                {
                    /* new value must be < high bracket */
                    if ( (LONG)pcursor->blknum >= high_value)
                        return DSM_S_ENDLOOP;
                }

                ret = FNDOK;
            }
            else if (low_value && high_value) /* two sided bracketed AND */
            {
                /* validate invalid high range and case where x == -1 */
                if ( (high_value < 1) || ((LONG)++pcursor->blknum > lastKey) )
                    return DSM_S_ENDLOOP;

                if (low_value < (LONG)pcursor->blknum)
                    low_value = pcursor->blknum;
                else if (HIGHVALUE(pdlo_key))
                {
                    /* If high values then this is a find next > bracket
                     * so get next one if possible.
                     */
                    low_value++;
                }

                /* validate new value within high bracket */
                if (HIGHVALUE(pdhi_key))
                {
                    /* If high values then this is a <= bracket.
                     * so new value must be <= high bracket
                     */
                    if ( low_value > high_value)
                        return DSM_S_ENDLOOP;
                }
                else
                {
                    /* new value must be < high bracket */
                    if ( low_value >= high_value)
                        return DSM_S_ENDLOOP;
                }

                /* validate new value within valid range */
                if ( low_value > lastKey)
                    return DSM_S_ENDLOOP;

                pcursor->blknum = low_value;
                ret = FNDOK;
            }
            else
            {
                return DSM_S_FNDFAIL;   /* should never get here! */
            }
        }
        else if (fndmode & CXPREV)
        {
            if (!high_value)  /* FIND prev > bracket or >= bracket */
            {
                if ( (low_value > lastKey) || ((LONG)--pcursor->blknum <= 0) )
                    return DSM_S_ENDLOOP;

                else if (HIGHVALUE(pdlo_key))
                {
                    /* If high values then this is a > bracket.
                     * so new value must be > low bracket
                     */
                    if ((LONG)pcursor->blknum <= low_value)
                        return DSM_S_ENDLOOP;
                }
                else
                {
                    /* new value must be >= low bracket */
                    if ((LONG)pcursor->blknum < low_value)
                        return DSM_S_ENDLOOP;
                }

                ret = FNDOK;
            }
            else if (!low_value)  /* FIND prev < bracket or <= bracket */
            {
                if (high_value < 1)
                    return DSM_S_ENDLOOP;

                if (high_value > lastKey)
                    high_value = lastKey;

                if ((LONG)pcursor->blknum <= high_value)
                    high_value = pcursor->blknum - 1;
                else if (!HIGHVALUE(pdhi_key))
                {
                    /* If NOT high values then this is a find prev < bracket
                     * so get prev one if possible.
                     */
                    high_value--;
                }

                /* validate new value within lower bracket */
                if (high_value < 1)
                    return DSM_S_ENDLOOP;

                pcursor->blknum = high_value;
                ret = FNDOK;
            }
            else if (low_value && high_value) /* two sided bracketed AND */
            {
                /* FIND prev >= low and <= high Case */
                if (high_value < 1)
                    return DSM_S_ENDLOOP;

                if (high_value > lastKey)
                    high_value = lastKey;

                if ((LONG)pcursor->blknum <= high_value)
                    high_value = pcursor->blknum - 1;
                else if (!HIGHVALUE(pdhi_key))
                {
                    /* If NOT high values then this is a find prev < bracket
                     * so get prev one if possible.
                     */
                    high_value--;
                }

                /* verify new value within lower bracket */
                if (HIGHVALUE(pdlo_key))
                {
                    /* find prev > low and < / <= high 
                     * so new value must be > low bracket
                     */
                    if (high_value <= low_value)
                        return DSM_S_ENDLOOP;
                }
                else
                {
                    /* find prev >= low and < / <= high 
                     * so new value must be >= low bracket
                     */
                    if (high_value < low_value)
                        return DSM_S_ENDLOOP;
                }

                /* validate new value within valid range of keys */
                if (high_value < 1)
                    return DSM_S_ENDLOOP;

                pcursor->blknum = high_value;
                ret = FNDOK;
            }
            else
            {
                return DSM_S_FNDFAIL;   /* should never get here! */
            }

        }
        else
        {
            return DSM_S_FNDFAIL;   /* should never get here! */
        }

        /* return the required dbkey */
        *pdbkey = (DBKEY) pcursor->blknum;

    }
    else
#endif  /* VST_ENABLED */
    {
        /* invalid request */
        ret = TOOFAR;
    }
        
    return ret;

}  /* end cxNextVirtual */


/* PROGRAM: cxValidVirtual - validate virtual index number
 *
 * RETURNS:   1 if valid, 0 if not valid
 */
LOCALF int
cxValidVirtual (
        COUNT  index)   /* VSI index number */
{

    if (index > SCIDX_VSI_FIRST || index < SCIDX_VSI_LAST)
    {
        return 0;
    }

    return 1;

}  /* end cxValidVirtual */




#if 0  /* DO NOT DELETE - Save for merge back into V8 baseline (richb) */
/* PROGRAM: cxIxDitemToExt - convert stored key value
 *
 * NOTE: We are assuming OLD STYLE KEY format
 *
 * NOTE: we are special casing for ONE component index (because we can)
 *
 * Attempt to convert from sorted value back to "real value"
 * (The sorted value was created by sortint().)
 *
 * pditem->f.key.keystr[0] is the length of the entire sort key value
 * pditem->f.key.keystr[1] is the length of the key data value
 * pditem->f.key.keystr[2] is the start of the stored key data value
 *
 * 1. Ensure TY_KEY ditem
 * 2. Special case length of 3 to be value of 0
 * 3. Save off value
 # 4. Determine if positive or negative value
 * 5. Perform the reverse of sortint().
 * 6. Restore ditem
 * 7. Return converted value.
 *
 * RETURNS: "external" dbkey data value extracted from ditem
 *          -1 on error (if not key type ditem
 */
LOCALF LONG
cxIxDitemToExt( struct ditem *pditem)
{
  LONG value;       /* converted value */
  TEXT hold_value;  /* save off original value */
  TEXT pos_test;    /* temp var used in pos/neg test */

  /* ensure we have a valid key type ditem */
  if (pditem->type != TY_KEY)
      return -1;

  /* special case key length of 3 to be value of 0 */
  if (pditem->f.key.keystr[0] <= 3)
     return 0;

  hold_value = pditem->f.key.keystr[2];

  pos_test = (pditem->f.key.keystr[1] + 4) << 4;
  if ( (pditem->f.key.keystr[2] & pos_test) == pos_test)
  {
    /* POSITIVE VALUE */
    pditem->f.key.keystr[2] &= (pos_test ^ 0xff);
    value = 0;
  }
  else
  {
#if 0
    /* the following ensures that a non-positive # is a negative # */
    neg_test = (5 - pditem->f.key.keystr[1]) << 4;
    FASSERT_CB(pcontext, ((pditem->f.key.keystr[2] & neg_test) != neg_test),
       "cxDitemCnv: Unknown data storage type - not posative, not negative.");
#endif
    /* set up mask depending on length of variable length stored value
     * (Assuming 8 bits/byte and 4 bits/nibble)
     */
    value = ((LONG)-1 >> ((pditem->f.key.keystr[1] * 8) - 4) ) <<
              ((pditem->f.key.keystr[1] * 8) - 4);
  }

  /* get long value from ditem's variable length storage */
  value |= xlngv(&pditem->f.key.keystr[2], pditem->f.key.keystr[1]);

  /* restore ditem sort data value */
  pditem->f.key.keystr[2] = hold_value;

  return (value);

}  /* end cxIxDitemToExt */

#endif 

