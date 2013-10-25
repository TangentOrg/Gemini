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

/* note: stdlib.h must go first in order to avoid conflicting
         definitions with DFILE */
#include <stdlib.h>
#include "dbconfig.h"
#if OPSYS==UNIX
#include <unistd.h>  /* for getpib */
#include <signal.h>  /* for kill */
#endif

#include <errno.h>

#include "bkcrash.h"
#include "bkpub.h"
#include "bkiopub.h"  /* for bkioCreateLkFile & bkioRemoveLkFile */
#include "cxpub.h"
#include "cxprv.h"
#include "dbcode.h"
#include "dbcontext.h"
#include "dblk.h"
#include "dbmsg.h"
#include "bimsg.h"
#include "dbpub.h"
#include "dbprv.h"
#include "dbvers.h"
#include "drmsg.h"
#include "dsmpub.h"
#include "extent.h"
#include "keypub.h"
#include "kypub.h"
#include "latpub.h"
#include "latprv.h"
#include "lkpub.h"
#include "lkmgr.h"
#include "ompub.h"
#include "objblk.h"
#include "recpub.h"
#include "rlpub.h"
#include "rltlprv.h"
#include "rmpub.h"       /* for RMNOTFND  */
#include "sempub.h"   /* for semUnlkLog & semLockLog */
#include "seqpub.h"
#include "shmpub.h"
#include "statpub.h"
#include "stmpub.h"
#include "stmprv.h"
#include "stmsg.h"
#include "tmprv.h"
#include "usrctl.h"
#include "utbpub.h"
#include "utcpub.h"
#include "utepub.h"
#include "utfpub.h"
#include "utmpub.h"
#include "utmsg.h"
#include "utospub.h"
#include "utspub.h"
#include "uttpub.h"
#include "scasa.h"
#include "bmpub.h"

#ifdef DEBUGSL
GLOBAL	FILE	*fsdebug = NULL;
GLOBAL	TEXT	filnm[8] = "xdebug0";
#endif

#include <time.h>
#if OPSYS==WIN32API
#include <io.h>         /* for fseek TODO: remove for tmp table fixup */
#include "windows.h"
#include "process.h"
int fWin95 = 0;
#endif

#if ASSERT_ON
#define MSGD MSGD_CALLBACK
#endif

#define DBRECMAX        3600  /* Probable max length of _Db record */

/*****  LOCAL FUNCTION PROTOTYPE DEFINITIONS  ****/

LOCALF DSMVOID	dbsvstuff	(dsmContext_t *pcontext);
LOCALF DSMVOID	dbldstuff	(dsmContext_t *pcontext);

LOCALF int
buildObjectRecord(
        dsmContext_t    *pcontext,
        dsmArea_t        areaNumber,
        dsmObject_t      objectNumber,
        dsmObjectType_t  objectAssociateType,
        dsmObject_t      objectAssociate,
        dsmObjectType_t  objectType,
        dsmObjectAttr_t  objectAttr,
        dsmDbkey_t       objectBlock,
        dsmDbkey_t       objectRoot,
        ULONG            objectSystem, /* not currently used */
        dsmDbkey_t      *pobjectRootBlock);

LOCALF dsmStatus_t
		dbenv1		(dsmContext_t *pcontext, TEXT *prunmode,
                                 int mode);
LOCALF dsmStatus_t
		dbenv2		(dsmContext_t *pcontext);

#if 0
LOCALF TEXT *	dbmovqnam	(dsmContext_t *pcontext, TEXT *pname,
				 STPOOL *ppool);
#endif

LOCALF TEXT *	dbpubname	(dsmContext_t *pcontext,
				 TEXT *pname, STPOOL *ppool);

LOCALF int	dbualloc	(dsmContext_t *pcontext);

LOCALF int	dbustatalloc	(dsmContext_t *pcontext);

LOCALF DSMVOID     dbCreateDB    (dsmContext_t *pcontext);

LOCALF DSMVOID     dbCreateDB2    (dsmContext_t *pcontext);

LOCALF DSMVOID     dbCheckParameters (dsmContext_t *pcontext);

LOCALF DSMVOID     dbWriteLoginMessage(dsmContext_t *pcontext,
                                    TEXT         *punname,
                                    TEXT         *pttyname);


/*****  END LOCAL FUNCTION PROTOTYPE DEFINITIONS  ****/


/* PROGRAM: dbopen - open the database and establish all needed control
 *		     structures
 *
 * This program is called by any process which will access the database
 * or its related control structures directly.
 * This includes usertype:
 *		  BROKER
 *		  SELFSERVICE (including single-user PROGRESS)
 *		  DBSERVER
 *		  DBSHUT
 * But does not include:
 *		  REMOTECLIENT or REMOTECLIENT+DBSHUT
 *
 * This program expects:
 *	dbctl - already allocated and filled in with command line parms
 *	dbpub - already allocated and filled in with command line parms
 *	usertype - already set to describe the type of process executing
 *	arg1usr  - already set if this user will have exclusive db use
 *
 * RETURNS: 0 = OK
 *      non-0 = not OK, an error msg was already printed
 */
dsmStatus_t
dbenv(
	dsmContext_t *pcontext,
	TEXT    *prunmode, /* string describing use, goes in lg file  */
	int      mode)	   /* bit mask with these bits:
	      		      DBOPENFILE, DBOPENDB, DBSINGLE, DBMULTI,
			      DBRDONLY, DBNOXLTABLES, DBCONVERS */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;  /* partly built dbctl -
                                                  * db to open */
    dsmStatus_t	 ret;
    
    /* The first user to access the database must call dbenv1 to open	*/
    /* it and create all shared resources such as shared memory.	*/
    /* The second and subsequent users then just call dbenv2 to open	*/
    /* the database and attach all the shared resources			*/
    if (   (pdbcontext->usertype & BROKER)
	|| pdbcontext->argnoshm)
    {
	ret = dbenv1(pcontext, prunmode, mode);
        if (ret)
        {
          /* If the db is in use, return DSM_S_DB_IN_USE to terminate the
          ** server.  In any other failure, the server starts up with
          ** Gemini disabled.
          */
          if (ret == DB_EXSRVUSE || ret == DB_EXSNGLUSE ||
              ret == BKIO_LKSTARTUP)
          {
            MSGD_CALLBACK(pcontext, "Gemini initialization failed.  This server session will be");
            MSGD_CALLBACK(pcontext, "terminated.");
            ret = DSM_S_DB_IN_USE;
          }
          else
          {
            MSGD_CALLBACK(pcontext, "Gemini initialization failed.  During this server session");
            MSGD_CALLBACK(pcontext, "all Gemini tables will be inaccessible.  If the problem");
            MSGD_CALLBACK(pcontext, "persists consider restoring all the Gemini files from");
            MSGD_CALLBACK(pcontext, "backup, or use the --gemini-recovery=FORCE argument to");
            MSGD_CALLBACK(pcontext, "skip crash recovery.");
          }
        }
    }
    else
	ret = dbenv2(pcontext);

    if ( pdbcontext->usertype & (BROKER+DBSHUT))
	pdbcontext->brkklflg = 1; /* enables brokkil when shutdown happens */

    if (ret)
    {	
        PL_UNLK_DBCONTEXT(&pdbcontext->dbcontextLatch);
	dbUserDisconnect(pcontext, DB_EXBAD);
        PL_LOCK_DBCONTEXT(&pdbcontext->dbcontextLatch);
	return ret;
    }

    /* The remainder is misc initializations which   */
    /* don't fall neatly into either dbenv1 or dbenv2 */

    /* 
     * If we have a condb structure (open is via client code), then we need to
     * complete initialization by determining db version.  We happen to have
     * the db record handy at this point so save collation/translation tables
     * for later use.  Otherwise, save the collation/translation tables using
     * special server-based code.
     */
    pdbcontext->psvusrctl = pcontext->pusrctl;

    /* put Login message in the .lg file */
    /* bug 97-09-30-008 - open lg file earlier. If we did open it
       earlier, then we may have already done the login message and don't
       need to do another one */
    if (pcontext->pusrctl->uc_loginmsg != -1)
    {
       /* we did not write a login message yet. Do it now */
       dbWriteLoginMessage (pcontext,
                            XTEXT(pdbcontext, pcontext->pusrctl->uc_qusrnam),
                            XTEXT(pdbcontext, pcontext->pusrctl->uc_qttydev));

    }

#ifdef DEBUGSL
    if (!(pcontext->pusrctl->usertype & REMOTECLIENT))
    {
	filnm[6] = '0' +  pcontext->pusrctl->uc_usrnum;
	fsdebug = fopen (filnm,"w");
	/* setbuf(fsdebug,0);  unbuffered */
    }
#endif

    /* verify critical db information */
    BKVERIFY("dbenv", pdbcontext);

    return 0;

} /* end dbenv */


/* PROGRAM: dbenv1 - The first user of a database calls this to open it
 *			and create all (shared) resources
 *
 * RETURNS: 0 = OK
 * 	non-0 = not OK, an error msg was already printed
 */
