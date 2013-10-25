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
#include "dsmxapub.h"
#include "dbxapub.h"
#include "dbcontext.h"
#include "tmtrntab.h"
#include "shmpub.h"
#include "usrctl.h"
#include "tmmgr.h"

/* PROGRAM: dbXAinit - Initialize the global xid free list
 *
 *
 * RETURNS: DSM_S_SUCCESS on success
 */
dsmStatus_t
dbxaInit(dsmContext_t *pcontext)
{
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    dbshm_t      *pdbshm = pdbcontext->pdbpub;
    LONG          maxxids,amount;
    int           i;
    dbxaTransaction_t *pxids, *plastxid = NULL;
    
    if (!pdbshm->maxxids)
        return 0;

   
    amount = pdbshm->maxxids;
    for(pdbshm->maxxids = 0; pdbshm->maxxids < amount;
        pdbshm->maxxids += 64 )
    {
        /* Allocate the xids in chunks of 64         */
        pxids = (dbxaTransaction_t *)
            stGet(pcontext,
                  XSTPOOL(pdbcontext, pdbshm->qdbpool),
                  64 * sizeof(dbxaTransaction_t));
        if(!pxids)
        {
            MSGD_CALLBACK(pcontext,
                          "%gInsuffient storage to allocate xid table.");
        }
        if(plastxid)
        {
            /* Make last of previous batch of 64 point to this batch */
            plastxid->qnextXID = P_TO_QP(pcontext,pxids);
        }
        if(!pdbshm->qxidFree)
        {
            pdbshm->qxidFree = P_TO_QP(pcontext,pxids);
        }
        
        for(i = 0; i < 64;i++,pxids++)
        {
            QSELF(pxids) = P_TO_QP(pcontext,pxids);
            pxids->qnextXID = P_TO_QP(pcontext,pxids + 1);
        }
        plastxid = pxids--;
        plastxid->qnextXID = 0;
    }
    
    return 0;
}

/* PROGRAM: dbxaGetSize - Return size of global transaction table

   RETURNS: sizeof global transaction table
   */
LONG
dbxaGetSize(dsmContext_t *pcontext)
{
    
    if(pcontext->pdbcontext->pdbpub->maxxids < 0)
        pcontext->pdbcontext->pdbpub->maxxids =
            pcontext->pdbcontext->pdbpub->argnclts / 2;
    else if(pcontext->pdbcontext->pdbpub->maxxids > 1000000)
        pcontext->pdbcontext->pdbpub->maxxids = 1000000;

    return (pcontext->pdbcontext->pdbpub->maxxids *
            sizeof (dbxaTransaction_t));
}

/* PROGRAM: dbxaXIDfind - Returns a pointer to global transaction for
 *                       the specified xid found in the transaction
 *                       table.
 *
 *
 * RETURNS: pointer to xid
 *          Null if there is no global tx for this xid.
 */
dbxaTransaction_t *dbxaXIDfind(dsmContext_t *pcontext,
                              dsmXID_t *pxid)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dbshm_t       *pdbshm = pdbcontext->pdbpub;
    dbxaTransaction_t *pglobalTran;

    MT_LOCK_TXT();
    for(pglobalTran = XXID(pdbcontext,pdbshm->qxidAlloc);
        pglobalTran;
        pglobalTran = XXID(pdbcontext,pglobalTran->qnextXID))
    {
        if(dbxaCompare(pxid,&pglobalTran->xid) == 0)
        {
            /* We found it      */
            MT_UNLK_TXT();
            return pglobalTran;
        }
    }
    MT_UNLK_TXT();
    /* If we get here it means we searched whole transaction table
       and we didn't get a match.                                   */
    pglobalTran = NULL;
        
    return pglobalTran;
}


/* PROGRAM: dbxaCompare - Compare two global transaction identifiers
                          for equality

   RETURNS: 0 - if equal
            1 - not equal
*/
int
dbxaCompare(dsmXID_t  *pxid1, dsmXID_t *pxid2)
{
    int  rc = 1;   /* Assume xids are not equal  */
    int  xidLen;
    int  j;
    
    /* Find if the xid matches                      */
    if(pxid1->gtrid_length == pxid2->gtrid_length &&
       pxid1->bqual_length == pxid2->bqual_length)
    {
        xidLen = pxid1->gtrid_length + pxid1->bqual_length;
        for( j = 0; j < xidLen; j++)
            if(pxid1->data[j] != pxid2->data[j])
                return rc;

        rc = 0;
    }    
    return rc;
}

