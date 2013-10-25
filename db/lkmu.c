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

/*********************************************************************
*
*       C Source:               lkmu.c
*       Subsystem:              1
*       Description:
*       %created_by:    marshall %
*       %date_created:  Tue Dec 20 10:11:50 1994 %
*
**********************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"

#include "bkpub.h"
#include "cxpub.h"
#include "cxprv.h"
#include "dbcontext.h"
#include "dbmsg.h"
#include "dbpub.h"  /* for dblksch & dbenvout call */
#include "drsig.h"  /* for use of imported signal variables */
#include "dsmpub.h"  /* for DSM API */
#include "latpub.h"
#include "lkpub.h"
#include "lkschma.h"
#include "mumsg.h"
#include "rlai.h"
#include "rlprv.h"
#include "rlmsg.h"
#include "stmpub.h"  /* storage mgt sub sys */
#include "stmprv.h"
#include "svmsg.h"
#include "tmmgr.h"
#include "tmpub.h"
#include "usrctl.h" 
#include "utcpub.h"
#include "utmsg.h"


/* PROGRAM: lkrmtask -- remove a user from the TSK waiting queue
 *
 *      Input Parameters
 *      ----------------
 *      premusr - pointer to the usrctl of the user to be removed
 *
 *
 * RETURNS:DSMVOID 
 */
DSMVOID
lkrmtask(
	dsmContext_t	*pcontext,
	usrctl_t	*premusr)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t 	*pdbshm = pdbcontext->pdbpub;
    usrctl_t	*pusr;
    usrctl_t	*prev=(usrctl_t *)NULL;

    MT_LOCK_TXT ();

    premusr->uc_wait = 0;
    premusr->uc_wait1 = 0;

    for(pusr = XUSRCTL(pdbcontext, pdbshm->qwttsk); pusr;) 
    {
	if (pusr == premusr)
	{
	    if (QSELF(pusr) == pdbshm->qwttsk)
	    {
		pdbshm->qwttsk = pusr->uc_qwtnxt;
	    }
	    else
	    {
		prev->uc_qwtnxt = pusr->uc_qwtnxt;
	    }
	    pusr->uc_qwtnxt = 0;
	    break;
	}
	else
	{
	    prev = pusr;
	    pusr = XUSRCTL(pdbcontext, pusr->uc_qwtnxt);
	}
    }
    MT_UNLK_TXT ();

}  /* end lkrmtask */


/* PROGRAM: lkwaketsk -- wakes up users waiting on the task.
 *
 *      Input Parameters
 *      ----------------
 *      taskn;          database task number
 *
 * RETURNS: int 
 */
int
lkwaketsk(
	dsmContext_t	*pcontext,
	LONG		 taskn)
{
    if (pcontext->pdbcontext->arg1usr)
        return (0);

    MT_LOCK_TXT ();
    lkwakeup(pcontext, &pcontext->pdbcontext->pdbpub->qwttsk, taskn, TSKWAIT);
    MT_UNLK_TXT ();

    return (0);

}  /* end lkwaketsk */


/*****************************************************************
 * 								
 * PROGRAM: lkwakeusr  		 				
 * 	wake up a given user. The wake up algorithm depened     
 * 	on the use user type SELFSERVE, DBSERVER, etc.         	
 *
 *      Input Parameters
 *      ----------------
 *      pusr
 *      type             type of wait queue
 *
 *
 * RETURNS: DSMVOID 
 */
DSMVOID
lkwakeusr(
	dsmContext_t	*pcontext,
	usrctl_t	*pusr,
	int		 type)
{
    latUsrWake (pcontext, pusr, type);
}  /* end lkwakeusr */


/* PROGRAM: lkwakeup   		 				
 * 	- remove all the users from a given wait chain	
 * 	and wake them upwake them up              if the  value	
 * 	is equal to that in the pusrctl->uc_wait1	
 * 	if the given value is 0, all the users in the chain	
 * 	are wakedup                                        
 * 							
 *      Input Parameters
 *      ----------------
 *      pqanchor       ptr to anchor of the chain of waiting
 *      value          value to be compared with uc_wait1 
 *      type;          type of wait queue
 *
 * RETURNS: DSMVOID 
 */
