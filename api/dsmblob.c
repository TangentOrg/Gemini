
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
#include "dsmpub.h"
#include "dbblob.h"
#include "utmpub.h"

/* PROGRAM: dsmBlobGet - retrieve a blob from the database
 *
 * RETURNS: status code received from rmFetchRecord or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dsmBlobGet(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN blob descriptor */
    dsmText_t           *pName)         /* IN reference name for messages */
{
    dsmStatus_t returnCode = DSM_S_BLOBLIMIT;
    
    TRACE_CB(pcontext, "dsmBlobGet");

    pcontext->pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    returnCode = dsmThreadSafeEntry(pcontext);
    if (DSM_S_SUCCESS == returnCode)
    {
      returnCode = dbBlobGet(pcontext, pBlob, pName);
    }
    else
    {
      returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmBlobGet");
    }

#ifdef DSM_BLOB_GET_TEST
    MSGD_CALLBACK(pcontext, "%LdsmBlobGet: bytesRead  = %l", pBlob->bytesRead);
#endif

done:
    dsmThreadSafeExit(pcontext);

    pcontext->pdbcontext->inservice--;
    return returnCode;

} /* end dsmBlobGet */


/* PROGRAM: dsmBlobPut - create a blob in the database
 *
 * RETURNS: status code received from rmCreateRec or other procedures
 *          which this procedure invokes.
 */

dsmStatus_t
dsmBlobPut(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN blob descriptor */
    dsmText_t           *pName)         /* IN reference name for messages */
{
    dsmStatus_t returnCode = DSM_S_BLOBLIMIT;

    TRACE_CB(pcontext, "dsmBlobPut");

    pcontext->pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    returnCode = dsmThreadSafeEntry(pcontext);
    if (DSM_S_SUCCESS == returnCode)
    {
      returnCode = dbBlobPut(pcontext, pBlob, pName);
    }
    else
    {
      returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmBlobPut");
    }

#ifdef DSM_BLOB_PUT_TEST
    MSGD_CALLBACK(pcontext, "%LdsmBlobPut: maxLength = %l", pBlob->maxLength);
    MSGD_CALLBACK(pcontext, "%LdsmBlobPut: segLength = %l", pBlob->segLength);
    MSGD_CALLBACK(pcontext, "%LdsmBlobPut: totLength = %l", pBlob->totLength);
#endif

done:
    dsmThreadSafeExit(pcontext);

    pcontext->pdbcontext->inservice--;
    return returnCode;

} /* end dsmBlobPut */


/* PROGRAM: dsmBlobUpdate - update an existing blob in the database
 *
 * RETURNS: status code received from rmUpdateRecord or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t 
dsmBlobUpdate(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN blob descriptor */
    dsmText_t           *pName)         /* IN reference name for messages */
{
    dsmStatus_t returnCode = DSM_S_BLOBLIMIT;

    TRACE_CB(pcontext, "dsmBlobUpdate");

    pcontext->pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    returnCode = dsmThreadSafeEntry(pcontext);
    if (DSM_S_SUCCESS == returnCode)
    {
      returnCode = dbBlobUpdate(pcontext, pBlob, pName);
    }
    else
    {
      returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmBlobUpdate");
    }

done:
    dsmThreadSafeExit(pcontext);

    pcontext->pdbcontext->inservice--;
    return returnCode;

}  /* end dsmBlobUpdate */


/* PROGRAM: dsmBlobDelete - delete a blob in the database
 *
 * RETURNS: status code received from rmDelete or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t 
dsmBlobDelete(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN blob descriptor */
    dsmText_t           *pName)         /* IN reference name for messages */
{
    dsmStatus_t  returnCode = DSM_S_BLOBLIMIT;

    TRACE_CB(pcontext, "dsmBlobDelete");

    pcontext->pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    returnCode = dsmThreadSafeEntry(pcontext);
    if (DSM_S_SUCCESS == returnCode)
    {
      returnCode = dbBlobDelete(pcontext, pBlob, pName);
    }
    else
    {
      returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmBlobDelete");
    }
 
done:
    dsmThreadSafeExit(pcontext);

    pcontext->pdbcontext->inservice--;
    return returnCode;

}  /* end dsmBlobDelete */


