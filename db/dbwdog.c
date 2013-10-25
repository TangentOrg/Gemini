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
#include "dbconfig.h"
#include "dbcontext.h"
#include "dblk.h"
#include "dbpub.h" 
#include "dbutret.h"
#include "bkmsg.h"
#include "drmsg.h"
#include "drsig.h"
#include "dsmpub.h"
#include "errno.h"

#include "latpub.h"
#include "mstrblk.h"

#include "sempub.h"
#include "svmsg.h"
#include "usrctl.h"
#include "utepub.h"
#include "utfpub.h"
#include "utmpub.h"
#include "utmsg.h"
#include "utspub.h"
#include "uttpub.h"
#include "bkiopub.h"

#include <sys/stat.h>
#include <signal.h>

#define SHUTSIG -1    /* use this to tell brsigall to use the default
                         shutdown signals.  SELFSERVICE = SIGEMT
                                            REMOTE = SIGTRAP  */

typedef struct shutSigsStruct
{
        COUNT   delayTime;
        int     sigToSend;
} shutSigsStruct_t;
 

LOCAL shutSigsStruct_t shutSigs[] =
{
#if OPSYS!=WIN32API
    {30, SIGALRM},
    {60, SIGINT},
#if LINUX
    {90, SVSIGC},
#else
    {90, SIGEMT},
#endif
    {300, SIGALRM},
    {330, SIGINT},
    {360, SIGTERM},
#endif
    {390, 0}
};


/**** LOCAL FUNCTION PROTOTYPES FOR WATCHDOG ****/

LOCALF DSMVOID brclrdead (dsmContext_t *pcontext, usrctl_t *pusr,
                       psc_user_number_t svnum);

LOCALF DSMVOID brsigsome (dsmContext_t *pcontext);

LOCALF int brsiguser (dsmContext_t *pcontext, usrctl_t *pusr, int pid,
                      int sig);

LOCALF int brcheckuser (dsmContext_t *pcontext, usrctl_t *pusr);

LOCALF TEXT *brpid2a (int pid, TEXT *pidstr);

LOCALF DSMVOID brsigall (dsmContext_t *pcontext, int sig);

LOCALF DSMVOID watchShut(dsmContext_t *pcontext);

LOCALF DSMVOID wdogLkFileCheck(dsmContext_t *pcontext);

LOCALF DSMVOID brclrlocks (dsmContext_t *pcontext, usrctl_t *pusr);

LOCALF DSMVOID brclrlats (dsmContext_t *pcontext, usrctl_t *pusr);

/**** END  LOCAL FUNCTION PROTOTYPES FOR WATCHDOG ****/


/* PROGRAM: dbWdogactive - check for active watchdog
 *
 * NOTE: dbWdogActive() was formally wdogactive()
 * 
 * RETURNS:
 *              1 if a watchdog process is alive
 *              0 if no watchdog or watchdog is dead
 */
int
dbWdogActive(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm     = pdbcontext->pdbpub;

    psc_user_number_t i;
    usrctl_t    *pusr;
    int          active = 0;
 
    for (pusr = pdbcontext->p1usrctl, i = 0;
         i < pdbshm->argnusr;
         i++, pusr++)
    {
        if (pusr->usrinuse && (pusr->uc_usrtyp & WDOG))
        {
            /* it is a watchdog */
            if (pusr != pcontext->pusrctl)
            {
                /* It is not ourselves */
                if (!deadpid (pcontext, pusr->uc_pid))
                {
                    /* it is alive */
                    active = 1;
                }
            }
        }
    }

    return (active);

}  /* end dbWdogActive */



/* PROGRAM: dbWatchdog - check for dead users
 *
 * NOTE: dbWatchdog() was formally watchdog()
 *
 * RETURNS: 0 success
 *          1 another watchdog is running
 *         -1 abnormal shutdown initiated because broker died
 *         -2 shutdown == DB_EXBAD
 */
