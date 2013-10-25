
 /* Copyright (C) 2000 by NuSphere Corporation
 *
 * This is an unpublished work.  No license is granted to use this software. 
 *
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"

#include "bkioprv.h"
#include "bkmsg.h"
#include "dbpub.h"
#include "dsmpub.h"
#include "svmsg.h"
#include "utcpub.h"
#include "utepub.h"
#include "utfpub.h"
#include "utospub.h"
#include "utspub.h"
#include "utmsg.h"
#include "dbcontext.h"
#include "bkiopub.h"

#include <fcntl.h>
#include <errno.h>

#if OPSYS==UNIX
#include <unistd.h>
#include <signal.h>
#endif

#if OPSYS==WIN32API
#include <io.h>
#endif

/* The following static variable was approved by richb. It should only
 * be used by dbenv1 processing and only 1 thread should be use it.
 * If this is not the case, then we have other problems. This variable
 * holds the handle for the database lock (.lk) file and allows us
 * to use file locking on NT as a means of establishing the current
 * validity of the file.
*/

typedef struct lockstr
{
    fileHandle_t lkHandle;  /* Handle to the .lk file */
    UCOUNT nameCrc;   /* CRC value of the abs path of the name */
} lockstr_t;

LOCAL lockstr_t lkstr[240] = {
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
};

/* PROGRAM: bkioCreateLkFile -- lock the database in desired mode.
 *
 *    Create a .lk file holding the following information:
 *      a.) The mode the db is in BKIO_LKSNGL | BKIO_LKMULT
 *      b.) The process ID of the starter of the db
 *      c.) The hostname of the machine where the db was started
 *
 *
 * RETURNS:
 *	0		database locked in requested mode.
 *	BKIO_LKSNGL	database currently locked in single-user mode,
 *			request denied.
 *	BKIO_LKMULT	database currently locked in multi-user mode,
 *			request denied.
 *      BKIO_LKSTARTUP  database is in the process of starting up
 *                      request denied.
 *
 */
int
bkioCreateLkFile (
        dsmContext_t    *pcontext,
        int		 mode,		/* desired usage mode, either
		            		   BKIO_LKSNGL or BKIO_LKMULT */
        TEXT		*dbname,	/* name of database */
        int             dispmsg)	/* if true, then display a message */

