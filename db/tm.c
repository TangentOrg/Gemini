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

#include <stdio.h>

#include "dbconfig.h"    /* standard db include file */
#include "dbmgr.h"       /* BUM - db public interface ? */
#include "usrctl.h"      /* BUM - db public interface ? */
#include "dbcontext.h"   /* BUM - db public interface ? */
#include "dbpub.h"
#include "bkcrash.h"     /* crash structures */
#include "bkpub.h"       /* BUM - shouldn't need to include this? */
#include "bmpub.h"       /* bm public interface */
#include "rlpub.h"       /* note processing, flushing buffers */
#include "tmmgr.h"       /* tm public interface */
#include "tmtrntab.h"    /* tm public interface? */
#include "tmdoundo.h"    /* tm note definitions */
#include "tmpub.h"       /* tm public interface */
#include "tmmsg.h"       /* tm message file */
#include "cxpub.h"       /* index delete chain */
#include "latpub.h"      /* latching structures */
#include "lkpub.h"       /* release locks at transaction end */
#include "stmpub.h"      /* storage subsystem */
#include "stmprv.h"      /* storage subsystem */

#include "tmprv.h"
#include "utcpub.h"      /* crash test code */
#include "utspub.h"

#include <time.h>        /* OS time() function */
#include "dbcode.h"      /* BUM - needed to detect temp tables */

/**** LOCAL FUNCTION PROTOTYPES ****/
LOCALF DSMVOID 	tmstrtdelay(dsmContext_t *pcontext, LONG curtime);
LOCALF DSMVOID	tmflsh(dsmContext_t *pcontext);
/**** END LOCAL FUNCTION PROTOTYPES ****/


/* PROGRAM: tmstrt -- start a transaction
 *
 * "reserves" an available transaction id from transaction table and
 * updates the masterblock last task value.  The transaction begin note
 * is postponed until the requestor actually does some work which 
 * requires recovery logging.
 *
 * RETURNS: nothing
 */
DSMVOID
tmstrt (dsmContext_t    *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    usrctl_t    *pusr   = pcontext->pusrctl;
    LONG     localTrid;
    TRANTAB *ptran = pdbcontext->ptrantab;
    int      slotNumber;
    TX      *pTranEntry;
    LONG        timestmp;
    
    TRACE_CB(pcontext, "tmstrt");
    
    if (pdbcontext->prlctl == (RLCTL *)NULL)
    {
        /* Logging disabled */
        if (pdbcontext->dbcode != PROCODET)
        {
            /* Not a temp-table database */
            return;
        }
    }
    
#if SHMEMORY
    if (pdbcontext->resyncing || lkservcon(pcontext)) return; /* resyncing in progress */
#endif
    /* Sanity checks */
    
    /* This should never happen if the caller is using the 
    appropriate interface to the dsm.  */
    if (pusr->uc_task)
    {
        /* transaction already running */
        FATAL_MSGN_CALLBACK(pcontext, tmFTL002);
    }
    if (pusr->uc_2phase & (TPRCOMM+TPLIMBO))
    {
        /* tmstrt: Cannot start a transaction while ready-to-commit */
        FATAL_MSGN_CALLBACK(pcontext, tmFTL005);
    }
    
    pusr->uc_2phase &= TPNO2PHS; /* Reset all but not TPNO2PHS */
    
    if (pdbcontext->dbcode == PROCODET)
    {
        /* no need to get the time for temp-tables.  Infact we have seen
        that the time() call is extremely slow on WIN95 when using the
        WIN32API calls. */
        timestmp = 0;
    }
    else
    {
        time (&timestmp);
    }
    
    MT_LOCK_MTX ();
    if (!pdbpub->didchange )
    {
        rlsetfl(pcontext);      /* label the database open and set flags */
        MT_UNLK_MTX();
        /* Start a micro-transaction to hold off the on-line backup utility */
        rlTXElock(pcontext,RL_TXE_SHARE,RL_MISC_OP);
        tmlstmod(pcontext, timestmp);
        rlTXEunlock(pcontext);
        MT_LOCK_MTX();
    }
    MT_LOCK_TXT ();
    
    if (ptran->numlive >= ptran->maxtrn)
    {

#ifdef TRANSTABLETRACE
    MSGD_CALLBACK(pcontext, (TEXT *)"%Ltmstrt: numlive[%l], maxtrn[%l]",
                  ptran->numlive, ptran->maxtrn);
#endif /* TRANSTABLETRACE */

        /* too many concurrent transactions */
        MT_UNLK_TXT ();
        MT_UNLK_MTX ();
        FATAL_MSGN_CALLBACK(pcontext, tmFTL001);
    }
    
    /* find the next available local transaction id */
    for (localTrid = pdbcontext->pmstrblk->mb_lasttask + 1; ; localTrid++)
    {
        if (localTrid < 0)
        {
            /* If the transaction number has become negative, we
            roll over and start at 1 again. This is extremely
            unlikely to cause a problem since it takes so long to
            roll over. The transaction id can not be negative
            because we use negative transaction number as locks
            in the index and for deleted rows */
            localTrid = 1;
        }
        slotNumber = tmSlot(pcontext, localTrid);
        if (slotNumber >= ptran->maxtrn)
        {
            /* off the end of the table, try a different transaction
            id */
            
            /* We don't have to do that, since we allocate */
            /* ptran->trmask + 1 slots. But taking out this */
            /* code caused problems. To do it we need more */
            /* examination.                    */ 
            localTrid = localTrid - slotNumber + ptran->trmask;
        }
        else
        {
            pTranEntry = &(ptran->trn[slotNumber]);
            
            /* skip over trans ids which have the same slot number
            as some other currently live transaction */
            if (pTranEntry->txstate == TM_TS_DEAD)
            {
                if (pTranEntry->transnum == 0)
                {
                    /* We take this one, it is free.
                    Mark the slot as being allocated. It is not
                    officially allocated until we have called the
                    transaction begin do function, but we need to
                    mark the slot so someone else does not come
                    along and take it when we release the lock on
                    the transaction table */
                    pTranEntry->txstate = TM_TS_ALLOCATED;
                    break;
                }
                else
                {
                    /* This is not supposed to happen */
                    MT_UNLK_TXT ();
                    MT_UNLK_MTX ();
                    MSGN_CALLBACK(pcontext, tmMSG008,
                        slotNumber, pTranEntry->transnum);
                    MT_LOCK_MTX ();
                    MT_LOCK_TXT ();
                }
            }
        }
    }
    MT_UNLK_TXT ();
    
    /* continue to fill in the transaction table entry for allocated status */
    pTranEntry->usrnum = pusr->uc_usrnum;
    pTranEntry->rlcounter = 0;
    pTranEntry->transnum = localTrid;
    pusr->uc_task = localTrid;
    pTranEntry->transnum = localTrid;
    pdbcontext->pmstrblk->mb_lasttask = localTrid;
    
    MT_UNLK_MTX();
    
    /* end tmstrt */
}