int
dbWatchdog(dsmContext_t *pcontext)
{
        dbcontext_t     *pdbcontext = pcontext->pdbcontext;
        dbshm_t         *pdbshm = pdbcontext->pdbpub;
        psc_user_number_t i;
        usrctl_t        *pusr;
        int              ret;
        LONG             currTime;

    /* the following value defines how often we check if another watchdog
     * is running.
     */
    LOCAL COUNT dogstate = 0;

    /* The following is the list of all possible users.
     * It is allocated once for the process and then 
     * re-used on subsequent calls.  It is protected by the 
     * checks that only one watchdog can be running at a time.
     */
    LOCAL psc_user_number_t *pdeaduser = 0;
 
    /* we are the watchdog. make sure we are the only one */
    if (dbWdogActive (pcontext))
        return(1);

    /* we are the watchdog. check that the broker is alive */
    if (deadpid (pcontext, pdbshm->brokerpid))
    {
        /* The broker is dead.  Perform abnormal shutdown. */

        /* first create a new .lk file */
        MSGN_CALLBACK(pcontext, bkMSG104, pdbcontext->pdbname);
        ret = bkioUpdateLkFile (pcontext, 0, utgetpid(),
                               (TEXT *)0, pdbcontext->pdbname);

        /* Initiate abnormal shutdown by the watchdog
         * watchShut is not expected to return
         */
         watchShut(pcontext);
         return (-1);
     }

    /* We are now running as the watchdog. */
    
    /* Check for dead users */
    if (!pdeaduser)
    {   psc_user_number_t maxusrs = shmGetMaxUSRCTL();
 
        pdeaduser = (psc_user_number_t *)utmalloc(
                           sizeof(psc_user_number_t) * maxusrs);
        for (i = 0; i < maxusrs; i++)
        {
            pdeaduser[i] = 0;
        }
    }

    /* make sure signals don't bother us right now */
    pdbcontext->inservice++;
 
    /* verify that we have a .lk file and that it's valid */
    wdogLkFileCheck(pcontext);
 

    /* Now look for dead self service clients, etc. Ignore remote clients
     * and servers, which were handled above 
     */
    time(&currTime);
    for (pusr = pdbcontext->p1usrctl, i = 0;
         i < pdbshm->argnusr;
         i++, pusr++)
    {
        if (pdbshm->shutdn)
        {
            /* clear all pending record locks by timing them out
             * and awakening the waiting user.
             */
            for (pusr = pdbcontext->p1usrctl, i = 0;
                 i < pdbshm->argnusr;
                 i++, pusr++)
            {
                if ( (pusr->usrinuse) && (pusr->uc_endTime) )
                {
                    pusr->uc_endTime = 0;
                    latLockTimeout(pcontext, pusr, currTime);
                }
            }
            break;
        }
 
        if ((pusr->usrinuse) && (pusr->uc_pid != 0))
        {
            /* Perform timeout processing */
            if (pusr->uc_endTime)
                latLockTimeout(pcontext, pusr, currTime);

            /* Perform dead user processing */

            /* This slot in use. Skip servers and remote clients */
            if (pusr->uc_usrtyp & (DBSERVER | REMOTECLIENT))
                continue;
 
            if (deadpid (pcontext, pusr->uc_pid))
            {
                /* process is dead */
                if (pdeaduser[i] == 3 )
                {
                    /* On this pass just clear lats and buffer locks
                       otherwise we could hang trying to back out the
                       dead users locks if a different dead user died
                       holding lats.                                  */
                    /* latches should have been freed by now,
                       but we have to check just to be sure */
                     brclrlats (pcontext, pusr);
                     /* clear other shared memory resource locks */
                     brclrlocks (pcontext, pusr);
                 }
                else if (pdeaduser[i] > 3)
                {
                    /* Its been dead a while. */
                    brclrdead (pcontext, pusr, 0);
                    pdeaduser[i] = 0;
                }
                pdeaduser[i]++;
                continue;
            }
            if ((pdbcontext->usertype & BROKER) && (pusr->uc_usrtyp & WDOG))
            {
                /* it is a watchdog which is alive so the broker can
                 * stop doing this */
                dogstate = 0;
                goto donechecking;
            }
        }
        /* user slot is not in use or the process is alive */
        pdeaduser[i] = 0;
    }
donechecking:
 
    if (pdbshm->shutdn == DB_EXBAD)
    {
        /* before we go, wake everybody */
        brsigall(pcontext, SHUTSIG);
        ret = -2;
    }
    else
        ret = 0;
 
    /* ok to have signals now */
    pdbcontext->inservice--;
 
    return (ret);

}  /* end dbWatchdog */


/* PROGRAM: dbKillUsers - broker finds out all user to kill
 *	if it is a self-server signal it to die 
 *	if it is a remote user mark its server attention
 *	and signal its server 
 *
 * NOTE: dbKillUsers() was formally brokkill()
 *
 * RETURNS:
 */
