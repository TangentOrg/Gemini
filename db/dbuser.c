
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
#if OPSYS==UNIX
#include <unistd.h>  /* for getuid, getgid & getpid */
#endif

#include <errno.h>

#include "bkpub.h"
#include "bkcrash.h"
#include "dbcontext.h"
#include "dbcode.h"
#include "dbmsg.h"
#include "dbpub.h"
#include "dbutret.h"
#include "drmsg.h"
#include "latpub.h"
#include "latprv.h"
#include "lkpub.h"
#include "lkschma.h"
#include "mtmsg.h"
#include "sempub.h"
#include "stmpub.h" /* storage management subsystem */
#include "stmprv.h"
#include "svmsg.h"
#include "usrctl.h"
#include "utcpub.h"
#include "utepub.h"
#include "utospub.h" /* for fdHandle_t */
#include "utspub.h"
#include "uttpub.h"  /* for utsleep() */
#include "utmsg.h"

#if OPSYS==WIN32API
#include <process.h> /* getpid() */
#include <time.h>    /* time()   */
#endif


/* PROGRAM: dbGetUsrctl - obtain a pointer to a free and cleared usrctl
*
*      Note: this routine must preserve any pre-initialized
*      fields in the usrctl structure.
*
* RETURNS: pointer to the usrctl
*          NULL if none available
*
*  The implementation of this function warrants some discussion prior to its
*  implementation due to the number of bugs logged against it. Most notably
*  has been the failure to reserve a user control slot for a shutdown
*  connection resulting in the inability to shutdown the database in an
*  orderly manner.
*
*  Lets begin with the discussion of how and where "userType" is set.
*  Each "userType" is assigned a bit mask that is defined in dbcontext.h
*  allowing it to be compared logically. "userType" is set early on in the
*  utilities main (or thereabouts) function prior to making a db connection.
*  Unfortunately "userType" hasn't been applied consistently making it 
*  difficult to rely upon.  
*
*  The other important aspects of this function are how the number of user
*  control structures are calculated and allocated. Let's start by looking at
*  the components that contribute to the number of user control structures 
*  CALCULATED. For this we need to look at the variable "argnusr" which is set 
*  in drargs.c. The general case is as follows:
*                 "argnusr = arnclts(-n) + argnsrv(-Mn + 1) + 1"
*  One (1) is added to the -Mn (Maximum Servers) parameter to account for the 
*  broker. One (1) is added to the "argnusr" variable to allow for a shutdown
*  connection. Now lets look at a sample value for "argnusr". Lets assume for 
*  this example that we set (-n) to (1) and that we use the default value for
*  the (-Mn) parameter which is (4). "argnusr", according the the calculation
*  presented above will be as follows:
*                 "argnusr = (-n)1 + (-Mn(4) + 1) + 1"
*                 "argnusr = 7"
*  Now that we've calculated "argnusr" to be 7 lets determine how the user
*  control structures are ALLOCATED. We should be able to establish 7
*  connections to a database, the first is always reserved for the BROKER,
*  the remaining 6 are allocated as follows; 4 DBSERVER connections, 1 
*  client connection, and 1 DBSHUT connection. We'll further define "client"
*  connections shortly. User control structures are allocated from the bottom
*  up for local connections and from the top down for remote connections.
*  This reserves the low numbers for connections that require semaphores.
*  First come first served.
*
*  To proceed we need to distinguish between "userType"s to ensure that we
*  reserve the structures as allocated. A client connection can be any type
*  of connection that is not a BROKER, DBSERVER, or DBSHUT. Now lets get on 
*  with the logic.
*  
*/
usrctl_t *
dbGetUsrctl ( dsmContext_t *pcontext,
             int           userType) /* type of user, DBSERVER, REMOTECLIENT,
                                      SELFSERVE, DBSHUT, etc... */
{
    int          i = -1;
    int          ret = 0;
    int          sysErrno = 0;     /* systerm errno from uttty() */
    int          semid;            /* index into the phsem array */
    usrctl_t    *pusr;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm     = pdbcontext->pdbpub;
    ULONG        userNumber;
    QTEXT        userName;
    QTEXT        ttyDevice;
    MTCTL       *pmt;
    COUNT        usercount = 0;
    TEXT        *pterminalDevice;    
    TEXT         userInfo[256];
    fdHandle_t   semnum;

    pmt = pdbcontext->pmtctl;

#if ID_USERTYPE
    MSGD_CALLBACK(pcontext, "%LuserType = %d", userType);
#endif

    /**********************************************************************
    The only type of connection that is allowed once the shutdown flag is
    set is DBSHUT.
    *********************************************************************/
    if ( !(userType & DBSHUT) )
    {
        /* If the shutdown flag is set bail out */
        if (pmt && pdbshm->shutdn)
        {
            ret = semUnlkLog(pcontext);
            if (ret) 
            {
                MSGD_CALLBACK(pcontext,
                    "%LdbGetUsrctl_1: semUnlkLog ret = %d",ret);
            }
            /* The database is being shutdown */
            MSGN_CALLBACK(pcontext, mtMSG003);
            return (usrctl_t *)NULL;
        }
        
        /*********************************************************************
        This is "the" BROKER
        *********************************************************************/
        if (userType & BROKER)
        {
            pusr = pdbcontext->p1usrctl;
            i = pusr->uc_usrnum;
        }
        
        /*********************************************************************
        This is a DBSERVER and therefore we need to find an unused usrctl
        entry in the range from p1usrctl to (argnsrv - 1).
        *********************************************************************/
        else if (userType & DBSERVER)
        {
            pusr = pdbcontext->p1usrctl + 1; /* BROKER gets slot zero */
            i = pusr->uc_usrnum;
            
            for (; (i <= pdbshm->argnsrv - 1) && (pusr->usrinuse); i++, pusr++);
            
            if (i > pdbshm->argnsrv - 1 || i < 0)
            {   
                ret = semUnlkLog(pcontext);
                if (ret)
                {
                    MSGD_CALLBACK(pcontext,
                        "%LdbGetUsrctl_4: semUnlkLog ret = %d", ret);
                }
                /* too many users, try later */
                MSGN_CALLBACK(pcontext, dbMSG031);
                return NULL;
            }
        }
        
        /*********************************************************************
        This is a REMOTECLIENT connection and therefore we need to find an
        unused usrctl entry beginning from the top of argnusr. Connections
        that come in from DBSERVER are allocated from the top of argnusr
        working backwards while all other connection types are allocated from
        the bottom of argnusr, or (argnsrv - 1) working forwards. The first
        argnsrv (-Mn) entries are reserved for DBSERVER connections.
        
        A single usrctl entry is reserved for DBSHUT. Hence (argnusr - 2),
        1 for DBSHUT, 1 because the usrctl structure is 0 based. 
        *********************************************************************/
        else if (userType & REMOTECLIENT)
        {
            pusr = pdbcontext->p1usrctl + pdbshm->argnusr - 2;
            i = pusr->uc_usrnum;
            
            for (; (i > pdbshm->argnsrv - 1) &&
                /* Do not use slots with active tasks - 95-08-16-005 */
                ((pusr->usrinuse) || (pusr->uc_task));
            i--, pusr--);
            
            /* if (i <= pdbshm->argnsrv - 1 || i >= pdbshm->argnusr - 1) */
            if (i <= pdbshm->argnsrv - 1)
            {   
                ret = semUnlkLog(pcontext);
                if (ret)
                {
                    MSGD_CALLBACK(pcontext,
                        "%LdbGetUsrctl_4: semUnlkLog ret = %d", ret);
                }
                /* too many users, try later */
                MSGN_CALLBACK(pcontext, dbMSG031);
                return NULL;
            }
        }
        
        /*********************************************************************
        This is a single user connection and therefore have no user control
        entry to worry about. Test for (pusr->uc_semid == -1) in the case of 
        a 4gl client, (pdbcontext->arg1usr == 1) in the case of DBUTIL.
        
        JFJ - This should be fixed.
        In this case it would be nice if "userType" where consistently applied
        so it could be used as a decision point. However at this time there
        are a handful of single user connections where "userType" could be one
        of many values. Until this is resolved this check needs to be done
        prior to a check for SELFSERVE.
        *********************************************************************/
        else if (pdbcontext->arg1usr)
        {
            pusr = pdbcontext->p1usrctl;
        }
        /*******************************************************************
        Through the process of elimination this is !DBSHUT && !BROKER &&
        !DBSERVER && !REMOTECLIENT &&
        !(pdbcontext->p1usrctl->uc_semid == -1) so therefore...
        This is a local SELFSERVE client connection and therefore we need to
        find an unused usrctl entry in the range from argnsrv to 
        to (argnusr - 1).
        
        JFJ - This should be fixed.
        In this case it would be nice if "userType" where consistently 
        applied so it could be used as a decision point. However at this
        time there are a handful of local client connections where
        "userType" could be one of many values.
        *******************************************************************/
        else
        {
            pusr = pdbcontext->p1usrctl + pdbshm->argnsrv;
            i = pusr->uc_usrnum;
            
            for (; ((i <= pdbshm->argnusr - 1) && (pusr->usrinuse));
            i++, pusr++);
            
            /* 20000717-009 - if the user control uc_semid is -1, then */
            /* there are no wait semaphores available. Chances are the */
            /* user already received a (1093) warning message that     */
            /* only so many wait semaphores were available.            */
            if (pusr->uc_semid == -1)
            {
                ret = semUnlkLog(pcontext);
                if (ret)
                {
                    MSGD_CALLBACK(pcontext,
                        "%LdbGetUsrctl_4: semUnlkLog ret = %d", ret);
                }
                /* No available wait semaphore */
                MSGN_CALLBACK(pcontext, dbMSG003);
                return NULL;
            }

            if (i >= pdbshm->argnusr - 1 || i < 0)
            {   
                ret = semUnlkLog(pcontext);
                if (ret)
                {
                    MSGD_CALLBACK(pcontext,
                        "%LdbGetUsrctl_4: semUnlkLog ret = %d", ret);
                }
                /* too many users, try later */
                MSGN_CALLBACK(pcontext, dbMSG031);
                return NULL;
            }
        }
        
    }
    else
    {
	/*********************************************************************
	This is a DBSHUT connection and therefore it's allowed to consume
	the last usrctl entry.

	JFJ - This should be fixed.
	In this case it would be nice if "userType" where consistently applied
	so it could be used as a decision point. However at this time there
	is a case where DBSHUT is ORed with DBUTIL to indicate the connection
	as a Quiet Point. Until this is resolved we allow a Quiet Point to
	consume the last usrctl entry.
	*********************************************************************/

        pusr = pdbcontext->p1usrctl + pdbshm->argnsrv;

        i = pusr->uc_usrnum;

        for (; (i <= pdbshm->argnusr) && (pusr->usrinuse); i++, pusr++);

        /* This should never happen */
        if (i > pdbshm->argnusr || i < 0)
        {   
            ret = semUnlkLog(pcontext);
            if (ret)
            {
                MSGD_CALLBACK(pcontext,
                    "%LdbGetUsrctl_4: semUnlkLog ret = %d", ret);
            }
            /* too many users, try later */
            MSGN_CALLBACK(pcontext, dbMSG031);
            return NULL;
        }

        /* 20000717-009 - if the user control uc_semid is -1, then */
        /* there are no wait semaphores available. Chances are the */
        /* user already received a (1093) warning message that     */
        /* only so many wait semaphores were available.            */
        /* In this case, we need to let shutdown execute, so try   */
        /* looking for a usrctl that is not in use and has a valid */
        /* semaphore id.                                           */
        if (pusr->uc_semid == -1)
        {
            for(; (i > 0) && ((pusr->uc_semid == -1) || (pusr->usrinuse)); 
                  i--, pusr--);

            /* If we get to the beginning of the usrctl structures chain */
            /* and there are no free usrctl slots with a valid semaphore */
            /* id, then display error message and exit.                  */
            if (i == 0)
            {
                ret = semUnlkLog(pcontext);
                if (ret)
                {
                    MSGD_CALLBACK(pcontext,
                        "%LdbGetUsrctl_4: semUnlkLog ret = %d", ret);
                }
                /* No available wait semaphore */
                MSGN_CALLBACK(pcontext,dbMSG003);
                return NULL;
            } 
        }
    }

    /* clear the usrctl */
    userNumber = pusr->uc_usrnum; /* save from destruction */
    userName = pusr->uc_qusrnam; /* save from destruction */
    ttyDevice = pusr->uc_qttydev; /* save from destruction */
    semid = pusr->uc_semid;
    semnum = pusr->uc_semnum;
    
    stnclr((TEXT *)pusr, sizeof(usrctl_t));
    pusr->uc_qusrnam = userName;
    pusr->uc_qttydev = ttyDevice;
    pusr->uc_usrnum = userNumber;
    pusr->uc_semid = semid;
    pusr->uc_semnum = semnum;
    QSELF(pusr) = P_TO_QP(pcontext, pusr);
    
#if OPSYS==UNIX
    
    /* check if there is a wait semaphore available for the user */
    if ( pdbcontext->dbcode == PROCODE)
        if ( !pdbcontext->argnoshm)
        {
            /* check if the semaphore number is a valid one */
            if (pusr->uc_semnum == -1)
            {
                /* not enough wait semaphores, increase SEMMN */
                ret = semUnlkLog(pcontext);
                if (ret)
                {
                    MSGD_CALLBACK(pcontext,
                        "%LdbGetUsrctl_5: semUnlkLog ret = %d",ret);
                }
                MSGN_CALLBACK(pcontext, dbMSG003);
                return NULL;
            }
            
            /* set the sem value to 0, it may be dirty from previous user */
            if (semSet(pcontext, pusr->uc_semid, pusr->uc_semnum, 0))
                return NULL;
        }
        
        /* save user's real id and group id, assuming that we
        were run with effective id of superuser */
        
        pusr->uc_uid = getuid();
        pusr->uc_gid = getgid();
#endif
        
        /* set pdbcontext->usrctl as soon as possible to prevent rapid login/logout
        problem (seen at irish gas)  */
        pcontext->pusrctl = pusr;
        pdbcontext->pusrctl = pusr;
        pusr->uc_usrtyp = userType;
        
        pusr->uc_pid = getpid ();
        
        if ((pdbcontext->dbcode == PROCODE) &&
            (!((pdbcontext->usertype & DBSHUT) && pdbcontext->argforce)))
        {
        /* lock USR except if we are proshut -F - when we just hope for the
            best */
            
            MT_LOCK_USR ();
        }
        
        /* Lock this usrctl for multi-threading */
        PL_LOCK_USRCTL(&pusr->uc_userLatch)
            
            /* fill in various fields in the usrctl */
            pusr->uc_usrtyp = userType;
        pusr->uc_logtime = time ( (time_t *)0 );
        pusr->uc_heartBeat = 1;
        pusr->uc_beatCount = 0;
        pusr->uc_userLatch = 0;
        pusr->uc_sqlhli    = 0;
        pusr->uc_block     = 1;  /* default is to block/wait rather than queue */
        
        if (userType & REMOTECLIENT)
        {
            /* These fields should be updated later with info from client */
            fillstr(pcontext, (TEXT *)" ", &pusr->uc_qusrnam);
            fillstr(pcontext, (TEXT *)" ", &pusr->uc_qttydev);
        }
        else
            if ( (pdbcontext->usertype & (SELFSERVE+BROKER+DBSERVER+DBSHUT+MONITOR)) ||
                pdbcontext->arg1usr )
            {
                /* user name and terminal name */
                if (pcontext->puserName)
                {
                    fillstr(pcontext, pcontext->puserName, &pusr->uc_qusrnam);
                }
                else
                {
                    fillstr(pcontext, utuid(userInfo, sizeof(userInfo)), 
                                  &pusr->uc_qusrnam);
                }
                pterminalDevice = uttty(0, userInfo, sizeof(userInfo), &sysErrno);
                if ( pterminalDevice == NULL )
                {
                    if ( sysErrno == ENFILE  ||
                        sysErrno == EMFILE  ||
                        sysErrno == ERANGE)  /* terminalDevice buffer too small */
                    {
                        ret = semUnlkLog(pcontext);
                        if (ret)
                        {
                            MSGD_CALLBACK(pcontext,
                                "%LdbGetUsrctl_6: semUnlkLog ret = %d",ret);
                        }
                        FATAL_MSGN_CALLBACK(pcontext, utFTL021);
                    }
                    else
                    {
                        pterminalDevice = (TEXT *)"batch";
                    }
                }
                
                fillstr(pcontext, pterminalDevice, &pusr->uc_qttydev);
            }
            else
            {
                /* others get all blanks */
                fillstr(pcontext, (TEXT *)" ", &pusr->uc_qusrnam);
                fillstr(pcontext, (TEXT *)" ", &pusr->uc_qttydev);
            }
            
            if (    (userType & (SELFSERVE+REMOTECLIENT))
                && !pdbcontext->arg1usr && pmt
                && !(userType & DBSHUT))
                usercount = latGetUsrCount(pcontext, LATINCR); /* count the users */
#ifdef LICENSE_MGT_INUSE
            if (( userType & (SELFSERVE+REMOTECLIENT)) &&
                ( !( userType & BATCHUSER) || ( utcxbdll(34, pgc, 0) == 0)))
                pdbshm->currentusers++;
#else
            if (( userType & (SELFSERVE+REMOTECLIENT)) &&
                ( !( userType & BATCHUSER) ))
                pdbshm->currentusers++;
#endif
            
            /* update license information in shared memory */
            if (userType & BATCHUSER)
            {
                /* update Batch connection counts */
                pdbshm->dblic.numCurBatCncts = pdbshm->dblic.numCurBatCncts + 1;
                if (pdbshm->dblic.maxNumBatCncts < pdbshm->dblic.numCurBatCncts)
                {
                    pdbshm->dblic.maxNumBatCncts = pdbshm->dblic.numCurBatCncts;
                }
                /* update overall connection counts */
                pdbshm->dblic.numCurCncts =  pdbshm->dblic.numCurCncts + 1;
                if (pdbshm->dblic.maxNumCncts < pdbshm->dblic.numCurCncts)
                {
                    pdbshm->dblic.maxNumCncts = pdbshm->dblic.numCurCncts;
                }
            }
            else if (userType & (REMOTECLIENT + SELFSERVE))
            {
                /* update the active connection counts */
                pdbshm->dblic.numCurActvCncts = pdbshm->dblic.numCurActvCncts + 1;
                if (pdbshm->dblic.maxNumActvCncts < pdbshm->dblic.numCurActvCncts)
                {
                    pdbshm->dblic.maxNumActvCncts = pdbshm->dblic.numCurActvCncts;
                }
                /* update overall connection counts */
                pdbshm->dblic.numCurCncts =  pdbshm->dblic.numCurCncts + 1;
                if (pdbshm->dblic.maxNumCncts < pdbshm->dblic.numCurCncts)
                {
                    pdbshm->dblic.maxNumCncts = pdbshm->dblic.numCurCncts;
                }
            }
            
            pcontext->connectNumber = ++pusr->uc_connectNumber;
            
            pusr->uc_lockTimeout = pdbshm->lockWaitTimeOut; 
            pusr->usrinuse = 1;
            
            return pusr;
            
} /* end dbGetUsrctl */