LOCALF dsmStatus_t
dbenv1(
	dsmContext_t	*pcontext,
	TEXT    *prunmode, /* string describing use, goes in lg file */
	int      mode)	   /* bit mask with these bits:
			      DBOPENFILE    - open the files only
			      DBOPENDB      - do crash recovery	*/
{
    int	     ret;
    dbshm_t *pdbshm;
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    int      createDB = 0;
#if OPSYS==UNIX
    bkIPCData_t *pbkIPCData;
#endif

#if OPSYS==WIN32API
    /* Check if we're running on a Win9x machine (not NT or 2000).
    ** Since Win95 lacks overlapped file I/O and other features of NT,
    ** we achieve thread safety on Win95 by making DSM essentially
    ** single-threaded.
    */
    fWin95 = utDetermineVersion() ? 0 : 1;
#endif

    /* We don't support read-only for this list of usertypes */
    if (pdbcontext->argronly && pdbcontext->usertype &
	(BROKER+DBSERVER+REMOTECLIENT+DBSHUT+MONITOR+BACKUP))
    { 
	/*Read-only is not supported in this mode. */
	MSGN_CALLBACK(pcontext, drMSG123);
        return -1;
    }

    /* may need to masage open mode */
    if ((pdbcontext->accessEnv & DSM_SQL_ENGINE) && 
        (mode & DSM_DB_SINGLE) )
    {
        mode |= DSM_DB_OPENFILE;
        if (!(pdbcontext->argronly || (mode & DSM_DB_RDONLY)) )
        {
            /* Not read only so ensure crash recovery for single user SQL */
            mode |= DSM_DB_OPENDB;
        }
    }

    /* check to see if the database is already locked and then lock it	*/

    /* Check if its the special gemini db name  */
    if(stncmp(pdbcontext->pdbname,(TEXT *)"gemini",6) == 0)
      {
	TEXT fdbname[MAXUTFLEN];

	utmypath(fdbname,pdbcontext->pdatadir,pdbcontext->pdbname,
		 (TEXT *)".db");
	/* Does it exist                               */
	if ( utfchk ( fdbname, UTFEXISTS ) == utMSG001 )
	  {
	    dbCreateDB(pcontext);
	    createDB = 1;
	  }
#ifdef HAVE_OLBACKUP
        else
          {
            /* if the .db file exists, make sure that it's not from an online
            ** backup which didn't finish
            */
            dsmBoolean_t backup;
            ret = rlBackupGetFlag(pcontext, fdbname, &backup);
            if (ret)
            {
              MSGD_CALLBACK(pcontext, "Could not check backup status in Gemini db");
              return DSM_S_CANT_CHECK_BACKUP;
            }
            else if (backup)
            {
              MSGD_CALLBACK(pcontext, "Gemini database cannot be opened because it is");
              MSGD_CALLBACK(pcontext, "an incomplete backup");
              return DSM_S_PARTIAL_BACKUP;
            }
          }
#endif /* HAVE_OLBACKUP */
      }
        
    if ((ret = dblk(pcontext, (mode & DBSINGLE ? DBLKSNGL : DBLKMULT), 1)))
      return ret;

    /* This is required by shmCreate to calculate shared memory size
       and also required by bmbufo called from dbSetOpen() 
    */

    if( ! (pdbcontext->pdbpub->directio ) )
    {
      if ((pdbcontext->pdbpub->dbblksize=
	   bkGetDbBlockSize (pcontext, 
			     bkioGetFd(pcontext,
		     pdbcontext->pbkfdtbl->pbkiotbl[pdbcontext->bklkfd],
				       BUFIO), pdbcontext->pdbname)) == 0)
	return -1;
    }
    else
    {
      if ((pdbcontext->pdbpub->dbblksize=
	   bkGetDbBlockSize (pcontext, bkioGetFd(pcontext,
		  pdbcontext->pbkfdtbl->pbkiotbl[pdbcontext->bklkfd],
		   UNBUFIO), pdbcontext->pdbname)) == 0)
	return -1;
    }
 
    /* Calculate dbCreateLim and dbTossLim */

    if (pdbcontext->pdbpub->dbblksize == 8192)
    {
        /* for 8K blocksize */
        pdbcontext->pdbpub->dbCreateLim = 600;
        pdbcontext->pdbpub->dbTossLim = 2000;
    }
    else
    {
        /* for all other blocksizes */
        pdbcontext->pdbpub->dbCreateLim = 300;
        pdbcontext->pdbpub->dbTossLim = 1000;
    }

    /* Check and adjust shm related parameters for proper values */
    dbCheckParameters(pcontext);
    pdbcontext->pdbpub->qdbpool = P_TO_QP(pcontext, pdbcontext->privpool);
    /* create the shared resources and structures to coordinate them	*/
    /* single user must do this also, to check if shm in use.
    this may occure if the broker is dead and shm is still in use */
    if ( pdbcontext->dbcode == PROCODE )    /* But not if opening a
					   temp database         */
#if OPSYS==UNIX
    {
      /* If we are doing a conv68 or conv78 it is not possible to try  */
      /* and do clean up anymore, since the ipc key generation changed */
      if (!(mode & DBCONVERS) &&
          !((mode & DBCONTROL) && (mode & DBRDONLY)) )
      {
        /* allocate storage for the IPC structure */
        pbkIPCData = (struct bkIPCData *)utmalloc(sizeof(struct bkIPCData));

        /* Make sure we got the storage */
        if (pbkIPCData == (struct bkIPCData *)0)
        {
            /* We failed to get storage for pbkIPCData */
            MSGN_CALLBACK(pcontext, stMSG006);
            return -1;
        }
        else
        {
            /* Initialize all slots in the structure holding IPC id's */
            pbkIPCData->OldShmId = -1;
            pbkIPCData->OldSemId = -1;
            pbkIPCData->NewShmId = -1;
            pbkIPCData->NewSemId = -1;
        }
        /* Try and read IPC values from mstrblk so IPC in use status can */
        /* be checked.  Failure means mstrblk couldnt be read  */
        if (bkGetIPC(pcontext, pdbcontext->pdbname, pbkIPCData) == 0)
        {
            if ((ret=semCreate(pcontext, pbkIPCData))) /* create semaphores */
            {
                pdbcontext->psemdsc = NULL;
                utfree((TEXT *)pbkIPCData);
                return ret;
            }
            else
            {
                if((pbkIPCData->NewShmId != -1) && (pbkIPCData->NewSemId != -1))
                {
                   if (bkPutIPC(pcontext, pdbcontext->pdbname, pbkIPCData) != 0)
                   {
                       /* No need to display message for -RO -1 */
                       if (!(mode & DBSINGLE))
                       {
                           MSGN_CALLBACK(pcontext, bkMSG116);
                           /* clean up the storage for pbkIPCData */
                           utfree((TEXT *)pbkIPCData);
                           return -1;
                       }
                   }
                }
                /* clean up previously allocated storage */
                utfree((TEXT *)pbkIPCData);
            }
        }
        else
        {
            /* Clean up previously allocated storage */
            utfree((TEXT *)pbkIPCData);
            return -1;
        }
     }     /* if not convdb */
    }
#else

    if ((ret=semCreate(pcontext))) /* create semaphores */
    {
            pdbcontext->psemdsc = NULL;
	    return ret;
    }
#endif


    /* by now, dbpub has been put in the correct location and wont move */
    pdbshm = pdbcontext->pdbpub;

    /* Get a MTCTL structure set up, allocate latches */
    latalloc(pcontext);

    /* init latches */
    latseto (pcontext);

    /* copy the .db, .bi and .ai file names to the shared memory */
    if (pdbshm->qdbname)
      pdbshm->qdbname = 
	P_TO_QP(pcontext,
                dbpubname(pcontext, XTEXT(pdbcontext, pdbshm->qdbname),
                XSTPOOL(pdbcontext, pdbshm->qdbpool)));

    if (pdbshm->qbiname)
	pdbshm->qbiname = 
	    P_TO_QP(pcontext, dbpubname(pcontext,
                    XTEXT(pdbcontext, pdbshm->qbiname),
                    XSTPOOL(pdbcontext, pdbshm->qdbpool)));
    if (pdbshm->qainame)
	pdbshm->qainame = 
	    P_TO_QP(pcontext, dbpubname(pcontext,
                    XTEXT(pdbcontext, pdbshm->qainame),
                    XSTPOOL(pdbcontext, pdbshm->qdbpool)));


    /* allocate the srvctl and usrctl arrays */
    if (dbualloc(pcontext))
        return -1;

    /* allocate sequence control structures */

    if (seqAllocate(pcontext))
        return -1;

    /* allocate object cache                 */
    ret = omAllocate(pcontext);
    if( ret )
        return ret;

#if OPSYS==WIN32API
    if ((!pdbcontext->arg1usr) && (!pdbcontext->argnoshm))
        if (ret=semUsrCr(pcontext))       /* create rest of semaphores    */
            return ret;
#endif

    if (dbGetUsrctl(pcontext, (int)pdbcontext->usertype)==NULL)
        return -1;

    if (MTHOLDING (MTL_USR)) MT_UNLK_USR();

    if (!prunmode)
        prunmode = (TEXT *)"";



    if(createDB)
    {
        mode |= DBCONTROL;  
        mode &= ~(DBOPENDB | DBOPENFILE);
    }
    
    ret = dbSetOpen(pcontext, prunmode, mode);
    if (ret)
        return ret;

    if ( dbustatalloc(pcontext) ) return -1;

    if(createDB)
    {
        dbCreateDB2(pcontext);
        mode |= DBOPENDB;
        mode &= ~DBCONTROL;   
        ret = dbSetOpen(pcontext, prunmode, mode);
        if (ret)
            return ret;
    }

    if (!pdbcontext->argnoshm)
    {
	/* save shared startup option arguments in the shared memory */
	/* must be after dbSetOpen, since some arguments may be changed by it */
	dbsvstuff(pcontext);

	ret = semRdyLog(pcontext); /* unlock the login semaphore */
	if (ret)
            return ret;
    }

    bkRemoveAreaRecords(pcontext);
 
    /* Test for client/database minor version incompatibility */
    if (pdbcontext->pmstrblk &&
        pdbcontext->pmstrblk->mb_clientMinorVers)
    {
        /* Client/database minor version mismatch.  Expected %l. Found %l. */
        MSGN_CALLBACK(pcontext, dbMSG045, 
                      0, pdbcontext->pmstrblk->mb_clientMinorVers);
        /* WARNING: updates to this database are not recomended. */
        MSGN_CALLBACK(pcontext, dbMSG044);
    }

    return 0;

} /* end dbenv1 */