DSMVOID
dbKillUsers(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm     = pdbcontext->pdbpub;

    psc_user_number_t	i;
    int          liveuser;
    int	         loopcount;
    usrctl_t	*pusr;
    int          j=0;
    int          sig;
    TEXT         pidstr[20];

    /* check if I was called too early during initialization */
    /* or recursively called during shutdown		     */

    if (!pdbcontext->p1usrctl)
        return;

    if (pdbcontext->brkklflg == 0)
        return;	/* turned on when dbopen finishes*/

    pdbcontext->brkklflg = 0;    /* also turned on at end of dbKillUsers */

    if (pdbshm->shutdn == 0)
    {
	/* find the one(s) to disconnect and signal them */

        brsigsome (pcontext);
        pdbcontext->brkklflg = 1;
    }
    else
    {
	/* log shutdown message */

        if (pdbshm->shutdn == 1)
            MSGN_CALLBACK(pcontext, utMSG083);
        else
            MSGN_CALLBACK(pcontext, utMSG084, pdbshm->shutdn);

	/* notify everyone */
	brsigall(pcontext, SHUTSIG);

        /* wait for everyone to go away */
        loopcount = 0;
	for (;;) /* loop for ever until all users die */
	{
	    utsleep (1);

            /* set the signal we will send to ping the processes */
            sig = shutSigs[j].sigToSend;

	    liveuser = 0;
	    i = 0;
            for (pusr = pdbcontext->p1usrctl;
                 i < pdbshm->argnusr; i++, pusr++)
	    {
	        if (!pusr->usrinuse) continue;

		if (pusr->uc_usrnum == 0) continue;

		/* skip remote clients because we are interested only
		   in processes on the same machine as us */

		if (pusr->uc_usrtyp & REMOTECLIENT) continue;

		/* proshut is skipped because it is waiting for broker
		   process to exit before it exits. */

		if (pusr->uc_usrtyp == DBSHUT) continue;
		if (pusr->uc_usrtyp == (DBSHUT | BATCHUSER)) continue;

		if (pusr->uc_usrtyp & MONITOR) continue;

		/* brcheckuser will skip our own pid */
                if (brcheckuser(pcontext, pusr))
                {
                    /* process is still there */
                    liveuser++;

                    /* if the process is still alive and we have
                       given it enough time, then check to see if
                       it has done any database work */
                    if ((sig == 0) && (loopcount % 30 == 0))
                    {
                        /* this is our first time thru */
                        if (pusr->uc_shutloop == 0)
                        {
                            if( pusr->uc_usrtyp & APW )
                            {
                                /* We let apw's keep going until the
                                   buffer pool has been flushed.  */
                                pusr->uc_lastBlock =
                                    pdbcontext->pbkctl->bk_numdirty;
                            }
                            else
                            {
                                pusr->uc_prevlastblk = pusr->uc_lastBlock;
                                pusr->uc_prevlastoff = pusr->uc_lastOffset;
                            }
                            pusr->uc_shutloop = 1;
                            continue;
                        }
                        
                        if (pusr->uc_usrtyp & APW)
                        {
                            if(pusr->uc_lastBlock ==
                               pdbcontext->pbkctl->bk_numdirty)
                                pusr->uc_shutloop++;
                            else
                                pusr->uc_shutloop = 0;
                        }
                        else
                        {
                            /* If we have not written any notes to the
                               bi file since the last check, then increment
                               the loop counter */
                            if ( (pusr->uc_prevlastblk ==
                                  pusr->uc_lastBlock) &&
                                 (pusr->uc_prevlastoff ==
                                  pusr->uc_lastOffset) )
                            {
                                pusr->uc_shutloop++;
                            }
                            else
                            {
                                /* some work was done, start over */
                                pusr->uc_shutloop = 0;
                            }
                        }
                        
                        if (pusr->uc_shutloop == 3)
                        {
                            /* This process is hung.  Smash him! */
                            MSGN_CALLBACK(pcontext, utMSG086,
                                          i, brpid2a(pusr->uc_pid, pidstr));
                            brdestroy(pusr->uc_pid);
                            latpoplocks(pcontext, pusr);
                        }

                        if (pusr->uc_shutloop > 3)
                        {
                            /* Sending a kill -9 didn't help.  Start an
                               emergency shutdown */
                            dbKillAll(pcontext, 1);
                        }
                    }
                }
	    }
	    if (liveuser == 0) break;
	    loopcount++;
#if OPSYS==UNIX
            sig = shutSigs[j].sigToSend;

            /* if we still have a valid signal to send */

            if (sig)
            {
                /* if the right amount of time has passed */

	        if ((loopcount % shutSigs[j].delayTime) == 0)
	        {
                    /* display the correct message to the .lg file */
#if LINUX
                    if (sig == SVSIGC) 
#else
                    if (sig == SIGEMT) 
#endif
                    {
                        /* Resending shutdown request. */
                        MSGN_CALLBACK(pcontext, utMSG098, liveuser);
                    } 
                    else
                    {
                        /* Sending signal %d to connected users */
                        MSGN_CALLBACK(pcontext, utMSG096, sig, liveuser);
                    }

                    /* send the signal */
                    brsigall(pcontext, sig);

                    /* go on to the next entry */
                    j++;
	        }
	    }
#else
	    if ((loopcount % 300) == 0)
	    {
		/* It has been 5 minutes. Try signalling remaining
		   users again */
		/*  Resending shutdown request. */
		MSGN_CALLBACK(pcontext, utMSG098, liveuser);
		brsigall(pcontext, SHUTSIG);
	    }
#endif
        }
    }

}  /* end dbKillUsers */