{

    TEXT	 namebuf[MAXPATHN+1];
    TEXT	 dbnamebuf[MAXPATHN+1];
    TEXT	 nodebuf[BKIO_NODESZ];
    int		 ret = 0;
    ULONG	 spid = 0;
    int          errorStatus = 0;
    int          bytesWritten;
    LONG         retWrite;
    LONG         retMakePath = 0;
    int		 lockret = 1; 
    int		 wmode = mode;
    fileHandle_t lkhandle;
    int          i;
    int          local = 0;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    /* see if the *.lk file already exists and is valid */
    if(pdbcontext->pdatadir)
       utmypath(namebuf,pdbcontext->pdatadir,dbname,
                (TEXT *)".lk");
    else
        retMakePath = utMakePathname(namebuf, sizeof(namebuf), dbname, 
                                     (TEXT *)".lk");
    if (retMakePath != DBUT_S_SUCCESS)
    {
        /* Report error */
        dbUserError(pcontext, retMakePath, errno, 0,
                    (DSMVOID *)NULL, (DSMVOID *)NULL);
    }
    if(pdbcontext->pdatadir)
        utmypath(dbnamebuf,pdbcontext->pdatadir,dbname,
                (TEXT *)".db");
    else
        retMakePath = utMakePathname(dbnamebuf, sizeof(dbnamebuf), dbname, 
                                     (TEXT *)".db");
    if (retMakePath != DBUT_S_SUCCESS)
    {
        /* Report error */
        dbUserError(pcontext, retMakePath, errno, 0,
                    (DSMVOID *)NULL, (DSMVOID *)NULL);
    }

    ret = bkioTestLkFile(pcontext, dbname);

    if (ret)
    {   
        switch(ret) 
        {
	    case BKIO_LKSNGL:
	        if(dispmsg)
	             MSGN_CALLBACK(pcontext, bkMSG005, dbname);
	        break; /* db in use single-user */
	    case BKIO_LKMULT:
	    case BKIO_LKSTARTUP:
	        if(dispmsg)
	 	     MSGN_CALLBACK(pcontext, bkMSG006, dbname);
	        break; /* db in use multi-user */
	    case BKIO_LKNOHOST:
	         MSGN_CALLBACK(pcontext, bkMSG007, namebuf);
	         MSGN_CALLBACK(pcontext, bkMSG008);
                 MSGN_CALLBACK(pcontext, bkMSG009);
                 MSGN_CALLBACK(pcontext, bkMSG010);
	         MSGN_CALLBACK(pcontext, bkMSG011, namebuf); break;
            case BKIO_LKTRUNC:
                break;  /* message was put out by bkioTestLkFile */
	    default:	 FATAL_MSGN_CALLBACK(pcontext, bkFTL009, dbname); break;
	  }
    }
    else
    {	
        /* open failed, try to creat it */
        /* 19990525-028 - small window of opportunity for two brokers    */
        /* to start on same database. Replace utOsCreat call with        */
        /* utOsOpen.                                                     */
        /* lkhandle = utOsCreat((TEXT *)namebuf, 0444, 0, &errorStatus); */
      
        /* 19990525-028 second try. Can only call utOsOpen if not on NFS */
        /*              See bug for details.                             */

        local = bkioIsLocalFile(pcontext,dbnamebuf);

        /* if .lk file is on NFS drive */
        if (local == 0)
        {
            lkhandle = utOsCreat((TEXT *)namebuf, 0444, 0, &errorStatus); 
        }
        else
        {
#if OPSYS == WIN32API
            lkhandle = utOsOpen((TEXT *)namebuf,
                                (O_CREAT | O_EXCL | O_WRONLY), 
                                CREATE_DATA_FILE, 0, &errorStatus);

#else
            lkhandle = utOsOpen((TEXT *)namebuf,
                                (O_CREAT | O_EXCL | O_WRONLY), 
                                0444, 0, &errorStatus);
#endif
        }

        if (lkhandle == INVALID_FILE_HANDLE) 
        {
            /* cant open */
            MSGN_CALLBACK(pcontext, bkMSG002, namebuf, errorStatus);
            return -1;
        }

        /* Remove this conditional once file locking is implemented for UNIX */
#if OPSYS == WIN32API
        /* attempt to get a lock in the .lk file itself */
        lockret = utLockLkFile(lkhandle);
#endif

        /* if we got the lock, write out the mode, pid and nodename */
        if (lockret)
        {
            if (mode == BKIO_LKMULT)
            {
                wmode = BKIO_LKSTARTUP;
            }
        
            /* write out the mode the database is in */
            retWrite = utWriteToFile(lkhandle, 0, (TEXT *)&wmode,
                                 sizeof(int), &bytesWritten, &errorStatus);

            /* get the process id and put that into the file */
            spid = utgetpid ();
            retWrite = utWriteToFile(lkhandle, sizeof(int), (TEXT *)&spid,
                                 sizeof(int), &bytesWritten, &errorStatus);

            stnclr(nodebuf,BKIO_NODESZ); /*fill with nulls*/
    
            /* get the hostname and put it into the file */
            ret = utgethostname((TEXT *)nodebuf,BKIO_NODESZ - 1);
            if (ret < 0)
            {
                /* if the utgethostname call failed, write out an ampty
                   hostname, that will cause the .lk file checking to be 
                   more strict
                */
                stnclr(nodebuf,BKIO_NODESZ); /*fill with nulls*/
            }
            retWrite = utWriteToFile(lkhandle,2*(sizeof(int)),nodebuf, 
                              BKIO_NODESZ, &bytesWritten, &errorStatus);

            /* Save the handle in the lkstr structure */
            for (i = 0; i < 240; i++)
            {
                if (lkstr[i].lkHandle == (fileHandle_t)0)
                {
                    /* we found an empty slot, populate it */
                    lkstr[i].lkHandle = lkhandle;
                    lkstr[i].nameCrc = 
                          calc_crc((UCOUNT)0, namebuf, (COUNT)stlen(namebuf));
                    break;
                }
            }

        }
        else
        {
            /* we did no succeed in locking the .lk file */
	    utOsClose(lkhandle,0);
            MSGN_CALLBACK(pcontext, bkMSG131, namebuf, errorStatus);
            return -1;
        }
    }
    return(ret);
}

/* PROGRAM: bkioRemoveLkFile -- unlock the database, version via UNIX 
 *                              locking call.
 *		Given a file descriptor for the database
 *		(".db") file, unlock the database if it is locked.
 *
 * RETURNS: DSMVOID
 */

