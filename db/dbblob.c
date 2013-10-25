
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

#include "dbcontext.h"
#include "dbutpub.h"
#include "dbblob.h"
#include "dsmpub.h"
#include "lkpub.h"
#include "rmprv.h"
#include "rmpub.h"
#include "ompub.h"
#include "utmpub.h"

#include <limits.h>


/*      Bits for blobCntx1: */

#define DSMBLOBOPMSK    0x0000000f      /* Which operation are we performing */
#define DSMBLOBGETOP           0x1
#define DSMBLOBPUTOP           0x2
#define DSMBLOBUPDOP           0x3

#define DSMBLOBLKMSK    0x000001f0      /* Locking information */
#define DSMBLOBNOLK          0x010      /* We have NOLOCK on the blob */
#define DSMBLOBBRLK          0x020      /* User requested BROWSE lock (same as N
O) */
#define DSMBLOBSHLK          0x040      /* We have a SHARE lock */
#define DSMBLOBEXLK          0x080      /* We have an EXCLUSIVE lock */
#define DSMBLOBTMLK          0x100      /* Release lock in dmsBlobEnd() */
#define DSMBLOBMORE     0x00000200      /* We're making multiple Get/Put/Upd cal
ls */
                            /* If not set, first Get/Put/Upd calls End */
                            /* may be OR'd with bits above             */



/* PROGRAM: dbBlobcalcrecsize - Calculate record size, checking for 
 *                          overflow.
 *
 * RETURNS: DSM_S_SUCCESS unqualified success
 *          DSM_S_BLOBBAD on failure
 */

dsmStatus_t
dbBlobcalcrecsize(
    ULONG               nent,          /* IN Number occupied entries */
    ULONG               *pRecordSize   /* OUT calculated record size  */
)
{
  ULONG tmp1;
  ULONG tmp2;

  *pRecordSize = 0;

  tmp2 = nent * SEGENT_LEN;
  if (tmp2 < nent)
  {
    return DSM_S_BLOBBAD;
  }
  tmp1 = tmp2 + SEGHDR_LEN;
  if (tmp1 < tmp2)
  {
    return DSM_S_BLOBBAD;
  }
  if (tmp1 > LONG_MAX)
  {
    return DSM_S_BLOBBAD;
  }

  *pRecordSize = tmp1;
  return DSM_S_SUCCESS;
}


/* PROGRAM: dbBlobprmchk - check the caller's parameters and 
 *                          map to the appropriate storage area.
 *
 * RETURNS: DSM_S_SUCCESS unqualified success
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t
dbBlobprmchk(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN blob descriptor */
    dsmMask_t           op,             /* IN Which blob operation */
    xDbkey_t            *pDbkey)        /* IN extended dbkey */
{
  dsmStatus_t  returnCode;
  dbcontext_t *pdbcontext;

  TRACE_CB(pcontext, "dbBlobprmchk");

  if (!(pBlob->blobContext.blobCntx1 & DSMBLOBOPMSK))
  {
    pBlob->blobContext.blobCntx1 |= op;
  }

  if ((pBlob->blobContext.blobCntx1 & DSMBLOBOPMSK) != op)
  {
    return DSM_S_BLOBBADCNTX;
  }

  pdbcontext = pcontext->pdbcontext;
  if (pdbcontext->usertype & SELFSERVE) 
  {
    if (pdbcontext->resyncing || lkservcon(pcontext))
    {
      return DSM_S_CTRLC;     /* Self-service only */
    }
  }

  pDbkey->dbkey = pBlob->blobId;

  returnCode = omIdToArea (pcontext, DSMOBJECT_BLOB, 
                           (COUNT)pBlob->blobObjNo, (COUNT)pBlob->blobObjNo,
                           &(pDbkey->area));

#ifdef DSM_BLOB_TEST
  MSGD_CALLBACK(pcontext, "%LdbBlobprmchk: ObjNo %l area %l",
                  pBlob->blobObjNo, pDbkey->area);
#endif
    return returnCode;
} /* end dbBlobprmchk */


/* PROGRAM: dbBlobFetch - local routine to fetch blob segment table
 *                       
 *
 * RETURNS: DSM_S_BLOBOK unqualified success
 *          DSM_S_BLOBTOOBIG - blob retrieved larger than malloc'd buffer 
 *          DSM_S_BLOBBAD - blob segment retrieved smaller than size expected  
 *          
 */
dsmStatus_t
dbBlobFetch(
    dsmContext_t	*pcontext,
    xDbkey_t            *pDbkey,       /* IN extended dbkey (area & recid) */
    dsmBuffer_t         *pRecord,      /* IN utmalloc'd buffer to use */
    LONG                maxLen,        /* IN length of utmallocd buffer */
    LONG                indicator,     /* IN Either DSMBLOBDATA or DSMBLOBSEG */
    dsmText_t          *pName _UNUSED_) /* IN reference name for messages */
{
    LONG                recordSize;
    ULONG               tmp;

    TRACE_CB(pcontext, "dbBlobFetch");

    recordSize = rmFetchRecord(pcontext, pDbkey->area, pDbkey->dbkey, pRecord, 
                            (COUNT)maxLen, 0);

    /* DSMBLOBDONTCARE only if its the 1st piece of the blob */

    if (recordSize == 4 && xlng (pRecord) == 0)
        return indicator == DSMBLOBDONTCARE ? DSM_S_BLOBDNE : DSM_S_BLOBBAD;

    if (recordSize < 1)
        return (dsmStatus_t) ((recordSize < 0) ? recordSize : DSM_S_BLOBBAD);

    if (indicator != DSMBLOBDONTCARE && *pRecord != indicator)
        return DSM_S_BLOBBAD;

    if (recordSize > maxLen)
        return DSM_S_BLOBTOOBIG;

    if (indicator == DSMBLOBSEG) 
    {
        if (recordSize < (LONG)SEGENT_LEN)
	{
            return DSM_S_BLOBBAD;
	}
        if (DSM_S_BLOBBAD == dbBlobcalcrecsize(xct((TEXT *)&(((dbBlobSegHdr_t *)pRecord)->nument)), &tmp))
	{
            return DSM_S_BLOBBAD;
	}
        if (tmp < (ULONG)recordSize)
        {
	  return DSM_S_BLOBBAD;
	}
    }

    return DSM_S_BLOBOK;
} /* end dbBlobFetch */


/* PROGRAM: dbBlobUpdSeg - retrieve a segment table and update it to
 *                          the new length passed in. 
 *
 * RETURNS: DSM_S_BLOBOK unqualified success
 *          DSM_S_BLOBTOOBIG - blob retrieved larger than malloc'd buffer 
 *          DSM_S_BLOBBAD - blob segment retrieved smaller than size expected  
 *          
 */
dsmStatus_t
dbBlobUpdSeg(
    dsmContext_t	*pcontext,
    xDbkey_t            *pDbkey,        /* IN extended dbkey (area & recid) */
    dsmDbkey_t           nextSeg,       /* IN recid of the next segment table */
    dsmLength_t          length,        /* IN Total blob length */
    dsmBuffer_t         *pSegTab,       /* IN utmalloc'd buffer to use */
    dsmText_t           *pName)         /* IN reference name for messages */
{
    dsmStatus_t          returnCode;
    LONG                 recordSize;
    ULONG                tmpRecordSize;
    dbBlobSegHdr_t      *pSegHdr = (dbBlobSegHdr_t *)pSegTab;

    TRACE_CB(pcontext, "dbBlobUpdSeg");

    returnCode = dbBlobFetch (pcontext, pDbkey, pSegTab,
                               BLOBMAXSEGTAB, DSMBLOBSEG, pName);

    if (returnCode == DSM_S_BLOBOK) 
    {
        if (length)
        {
            slng((TEXT *)&pSegHdr->length, length);
        }
        sdbkey((TEXT *)&pSegHdr->dbkey, nextSeg);

        if (DSM_S_BLOBBAD == dbBlobcalcrecsize(xct((TEXT *)&pSegHdr->nument), &tmpRecordSize))
	{
            return DSM_S_BLOBBAD;
	}
        recordSize = (LONG) tmpRecordSize;

        returnCode = rmUpdateRecord (pcontext, 0, pDbkey->area, pDbkey->dbkey, 
                                     pSegTab, (COUNT)recordSize);
#ifdef DSM_BLOB_TEST
    MSGD_CALLBACK(pcontext,
                 (TEXT *)"%LdbBlobUpdSeg: area %l, dbkey %l, recordsize %l",
                 pDbkey->area, pDbkey->dbkey, recordSize);
#endif
    }
    return returnCode;
} /* end dbBlobUpdSeg */