/* PROGRAM: dbKillAll - destroy all processes connected to the database
 *                      delete semaphores
 *                      delete shared memory
 *                      calls drexit when done
 *
 * NOTE: dbKillAll() was formally brkillall()
 */
DSMVOID
dbKillAll (
    dsmContext_t *pcontext,
    int          logthem)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    int          pid;
    int          mypid;
    int          brokpid;
    psc_user_number_t               i;
    usrctl_t    *pusr;
    TEXT         pidstr[20];
    int		 n;
 
    mypid = utgetpid();
 
    /* make sure signals dont bother us right now */
 
    pdbcontext->inservice++;
 
    if (pdbshm->brokertype != JUNIPER_BROKER)
    {
        /* Become owner of the .lk file so that when the owner
         * notices pmt->shutdn = EXBAD set it doesn't remove it
         * since shutdown isn't complete and we don't want anyone
         * connecting until shutdown is complete.  Also, set the
         * brokerpid immediately so the WDOG .lk file error checking
         * doesn't attempt to shut down the database before we do.
         * 95-03-29-042
         */
        bkioUpdateLkFile(pcontext, 0, mypid, PNULL, pdbcontext->pdbname);
        pdbshm->brokerpid = mypid;
    }
    else
    {
        /* inform juniper that we are beginning shutdown */
        pdbshm->shutdnStatus = SHUTDOWN_INITIATED;
        pdbshm->shutdnInfo = PROSHUT_FORCE;
    }
 
    pusr = pdbcontext->p1usrctl;
    brokpid = pusr->uc_pid;
 
    /* If this is called becuase normal shutdown has hung, then
       the broker is calling this code, so don't kill ourself */
    if ((brokpid != mypid) && (pdbshm->brokertype != JUNIPER_BROKER))
    {
        /* Kill broker so he won't interfere */
        brdestroy(brokpid);
    }
 
   /* set EXBAD AFTER killing the broker so that the broker
    * won't try and perform shutdown only to be killed by brdestroy.
    */
    pdbshm->shutdn = DB_EXBAD;
 
    /* pretend we are the broker so servers don't freak */
 
    pusr->uc_pid = mypid;
    pcontext->pusrctl->uc_usrtyp |= BROKER;
    pdbcontext->usertype |= BROKER;
 
    /* poke everyone to give them a chance to go away on their own */
 
    brsigall(pcontext, SHUTSIG);
 
    /* wait 10 sec. to allow anybody who wants to to exit */
 
    utsleep (10);
 
    for (pusr = pdbcontext->p1usrctl, i = 0;
         i < pdbshm->argnusr;
         i++, pusr++)
    {
        /* skip unused slots */
        if (!pusr->usrinuse) continue;
 
        /* Skip remote clients */
        if (pusr->uc_usrtyp & REMOTECLIENT) continue;
 
        pid = pusr->uc_pid;
 
        if (!pusr->usrinuse) continue;
 
        /* Don't kill ourself */
        if (pid == mypid) continue;
 
        /* Skip broker */
        if (pid == brokpid) continue;
 
        if (logthem)
            MSGN_CALLBACK(pcontext, utMSG086, i, brpid2a (pid, pidstr));
 
        /* destroy the process if os lets us. Dont care about errors */
        brdestroy (pid);
    }
    /* wait for the dust to settle */
    utsleep (5);
 
    if (pdbshm->brokertype == JUNIPER_BROKER)
    {
        /* notify juniper that all users are gone */
        pdbshm->shutdnStatus = SHUTDOWN_BROKER_WAIT;

        /* check to make sure that juniper has disconnected,
           then remove shared memory and semaphores and
           then leave. */
        n = 0;
        if (deadpid(pcontext, pdbshm->brokerpid))
        while (1)
        {
            if (deadpid(pcontext, pdbshm->brokerpid))
            {
                /* juniper is gone, time to leave */
                break;
            }
 
            utsleep(1);
            if (n++ > JUNIPER_TIMEOUT)
            {
                /* took too long */
                brdestroy(pdbshm->brokerpid);
                break;
            }
        }
    }

    /* get rid of shared memory and semaphores */
    pdbcontext->shmgone = 1;
 
    /* Allow proshut to delete the .lk file 93-01-22-035 */
    pdbcontext->bklkmode = DBLKMULT;
    dbunlk(pcontext, pdbcontext);
 
    /* Multi-user session end */
    MSGN_CALLBACK(pcontext, drMSG013, "Multi-user");

    /* FIXFIX: should be in a subroutine in mtshm */
 
    /* get rid of shared memory and semaphores */
    pdbcontext->shmgone = 1;
 
    semDelete (pcontext);
    shmDelete (pcontext);
 
    /* exit, do not return */
 
    /* mark that we removed shared memory */
    pdbcontext->argnoshm = 1;
 
    dbExit(pcontext, 0, DB_EXBAD);

}  /* end dbKillAll */


