
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

#include <stdlib.h>
#include "dbconfig.h"

#include "bkpub.h"   /* needed for bkClose call */
#include "bmpub.h"   /* needed for bkbuclr & bmPopLocks call */
#include "dbcode.h"
#include "dbcontext.h"
#include "dbpub.h"
#include "dbprv.h"
#include "drmsg.h"
#include "dsmpub.h"  /* for DSM API */
#include "latpub.h"
#include "lkpub.h"   /* needed for lkresync call */
#include "rlpub.h"   /* needed to satisfy TMNOTE in tmmgr.h */
#include "sempub.h"  /* needed for semDelete call */
#include "shmpub.h"
#include "stmprv.h"  /* pshmdsc */
#include "stmpub.h"  /* storage management subsystem */
#include "tmmgr.h"   /* needed for tmchkdelay call */
#include "usrctl.h"
#include "utospub.h"  /* for utOsCloseTemp() */
#include "utspub.h"

#include <setjmp.h>

#if OPSYS==UNIX
#include <unistd.h>
#endif

#if OPSYS == WIN32API
#include "dbmsg.h"
int unlink (const char *);
#endif

LOCALF DSMVOID misctmpc( dsmContext_t  *pcontext);


/* PROGRAM: dbExit - 
 * call users exit callback routine so they can cleanup/exit.
 * If they return to us, we'll dump core for them.
 * If the problem occurred below DSM, 
 *     we'll return to the caller via longjump
 * else exit(EXBAD)
 *
 * RETURNS: This function DOES NOT RETURN.
 */
dsmStatus_t
dbExit(
        dsmContext_t *pcontext,
        int           mode,       /* 0, DB_DUMP_EXIT, DB_NICE_EXIT */
        int           exitCode)   /* ret code to pass to exit 
                                   * Currently used values are: DB_EXGOOD,
                                   * DB_EXBAD, DB_EXNOSERV, DB_EXLIMBO
                                   */
{

    /* write stack trace for SQL users.  We expect that the 4GL
     * will perform its own stack tracing via drexit.
     */
    if ( (mode & DB_DUMP_EXIT) &&
         (pcontext->pdbcontext->accessEnv & DSM_SQL_ENGINE) )
    {
#if OPSYS==UNIX
        uttrace();
#elif OPSYS==WIN32API
        utcore();
#endif
    }
    if (pcontext->pdbcontext->usertype & BROKER)
    {
      dbenvout(pcontext,exitCode);
      return 0;
    }
    if (pcontext->exitCallback)
        pcontext->exitCallback(mode, exitCode);

    /* Since no exit processing specified by the caller 
     * (or we returned from their exit handler!)
     * we will disconnect the user then exit the dsm using longjump.
     */

    /* NOTE: need to avoid dbUserDisconnect calling dbExit! */
    if (pcontext && (exitCode != DB_EXBAD) )
    {
        dbUserDisconnect(pcontext, exitCode);
        pcontext->pusrctl = NULL;
    }

    /* NOTE: We don't ever expect to be here without having gone through
     *       dsmThreadSafeEntry() or without having the setjmp explicitly
     *       performed by a caller in our stack trace.
     */
    if (pcontext->savedEnv.jumpBufferValid)
        longjmp(pcontext->savedEnv.jumpBuffer, exitCode ? exitCode : DB_EXBAD);

    /* We don't expect the exit Callback or the long jump to return but 
     * just in case we will exit the process if we get this far.
     */
     exit(DB_EXBAD);

    return DSM_S_FAILURE;
 
}  /* end dbExit */


/* PROGRAM: dbenvout - undo everything done by dbenv
 *
 * RETURNS: 0 for success
 *          No failures
 */
