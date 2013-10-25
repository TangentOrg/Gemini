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
#define _STDIO_INCLUDED
#include <errno.h>
#include <sys/types.h>

#if OPSYS==UNIX
#include <unistd.h>
#endif

#include "dbcontext.h"
#include "bkcrash.h"
#include "bkpub.h"

/* This turns on the function pointer  array */
#define BKIOFUNCTIONS
#include "bkioprv.h"

#include "stmpub.h"
#include "stmprv.h"
#include "utcpub.h"
#include "utfpub.h"
#include "utmsg.h"
#include "utospub.h"    /* for utOsClose */
#include "utspub.h"
#include "bkiopub.h"

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#define BKIO_MAX	32
#define BKIO_RETRY	25

#define BKIOSTAT(x,y)	(x->y++)


/*** LOCAL FUNCTION PROTOTYPES ***/

LOCALF int bkioOSerror (dsmContext_t *pcontext,
                        TTINY *,TTINY *,int, fileHandle_t, int,LONG,TTINY *);

/*** END LOCAL FUNCTION PROTOTYPES ***/



/* PROGRAM:  bkioClose - closes all file desc associated with this file
 *
 * RETURNS: 0 on SUCCESS, else FAILURE
 */
int
bkioClose(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* the bkiotbl sructure */
        UCOUNT        needSync _UNUSED_)   	   
{
   
    int rc,ret = 0;

    if (!pbkiotbl)
    {
        return 0;
    }

    if (pbkiotbl->buffered_filePointer)
    {
        rc = utOsFlush(pbkiotbl->buffered_filePointer);
        if (rc)
            ret = rc;
        rc = utOsClose(pbkiotbl->buffered_filePointer,0 );
        if (rc)
            ret = rc;
    }
 
    if ((pbkiotbl->unbuffered_filePointer) &&
         (pbkiotbl->unbuffered_filePointer != pbkiotbl->buffered_filePointer))
    {
        rc = utOsClose(pbkiotbl->unbuffered_filePointer, 0);
        if (rc)
            ret = rc;
    }

    /* make sure we don't leave any old pointers hanging around */
    bkioFreeIotbl(pcontext, pbkiotbl);

    return ret;

}  /* end bkioClose */

/* PROGRAM:  bkioFlush - Flush all OS buffers for the file
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkioFlush( dsmContext_t *pcontext _UNUSED_,
           bkiotbl_t    *pbkiotbl)   /* the bkiotbl sructure */
{
    int rc;

    if (!pbkiotbl)
    {
        return;
    }

    if (pbkiotbl->buffered_filePointer)
    {
        rc = utOsFlush(pbkiotbl->buffered_filePointer);
    }
    return;
 
}  /* end bkioFlush */


/* PROGRAM:  bkioCopyBufferedToUnbuffered - copies the buffered info in a 
 *           bkiotbl structure to the unbuffered slots.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkioCopyBufferedToUnbuffered(
        dsmContext_t *pcontext _UNUSED_,
        bkiotbl_t    *pbkiotbl)   /* I/O tables structure */
{
    pbkiotbl->unbuffered_filePointer = pbkiotbl->buffered_filePointer;
    pbkiotbl->unbufwrite = pbkiotbl->bufwrite;
    pbkiotbl->unbufread = pbkiotbl->bufwrite;

    return;

}  /* end bkioCopyBufferedToUnbuffered */


/* PROGRAM:  bkioFreeIotbl - clear iotbl information associated with this file
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkioFreeIotbl(
        dsmContext_t *pcontext _UNUSED_,
        bkiotbl_t    *pbkiotbl) 
{
    /* if we don't have a iotbl entry, then there's nothing to worry about */
    if (!pbkiotbl)
    {
        return;
    }

    /* now null out any freed pointers */
    pbkiotbl->buffered_filePointer = 0;
    pbkiotbl->unbuffered_filePointer = 0;
    
    return;

}  /* end bkioFreeIotbl */