/* PROGRAM: tmend -- transaction end
 *
 * Perform commit or rollback processing depending on the accept/reject code
 * provided.  Commit of individual phases of 2PC is performed if requested.
 * The accept/reject code can be one of the following:
 *
 *	PHASE1:		commit phase 1a: prepare to commit
 *	PHASE1q2:	commit phase 1b: flush prepare record to log
 *	TMACC:		commit phase 2: commit
 *	TMREJ:		roll back to start
 *
 * RETURNS: 0 if success or nothing done
 *          DSM_S_NO_TRANSACTION - a transaction operation request
 *                                 was issued against prior to a
 *                                 start request.
 *          -1 on failures
 */
int
tmend (
	dsmContext_t	*pcontext,
	int		 accrej,  /* accept/reject code */
	int		 *ptpctl,  /* Not implemented for gemini */
	int		 service) /* 1 if normal clients' TMEND request */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    usrctl_t    *pusr   = pcontext->pusrctl;
    TX		*pTranEntry;
    LONG	localTrid;
    TMNOTE	note;
    LONG	curtime;
    TEXT	rcommbuf[sizeof(LONG)+MAXNICKNM+4];
    TEXT	*ptmp;
    LONG64	dpend;
    int		ret;
    int		iAmCoordinator = 0;
    ULONG64     lockChains;
    
    TRACE_CB(pcontext, "tmend")

    if (pdbcontext->prlctl == (RLCTL *)NULL)
    {
	/* Logging disabled */

        if (pdbcontext->dbcode != PROCODET)
	{
	    /* Not a temp-table database */

	    return (0);
	}
    }
    if (pdbcontext->resyncing) return (-1);
#if SHMEMORY
    if (lkservcon(pcontext)) return (-1);