/* PROGRAM: dbBlobPutSeg - Create a record containing the segment table
 *                          for a blob.           
 *
 * RETURNS: DSM_S_BLOBOK unqualified success
 *          DSM_S_BLOBTOOBIG - blob retrieved larger than malloc'd buffer 
 *          DSM_S_BLOBBAD - blob segment retrieved smaller than size expected  
 *          DSM_S_FAILURE - internal error - dbkey equals zero
 *          
 */
dsmStatus_t
dbBlobPutSeg(
    dsmContext_t       *pcontext,
    xDbkey_t           *pDbkey,     /* I/O extended dbkey (I area & O recid) */
    dsmBlob_t          *pBlob,      /* IN blob descriptor */
    dsmLength_t        segLen,      /* IN Length of this seg table or 0 */
    LONG               nent,        /* IN Number occupied entries */
    dsmBuffer_t        *pSegTab,    /* IN utmalloc'd buffer to use */
    dsmText_t          *pName _UNUSED_) /* IN reference name for messages */
{
    dsmStatus_t          returnCode;
    dsmLength_t          Len;
    ULONG                tmpLen;
    dbBlobSegHdr_t      *pSegHdr = (dbBlobSegHdr_t *)pSegTab;

    TRACE_CB(pcontext, "dbBlobPutSeg");

    sct((TEXT *)&pSegHdr->nument, (COUNT)nent);
    if (DSM_S_BLOBBAD == dbBlobcalcrecsize(nent, &tmpLen))
    {
      return DSM_S_BLOBBAD;
    }
    Len = (dsmLength_t) tmpLen;

    if (segLen && Len > segLen)
        return DSM_S_BLOBBAD;

    pDbkey->dbkey = rmCreate(pcontext, pDbkey->area,
                             pBlob->blobObjNo /* blob object number */,
                             pSegTab, (COUNT)Len,
                             (UCOUNT)RM_LOCK_NOT_NEEDED);
    
#ifdef DSM_BLOB_PUT_TEST
    MSGD_CALLBACK(pcontext, 
                  (TEXT*)"%LdbBlobPutSeg: rmCreate dbkey %l Len = %l nent %l", 
                  pDbkey->dbkey, Len, nent);
#endif
    /* Check to see if record creation was successful */
    if (pDbkey->dbkey == 0)  /* should never happen */
        returnCode = DSM_S_FAILURE;
    else if (pDbkey->dbkey < 0) /* error code from rm layer */
        returnCode = (dsmStatus_t) pDbkey->dbkey;
    else
        returnCode = DSM_S_BLOBOK;

    return returnCode;
} /* end dbBlobPutSeg */


/* PROGRAM: dbBlobDelSeg - specialized routine to conditional remove all or
 *                          parts of a blob.
 *
 * RETURNS: DSM_S_BLOBOK unqualified success
 *          DSM_S_BLOBTOOBIG - blob retrieved larger than malloc'd buffer 
 *          DSM_S_BLOBBAD - blob segment retrieved smaller than size expected  
 *          DSM_S_FAILURE - internal error - dbkey equals zero
 *          
 */
dsmStatus_t
dbBlobDelSeg(
    dsmContext_t	*pcontext,
    dsmBlob_t           *pBlob,
    xDbkey_t            *pDbkey,        /* IN extended dbkey */
    LONG                 ent,           /* IN First entry to delete 
                                         * Both 0 & -1 delete all datasegs
                                         *  0 --> delete the SegTab also
                                         * -1 --> keep the SegTab */
    dsmBuffer_t         *pSegTab,       /* IN utmalloc'd buffer to use, already
                                         *    contains the pDbkey segment */
    dsmText_t           *pName)         /* IN reference name for messages */
{
    dsmStatus_t returnCode = DSM_S_BLOBOK;
    dbBlobSegEnt_t *pST;           /* within pSegTab */
    dsmBlobId_t  next_seg;
    LONG         i, nent;
    dbBlobSegHdr_t *pSegHdr = (dbBlobSegHdr_t *)pSegTab;

    TRACE_CB(pcontext, "dbBlobDelSeg");

#ifdef DSM_BLOB_TEST
    MSGD_CALLBACK(pcontext, 
    "%LdbBlobDelSeg: dbkey %l ent %l",
    pDbkey->dbkey, ent);
#endif
    while (returnCode == DSM_S_BLOBOK)
    {
        nent     = xct((TEXT *)&pSegHdr->nument);
        next_seg = xdbkey((TEXT *)&pSegHdr->dbkey);

        /* Update/Delete this segment table record */

        if (ent) 
        {          /* < 0 --> becoming a direct blob, don't del pDbkey */
            sdbkey((TEXT *)&pSegHdr->dbkey, 0);
            sct((TEXT *)&pSegHdr->nument, (COUNT)ent);
            if (ent > 0)
                returnCode = (dsmStatus_t)rmUpdateRecord(pcontext, 0,
                                       pDbkey->area, pDbkey->dbkey, pSegTab,
                                       (COUNT)(SEGHDR_LEN + ent * SEGENT_LEN));
            else
                ent = 0;
        } 
        else
        {
#ifdef DSM_BLOB_TEST
    MSGD_CALLBACK(pcontext, 
    "%LdbBlobDelSeg: SH rmDeleteRec dbkey %l area %l",
    pDbkey->dbkey, pDbkey->area);
#endif
            /* Deletion of the Segment header */
            returnCode = (dsmStatus_t)rmDeleteRec (pcontext, pBlob->blobObjNo,
                                                   pDbkey->area,
                                                   pDbkey->dbkey);
        }

        if (returnCode < 0)
            break;

        /* Deletion of the data SEGMENTs from the segment table */
        pST = (dbBlobSegEnt_t *)(pSegTab + SEGHDR_LEN) + ent;
        for (i = ent; i < nent; i++, pST++)
        {
            if (!xct((TEXT *)&pST->seglength))    /* segment length */
                continue;

            pDbkey->dbkey = xdbkey((TEXT *)&pST->segdbkey);

#ifdef DSM_BLOB_TEST
    MSGD_CALLBACK(pcontext, 
    "%LdbBlobDelSeg: rmDeleteRec dbkey %l area %l",
    pDbkey->dbkey, pDbkey->area);
#endif
            returnCode = (dsmStatus_t)rmDeleteRec (pcontext, pBlob->blobObjNo,
                                                   pDbkey->area,
                                                   pDbkey->dbkey);
            if (returnCode < 0)
                break;
        } /* for i ... < nent */

        if (returnCode != DSM_S_BLOBOK)
            break;

        if (!next_seg)
            break;              /* All done */

        ent = 0;
        pDbkey->dbkey = next_seg;

        /* Fetch the next segment header for a multi-segmented blob */
        returnCode = dbBlobFetch (pcontext, pDbkey, pSegTab,
                                   BLOBMAXSEGTAB, DSMBLOBSEG, pName);

    } /* while returnCode == DSM_S_BLOBOK */

    return returnCode;
} /* end dbBlobDelSeg */