/* PROGRAM:  bkioGetFd - returns a valid file descriptor 
 * 
 * RETURNS:  file descriptor on success
 *           -1 on failure
 */
fileHandle_t
bkioGetFd(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,
        LONG          mode)  /* The bkiotbl sructure */
{
    fileHandle_t badReturnCode = INVALID_FILE_HANDLE;

    if (!pbkiotbl)
    {
        /* I/O table not initialized. */
        MSGN_CALLBACK(pcontext, bkMSG064);
        return badReturnCode;
    }

    /* we're not sure if we have a buffered or unbuffered fd, so
       try the buffered first, then the unbuffered */

    if ((mode == BUFIO) &&
        (pbkiotbl->buffered_filePointer != INVALID_FILE_HANDLE) )
        return pbkiotbl->buffered_filePointer;

    else if ((mode == UNBUFIO) &&
             (pbkiotbl->unbuffered_filePointer != INVALID_FILE_HANDLE) )
        return pbkiotbl->unbuffered_filePointer;

    else
        return badReturnCode;

}  /* end bkioGetFd */


/* PROGRAM:  bkioGetIoTable - creates storage for an IO table structure
 *
 * RETURN: pointer to bkiotbl structure
 *         NULL on failure
 */
bkiotbl_t *
bkioGetIoTable(
        dsmContext_t  *pcontext,
        bkio_stpool_t *pool)	/* pool to get storage from */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        dbshm_t *pdbpub = pdbcontext->pdbpub;
	bkiotbl_t *pbkiotbl = (bkiotbl_t *)0;

        /* get storeage for the bkio structure for this file */
        pbkiotbl = (bkiotbl_t *)stGet(pcontext, pool, sizeof(bkiotbl_t));

        if (pbkiotbl == (bkiotbl_t *)0)
        {
            /* stGet failed (is this possible?) return null */
            return pbkiotbl;
        }

        /* make sure we have clean memory */
        stnclr((TEXT *)pbkiotbl, sizeof(bkiotbl_t));
 
        /* load the slot with the appropriate functions */
        if (pdbpub->directio)
        {
            pbkiotbl->unbufwrite = *bkioFunctions[BKIO_WRITE_UNBUFFERED];
            pbkiotbl->unbufread  = *bkioFunctions[BKIO_READ_UNBUFFERED];
            pbkiotbl->bufwrite   = *bkioFunctions[BKIO_WRITE_UNBUFFERED];
            pbkiotbl->bufread    = *bkioFunctions[BKIO_READ_UNBUFFERED];
        }
        else
        {
            pbkiotbl->bufwrite   = *bkioFunctions[BKIO_WRITE_BUFFERED];
            pbkiotbl->bufread    = *bkioFunctions[BKIO_READ_BUFFERED];
            pbkiotbl->unbufwrite = *bkioFunctions[BKIO_WRITE_UNBUFFERED];
            pbkiotbl->unbufread  = *bkioFunctions[BKIO_READ_UNBUFFERED];
        }

        pbkiotbl->file_open   = *bkioFunctions[BKIO_OPEN];

        return pbkiotbl;

}  /* end bkioGetIoTable */


/* PROGRAM:  bkioIsLocalFile - Determine if the passed file is a on
 *           a local file system or on a remote file system.
 *
 *           The expects the absolute path.
 *
 * RETURNS: 1 if the file is local
 *          0 if the file is remote
 *         -1 on error
 */
int
bkioIsLocalFile(
        dsmContext_t *pcontext,
        TEXT         *pfullpath)	/* Absolute path of the file */
{
    TEXT dirname[MAXPATHN+1];	/* Path name of the parent directory */
    int  ret;			/* ret code from platform specific routine */

    if (pcontext->pdbcontext->znfs)
    {
        /* some modules, like proshut, dont care about this and should not
           print the warning messages. Also, the -z3 switch can be used
           internally to shut off the check so the qa test results will
           compare properly */

        return (1);
    }


    /*
     * Make sure that the path length does not exceed the MAXPATHN size
     * I will make this a fatal error, since the max length is abused in
     * other places and eventually causes a crash.  
     *
     */
    if ( stlen(pfullpath) > MAXPATHN ) {

      MSGN_CALLBACK(pcontext,utMSG004 );      

      return ( -1 );
    }

    /* get the parent directory name */
    stnclr(dirname, MAXPATHN+1);

    bufcop(dirname,pfullpath, stlen(pfullpath) );
    utfpath(dirname);

    /* call the platform specific routine to see if the path or directory
       is on an NFS mounted disk
    */
    ret = bkioTestLocalFile(pcontext, pfullpath, dirname);

    return (ret);

}  /* end bkioIsLocalFile */