#endif

    /* Check to make sure a transaction start was issued prior */
    if (pusr->uc_task == 0)
            return(DSM_S_NO_TRANSACTION);

    localTrid = pusr->uc_task;

      /*------------------------------------------------*/
      /* Bug 20000114-034:				*/
      /*						*/
      /*    Initialize the fillers in the TMNOTE.	*/
      /*------------------------------------------------*/

    INIT_S_TMNOTE_FILLERS( note );

    if (pusr->uc_2phase & TPCRD)
    {
	/* I am the corrdinator for this transaction and am responsoible
	   for logging the outcome of the transaction */

	iAmCoordinator = 1;
    }

    pTranEntry = tmEntry(pcontext, localTrid);
 
    if (pusr->uc_2phase & TPLIMBO)
    {
	/* This is not supposed to happen. but just in case, we check here */

        if (pdbcontext->usertype & SELFSERVE) /* Limbo self-server must die. */
	{
            /* died with a limbo transaction. */
            FATAL_MSGN_CALLBACK(pcontext, tmFTL006);
	}
        else if (service)
	{
	    /* Limbo client is ignored */

            return 0;
	}
    }
    switch (accrej)
    {
	case PHASE1:	/* phase 1: please prepare to commit */
            if (!ptpctl || !pdbcontext->pnicknm)
            {
	        /* no two-phase commit for this transaction. 
	           We always vote yes. */

	        return (0);
            }

            if (pTranEntry->txstate == TM_TS_ALLOCATED)
            {
                /* This transaction never really did anything but
                 * we must write the txbgn note now to keep 2pc happy.
                 */
                rlWriteTxBegin(pcontext);
            }

            /* Enter the ready to commit state. */

            if (ptpctl && pdbcontext->pnicknm)
            {
                /* We are being asked if we want to be coordinator, so we
		   return the transaction id and that we agree to do it */

                pusr->uc_2phase |= TPCRD;
                return (0);
            }
            pTranEntry->txstate = TM_TS_PREPARING;

	    /* Spool a ready to commit record to the log */

            STATINC (tprcCt);
 
            /* determine the time the xaction ended */
            if (pdbcontext->dbcode == PROCODET)
            {
                /* no need to get the time for temp-tables.  Infact we have
                   seen that the time() call is extremely slow on WIN95 when
                   using the WIN32API calls. */
                curtime = 1;
            }
            else
            {
                time (&curtime);
            }
 
            /* set up a ready to commit note and DO it */
            INITNOTE(note, RL_RCOMM, RL_PHY | RL_LOGBI );
            note.xaction  = localTrid;
            note.timestmp = curtime;
            note.accrej   = accrej;

	    /* store coordinator's transaction id */ 

            slng (rcommbuf, 0);

            /* coordinator's dbname */

            ptmp = stcopy(rcommbuf + sizeof(LONG), (TEXT *)"Not implemented");
 
	    MT_LOCK_MTX ();

            rlLogAndDo(pcontext, &note.rlnote, 0, (COUNT)(ptmp - rcommbuf + 1 ),
                    rcommbuf );
            pusr->uc_lastAiCounter = pdbcontext->pmstrblk->mb_aictr;
            /* Now in Ready-To_Commit (prepared) state */

            pusr->uc_2phase |= TPRCOMM;

	    MT_UNLK_MTX ();

            /* self-server waits in phase PHASE1q2 */

            if (pdbcontext->usertype & DBSERVER)
            {  
	        /* Server now waits for ready to commit record to be flushed
	           safely to disk */

                dpend = pusr->uc_lstdpend;
                pusr->uc_lstdpend = 0;
 
	        STATINC (tpflCt);
                if (dpend)
                    return (rlwtdpend (pcontext, dpend,pusr->uc_lastAiCounter));
            }
            return 0;

        case PHASE1q2:	/* phase 1: are you prepared ? */
            if (pTranEntry->txstate == TM_TS_ALLOCATED)
            {
                /* This transaction never really did anything but
                 * we must write the txbgn note now because the txstate
                 * is about to change and to keep 2pc happy.
                 * (This should never happen since when asked to prepare
                 *  we should have begun the transaction.)
                 */
                rlWriteTxBegin(pcontext);
            }

            if ( !pdbcontext->pnicknm)
            {
	        /* not doing 2pc on this database so LIE and say ready */

                pTranEntry->txstate = TM_TS_PREPARED;
	        return (0);
	    }
            if (iAmCoordinator)
	    {
	        /* coordinator does not have to wait */

                pTranEntry->txstate = TM_TS_PREPARED;
                return (0);
	    }
	    /* Self-service now waits for ready to commit record to be flushed
	       safely to disk */

            dpend = pusr->uc_lstdpend;
            pusr->uc_lstdpend = 0;
 
            STATINC (tpflCt);
            rlwtdpend (pcontext, dpend,pusr->uc_lastAiCounter);

	    /* now we are prepared */

            pTranEntry->txstate = TM_TS_PREPARED;
            return 0;

        default:
	    accrej = TMREJ;

	case TMACC: /* Phase 2: Commit */
	case TMREJ: /* Phase 2: Abort */

            if (pdbcontext->resyncing) return (0);
#if SHMEMORY
	    if (lkservcon(pcontext)) return (0);
#endif
            if (pTranEntry->txstate == TM_TS_ALLOCATED)
            {
                /* This transaction never really did anything so
                 * just free the entry from the transaction table
                 * and do no recovery logging.
                 */

               MT_LOCK_MTX ();
               MT_LOCK_TXT ();
               lockChains = pTranEntry->txP.lockChains | pusr->uc_lkmask;
               stnclr((TEXT *)pTranEntry, (int)sizeof(TX));
               MT_UNLK_TXT ();
        
               pusr->uc_2phase &= TPNO2PHS;  /* Reset all but not TPNO2PHS */
               MT_UNLK_MTX ();

               /* release lock table locks */
               lktend(pcontext,lockChains );
               pusr->uc_task = 0;
               pusr->lktbovfw = 0;
               pusr->uc_lastBlock = 0; /* chains together notes for 1 xaction */
               pusr->uc_lastOffset = 0; /* chains together notes for 1 xaction*/
               pusr->uc_lstdpend = 0;

               /* wake anybody who is waiting for our transaction to end */
               lkwaketsk (pcontext, localTrid);

                if (accrej == TMACC)
                    STATINC (commitCt);

               return 0;
            }

            pTranEntry->txstate = TM_TS_COMMITTING;

            /* Check for active crashtesting */
            if (pdbcontext->p1crashTestAnchor)
                bkCrashTm(pcontext);

            if (pdbcontext->dbcode == PROCODET)
            {
                /* no need to get the time for temp-tables.  Infact we have
                   seen that the time() call is extremely slow on WIN95 when
                   using the WIN32API calls. */
                curtime = 1;
            }
            else
            {
                time (&curtime);
            }

            /* set up a transaction commit note and DO it */

            INITNOTE(note, iAmCoordinator ? RL_CTEND : RL_TEND,
                     RL_PHY | RL_LOGBI );
            note.xaction  = localTrid;
            note.timestmp = curtime;
            note.accrej   = accrej;
            note.rlcounter = 0;		/* bug 20000114-034 by dmeyer in v9.1b */
            note.usrnum = 0;		/* bug 20000114-034 by dmeyer in v9.1b */
            note.lasttask = 0;		/* bug 20000114-034 by dmeyer in v9.1b */


	    /* For crash recovery to work correctly, the transaction
	       commit record can not be written during someone else's
	       microtransaction. So we get an exclusive transaction
	       end lock. This ensures that all microtransactions
	       currently active will be finished and no new ones can
	       begin till our commit is done */

            rlTXElock(pcontext, RL_TXE_COMMIT, RL_MISC_OP);

            lockChains = pTranEntry->txP.lockChains | pusr->uc_lkmask;
            MT_LOCK_MTX ();
	    /* do the transaction end. This writes the transaction end
	       record and frees our slot in the transaction table */

            rlLogAndDo(pcontext, &note.rlnote, 0, (COUNT)0, PNULL );

            pusr->uc_lastAiCounter = pdbcontext->pmstrblk->mb_aictr;

            /* release the transaction end lock */

            rlTXEunlock (pcontext);

            pusr->uc_2phase &= TPNO2PHS;  /* Reset all but not TPNO2PHS */

             /* flush the bi and ai blocks MUST be AFTER rlLogAndDo */

            if (pdbpub->fullinteg)
	    {
		/* start the lazy commit timer. This puts an upper
		   bound on the time the commit record can sit in the
		   buffers when we are using lazy commit */

		tmstrtdelay(pcontext, curtime);
	    }
            MT_UNLK_MTX (); /* unlock mt before releasing locks */

            /* release lock table locks */
            lktend(pcontext, lockChains);
            
            pusr->uc_task = 0;
            pusr->lktbovfw = 0;
            pusr->uc_lastBlock = 0;  /* chains together notes for 1 xaction */
            pusr->uc_lastOffset = 0;  /* chains together notes for 1 xaction */
            dpend = pusr->uc_lstdpend;
            pusr->uc_lstdpend = 0;

	    /* wake anybody who is waiting for our transaction to end */

            lkwaketsk (pcontext, localTrid);

            if (accrej == TMACC) STATINC (commitCt);

            if ((pdbpub->tmenddelay == 0 || iAmCoordinator) && (dpend != 0))
            {
                /* wait for spooled transaction end log record to be
		   written to disk */
 
                ret = rlwtdpend (pcontext, dpend, pusr->uc_lastAiCounter);
            }
            else ret = 0;
 
            return (pdbcontext->usertype & DBSERVER ? ret : 0);
        }

}  /* end tmend */