/* PROGRAM: dsmBlobEnd - External function to end access to a particular blob
 *
 * RETURNS: status code received from lkrels or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dsmBlobEnd(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob)         /* IN blob descriptor */
{
    dsmStatus_t returnCode = DSM_S_BLOBLIMIT;
 
    TRACE_CB(pcontext, "dsmBlobEnd");
 
    pcontext->pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */
 
    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */
 
    returnCode = dsmThreadSafeEntry(pcontext);
    if (DSM_S_SUCCESS == returnCode)
    {
      returnCode = dbBlobEnd(pcontext, pBlob);
    }
    else
    {
      returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmBlobEnd");
    }
   
done:
    dsmThreadSafeExit(pcontext);

    pcontext->pdbcontext->inservice--;
    return returnCode;

} /* end dsmBlobEnd */


/* PROGRAM: dsmBlobStart - Start access to a particular blob across multiple
 *                         Put/Get/Upd calls
 *
 * RETURNS: Always successful for v2, always fails for v1.
 */

dsmStatus_t 
dsmBlobStart(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob)         /* IN blob descriptor */
{
    dsmStatus_t returnCode = DSM_S_BLOBLIMIT;

    TRACE_CB(pcontext, "dsmBlobStart");

    pcontext->pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    returnCode = dsmThreadSafeEntry(pcontext);
    if (DSM_S_SUCCESS == returnCode)
    {
      returnCode = dbBlobStart(pcontext, pBlob);
    }
    else
    {
      returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmBlobStart");
    }
 
done:
    dsmThreadSafeExit(pcontext);

    pcontext->pdbcontext->inservice--;
    return returnCode;
} /* end dsmBlobStart */


/* PROGRAM: dsmBlobUnlock - Unlock an explicitly locked blob
 *
 * RETURNS: status code received from rmUpdateRecord, lkrels or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t 
dsmBlobUnlock(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob)         /* IN blob descriptor */
{
    dsmStatus_t  returnCode = DSM_S_BLOBLIMIT;

    TRACE_CB(pcontext, "dsmBlobUnlock");

    pcontext->pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    returnCode = dsmThreadSafeEntry(pcontext);
    if (DSM_S_SUCCESS == returnCode)
    {
      returnCode = dbBlobUnlock(pcontext, pBlob);
    }
    else
    {
      returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmBlobUnlock");
    }

done:
    dsmThreadSafeExit(pcontext);

    pcontext->pdbcontext->inservice--;
    return returnCode;

}  /* end dsmBlobUnlock */


#ifdef BX_DEBUG

/* PROGRAM: dsmBlobDmp - dump a blob from the database
 *
 * RETURNS: status code received from rmFetchRecord or other procedures
 *          which this procedure invokes.
 */