DSMVOID
bkioRemoveLkFile (
        dsmContext_t    *pcontext,
	TEXT		*dbname)	/* name of database */
{
        TEXT	 namebuf[MAXPATHN+1];
        LONG     retMakePath = 0;
        int      lockret;
        UCOUNT   nameCrc;
        int      i;
        dbcontext_t *pdbcontext = pcontext->pdbcontext;

    /* delete the dbname.lk file */

    if(pdbcontext->pdatadir)
        utmypath(namebuf,pdbcontext->pdatadir,dbname,
                (TEXT *)".lk");
    else
        retMakePath = utMakePathname(namebuf, sizeof(namebuf), dbname, 
                                    (TEXT *)".lk");
    if (retMakePath != DBUT_S_SUCCESS)
    {
        /* Report error */
        dbUserError(pcontext, retMakePath, errno, 0,
                    (DSMVOID *)NULL, (DSMVOID *)NULL);
    }

    nameCrc = calc_crc((UCOUNT)0, namebuf, (COUNT)stlen(namebuf));

    for (i = 0; i < 240; i++)
    {
        if ((lkstr[i].nameCrc == nameCrc) && 
            (lkstr[i].lkHandle != (fileHandle_t)0) &&
            (lkstr[i].lkHandle != INVALID_FILE_HANDLE) )
        {
            /* we found the handle */ 

            lockret = utUnlockLkFile(&lkstr[i].lkHandle);
            lkstr[i].nameCrc = 0;
            break;
        }
    }

#if OPSYS==UNIX
    unlink((const char *)namebuf);
#endif
#if OPSYS==WIN32API
    lockret = DeleteFile(namebuf);
#endif
}

/* PROGRAM: bkioTestLkFile -- examine the database to see if it is locked, and
 *		if so return the mode, i.e. single-user or multi-user.
 *		This version uses a special file, ".lk", instead of the
 *		UNIX locking call.
 *
 * RETURNS:	Returns 0 if the database is not locked, else returns
 *		BKIO_LKSNGL or BKIO_LKMULT.
 */
int
bkioTestLkFile (
        dsmContext_t    *pcontext,
	TEXT		*dbname)	/* path name for ".db" file */
                                          