/* PROGRAM:  bkioOSerror - Print an appropiate error based on errno 
 *
 * RETURNS: 0 if syscall should be retried 
 *         -1: if syscall should fail
 */
LOCALF int
bkioOSerror (
        dsmContext_t *pcontext,
        TTINY        *pcall,	/* callers name */
        TTINY        *psyscall,	/* system call that failed */
        int           terrno,	/* errno from failed system call */
        fileHandle_t  fd,	/* file descriptor call failed on */
        int           len,	/* lenght for the system call */
        LONG          offset,	/* offset for the system call */
        TTINY        *pname)	/* name of the file that errored */
{
    int		ret = 0; 

    switch (terrno)
    {
    case 0:             /* errno 0 means out of disk space */
        /* Insufficient disk space during call %s, fd %i, len %l, offset %l */
        MSGN_CALLBACK(pcontext, bkMSG053, pcall, psyscall,
                      fd, (LONG)len, offset, pname);

        ret = 0;          /* retry this system call */
        break;

#ifdef EBADF
    case EBADF:          /* bad file descriptor */

        /* %s:Bad file descriptor was used during call, fd, len, offset */
        MSGN_CALLBACK(pcontext, bkMSG049, pcall, psyscall,
                      fd, len, offset, pname);

        ret = -1;         /* This error will not improve with a retry */
        break;
#endif

#ifdef EINVAL
    case EINVAL:          /* invalid argument */

        /* %s:Invalid argument during call %s, fd %i, len %l, offset %l */
        MSGN_CALLBACK(pcontext, bkMSG050, pcall, psyscall,
                      fd, len, offset, pname);

        ret = -1;         /* This error will not improve with a retry */
        break;
#endif

#ifdef EDQUOT
    case EDQUOT:          /* Disk quota exceeded */

        /* %s:Disk quota exceeded during call %s, fd %i, len %l, offset %l */
        MSGN_CALLBACK(pcontext, bkMSG051, pcall, psyscall,
                      fd, len, offset, pname);

        ret = 0;          /* retry this system call */
        break;
#endif

#ifdef EFBIG
    case EFBIG:           /* Maximum file size exceeded */

        /* Maximum file size exceeded during call, fd, len, offset */
        MSGN_CALLBACK(pcontext, bkMSG052, pcall, psyscall,
                      fd, len, offset, pname);

        ret = 0;          /* retry this system call */
        break;
#endif

#ifdef ENOSPC
    case ENOSPC:          /* Insufficient disk space */

        /* Insufficient disk space during call %s, fd %i, len %l, offset %l */
        MSGN_CALLBACK(pcontext, bkMSG053, pcall, psyscall,
                      fd, len, offset, pname);

        ret = 0;          /* retry this system call */
        break;
#endif

#ifdef E_DISKFULL
    case E_DISKFULL:      /* Insufficient disk space */

        /* Insufficient disk space during call %s, fd %i, len %l, offset %l */
        MSGN_CALLBACK(pcontext, bkMSG053, pcall, psyscall,
                      fd, len, offset, pname);

        ret = 0;          /* retry this system call */
        break;
#endif


#ifdef ENMFILE
    case ENMFILE:         /* Insufficient disk space */

        /* Insufficient disk space during call %s, fd %i, len %l, offset %l */
        MSGN_CALLBACK(pcontext, bkMSG053, pcall, psyscall,
                      fd, len, offset, pname);

        ret = 0;          /* retry this system call */
        break;
#endif

#ifdef EINTR
    case EINTR:           /* Interrupted system call */
			  /* just return */
        ret = 0;          /* retry this system call */
        break;
#endif

    default:
        /* incase we didn't fall into any of the above messages,
           print a message anyways.
        */

        /* Unknown O/S error during call %s, errno %i, fd %i, len %l, offset */
        MSGN_CALLBACK(pcontext, bkMSG054, pcall, psyscall, terrno,
                      fd, len, offset, pname);
        break;

    }

    return (ret);

}  /* end bkioOSerror */