/* PROGRAM: dbBlobTrunc - routine to reduce a blob down to a specified size.
 *                       
 *
 * RETURNS: DSM_S_BLOBOK unqualified success
 *          DSM_S_BLOBNOMEMORY
 *          DSM_S_BLOBTOOBIG - blob retrieved larger than malloc'd buffer 
 *          DSM_S_BLOBBAD - blob segment retrieved smaller than size expected  
 *          DSM_S_FAILURE - internal error - dbkey equals zero
 *          
 */
dsmStatus_t
dbBlobTrunc(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN for areaType & blobObjNo */
    xDbkey_t            *pDbkey,        /* IN extended dbkey 
                                   *  N.B.  This MUST be a segemnted blob */
    dsmBuffer_t         *pSegTab,       /* IN utmalloc'd buffer to use, already
                                         *    contains the pDbkey segment */
    dsmLength_t          newLength)     /* IN the Blob's new length */
{
    dsmStatus_t returnCode = DSM_S_BLOBOK;
    dbBlobSegEnt_t *pST;           /* within pSegTab */
    dsmBuffer_t *pRecord;
    dsmBlob_t    xBlob;
    xDbkey_t     xDbkey;
    dsmLength_t  segLen;
    LONG         i, nent;
    dsmText_t    pName [] = "?";
    dbBlobSegHdr_t *pSegHdr = (dbBlobSegHdr_t *)pSegTab;

    TRACE_CB(pcontext, "dbBlobTrunc");

    xDbkey.area = pDbkey->area;

    if (newLength < DSMBLOBMAXLEN) {    /* Transform into a Direct Blob */
        pRecord = (dsmBuffer_t *)utmalloc (newLength + 1);
        if (!pRecord)
            return DSM_S_BLOBNOMEMORY;
        stnclr ((dsmBuffer_t *) &xBlob, sizeof (xBlob));
        xBlob.blobId    = pDbkey->dbkey;
        xBlob.areaType  = pBlob->areaType;
        xBlob.blobObjNo = pBlob->blobObjNo;
        xBlob.pBuffer = pRecord + 1;
        xBlob.maxLength = newLength;
        *pRecord = DSMBLOBDATA;

        returnCode = dbBlobGet (pcontext, &xBlob, pName);

        if (returnCode == DSM_S_BLOBOK)
            returnCode = dbBlobDelSeg (pcontext, pBlob, pDbkey, -1, pSegTab,
                                       pName);

        /* pDbkey->dbkey has been destroyed by dbBlobDelSeg () */

        if (returnCode == DSM_S_BLOBOK)
            returnCode = rmUpdateRecord(pcontext, 0, pDbkey->area, xBlob.blobId,
                                        pRecord, (COUNT)(newLength + 1));
        utfree ((dsmBuffer_t *)pRecord);
        return returnCode;
    }

    while (newLength > 0 && returnCode == DSM_S_BLOBOK)
    {
        pST = (dbBlobSegEnt_t *)(pSegTab + SEGHDR_LEN);
        nent = xct((TEXT *)&pSegHdr->nument);
        for (i = 0; i < nent; i++, pST++)
        {
            segLen = xct((TEXT *)&pST->seglength);
            if (!segLen)                /* segment length */
                continue;

            if (segLen > newLength) 
            {
                xDbkey.dbkey = xdbkey((TEXT *)&pST->segdbkey);
                segLen = newLength;
                sct((TEXT *)&pST->seglength, (COUNT)segLen);
                returnCode = dbBlobDelSeg (pcontext, pBlob, pDbkey, i+1,
                                           pSegTab, pName);
                if (!returnCode) 
                {
                    if (newLength) 
                    {            /* Segemnt changed size */
                        returnCode = rmFetchRecord (pcontext, xDbkey.area,
                                                    xDbkey.dbkey, pSegTab,
                                                    DSMBLOBMAXLEN + 1, 0);
                        if (returnCode == 4 && xlng (pSegTab) == 0)
                            returnCode = DSM_S_BLOBDNE;
                        else if (returnCode > 0)
                            returnCode =  rmUpdateRecord(pcontext, 0, xDbkey.area,
                                                    xDbkey.dbkey, pSegTab,
                                                    (COUNT)(newLength + 1));
                    }
                        return returnCode;
                }
            }

            newLength -= segLen;

            if (returnCode < 0)
                break;
        } /* for i ... < nent */

        if (returnCode != DSM_S_BLOBOK)
            break;

        pDbkey->dbkey = xdbkey((TEXT *)&pSegHdr->dbkey);
        if (!pDbkey->dbkey)
            break;              /* All done */

        returnCode = dbBlobFetch (pcontext, pDbkey, pSegTab,
                                   BLOBMAXSEGTAB, DSMBLOBSEG, pName);

    } /* while returnCode == DSM_S_BLOBOK */

    return returnCode;
} /* end dbBlobTrunc */


/* PROGRAM: dbBlobPut - create a blob in the database
 *
 * RETURNS: status code received from rmCreateRec or other procedures
 *          which this procedure invokes.
 */