DSMVOID
lkwakeup(
	dsmContext_t	*pcontext,
	QUSRCTL		*pqanchor,
	DBKEY		 value,
	int		 type)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t	*pcur; 
    usrctl_t	*prev = (usrctl_t *)0; 
    usrctl_t	*pusr; 

    for (pcur = XUSRCTL(pdbcontext, *pqanchor); pcur; )
    {
	if ((value == 0) || (pcur->uc_wait1 == value))
	{

            /* ignore (don't remove) expired schema locks -
             * (the user will take care of this as part of t/o processing)
             */
            if ( (type == SCHWAIT) && (pcur->uc_schlk & SLEXPIRED) )
            {
                /* next entry */
                pcur = XUSRCTL(pdbcontext, pcur->uc_qwtnxt);
                continue;
            }

            /* match - remove this usrctl from chain */
            if (*pqanchor == QSELF (pcur))
            {
                /* first on queue, so we have to adjust head  to next */
                *pqanchor = pcur->uc_qwtnxt;
            }
            else
            {
                /* not first on queue (remove it) */
                prev->uc_qwtnxt = pcur->uc_qwtnxt;
            }

            pusr = pcur;

            /* next entry */
            pcur = XUSRCTL(pdbcontext, pcur->uc_qwtnxt);

            /* wake current entry */
            pusr->uc_qwtnxt = 0;
            lkwakeusr(pcontext, pusr, type); /* wake up the user */
        }
        else
        {
            /* no match - advance to next entry */
            prev = pcur;
            pcur = XUSRCTL(pdbcontext, pcur->uc_qwtnxt);
        }
    }

}  /* end lkwakeup */


/* PROGRAM: lkunchain - remove a user from a wait chain
 *
 *
 * RETURNS: DSMVOID 
 */
DSMVOID
lkunchain(
	dsmContext_t	*pcontext,
	int		 latchNo,  /* latch which protects the queue */
	usrctl_t	*pusr,     /*user to be removed from the chain */
	QUSRCTL		*pqhead,   /* ptr to head of the chain of waiting */
	QUSRCTL		*pqtail)   /* ptr to tail of the chain of waiting */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t	*pcur; 
    usrctl_t	*prev; 
    QUSRCTL		 q;

    /* lock the chain */

    if (latchNo)
        MT_LATCH (latchNo);

    /* scan the wait chain, looking for the user we want to dequeue */

    prev = NULL;
    q = QSELF (pusr);
    for (pcur = XUSRCTL(pdbcontext, *pqhead); pcur; )
    {
	if (pusr == pcur)
	{
	    /* found the user we are looking for */

	    if (*pqhead == q)
	    {
		/* we are removing first entry so update head */

		*pqhead = pcur->uc_qwtnxt;
	    }
            else prev->uc_qwtnxt = pcur->uc_qwtnxt;
             
            if (pqtail)
            {
		/* There is a tail pointer for this queue */

                if (*pqhead == 0)
		{
		    /* queue is empty now */

		    *pqtail = 0;
		}
                else if (*pqtail == q)
                {
                   /* we took off the last entry, update tail */

                   if (prev == NULL) *pqtail = 0;
                   else *pqtail = QSELF (prev);
                }
            }

	    /* reset wait control info in usrctl */
            if (pusr->uc_wait == SCHWAIT) 
                pusr->uc_schlk &= ~(SLEXPIRED);
	    pusr->uc_qwtnxt = 0;
            pusr->usrwake = 0;
            pusr->uc_wait = 0;
            pusr->uc_wait1 = 0;
              
	    break;
	}
        /* advance to next entry in chain */

	prev = pcur;
        pcur = XUSRCTL(pdbcontext, pcur->uc_qwtnxt);
    }

    /* unlock the chain */
    if (latchNo)
        MT_UNLATCH (latchNo);

}  /* end lkunchain */


/* PROGRAM:   lkresync    
 *      - resync. the user, clear all its locks                 
 *
 *      Input Parameters
 *      ----------------
 *      pusr
 *
 *
 * RETURNS: 0 on success
 *         -1 on error 
 */
LONG
lkresync(
	dsmContext_t *pcontext)