/* PROGRAM:  bkioRead - Read a database block
 *
 * This is the ONLY program that should be used for database I/O!
 *
 * The program will read from the disk into the passed buffer by the 
 * method available for either BUFFERED or UNBUFFERED I/O.
 * 
 * RETURNS:  Numnber of blocks read on success
 *           -1 on failure
 */
LONG
bkioRead(
        dsmContext_t *pcontext,
        bkiotbl_t   *pbkiotbl,	  /* bkiotbl pointer */
        bkiostats_t *pbkiostats, /* pointer for io statistics */
        gem_off_t    offset,	  /* offset in BLOCKS */
        TEXT        *pbuf,	  /* pointer to the I/O buffer */
        int          size,	  /* size in BLOCKS */
        int          blklog,	  /* used to convert from BLOCKS to bytes */
        int          buffmode)	  /* I/O Method to use */
{
        LONG	        blocksRead;
        int	        retryCount;
        LONG            (*readfunc)(BKIO_PROTOTYPE);     /* function writes */
        fileHandle_t     readfd;

        /* check for a NULL pointer */
        if (pbuf == PNULL)
        {
            /* bkioRead:Bad address passed addr */
            MSGN_CALLBACK(pcontext, bkMSG043,pbuf);
            return -1;
        }

        /* if bkioInit() hasn't been called yet, then complain */
        if (!pbkiotbl)
        {
            /* bkioRead: I/O table not initialized index %d<num>. */
            MSGN_CALLBACK(pcontext, bkMSG062, pbkiotbl);
            return -1;
        }

#if OPSYS==VMS	/* VMS only handles UNBUFFERED */
	buffmode = UNBUFFERED;
#endif
        if (buffmode == BUFFERED)
        {
	    /* setup call and file descriptor for buffered writing */
	    readfunc = pbkiotbl->bufread;
	    readfd = pbkiotbl->buffered_filePointer;
        }
        else if (buffmode == UNBUFFERED)
        {
	    /* setup call and file descriptor for unbuffered writing */
	    readfunc = pbkiotbl->unbufread;
	    readfd = pbkiotbl->unbuffered_filePointer;
        }
        else
        {
            /* bkioRead: Unknown I/O mode specified %d<num>. */
            MSGN_CALLBACK(pcontext, bkMSG063, buffmode, pbkiotbl->fname);
            return  -1;
        }

        /* if we made it this far, we are ready to perform the write */

        /* 
           Since the size can be larger than 1 block, we should put a maximum
           limit on how much we write in one chunk.  There may be some limits
           to what an OS will allow for 1 write call.
        */

        retryCount = BKIO_RETRY;

        while (retryCount > 0)
        {
            blocksRead = (*readfunc)(pcontext,
                                     pbkiotbl,
                                     pbkiostats,
                                     offset,
                                     pbuf, 
                                     size,
                                     blklog);

            if (blocksRead == size)
            {
                break;
            }
            else    /* we wrote an unexpected amount of data */
            {
	        if ((bkioOSerror(pcontext, (TTINY *)"bkioRead",
                    (TTINY *)"Read", 
                    errno, readfd, size << blklog, offset << blklog, 
                    pbkiotbl->fname) < 0))
	        {
	            /* the error was bad enough to give up now! */
	            retryCount = 0;
	        }

	        /* if the error is worth retrying */
		retryCount--;
    
            }
        }

        return blocksRead;

}  /* end bkioRead */