{
     TEXT		namebuf[MAXPATHN+1];
#if OPSYS == UNIX
     TEXT		nodebuf[BKIO_NODESZ];
     TEXT		mynode[BKIO_NODESZ];
#endif
     LONG		retRead;
     ULONG		spid = 0;
     int		lkmode = 0;
     int		errorStatus = 0;
     int                bytesRead;
     LONG               retMakePath = 0;
     fileHandle_t       lkhandle;
     int                lockret = 0;

    /* form path name for *.lk file, see if it's there and current */
    if(pcontext->pdbcontext->pdatadir)
        utmypath(namebuf,pcontext->pdbcontext->pdatadir,
                         pcontext->pdbcontext->pdbname,
                         (TEXT *)".lk");
    else 
        retMakePath = utMakePathname(namebuf, sizeof(namebuf), dbname, 
                                    (TEXT *)".lk");
    if (retMakePath != DBUT_S_SUCCESS)
    {
        /* Report Error */
        dbUserError(pcontext, retMakePath, errno, 0,
                    (DSMVOID *)NULL, (DSMVOID *)NULL);
    }

#if OPSYS == WIN32API
    lkhandle = utOsOpen(namebuf, OPEN_RW, DEFLT_PERM, OPEN_SEQL, &errorStatus);
#else
    lkhandle = utOsOpen(namebuf, OPEN_R, DEFLT_PERM, OPEN_SEQL, &errorStatus);
#endif

    if (lkhandle == INVALID_FILE_HANDLE)
    {	
        /* open failed, either db not in use, or permission screwed up */
	if (errorStatus != ENOENT)
            FATAL_MSGN_CALLBACK(pcontext, svFTL013, namebuf, errorStatus);
    }
    else 
    {
        /* read lock mode and server pid out of lock file */
	/* the following loop takes care of a timing window that exists
	*  when the server creates the .lk file, and the client attempts
	*  to read the file before the server has written the lock mode
	*  and its pid into the file. This loop should also handle interrupted
	*  system call under Unix, and hence no special case for EINTR
	*/

        /* Remove this conditional once file locking is implemented for UNIX */
#if OPSYS == WIN32API
        /* Try to lock the .lk file */
        lockret = utLockLkFile(lkhandle);
#endif
   
        /* We got a lock, so the .lk file is invalid. Now unlock so it 
         * can be overwritten.
        */ 
        if (lockret)
        {
            lockret = utUnlockLkFile(&lkhandle);
            /* 19990525-028 - If we are going to change behavior such   */
            /* that the .lk file is only created if it does not already */
            /* exist, then if the .lk file is invalid we must delete it */
            bkioRemoveLkFile(pcontext, dbname);
            return (0);
        } 
        else
        {
            /* The .lk file exists, we can't lock it, so it is valid */
	    retRead = utReadFromFile(lkhandle, 0, (TEXT *)&lkmode,
                                 sizeof (int), &bytesRead, &errorStatus);

	    if ( bytesRead != sizeof(int))
            {
	        MSGN_CALLBACK(pcontext, svMSG005);
                return BKIO_LKTRUNC;
            }

#if OPSYS == UNIX
	    /* get PID and HOST name from file-- if its the same HOST as we
               are on, it is then safe to test the PID and erase .lk if its
	       not a live process on this same machine and allow login.

	       if the HOSTS are different, or gethostname() is not supported 
               on this machine, the PID cannot be guaranteed to be valid 
               because of NFS so return BKIO_LKNOHOST */

	    retRead = utReadFromFile(lkhandle, sizeof(int), (TEXT *)&spid,
                                 sizeof(int), &bytesRead, &errorStatus);
	    if ( spid )
	    {
	        stnclr(mynode,BKIO_NODESZ);

	        utgethostname((TEXT *)mynode,BKIO_NODESZ -1);
    
	        retRead = utReadFromFile(lkhandle, 2 * sizeof(int), 
                            (TEXT *)nodebuf, BKIO_NODESZ, &bytesRead, 
                            &errorStatus);
	        if ( bytesRead == BKIO_NODESZ)
                {
		    if (nodebuf[0] == '\0') /* No hostname in the .lk file */
                    {
                        return BKIO_LKNOHOST;
                    }
		    if (stpcmp(nodebuf,mynode) == 0) /*see if nodes match*/
		    {   
                        if ( (kill ( spid, 0 ) == -1) && (errno == ESRCH) )
                        {
			    lkmode= 0;
                        }
		    }
                }
	    }

#endif /* UNIX */
        }
    }

    if (lkhandle != INVALID_FILE_HANDLE)
    {
        utOsClose(lkhandle, OPEN_SEQL);
        if (lkmode == 0)
        {
            bkioRemoveLkFile(pcontext, dbname);
        }
    }
    return(lkmode);
}

/* PROGRAM: bkioUpdateLkFile - update the .lk file
 *
 * Update the .lk file with the NON-NULL parameters passed to the 
 * routine.  This is used to revalidate the .lk file incase it's 
 * creator has disappeared.  An example of this is during Emergency
 * Shutdown, the first thing that happens is that the broker dies
 * hence the .lk is now "stale".
 *
 * RETURNS: 0 - success
 *         -1 - failure
 */