/* PROGRAM: tmlstmod - change database master block last modification field
 *
 * This program creates a note to modify the masterblock last modification
 * time and then executes the note and flushes the masterblock to disk.
 *
 * RETURNS: nothing
 */
DSMVOID
tmlstmod(
	dsmContext_t	*pcontext,
	LONG		 timeval)  /* the new value for last modification date */
{
    dbshm_t           *pdbpub = pcontext->pdbcontext->pdbpub;
    RLMISC            lstmodnt;
    bmBufHandle_t     mstrHandle;
    struct  mstrblk   *pmstr;


    INITNOTE(lstmodnt, RL_LSTMOD, RL_PHY);
    lstmodnt.rlvalue = timeval;
    lstmodnt.rlvalueBlock = 0;	    /* bug 20000114-034 by dmeyer in v9.1b */
    lstmodnt.rlvalueOffset = 0;	    /* bug 20000114-034 by dmeyer in v9.1b */


    mstrHandle = bmLocateBlock(pcontext, BK_AREA1, 
                        BLK1DBKEY(BKGET_AREARECBITS(DSMAREA_SCHEMA)), TOMODIFY);
    pmstr = (struct mstrblk *)bmGetBlockPointer(pcontext,mstrHandle);
    pdbpub->didchange = (TEXT)1;

    rlLogAndDo(pcontext, &lstmodnt.rlnote, mstrHandle, (COUNT)0, PNULL );

    bmReleaseBuffer(pcontext, mstrHandle);

    bmFlushMasterblock(pcontext);

}  /* end tmlstmod */


/* PROGRAM: tmrej -- backout but not end a task
 *
 * This function causes the work associated with a transaction to be undone
 * but does not end the transaction.
 * 
 * RETURNS: nothing
 */
DSMVOID
tmrej(
	dsmContext_t	*pcontext,
	int		 force,    /* 1 if force reject after Ready-To-Commit */
        dsmTxnSave_t *psavepoint)  /* Savepoint number - SQL ONLY */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    usrctl_t    *pusr = pcontext->pusrctl;
    LONG	localTrid;
    TX		*pTranEntry;

    TRACE_CB(pcontext, "tmrej")

    if (pdbcontext->prlctl == (RLCTL *)NULL)
    {
	/* Logging disabled */

        if (pdbcontext->dbcode != PROCODET)
	{
	    /* Not a temp-table database */

	    return;
	}
    }
#if SHMEMORY
    if (pdbcontext->resyncing || lkservcon(pcontext))
        return; /* resyncing in progress */
#endif
    localTrid = pusr->uc_task;
    /* This should never happen if the caller is using
       the appropriate dsm api interface.  */
    if (localTrid == 0)
        FATAL_MSGN_CALLBACK(pcontext, tmFTL004);  /* forgot to start tsk? */

    pTranEntry = tmEntry(pcontext, localTrid);
 
    if ( pTranEntry->txstate && 
         (pTranEntry->txstate != TM_TS_ALLOCATED) )
    {
        /* give user lock table extension privilege for resyncing*/
        pusr->lktbovfw |= INTMREJ;
        pTranEntry->txflags |= TM_TF_UNDO;

        if (pusr->uc_lastBlock || pusr->uc_lastOffset)
        {
            /* back out the task */

            /* Check for active crashtesting - if this is active the task
               will of backing out will purposely be interrupted */
            if (pdbcontext->p1crashTestAnchor)
                bkBackoutExit(pcontext, (TEXT *)"tmrej", pusr->uc_pid);

    	    rlrej (pcontext, localTrid, psavepoint, force);
        }
        pusr->lktbovfw = 0; /* end lock table extension privilage */
        pTranEntry->txflags = pTranEntry->txflags & (~TM_TF_UNDO);

    }

    STATINC (undoCt);

    return;

}  /* end tmrej */


/* PROGRAM: tmchkdelay - Make sure tmend notes get flushed periodically
 *
 * This program should be called periodically by the BROKER.  It checks 
 * if the oldest unflushed tmend note is older than the limit set by the
 * -Mf parm.
 *
 * RETURNS: nothing
 */
DSMVOID
tmchkdelay(
	dsmContext_t	*pcontext,
	int		 reset)	/* if non-0, turn off delayed tmend mechanism */
{
    dbshm_t     *pdbpub = pcontext->pdbcontext->pdbpub;
    LONG	 curtime;
    int	 maxdelay;

    maxdelay = pdbpub->tmenddelay;
    if (maxdelay == 0) return;	/* -Mf parm was not used */

    time (&curtime);	/* get current time */

    if ((pdbpub->tmend_bi > 0) && ((pdbpub->tmend_bi + maxdelay) <= curtime))
    {
        /* there is an old unflushed tmend note in .bi file */

	tmdoflsh(pcontext);
    }
    else if ((pdbpub->tmend_ai > 0) &&
	     ((pdbpub->tmend_ai + maxdelay) <= curtime))
    {
        /* there is an old unflushed tmend note in .ai file */

	tmdoflsh(pcontext);
    }
    if (reset)
    {
	/* turn off the timer */

	pdbpub->tmenddelay = 0;
    }

    return;

}  /* end tmchkdelay */


/* PROGRAM: tmstrtdelay - either flush a tmend note or start its timer
 *
 * This program is called AFTER a tmend note is written to .bi or .ai
 * file.  It either flushes the buffers or starts a delay timer
 */
LOCALF DSMVOID
tmstrtdelay(
	dsmContext_t	*pcontext,
	LONG		 curtime) /* start time */
{
    dbshm_t     *pdbpub = pcontext->pdbcontext->pdbpub;

    if (pdbpub->tmenddelay > 0)
    {
	/* if the timer is not running, start a timer for this one */

	if (pdbpub->tmend_bi == 0) pdbpub->tmend_bi = curtime;
	if (pdbpub->tmend_ai == 0) pdbpub->tmend_ai = curtime;
    }
 
   return;

}  /* end tmstrtdelay */