{
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    usrctl_t     *pusr = pcontext->pusrctl;
    int           force_rej = 0;
    int           force;
    int           noresync_type;
    int           rcomm;
    LONG          ret;

    if ( !pusr )
        return 0; /* 92-08-19-031 */ /* Not logged in yet */

    if (pdbcontext->shmgone)
        return 0;
    if (pusr->resyncing)
        return 0; /* in the resync process already */

    MT_LOCK_USR ();

    pusr->resyncing = 1;
    force = pusr->uc_2phase & TPFORCE;
    noresync_type = pdbcontext->usertype &
                           (BROKER+DBSERVER+DBSHUT+DBUTIL+MONITOR);
    rcomm =  pusr->uc_2phase & TPRCOMM ;
 
    /* Attempt to resync noresync_type in the middle of 2phase commit will */
    /* cause a crash. Instead, turn the client into a LIMBO client.        */
    /* A self server (noresync_type == 0) should not normally get to this  */
    /* point. If it does, however, it will crash in tmrej().               */
    if (rcomm && noresync_type)
    {
        if (!force)
        {
            pusr->uc_2phase |= TPLIMBO;

            MT_UNLK_USR ();

	    /* Can not reject transaction of user # on tty */

            MSGN_CALLBACK(pcontext, muMSG001,
                          pusr->uc_usrnum, XTEXT(pdbcontext, pusr->uc_qttydev));

            ret = -1;
            goto done;
        }
        else
        {
            force_rej = 1;
            pusr->uc_2phase &= ~(TPFORCE+TPLIMBO);
        }
    }
    MT_UNLK_USR ();

    /* resync dispensed by HLI layer in order to close cursors and resync to
       a the point at which transactions are started/ended (at the same level
       that the commit work/rollback work occur) */

    if (pusr->uc_sqlhli)
    {
        ret = 0;
	goto done;
    }

    /* cancel waiting if possible, else wait */
    lkrmwaits(pcontext);
 
    if (pdbcontext->pdbpub->shutdn == DB_EXBAD)
        goto bad; /* avoid resync, bad exit */

    /* become that user for a while to finish up */

    if (pusr->uc_task != 0)
    {
      if(tmNeedRollback(pcontext,pusr->uc_task))
      {
        MSGN_CALLBACK(pcontext, utMSG087);
	tmrej (pcontext, force_rej, (dsmTxnSave_t *)PNULL);
        MSGN_CALLBACK(pcontext, utMSG088);
      }
      tmend (pcontext, TMREJ, NULL, (int)0);
    }

    lkrend(pcontext, 0);
    cxDeaCursors(pcontext); /*free any CX cursors owned by the child*/
 
    /* if the user had a schema lock, release it */

    if (pusr->uc_schlk & SHARECTR)
        dblksch(pcontext, SLRELS);

    if (pusr->uc_schlk & EXCLCTR)
        dblksch(pcontext, SLRELX);

    if (pusr->uc_schlk & GUARSERCTR)
        dblksch(pcontext, SLRELGS);
 
bad:

#ifndef PRO_REENTRANT
    if (pdbcontext->usertype & SELFSERVE)
        pdbcontext->resyncing = 1; /* to prevent bfrej from server functions */
#endif
 
    ret = 0;
 
done:
    pusr->resyncing = 0;
    return ret;

}  /* end lkresync */


/* PROGRAM:  lkrmwaits - cancel any wait condition by removing
 *      its information from the respective table or chain 
 *
 *      Input Parameters
 *      ----------------
 *      pusr
 *
 *
 * RETURNS: 
 */