/* PROGRAM: dbenv2 - used with SHMEMORY to access an open database
 *		     called by SELFSERVICE, DBSERVER, DBSHUT
 *
 * RETURNS: 0 = OK
 *      non-0 = not OK, an error msg was already printed
 */
LOCALF dsmStatus_t
dbenv2(
	dsmContext_t	*pcontext) /* partly built dbctl of db to open */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int     ret;
    int     semret = 0;
    MTCTL  *pmt;
    GTBOOL   shutdown; /* is this called by proshut ??? */

    /* NOTE!!! there's probably a bug here given we attach to shared
     * before we grab the login semaphore.
     */

    ret = shmAttach(pcontext);	/* attach the shared memory */
    if (ret)
        goto xret;

    /* attach the semaphores */
    ret = semGetId(pcontext);
    if (ret)
        return ret;

    /* wait for the login sem	*/
    semret = semLockLog(pcontext);
    if (semret)
    {
        MSGD_CALLBACK(pcontext,"%Bdbenv2_1:mtlocklg ret = %d",semret);
        return semret;
    }

    dbldstuff(pcontext);	/* Get private copies of shared pointers and etc */
    pmt = pdbcontext->pmtctl;
    shutdown = pdbcontext->usertype & DBSHUT;

    /* force -F shutdown if bad things have happened */
    if(shutdown && pdbcontext->pdbpub->shutdn == DB_EXBAD)
       pdbcontext->argforce = 1;

    if (!(shutdown && pdbcontext->argforce) &&
        (deadpid(pcontext, pdbcontext->pdbpub->brokerpid)))
    {
	/* There is no server for database %s */
	MSGN_CALLBACK(pcontext, dbMSG008, pdbcontext->pdbname);
	ret = -1;
	goto xret;
    }

#if OPSYS==WIN32API
    if (ret = semUsrGet(pcontext)) 
    {
	ret = -1;
        MSGD_CALLBACK(pcontext,"Unable to load semaphore table");
    	goto xret;
    }
#endif

    /* get my very own usrctl structure */
    if (dbGetUsrctl(pcontext, (int)pdbcontext->usertype)==NULL)
    {
	ret = -1;
	goto xret;
    }

    if( MTHOLDING(MTL_USR)) MT_UNLK_USR();

    /* Bug 97-09-30-008 open .lg file, put the Login message in the .lg file
       asap - BEFORE opening database, so we do not lose any error messages
       that may occur during database open. This will cause file descriptors
       to be used in a different order than before. Someone may notice this.
       Note that we are in dbenv2, which is called by self-serving clients,
       servers, and other processes that are connecting multi-user to a
       database that has already had the broker started for it.
    */

    if (dbLogOpen(pcontext,pdbcontext->pdbname) < 0)
    {
        ret = -1;
        goto xret;
    }

    dbWriteLoginMessage(pcontext,
                        XTEXT(pdbcontext, pcontext->pusrctl->uc_qusrnam),
                        XTEXT(pdbcontext, pcontext->pusrctl->uc_qttydev));

    semret = semUnlkLog(pcontext);	/* and unlock the login semaphore */
    if (semret)
        MSGD_CALLBACK(pcontext,"%Ldbenv2_1: semUnlkLog ret = %d",semret);

    if(bkOpen2ndUser(pcontext))	/* open db files, alloc private db buffers */
    {
	ret = -1;
	goto xret;
    }


#if 0
    /* SELFSERVE (interactive or batch) users must be checked against */
    /* the configuration file's specified max users, but first check  */
    /* to see if the USER_COUNT_BYPASS flag is set or if the server   */
    /* is being used as a scheme holder.                              */

    if ( !(globLicenseFlags & USER_COUNT_BYPASS) &&
           (pdbcontext->pdbpub->dbIsHolder == (TEXT)0) )
    {
#ifdef LICENSE_MGT_INUSE
        if ( ( pdbcontext->usertype & (SELFSERVE)) &&
             ( !( pdbcontext->usertype & BATCHUSER)  || 
               	( utcxbdll(34, (DSMVOID *)NULL, 0) == 0)) ) 
#else
        if ( ( pdbcontext->usertype & (SELFSERVE)) &&
             ( !( pdbcontext->usertype & BATCHUSER) ) ) 
#endif
        {
            /* The user defined threshold (-usrcount) must be checked against the */
            /* current number of users except in the case of -usrrpt 0, then no   */
            /* reporting is done.                                                 */ 
            if (( pdbcontext->pdbpub->dblic.userThreshold != 0) && 
                ( pdbcontext->pdbpub->currentusers > pdbcontext->pdbpub->dblic.userThreshold ))       
            {
                /* increment count of attempts to exceed user */
                /* defined threshold count */
                pdbcontext->pdbpub->dblic.thresholdExceeded++;
                
                /* User defined threshold exceeded by <userid> on <device> */
                MSGN_CALLBACK(pcontext, dbMSG019,
                              utuid(userId, sizeof(userId)),  ttyidx);

                /* <N> attempts to exceed the threshold count (<n>) */
                /* since server startup                             */     
                MSGN_CALLBACK(pcontext, dbMSG020, 
                      pdbcontext->pdbpub->dblic.thresholdExceeded, 
                      pdbcontext->pdbpub->dblic.userThreshold);

            }

            /* check to see if pdbpub->configusers is LESS than */
            /* pdbpub->currentusers because pdbpub->currentusers is */
            /* incremented in dbGetUsrctl() and includes the user that */
            /* is currently being tested. ie. pdbpub->currentusers   */
            /* could be equal to pdbpub->configusers */

            if (( pdbcontext->pdbpub->configusers != -1) &&
               ( pdbcontext->pdbpub->configusers < pdbcontext->pdbpub->currentusers))
            {
                /* increment count of attempts to exceed user count */
                pdbcontext->pdbpub->dblic.cfgmaxuExceeded++;

                /* You have exceeded the number of users specified in your license. */
                /* displayed to the client screen */
                MSGN_CALLBACK(pcontext, dbMSG006);

                /* User count exceeded. Failed attempt to login by <userid> */
                /* on <device> */
                MSGN_CALLBACK(pcontext, dbMSG018, 
                              utuid(userId, sizeof(userId)), ttyidx);

                /* <N> attempts to exceed the licensed user count (<n>) */
                /* since server startup */
                MSGN_CALLBACK(pcontext, dbMSG017,
                       pdbcontext->pdbpub->dblic.cfgmaxuExceeded,
                       pdbcontext->pdbpub->configusers);

#ifdef LICENSE_MGT_INUSE
                /* if the LICENSE-ENFORCE bit is on, then disconnect the user */
                if(utcxbdll(392,(DSMVOID *)NULL,1) == 0)
                {
                    /* set error code and goto end */
                    ret = -1;
                    goto xret;
                }
#endif
            else 
                ret = 0;
            }

            /* increment the userTotal count (total users connected */
            /* since server starup */
            pdbcontext->pdbpub->dblic.userTotal++;

            /* if the current number of users is greater than the max */
            /* concurrent users, then set the max concurrent users to */
            /* the current user count. */

            if (pdbcontext->pdbpub->currentusers >
                 pdbcontext->pdbpub->dblic.maxConcurrent) 
                pdbcontext->pdbpub->dblic.maxConcurrent =
                                    pdbcontext->pdbpub->currentusers;

            /* if the current number of users is greater than the max */
            /* concurrent users since the last report entry, then set */
            /* the variable to the current user count */    

            if (pdbcontext->pdbpub->currentusers >
                 pdbcontext->pdbpub->dblic.maxLastEntry) 
                pdbcontext->pdbpub->dblic.maxLastEntry =
                                    pdbcontext->pdbpub->currentusers; 
        }
    }
#endif


xret:

    semret = semUnlkLog(pcontext);	/* and unlock the login semaphore */
    if (semret)
        MSGD_CALLBACK(pcontext,"%Ldbenv2_2: semUnlkLog ret = %d",semret);

    if (ret == -1)
        pdbcontext->psemdsc = NULL;
    
    /* make sure we don't have any inhereted locks */
    lkrend(pcontext, 1);

    return ret;

} /* end dbenv2 */




/* PROGRAM: dbenv3 - simple user connect
 *          basically get a new usrctl
 *
 * RETURNS: 0 success
 *          1 failure
 */