dsmStatus_t
dbBlobPut(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN blob descriptor */
    dsmText_t           *pName)         /* IN reference name for messages */
{
    dsmStatus_t returnCode = DSM_S_BLOBLIMIT;
    xDbkey_t     xPST;
    xDbkey_t     sDbkey; 
    xDbkey_t     xDbkey;        /* extended dbkey (area & recid) */
    dsmBuffer_t *pRecord;       /* With the 1 byte indicator */
    dsmBuffer_t *pSegTab;       /* With the 1 byte indicator */
    dsmBuffer_t *pB;            /* within pBlob->pBuffer */
    dbBlobSegEnt_t *pST = 0;    /* within pSegTab */
    LONG          nent = 0;      /* number seg tab entries */
    dsmLength_t  segLen;
    LONG         remainder;
    ULONG        tmpLen;
    dbBlobSegHdr_t *pSegHdr;

    TRACE_CB(pcontext, "dbBlobPut");

    if (pBlob->blobContext.blobOffset != 0) 
    {
    
        /* Subsequent PUT calls become updates */

        pBlob->blobContext.blobCntx1 &= ~DSMBLOBPUTOP;
        pBlob->blobContext.blobCntx1 |= DSMBLOBUPDOP;
        return dbBlobUpdate(pcontext, pBlob, pName);
    }

    /* Make sure the length parameters are not allowed to be invalid */
    if ( pBlob->segLength <= 0 || pBlob->totLength <= 0 )  
    {
      return DSM_S_BLOBBADCNTX;
    }

    /* Check the callers parameters and map to appropriate area */
    returnCode = dbBlobprmchk (pcontext, pBlob, (dsmMask_t)DSMBLOBPUTOP, 
                                &xDbkey);
    if (returnCode)
    {
      return returnCode;
    }

    segLen = DSMBLOBMAXLEN;
    if (pBlob->segLength < segLen)
    {
      segLen = pBlob->segLength;
    }

    pRecord = (dsmBuffer_t *)utmalloc (segLen + 1);
    if (!pRecord)
    {
        return DSM_S_BLOBNOMEMORY;
    }	
#ifdef DSM_BLOB_PUT_TEST
    MSGD_CALLBACK(pcontext, "%LdbBlobPut: segLen = %l", segLen);
#endif

    /* xPST is used to link the segemnt tables together. */

    xPST.area  = xDbkey.area; /* contains blob area */
    xPST.dbkey = 0;
    sDbkey.area = xDbkey.area;

    if (pBlob->totLength > DSMBLOBMAXLEN) 
    {
        pSegTab = (dsmBuffer_t *)utmalloc(BLOBMAXSEGTAB);
        if (!pSegTab) 
        {
            utfree (pRecord);
            return DSM_S_BLOBNOMEMORY;
        }
        pSegHdr = (dbBlobSegHdr_t *)pSegTab;
        stnclr (pSegTab, BLOBMAXSEGTAB);
        pSegHdr->segid = DSMBLOBSEG;
        slng((TEXT *)&pSegHdr->length, pBlob->segLength);

        pST = (dbBlobSegEnt_t *)(pSegTab + SEGHDR_LEN);
        nent = 0;
#ifdef DSM_BLOB_PUT_TEST
    MSGD_CALLBACK(pcontext, "%LdbBlobPut: stable created, size = %l",
                  BLOBMAXSEGTAB);
#endif
    } 
    else
    {
        pSegTab = 0;
        pSegHdr = 0;
        pST = 0;
#ifdef DSM_BLOB_PUT_TEST
    MSGD_CALLBACK(pcontext, "%LdbBlobPut: no stable created");
#endif
    }

    remainder = pBlob->segLength;
    *pRecord = DSMBLOBDATA;
    pB = pBlob->pBuffer;

    /* Loop until the entire blob image has been stored by rm manager */
    while (remainder > 0 && returnCode == DSM_S_BLOBOK) 
    {
        bufcop (pRecord + 1, pB, segLen);

        xDbkey.dbkey = rmCreate(pcontext, xDbkey.area,
                                pBlob->blobObjNo /* blob object number */,
                                pRecord,
                                (COUNT)(segLen + 1),
                                (UCOUNT)RM_LOCK_NOT_NEEDED);

        /* rmCreate returns a recid upon success or negative number
         * for failure and should never return 0
         */
#ifdef DSM_BLOB_PUT_TEST
    MSGD_CALLBACK(pcontext, "%LdbBlobPut: rmcreate dbkey %l segLen %l", 
                  xDbkey.dbkey, segLen);
#endif

        if (xDbkey.dbkey == 0) 
            returnCode = DSM_S_FAILURE;
        else if (xDbkey.dbkey < 0)
            returnCode = (dsmStatus_t) xDbkey.dbkey;
        else 
        {
            if (!pSegTab)
                pBlob->blobId = xDbkey.dbkey;
            pBlob->blobContext.blobOffset += segLen;
            returnCode = DSM_S_BLOBOK;
        }

        if (pSegTab) 
        {
            if (nent >= MAXSEGENT) 
            {
                returnCode = dbBlobPutSeg (pcontext, &sDbkey, pBlob, (LONG) 0,
                                            nent, pSegTab, pName);
                if (returnCode != DSM_S_BLOBOK)
                    break;

                /* We just wrote the first segment */
                if (!pBlob->blobId)
                    pBlob->blobId = sDbkey.dbkey;

                /* Update link from previous segment */

                if (xPST.dbkey) 
                {
                    returnCode = dbBlobUpdSeg (pcontext, &xPST, sDbkey.dbkey,
                                               (dsmLength_t) 0, pSegTab, pName);

                }

               if (returnCode != DSM_S_BLOBOK)
                    break;

                xPST.dbkey = sDbkey.dbkey;

                stnclr (pSegTab, BLOBMAXSEGTAB);
                pSegHdr->segid = DSMBLOBSEG;
                pST = (dbBlobSegEnt_t *)(pSegTab + SEGHDR_LEN);
                nent = 0;
            }
            nent++;
            sct((TEXT *)&pST->seglength, (COUNT)segLen);
            sdbkey((TEXT *)&pST->segdbkey, xDbkey.dbkey);
            pST++;
        }

        pB += segLen;

        if (remainder < (LONG)segLen)
	{
	  remainder = segLen;
	}
        remainder -= segLen;

        if ((LONG)segLen > remainder)
	{
	  segLen = remainder;
	}

    } /* while remainder > 0 */

    if (returnCode == DSM_S_BLOBOK && pSegTab) 
    {
        if (DSM_S_BLOBBAD == dbBlobcalcrecsize(nent, &tmpLen))
        {
          return DSM_S_BLOBBAD;
        }
        segLen = tmpLen;

        /* if we're expecting more, reserve space in the seg table now */

        if (pBlob->blobContext.blobCntx1 & DSMBLOBMORE) 
	{
            segLen = (pBlob->totLength + DSMBLOBMAXLEN - 1) / DSMBLOBMAXLEN;
	    if (DSM_S_BLOBBAD == dbBlobcalcrecsize(segLen, &segLen))
	    {
	      return DSM_S_BLOBBAD;
	    }
            if (segLen > BLOBMAXSEGTAB)
	     {
	       segLen = BLOBMAXSEGTAB;
	     }
        }

        returnCode = dbBlobPutSeg (pcontext, &sDbkey, pBlob, segLen, nent, 
                                    pSegTab, pName);

        /* We just wrote the first segment */
        if (!pBlob->blobId)
	{
	  pBlob->blobId = sDbkey.dbkey;
	}

        /* Update link from previous segment */
        
        if (xPST.dbkey) 
        {
            returnCode = dbBlobUpdSeg (pcontext, &xPST, sDbkey.dbkey,
                                        (dsmLength_t) 0, pSegTab, pName);
        }

        utfree (pSegTab);
    }

    if (returnCode == DSM_S_BLOBOK && 
       !(pBlob->blobContext.blobCntx1 & DSMBLOBMORE))
    {
      returnCode = dbBlobEndSeg(pcontext, pBlob);
    }

    utfree (pRecord);

    return returnCode;

} /* end dbBlobPut */


/* PROGRAM: dbBlobEndSeg - end access to a particular blob
 *
 * RETURNS: DSM_S_BLOBBADCNTX 
 *          DSM_S_BLOBNOMEMORY
 *          DSM_S_BLOBDNE
 *          DSM_S_BLOBOK
 *          bad record size from rmFetchRecord()
 *           
 */
dsmStatus_t 
dbBlobEndSeg(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob)         /* IN blob descriptor */
{
    dsmStatus_t  returnCode;
    xDbkey_t     xDbkey;        /* extended dbkey (area & recid) */
    dsmBuffer_t *pRecord;       /* With the 1 byte indicator */
    LONG          recordSize;
    dsmLength_t  newLength;
    LONG          maxRecordSize = DSMBLOBMAXLEN + 1;
    dbBlobSegHdr_t *pSegHdr;

    TRACE_CB(pcontext, "dbBlobEndSeg");

    if ((pBlob->blobContext.blobCntx1 & DSMBLOBOPMSK) == DSMBLOBUPDOP)
    {
      returnCode = omIdToArea (pcontext, DSMOBJECT_BLOB, 
                               (COUNT)pBlob->blobObjNo, (COUNT)pBlob->blobObjNo,
                               &(xDbkey.area));

      if (returnCode)
      {
	return returnCode;
      }

      pRecord = (dsmBuffer_t *)utmalloc (maxRecordSize);
      if (!pRecord)
      {
	return DSM_S_BLOBNOMEMORY;
      }
      pSegHdr = (dbBlobSegHdr_t *)pRecord;

      xDbkey.dbkey = pBlob->blobId;

      /* returns size of record if record is found.  Returns
       * a negative number if record is not found.
       */
      recordSize = rmFetchRecord(pcontext, xDbkey.area, xDbkey.dbkey, pRecord,
                                (COUNT)maxRecordSize, 0 /* not continuation */);
      if (recordSize == 4 && xlng (pRecord) == 0) 
      {
	pBlob->segLength = 0;
	returnCode = DSM_S_BLOBDNE;
      }
      else if (recordSize < 0) 
      {
	pBlob->segLength = 0;
	returnCode = (dsmStatus_t) recordSize;
      }
      else
      {
	returnCode = DSM_S_BLOBOK;
      }

      newLength = pBlob->blobContext.blobOffset;
      pBlob->blobContext.blobCntx1 = 0;
      pBlob->blobContext.blobCntx2 = 0;
      pBlob->blobContext.blobOffset = 0;

      if (*pRecord == DSMBLOBDATA) 
      {          /* Direct blob */
	if (newLength <= (dsmLength_t)(recordSize - 1)) 
	{
	  returnCode = rmUpdateRecord(pcontext, 0, xDbkey.area,
                                            xDbkey.dbkey, pRecord, 
                                            (COUNT)(newLength + 1));
	  utfree (pRecord);
          return returnCode;
	} 
	else
	{
	  returnCode = DSM_S_BLOBBAD;
	}
      }

      if (returnCode) 
      {
	utfree (pRecord);
        return returnCode;
      }

      /* It must be a segmented blob.  
           We won't worry about turning into a direct */

      if ((dsmLength_t)xlng((TEXT *)&pSegHdr->length) < newLength) 
      {
	returnCode = DSM_S_BLOBBAD;
      } 
      else 
      {
	slng((TEXT *)&pSegHdr->length, newLength);
	returnCode = rmUpdateRecord(pcontext, 0, xDbkey.area, xDbkey.dbkey,
                                        pRecord, (COUNT)recordSize);
      }

      if (!returnCode)
      {
	returnCode = dbBlobTrunc (pcontext, pBlob, &xDbkey, 
                                       pRecord, newLength);
      }
      utfree (pRecord);
      return returnCode;
    }

    if ((pBlob->blobContext.blobCntx1 & DSMBLOBOPMSK))
    {
        
        /* Subsequent call */

      pBlob->blobContext.blobCntx1 = 0;
      pBlob->blobContext.blobCntx2 = 0;
      pBlob->blobContext.blobOffset = 0;

      return DSM_S_BLOBOK;
    } 

    return DSM_S_BLOBBADCNTX;

} /* end dbBlobEndSeg */