/* PROGRAM: brclrlats - Free shared memory latches held by a dead process
 *
 * RETURNS:
 */
LOCALF DSMVOID
brclrlats (
        dsmContext_t *pcontext,
        usrctl_t     *pusr)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm     = pdbcontext->pdbpub;    

    int     n;
    int     domsg = 1;
 
    if (pdbshm->shutdn == DB_EXBAD)
        domsg = 0;
 
    /* release his latches */
    n = latpoplocks(pcontext, pusr);
 
    /* Make sure we get our own usrctl back. Errors sometimes leave it set to
       the dead usrctl */
    pcontext->pusrctl = pdbcontext->psvusrctl;

    if (n && domsg)
    {
        /* %LUser %i died holding %i shared memory locks. */
        MSGN_CALLBACK(pcontext, drMSG172, pusr->uc_usrnum, n);
    }

}  /* end brclrlats */


/* PROGRAM: brclrlocks - Free shared memory resources held by a dead process 
 *
 * RETURNS:
 */
LOCALF DSMVOID
brclrlocks (
        dsmContext_t *pcontext,
        usrctl_t     *pusr)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm     = pdbcontext->pdbpub;
    int          n;
    int          domsg = 1;
 
 
    if (pdbshm->shutdn == DB_EXBAD)
        domsg = 0;
    else if (pusr->uc_txelk)
    {
        pdbshm->shutdn = DB_EXBAD;
        domsg = 0;

        /* %LUser %i died during microtransaction */
        MSGN_CALLBACK(pcontext, utMSG090, pusr->uc_usrnum);
    }

    /* release his buffer locks */
    n = bmPopLocks(pcontext, pusr);
 
    /* Make sure we get our own usrctl back. Errors sometimes leave it set to
       the dead usrctl */
 
    pcontext->pusrctl = pdbcontext->psvusrctl;
 
    if (n && domsg && pdbshm->shutdn == DB_EXBAD)
    {
        /* %LUser %i died with %i buffers locked */
        MSGN_CALLBACK(pcontext, drMSG173, pusr->uc_usrnum, n);
    }

}  /* end brclrlocks */

/* PROGRAM: brsigsome -
 *
 * RETURNS:
 */