dsmStatus_t
dbenv3(dsmContext_t *pcontext)
{

    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int          userType;
    int          ret = 0;

    /* We assume only one server per context. So, if here and we have 
     * the context of a server then we are trying to conntect a 
     * remote client for this server.
     */
    userType = (pdbcontext->usertype & DBSERVER)
        ? REMOTECLIENT
        : pdbcontext->usertype;

    /* wait for the login semaphore */
    ret = semLockLog(pcontext);
    if (ret)
        MSGD_CALLBACK(pcontext,"%Ldbenv3_1: semLockLog ret = %d",ret);

    /* For remote client connection, we assume that get best srvctl
     * has already incremented the server's usrcnt and therefore
     * we don't need to validate or increment it here.
     */

    /* get my very own usrctl structure */
    if ( !dbGetUsrctl(pcontext, userType) )
    {
        ret = semUnlkLog(pcontext);
        if (ret)
            MSGD_CALLBACK(pcontext,"%Ldbenv3_2: semUnlkLog ret = %d",ret);

        return 1;
    }

    pcontext->pusrctl->uc_logtime = time(0);

    MT_UNLK_USR();  /* it was locked inside dbGetUsrctl */

    ret = semUnlkLog(pcontext);
    if (ret)
        MSGD_CALLBACK(pcontext,"%Ldbenv3_3: semUnlkLog ret = %d",ret);

    MSGN_CALLBACK(pcontext, drMSG021,
                  XTEXT(pdbcontext, pcontext->pusrctl->uc_qusrnam),
                  XTEXT(pdbcontext, pcontext->pusrctl->uc_qttydev));
     pcontext->pusrctl->uc_loginmsg = 1;

    return 0;

}  /* end dbenv3 */



#if 0
/* PROGRAM: dbmovqnam - put name in shared mem
 *
 * RETURNS: pointer to the name in shared memory
 */
LOCALF TEXT *
dbmovqnam(
	dsmContext_t	*pcontext,
	TEXT	*pname,	/* the name to be moved */
	STPOOL	*ppool)	/* put the expanded name in this pool	 */
{
TEXT	*ptmp;
    ptmp = stGetd(pcontext, ppool, (unsigned)(stlen(pname) +1));
    stcopy(ptmp, pname);				/* move it in	 */
    return ptmp;

} /* end dbmovqnam */
#endif


/* PROGRAM: dbpubname - expand a name to full path name and put in shared mem
 *
 * RETURNS: pointer to the name in shared memory
 */

LOCALF TEXT *
dbpubname(
	dsmContext_t *pcontext,
	TEXT	*pname,	/* the relative file name to be expanded */
	STPOOL	*ppool)	/* put the expanded name in this pool	 */
{
	TEXT	*ptmp;
	TEXT	 buffer[MAXUTFLEN+1];

    ptmp = utapath(buffer, MAXUTFLEN, pname, (TEXT *) ""); /* full pathname */
#if 1 /* The following code was modified to avoid a SCO optimizer problem.
       * The calculation of (ptmp-buffer+1) returned the wrong value.
       * bug #96-01-24-036 and friends. (richb)
       */
    ptmp = stGetd(pcontext, ppool, (unsigned)(stlen(buffer)+1));
#else
    ptmp = stGetd(ppool, (unsigned)(ptmp-buffer+1));
#endif
    stcopy(ptmp, buffer);				/* move it in	 */
    return ptmp;

} /* end dbpubname */


/* PROGRAM: dbsvstuff -- save major global pointers in shared memory
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
dbsvstuff(
	dsmContext_t	*pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t *pdbshm = pdbcontext->pdbpub;

    /* misc values */
    pdbshm->numsvbuf = pdbcontext->argprbuf;
    pdbshm->numsems  = pdbcontext->psemdsc->numsems;
    pdbshm->qtmpmtctl= P_TO_QP(pcontext, pdbcontext->pmtctl);

} /* end dbsvstuff */


/* PROGRAM: dbldstuff -- load major global pointers from shared memory
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
dbldstuff(
	dsmContext_t	*pcontext)
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    dbshm_t	*pdbshm = pdbcontext->pdbpub;

    /* misc values */
    if (pdbcontext->usertype & DBSERVER)  /* server inherits -O from broker */
	pdbcontext->argprbuf = pdbshm->numsvbuf;
    pdbcontext->psemdsc->numsems = pdbshm->numsems;

    /* set private pointers */
    pdbcontext->pmtctl = XMTCTL(pdbcontext, pdbshm->qtmpmtctl);
    pdbcontext->pmstrblk = XMSTRBLK(pdbcontext, pdbshm->qmstrblk);
    pdbcontext->pbkctl = XBKCTL(pdbcontext, pdbshm->qbkctl);
    pdbcontext->prlctl = XRLCTL(pdbcontext, pdbshm->qrlctl);
    pdbcontext->paictl = XAICTL(pdbcontext, pdbshm->qaictl);
    pdbcontext->ptlctl = XTLCTL(pdbcontext, pdbshm->qtlctl);
    pdbcontext->plkctl = XLKCTL(pdbcontext, pdbshm->qlkctl);
    pdbcontext->p1usrctl = XUSRCTL(pdbcontext, pdbshm->q1usrctl);
    pdbcontext->ptrantab = XTRANTAB(pdbcontext, pdbshm->qtrantab);
    pdbcontext->ptrcmtab = XTRANTABRC(pdbcontext, pdbshm->qtrantabrc);
    pdbcontext->pseqctl = XSEQCTL(pdbcontext, pdbshm->qseqctl);
    pdbcontext->pwtoktbl = XTEXT(pdbcontext, pdbshm->qwtoktbl);
    pdbcontext->pomCtl = XOMCTL(pdbcontext, pdbshm->qomCtl);
    pdbcontext->p1crashTestAnchor = 
                         XCRASHANCHOR(pdbcontext, pdbshm->q1crashAnchor);
    pdbcontext->pidxstat =  XIDXPTR( pdbcontext, pdbshm->qindexarray);
    pdbcontext->ptabstat  = XTABLPTR(pdbcontext, pdbshm->qtablearray);



} /* end dbldstuff */


/* PROGRAM: pidofsrv -- get the pid of the server if remote client
 *                      or pid of itself if self-service
 *
 * RETURNS: pid
 */
int
pidofsrv(dsmContext_t *pcontext _UNUSED_, usrctl_t *pusr)
{
    return pusr->uc_pid;
} /* end pidofsrv */


/* PROGRAM: deadpid -- check if the process is dead
 *
 * RETURNS: 0 if it is alive
 *		else it is dead
*/
int
deadpid(
        dsmContext_t *pcontext,
	int           pid)
{
    int returnCode;

#if OPSYS!=WIN32API
    if (kill(pid, 0))
    {
        switch(errno)
        {
            case ESRCH:
                returnCode = 1;
                break;
            case EPERM:	/* Permission Denied */
                returnCode = 0;
                break;
            default:
                /* failed to check if broker alive, assume it is */
                MSGN_CALLBACK(pcontext, dbMSG002);
                returnCode = 0;
                break;
        }
    }
    else
    {
        /* no error from kill, means process is there */
        returnCode = 0;
    }

    return returnCode;

#else
    HANDLE procHandle;
    DWORD  exitCode;
    DWORD  lastError;

    /* try and open the process to get its status */
    if (procHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid))
    {
        if (GetExitCodeProcess(procHandle, &exitCode))
        {
            returnCode = (exitCode == STILL_ACTIVE) ? 0 : 1;
        }
        else
        {
            /* failed to check if broker alive, assume it is */
            MSGN_CALLBACK(pcontext, dbMSG002);
            returnCode = 0;
        }
        
        /* clean up the open handle */
        CloseHandle(procHandle);
    }
    else
    {
        /* couldn't open process, find out why */
        lastError = GetLastError();

        switch(lastError)
        {
            case ERROR_INVALID_PARAMETER:
                /* the pid doesn't exist on the system */
                returnCode = 1;
                break;
            case ERROR_ACCESS_DENIED:
                /* the pid exists, but we don't have permission to
                   interogate it */
                returnCode = 0;
                break;
            default:
                /* failed to check if broker alive, assume it is */
                MSGN_CALLBACK(pcontext, dbMSG002);
                returnCode = 0;
                break;
        }
    }

    return returnCode;

#endif /* not WIN32API */

} /* end deadpid */



/* PROGRAM: dbualloc - Allocate servers svctl and users usrctl tables
 *
 * RETURNS: 0 if successful
 *         -1 if error
 */