/* PROGRAM: tmdidflsh - record the fact that a bi/ai block was flushed
 *
 * When a .bi or .ai block is flushed, then there is no tmend note left
 * so this program resets the delay timer.
 *
 * RETURNS: nothing
 */
DSMVOID
tmdidflsh(
	dsmContext_t	*pcontext,
	int		 type) /*BKBI or BKAI to indicate type of flush */
{
    dbshm_t     *pdbpub = pcontext->pdbcontext->pdbpub;

    switch(type)
    {
        case BKBI: pdbpub->tmend_bi = 0; break;
        case BKAI: pdbpub->tmend_ai = 0; break;
    }
 
    return;

}  /* end tmdidflsh */


/* PROGRAM: tmdoflsh - flush any pending tmend notes unconditionally
 *
 * This program is called when a user logs out to make sure transaction end
 * notes are flushed.
 *
 * RETURNS: nothing
 */
DSMVOID
tmdoflsh(dsmContext_t *pcontext)
{
    dbshm_t     *pdbpub = pcontext->pdbcontext->pdbpub;
    /* check for pending transaction end notes */
    if ((pdbpub->tmend_bi > 0)	|| (pdbpub->tmend_ai > 0))
    {
	/* could be one, flush the buffers */
	tmflsh(pcontext);
    }
    
    return;

}  /* end tmdoflsh */


/* PROGRAM: tmflsh - flush the bi and ai blocks for a transaction end
 *
 * This program tell the logging subsytem to flush blocks as part of
 * transaction end processing.
 */
LOCALF DSMVOID
tmflsh(
	dsmContext_t	*pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    if (pdbcontext->prlctl == (RLCTL *)NULL)
    {
	/* Logging is turned off */

        pdbpub->tmend_ai = 0;
	pdbpub->tmend_bi = 0;

	return;
    }
    MT_LOCK_MTX (); /* LOCK MT */

    /* flush all the bi buffers */

    rlbiflsh(pcontext, RL_FLUSH_ALL_BI_BUFFERS);

    rlaifls(pcontext);

    pdbpub->tmend_ai = 0;	/* needed in case .ai is turned off */

    MT_UNLK_MTX (); /* UNLOCK MT */

    return;

}  /* end tmflsh */


/* BUM - this needs new name needs to be reworked - it does gets() - yuck! */
/* PROGRAM: tmdlg - resolve or display ready-to-commit trancations 
 *  
 * This program attempts to resolve in-doubt LIMBO transactions that are in the
 * ready-to-commit state.  If recovery mode DB2PHASE is on, ask the user what 
 * to do with each of the LIMBO transactions. If DB2PHASE is off, list the
 * LIMBO transactions and exit. This function is called at the end of recovery.
 *
 * RETURNS: nothing
 */
DSMVOID
tmdlg(
	dsmContext_t	*pcontext,
	int		 mode)  /* recovery mode */
{
    dbcontext_t    *pdbcontext = pcontext->pdbcontext;
    COUNT      i;
    TRANTABRC  *ptrctab = pdbcontext->ptrcmtab;
    TXRC       *ptxrc;
    TMNOTE     tmnote;
    int        failrcvr = 0;
    int        commit = 0;
    int        sure;
    TEXT       ans[8];

    if (ptrctab == NULL) return;

    /* Sanity check */	

    if (ptrctab->numrc < 0)
        FATAL_MSGN_CALLBACK(pcontext, tmFTL007);

    if (!ptrctab->numrc) return;

    if (!(mode & DB2PHASE)) failrcvr = 1;

    for (i = 0; i <= pdbcontext->ptrantab->trmask; i++)
    {
	ptxrc = &ptrctab->trnrc[i];
	
        if (ptxrc->trnum)
	{
	    if (failrcvr)
	    {
		/* Transaction %d, on coordinator <nickname> in rcomm state */
		MSGN_CALLBACK(pcontext, tmMSG002,
                              ptxrc->trnum, ptxrc->crdnam, ptxrc->crdtrnum);
	    }
	}
        if (ptxrc->trnum && !failrcvr)
	{
            sure = 0;
            while (!sure)
            {
		/* Commit transaction %d, on coordinator <nickname> ? */
                MSGN_CALLBACK(pcontext, tmMSG003,
                              ptxrc->trnum, ptxrc->crdnam, ptxrc->crdtrnum);
    	        fgets((char *)ans,sizeof (ans),stdin);
                if ( stcomp(ans, (TEXT *) "q\n") == 0 ||
                     stcomp(ans, (TEXT *) "Q\n") == 0 )
		{
		    dbExit(pcontext, DB_NICE_EXIT, DB_EXLIMBO);
		}
		else 
		{
                    if ((stcomp(ans, (TEXT *) "y\n") == 0) ||
                        (stcomp(ans, (TEXT *) "Y\n") == 0))
		    {
			commit = TP_COMMIT;
		    }
		    else
		    {
			commit = TP_ABORT;
		    }
		}
 
                if (commit == TP_COMMIT)
		{
		    /* Are you sure you want to commit ? */
                    MSGN_CALLBACK(pcontext, tmMSG004, ptxrc->trnum);
		}
                else
		{
		    /* Are you sure you want to abort ? */
                    MSGN_CALLBACK(pcontext, tmMSG005, ptxrc->trnum);
		}
                fgets((char *)ans,sizeof(ans),stdin);
                sure = !(stcomp(ans, (TEXT *) "y\n") == 0 ||
                         stcomp(ans, (TEXT *) "Y\n") == 0) ?
    		         0 : 1; 
            }

	    INITNOTE (tmnote, RL_RTEND, (RL_PHY | RL_LOGBI));

	      /*--------------------------------------------*/
	      /* Bug 20000114-034:			    */
	      /*					    */
	      /*    Initialize the fillers in the TMNOTE.   */
	      /*--------------------------------------------*/

	    INIT_S_TMNOTE_FILLERS( tmnote );

            slng((TEXT *)&tmnote.rlnote.rlTrid, ptxrc->trnum);
	    tmnote.xaction = ptxrc->trnum;
	    
	    if (commit == TP_COMMIT) tmnote.accrej = TMACC;
	    else if (commit == TP_ABORT) tmnote.rlnote.rlcode = RL_TABRT;

 	    rlLogAndDo(pcontext, &tmnote.rlnote, 0, (COUNT)0, PNULL );
	}
    }
    if (failrcvr)
    {

	/* The database contains Ready-To-Commit transactions */
        MSGN_CALLBACK(pcontext, tmMSG007);

	/* See list on the log. Use proutil %s -C 2phase recover. */
        MSGN_CALLBACK(pcontext, tmMSG006, pdbcontext->pdbname);

        dbExit(pcontext, DB_NICE_EXIT, DB_EXLIMBO);
    }

    return;

}  /* end tmdlg */