/* PROGRAM: dbDelUsrctl -- mark a usrctl unused and remove it from the
*      active round-robin ring.
*
* RETURNS: DSMVOID
*/
DSMVOID
dbDelUsrctl ( dsmContext_t    *pcontext, usrctl_t    *pusr)  
{
      const LONG  CRASHNUMBERSIZE =1024;
      dbcontext_t *pdbcontext = pcontext->pdbcontext;
      dbshm_t     *pdbshm     = pdbcontext->pdbpub;
      ULONG        userNumber;
      QTEXT        userName;
      QTEXT        ttyDevice;
      MTCTL   *pmt;
      COUNT        usercount;
      crashtest_t *pcrashtest;
      TEXT        pcrashmagic[1024]; /*the size is same as that of 
      CRASHNUMBERSIZE */
      int          crashVersion = 0;
      fdHandle_t   semnum;
      int          semid;
      int          ret = 0;
      
      pmt = pdbcontext->pmtctl;
      if (pdbshm->shutdn == 0)
      {
          /* not shutting down the database */
          
          if (pusr->uc_2phase & TPRCOMM)
          {
          /* User has a 2phase transaction ready to commit and
              then something went wrong. */
              
              if ((pusr->uc_2phase & TPFORCE) == 0)
              {
                  /* Can not delete this user unless the force bit is set */
                  
                  return;
              }
          }
      }
      
      /* 95-08-16-005 - Cannot remove usrctl with an active task
      * 95-10-31-028 - unless shutdn == DB_EXBAD
      */
      if ((pusr->uc_task) && (pdbshm->shutdn != DB_EXBAD))
      {
          return;
      }
      
      if (pdbshm->shutdn == DB_EXBAD)
      {
          /* Do this way to avoid getting hung on locks */
          
          pusr->usrinuse = 0;
          utsleep (1);
          pusr->uc_pid = 0;
      }
      
      
      /* free up any seq reader buffers (done here fore client server) */
      pcontext->pusrctl = pusr;
      bmfpbuf(pcontext, pusr);
      pcontext->pusrctl = NULL;
      
      /*******semLockLog MUST be called after USR is free, to avoid deadlocks ***/
      if (!pdbshm->shutdn)
      {
          ret = semLockLog(pcontext);  /* lock login semaphore*/
          if (ret)
              MSGD_CALLBACK(pcontext,"%LdbDelUsrctl: SemLockLog ret = %d",ret);
      }
      
      pcontext->pusrctl = pusr;
      MT_LOCK_USR();         /* 92-05-27-021 */
      pcontext->pusrctl = NULL;
      
      /* count the users #*/
      if ((pusr->uc_usrtyp & (SELFSERVE+REMOTECLIENT)) && pdbcontext->pmtctl &&
          !(pusr->uc_usrtyp & DBSHUT))
          usercount = latGetUsrCount(pcontext, LATDECR);
      
#ifdef LICENSE_MGT_INUSE
      if (( pusr->uc_usrtyp & (SELFSERVE+REMOTECLIENT)) &&
          ( !( pusr->uc_usrtyp & BATCHUSER) || 
          ( utcxbdll(34, (DSMVOID *)NULL, 0) == 0)))
          pdbshm->currentusers--;
#else
      if (( pusr->uc_usrtyp & (SELFSERVE+REMOTECLIENT)) &&
          !( pusr->uc_usrtyp & BATCHUSER))
          pdbshm->currentusers--;
#endif
      
      /* update license information in shared memory */
      if (pusr->uc_usrtyp & BATCHUSER)
      {
          /* update Batch connection counts */
          if (pdbshm->dblic.numCurBatCncts != 0)
          {
              pdbshm->dblic.numCurBatCncts =  pdbshm->dblic.numCurBatCncts - 1;
              if (pdbshm->dblic.minNumBatCncts > pdbshm->dblic.numCurBatCncts)
              {
                  pdbshm->dblic.minNumBatCncts = pdbshm->dblic.numCurBatCncts;
              }
          }
          /* update Overall connection counts */
          if (pdbshm->dblic.numCurCncts != 0)
          {
              pdbshm->dblic.numCurCncts =  pdbshm->dblic.numCurCncts - 1;
              if (pdbshm->dblic.minNumCncts > pdbshm->dblic.numCurCncts)
              {
                  pdbshm->dblic.minNumCncts = pdbshm->dblic.numCurCncts;
              }
          }
      }
      else if (pusr->uc_usrtyp & (REMOTECLIENT + SELFSERVE))
      {
          /* update Active connection counts */
          if (pdbshm->dblic.numCurActvCncts != 0)
          {
              pdbshm->dblic.numCurActvCncts = pdbshm->dblic.numCurActvCncts - 1;
              if (pdbshm->dblic.minNumActvCncts > pdbshm->dblic.numCurActvCncts)
              {
                  pdbshm->dblic.minNumActvCncts = pdbshm->dblic.numCurActvCncts;
              }
          }
          /* update Overall connection counts */
          if (pdbshm->dblic.numCurCncts != 0)
          {
              pdbshm->dblic.numCurCncts =  pdbshm->dblic.numCurCncts - 1;
              if (pdbshm->dblic.minNumCncts > pdbshm->dblic.numCurCncts)
              {
                  pdbshm->dblic.minNumCncts = pdbshm->dblic.numCurCncts;
              }
          }
      }
      
      /* now we can remove usrctl */
      
      userNumber = pusr->uc_usrnum;
      userName = pusr->uc_qusrnam; /* save from destruction */
      ttyDevice = pusr->uc_qttydev; /* save from destruction */
      semid = pusr->uc_semid;
      semnum = pusr->uc_semnum;
      
      /* Check to see if crash testing is enabled */
      if (pdbcontext->p1crashTestAnchor != NULL) 
      {
          /* Verify magic crash magic number before allocating storage */
          *pcrashmagic='\0';
          if (!utgetenvwithsize((TEXT*)"BKCRASHNUMBER", pcrashmagic, CRASHNUMBERSIZE))
          {
              crashVersion = atoin(pcrashmagic, 1024);
              if (crashVersion == BK_CRASHMAGIC)
              {
                  /* Locate the user of interest */
                  pcrashtest = pdbcontext->p1crashTestAnchor + pusr->uc_usrnum;
                  {
                      pcrashtest->cseed = 0;
                      pcrashtest->ntries = 0;
                      pcrashtest->probab = 0;
                      pcrashtest->crash = 0;
                      pcrashtest->backout = 0;
                  }
              }
          }
      }
      
      pusr->usrinuse = 0;        /* 92-05-27-021 */
      pcontext->pusrctl = pusr;
      MT_UNLK_USR();             /* 92-05-27-021 */
      
      PL_UNLK_USRCTL(&pusr->uc_userLatch)
          
          pcontext->pusrctl = NULL;
      
      stnclr((TEXT *)pusr, sizeof(usrctl_t));
      pusr->uc_qusrnam = userName;
      pusr->uc_qttydev = ttyDevice;
      pusr->uc_usrnum = userNumber;
      pusr->uc_semid = semid;
      pusr->uc_semnum = semnum;
      
      QSELF(pusr) = P_TO_QP(pcontext, pusr);
      
      if (!pdbshm->shutdn) /* semaphore is not locked for shut down */
      {
          ret = semUnlkLog(pcontext); /* unloc login semaphore           */
          if (ret)
              MSGD_CALLBACK(pcontext,"%LdbDelUsrctl: semUnlkLog ret = %d",ret);
      }
      
} /* end dbDelUsrctl */