int
bkioUpdateLkFile(
        dsmContext_t *pcontext,
        int           mode,		/* mode database is opened in */
        int           pid,		/* pid of the owner */
        TEXT         *phost,		/* hostname */
        TEXT         *dbname)         /* name of database */
{
    LONG	    ret;
    LONG            retMakePath = 0;
    int		    errorStatus = 0;
    int		    bytesWritten;
    TEXT            namebuf[MAXPATHN+1];
    int             lockret;
    UCOUNT          nameCrc;
    fileHandle_t    lkhandle = (fileHandle_t) -1;
    int i;

    /* get the absolute path of the .lk file */
    if(pcontext->pdbcontext->pdatadir)
        utmypath(namebuf,pcontext->pdbcontext->pdatadir,
                 pcontext->pdbcontext->pdbname,
                 (TEXT *)".lk");
    else
        retMakePath = utMakePathname(namebuf, sizeof(namebuf), dbname,
                                                 (TEXT *)".lk");
    if (retMakePath != DBUT_S_SUCCESS)
    {
        /* Report error */
        dbUserError(pcontext, retMakePath, errno, 0,
                    (DSMVOID *)NULL, (DSMVOID *)NULL);
    }

    nameCrc = calc_crc((UCOUNT)0, namebuf, (COUNT)stlen(namebuf));

    /* find the handle for the lkfile */
    for (i = 0; i < 240; i++)
    {
        if ((lkstr[i].nameCrc == nameCrc) && 
            (lkstr[i].lkHandle != (fileHandle_t)0))
        {
            lkhandle = lkstr[i].lkHandle;
            break;
        }
    }

    /* If we were not called from a process that knew the lock file 
     * handle, go get it.
    */
    if (lkhandle == (fileHandle_t) -1)
    {
        lkhandle = utOsOpen((TEXT *)namebuf, OPEN_RW, DEFLT_PERM,
                           OPEN_SEQL, &errorStatus);
    
        if (lkhandle == INVALID_FILE_HANDLE)
        {
            /* failed to open the .lk file, return the failure */
            MSGN_CALLBACK(pcontext, bkMSG100, namebuf, errorStatus);
            return -1;
        }

        /* we are going to end up owning this lk file, save the handle
           for future use. */
        for (i = 0; i < 240; i++)
        {
            if (lkstr[i].lkHandle == (fileHandle_t)0)
            {
                /* we found an empty slot, populate it */
                lkstr[i].lkHandle = lkhandle;
                lkstr[i].nameCrc = calc_crc((UCOUNT)0, namebuf, 
                                            (COUNT)stlen(namebuf));
                break;
            }
        }
    }
#if OPSYS==UNIX
    else
    {
        /* we are going to close this file at the end so we 
           need to make sure every knows that */
        lkstr[i].lkHandle = (fileHandle_t) -1;
    }
#endif

    /* If we get the lock, then we were called by brkillall, watchdog,
     * or wdogLkFileCheck (after it was determined that the broker was gone).
     * If we don't get a lock, then we were called by nsaloop during startup
     * and we need to change the mode from DBLKSTARTUP to DBLKMULT.
    */
    lockret = utLockLkFile(lkhandle);

    if (mode)
    {
        /* update the mode with the passed one */

        ret = utWriteToFile(lkhandle, 0, (TEXT *)&mode,sizeof(int),
                            &bytesWritten, &errorStatus);
        if (ret < 0)
        {
            /* failed to write mode to the .lk file, return the failure */
            MSGN_CALLBACK(pcontext, bkMSG099,namebuf,errorStatus);
            return -1;
        }
 
    }
    
    if (pid)
    {
        /* update the pid with the passed one */
        ret = utWriteToFile(lkhandle, sizeof(int), (TEXT *)&pid,sizeof(int),
                            &bytesWritten, &errorStatus);
        if (ret < 0)
        {
            /* failed to seek to pid in the .lk file, return the failure */
            MSGN_CALLBACK(pcontext, bkMSG099,namebuf,errorStatus);
            return -1;
        }
    }
    
    if (phost)
    {
        /* update the hostname with the passed one */
        ret = utWriteToFile(lkhandle,2 * sizeof(int), phost,stlen(phost),
                            &bytesWritten, &errorStatus);
        if (ret < 0)
        {
            /* failed to write hostname to the .lk file, return the failure */
            MSGN_CALLBACK(pcontext, bkMSG099, namebuf, errorStatus);
            return -1;
        }
    }

    /* if we got here, time to close up shop and return success */
#if OPSYS == UNIX
    utOsClose (lkhandle, OPEN_SEQL);
#endif

    return 0;
    
}

/* PROGRAM: bkioVerifyLkFile - verify that the .lk file is valid with the
 *                           passed in pid.
 *
 *
 * RETURNS: 0 - lk is valid
 *         -1 - lk is invalid
 */