dsmStatus_t
dsmBlobDmp(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pBlob,         /* IN blob descriptor */
    GBOOL                 silent)        /* IN silent except if an error */
{
    dsmStatus_t   returnCode;
    dbcontext_t  *pdbcontext;
    xDbkey_t      xDbkey;        /* extended dbkey (area & recid) */
    LONG          recordSize, segRC;
    LONG          maxRecordSize;
    dsmBuffer_t *pRecord;       /* With the 1 byte indicator */
    dsmBuffer_t *pSegTab;       /* With the 1 byte indicator */
    dsmBuffer_t *pST;           /* within pSegTab */
    LONG          nent;          /* number seg tab entries */
    LONG          segLen;
    LONG          i;
    LONG         remainder;
    long	 tl;
    LONG          n_ds = 0, n_ss = 0;
    dsmBuffer_t  pName [] = "bozo";
    void         printf (...);

    TRACE_CB(pcontext, "dsmBlobDmp");

    pdbcontext = pcontext->pdbcontext;
    if (pdbcontext->usertype & SELFSERVE) 
    {
	if (pdbcontext->resyncing || lkservcon(pcontext))
	    return DSM_S_CTRLC;                  /* Self-service only */
    }

    xDbkey.dbkey = pBlob->blobId;

    if (!silent)
        printf ("\nBlobId: %10ld ", xDbkey.dbkey);

    returnCode = omIdToArea (pcontext, DSMOBJECT_BLOB, 
                             (COUNT)pBlob->blobObjNo, &(xDbkey.area));

    if (returnCode) {
        if (!silent)
            printf ("omIdToArea for object Type: %ld blobObjNo: %ld returned %ld <--\n",
                    DSMOBJECT_BLOB, pBlob->blobObjNo, returnCode);
        return returnCode;
    }

    pdbcontext->inservice++;
    
    maxRecordSize = DSMBLOBMAXLEN + 1;

    pRecord = (dsmBuffer_t *)utmalloc (maxRecordSize);
    if (!pRecord)
    {
        pdbcontext->inservice--;
        returnCode = DSM_S_BLOBNOMEMORY;
        return returnCode;
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
        if (!silent)
            printf ("D.N.E. recordSize: 4, record: 0 0 0 0 <--\n");
    }
    else if (recordSize < 0) 
    {
        pBlob->segLength = 0;
	returnCode = (dsmStatus_t) recordSize;
        if (!silent)
            printf ("returnCode: %ld <--\n", returnCode);
    }
    else
    {
        returnCode =  DSM_S_BLOBOK;

        /* Set the total length of the blob */

        if (*pRecord == DSMBLOBDATA) 
        {
            n_ds++;
            pBlob->totLength = recordSize - 1;
        } else if (recordSize > 10)
            pBlob->totLength = xlng (pRecord + 7);
        else {
            pBlob->totLength = 0;
            returnCode = DSM_S_BLOBBAD;
        }

        if (!silent)
            printf ("Length: %10ld Type: %s\n", pBlob->totLength,
                    (char *) ((*pRecord == DSMBLOBDATA) ? "Direct" :
                              (*pRecord == DSMBLOBSEG) ? "Segmented" : "Bad <--"));
        tl = remainder = pBlob->totLength;

        if (returnCode == DSM_S_BLOBOK && *pRecord != DSMBLOBDATA) {
            if (xlng(pRecord+3) == 0)       /* Only 1 seg table */
                pSegTab = utmalloc (recordSize);
            else
                pSegTab = utmalloc (BLOBMAXSEGTAB);
            if (!pSegTab) 
            {
               utfree (pRecord);
               pdbcontext->inservice--;
               returnCode = DSM_S_BLOBNOMEMORY;
               return returnCode;
            }

            bufcop (pSegTab, pRecord, recordSize);

            while (returnCode == DSM_S_BLOBOK) 
            {
                n_ss++;
                pST = pSegTab + SEGHDR_LEN;
                nent = xct (pSegTab + 1);
                if (!silent) 
                    printf (" SegTab: %10ld number entries: %d %s\n", 
                            xDbkey.dbkey, nent, ((nent < 1 || nent > MAXSEGENT) ?
                                                 "<--" : ""));

                if (nent < 1 || nent > MAXSEGENT) 
                {
                    returnCode = DSM_S_BLOBBAD;
                    break;
                }
 
                for (i = 0; i < nent; i++, pST += SEGENT_LEN)
                {
                    segLen = xct (pST);
                    xDbkey.dbkey = xlng (pST + 2);
                    segRC = rmFetchRecord(pcontext, xDbkey.area,
                                       xDbkey.dbkey, pRecord,
                                       (COUNT)maxRecordSize, 0);
                    if (segRC <= 0) 
                    {
                        printf (" Seg%3d: %10ld returnCode: %d <--\n", i, 
                                xDbkey.dbkey, segRC);
                        returnCode = (dsmStatus_t)
                            (segRC ? segRC : DSM_S_BLOBBAD);
                        break;
                    }
                    if (segRC - 1 != xct (pST) || 
                        *pRecord != DSMBLOBDATA) 
                    {
                        printf (" Seg%3d: %10ld Len: %d ActualLen: %d type: %d <--\n",
                                i, xDbkey.dbkey, xct (pST), segRC - 1, *pRecord);
                        returnCode = DSM_S_BLOBBAD;
                        break;
                    }
                    if (!silent)
                         printf (" Seg%3d: %10ld Len: %d\n", i, xDbkey.dbkey, xct (pST));
                    n_ds++;
                    remainder -= xct (pST);
                } /* for i ... < nent */

                if (returnCode != DSM_S_BLOBOK)
                    break;

                xDbkey.dbkey = xlng (pSegTab + 3);

                if (!silent)
                    printf ("  N Seg: %10ld\n", xDbkey.dbkey);

                if (!xDbkey.dbkey) {
                    break;
                }
                returnCode = dbBlobFetch (pcontext, &xDbkey, pSegTab,
                                           BLOBMAXSEGTAB, 
                                           DSMBLOBSEG, pName);
            } /* while remainder */

            if (remainder)
                printf ("Length error: remainder: %10ld <--\n", remainder);

            utfree (pSegTab);

        } /* inital blob segemnt ok */
    } /* initial rmfetch ok */

    utfree (pRecord);

    pdbcontext->inservice--;
    if (!silent)
         printf ("tl: %10ld ds: %4d ss: %4d\n", tl, n_ds, n_ss);
    return returnCode;
}
#endif /* BX_DEBUG */


