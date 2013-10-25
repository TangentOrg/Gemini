
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
#include "dbcontext.h"
#include "tmtrntab.h"
#include "dbxapub.h"
#include "usrctl.h"
#include "tmmgr.h"

/* PROGRAM: dsmXAcommit - Commit a branch of a global transaction
 *
 *
 * RETURNS: DSM_S_SUCCESS on success
 */
dsmStatus_t
dsmXAcommit(dsmContext_t *pcontext,dsmXID_t *pxid, LONG flags)
{
    dsmStatus_t  returnCode = 0;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    dbxaTransaction_t  *pcurrXid;
    usrctl_t     *pusr = pcontext->pusrctl;
    dsmTrid_t    saveTrid = pusr->uc_task;
    
    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmXAcommit");
        goto done;
    }
    
    /* Get a pointer to the global trid if there is one    */
    pcurrXid = dbxaXIDfind(pcontext,pxid);
    if(!pcurrXid)
    {
        returnCode = DSM_S_XAER_NOTA;
        goto done;
    }
    /* uc_task is where tm<xxx> routines find transaction number
       to end or rollback                                         */
    pusr->uc_task = pcurrXid->trid;
    switch(flags)
    {
        case DSM_TMONEPHASE:
            {
                MT_LOCK_TXT();
                if(pcurrXid->referenceCount)
                {
                    returnCode = DSM_S_XAER_PROTO;
                    MT_UNLK_TXT();
                    goto done;
                }
                MT_UNLK_TXT();

                tmend(pcontext,TMACC,NULL,1);
                MT_LOCK_TXT();
                dbxaFree(pcontext,pcurrXid);
                MT_UNLK_TXT();
                break;
            }
        case DSM_TMNOFLAGS:
            {
                MT_LOCK_TXT();
                if(!(pcurrXid->flags & DSM_XA_PREPARED))
                {
                    returnCode = DSM_S_XAER_PROTO;
                    MT_UNLK_TXT();
                    goto done;
                }
                MT_UNLK_TXT();
                tmend(pcontext,TMACC,NULL,1);
                MT_LOCK_TXT();
                dbxaFree(pcontext,pcurrXid);
                MT_UNLK_TXT();
                break;
            }
        default:
            {
                returnCode = DSM_S_XAER_INVAL;
                goto done;
            }        
    }
    
done:
   
    pusr->uc_task = saveTrid;
    
    dsmThreadSafeExit(pcontext);
    
    pdbcontext->inservice--;

    return returnCode;
}


/* PROGRAM: dsmXAprepare - Prepare a global transaction for commit
 *          
 *
 *
 * RETURNS: DSM_S_SUCCESS on success
 */
dsmStatus_t
dsmXAprepare(dsmContext_t *pcontext,dsmXID_t *pxid, LONG flags)
{
    dsmStatus_t  returnCode = 0;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    dbxaTransaction_t *pcurrXid;
    
    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmXAprepare");
        goto done;
    }

    /* Get a pointer to the global trid if there is one    */
    pcurrXid = dbxaXIDfind(pcontext,pxid);
    if(!pcurrXid)
    {
        returnCode = DSM_S_XAER_NOTA;
        goto done;
    }
    switch(flags)
    {
        case DSM_TMNOFLAGS:
            {
                MT_LOCK_TXT();
                if(pcurrXid->referenceCount ||
                   pcurrXid->flags & DSM_XA_PREPARED)
                {
                    returnCode = DSM_S_XAER_PROTO;
                    MT_UNLK_TXT();
                    goto done;
                }
                if(pcurrXid->flags & DSM_XA_ROLLBACK_ONLY)
                {
                    MT_UNLK_TXT();
                    returnCode = DSM_XA_RBROLLBACK;
                    goto done;
                }
                /* TODO Write a prepared note to the bi file.  */
                pcurrXid->flags |= DSM_XA_PREPARED;
                MT_UNLK_TXT();
                break;
            }
        default:
            {
                returnCode = DSM_S_XAER_INVAL;
                goto done;
            }
    }

    
done:

    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;

    return returnCode;
}


/* PROGRAM: dsmXArecover - Return the list of in-doubt global transactions
 *                    
 *
 *
 * RETURNS: DSM_S_SUCCESS on success
 */
dsmStatus_t
dsmXArecover(dsmContext_t *pcontext,dsmXID_t *pxid, LONG xidArraySize,
         dsmXID_t  *pxids, LONG *pxidsReturned,
         LONG *pxidsSoFar, LONG flags)
{
    dsmStatus_t  returnCode = 0;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
   
    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmXArecover");
        goto done;
    }

    *pxidsReturned = 0;
    *pxidsSoFar    = 0;
    
done:

    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;

    return returnCode;

}


/* PROGRAM: dsmXArollback - Rollback a global transaction
 *                     
 *
 *
 * RETURNS: DSM_S_SUCCESS on success
 */