/* PROGRAM: dbBlobGet - retrieve a blob from the database
 *
 * RETURNS: status code received from rmFetchRecord or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dbBlobGet(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN blob descriptor */
    dsmText_t           *pName)         /* IN reference name for messages */
{
    dsmStatus_t returnCode = DSM_S_BLOBLIMIT;
    lockId_t     lockId;
    xDbkey_t     xDbkey;        /* extended dbkey (area & recid) */
    LONG         recordSize, segRC;
    LONG         maxRecordSize;
    dsmBuffer_t *pRecord;       /* With the 1 byte indicator */
    dsmBuffer_t *pSegTab;       /* With the 1 byte indicator */
    dsmBuffer_t *pB;            /* within pBlob->pBuffer */
    dbBlobSegEnt_t *pST;        /* within pSegTab */
    LONG          needRgLock = 1;
    LONG          nent;          /* number seg tab entries */
    ULONG         segLen = 0;
    LONG          i;
    LONG         remainder;
    ULONG        offset = 0;
    dbBlobSegHdr_t *pSegHdr;

    TRACE_CB(pcontext, "dbBlobGet");

    returnCode = dbBlobprmchk (pcontext, pBlob, (dsmMask_t)DSMBLOBGETOP,
                                &xDbkey);
    if (returnCode)
    {
      return returnCode;
    }

    /* set lockId for a record get lock */
    lockId.table = xDbkey.area;
    lockId.dbkey = xDbkey.dbkey;

    if (needRgLock)
    {
        returnCode = (dsmStatus_t)lkrglock(pcontext, &lockId,LKSHARE);
        if (returnCode)
        {
            /* CTRLC or LKTBFULL */
	  return returnCode;
        }
    }

   
    maxRecordSize = DSMBLOBMAXLEN + 1;  /* Need to rmfetch entire segement
                                         * Even if we only want to return
                                         * 1 byte */

    /* Set a max record size buffer regardless of the callers buffer size */
    pRecord = (dsmBuffer_t *)utmalloc (maxRecordSize);
    if (!pRecord)
    {
        return DSM_S_BLOBNOMEMORY;
    }

    /* returns size of record if record is found.  Returns
     * a negative number if record is not found.
     */
    recordSize = rmFetchRecord(pcontext, xDbkey.area, xDbkey.dbkey, pRecord,
                            (COUNT)maxRecordSize, 0 /* not continuation */);
    if (recordSize == 4 && xlng (pRecord) == 0)
    {
        pBlob->segLength = 0;
        returnCode = DSM_S_BLOBDNE;
    }
    else if (recordSize < 0) {
        pBlob->segLength = 0;
        returnCode = (dsmStatus_t) recordSize;
    }
    else
    {
        returnCode =  DSM_S_BLOBOK;

        /* Set the total length of the blob */

        pSegHdr = (dbBlobSegHdr_t *)pRecord;
        if (pSegHdr->segid == DSMBLOBDATA)
        {
            pBlob->totLength = recordSize - 1; /* size of direct blob */
        }
        else if (recordSize > 10)
        {
            pBlob->totLength = xlng((TEXT *)&pSegHdr->length); /* total size of indirect blob */
        }
        else
        {
            pBlob->totLength = 0;
            pBlob->bytesRead = 0;
            returnCode = DSM_S_BLOBBAD;
        }

        /* Set the length of this segment - based on callers buffer size*/

        offset = pBlob->blobContext.blobOffset;
        if (returnCode == DSM_S_BLOBOK)
        {
            /* Protect against negative numbers */
            if (offset <= pBlob->totLength)
            {
                pBlob->segLength = pBlob->totLength - offset;
                if (pBlob->segLength > pBlob->maxLength)
                    pBlob->segLength = pBlob->maxLength;
            }
            else
            {
                /* No more to read - pBlob->segLength <= 0 */
                pBlob->segLength = 0;
                pBlob->bytesRead = 0;
                returnCode = DSM_S_BLOBNOMORE;
            }
        }


#ifdef DSM_BLOB_GET_TEST
    MSGD_CALLBACK(pcontext, "%LdbBlobGet: blobOffset = %l", offset);
    MSGD_CALLBACK(pcontext, "%LdbBlobGet: maxRecordSize = %l", maxRecordSize);
    MSGD_CALLBACK(pcontext, "%LdbBlobGet: maxLength = %l", pBlob->maxLength);
    MSGD_CALLBACK(pcontext, "%LdbBlobGet: segLength = %l", pBlob->segLength);
#endif
        if (returnCode == DSM_S_BLOBOK)
        {
            /* If this is data copy it to the caller's buffer */
            if (pSegHdr->segid == DSMBLOBDATA)
            {
                bufcop (pBlob->pBuffer, pRecord+offset+1, pBlob->segLength);
                pBlob->bytesRead = pBlob->segLength;
            }
            else                                /* A v2 blob */
            {
                if (xdbkey((TEXT *)&pSegHdr->dbkey) == 0) /* Only 1 seg table */
                    pSegTab = (dsmBuffer_t *)utmalloc(recordSize);
                else
                    pSegTab = (dsmBuffer_t *)utmalloc(BLOBMAXSEGTAB);
                if (!pSegTab) {
                   utfree((dsmBuffer_t *)pRecord);
                   return DSM_S_BLOBNOMEMORY;
                }
                pSegHdr = (dbBlobSegHdr_t *)pSegTab;
                bufcop (pSegTab, pRecord, recordSize);

                /* Set a local pointer equal to the caller's buffer */
                pB = pBlob->pBuffer;

                /* remainder equals whats left in the caller's buffer */
                remainder = pBlob->segLength;
                while (remainder > 0 && returnCode == DSM_S_BLOBOK)
                {
                    pST = (dbBlobSegEnt_t *)(pSegTab + SEGHDR_LEN);
                    nent = xct((TEXT *)&pSegHdr->nument);
#ifdef DSM_BLOB_GET_TEST
    MSGD_CALLBACK(pcontext, "%LdbBlobGet: nentries = %l", nent);
#endif
                    for (i = 0; i < nent; i++, pST++)
                    {
                        segLen = xct((TEXT *)&pST->seglength);
                        if (offset >= segLen)
                        {
                            offset -= segLen;
                            continue;
                        }
                        xDbkey.dbkey = xdbkey((TEXT *)&pST->segdbkey);
                        segRC = rmFetchRecord(pcontext, xDbkey.area,
                                           xDbkey.dbkey, pRecord,
                                           (COUNT)maxRecordSize, 0);
                        if (segRC <= 0)
                        {
                            returnCode = (dsmStatus_t)
                                (segRC ? segRC : DSM_S_BLOBBAD);
                            break;
                        }
                        if (segRC - 1 != xct((TEXT *)&pST->seglength) ||
                            *pRecord != DSMBLOBDATA)
                        {
                            returnCode = DSM_S_BLOBBAD;
                            break;
                        }
                        segLen -= offset;
                        if ((LONG)segLen > remainder)
                            segLen = remainder;
                        /* Copy the segment of data to the caller's buffer */
                        bufcop (pB, pRecord + offset + 1, segLen);
                        pBlob->bytesRead += segLen;
                        offset = 0;
                        remainder -= segLen;
                        pB += segLen;
                        if (remainder <= 0)
                            break;
                    } /* for i ... < nent */

                    if (remainder <= 0 || returnCode != DSM_S_BLOBOK)
                        break;

                    xDbkey.dbkey = xdbkey((TEXT *)&pSegHdr->dbkey);
                    if (!xDbkey.dbkey)
                    {
                        returnCode = DSM_S_BLOBBAD;
                        break;
                    }
                    returnCode = dbBlobFetch (pcontext, &xDbkey, pSegTab,
                                               BLOBMAXSEGTAB,
                                               DSMBLOBSEG, pName);
                } /* while remainder */

                utfree (pSegTab);

            } /* a v2 blob */

            pBlob->blobContext.blobOffset += pBlob->segLength;

        } /* inital blob segemnt ok */
    } /* initial rmfetch ok */

    utfree (pRecord);

    if (needRgLock )
    {
        lkrgunlk (pcontext, &lockId);
    }

    if (returnCode == DSM_S_BLOBOK &&
        !(pBlob->blobContext.blobCntx1 & DSMBLOBMORE))
        returnCode = dbBlobEndSeg(pcontext, pBlob);

#ifdef DSM_BLOB_GET_TEST
    MSGD_CALLBACK(pcontext, "%LdbBlobGet: bytesRead  = %l", pBlob->bytesRead);
#endif

    return returnCode;

} /* end dbBlobGet */