dsmStatus_t
dbenvout(
        dsmContext_t    *pcontext,
        int      exitflags)     /* may have DB_NICE_EXIT, etc        */
{
        dbcontext_t     *pdbcontext = pcontext->pdbcontext; /* close down this db */
        usrctl_t        *pusr = pcontext->pusrctl; /* usrctl to be freed up */
        int             i;
        int             good;

    if (pdbcontext->dbimflgs & DBENVOUTED)
        return 0; /* prevent recursion */
    else
        pdbcontext->dbimflgs |= DBENVOUTED;

#ifdef HAVE_OLBACKUP
    if ((pdbcontext->pmtctl) &&
        (pusr && !pdbcontext->shmgone && (pdbcontext->usertype & BACKUP)) &&
        (pdbcontext->pdbpub->shutdn != DB_EXBAD))
    {
        /* we are an online backup shutting down unexpectedly */
        /* Make sure bk knows we are going away so all the users don't hang */

        bmBackupClear (pcontext);
        MSGN_CALLBACK(pcontext, drMSG119); /* backup terminated due to errors */
    }
#endif /* HAVE_OLBACKUP */

    /* close all the areas */
    if (exitflags != DB_EXBAD)
        bkCloseAreas(pcontext);

    if ((pusr && !pdbcontext->shmgone) &&
        (pdbcontext->argnoshm || pdbcontext->usertype & BROKER))
    {
        /* broker or only user so close the database */

        if (!pdbcontext->argronly) /* if NOT read only database */
            dbSetClose(pcontext, exitflags);
    }
    if (pdbcontext->argnoshm)
    {
        dbunlk(pcontext, pdbcontext); /* remove the <dbname>.lk or equivalent */
        bkClose(pcontext);      /* close files */
        if (!pdbcontext->argronly && pdbcontext->dbcode == PROCODE )
        {
            if (pdbcontext->dbopnmsg)
            {
                pdbcontext->dbopnmsg = 0;
                MSGN_CALLBACK(pcontext, drMSG013, pdbcontext->runmode);
            }
            dbLogClose(pcontext);  /*close log file */
        }
        return 0;
    }

  if (pdbcontext->pmtctl != NULL) 
  {
    if (pusr && !pdbcontext->shmgone)
    {
        /* remember if we were bad shutdown before these checks */

        good = (pdbcontext->pdbpub->shutdn != DB_EXBAD);

        i = latpoplocks (pcontext, pusr);
        if (good && i && pdbcontext->pdbpub->shutdn == DB_EXBAD)
        {
            /* User %i died holding %i shared memory locks    */
            FATAL_MSGN_CALLBACK(pcontext, drFTL012, pusr->uc_usrnum, i);
        }
        i = bmPopLocks(pcontext, pusr);
        if (good && i && pdbcontext->pdbpub->shutdn == DB_EXBAD)
        {
            /* User %i died with %i buffers locked            */
            FATAL_MSGN_CALLBACK(pcontext, drFTL013, pusr->uc_usrnum, i);
        }
    }
    if (pusr && !pdbcontext->shmgone &&
        !(pdbcontext->usertype & (BROKER + WDOG + APW + BIW + AIW + MONITOR))
        && pdbcontext->pdbpub->shutdn != DB_EXBAD)
    {
        if (!((pdbcontext->usertype & DBSHUT) && pdbcontext->argforce))
        {
            /* cancel all my waits, record locks, etc */

            pdbcontext->dboutflg = 1; 
            lkresync(pcontext);
        }
    }
    if (pusr && !pdbcontext->shmgone && pdbcontext->pdbpub->shutdn != DB_EXBAD)
    {
        /* free up my private buffers */

        bmfpbuf(pcontext, pusr);

        /* make sure we are not exiting holding any record locks */
        lkrend(pcontext, 1);
    }
  }  /* end pdbcontext->pmtctl != NULL */

    /* Print a logout message for the SELF-SERVICE user.
     * Since we are essentially erasing the usrctl, this is the
     * latest we can write to the .lg and maintain user info for the message.
     */
    if (pusr && pusr->uc_loginmsg)
    {
        pusr->uc_loginmsg = 0;
        if( pusr )
            MSGN_CALLBACK(pcontext, drMSG022,
                          XTEXT(pdbcontext,pusr->uc_qusrnam),
                          XTEXT(pdbcontext,pusr->uc_qttydev) );
    }
    if (pusr && !pdbcontext->shmgone)
    {
#if OPSYS==WIN32API
        /* we are almost disconnected so we need to 
           get rid of the proshut thread */
        if (pdbcontext->proshutThread)
        {
            GBOOL ret;
            ret = TerminateThread(pdbcontext->proshutThread,0);
            if (ret == FALSE)
            {
                DWORD error;
                error = GetLastError();
                /* Unable to terminate thread %X, errno %d */
                MSGN_CALLBACK(pcontext, dbMSG034,
                              pdbcontext->proshutThread, error);
            }
        }
#endif

        /* get rid of my usrctl */
        pcontext->pusrctl = NULL;  /* prevent infinite loops */
        pdbcontext->psvusrctl = NULL;  /* prevent infinite loops */
        if (pdbcontext->pmtctl != NULL)
            dbDelUsrctl (pcontext, pusr);
    }
#define brokergone 0

    if (!pdbcontext->shmgone)
    {
        pdbcontext->shmgone = 1;   /* prevent infinite loops */
        /* the broker must delete the shared memory and the semaphores */
        if (   pdbcontext->usertype & BROKER
            ||(pdbcontext->usertype & DBSHUT && brokergone))
        {
            semDelete(pcontext);
            /* for OS2, shmDelete must come after semDelete */
            shmDelete(pcontext);
        }
        else
        {   /* all other users must simply release the share memory and sem */
#if OPSYS==WIN32API
            semRelease(pcontext);
            /* for OS2, shmDetach must come after semRelease */
            shmDetach(pcontext);
#else
            shmDetach(pcontext);
            semRelease(pcontext);
#endif
        }
    }
    /* log shutdown */
    if (pdbcontext->dbopnmsg)
    {
        pdbcontext->dbopnmsg = 0;
        MSGN_CALLBACK(pcontext, drMSG013, pdbcontext->runmode);
    }
    /* whoever created <dbname>.lk must delete it */

    if (pdbcontext->usertype & BROKER)
        dbunlk(pcontext, pdbcontext); /* remove the <dbname>.lk or equivalent */

    bkClose(pcontext);  /* close all shared files */

    dbLogClose(pcontext);  /*close log file */

    return 0;

}  /* end dbenvout */