dsmStatus_t
dsmXArollback(dsmContext_t *pcontext,dsmXID_t *pxid, LONG flags)
{
    dsmStatus_t  returnCode = 0;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    dbxaTransaction_t  *pcurrXid;
    
    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmXArollback");
        goto done;
    }
    /* Get a pointer to the global trid if there is one    */
    pcurrXid = dbxaXIDfind(pcontext,pxid);
    if(!pcurrXid)
    {
        returnCode = DSM_S_XAER_NOTA;
        goto done;
    }
    switch(flags)
    {
        case DSM_TMNOFLAGS:
            {
                MT_LOCK_TXT();
                if(pcurrXid->referenceCount != pcurrXid->numSuspended)
                {
                    /* referenceCount should be zero or equal
                       to number of suspended associations */
                    returnCode = DSM_S_XAER_PROTO;
                    MT_UNLK_TXT();
                    goto done;
                }
                if(!(pcurrXid->flags & DSM_XA_ROLLBACK_ONLY))
                {
                    dsmTrid_t saveTrid;
                    LONG      saveRLblock;
                    LONG      saveRLoffset;
                    usrctl_t  *pusr = pcontext->pusrctl;
                    
                    pcurrXid->flags |= DSM_XA_ROLLBACK_ONLY;
                    MT_UNLK_TXT();
                    saveTrid = pusr->uc_task;
                    saveRLblock = pusr->uc_lastBlock;
                    saveRLoffset = pusr->uc_lastOffset;
                    pusr->uc_task = pcurrXid->trid;
                    pusr->uc_lastBlock = pcurrXid->lastRLblock;
                    pusr->uc_lastOffset = pcurrXid->lastRLoffset;
                    tmrej(pcontext,0,NULL);
                    tmend(pcontext,TMREJ,NULL,1);
                    pusr->uc_task = saveTrid;
                    pusr->uc_lastBlock = saveRLblock;
                    pusr->uc_lastOffset = saveRLoffset;
                    MT_LOCK_TXT();
                }
                /* Now free the global transaction structure  */
                dbxaFree(pcontext,pcurrXid);
                MT_UNLK_TXT();
                break;
            }
        default:
            {
                returnCode = DSM_S_XAER_INVAL;
                goto done;
            }
    }


done:

    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;

    return returnCode;
}


/* PROGRAM: dsmXAstart - Start an association between a user context
 *                       and a global transaction
 *                     
 *
 *
 * RETURNS: DSM_S_SUCCESS on success
 */
dsmStatus_t
dsmXAstart(dsmContext_t *pcontext,dsmXID_t *pxid, LONG flags)
{
    dsmStatus_t  returnCode = 0;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    dbxaTransaction_t *pcurrXid;
    TX           *plocalTran;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmXAstart");
        goto done;
    }

    /* Get a pointer to the global trid if there is one    */
    pcurrXid = dbxaXIDfind(pcontext,pxid);
    switch (flags)
    {
    case DSM_TMNOFLAGS:
        {
            if(pcurrXid)
            {
                returnCode = DSM_S_XAER_DUPID;
                goto done;
            }
            if(pcontext->pusrctl->uc_task)
            {
                returnCode = DSM_S_XAER_OUTSIDE;
                goto done;
            }
            /* Allocate a global transaction structure  */
            MT_LOCK_TXT();
            pcurrXid = dbxaAllocate(pcontext);
            if(!pcurrXid)
            {
                returnCode = DSM_S_XAER_RMFAIL;
                MT_UNLK_TXT();
                goto done;
            }
            MT_UNLK_TXT();
            /* Start a local transaction */
            tmstrt(pcontext);
            /* Put a qpointer to it in the local transaction entry */
            plocalTran = tmEntry(pcontext, pcontext->pusrctl->uc_task);
            plocalTran->txP.qXAtran = QSELF(pcurrXid);
            pcurrXid->trid = plocalTran->transnum;
            bufcop(&pcurrXid->xid,pxid,sizeof(dsmXID_t));
            pcurrXid->referenceCount = 1;
            break;
        }
    case DSM_TMRESUME:
    case DSM_TMJOIN:
        {
            if(!pcurrXid)
            {
                returnCode = DSM_S_XAER_NOTA;
                goto done;
            }
            if(pcontext->pusrctl->uc_task)
            {
                returnCode = DSM_S_XAER_OUTSIDE;
                goto done;
            }
            MT_LOCK_TXT();
            if(pcurrXid->flags & DSM_XA_ROLLBACK_ONLY)
            {
                returnCode = DSM_XA_RBROLLBACK;
                MT_UNLK_TXT();
                goto done;
            }
            
            if(flags == DSM_TMJOIN)
            {
                /* increment the reference count on the xid */
                pcurrXid->referenceCount++;
            }
            else
            {
                pcurrXid->numSuspended--;
            }
            
            MT_UNLK_TXT();
            /* Assign local trid number to user control   */
            pcontext->pusrctl->uc_task = pcurrXid->trid;
            break;
        }
    default:
        {
        returnCode = DSM_S_XAER_INVAL;
        goto done;
        }
    }

    
done:

    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;

    return returnCode;
}

/* PROGRAM: dsmXAend - End an association between a user context
 *                       and a global transaction
 *                     
 *
 *
 * RETURNS: DSM_S_SUCCESS on success
 */
dsmStatus_t
dsmXAend(dsmContext_t *pcontext,dsmXID_t *pxid, LONG flags)
{
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t   returnCode = 0;
    dbxaTransaction_t *pcurrXid;
    
    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmXAend");
        goto done;
    }
    /* Get a pointer to the global trid if there is one    */
    pcurrXid = dbxaXIDget(pcontext);
    if(!pcurrXid)
    {
        returnCode = DSM_S_XAER_NOTA;
        goto done;
    }

    returnCode = dbxaEnd(pcontext,pcurrXid,flags);

done:

    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;

    return returnCode;
}