/* PROGRAM: tmdtask - check if transaction is dead or alive
 *
 * Takes a transaction id and looks at the appropriate transaction table slot
 * to see if it contains the requested transaction.
 *
 * RETURNS: 0 if transaction is alive and well
 *          1 if the transaction is dead and gone
 */
int
tmdtask(
	dsmContext_t	*pcontext,
	LONG		 localTrid)  /* transaction id */
{
    int	slot;

    slot = tmSlot(pcontext, localTrid);
    if (tmnum(pcontext, slot) == localTrid)
    {
        /* transaction is still alive */

        return (0);
    }
    return (1);

}  /* end tmdtask */


/* PROGRAM: tmNeedRollback - Did the tx actually change anything
 *  
 *
 * RETURNS:	0 - Tx did NOT change anything.
 *		1 - Tx DID change something.
 */
int
tmNeedRollback (dsmContext_t *pcontext, dsmTrid_t localTrid)
{
    TX		*pTranEntry;
   
    if (!localTrid) return (0);

    pTranEntry = tmEntry(pcontext, localTrid);

    if ( pTranEntry->txstate &&
	 (pTranEntry->txstate != TM_TS_ALLOCATED) )
      return(1);

    return (0);

} 


/* PROGRAM: tmremdlcok - check if ok for transaction to remove index delete lock
 *
 * Lock can be removed if:
 *      the transaction is our own
 *      the transaction is dead
 *      the transaction is in the COMMITTING state, which means
 *      it is clearing or has cleared its chain
 *      the transaction number has changed meaning the tx ended
 *      and the tx table slot has been reused
 *
 * RETURNS:  0 - delete NOT allowed
 *           1 - delete allowed
 */
int
tmremdlcok(
        dsmContext_t    *pcontext,
        LONG             localTrid) /* transaction id */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    TX          *pTranEntry;


    if ( pdbcontext->ptrantab == NULL )
    {
        /* Must be being called during index rebuild; so its always ok
           to remove the delete lock since all txs must have ended before
           index rebuild.                                      */
        return (1);
    }

    pTranEntry = tmEntry(pcontext, localTrid);

    if ((pTranEntry->txstate == TM_TS_DEAD) ||
        (pTranEntry->txstate == TM_TS_COMMITTING) ||
        (pTranEntry->transnum != localTrid))
        return (1);

    return (0);

}  /* end tmremdlcok */  

/* PROGRAM: tmctask - get current task number
 *
 * RETURNS: n - current task number from user control structure
 */
LONG
tmctask(dsmContext_t *pcontext)
{
    return pcontext->pusrctl->uc_task;
}

/* PROGRAM: tmntask - get number of live transactions
 *
 * RETURNS: n - number of live transactions in transaction table
 */
int
tmntask(dsmContext_t *pcontext)
{
    int	        n;

    MT_LOCK_TXT ();
    n = pcontext->pdbcontext->ptrantab->numlive;
    MT_UNLK_TXT ();
    return (n);
}


/* PROGRAM: tmSlot - Return the slot number in the trans table for a given trid
 *
 */
ULONG
tmSlot(
        dsmContext_t *pcontext,
        LONG          trId)   /* transaction id */
{
    return (trId & pcontext->pdbcontext->ptrantab->trmask);
 
}  /* end tmSlot */


/* PROGRAM: tmEntry - Return a pointer to the transaction entry given the trid
 *
 * RETURNS 0 - If invalid trid
 */
TX *
tmEntry(
        dsmContext_t *pcontext,
        LONG          trId)   /* transaction id */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    return pdbcontext->ptrantab->trn + (trId & pdbcontext->ptrantab->trmask);
 
}  /* end tmEntry */

/* PROGRAM: tmLockChainSet - Set bit in transactions lock chain mask
 *
 * RETURNS:  DSMVOID
 */
DSMVOID
tmLockChainSet(dsmContext_t *pcontext, ULONG64 lkmask)
{
    usrctl_t  *pusr = pcontext->pusrctl;
    TX        *ptran;
    
    if(pusr->uc_task)
    {
        ptran = tmEntry(pcontext,pusr->uc_task);
        ptran->txP.lockChains |= lkmask;
    }
    else
        pusr->uc_lkmask |= lkmask;

    return;
}


/* PROGRAM: tmnum - get transaction number given 
 *
 * Return the full transaction number corresponding to provided
 * transaction index.
 *
 * RETURNS: 0 - If the parameter represents a dead transaction
 *          n - transaction number for active transaction
 */
LONG
tmnum(
	dsmContext_t	*pcontext,
	ULONG		 txSlot) /* transaction index */
{
    return pcontext->pdbcontext->ptrantab->trn[txSlot].transnum;
}


/* PROGRAM: tmGetTxInfo - 
 *
 * RETURNS: DSMVOID
 */