/* PROGRAM: dbunlk - unlock a database locked by dblk
 *
 * RETURNS: DSMVOID
 */
DSMVOID
dbunlk(
        dsmContext_t   *pcontext,
        dbcontext_t    *pdbcontext)        /* unlock this database */
{
        int      lockmode = pdbcontext->bklkmode;

    /* prevent infinite loops */
    if (lockmode == 0) return;
    if (pdbcontext->argronly) return;/* if NOT read only database */
    pdbcontext->bklkmode = 0;

    if (!pdbcontext->argronly) /* if NOT read only database */
        bkioRemoveLkFile(pcontext, pdbcontext->pdbname);

} /* end dbunlk */


/* PROGRAM: dbSetClose -- close down database processing in an orderly
 *              fashion. This includes closing out the before-image
 *              facility and shutting down the block manager, which
 *              closes the database file(s).
 *
 *              [Function previously existed as dbsetc in drdbsetc.c]
 *
 * RETURNS: DSMVOID
 */
DSMVOID
dbSetClose(
        dsmContext_t    *pcontext,
        int             exitflags)      /* may have DB_NICE_EXIT on */
{
    dbcontext_t  *pdbcontext = pcontext->pdbcontext; /* close down this db */

    /* close the before image manager and the logical db */
    if( pdbcontext->dbcode == PROCODE )
    {
        if (exitflags & DB_NICE_EXIT)
            rlsetc(pcontext);
        

    }
    else /* Progress temporary database */
        misctmpc(pcontext);

    /* release the file extend buffer */
    bkxtnfree(pcontext);

}  /* end dbSetClose */


/**************************************************************
 * PROGRAM: dbGetShmgone -  get the value if shmgone
 *
 */
LONG
dbGetShmgone(
        dbcontext_t   *pdbcontext)      /* pointer to the Entctl structure */
{
    if (pdbcontext && pdbcontext->pmtctl)
    {
        return (LONG)pdbcontext->shmgone;
    }
    else
    {
        return 1;
    }

}  /* end dbGetShmgone */

 
/* PROGRAM: dbGetShutdown - returns the value of the shutdn
 *                          value
 *
 *
 * RETURNS: int
 */
int
dbGetShutdown(
        dbcontext_t     *pdbcontext)
{
    if (!pdbcontext)
    {
        return 0;
    }
 
    if (!pdbcontext->pdbpub)
    {
        return 0;
    }
 
    return pdbcontext->pdbpub->shutdn;

}  /* end dbGetShutdown */


/**************************************************************
 * PROGRAM: dbGetInService - get the inservice value
 */
LONG
dbGetInService(dbcontext_t  *pdbcontext)
{
    if (pdbcontext)
    {
        return (LONG)pdbcontext->inservice;
    }
    else
    {
        return 0;
    }

}  /* end dbGetInService */
 

/**************************************************************
 * PROGRAM: dbGetUserctl - returns the value of the current user
 *
 * RETURNS: the userctl
 */
usrctl_t *
dbGetUserctl(
        dsmContext_t    *pcontext)
{
    if (!pcontext)
    {
        return 0;
    }

    return pcontext->pusrctl;

}  /* end dbGetUserctl */
 

/* PROGRAM: misctmpc - Procedure to close and unlink temp database
 *
 *              [Function previously existed as dbtmpc in drdbsetc.c]
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
misctmpc(
        dsmContext_t    *pcontext)
{
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    TEXT tempDbFileName[MAXUTFLEN];
   /* TODO: This code can go away after temp table work
    * is done.  Today we support temp tables w/ a temp
    * database, but this will probably change before v9
    * is shipped.
    */

    bkioClose(pcontext, pdbcontext->pbkfdtbl->pbkiotbl[pdbcontext->bklkfd], 0);
 
    /* NOTE: bkioClose SHOULD delete the file at close if it was a temp file. */
    /* 98-05-05-061 use of -T meant DBI file was not being deleted for
       WIN32 platforms.                                                    */
    stcopy(tempDbFileName,pdbcontext->tempdir);
    stcopy(&tempDbFileName[stlen(pdbcontext->tempdir)],pdbcontext->pdbname);
    unlink((char *)tempDbFileName);

}  /* end misctmpc */