/* PROGRAM: dbBlobUpdate - update an existing blob in the database
 *
 * RETURNS: status code received from rmUpdateRecord or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dbBlobUpdate(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN blob descriptor */
    dsmText_t           *pName)         /* IN reference name for messages */
{
    dsmStatus_t returnCode = DSM_S_BLOBLIMIT;
    xDbkey_t     xPST;
    xDbkey_t     sDbkey;
    xDbkey_t     xDbkey;        /* extended dbkey (area & recid) */
    dsmBuffer_t *pRecord;       /* With the 1 byte indicator */
    dsmBuffer_t *pSegTab;       /* With the 1 byte indicator */
    dbBlobSegHdr_t emptySegTab;
    dsmBuffer_t    *pB;      /* within pBlob->pBuffer */
    dbBlobSegEnt_t *pST;     /* within pSegTab */
    dsmLength_t  segLen;
    LONG          nent = 0, i;
    LONG         offset, remainder;
    LONG          recordSize;
    LONG          rewrite_segtab = 0;
    GBOOL         truncate;
    dsmLength_t  len, newLength;
    ULONG        tmpRecordSize;
    dbBlobSegHdr_t *pSegHdr;

    TRACE_CB(pcontext, "dbBlobUpdate");

    returnCode = dbBlobprmchk (pcontext, pBlob, (dsmMask_t)DSMBLOBUPDOP,
                               &xDbkey);
    if (returnCode)
    {
      return returnCode;
    }

    pRecord = (dsmBuffer_t *)utmalloc (DSMBLOBMAXLEN + 1);
    if (!pRecord)
    {
        return DSM_S_BLOBNOMEMORY;
    }

    /* Even if more isn't set & offset == 0, we still need to fetch original
     * blob incase its segmented */

    recordSize = rmFetchRecord(pcontext, xDbkey.area, xDbkey.dbkey, pRecord,
                       (COUNT)(DSMBLOBMAXLEN + 1), 0);

    if (recordSize == 4 && xlng (pRecord) == 0)
    {
        pBlob->segLength = 0;
        returnCode = DSM_S_BLOBDNE;
    }
    else if (recordSize < 0)
    {
        pBlob->segLength = 0;
        returnCode = (dsmStatus_t) recordSize;
    } else
        returnCode = DSM_S_BLOBOK;

    if (returnCode != DSM_S_BLOBOK)
    {
        utfree (pRecord);
        return returnCode;
    }

    truncate  = !(pBlob->blobContext.blobCntx1 & DSMBLOBMORE);
    pB        = pBlob->pBuffer;
    offset    = pBlob->blobContext.blobOffset;
    remainder = pBlob->segLength;
    newLength = offset + remainder;

  /* First case: a replacement of a piece of a direct blob. The
     * New blob is still small enough to be a direct blob.  If there
     * isn't anymore, the blob is truncated at the end of this piece.
     */

    if (*pRecord == DSMBLOBDATA && offset < recordSize - 1 &&
        newLength <= DSMBLOBMAXLEN)
    {

        bufcop (pRecord + offset + 1, pB, remainder);

        /* If this isn't the final update, don't truncate */

        if (!truncate && (dsmLength_t)recordSize > newLength)
            newLength = recordSize - 1;

        pBlob->totLength = newLength;
        returnCode = rmUpdateRecord(pcontext, 0, xDbkey.area, xDbkey.dbkey,
                                    pRecord, (COUNT)(newLength + 1));
#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 1a: Direct rmUpdateRecord dbkey %l, segLen %l",
       xDbkey.dbkey, newLength);
#endif

        utfree (pRecord);

        if (returnCode == DSM_S_BLOBOK)
        {
            pBlob->blobContext.blobOffset += remainder;
            if (!(pBlob->blobContext.blobCntx1 & DSMBLOBMORE))
            {
                /* Don't call dsmBlobEnd since we've already
                 * set the length above */

                pBlob->blobContext.blobCntx1 = 0;
                pBlob->blobContext.blobCntx2 = 0;
                pBlob->blobContext.blobOffset = 0;
            }
        }
        return returnCode;
 }

    /* The blob will be segmented */

    pSegHdr = (dbBlobSegHdr_t *)pRecord;
    if (pSegHdr->segid == DSMBLOBSEG)
        pBlob->totLength = xlng((TEXT *)&pSegHdr->length);
    else
        pBlob->totLength = recordSize - 1;

    if ((dsmLength_t)offset > pBlob->totLength)
    {
        utfree (pRecord);
        return DSM_S_BLOBBAD;  /* Attempt to create a blob with a hole */
    }

    /* We need a seg table.  Either it was a direct blob becoming a segmented
     * blob, or, its already segmented (although it may become direct)
     */

    pSegTab = (dsmBuffer_t *)utmalloc(BLOBMAXSEGTAB);
    if (!pSegTab)
    {
        utfree((dsmBuffer_t *)pRecord);
        return DSM_S_BLOBNOMEMORY;
    }
    pSegHdr = (dbBlobSegHdr_t *)pSegTab;
    pST = (dbBlobSegEnt_t *)(pSegTab + SEGHDR_LEN);

    /* xPST is used to link the segemnt tables together. */

    xPST.area    = xDbkey.area;
    xPST.dbkey   = 0;
    sDbkey.area  = xDbkey.area;
    sDbkey.dbkey = xDbkey.dbkey;

    stnclr (pSegTab, BLOBMAXSEGTAB);
    if (((dbBlobSegHdr_t *)pRecord)->segid == DSMBLOBSEG)
        bufcop (pSegTab, pRecord, recordSize);
    else {
        /* It was a direct blob, becoming a segmented blob.
         * It has exactly one segemnt for the moment.  The data segment
         * needs to be rmCreate'd so the original recid can be retained for
         * the first piece of the segment table.
         */

        segLen = recordSize - 1;

        xDbkey.dbkey = rmCreate(pcontext, xDbkey.area,
                                pBlob->blobObjNo /* blob object number */,
                                pRecord,
                                (COUNT)(segLen + 1),
                                (UCOUNT)RM_LOCK_NOT_NEEDED);

#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 1: rmCreate dbkey %l, segLen %l",
       xDbkey.dbkey, segLen);