/* PROGRAM: dbxaXIDget - Returns a pointer to global transaction for
 *                       the specified user context.
 *
 *
 * RETURNS: pointer to xid
 *          Null if there is no global tx for this user.
 */
dbxaTransaction_t *dbxaXIDget(dsmContext_t *pcontext)
{
    TX     *ptran;

    if(pcontext->pusrctl->uc_task)
        ptran = tmEntry(pcontext,pcontext->pusrctl->uc_task);
    else
        return NULL;

    return XXID(pcontext->pdbcontext,ptran->txP.qXAtran);
}


/* PROGRAM: dbxaAllocate - Allocate a global transaction structure from
                           the free list.  Assumes the caller has locked
                           the list before calling.

   RETURNS: pointer to global tx structure
 */
dbxaTransaction_t *dbxaAllocate(dsmContext_t *pcontext)
{
    dbxaTransaction_t  *pxaTran;
    dbcontext_t        *pdbcontext = pcontext->pdbcontext;
    dbshm_t            *pdbshm = pdbcontext->pdbpub;
    
    pxaTran = XXID(pdbcontext,pdbshm->qxidFree);
    if(pxaTran)
    {
        pdbshm->qxidFree = pxaTran->qnextXID;
        pxaTran->qnextXID = pdbshm->qxidAlloc;
        pdbshm->qxidAlloc = QSELF(pxaTran);
    }
    else
    {
        /* If the free list is empty we should wait for one to become free */
        ;
    }
    return pxaTran;
}

/* PROGRAM: dbxaFree - Return the specified global transaction structure
                       to the free list.

   RETURNS: nothing
*/
DSMVOID
dbxaFree(dsmContext_t  *pcontext, dbxaTransaction_t *ptran)
{
    dbshm_t   *pdbshm = pcontext->pdbcontext->pdbpub;
    
    ptran->flags = 0;
    ptran->trid = 0;
    ptran->referenceCount = 0;
    ptran->numSuspended = 0;
    ptran->lastRLblock = 0;
    ptran->lastRLoffset = 0;
    pdbshm->qxidAlloc = ptran->qnextXID;
    ptran->qnextXID = pdbshm->qxidFree;
    pdbshm->qxidFree = QSELF(ptran);

    return;
}

/* PROGRAM: dbxaEnd - End an association with a global transaction

   RETURNS:
   */
dsmStatus_t
dbxaEnd(dsmContext_t *pcontext, dbxaTransaction_t *pxaTran, LONG flags)
{
    dsmStatus_t returnCode = DSM_S_SUCCESS;
    usrctl_t    *pusr = pcontext->pusrctl;
    
    if(pxaTran->trid != pcontext->pusrctl->uc_task)
    {
        /* Maybe we're ending a suspended association */
        if(pcontext->pusrctl->uc_task == 0 &&
           pxaTran->numSuspended)
        {
            /* That must be what it is  */
            ;
        }
        else
        {
            returnCode = DSM_S_XAER_NOTA;
            goto done;
        }
    }
    if(pxaTran->flags & DSM_XA_PREPARED)
    {
        returnCode = DSM_S_XAER_PROTO;
        goto done;
    }
    
    switch (flags)
    {
        case DSM_TMSUCCESS:
            {
                MT_LOCK_TXT();
                pxaTran->referenceCount--;

                if(pusr->uc_task)
                    pusr->uc_task = 0;
                else
                    pxaTran->numSuspended--;
                
                pxaTran->lastRLblock = pusr->uc_lastBlock;
                pxaTran->lastRLoffset = pusr->uc_lastOffset;
                MT_UNLK_TXT();
                break;
            }
        case DSM_TMSUSPEND:
            {
                pusr->uc_task = 0;
                MT_LOCK_TXT();
                pxaTran->lastRLblock = pusr->uc_lastBlock;
                pxaTran->lastRLoffset = pusr->uc_lastOffset;                
                pxaTran->numSuspended++;
                MT_UNLK_TXT();
                break;
            }
        case DSM_TMFAIL:
            {
                MT_LOCK_TXT();
                pxaTran->referenceCount--;
                pxaTran->flags |= DSM_XA_ROLLBACK_ONLY;
                MT_UNLK_TXT();
                tmrej(pcontext,0,NULL);
                tmend(pcontext,TMREJ,NULL,1);

                break;
            }
        default:
            {
                returnCode = DSM_S_XAER_INVAL;
            }
    }
done:
    
    return returnCode;
}