DSMVOID
lkrmwaits(dsmContext_t *pcontext)
{
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    dbshm_t  *pdbshm = pdbcontext->pdbpub;
    usrctl_t *pusr   = pcontext->pusrctl;

     /* The shutdown can not be waiting on any of these things and will
        get an error when clearing the semaphore since it waits for
        the broker to delete the semaphore before leaving */
 
     if (pusr->uc_usrtyp == DBSHUT) return;
     if (pusr->uc_usrtyp == DBSHUT+BATCHUSER) return;

    /* Cancel any waiting on lock (excluding latches).
       Each wait queue is protected by some latch or another, so to remove
       the user from the queue, the latch must be locked first */
    /* NB: It is not possible to remove someone from a latch wait queue
       using this mechanism */

    switch (pusr->uc_wait)
    {
    case 0:		/* not waiting */
    	    break;

    case RECWAIT:	/* waiting for record lock */
	    lkcncl (pcontext); /* cancel any pending record lock */
	    break;
    case SCHWAIT:	/* waiting for schema lock */
	    lkunchain(pcontext, MTL_SCC, pusr,&(pdbshm->qwtschma),
                     (QUSRCTL  *)PNULL);
	    break;

    case TSKWAIT:	/* waiting for transaction to complete */
	    lkunchain(pcontext, MTL_TXT, pusr, &(pdbshm->qwttsk),
                      (QUSRCTL *)PNULL);
	    break;

    case IXWAIT:	/* waiting for index lock */
	    /* lkrend takes care of index locks */
	    break;

    case NOBUFWAIT:	/* waiting for database buffer to become available */
	    bmCancelWait (pcontext);
	    break;

    case RGWAIT:	/* waiting for record get lock */
	    /* lkrend takes care of rec-get locks */
	    break;

    case BIREADWAIT:	/* waiting for buffer to be read from bi file */
	    lkunchain (pcontext, MTL_BIB, pusr, &(pdbcontext->prlctl->qrdhead),
		       &(pdbcontext->prlctl->qrdtail));
	    break;

    case BIWRITEWAIT:	/* waiting for buffer to be written to bi file */
	    lkunchain (pcontext, MTL_BIB, pusr, &(pdbcontext->prlctl->qwrhead),
		       &(pdbcontext->prlctl->qwrtail));
	    break;

    case AIWRITEWAIT:	/* waiting for buffer to be written to ai file */
	    lkunchain (pcontext, MTL_AIB, pusr, &pdbcontext->paictl->qwrhead,
		       &(pdbcontext->paictl->qwrtail));
	    break;

    case SHAREWAIT:	/* waiting for share lock on a database buffer */
    case EXCLWAIT:	/* waiting for exclusive lock on a database buffer */
	    bmCancelWait (pcontext);
	    break;

    case BIWWAIT:	/* bi writer waiting for work */
	    lkunchain (pcontext, MTL_BIB, pusr, &pdbcontext->prlctl->qbiwq,
                       NULL);
	    break;
    case AIWWAIT:	/* ai writer waiting for work */
	    lkunchain (pcontext, MTL_AIB, pusr, &pdbcontext->paictl->qaiwq,
                       NULL);
	    break;

    case TXSWAIT:
    case TXBWAIT:
    case TXXWAIT:
    case TXEWAIT:	/* transaction end lock */
	    lkunchain (pcontext, MTL_TXQ, pusr, &(pdbcontext->prlctl->qtxehead),
		       &(pdbcontext->prlctl->qtxetail));
	    break;
    default:
	    FATAL_MSGN_CALLBACK(pcontext, dbFTL011, pusr->uc_wait);
    }

    latSemClear (pcontext);

}  /* end lkrmwaits */


/* PROGRAM: lktskchain  -- used when a user is supposed to wait for TSK.
 *               	 the user is chained to the task wait chain. 
 *
 *      Input Parameters
 *      ----------------
 *      pusr
 *
 * RETURNS:	0 if our usrctl has been queued in the task wait chain
 *		1 if task is dead
 */
int
lktskchain(dsmContext_t *pcontext)
{
    dbshm_t 	*pdbshm = pcontext->pdbcontext->pdbpub;
    usrctl_t	*pusr = pcontext->pusrctl;
    MT_LOCK_TXT ();

    if (tmdtask (pcontext, pusr->uc_wait1))
    {
        /* task is dead. Clear the wait */

        pusr->uc_wait = 0;
        pusr->uc_wait1 = 0;
        MT_UNLK_TXT ();
        return (1);
    }
    /* chain the usr in front of the task waiting chain */

    pusr->uc_qwtnxt = pdbshm->qwttsk;
    pdbshm->qwttsk = QSELF(pusr);

    MT_UNLK_TXT ();

    return (0);

}  /* end lktskchain */


/* PROGRAM: lkservcon  -- check if shared memory is available.
 *			if available return 0
 *			if not available - writes msg "server disconnect"
 *			and returns DSM_S_CTRLC.	
 *
 *      
 *
 * RETURNS:  int - (0) - if shared memory, service allowed.
 *                       if not, no check done.
 *                 (DSM_S_CTRLC) - stop the task.
 */
int
lkservcon(
	dsmContext_t	*pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    if (!pdbcontext->shmgone)
        return 0; /* service is allowed, shm ok */

    utsctlc(); /*set IOSTOP (ctrl-c) on*/

    return DSM_S_CTRLC;

}  /* end lkservcon */