#endif
        if (xDbkey.dbkey == 0)
            returnCode = DSM_S_FAILURE;
        else if (xDbkey.dbkey < 0)
            returnCode = (dsmStatus_t) xDbkey.dbkey;
        else
        {
            /* stnclr (pSegTab, SEGHDR_LEN + SEGENT_LEN); <-- Done above */

            pSegHdr->segid = DSMBLOBSEG;
            sct((TEXT *)&pSegHdr->nument, 1);   /* Only one element */
            slng((TEXT *)&pSegHdr->length, segLen);
            sct((TEXT *)&pST->seglength, (COUNT)segLen);
            sdbkey((TEXT *)&pST->segdbkey, xDbkey.dbkey);
            returnCode = rmUpdateRecord(pcontext, 0, sDbkey.area, sDbkey.dbkey,
                                        pSegTab, SEGHDR_LEN + SEGENT_LEN);
#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 1: Indirect rmUpdateRecord dbkey %l, segLen %l",
       sDbkey.dbkey, (SEGHDR_LEN+SEGENT_LEN));
#endif
            if (returnCode) {
                utfree (pSegTab);
                utfree (pRecord);
                return returnCode;
            }
        }
    }       

    /* We now have the first segment in pSegTab.  sDbkey is its dbkey. offset &
     * remainder are set. */

    while (remainder > 0 && returnCode == DSM_S_BLOBOK)
    {
        rewrite_segtab = 0;

        pST = (dbBlobSegEnt_t *)(pSegTab + SEGHDR_LEN);
        nent = xct((TEXT *)&pSegHdr->nument);
        for (i = 0; i < nent; i++, pST++)
        {
            segLen = xct((TEXT *)&pST->seglength);
            if (offset >= (LONG)segLen)
            {
                offset -= segLen;
                continue;
            }

            /* something goes in this data segment */

            xDbkey.dbkey = xdbkey((TEXT *)&pST->segdbkey);
            recordSize = rmFetchRecord(pcontext, xDbkey.area,
                               xDbkey.dbkey, pRecord,
                               (COUNT)(DSMBLOBMAXLEN + 1), 0);

            if (recordSize == 4 && xlng (pRecord) == 0)
                returnCode = DSM_S_BLOBDNE;
            else if (recordSize < 0)
                returnCode = (dsmStatus_t) recordSize;
            else if ( ((dsmLength_t)(recordSize - 1) != segLen) ||
                      (*pRecord != DSMBLOBDATA) )
                returnCode = DSM_S_BLOBBAD;
            else
                returnCode = DSM_S_BLOBOK;

            if (returnCode != DSM_S_BLOBOK)
                break;

#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 3: rmFetchRecord dbkey %l rSize %l",
       xDbkey.dbkey, recordSize);
#endif
            len = segLen - offset;
            if (len > (dsmLength_t)remainder)
                len = remainder;
            else if ( (i+1 >= nent)                 &&
                      (segLen < DSMBLOBMAXLEN)      &&
                      (segLen < (dsmLength_t)(offset + remainder)) &&
                      (xdbkey((TEXT *)&pSegHdr->dbkey) == 0) )
            {
                /* We can extend this data segment */

                len = DSMBLOBMAXLEN - offset;
                if (len > (dsmLength_t)remainder)
                    len = remainder;
                recordSize = len + offset + 1;
                sct((TEXT *)&pST->seglength, (COUNT)(recordSize - 1));
                rewrite_segtab = 1;
            }

            bufcop (pRecord + offset + 1, pB, len);
            pB += len;
            remainder -= len;
            offset = 0;

#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 3a: Indirect 2 rmUpdate dbkey %l rSize %l",
       xDbkey.dbkey, (recordSize-1));
#endif
            returnCode = rmUpdateRecord(pcontext, 0, xDbkey.area, xDbkey.dbkey,
                                        pRecord, (COUNT)recordSize);

            if (remainder <= 0 || returnCode != DSM_S_BLOBOK)
                break;

        } /* for i = 0; i < nent ... */

        if (remainder <= 0 || returnCode != DSM_S_BLOBOK)
            break;

        if (!xdbkey((TEXT *)&pSegHdr->dbkey))
        {
          /* Need to add a data segment or extend the segment table
           * Either we're adding data segments or we're adding a link
           * to another segment table.  Either way, we need to
           * rewrite this segment table */

            rewrite_segtab = 1;

            while (remainder > 0 && nent < MAXSEGENT)
            {
                /* Create new data segment(s) */

                *pRecord = DSMBLOBDATA;
                len = remainder;
                if (len > DSMBLOBMAXLEN)
                    len = DSMBLOBMAXLEN;
                bufcop (pRecord + 1, pB, len);
                pB += len;
                remainder -= len;

                xDbkey.dbkey = rmCreate(pcontext, xDbkey.area,
                                        pBlob->blobObjNo,
                                        pRecord,
                                        (COUNT)(len + 1),
                                        (UCOUNT)RM_LOCK_NOT_NEEDED);
#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 2: rmCreate dbkey %l, len %l",
       xDbkey.dbkey, len);
#endif

                if (xDbkey.dbkey == 0)
                {
                   returnCode = DSM_S_FAILURE;
                    break;
                }
                else if (xDbkey.dbkey < 0)
                {
                    returnCode = (dsmStatus_t) xDbkey.dbkey;
                    break;
                }
                sct((TEXT *)&pST->seglength, (COUNT)len);
                sdbkey((TEXT *)&pST->segdbkey, xDbkey.dbkey);
                nent++;
                pST++;
            }
            if (returnCode == DSM_S_BLOBOK && remainder > 0)
            {
                /* Need to create another segment */

#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 5: rmCreate dbkey %l, len %l",
       xDbkey.dbkey, SEGHDR_LEN);
#endif
                stnclr(&emptySegTab, SEGHDR_LEN);
                emptySegTab.segid = DSMBLOBSEG;
                xDbkey.dbkey = rmCreate(pcontext, xDbkey.area,
                                        pBlob->blobObjNo,
                                        (TEXT *)&emptySegTab,
                                        SEGHDR_LEN,
                                        (UCOUNT)RM_LOCK_NOT_NEEDED);

                if (xDbkey.dbkey == 0)
                {
                    returnCode = DSM_S_FAILURE;
                    break;
                }
                else if (xDbkey.dbkey < 0)
                {
                    returnCode = (dsmStatus_t) xDbkey.dbkey;
                    break;
                }

               sdbkey((TEXT *)&pSegHdr->dbkey, xDbkey.dbkey);
                rewrite_segtab = 2;
            }

        }

        if (rewrite_segtab)
        {
            sct((TEXT *)&pSegHdr->nument, (COUNT)nent);
	    if (DSM_S_BLOBBAD == dbBlobcalcrecsize(nent, &tmpRecordSize))
	    {
	      return DSM_S_BLOBBAD;
	    }
	    recordSize = (LONG) tmpRecordSize;
#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 6: rmUpdateRecord dbkey %l, rSize %l",
       sDbkey.dbkey, recordSize);
