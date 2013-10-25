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
#include "bmpub.h"
#include "dbpub.h"
#include "uttpub.h"
#include "drmsg.h"

#include "utospub.h"
#include "mstrblk.h"
#include "rlprv.h"
#include "rlai.h"
#include "tmmgr.h"
#include "usrctl.h"
#include "utspub.h"
#include "lkpub.h"

#include <setjmp.h>
#include <time.h>

#if OPSYS==WIN32API
extern int fWin95;
#endif

/* PROGRAM: dsmShutdown -- 
 * 
 * RETURNS: 
 */
dsmStatus_t
dsmShutdown(
        dsmContext_t *pcontext,
        int           exitFlags,
        int           exitCode)  
{
   
    dbExit(pcontext, exitFlags, exitCode);

    return DSM_S_SUCCESS;

}  /* end dsmShutdown */


/* PROGRAM: dsmShutdownSet -- 
 * 
 * RETURNS: 
 */
dsmStatus_t
dsmShutdownSet(
        dsmContext_t *pcontext,
        int           flag)
{
    /* TBD: get the db context from the user context. */
    pcontext->pdbcontext->pdbpub->shutdn = flag;
    /* Wake the biw so he knows to exit at shutdown  */
    rlwakbiw(pcontext);
    return DSM_S_SUCCESS;

}  /* end dsmShutdownSet */



/* PROGRAM: dsmWatchdog - perform watchdog processing
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_SHUT_DOWN
 *          DSM_S_WATCHDOG_RUNNING
 */
dsmStatus_t
dsmWatchdog(
        dsmContext_t *pcontext)
{

    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t returnCode;

    pdbcontext->inservice++;  /* "post-pone" signal handling while in DSM API */
 
    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    pcontext->pusrctl->uc_usrtyp = WDOG;

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmWatchdog");
        goto done;
    }

    /* check for dead processes */
    returnCode = dbWatchdog(pcontext);

    if(pdbcontext->pdbpub->shutdn)
        returnCode = DSM_S_SHUT_DOWN;
    else if (returnCode == 0)
        returnCode = DSM_S_SUCCESS;
    else if (returnCode == 1)
        returnCode = DSM_S_WATCHDOG_RUNNING;
    else if (returnCode < 0)
        returnCode = DSM_S_SHUT_DOWN;

done:
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;  /* re-allow signal handling */

    return returnCode;

}  /* end dsmWatchdog */


/* PROGRAM: dsmDatabaseProcessEvents - perform xxx processing
 *
 * NOTE: Quiet points and tmdelayed commit are supported here
 * 
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_INVALID_USER
 *          DSM_S_SHUT_DOWN on EXBAD or EXGOOD
 */
dsmStatus_t
dsmDatabaseProcessEvents(
        dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm     = pdbcontext->pdbpub;
    usrctl_t    *pusr       = pcontext->pusrctl;
    mstrblk_t   *pmstr;
    dsmStatus_t  returnCode;

#if OPSYS==WIN32API
    if (fWin95)
        return -1;
#endif

    pdbcontext->inservice++;  /* "post-pone" signal handling while in DSM API */
 
    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmDatabaseProcessEvents");
        goto done;
    }

    /* Check for quiet point command requests */
    if (pdbshm->quietpoint == QUIET_POINT_REQUEST)
    {
        /* Shut off update activity for the quiet point */
        rlTXElock(pcontext,RL_TXE_EXCL,RL_MISC_OP);
        MT_LOCK_MTX();
        pmstr = pdbcontext->pmstrblk;

        /* Switch after imaging extent if there are ai extents */
        if( rlaiqon(pcontext) && pmstr->mb_aibusy_extent > 0 )
        {
            MT_LOCK_AIB();
            if (rlaiswitch(pcontext, (int)RLAI_NEW, 1) )
            {
                MT_UNLK_AIB();
                /* Unable to switch to new ai extent */
                MSGN_CALLBACK(pcontext, drMSG205);
                if ( MTHOLDING(MTL_MTX) )
                     MT_UNLK_MTX();

                if ( pusr->uc_txelk )
                    rlTXEunlock(pcontext);
                goto done;
            }
            MT_UNLK_AIB();
        }

        /* Flush the buffer pool and bi buffers */
        rlbiflsh(pcontext,RL_FLUSH_ALL_BI_BUFFERS);
 
        /* At this point, the quiet point is in place */
        MSGN_CALLBACK(pcontext, drMSG402);
        pdbshm->quietpoint = QUIET_POINT_ENABLED;
        while(pdbshm->quietpoint == QUIET_POINT_ENABLED &&
              !pdbshm->shutdn)
        {
            utsleep(1);
        }
        MT_UNLK_MTX();
        rlTXEunlock(pcontext);        /* Release transaction end lock */

        /* At this point, normal operations can return */
        MSGN_CALLBACK(pcontext, drMSG403);
        pdbshm->quietpoint = QUIET_POINT_NORMAL;       
    }
 
    /* Process delayed commit (-Mf) */
    tmchkdelay(pcontext, 0);
    /* Check if table locking can be shut off    */
    lkTableLockCheck(pcontext);

    returnCode = DSM_S_SUCCESS;

done:
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;  /* re-allow signal handling */

    return returnCode;

}  /* end dsmDatabaseProcessEvents */