LOCALF int
dbualloc(
	dsmContext_t *pcontext)
{
	dbcontext_t *pdbcontext = pcontext->pdbcontext;
	dbshm_t	*pdbshm = pdbcontext->pdbpub;
	int	 nusr;
	STPOOL	*pdbpool;
	usrctl_t *pusr;
	int	 i;
        crashtest_t *pcrashtest;
        TEXT        pcrashmagic[BKCRASHNUMSIZE];
        int      crashVersion = 0;
	ULONG	 semset = 0;
	LONG	 semnum = 0;
	LONG	 maxSize = 0;
#if OPSYS==UNIX
    ULONG    numSemSets;
    semid_t *psemids;
#endif

    pdbpool = XSTPOOL(pdbcontext, pdbshm->qdbpool);
    nusr = pdbshm->argnusr;



    /* allocate user control array */

    /* check if there is enough space in one segment for all the users#*/
    if (pdbcontext->pshmdsc)
    {
	if ((LONG)sizeof(usrctl_t) * nusr > shmSegArea(pdbcontext))
	{   
	    MSGN_CALLBACK(pcontext, drMSG070);
	    /* A single sh mem segment is too small for all users */
	    return -1;
	}
    }

    pusr = (usrctl_t *) stGet(pcontext, pdbpool,
                              (unsigned)(sizeof(usrctl_t) * nusr));

    pdbshm->q1usrctl = P_TO_QP(pcontext, pusr);
    pdbcontext->p1usrctl = pusr;

    for(i = 0; i < nusr; i++, pusr++ )
    {
	QSELF(pusr) = P_TO_QP(pcontext, pusr);
	pusr->uc_usrnum = i;
#if OPSYS==UNIX
	pusr->uc_semid = -1;
#endif
    }

#if OPSYS==UNIX
    if (!pdbcontext->arg1usr)
    {
        /* get the maximum semaphore set size */
        for (i=0, psemids = pdbcontext->psemids; 
               (ULONG)i < pdbshm->numsemids; psemids++, i++)
        {
            if (maxSize < (LONG)psemids->size)
                maxSize = psemids->size;
        }

        /* put the appropiate semaphore id for every user */
        if (pdbshm->numsemids > 1)
        {
            semnum = 0;
            numSemSets = pdbshm->numsemids -1;
        }
        else
        {
            semnum = FRSTUSRSEM;
            numSemSets = 1;
        }
        semset = 0;

        /* start at the beginning */
        pusr = pdbcontext->p1usrctl;
        psemids = pdbcontext->psemids;
        for(i = 0; i < nusr;)
        {
#if 0
MSGD_CALLBACK(pcontext, "i = %d, semset = %d, semnum = %d, nusr = %d",
           i,semset,semnum,nusr);
#endif
 
            /* if we have room in this set, assign this user to the
               current value we are looking at */
            if (psemids->size > semnum)
            {
                pusr->uc_semnum = semnum;
                pusr->uc_semid = psemids->id;
 
                /* move onto the next user */
                i++;
                pusr++;
 
            }
            /* move on to the next semaphore set */
            semset++;
            psemids++;
 
            if (semset >= numSemSets)
            {
                semnum++;
                semset = 0;
                psemids = pdbcontext->psemids;
                if (semnum > maxSize)
                {
                    /* we've gone past the end of the largest semaphore set */
                    break;
                }
            }
        }
    }

#endif

    pdbcontext->p1crashTestAnchor = NULL; /* init for safety */

    /* Anchor the list of crashtest structures in dbcontext */
    /* Verify magic crash magic number before allocating storage */
    if (!utgetenvwithsize((TEXT*)"BKCRASHNUMBER",
                          pcrashmagic,
                          BKCRASHNUMSIZE))
    {
        crashVersion = atoin(pcrashmagic, stlen(pcrashmagic));
        if (crashVersion != BK_CRASHMAGIC)
        {
              /* none of our business - just return */
              return(0);
        }

        /* check if there is enough space in one segment for all the crash
           test structures */
        if (pdbcontext->pshmdsc)
        {
            if ((LONG)sizeof(crashtest_t) * nusr > shmSegArea(pdbcontext))
            {  
                MSGN_CALLBACK(pcontext, drMSG070);
                /* A single sh mem segment is too small for all users */
                return -1;
            }
        }
 
        /* allocate a crashtest structure for each user */
        pcrashtest = (crashtest_t *) stGet(pcontext, pdbpool,
                                  (unsigned)(sizeof(crashtest_t) * nusr));
 
        /* Anchor the list of crashtest structures in dbcontext */
        pdbshm->q1crashAnchor = (QCRASHANCHOR)P_TO_QP(pcontext, pcrashtest);
        pdbcontext->p1crashTestAnchor = pcrashtest;
 
        for(i = 0; i < nusr; i++, pcrashtest++ )
        {
            pcrashtest->cseed = SEEDDFLT;
            pcrashtest->probab = (float)0.1;
        }
    }


    return 0;

} /* end dbualloc */


/* PROGRAM: dbustatalloc - Allocate servers table and index arrays
 *
 * RETURNS:
 */
LOCALF int
dbustatalloc(dsmContext_t *pcontext)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        dbshm_t *pdbshm = pdbcontext->pdbpub;
        STPOOL  *pdbpool;
        dbshm_t *pdbpub;

        struct tablestat *ptabstat;
        struct indexstat *pidxstat;
        int      ntable;
        int      nindex;
 
    pdbpub  = pdbshm;
    pdbpool = XSTPOOL(pdbcontext, pdbshm->qdbpool);
 


    /* check if there is enough space in one segment for all the users#*/
    if (pdbcontext->pshmdsc)
    {
        if ( (pdbpub->argtablesize)* sizeof(struct tablestat ) +
             (pdbpub->argindexsize)* sizeof(struct indexstat )
             >  (ULONG)shmSegArea(pdbcontext) )
        {
            MSGN_CALLBACK(pcontext, drMSG070);
            /* A single sh mem segment is too small */
            return -1;
        }
    }

 
    if ( pdbpub->argtablesize != 0 )
    {
         ntable  = pdbpub->argtablesize;
         ptabstat = (struct tablestat *) stGet(pcontext, pdbpool,
               (unsigned) (sizeof(struct tablestat ) * ntable));
         pdbpub->qtablearray = (QTBLARR)P_TO_QP(pcontext, ptabstat);
         pcontext->pdbcontext->ptabstat = ptabstat;
    }
    else
    {
 
         pdbpub->qtablearray = 0; pcontext->pdbcontext->ptabstat = 0; ntable = 0;
    }
    if ( pdbpub->argindexsize != 0 )
    {
         nindex  = pdbpub->argindexsize;
         pidxstat = (struct indexstat *) stGet(pcontext, pdbpool,
               (unsigned)(sizeof(struct indexstat ) * nindex));
         pdbpub->qindexarray = (QIDXARR)P_TO_QP(pcontext, pidxstat);
         pcontext->pdbcontext->pidxstat  = pidxstat;
    }
    else
    {
          pdbpub->qindexarray = 0; pdbcontext->pidxstat = 0; nindex = 0;
    }
 
 
    return 0;

}  /* end dbustatalloc */



/* PROGRAM: dblk - open a .db file and lock it
 *
 * RETURNS: 0 = OK, non-0 = non-OK
 */
int
dblk(
        dsmContext_t *pcontext,	/* lock this database	    */
        int           lockmode,	/* DBLKSNGL or DBLKMULT	    */
        int           dispmsg)  /* if true, then display message */
{
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;	/* lock this database */
	dbshm_t	        *pdbshm     = pdbcontext->pdbpub;	
	int	  locked;
	TEXT	  dbname[MAXUTFLEN +1];
	int	  openmode = pdbcontext->argronly ? OPENREAD : OPENMODE;
        int       fileIndex;
        int       openReturn;
#if 0
        BKFDTBL *pbkfdtbl;
struct bkftbl   *pbkftbl;
#endif

    if(pdbcontext->pdatadir)
        utmypath(dbname,pdbcontext->pdatadir,pdbcontext->pdbname,
                 (TEXT *)".db");
    else
        /* convert the database name to a fully qualified path name */
        utapath(dbname, MAXUTFLEN, pdbcontext->pdbname, (TEXT *) ".db");

    /* length of the absolute pathname must be able to fit inside MAXUTFLEN */
    if (stlen(dbname) > MAXUTFLEN)
    {
	/* Absolute database name too long */
        MSGN_CALLBACK(pcontext, dbMSG016,
                     (int)stlen(dbname), (int)MAXUTFLEN, dbname);
	return(DB_EXBAD);
    }
	
    /* initialize the I/O table strcutures */
    fileIndex = bkInitIoTable(pcontext, 1);
#if 0
    pbkfdtbl = (bkfdtbl_t *)pdbcontext->pbkfdtbl;
    pbkftbl = (bkftbl_t *)utmalloc(sizeof(bkftbl_t));
    stnclr(pbkftbl, sizeof(bkftbl_t));
    pbkftbl->qself = P_TO_QP(pcontext, pbkftbl);
    pbkftbl->ft_qname = P_TO_QP(pcontext, dbname);
#endif

    /* conditionally lock the database, bkioCreateLkFile gives error if can't */
    if (!pdbcontext->argronly) /* if NOT read only database */
    {
	if ( (locked=bkioCreateLkFile(pcontext, lockmode, pdbcontext->pdbname,
                                      dispmsg))
              != 0 )
	{
	    if (locked==BKIO_LKSNGL) return(DB_EXSNGLUSE);
	    else
	    if (locked==BKIO_LKMULT) return(DB_EXSRVUSE);
	    else
            if (locked==BKIO_LKSTARTUP) return(BKIO_LKSTARTUP);
            else
	    return(DB_EXBAD);
	}
    }
    if (pdbshm->directio)
    {
       openReturn = bkioOpen (pcontext,
                              pdbcontext->pbkfdtbl->pbkiotbl[fileIndex],
                              NULL, OPEN_RELIABLE | AUTOERR,
                              (TEXT *)dbname,  openmode, DEFLT_PERM);
    }
    else
    {
      openReturn = bkioOpen ( pcontext,
                              pdbcontext->pbkfdtbl->pbkiotbl[fileIndex],
                              NULL, OPEN_SEQL | AUTOERR,
                              (TEXT *)dbname, openmode, DEFLT_PERM);
    }

    if (openReturn < 0)
    {
        if (pdbshm->directio)
        {
            bkioRemoveLkFile(pcontext, pdbcontext->pdbname);
        }
        else
        {
            bkioRemoveLkFile(pcontext, pdbcontext->pdbname);
        }
        bkioFreeIotbl(pcontext, pdbcontext->pbkfdtbl->pbkiotbl[fileIndex]);
	return DB_EXBAD;
    }

    pdbcontext->bklkmode = lockmode;

    /* save fileIndex entry for future use */
    pdbcontext->bklkfd = fileIndex;

    return 0;

} /* end dblk */