DSMVOID
tmGetTxInfo (
        dsmContext_t *pcontext, 
        LONG    *pTid,   /* returned tx id or 0 if none */
        LONG    *pSlot,  /* returned trans table entry number */
        ULONG   *pTtime) /* returned tx start time or 0 if none */
{
        TX              *pTranEntry;
    usrctl_t *pusr = pcontext->pusrctl;
 
    if (!pusr->uc_task)
    {
        /* No transaction is active */
 
        *pTid = (LONG)0;
        *pTtime = (ULONG)0;
    }
    /* Find transaction table entry  */
 
    pTranEntry = tmEntry(pcontext, pusr->uc_task);
 
    *pTid = pTranEntry->transnum;
    *pTtime = pTranEntry->txtime;
    *pSlot = tmSlot(pcontext, pusr->uc_task);

}  /* end tmGetTxInfo */


/* PROGRAM: tmrlctr - check if cluster contains live transactions
 *
 * This function determines if the given rlcounter belongs to
 * an rl cluster containing live xaction notes by looking at
 * each active transaction.
 *
 * RETURNS: 0 - The rl file cluster contains no live transaction notes
 *	    1 - The rl file cluster does contain live xaction notes
 */
int
tmrlctr(
	dsmContext_t	*pcontext,
	LONG		 rlcounter)	/* rlcounter from the rl cluster */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
	TX	*ptrn;
	int	 i;

    for (ptrn = pdbcontext->ptrantab->trn, i = pdbcontext->ptrantab->maxtrn;
	 i > 0; i--, ptrn++)
    {
	if (ptrn->txstate &&
            (ptrn->txstate != TM_TS_ALLOCATED) &&
            (ptrn->rlcounter <= rlcounter) )
            return 1;
    }

    return 0;

}  /* end tmrlctr */


/* PROGRAM: tmInitMask - calculate transaction table mask
 *
 * Calculate the transaction table mask value based on the number of concurrent
 * transacations that is requested.
 *
 * RETURNS: mask value
 */
int
tmInitMask(int maxusr)  /* max number of concurrent transactions */
{
    int j = 1;         
    while (j < maxusr )  j <<= 1;
    return(j - 1);
}

/* PROGRAM: tmalc - allocate a live transaction table
 *
 * Allocates a transaction table and hangs it off the database control structure
 * to handle the requested number of concurrent transactions.  If the requested
 * number is larger than than the max allowed, then the max allowed is silently
 * used.  If 2PC is active, an additional table is allocated.
 *
 * RETURNS: nothing
 */
DSMVOID
tmalc(
	dsmContext_t	*pcontext,
	int		 maxtrn) /* max number of concurrent transactions */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    int mask;
    TRANTAB *ptran;
    ULONG   maxActiveTxs;
TRACE1_CB(pcontext, "tmalc maxtrn=%i", maxtrn)

    maxActiveTxs = rlGetMaxActiveTxs(pcontext);
    MT_LOCK_TXT ();
    /* BUM - need to change argnclts interface from int to LONG
     *       assume here that mactrn > 0
     */
    
#ifdef TRANSTABLETRACE
    MSGD_CALLBACK(pcontext, (TEXT *)"%Ltmalc: maxtrn[%l], maxActiveTxs[%l]",
                  maxtrn, maxActiveTxs);
#endif /* TRANSTABLETRACE */

    if ((ULONG)maxtrn > maxActiveTxs)
    {
        maxtrn = maxActiveTxs;
    
#ifdef TRANSTABLETRACE
    MSGD_CALLBACK(pcontext, (TEXT *)"%Ltmalc: maxtrn[%l] = maxActiveTxs[%l]",
                  maxtrn, maxActiveTxs);
#endif /* TRANSTABLETRACE */
    }

    mask = tmInitMask(maxtrn);

    ptran = (TRANTAB *)stGet(pcontext,(STPOOL *)QP_TO_P(pdbcontext, pdbpub->qdbpool),
			 (unsigned)(TRNSIZE(mask+1)));

    pdbpub->qtrantab = P_TO_QP(pcontext, ptran);
    pdbcontext->ptrantab = ptran;
    
    pdbcontext->ptrantab->maxtrn = maxtrn;
    pdbcontext->ptrantab->trmask = mask;
    
    if (pdbcontext->dbcode != PROCODET && pdbcontext->pmstrblk->mb_nicknm[0])
    {
	/* 2phase commit is enabled so allocate a ready to commit
	   transaction table */

	tmrcmalc(pcontext);
    }
    MT_UNLK_TXT ();
    return;

}  /* end tmalc */


/* PROGRAM: tmrealc - adjust the size of an existing transaction table
 *
 * This function shrinks the size of an existing transaction table to the
 * requested size.  If the request is larger than existing table, then it
 * is ignored.
 *
 * RETURNS: nothing
 */
DSMVOID
tmrealc(
	dsmContext_t	*pcontext,
	int		 maxtrn)   /* the new desired size */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    ULONG    maxActiveTxs;

    TRACE1_CB(pcontext, "tmrealc maxtrn=%i", maxtrn)
    maxActiveTxs = rlGetMaxActiveTxs(pcontext);
    /* BUM - need to change argnclts interface from int to LONG
     *       assume here that mactrn > 0
     */
    
#ifdef TRANSTABLETRACE
    MSGD_CALLBACK(pcontext, (TEXT *)"%Ltmrealc: maxtrn[%l], maxActiveTxs[%l]",
                  maxtrn, maxActiveTxs);
#endif /* TRANSTABLETRACE */

    if( (ULONG)maxtrn > maxActiveTxs )
    {
	    maxtrn = maxActiveTxs;
    
#ifdef TRANSTABLETRACE
    MSGD_CALLBACK(pcontext, (TEXT *)"%Ltmrealc: maxtrn[%l] = maxActiveTxs[%l]",
                  maxtrn, maxActiveTxs);
#endif /* TRANSTABLETRACE */
    }

    MT_LOCK_TXT ();
    if (maxtrn < pdbcontext->ptrantab->maxtrn)
    {
       pdbcontext->ptrantab->maxtrn = maxtrn;
       pdbcontext->ptrantab->trmask = tmInitMask(maxtrn);
    }
    MT_UNLK_TXT ();
    return;

}  /* end tmrealc */