#endif
            returnCode = rmUpdateRecord(pcontext, 0, sDbkey.area, sDbkey.dbkey,
                                        pSegTab, (COUNT)recordSize);
            rewrite_segtab = 0;
        }

        if (xdbkey((TEXT *)&pSegHdr->dbkey))
        {
            sDbkey.dbkey = xdbkey((TEXT *)&pSegHdr->dbkey);
            returnCode = dbBlobFetch (pcontext, &sDbkey, pSegTab,
                                       BLOBMAXSEGTAB,
                                       DSMBLOBSEG, pName);
        }

    } /* while remainder && returnCode is ok */


    /* If the blob has grown, update its length,
       both in pBlob & the 1st seg table */

    if (returnCode == DSM_S_BLOBOK && newLength > pBlob->totLength)
    {
        if (rewrite_segtab)
        {
            sct((TEXT *)&pSegHdr->nument, (COUNT)nent);
	    if (DSM_S_BLOBBAD == dbBlobcalcrecsize(nent, &tmpRecordSize))
	    {
	      return DSM_S_BLOBBAD;
	    }
	    recordSize = (LONG) tmpRecordSize;

#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 7: rmUpdateRecord dbkey %l, rSize %l",
       sDbkey.dbkey, recordSize);
#endif
            returnCode = rmUpdateRecord(pcontext, 0, sDbkey.area, sDbkey.dbkey,
                                        pSegTab, (COUNT)recordSize);
        }

        pBlob->totLength = newLength;

        xDbkey.dbkey = pBlob->blobId;

        returnCode = dbBlobFetch (pcontext, &xDbkey, pSegTab,
                                   BLOBMAXSEGTAB, DSMBLOBSEG, pName);

        if (returnCode == DSM_S_BLOBOK)
        {
            slng((TEXT *)&pSegHdr->length, newLength);

	    if (DSM_S_BLOBBAD == dbBlobcalcrecsize(xct((TEXT *)&pSegHdr->nument), &tmpRecordSize))
	    {
	      return DSM_S_BLOBBAD;
	    }
	    recordSize = (LONG) recordSize;

#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 8: rmUpdateRecord dbkey %l, rSize %l",
       xDbkey.dbkey, recordSize);
#endif
            returnCode = rmUpdateRecord(pcontext, 0, xDbkey.area, xDbkey.dbkey,
                                        pSegTab, (COUNT)recordSize);
        }

    }

    pBlob->blobContext.blobOffset += pBlob->segLength;
#ifdef DSM_BLOB_TEST_UPDATE
    MSGD_CALLBACK(pcontext,
       (TEXT*)"%LdbBlobUpdate 9:  blobOffset %l totLength %l",
       pBlob->blobContext.blobOffset, pBlob->totLength);
#endif
    if (returnCode == DSM_S_BLOBOK && truncate)
    {
      returnCode = dbBlobEndSeg(pcontext, pBlob);
    }

    utfree (pSegTab);
    utfree (pRecord);

    return returnCode;

}  /* end dbBlobUpdate */


/* PROGRAM: dbBlobDelete - delete a blob in the database
 *
 * RETURNS: status code received from rmDelete or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t 
dbBlobDelete(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN blob descriptor */
    dsmText_t           *pName)         /* IN reference name for messages */
{
    dsmStatus_t  returnCode = DSM_S_BLOBLIMIT;
    xDbkey_t     xDbkey;        /* extended dbkey (area & recid) */
    dsmBuffer_t *pSegTab;       /* With the 1 byte indicator */
    LONG         segRC;
    lockId_t     lockId;
    dbBlobSegHdr_t *pSegHdr;

    TRACE_CB(pcontext, "dbBlobDelete");

    returnCode = dbBlobprmchk (pcontext, pBlob, (dsmMask_t)0, &xDbkey);
    if (returnCode)
    {
      return returnCode;
    }

    pSegTab = (dsmBuffer_t *)utmalloc(BLOBMAXSEGTAB);
    if (!pSegTab) 
    {
      return DSM_S_BLOBNOMEMORY;
    }
    pSegHdr = (dbBlobSegHdr_t *)pSegTab;

#ifdef DSM_BLOB_TEST
    MSGD_CALLBACK(pcontext, "%LdbBlobDelete: dbkey %l area %l",
                  xDbkey.dbkey, xDbkey.area);
#endif

    /* set lockId for a record get lock */
    lockId.table = xDbkey.area;
    lockId.dbkey = xDbkey.dbkey;
    
    returnCode = (dsmStatus_t)lkrglock(pcontext, &lockId, LKEXCL);
    if (returnCode) 
    {
      return returnCode;
    }

    segRC = rmFetchRecord (pcontext, xDbkey.area, xDbkey.dbkey, pSegTab,
                           BLOBMAXSEGTAB, 0);

    if (segRC >= 16 && pSegHdr->segid == DSMBLOBSEG)
    {
       returnCode = (dsmStatus_t)dbBlobDelSeg (pcontext, pBlob, &xDbkey, 0,
                                                pSegTab, pName);
    }
    else if (segRC >= 0)
    {
      returnCode = (dsmStatus_t)rmDeleteRec (pcontext, pBlob->blobObjNo,
                                             xDbkey.area,
                                             xDbkey.dbkey);
    }
    else 
    {
      returnCode = (dsmStatus_t)(segRC < 0 ? segRC : DSM_S_BLOBBAD);
    }

    utfree (pSegTab);

    lkrgunlk (pcontext, &lockId);

    return returnCode;

}  /* end dbBlobDelete */

/* PROGRAM: dbBlobEnd - Local function to end access to a particular blob
 *
 * RETURNS: status code received from lkrels or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dbBlobEnd(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob)         /* IN blob descriptor */
{
    TRACE_CB(pcontext, "dbBlobEnd");

    /* This was done to provide a common code within thread safe deadlock */

    if ((0 != pBlob->blobContext.blobCntx1) ||
	(0 != pBlob->blobContext.blobCntx2) ||
	(0 != pBlob->blobContext.blobOffset))
    {
      return dbBlobEndSeg(pcontext, pBlob);
    }

    return DSM_S_BLOBOK;
} /* end dbBlobEnd */


/* PROGRAM: dbBlobStart - Start access to a particular blob across multiple
 *                         Put/Get/Upd calls
 *
 * RETURNS: Always successful for v2, always fails for v1.
 */

dsmStatus_t 
dbBlobStart(
    dsmContext_t        *pcontext _UNUSED_, /* IN database context */
    dsmBlob_t           *pBlob)         /* IN blob descriptor */
{
    TRACE_CB(pcontext, "dbBlobStart");

    pBlob->blobContext.blobCntx1 = DSMBLOBMORE;
    pBlob->blobContext.blobCntx2 = 0;

    return DSM_S_BLOBOK;

} /* end dbBlobStart */

/* PROGRAM: dbBlobUnlock - Unlock an explicitly locked blob
 *
 * RETURNS: status code received from rmUpdateRecord, lkrels or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t 
dbBlobUnlock(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob)         /* IN blob descriptor */
{
    dsmStatus_t  returnCode = DSM_S_BLOBLIMIT;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    GBOOL         tmp_lock;
    xDbkey_t     xDbkey;          /* extended dbkey (area & recid) */
    lockId_t     lockId;
    

    TRACE_CB(pcontext, "dbBlobUnlock");

    if (pdbcontext->usertype & SELFSERVE) 
    {
	if (pdbcontext->resyncing || lkservcon(pcontext))
	{
	  return DSM_S_CTRLC;              /* Self-service only */
	}
    }

    returnCode = omIdToArea (pcontext, DSMOBJECT_BLOB, 
                             (COUNT)pBlob->blobObjNo, (COUNT)pBlob->blobObjNo,
                             &(xDbkey.area));

    if (returnCode)
    {
      return returnCode;
    }

    xDbkey.dbkey = pBlob->blobId;

    lockId.table = pBlob->blobObjNo;
    lockId.dbkey = pBlob->blobId;
    
    tmp_lock = ((pBlob->blobContext.blobCntx1 & DSMBLOBTMLK) != 0);

    if ((pBlob->blobContext.blobCntx1 & DSMBLOBOPMSK))
    {

        /* Update the blob length &/or release temp lock */

        returnCode = dbBlobEnd (pcontext, pBlob);
        if (returnCode)
        {
	  return returnCode;
	}
    }

    if (!tmp_lock)
    {
      lkrgunlk(pcontext, &lockId);
    }
    return DSM_S_BLOBOK;

}  /* end dbBlobUnlock */