/* PROGRAM: dbSetOpen -- initialize the various database subsystems
 *
 *              [Function previously existed as dbseto in drdbsetc.c]
 *
 * RETURNS: 0   database fully opened.
 */
int
dbSetOpen(
        dsmContext_t    *pcontext,
        TEXT            *prunmode,      /* char string to go in .lg           */
        int             mode)           /* one or more of DBOPENFILE          */
                                        /* DBOPENDB DBSINGLE DBMULTI DBRDONLY */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    int              ret;
    mstrblk_t       *pmstr;
    TEXT             userId[256]; 

    stncop(pdbcontext->runmode, prunmode, RUNMODLEN-1);

    if (mode & DBOPENFILE)
    {
        if (!pdbcontext->argronly) /* if NOT read only database */
        if ( pdbcontext->dbcode == PROCODE ) /* And not a temp db */
        {
             
            /* open the database log file */
            ret = dbLogOpen(pcontext, pdbcontext->pdbname);
            if (ret < 0)
                return ret;
        }

        /* open up the block manager */
        ret = bkOpen(pcontext, mode);
        if (ret) return ret;

        if ( !((mode & DBCONTROL) && (mode & DBRDONLY)) )
        {

           pmstr = pdbcontext->pmstrblk;

           /* if read-only and bi not truncated, warn) */
           if (pdbcontext->argronly && pmstr->mb_bistate != BITRUNC)
           {
               /* bi file not truncated */
               MSGN_CALLBACK(pcontext, drMSG113, pdbcontext->pdbname);
           }

           /* if the the database is void, complain */
           if (  !(pdbcontext->usertype & DBUTIL)
               && pmstr->mb_dbstate & DBDSMVOID)
           {   
               MSGN_CALLBACK(pcontext, drMSG001, pdbcontext->pdbname);
               return -1;
           }

           /* if the the database has failed restore, complain */

           if (pmstr->mb_dbstate & DBRESTORE)
           {
               MSGN_CALLBACK(pcontext, drMSG026, pdbcontext->pdbname);
               pmstr->mb_tainted |= DBRESTORE;
           }
           if (pmstr->mb_buseq == 0) pmstr->mb_buseq = pmstr->BKINCR;

           if (pmstr->mb_buseq < pmstr->BKINCR)
           {
               /*  the last backup failed or was cancelled */

               /* the last backup was not completed */
               MSGN_CALLBACK(pcontext, drMSG114);
           }

           /* if the the database is in a middle of index build*/
           if ( (pmstr->mb_dbstate == DBIXRPR) &&
                !cxIxrprGet() /* to allow index repair to continue */ )
           {
               FATAL_MSGN_CALLBACK(pcontext, biFTL019 );
           }
           else
           {
               /* see if the database requires index rebuild before continuing */
               if ( (pmstr->mb_tainted & DBIXBUILD) &&
                     !cxIxrprGet() /* allow index repair to continue */ )
               {
                   /* Open failed, database requires rebuild of all indexes */
                   MSGN_CALLBACK(pcontext, drMSG129, pdbcontext->pdbname);
                   return -1;
               }
           }

           /* if the database was in the middle of character or version convert */
           if (pmstr->mb_dbstate & DBCONVCH || pmstr->mb_dbstate & DBCONVDB)
           {
               /* Open failed, database unusable due to partial conversion. */
               MSGN_CALLBACK(pcontext, drMSG046);
               return -1;
           }

       }

        /* indicate that session is started */
        if (pdbcontext->bklkmode == DBLKMULT)
        {
             MSGN_CALLBACK(pcontext, drMSG012 );
#if 0
            /* if the -rptint parameter is set then print out
             * the interval to the log file
             */

            if (pdbcontext->pdbpub->dblic.reportInterval != -1)
                MSGN_CALLBACK(pcontext, drMSG269,
                              pdbcontext->pdbpub->dblic.reportInterval);
#endif
        }
        else
        if (!pdbcontext->argronly && pdbcontext->dbcode == PROCODE )
            /* dont print msg if RO or temp db cause no .lg file */
            MSGN_CALLBACK(pcontext, drMSG020, prunmode,
                 utuid(userId, sizeof(userId)),
                 XTEXT(pdbcontext, pcontext->pusrctl->uc_qttydev));
        pdbcontext->dbopnmsg = 1;
    }
    if (mode & DBOPENDB)
    {
        /* open the bi manager and do crash recovery */

        if (!pdbcontext->argronly && pdbcontext->dbcode == PROCODE)
            /* if NOT read only and not temp database */
        {
            if (rlseto(pcontext, mode)) return -1;
            lkinit(pcontext);   /* initialize the lock table */
        }

        /* Get the language and userfile status */
        pmstr = pdbcontext->pmstrblk;
        /* load the sequence generator cache */

        if (seqInit (pcontext)) return (-1);
    }
    else if (mode & DBCONTROL)
    {
        if (!pdbcontext->argronly) /* if NOT read only database */
        if ( pdbcontext->dbcode == PROCODE ) /* And not a temp db */
        {
             
            /* open the database log file */
            ret = dbLogOpen(pcontext, pdbcontext->pdbname);
            if (ret < 0)
                return ret;
        }

        /* bug 19990330-016 J. Meleedy, just open db and don't update
           extent headers or put anything in the .lg. v8/v9 compatibility */
        /* open up the block manager */
        ret = bkOpen(pcontext, mode);
        if (ret) return ret;

    }

    return 0;

}  /* end dbSetOpen */

#define  INITIALBLKS  2

/* PROGRAM: dbCreateDB - Procedure to create a gemini database
 *
 *
 * RETURNS: DSMVOID
 *
 */