LOCALF DSMVOID
brsigsome (dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    psc_user_number_t i;
    int          pid;
    usrctl_t    *pusr;
 
    /* loop over all usser slots and wake those that are flagged for
       disconnecting */
 
    for (pusr = pdbcontext->p1usrctl, i = 0;
         i < pdbshm->argnusr;
         i++, pusr++)
    {
        if (!pusr->usrinuse) continue;
 
        MT_LOCK_USR ();
 
        if ((!pusr->usrinuse) || (!pusr->usrtodie))
        {
            MT_UNLK_USR ();
            continue;
        }
        pid = pusr->uc_pid;

        MT_UNLK_USR ();
 
        /* signal the user process */
        if (brsiguser (pcontext, pusr, pid, SHUTSIG))
        {
            /* process is gone, clean up after it */
            brclrdead(pcontext, pusr, -1);
        }
    }

}  /* end brsigsome */


/* PROGRAM: brclrdead - clear resources held by dead user
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
brclrdead (
        dsmContext_t      *pcontext,
        usrctl_t          *pusr,
        psc_user_number_t svnum)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        dbshm_t     *pdbshm = pdbcontext->pdbpub;
        int          delusr = 1;
        int          resusr = 1;

    if (pusr->usrinuse == 0) return;
    if (pusr->uc_pid == 0) return;
 
    if (pdbshm->shutdn == DB_EXBAD)
    {
        /* Just mark it free. We do this to avoid getting hung on the
           latch for the usrctl. This is a bad shutdown anyway, so
           we don't need to be kosher. */
 
        pusr->usrinuse = 0;
        pusr->uc_pid = 0;
    }
 
    /* This user is now us. So the pid is valid for now */
    pusr->uc_pid = pdbcontext->psvusrctl->uc_pid;
 
    if (pusr->uc_2phase & (TPLIMBO | TPRCOMM))
    {
        /* user is in a ready to commit stage of a 2 phase transaction. We
           can't normally resync it or release its locks. */
 
        if ((pusr->uc_2phase & TPFORCE) == 0)
        {
            /* We only disconnect user if a forced disconnect has been
               requested. */
            delusr = 0;
            resusr = 0;
        }
        if ((pusr->uc_2phase & TPLIMBO) == 0)
        {
            /* haven't reported this yet */
            pusr->uc_2phase |= TPLIMBO;
 
            /*User %i died during 2 phase commit*/
            MSGN_CALLBACK(pcontext, utMSG082, pusr->uc_usrnum);
        }
    }
    if (resusr)
    {
        if (pusr->uc_usrtyp & DBSERVER)
        {
            /* Disconnecting dead server %i */
            MSGN_CALLBACK(pcontext, drMSG175, svnum);
        }
        else if ((pusr->uc_usrtyp & REMOTECLIENT) && (svnum != -1))
        {
            /* Disconnecting client %i of dead server %i */
            MSGN_CALLBACK(pcontext, drMSG176, pusr->uc_usrnum, svnum);
        }
        else
        {
            /* Disconnecting dead user %i */
            MSGN_CALLBACK(pcontext, drMSG177, pusr->uc_usrnum);
        }
    }
    /* latches should have been freed by now, but we have to check
       just to be sure */
 
    brclrlats (pcontext, pusr);
 
    /* clear other shared memory resource locks */
    brclrlocks (pcontext, pusr);
 
    if (pdbshm->shutdn == DB_EXBAD)
        resusr = 0;
 
    if (resusr && (pusr->uc_usrtyp & (SELFSERVE | REMOTECLIENT)))
    {
        /* back out transaction, free locks */
 
         /* Check for an active task and cause it to resync - 95-08-16-005 */
        if (pusr->uc_task)
            pusr->resyncing = 0;
 
        /* TODO: Here is a case where we need the user's context, not ours! */
        pcontext->pusrctl = pusr;

        if (dsmUserReset (pcontext))
        {
            /* could not resync, so we can't free the slot */
            delusr = 0;
        }
        pcontext->pusrctl = pdbcontext->psvusrctl;
    }

    if (pdbshm->shutdn == 1)
    {
        delusr = 1;
    }
 
    if (delusr)
    {
        /* free up the usrctl slot */
        dbDelUsrctl(pcontext, pusr);
    }
    else
    {
        pusr->uc_pid = 0;
    }
    pcontext->pusrctl = pdbcontext->psvusrctl;

} /* end brclrdead */


/* PROGRAM: brsiguser - signal a user or its server
 *
 * RETURNS:
 *          1: process is gone
 *          0: process exists
 */