/* PROGRAM: tmrcmalc - allocate a ready-to-commit transaction table
 *
 * Allocate ready-to-commit transaction table to support 2PC transacations
 * using the size of the existing transacation table.
 *
 * If transaction T is in slot i in the trantab then it's RL_RCOMM note will
 * be in slot i in the trcmtab.
 *
 * RETURNS: nothing
 */
DSMVOID
tmrcmalc(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    TRANTABRC *ptrantabrc;
    
    ptrantabrc =  (TRANTABRC *)stGet(pcontext, (STPOOL *)QP_TO_P(pdbcontext, pdbpub->qdbpool),
			 (unsigned)(TRNRCSIZE(pdbcontext->ptrantab->trmask + 1)));
    pdbcontext->ptrcmtab = ptrantabrc;
    pdbpub->qtrantabrc = P_TO_QP(pcontext, ptrantabrc);
    
    ptrantabrc->numrc = 0;
    return;

}  /* end tmrcmalc */


#if 0
/* PROGRAM: trcmscan - FOR DEBUGGING. */
DSMVOID
trcmscan()
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    TMNOTE      **pprnote;
    TMNOTE      *prnote;
    COUNT       notsz;
    COUNT       i;
    LONG        trnum;
    TEXT        *pcoordnm;
    TEXT        hdrsz; 

    for(pprnote = (TMNOTE **)pdbcontext->ptrcmtab, i = 0;
        i <= pdbcontext->ptrantab->trmask;
	pprnote++, i++)
    {
	if (prnote = *pprnote)
	{
	    notsz = sct((TEXT *) prnote);
	    prnote = (TMNOTE *)((TEXT *)prnote + sizeof(COUNT));
	    hdrsz =  prnote->rlnote.rllen;
	    trnum = xlng( (TEXT *)prnote + hdrsz);
	    pcoordnm = ((TEXT *)prnote + hdrsz + sizeof(LONG));
	}
    }

   return;

}  /* end trcmscan */
#endif


/* PROGRAM: tmMarkSavePoint - establish savepoint and record its value
 *                            in usrctl.
 *
 *
 * RETURNS: DSM_S_SUCCESS - successful
 *          DSM_S_BADSAVE - savepoint value is invalid.
 *          Or never return because of FATAL
 *          DSM_S_NO_TRANSACTION - a transaction operation request
 *                                 was issued against prior to a
 *                                 start request.
 *          DSM_S_FAILURE - unsuccessful savepoint operation.
 */
dsmStatus_t 
tmMarkSavePoint(
	dsmContext_t	*pcontext,
        dsmTxnSave_t    *psavepoint) 
{
    usrctl_t    *pusr = pcontext->pusrctl;
    TX          *pTranEntry;
    SAVENOTE	savenote;

    /* Limit functionality to SQL engine */
    if (pcontext->pdbcontext->accessEnv != DSM_SQL_ENGINE)
        return(DSM_S_FAILURE);

    /* Validate that we are in a transaction for this user */
    if (pusr->uc_task == 0)
        return(DSM_S_NO_TRANSACTION);
 
    /* Validate that the requested savepoint is greater than zero
     * and is one greater than the last one set in the user's context,
     * unless this tmMarkSavePoint is being called within a delayed 
     * transaction begin (before the transactio begin note has been written).
     *  In this case, the only valid savepoint number is 2.
     */
    if (*psavepoint < DSMTXN_SAVE_MINIMUM)
        return(DSM_S_BADSAVE);

    pTranEntry = tmEntry(pcontext, pusr->uc_task);
    if ( pTranEntry->txstate == TM_TS_ALLOCATED )
    {
        if (*psavepoint != 2)
            return(DSM_S_BADSAVE);
    }
    else
        if (*psavepoint != (ULONG)pusr->uc_savePoint + 1)
            return(DSM_S_BADSAVE);

    /* Initialize the note */

    INITNOTE (savenote, RL_TMSAVE, RL_PHY | RL_LOGBI );
    savenote.usrnum    = pusr->uc_usrnum;
    savenote.xaction   = pusr->uc_task;
    savenote.recordtype = TMSAVE_TYPE_SAVE;
    savenote.savepoint  = *psavepoint;

    /* DO the note */
    rlLogAndDo (pcontext, (RLNOTE *)&savenote, 0, 0, PNULL);

    /* Store the save point in user control */
    pusr->uc_savePoint = *psavepoint;

    return((dsmStatus_t)DSM_S_SUCCESS);

}   /* end tmMarkSavePoint */


/* PROGRAM: tmMarkUnsavePoint - rollback to a specified savepoint
 *
 *
 * RETURNS: DSM_S_SUCCESS - successful
 *          Or never return because of FATAL
 *          DSM_S_FAILURE - unsuccessful savepoint operation.
 */
dsmStatus_t 
tmMarkUnsavePoint(
	dsmContext_t	*pcontext,
        LONG            noteBlock, 
        LONG            noteOffset) 
{
    SAVENOTE	savenote;

    /* Limit functionality to SQL engine */
    if (pcontext->pdbcontext->accessEnv != DSM_SQL_ENGINE)
        return(DSM_S_FAILURE);

    /* Validate that we are in a transaction for this user */
    if (pcontext->pusrctl->uc_task == 0)
        return(DSM_S_BADSAVE);

    /* Initialize the note */

    INITNOTE (savenote, RL_TMSAVE, RL_PHY | RL_LOGBI );
    savenote.usrnum = pcontext->pusrctl->uc_usrnum;
    savenote.xaction = pcontext->pusrctl->uc_task;
    savenote.recordtype = TMSAVE_TYPE_UNSAVE;
    savenote.savepoint = pcontext->pusrctl->uc_savePoint;
    slng((TEXT *)&savenote.noteblock, noteBlock);
    slng((TEXT *)&savenote.noteoffset, noteOffset);

    /* DO the note */
    rlLogAndDo (pcontext, (RLNOTE *)&savenote, 0, 0, PNULL);

    return((dsmStatus_t)DSM_S_SUCCESS);

}  /* end tmMarkUnsavePoint */