LOCALF DSMVOID
dbCreateDB(
         dsmContext_t    *pcontext)
{
    dbcontext_t             *pdbcontext = pcontext->pdbcontext;

TEXT *pblockBuffer;

INTERN  extentBlock_t extentBlock;

INTERN  mstrblk_t mstrblk;

INTERN  objectBlock_t objectBlock;


    UCOUNT       areaRecbits;
    fileHandle_t fd          = INVALID_FILE_HANDLE;
    bkfdtbl_t    *pbkfdtbl;
    bkftbl_t     *pbkftbl;
    LONG         retWrite;  /* return for utWriteToFile() */
    int          errorStatus;
    int          bytesWritten;
    TEXT         dbnamebuf[MAXPATHN+1];
    int          dbBlockSize;
    time_t       now;
    
    dbBlockSize = BLKSIZE;
    pdbcontext->pdbpub->dbblksize = dbBlockSize;

    pdbcontext->pdbpub->recbits = DEFAULT_RECBITS;
 
    areaRecbits = pdbcontext->pdbpub->recbits;
    
    pblockBuffer = utmalloc(dbBlockSize);
    stnclr(pblockBuffer, dbBlockSize);

    utmypath (dbnamebuf, pdbcontext->pdatadir,
              pdbcontext->pdbname,
              (TEXT *)".db");  

#if OPSYS == WIN32API
    fd = utOsOpen((TEXT *)dbnamebuf,
                                (O_CREAT | O_EXCL | O_RDWR),
                                CREATE_DATA_FILE, 0, &errorStatus);

#else
    fd = utOsOpen((TEXT *)dbnamebuf,
                                (O_CREAT | O_EXCL | O_RDWR),
                                CREATE_DATA_FILE, 0, &errorStatus);
#endif


    if (fd == INVALID_FILE_HANDLE)
      FATAL_MSGN_CALLBACK(pcontext, utFTL007, dbnamebuf, errorStatus);

    /* this is a hanlde on NT and needs to be converted to a int/fd */

    /* initialize the I/O table strcutures */
    pdbcontext->bklkfd = bkInitIoTable(pcontext, 1);
    pbkfdtbl = (bkfdtbl_t *)pdbcontext->pbkfdtbl;
    pbkftbl = (struct bkftbl *)utmalloc(sizeof (struct bkftbl));
    stnclr(pbkftbl, sizeof(struct bkftbl));
    pbkftbl->qself = P_TO_QP(pcontext, pbkftbl);
    pbkftbl->ft_qname = P_TO_QP(pcontext, pdbcontext->pdbname);
    pbkftbl->ft_fdtblIndex = pdbcontext->bklkfd;

    /* by saving the fd here, we get the pbkftbl pointer linked into the
       pbkiotbl strcuture.  The fd needs to be a handle for NT, otherwise
       bkio calls will fail */
    bkioSaveFd (pcontext, pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                pdbcontext->pdbname, fd,BUFIO);

    
    /* create masterblk and index anchor table block in database*/

    stnclr(&extentBlock, sizeof(extentBlock_t));
    extentBlock.BKDBKEY  = 0;
    extentBlock.BKTYPE   = EXTHDRBLK;
    extentBlock.BKFRCHN  = NOCHN;
    extentBlock.BKINCR   = 1;

    /* get current date and time for time stamps */
    now = time ( (time_t *)0 );

    extentBlock.dateVerification = 1;
    extentBlock.creationDate[0]   = now;
    extentBlock.lastOpenDate[0]   = now;
    extentBlock.creationDate[1]   = now;
    extentBlock.lastOpenDate[1]   = now;
    
    extentBlock.extentType       = DBMSTR;
    extentBlock.dbVersion        = DBVERSN;
    extentBlock.blockSize        = dbBlockSize;

    BK_TO_STORE(&extentBlock.bk_hdr);
    /* Create Master block as block 1 */

    stnclr(&mstrblk, sizeof(mstrblk_t));
    mstrblk.BKDBKEY  = (LONG) 1 << areaRecbits;
    mstrblk.BKTYPE   = MASTERBLK;
    mstrblk.BKFRCHN  = NOCHN;
    mstrblk.BKINCR   = 1;

    mstrblk.mb_dbvers        = DBVERSN;
    mstrblk.mb_blockSize     = dbBlockSize;
    mstrblk.mb_dbstate       = DBCLOSED;
    mstrblk.mb_bistate       = BITRUNC;
    mstrblk.mb_biblksize     = dbBlockSize;
    mstrblk.mb_rlclsize      = 32;    /* 128 kb clusters for bi file */ 
    mstrblk.mb_shmid         = -1;
    mstrblk.mb_semid         = -1;
    
    BK_TO_STORE(&mstrblk.bk_hdr);
    /* Create Object block as block 2 */

    stnclr(&objectBlock, sizeof(objectBlock_t));
    objectBlock.BKDBKEY  = (LONG) 2 << areaRecbits;
    objectBlock.BKTYPE   = OBJECTBLK;
    objectBlock.BKFRCHN  = NOCHN;
    objectBlock.BKINCR   = 1;

    objectBlock.totalBlocks  = INITIALBLKS;
    objectBlock.hiWaterBlock = INITIALBLKS;
    
    BK_TO_STORE(&objectBlock.bk_hdr);
    
    bufcop(pblockBuffer, &extentBlock, sizeof(extentBlock_t));

    retWrite = utWriteToFile(fd, (gem_off_t)0, pblockBuffer, dbBlockSize,
                             &bytesWritten, &errorStatus); 
    stnclr(pblockBuffer, sizeof(extentBlock_t));

    bufcop(pblockBuffer, &mstrblk, sizeof(mstrblk_t));
    retWrite = utWriteToFile(fd, (gem_off_t)dbBlockSize, pblockBuffer, dbBlockSize,
                             &bytesWritten, &errorStatus);
    stnclr(pblockBuffer, sizeof(mstrblk_t));

    bufcop(pblockBuffer, &objectBlock, sizeof(objectBlock_t));
    retWrite = utWriteToFile(fd, (gem_off_t)(dbBlockSize*2), pblockBuffer, dbBlockSize,
                             &bytesWritten, &errorStatus);
    stnclr(pblockBuffer, sizeof(objectBlock_t));

    /* need to close this file descriptio otherwise windows stat will fail */
    bkioClose(pcontext, pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 0);

}  /* end dbCreateDB */

/* PROGRAM: dbCreateDB2 - Create area/extent records for newly created db.
 *
 */
DSMVOID
dbCreateDB2(dsmContext_t *pcontext)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmObject_t    objectNumber;
    dsmDbkey_t     rootBlock;
    dsmStatus_t    rc;
    bkftbl_t *     pbkftbl;
    dsmArea_t      areaNumber = DSMAREA_CONTROL;

    /* Create root block for area index             */
    pdbcontext->inservice++;
    objectNumber = cxCreateIndex(pcontext, DSMINDEX_UNIQUE, SCIDX_AREA,
                                       areaNumber, SCTBL_AREA, &rootBlock);
    pdbcontext->inservice--;
    if (objectNumber != SCIDX_AREA)
    {
        MSGD_CALLBACK(pcontext,"%gFailed to create root block for area index");
    }
    pdbcontext->pmstrblk->areaRoot = rootBlock;
   
    /* Create control area record                         */
    rc = dbAreaRecordCreate(pcontext, 0,
                            areaNumber, (TEXT *)"Control Area",
                            DSMAREA_TYPE_DATA,
                            0, 0, 0,
                            pdbcontext->pdbpub->dbblksize,
                            pdbcontext->pdbpub->recbits, 0);

    /* Create root block for the area/extent index  */
    pdbcontext->inservice++;
    objectNumber = cxCreateIndex(pcontext, DSMINDEX_UNIQUE, SCIDX_EXTAREA,
                                       areaNumber, SCTBL_EXTENT, &rootBlock);
    pdbcontext->inservice--;
    if (objectNumber != SCIDX_EXTAREA)
    {
        MSGD_CALLBACK(pcontext,"%gFailed to create root block for area index");
    }
    pdbcontext->pmstrblk->areaExtentRoot = rootBlock;
    
    /* Create area/extent record for the .db file   */
    pbkftbl = (bkftbl_t *)NULL;
    rc = bkGetAreaFtbl(pcontext, DSMAREA_CONTROL, &pbkftbl);

    rc = dbExtentRecordCreate(pcontext, areaNumber, 1,
                                       0, 0, BKUNIX | BKDATA,
                                      XTEXT(pdbcontext,pbkftbl->ft_qname));

    /* Create area for the recovery log      */
    rc = dbAreaCreate(pcontext,pdbcontext->pdbpub->dbblksize,
                       DSMAREA_TYPE_BI,DSMAREA_BI,0,
                       (TEXT *)"Recovery Log");
    

    /* Create the extent for the recovery log       */
    
    rc = dbExtentCreate(pcontext, DSMAREA_BI, 1, 1, BKBI,
                         (TEXT *)"gemini.rl");
    
    /* Create root block for storageObject table             */
    pdbcontext->inservice++;
    objectNumber = cxCreateIndex(pcontext, DSMINDEX_UNIQUE, SCIDX_OBJID,
                                       areaNumber, SCTBL_OBJECT, &rootBlock);
    pdbcontext->inservice--;
    if (objectNumber != SCIDX_OBJID)
    {
        MSGD_CALLBACK(pcontext,"%gFailed to create root block for area index");
    }
    pdbcontext->pmstrblk->mb_objectRoot = rootBlock;

    /* Create storageObject record for the storageObject table */
    /* Object record for: Object record mapping */
    buildObjectRecord(pcontext, DSMAREA_CONTROL,
                            SCTBL_OBJECT,
                            (dsmObjectType_t)DSMOBJECT_MIXINDEX,
                            (dsmObject_t)SCIDX_OBJID,
                            DSMOBJECT_MIXTABLE, 0,
                            0, 0,
                            0, &rootBlock);
    /* Object record for: Obejct-Id index mapping */
    buildObjectRecord(pcontext, DSMAREA_CONTROL,
                            SCIDX_OBJID,
                            (dsmObjectType_t)DSMOBJECT_MIXTABLE,
                            (dsmObject_t)SCTBL_OBJECT,
                            DSMOBJECT_MIXINDEX, DSMINDEX_UNIQUE,
                            0, rootBlock,
                            0, &rootBlock);

    bmFlushx(pcontext, FLUSH_ALL_AREAS, 1);
    
    bmFlushMasterblock(pcontext);

    return;
}

/* PROGRAM: buildObjectRecord
 *
 *
 * RETURNS:
 */