/* PROGRAM: dsmRLwriter - Recovery log block writer thread
 *
 *
 * RETURNS: DSM_S_SUCCESS
 */
dsmStatus_t DLLEXPORT
dsmRLwriter(
        dsmContext_t *pcontext)
{
    dsmStatus_t  returnCode = 0;

#if OPSYS==WIN32API
    if (fWin95)
        return -1;
#endif

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRLwriter");
        goto done;
    }
    pcontext->pusrctl->uc_usrtyp = BIW;

    pcontext->pdbcontext->prlctl->abiwpid = pcontext->pusrctl->uc_pid;
    pcontext->pdbcontext->prlctl->qbiwq = QSELF(pcontext->pusrctl);
    
    rlbiclean(pcontext);
    if(pcontext->pdbcontext->pdbpub->shutdn)
        returnCode = DSM_S_SHUT_DOWN;
done:
    dsmThreadSafeExit(pcontext);

    return returnCode;
}

/* PROGRAM: dsmAPW - Buffer pool modified page writer. 
 *
 *
 * RETURNS: DSM_S_SUCCESS
 */
dsmStatus_t DLLEXPORT
dsmAPW(
        dsmContext_t *pcontext)
{
    dsmStatus_t  returnCode = 0;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    ULONG        lastScanTime = 0;
    LONG         curHashNum   = 0;
    LONG         extraScans   = 0;
    LONG         numSkipped   = 0;
    LONG64       bkApwLastBi  = 0;
    int          sleepTime = 0; 
    time_t      curTime;

#if OPSYS==WIN32API
    if (fWin95)
        return -1;
#endif

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmAPW");
        goto done;
    }
    pcontext->pusrctl->uc_usrtyp = APW;
    bkapwbegin (pcontext);

    for(;;)
    {
        curTime = time ((time_t)0);

        bkApw( pcontext, (ULONG)curTime, &lastScanTime,
              &curHashNum, &extraScans, &numSkipped, &bkApwLastBi );

        /* get sleep time from here because promon can adjust it */

        if ( pdbcontext->pdbpub->shutdn )
        {
            if (  (pdbcontext->pdbpub->shutdn == 1) )
            {
                /* Want page writers to help with the final buffer pool
                   flush on normal database shutdown.                   */

                bmFlushx(pcontext, FLUSH_ALL_AREAS, 1);

            }
            break;
        }
        /* sleep in milliseconds */

        sleepTime = (int)(pdbcontext->pdbpub->pwqdelay);
        utnap( sleepTime );

    }
    if(pcontext->pdbcontext->pdbpub->shutdn)
        returnCode = DSM_S_SHUT_DOWN;
done:
    bkapwend( pcontext );

    dsmThreadSafeExit(pcontext);

    return returnCode;
}

/* PROGRAM: dsmOLBackupInit - Start online backup
 *          
 *
 * RETURNS: 
 *      0        - success
 *      negative - an error has occurred
 */
dsmStatus_t 
dsmOLBackupInit(dsmContext_t *pcontext)
{
    int          returnCode;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordUpdate");
        goto done;
    }

#ifdef HAVE_OLBACKUP
    returnCode = (dsmStatus_t)bmBackupOnline(pcontext);
#else
    returnCode = -1;
#endif /* HAVE_OLBACKUP */

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return(returnCode);
}  /* end dsmOLBackupInit */

/* PROGRAM: dsmOLBackupEnd - End online backup
 *          
 *
 * RETURNS: 
 *      0        - success
 *      negative - an error has occurred
 */
dsmStatus_t 
dsmOLBackupEnd(dsmContext_t *pcontext)
{
    int          returnCode;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordUpdate");
        goto done;
    }

#ifdef HAVE_OLBACKUP
    returnCode = (dsmStatus_t)bmBackupClear(pcontext);
#else
    returnCode = -1;
#endif /* HAVE_OLBACKUP */

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return(returnCode);
}  /* end dsmOLBackupEnd */

/* PROGRAM: dsmOLBackupRL - Back up the recovery log (bi file)
 *
 * RETURNS: 
 *      0        - success
 *      negative - an error has occurred
 */
dsmStatus_t 
dsmOLBackupRL(dsmContext_t *pcontext, dsmText_t *ptarget)
{
    int          returnCode;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordUpdate");
        goto done;
    }

#ifdef HAVE_OLBACKUP
    returnCode = rlBackup(pcontext, ptarget);
#else
    returnCode = -1;
#endif /* HAVE_OLBACKUP */

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return(returnCode);
}  /* end dsmOLBackupRL */

/* PROGRAM: dsmOLBackupSetFlag - Set the backup status bit in the db file
 *
 * RETURNS: 
 *      0        - success
 *      negative - an error has occurred
 */
dsmStatus_t
dsmOLBackupSetFlag(
        dsmContext_t *pcontext,
        dsmText_t    *ptarget,
        dsmBoolean_t  backupStatus)
{
    int          returnCode;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmRecordUpdate");
        goto done;
    }

#ifdef HAVE_OLBACKUP
    returnCode = rlBackupSetFlag(pcontext, ptarget, backupStatus);
#else
    returnCode = -1;
#endif /* HAVE_OLBACKUP */

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return(returnCode);
} /* end dsmOLBackupSetFlag */