int
bkioVerifyLkFile(
        dsmContext_t *pcontext,
        int           pid,		/* .lk creators pid */
        TEXT         *dbname)		/* basename for the .lk file */
{
    LONG	  ret;	                /* return for utReadFromFile() */
    int		  status =0;		/* value returned */
    int		  errorStatus = 0;
    TEXT          namebuf[MAXPATHN+1];
    TEXT          nodebuf[BKIO_NODESZ];
    TEXT          snodebuf[BKIO_NODESZ];
    int           spid = 0;
    LONG          retMakePath = 0;
    int           bytesRead;
    int           lockret = 0;
    int           needopen = 0;
    UCOUNT        nameCrc;
    fileHandle_t  lkhandle = (fileHandle_t) -1;
    int           i;
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;

    if(pdbcontext->pdatadir)
        utmypath(namebuf,pdbcontext->pdatadir,dbname,
                (TEXT *)".lk");
    else
        retMakePath = utMakePathname(namebuf, sizeof(namebuf), dbname, 
                                     (TEXT *)".lk");
    if (retMakePath != DBUT_S_SUCCESS)
    {
        /* Report Error */
        dbUserError(pcontext, retMakePath, errno, 0,
                    (DSMVOID *)NULL, (DSMVOID *)NULL);
    }

    nameCrc = calc_crc((UCOUNT)0, namebuf, (COUNT)stlen(namebuf));

    /* find the handle for the lkfile */
    for (i = 0; i < 240; i++)
    {
        if ((lkstr[i].nameCrc == nameCrc) && 
            (lkstr[i].lkHandle != (fileHandle_t)0))
        {
            lkhandle = lkstr[i].lkHandle;
            break;
        }
    }

    /* if we don't have a valid file handle, open the lock file and get one */
    if (lkhandle == (fileHandle_t) -1)
    {
        /* note that we needed to open the lock file */
        needopen = 1;
    
        /* get the absolute path of the .lk file */

        lkhandle = utOsOpen((TEXT *)namebuf, OPEN_R, DEFLT_PERM, OPEN_SEQL,
                                                            &errorStatus);
    
        if (lkhandle == INVALID_FILE_HANDLE)
        {
            /* failed to open the .lk file, return the failure */
            MSGN_CALLBACK(pcontext, bkMSG100,namebuf,errorStatus);
            return -1;
        }
    }

    /* Try to lock the lock file - if we get a lock, then the .lk file
     * is not valid.
    */
    

/* Remove this conditional once file locking is implemented for UNIX */
#if OPSYS == WIN32API
    lockret = utLockLkFile(lkhandle);
#endif

    if (lockret)
    {
        /* we got the lock so the old .lk file must have been invalid */
        MSGN_CALLBACK(pcontext, bkMSG132, namebuf, errorStatus);
        return -1;
    }
    
    /* now lets read the .lk file */
    /* read the past the mode from the .lk file */
    ret = utReadFromFile(lkhandle, 0, (TEXT *)&spid, sizeof(int),
                         &bytesRead, &errorStatus);
    if (bytesRead != sizeof(int))
    {
        /* read failed.  Either the file is too short, or read gave an
           error.  Regardless, we need to shutdown */
        MSGN_CALLBACK(pcontext, bkMSG101,"MODE",namebuf,errorStatus);
        utOsClose(lkhandle,OPEN_SEQL);
        return -1;
    }

    /* read the pid from the .lk file */
    ret = utReadFromFile(lkhandle, sizeof(int), (TEXT *)&spid,sizeof(int),
                         &bytesRead, &errorStatus);
    if (bytesRead != sizeof(int))
    {
        /* read failed.  Either the file is too short, or read gave an
           error.  Regardless, we need to shutdown */
        MSGN_CALLBACK(pcontext, bkMSG101,"PID",namebuf,errorStatus);
        utOsClose(lkhandle,OPEN_SEQL);
        return -1;
    }

    /* read the hostname from the .lk file */
    ret = utReadFromFile(lkhandle, 2 * sizeof(int), snodebuf,BKIO_NODESZ,
                         &bytesRead, &errorStatus);
    if (bytesRead != BKIO_NODESZ)
    {
        /* read failed.  Either the file is too short, or read gave an
           error.  Regardless, we need to shutdown */
        MSGN_CALLBACK(pcontext, bkMSG101,"HOSTNAME",namebuf,errorStatus);
        utOsClose(lkhandle,OPEN_SEQL);
        return -1;
    }

    /* get the systems hostname don't worry about failures, the 
       stpcmp will verify the host for us later */
    ret = utgethostname((TEXT *)nodebuf,BKIO_NODESZ - 1);

    /* now that we have read the .lk file, verify it's a valid one */
    if (pid == spid)
    {
        /* pids match, now check the hostname */
        if (stpcmp(nodebuf,snodebuf) != 0)
        {
            /* hostnames don't match.  .lk file is invalid for this server */
            MSGN_CALLBACK(pcontext, bkMSG102,namebuf,snodebuf,nodebuf);
            status = -1;
        }
    }
    else
    {
        /* pids don't match.  .lk file is invalid for this server */
        MSGN_CALLBACK(pcontext, bkMSG103,namebuf,spid,pid);
        status = -1;
    }

    if (needopen)
    {
        utOsClose (lkhandle, OPEN_SEQL);
        lkhandle = (fileHandle_t) -1;
    }
    return(status);
}