LOCALF int
brsiguser (
        dsmContext_t *pcontext,
        usrctl_t     *pusr,
        int          pid,
        int          sig)   /* used)for UNIX only */
{
#if OPSYS==UNIX
        int     SigToSend;
#endif
 
#if OPSYS==UNIX
    if (sig < 0)
        SigToSend = (pusr->uc_usrtyp & REMOTECLIENT) ? SIGTRAP : SVSIGC;
    else
        SigToSend = sig;
    if (kill (pid, SigToSend) == -1)
    {
        if (errno == ESRCH)
            return (1);
    }
#endif
 
    if (pusr->uc_usrtyp & BIW)
    {
        /* dequeue bi writer */
        rlwakbiw(pcontext);
    }
    else if (pusr->uc_usrtyp & AIW)
    {
        /* dequeue ai writer */
        rlwakaiw(pcontext);
    }

    return (0);

}  /* end brsiguser */


/* PROGRAM: brcheckuser - check if a user process exists.
 *          If it does not exist, clean up after it.
 * RETURNS:
 *          0 - dead and was cleaned up or is us
 *          1 - exists
 */
LOCALF int
brcheckuser (
        dsmContext_t *pcontext,
        usrctl_t     *pusr)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
 
    /* the latlatch performs sanity checks for pusrctl,
     * NOT pusr.
     */
    MT_LOCK_USR ();
    if ((!pusr->usrinuse) || (pusr->uc_pid == 0))
    {
        /* slot freed, user gone */
        MT_UNLK_USR ();
        return (0);
    }
    if (pusr->uc_pid == (int)utgetpid())
    {
        /* he has the same pid as us so he doesn't count.
           This can happen if we start to disconnect somebody but we can't
           clear his usrctl for some reason. */
 
        MT_UNLK_USR ();
        return (0);
    }
    if (! deadpid (pcontext, pusr->uc_pid))
    {
        /* still exists */
        MT_UNLK_USR ();
        return (1);
    }
    MT_UNLK_USR ();
 
    /* the user (process) is dead. */
    brclrdead (pcontext, pusr, -1);
 
    pcontext->pusrctl = pdbcontext->psvusrctl;
 
    return (0);

}  /* end brcheckuser */

 
/* PROGRAM: brpid2a - get the character representation of a process id
 *
 * RETURNS:
 */
LOCALF TEXT *
brpid2a (
        int pid,
        TEXT *pidstr)
{
        /* Convert process id number to ascii string in system
           dependent form. Note that it is 10 hex characters on vms */
 
    sprintf ((char *)pidstr, "%d", pid);

    return ((TEXT *)pidstr);

}  /* end brpid2a */


/* PROGRAM: brdestroy - destroy a process
 *
 * RETURNS:
 */
DSMVOID
brdestroy (int pid)
{
#if OPSYS==WIN32API
        HANDLE  processHandle;
#endif
 
    if (pid == 0) return;
    if (pid == 1) return;
 
    /* We do not care if this fails because the process no longer exists
    */
#if OPSYS==UNIX
 
    kill (pid, SIGKILL);
 
#else
#if OPSYS==WIN32API
 
    /* get a handle to the process */
    processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
 
    /* only kill the process if the openProcess succeeds. */
    if (processHandle != (HANDLE *)NULL)
    {
        /* send a kill -9 */
        TerminateProcess(processHandle, DB_EXBAD);
 
        /* close the handle */
        CloseHandle(processHandle);
    }
 
#else
    GRONK; How do I send a signal on this machine???
#endif  /* WIN32API */
#endif  /* UNIX */

}  /* end brdestroy */


/* PROGRAM: brsigall - signal all processes connected to the database
 *                      so they can bail out. We hope any process
 *                      which is waiting on a semaphore or trying to get
 *                      a latch will notice the shutdn flag and exit.
 *
 * RETURNS:
 */
LOCALF DSMVOID
brsigall (
        dsmContext_t *pcontext,
        int           sig)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    int          pid;
    int          mypid;
    psc_user_number_t i;
    usrctl_t    *pusr;
 
    mypid = utgetpid();
 
    for (pusr = pdbcontext->p1usrctl, i = 0;
         i < pdbshm->argnusr;
         i++, pusr++)
    {
        /* skip unused slots */
 
        if (!pusr->usrinuse) continue;
 
        /* Skip remote clients */
 
        if (pusr->uc_usrtyp & REMOTECLIENT)
        {
            pusr->usrtodie = 1;
            continue;
        }
 
        if (pusr->uc_usrtyp == DBSHUT) continue;
        if (pusr->uc_usrtyp == (DBSHUT | BATCHUSER)) continue;
 
        pid = pusr->uc_pid;
 
        if (!pusr->usrinuse) continue;
 
        if (pid == 0) continue;
 
        /* Don't wake ourself */
        if (pid == mypid) continue;
 
        /* wake the process if os lets us. Dont care about errors */
        brsiguser (pcontext, pusr, pid, sig);
    }

}  /* end brsigall */