LOCALF int
buildObjectRecord(
        dsmContext_t    *pcontext,
        dsmArea_t        areaNumber,
        dsmObject_t      objectNumber,
        dsmObjectType_t  objectAssociateType,
        dsmObject_t      objectAssociate,
        dsmObjectType_t  objectType,
        dsmObjectAttr_t  objectAttr,
        dsmDbkey_t       objectBlock,
        dsmDbkey_t       objectRoot,
        ULONG            objectSystem, /* not currently used */
        dsmDbkey_t      *pobjectRootBlock)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t    *pusr   = pcontext->pusrctl;
    dsmStatus_t  returnCode = 0;
    ULONG        recSize;
    svcByteString_t fieldValue;
 
    TEXT        *pobjectRecord = 0;
    lockId_t     lockId;
 
    AUTOKEY(keyStruct, 2*KEYSIZE )
 
    svcDataType_t   component[4];
    svcDataType_t  *pcomponent[4];
    COUNT           maxKeyLength = 2*KEYSIZE;

    pobjectRecord = (TEXT *)stRent(pcontext, pdbcontext->privpool, MAXRECSZ);
 
    returnCode = DSM_S_FAILURE;
    /* materialize _Object template record */
    recSize = recFormat(pobjectRecord, MAXRECSZ, SCFLD_OBJNAME);
    if (recSize > MAXRECSZ)
        goto done;
 
    /* Format misc field */

    returnCode = recInitArray(pobjectRecord, MAXRECSZ/2,  &recSize, 
			      SCFLD_OBJNAME, 8 );
    if ( returnCode != RECSUCCESS)
        goto done;
    /* 1st field is ALWAYS table number */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   1, (ULONG)0, SCTBL_OBJECT, 0) )
        goto done;
 
    /* set _Db-recid */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                SCFLD_OBJDB, (ULONG)0, pdbcontext->pmstrblk->mb_dbrecid, 0) )
        goto done;
 
    /* set _Object-type */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJTYPE, (ULONG)0, objectType, 0) )
        goto done;
 
    /* set _Object-number */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJNUM, (ULONG)0, objectNumber, 0) )
        goto done;
 
    /* set _Object-associate-type    */
    if(recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                  SCFLD_OBJATYPE,(ULONG)0, objectAssociateType,0))
        goto done;
   
    /* set _Object-associate */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJASSOC,(ULONG)0, objectAssociate, 0) )
        goto done;

    /* set _Area-number */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJAREA, (ULONG)0, areaNumber, 0) )
        goto done;
 
    /* set _Object-block */
    if (recPutBIG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJBLOCK, (ULONG)0, objectBlock, 0) )
        goto done;
 
    /* set _Object-root */
    if (recPutBIG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJROOT, (ULONG)0, objectRoot, 0) )
        goto done;
 
    /* set _Object-attrib */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJATTR, (ULONG)0, (int)objectAttr, 0) )
        goto done;
 
    /* set _Object-system */
    if (recPutLONG(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJSYS, (ULONG)0, objectSystem, 0) )
        goto done;
 
    fieldValue.pbyte = (TEXT *)" ";
    fieldValue.size = stlen(fieldValue.pbyte);
    fieldValue.length = stlen(fieldValue.pbyte);

    /* set _Object-name */
    if (recPutBYTES(pobjectRecord, MAXRECSZ, &recSize,
                   SCFLD_OBJNAME, (ULONG)0, &fieldValue, 0) )
        goto done;
 
    recSize--;  /* omit the ENDREC from the recSize */
    /**** Write out the Object record ****/
    pdbcontext->inservice++;
    lockId.dbkey = rmCreate(pcontext, DSMAREA_SCHEMA, SCTBL_OBJECT,
                            pobjectRecord, (COUNT)recSize,
                            (UCOUNT)RM_LOCK_NEEDED);
    pdbcontext->inservice--;

    /* area records are ALWAYS in the schema area */
    lockId.table = SCTBL_OBJECT;

    /* _Object record generation completed
     *
     * Now index the record just added.
     */
    /**** Build the SCIDX_OBJID index for the _Object Table ****/

    keyStruct.akey.area         = DSMAREA_SCHEMA;
    keyStruct.akey.root         = *pobjectRootBlock;
    keyStruct.akey.unique_index = DSMINDEX_UNIQUE;
    keyStruct.akey.word_index   = 0;
    
    keyStruct.akey.index    = SCIDX_OBJID;
    keyStruct.akey.keycomps = 4;
    keyStruct.akey.ksubstr  = 0;

    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];
    pcomponent[2] = &component[2];
    pcomponent[3] = &component[3];

    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = (LONG)pdbcontext->pmstrblk->mb_dbrecid;
 
    component[1].type            = SVCLONG;
    component[1].indicator       = 0;
    component[1].f.svcLong       = (LONG)objectType;

    component[2].type            = SVCSMALL;
    component[2].indicator       = 0;
    component[2].f.svcSmall      = objectAssociate;

    component[3].type            = SVCSMALL;
    component[3].indicator       = 0;
    component[3].f.svcSmall      = objectNumber;

    returnCode = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                  &keyStruct.akey);
    if (returnCode)
    {
        returnCode = DSM_S_FAILURE;
        goto done;
    }
 
    pdbcontext->inservice++; /* keep signal handler out */
      pusr->uc_task = 1;  /* fake out cxAddNL */
      returnCode = cxAddEntry(pcontext, &keyStruct.akey, &lockId, PNULL, 
                              objectAssociate);
      pusr->uc_task = 0;
    pdbcontext->inservice--;
 
done:
   if (pobjectRecord)
     stVacate(pcontext, pdbcontext->privpool, pobjectRecord);
 
    return returnCode;
 
}  /* end buildObjectRecord */
 

/* PROGRAM: dbGetPrime - get a prime number close to the specified value
 *
 * RETURNS: ULONG
 */
ULONG
dbGetPrime (ULONG n)
{
        ULONG   *p;
 
/* prime number table for setting defauls for buffer pool and lock table
   hash parameters. The table is approximately a geometric progression
   using the ratio 1 to 1.4. The zero marks the end of the table */
 
INTERN ULONG drprimes[] =
{   13,    17,    19,    23,    29,    31,    43,    61,    83,   113,
   163,   229,   317,   449,   631,   887,  1237,  1733,  2437,  3407,
  4759,  6661,  9337, 13063, 18289, 25621, 35863, 50207, 70289, 98407,
     0 };


    for (p = &(drprimes[0]); ; p++)
    {
        /* search the table till we find the first number larger than
           the one requested */
 
        if (*p >= n) break;
        if (*p == 0)
        {
            /* off the end of the table, return the last one */
 
            p--;
            break;
        }
    }    
    return (*p);

}  /* end dbGetPrime */


/* PROGRAM: dbCheckParameters - Check and adjust start up parameters related
 *                              to shared memory
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
dbCheckParameters (dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;

    ULONG       lowHash; /* smallest value from dbGetPrime() */
 
/* Check if -charset tag has been provided, if not set default */

   if (pdbshm->brokerCsName[0] == '\0')
   {
      stcopy(pdbshm->brokerCsName,(TEXT *) "iso8859-1"); 
   }

/* TODO: this checking is done for the 4GL in drargs:drcdbargs() */
/*       it should be removed from drcdbargs()                   */
 
   /* make sure a valid buffer size has been provided */
    if (pdbshm->argbkbuf == 0)
    {
        /* if single-user use default, else calculate based on # clients */
        pdbshm->argbkbuf = (pdbcontext->arg1usr ? 100 :
                             (pdbshm->argnclts * DFTBUFPU));
    }
 
    /* if argbkbuf is less than minimum, reset it to the minimum */
    pdbshm->argbkbuf = ((pdbshm->argbkbuf < MINBUFS) ? MINBUFS :
                                                pdbshm->argbkbuf);
 
    /* if argbkbuf is more than the maximum, reset it to the maximum */
    pdbshm->argbkbuf = ((pdbshm->argbkbuf > MAXBUFS) ? MAXBUFS :
                                                pdbshm->argbkbuf);
 
    /* make sure a valid has argument has been provided */
    if (pdbshm->arghash == 0)
    {
        /* buffer pool hash was not specified so we pick a prime number
           approximately the number of buffers / 4 as a default */
        pdbshm->arghash = dbGetPrime ((ULONG)(pdbshm->argbkbuf / 4));
    }
    else
    {
        /* make sure the user did not specify a bad value */
       lowHash = dbGetPrime((ULONG)0);
       pdbshm->arghash = ((pdbshm->arghash) > lowHash ?
                                       pdbshm->arghash : lowHash);
    }

    if (pdbshm->lockWaitTimeOut == 0)
    {
        pdbshm->lockWaitTimeOut = 10;
    }
 
    if (pdbshm->arglkhash == 0)
    {
        /* lock table hash was not specified so we pick a prime number
           approximately the number of locks / 8 as a default */
        pdbshm->arglkhash = dbGetPrime ((ULONG)(pdbshm->arglknum / 8));
    }
    else
    {
        /* make sure the user did not specify a bad value */
        lowHash = dbGetPrime((ULONG)0);
        pdbshm->arglkhash = ((pdbshm->arglkhash < lowHash) ? lowHash :
                                        pdbshm->arglkhash);
    }
 
    /* if there are less users than semsets, set semsets to match
       the number of users */
    if ((ULONG)pdbshm->argnusr < pdbshm->numsemids)
    {
        pdbshm->numsemids = (ULONG)pdbshm->argnusr;
    }

    return;

}  /* end dbCheckParameters */


/* PROGRAM: dbWriteLoginMessage - Write login message to lg file
 *                              
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
dbWriteLoginMessage (
        dsmContext_t  *pcontext,
        TEXT          *puname,    /* ptr to user name */
        TEXT          *pttyname)  /* ptr to terminal name */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t    *pusr = pcontext->pusrctl;

    if (pusr->uc_loginmsg == 1) return;

    /* Need to put Login message in the .lg file */

    if ( ((!pdbcontext->arg1usr) && (pdbcontext->usertype & SELFSERVE)) ||
         (pdbcontext->usertype == (DBUTIL+DBSHUT)) )
    {
        /* Issue a login for the quiet point requestor and set logout */

        /* JFJ - This is wrong, DBUTIL is reserved for single user
           access to the db. DBSHUT is reserved for shutdown
           requests only. */
        if (pdbcontext->usertype == (DBUTIL+DBSHUT))
        {
            /* Quiet point request login by user %s on %s */

            MSGN_CALLBACK(pcontext, drMSG389, puname, pttyname);
        }
        else
        {
            /* regular Login message */
            MSGN_CALLBACK(pcontext, drMSG021, puname, pttyname);
        }
        /* Remember that we wrote it. On Logout, we use this to check
           if we should generate a logout message */

        pusr->uc_loginmsg = 1;
    }

}  /* end dbWriteLoginMessage */