/* PROGRAM: fillstr - allocate memory for the string, fills it
*      and returns the address of the filled string
*
* RETURNS: 0 for success
*          There is no failure condition.
*/
dsmStatus_t
fillstr( dsmContext_t *pcontext, TEXT    *instring, QTEXT   *pq)
{
      int leng;
      TEXT    *pstr;
      dbcontext_t *pdbcontext = pcontext->pdbcontext;
      dbshm_t *pdbshm     = pdbcontext->pdbpub;
      
      leng = stlen(instring);
      if (leng > 60) leng = 60;
      
      if (*pq) /* if a pointer to the string exists */
          stVacate(pcontext,
          XSTPOOL(pdbcontext, pdbshm->qdbpool), XTEXT(pdbcontext, *pq));
      /* free the old string space */
      
      /* allocate space for the new string */
      pstr = stRent(pcontext,
          XSTPOOL(pdbcontext, pdbshm->qdbpool), (unsigned)leng + 1);
      
      stncop(pstr, instring, leng);
      *pq = (QTEXT)P_TO_QP(pcontext, pstr); /* return the pointer to the filled string */
      
    return 0;
      
} /* end fillstr */
  
/* PROGRAM: dbUserDisconnect - Disconnect a user from a database.
 *      
 *
 * RETURNS: 0 for success
 *          
*/
dsmStatus_t
dbUserDisconnect( dsmContext_t *pcontext, int exitflags)
{
      dbcontext_t *pdbcontext = pcontext->pdbcontext;
      dsmStatus_t  rc = 0;
      usrctl_t     *pusr = pcontext->pusrctl; /* usrctl to be freed up */
      int           i;
      
      /* THIS FUNCTION SHOULD NEVER ISSUE FATAL MESSAGES --
      * OTHERWISE WE'LL RECURSE AND DECREMENT THE USECOUNT
      * FLAG TOO OFTEN.
      */
      
      /* avoid race condition when 2 threads are logging out at the same time 
      * or a thread logging in while another thread is logging out.
      */
      PL_LOCK_DBCONTEXT(&pdbcontext->dbcontextLatch)
          
          if( pdbcontext->useCount > 0 )
              pdbcontext->useCount--;
          
          if (pdbcontext->useCount == 0)
          {
              /* This is the last user so detach from the database   */
              rc = dbenvout(pcontext, exitflags);
              PL_UNLK_DBCONTEXT(&pdbcontext->dbcontextLatch)
                  return rc;
          }
          
          if( pdbcontext->pdbpub->shutdn != DB_EXBAD )
          {
              i = latpoplocks (pcontext, pusr);
              if ( i && pdbcontext->pdbpub->shutdn == DB_EXBAD)
              {
                  /* User %i died holding %i shared memory locks    */
                  MSGN_CALLBACK(pcontext,drFTL012,pusr->uc_usrnum, i);
              }
              i = bmPopLocks(pcontext, pusr);
              if ( i && pdbcontext->pdbpub->shutdn == DB_EXBAD)
              {
                  MSGD_CALLBACK(pcontext,drFTL013, pusr->uc_usrnum, i);
              }
          }
          
          if ( pdbcontext->pdbpub->shutdn != DB_EXBAD )
          {
              latcanwait(pcontext);
              rc = lkresync(pcontext);
          }
          
          /*----------------------------------------------------------*/
          /* Bug 19991129-020 (start of added code).          */
          /*----------------------------------------------------------*/
          /*                              */
          /* Coded by richt & dmeyer, V9.1a, 1/5/2000.        */
          /*                              */
          /* When we are the server we are shutting down the user */
          /* and heading to a server shutdown.  In the case of the    */
          /* server this restores the message that used to be logged  */
          /* prior to V9.0.  For all others the "Logout by %s on %s"  */
          /* will continue to be generated.               */
          /*----------------------------------------------------------*/
          
          if ( pusr -> uc_usrtyp & DBSERVER )
              
              /*------------------------------*/
              /* Message "Stopped." ala v8.0x */
              /*------------------------------*/
              
              MSGN_CALLBACK( pcontext, drMSG170 );
          
          else
              
              /*------------------------------------------------------*/
              /* Bug 19991129-020 (end of added code).        */
              /*------------------------------------------------------*/
              
              MSGN_CALLBACK(pcontext, drMSG022,
              XTEXT(pdbcontext, pusr->uc_qusrnam),
              XTEXT(pdbcontext, pusr->uc_qttydev));
          
          pcontext->pusrctl = NULL;  /* prevent infinite loops */
          
          if( pdbcontext->pdbpub->shutdn == DB_EXBAD )
          {
          /* In abnormal shut down we want to do as little as
          possible to get the user disconnected from the database
              so just clear in-use flag in the user control               */
              pusr->usrinuse = 0;
          }
          else
          {
              dbDelUsrctl(pcontext, pusr);
          }
          
          PL_UNLK_DBCONTEXT(&pdbcontext->dbcontextLatch)
              
              return rc;
          
}  /* end dbUserDisconnect */