/* PROGRAM:  bkioUpdateCounters - adds the counters and puts the reults
 *           into the first structure passed
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkioUpdateCounters(
        dsmContext_t *pcontext _UNUSED_,
        bkiostats_t  *ptarget,
        bkiostats_t  *psource) 
{
    ptarget->bkio_bufwrites += psource->bkio_bufwrites;
    ptarget->bkio_unbufwrites += psource->bkio_unbufwrites;
    ptarget->bkio_bufreads += psource->bkio_bufreads;
    ptarget->bkio_unbufreads += psource->bkio_unbufreads;

    return;

}  /* end bkioUpdateCounters */


/* PROGRAM:  bkioWrite - Write a database block
 *
 * This is the ONLY program that should be used for database I/O!
 *
 * The program will write the passed buffer to the disk by the 
 * method available for either BUFFERED or UNBUFFERED I/O.
 * 
 * RETURNS:  Numnber of blocks written on success
 *           -1 on failure
 */
LONG
bkioWrite(
        dsmContext_t *pcontext,
        bkiotbl_t   *pbkiotbl,   /* bkiotbl pointer */
        bkiostats_t *pbkiostats, /* pointer for io statistics */
        gem_off_t    offset,	   /* offset in BLOCKS */
        TEXT        *pbuf,	   /* pointer to the I/O buffer */
        int          size,	   /* size in BLOCKS */
        int          blklog,	   /* used to convert from BLOCKS to bytes */
        int	       buffmode)   /* I/O Method to use */
{
        LONG	    blocksWritten;
        int	    retryCount;
        LONG        (*writefunc)(BKIO_PROTOTYPE);     /* function writes */
        fileHandle_t writefd;
        dbcontext_t *pdbcontext = pcontext->pdbcontext;

        /* check for a NULL pointer */
        if (pbuf == PNULL)
        {
            /* bkioWrite:Bad address passed addr */
            MSGN_CALLBACK(pcontext, bkMSG043,pbuf);
            return -1;
        }

        /* if bkioInit() hasn't been called yet, then complain */
        if (!pbkiotbl)
        {
            /* bkioWrite: called before io table initialized */
            MSGN_CALLBACK(pcontext, bkMSG060, pbkiotbl );
            return -1;
        }

#if OPSYS==VMS	/* VMS only handles UNBUFFERED */
	buffmode = UNBUFFERED;
#endif

        if (buffmode == BUFFERED)
        {
	    /* setup call and file descriptor for buffered writing */
	    writefunc = pbkiotbl->bufwrite;
	    writefd = pbkiotbl->buffered_filePointer;
        }
        else if (buffmode == UNBUFFERED)
        {
	    /* setup call and file descriptor for unbuffered writing */
	    writefunc = pbkiotbl->unbufwrite;
	    writefd = pbkiotbl->unbuffered_filePointer;
        }
        else
        {
            /* bkioWrite: Unknown I/O mode passed %d */
            MSGN_CALLBACK(pcontext, bkMSG061, buffmode, pbkiotbl->fname);
            return  -1;
        }

        /* Check for active crashtesting */
        if (pdbcontext->p1crashTestAnchor)
            bkCrashTry(pcontext, (TEXT *)"bkioWrite");

        /* if we made it this far, we are ready to perform the write */

        /* If the I/O fails, instead of aborting hence causing a failure
           it does no harm to retry the operation.  This has proven to
	   work on various machine like SCO and SEQUENT */

        retryCount = BKIO_RETRY;

        while (retryCount > 0)
        {
            /* call the appropiate write call */

            blocksWritten = (*writefunc)(pcontext, 
                                         pbkiotbl,
                                         pbkiostats,
                                         offset,
                                         pbuf, 
                                         size,
                                         blklog);

	    /* the write succeeded!  break out of the loop */

            if (blocksWritten == size)
            {
                break;
            }
            else    /* we wrote an unexpected amount of data */
            {
	        if ((bkioOSerror(pcontext, (TTINY *)"bkioWrite",
                   (TTINY *)"write", 
                    errno, writefd, size << blklog, offset << blklog, 
                    pbkiotbl->fname) < 0))
	        {
	            /* the error was bad enough to give up now! */
	            retryCount = 0;
	        }

	        /* if the error is worth retrying */
		retryCount--;
    
            }
        }

        return blocksWritten;

}  /* end bkioWrite */