/* PROGRAM: watchShut - will perform abnormal shutdown of the database.
 *
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
watchShut(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
 
    /* set the shutdown bit */
    pdbshm->shutdn = DB_EXBAD;
 
    /* make us the broker so we can complete the shutdown */
    pdbcontext->pusrctl->uc_usrtyp |= BROKER;
    pcontext->pusrctl->uc_usrtyp |= BROKER;
    pdbcontext->usertype |= BROKER;
 
    /* shutdown the database */
    dbKillUsers(pcontext);
 
    /* Multi-user session end */
    MSGN_CALLBACK(pcontext, drMSG013, "Multi-user");
 
    dsmUserDisconnect(pcontext, 0);
 
    /* Allow proshut to delete the .lk file 93-01-22-035 */
    pdbcontext->bklkmode = DBLKMULT;
    dbunlk(pcontext, pdbcontext);
 
    /* time to leave */
    dbExit(pcontext, 0, DB_EXBAD);

}  /* end watchShut */


/* PROGRAM: wdogLkFileCheck -
 *
 */
/* PROGRAM: wdogLkFileCheck - check to see if the .lk file exists
 *                            if it doean't set shutdn to EXBAD.
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
wdogLkFileCheck(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm     = pdbcontext->pdbpub;

       int     ret;            /* return from stat */
struct stat   *statbuf, buf;   /* buffer for stat */
       TEXT    namebuf[MAXPATHN+1];
       LONG     retMakePath=0;    /* return for utMakePathname */
 
    /* get the absolute path of the .lk file */
    if(pdbcontext->pdatadir)
        utmypath(namebuf,pdbcontext->pdatadir,pdbcontext->pdbname,
                (TEXT *)".lk");
    else 
        retMakePath = utMakePathname(namebuf, sizeof(namebuf),
                                    pdbcontext->pdbname, (TEXT *)".lk");
    if (retMakePath == DBUT_S_NAME_TOO_LONG)
    {
        /* Attempt to make a too long file-name. */
        MSGN_CALLBACK(pcontext, utFTL003);
    }
 
    statbuf = &buf;
 
    /* stat the file */
    ret = stat((char *)namebuf, statbuf);
 
    if ((ret == -1 && errno == ENOENT))
    {
        MSGN_CALLBACK(pcontext, bkMSG105, namebuf);
 
        /* if the file doesn't exist, then we MUST shutdown right away
           to protect from possible database corruption.  Another process
           could open the database while the server is running
        */
        if (deadpid (pcontext, pdbshm->brokerpid))
        {
 
            /* first create a new .lk file */
            MSGN_CALLBACK(pcontext, bkMSG104, pdbcontext->pdbname);
            ret = bkioUpdateLkFile (pcontext, 0, utgetpid(), (TEXT *)0,
                                    pdbcontext->pdbname);
 
            /* Initiate abnormal shutdown by the watchdog */
             watchShut(pcontext);
        }
        else
        {
            /* let the broker perform the shutdown */
            pdbshm->shutdn = DB_EXBAD;
        }
    }
    else
    {
#if 0
        /* if the .lk file exists, then lets verify that it's ours */
        ret = bkioVerifyLkFile(pcontext, pdbshm->brokerpid,
                               pdbcontext->pdbname);
#else
        ret = 0;
#endif 
        if (ret)
        {
            /* lk failed verification, time to shutdown */
            MSGN_CALLBACK(pcontext, bkMSG106, namebuf);
 
            if (deadpid (pcontext, pdbshm->brokerpid))
            {
                 /* Initiate abnormal shutdown by the watchdog */
                 watchShut(pcontext);
            }
            else
            {
                /* let the broker perform the shutdown */
                pdbshm->shutdn = DB_EXBAD;
            }
        }
    }
 
} /* end wdogLkFileCheck */