/* PROGRAM: dbUserError - display an error message for the user
 *      
 *
 * RETURNS: nothing
 *          
 */
DSMVOID
dbUserError(dsmContext_t *pcontext,   /* context of error occurance */
    LONG errorStatus,         /* Return status from function error */
    int osError,              /* system error number */
    int autoerr,              /* is this a fatal ? */
    DSMVOID *msgDataA,           /* data to be displayed in message */
    DSMVOID *msgDataB)           /* data to be displayed in message */
{
       
       switch(errorStatus)
       {
       case DBUT_S_NAME_TOO_LONG:
           /* Attempt to make a too long file-name. */
           FATAL_MSGN_CALLBACK(pcontext, utFTL003);
           break;
       case DBUT_S_SEEK_TO_FAILED:
       case DBUT_S_SEEK_FROM_FAILED:
           FATAL_MSGN_CALLBACK( pcontext, utFTL010, osError,
               (fileHandle_t)msgDataA, (LONG)msgDataB);
           break;
       case DBUT_S_GET_OFFSET_FAILED:
           break;
       case DBUT_S_READ_FROM_FAILED:
           FATAL_MSGN_CALLBACK(pcontext,  utFTL001, osError,
               "utReadFromFile", errorStatus,
               (fileHandle_t)msgDataA, (LONG)msgDataB);
           break;
       case DBUT_S_WRITE_TO_FAILED:
           switch(osError)
           {
           case EFBIG:
               if (autoerr)
               {
                   FATAL_MSGN_CALLBACK(pcontext, utFTL005,
                       "utWriteToFile" );
               }
               else
               {
                   MSGN_CALLBACK(pcontext, utMSG178,
                       "utWriteToFile");
               }
               break;
           case ENOSPC:
               /* no disk space left */
               if( autoerr )
               {
                   FATAL_MSGN_CALLBACK(pcontext, utFTL002);
               }
               else
               {
                   MSGN_CALLBACK(pcontext, utMSG179);
               }
               break;
           default:
               if (autoerr)
               {
                   FATAL_MSGN_CALLBACK(pcontext, utFTL001, osError,
                       "utWriteToFile", errorStatus,
                       (fileHandle_t)msgDataA,
                       (LONG)msgDataB);
               }
               else
               {
                   MSGN_CALLBACK(pcontext, utMSG070, osError,
                       "utWriteToFile", errorStatus, 
                       (fileHandle_t)msgDataA,
                       (LONG)msgDataB);
               }
           }
           break;
           default:
               break;
               
       }
       
   return;
       
}  /* end dbUserError */