/* PROGRAM: bkioOpen - Open a database file
 *
 * This is the ONLY program that should be used for database I/O!
 *
 * The program will open the database file and  store the appropriate
 * file handle/descriptors for either BUFFERED or UNBUFFERED I/O.
 *
 * Returns: Status -  BKIO_OPEN_OK      = success
 *                    BKIO_EINVAL       = invalid file name characters
 *                    BKIO_ENOENT       = no such file
 *                    BKIO_EMFILE       = too many open files
 *                    BKIO_EACCES       - permission denied
 *                    BKIO_EEXISTS      = file exists
 *                    BKIO_ENXIO        = bad i/o table pointer
 *
 */
LONG
bkioOpen(
         dsmContext_t *pcontext, 
         bkiotbl_t    *pbkiotbl,      /* pbkiotbl structure */
         bkiostats_t  *bkiostats _UNUSED_,     /* stats for promon */
         gem_off_t     fileIOflags,   /* constructed by ORing utfio.h flags */
         TEXT         *fileName,          /* name of file to open */
         int           openFlags,         /* open flags */
         int           createPermissions) /* creation permissions mask */

{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t *pdbpub = pdbcontext->pdbpub;

    int     retCode = 0;       /* return from open function */
    int     local   = 0;
    LONG    (*openFunc)(BKIO_OPEN_PROTOTYPE); /* function open */

     /* initialize openFunc() */
     openFunc = NULL;

     /* Check for special characters in file name  */
     if (utCheckFname(fileName) == DBUT_S_INVALID_FILENAME)
     {
         /* Set errno for caller to display */
         errno = BKIO_EINVAL;
         return (BKIO_EINVAL);
     }

     /* if bkioInit() hasn't been called yet, then complain */
     if (!pbkiotbl)
     {
         /* bkioRead: I/O table not initialized index %d<num>. */
         MSGN_CALLBACK(pcontext, bkMSG064);
         /* Set errno for caller to display */
         errno = BKIO_EINVAL;
         return (BKIO_ENXIO);
     }

     /* get the open function for the file from the bkiotbl entry */
     openFunc = pbkiotbl->file_open;

     /* Check if the file is on a remote file system but not if  the db */
     /* is read only or if bi i/o is buffered since we can't guarantee  */
     /* integrity anyway or if its the ai file we dont want to set      */
     /* buway or if its the ai file we dont want to set                 */
     /* buffered io and we've already warned 'em about the ai file      */
     /* before here                                                     */
      if (pdbpub->useraw && !pdbcontext->argronly &&
         (stcomp( fileName + stlen(fileName) - 3, (TEXT *)".ai") != 0))
      {
          if ((local = bkioIsLocalFile(pcontext, fileName)) != 1)
          {
              if (local < 0)
              {
                  /* Set errno for caller to display */
                  errno = local;
                  return local;
              }

              MSGN_CALLBACK(pcontext, bkMSG034,fileName);
              MSGN_CALLBACK(pcontext, bkMSG038);

              /* Set flag for buffered to bi file. */
              /* If we crash then user will be told that      */
              /* their database is damaged            */
              pdbpub->useraw = 0;
          }
       }

       /* Put the path and filename to be opened in the bkiotbl structure */
       bufcop (pbkiotbl->fname,fileName,strlen((const char *)fileName));

       /* if we made it this far, we are ready to perform the open */
       retCode = (*openFunc)(pcontext,
                             pbkiotbl,
                             NULL,
                             fileIOflags,
                             fileName,
                             openFlags,
                             createPermissions);

       /* pass back what we got from the platform open() call */ 
       return retCode;

}  /* end bkioOpen */

