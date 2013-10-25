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
#include "bkpub.h"

#define  BKIOFUNCTIONS
#include "bkioprv.h"
#include "bkmsg.h"
#include "utmsg.h"

#include "utmpub.h"
#include "utospub.h"
#include "utspub.h"
#include "utfpub.h"
#include "utdpub.h"
#include "bkiopub.h"

#define _STDIO_INCLUDED
#include <errno.h>
#include <sys/types.h>

#if OPSYS==UNIX
#include <unistd.h>
#if !IBMRIOS
#include <sys/fcntl.h>
#endif
#endif

#if IBMRIOS
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/vmount.h>
#endif

#if OPSYS == WIN32API
#include "dos.h"
#include <fcntl.h>
#include <io.h>  
#include <sys\stat.h>
#include <direct.h>  
#include <ctype.h>  
#endif /* MS_DOS || WIN32API */

#if SUN45
#include <fcntl.h>
#include <errno.h>
#include <sys/mntent.h>
#include <sys/statvfs.h>
#if !SOLARIS
#include <sys/fcntlcom.h>
#endif /* !SOLARIS */
#endif /* !SUN45 */

#if SCO
#include <sys/statfs.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <sys/mount.h>
#endif


#if UNIX486V4  || DG5225 || ALPHAOSF 
#include <sys/statvfs.h>
#endif

#if SUN4
#include <sys/param.h>
#include <mntent.h>
#endif

#if HP825
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/fstyp.h>
#endif

#if LINUXX86
#include <sys/statfs.h>
#define NFS_SUPER_MAGIC       0x6969
#endif

/* BUM - the following exists in stdio on WIN32API and unistd.h on UNIX */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif

/* ALL PORT SPECIFIC I/O ROUTINES SHOULD BE BELOW THIS LINE */
/* Naming scheme is based on: nkio<operaion><platform>()
   where <operations> is:
		ReadBuffered
		WriteBuffered
		ReadUnBuffered
		WriteUnBuffered
		ReadDirect
		WriteDirect
                   .
                   .
                   .

    and <platform> is:
		PTX - Sequentptx
		DGUX - 	Data general DG/UX
		HPUX - HP
		SOL - Solaris
		DOS - DOS/Windows
                   .
                   .
                   .

    Basically porters should take a copy of 

 		bkioWriteUnBuffered(), bkioReadUnBuffered(),

    name them with the above naming scheme, and put the port specific
    code here.
*/

/* PROGRAM:  bkioSaveFd - Place the passed fd into the I/O table.  
 *
 *		       If the mode is BUFIO, the passed fd will
 *                     be placed in both the buffered_filePointer and
 *                     unbuffered_filePointer slot and BUFFERED calls
 *                     will be used on both 
 *                     BUFFERED and UNBUFFERED I/O calls
 *
 *		       If the mode is UNBUFIO, the passed fd will
 *                     be placed in both the buffered_filePointer and
 *                     unbuffered_filePointer slot and UNBUFFERED calls
 *                     will be used on both 
 *                     BUFFERED and UNBUFFERED I/O calls
 *
 *		       If the mode is BOTHIO, the passed fd will
 *                     be placed in both the buffered_filePointer and
 *                     unbuffered_filePointer slot and UNBUFFERED calls
 *                     will be used on both 
 *                     BUFFERED and UNBUFFERED I/O calls
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkioSaveFd (
        dsmContext_t *pcontext, 
        bkiotbl_t    *pbkiotbl,      /* pbkiotbl structure */
        TEXT         *pname,         /* pointer to the name of the file */
        fileHandle_t  fd,	     /* file descriptor */
        int	      mode)	     /* mode:BUFIO, UNBUFIO, BOTHIO */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t *pdbpub = pdbcontext->pdbpub;

    if (!pbkiotbl)
    {
        /* I/O table not initialized. */
        MSGN_CALLBACK(pcontext, bkMSG064);
        return;
    }

    /* initialize this structure */
    stnclr (pbkiotbl->fname, MAXPATHN);
    bufcop (pbkiotbl->fname,pname, stlen(pname));

    if (pdbpub->directio)
        mode = UNBUFIO;

    switch (mode)                      
    {
	case BUFIO:

	    /* fill in ALL the slots with buffered */
	    pbkiotbl->buffered_filePointer =     fd;
	    pbkiotbl->unbuffered_filePointer =   fd;

            /* buffered read/write functions already correctly loaded */
            pbkiotbl->unbufwrite = *bkioFunctions[BKIO_WRITE_BUFFERED];
            pbkiotbl->unbufread  = *bkioFunctions[BKIO_READ_BUFFERED];
	    break;

	case UNBUFIO:

	   /* first fill in the unbuffered slots, then copy them 
	      to the buffered slots */
            pbkiotbl->buffered_filePointer   = fd;
            pbkiotbl->unbuffered_filePointer = fd;

	    /* fill in the buffered slots with unbuffered */
#if DG5225
            if (pdbpub->directio)
            {
                pbkiotbl->bufwrite   = bkioWriteDirectDGUX;
                pbkiotbl->bufread    = bkioReadDirectDGUX;
                pbkiotbl->unbufwrite = bkioWriteDirectDGUX;
                pbkiotbl->unbufread  = bkioReadDirectDGUX;
            }
            else
            {
                pbkiotbl->bufwrite   = *bkioFunctions[BKIO_WRITE_UNBUFFERED];
                pbkiotbl->bufread    = *bkioFunctions[BKIO_READ_UNBUFFERED];
                pbkiotbl->unbufwrite = *bkioFunctions[BKIO_WRITE_UNBUFFERED];
                pbkiotbl->unbufread  = *bkioFunctions[BKIO_READ_UNBUFFERED];
            }
#else
            pbkiotbl->bufwrite   = *bkioFunctions[BKIO_WRITE_UNBUFFERED];
            pbkiotbl->bufread    = *bkioFunctions[BKIO_READ_UNBUFFERED];
            pbkiotbl->unbufwrite = *bkioFunctions[BKIO_WRITE_UNBUFFERED];
            pbkiotbl->unbufread  = *bkioFunctions[BKIO_READ_UNBUFFERED];
#endif

	    break;

	case BUFONLY:

	    pbkiotbl->buffered_filePointer =   fd;
	    break;

	case UNBUFONLY:

            /* fd slots are loaded correctly at this point */
            pbkiotbl->unbufwrite = *bkioFunctions[BKIO_WRITE_UNBUFFERED];
            pbkiotbl->unbufread  = *bkioFunctions[BKIO_READ_UNBUFFERED];
	    break;

	default:
            /* Invalid I/O mode specified requested %d<mode>. */
	    MSGN_CALLBACK(pcontext, bkMSG065,mode);
	    return;
    }

}  /* end bkioSaveFd */


/* PROGRAM:  bkioSync - Perform a machine independent sync() operation
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkioSync(dsmContext_t *pcontext _UNUSED_)
{
#if OPSYS==UNIX

    sync();
    return;

#else  /* UNIX */
#if  OPSYS==WIN32API
    bkfdtbl_t *pbkfdtbl = pcontext->pdbcontext->pbkfdtbl;
    ULONG      i;
    bkiotbl_t *pbkiotbl;

    if (pbkfdtbl != NULL)
    {
       for (i=0; i< pbkfdtbl->maxEntries; i++)
       {
          /* file the bkiotbl for the specified file */
          pbkiotbl = pbkfdtbl->pbkiotbl[i];

          /* if bkioInit() hasn't been called yet, then complain */
          if (pbkiotbl)
          {
             if (pbkiotbl->buffered_filePointer > 0)
                FlushFileBuffers(pbkiotbl->buffered_filePointer);
          }
       }
    }

    return;

#else /* WIN32API */

    PSCGRONK; How do I perform a sync() call??

#endif  /* WIN32API */
#endif  /* UNIX */

}  /* end bkioSync */


#if OPSYS != WIN32API

/* PROGRAM:  bkioWriteBuffered - write the specified number of bytes
 *      to one of the database files, starting at the specified
 *      Block offset.
 *
 * RETURNS: blockswritten on sucess
 *         -1 on failure
 */
LONG
bkioWriteBuffered(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* I/O tables structure */
        bkiostats_t  *pbkiostats, /* pointer for io statistics */
        gem_off_t     offset,	   /* BLOCK offset in the file */
        TEXT	     *pbuf,	   /* pointer to the buffer */
        int 	      size,	   /* length of the buffer in BLOCKS */
        int  	      blklog)	   /* blklog used for offset and size */
{
    int 	bytesWritten;
    int         errorStatus;
    LONG        retWrite;
    psc_bit_t   bytesToWrite = size << blklog;
#ifdef TRACE_BKIO
        MSGD_CALLBACK( pcontext, "%L Write to fileDescriptor: %d numBytes: %i",
                       pbkiotbl->buffered_filePointer, bytesToWrite);
#endif
    retWrite = utWriteToFile(pbkiotbl->buffered_filePointer, 
                             offset << blklog,
                             pbuf, bytesToWrite,
                             &bytesWritten, &errorStatus);

    switch(retWrite)
    {
        case DBUT_S_SUCCESS:
             pbkiostats->bkio_bufwrites++;
             break;
        case DBUT_S_SEEK_TO_FAILED:
        case DBUT_S_SEEK_FROM_FAILED:
        case DBUT_S_GET_OFFSET_FAILED:
	     /* bkioWrite: lseek error %d on file %n at %l */
             MSGN_CALLBACK(pcontext, bkMSG044, errorStatus,
                           pbkiotbl->buffered_filePointer, 
	                   offset << blklog, pbkiotbl->fname);
             return -1;
        default:
             break;
     }

     return bytesWritten >> blklog;

}  /* end bkioWriteBuffered */


/* PROGRAM:  bkioReadBuffered - read the specified number of bytes
 *      to one of the database files, starting at the specified
 *      Block offset.
 * 
 * RETURNS: bytesread on sucess
 *         -1 on failure
 */
LONG
bkioReadBuffered(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* I/O tables structure */
        bkiostats_t  *pbkiostats, /* pointer for io statistics */
        gem_off_t     offset,     /* BLOCK offset in the file */
        TEXT         *pbuf,       /* pointer to the buffer */
        int           size,       /* length of the buffer in BLOCKS */
        int           blklog)     /* blklog used for offset and size */

{
    int  	bytesRead;
    int         errorStatus;
    LONG        retRead;
    psc_bit_t   bytesToRead = size << blklog;

#ifdef TRACE_BKIO
    MSGD_CALLBACK( pcontext, "%L Read from fileDescriptor: %d numBytes: %i",
                        pbkiotbl->buffered_filePointer, bytesToRead);
#endif
    retRead = utReadFromFile(pbkiotbl->buffered_filePointer, 
                             offset << blklog,
                             pbuf, bytesToRead, 
                             &bytesRead, &errorStatus);

    switch(retRead)
    {
        case DBUT_S_SUCCESS:
             pbkiostats->bkio_bufreads++;
             break;
        case DBUT_S_SEEK_TO_FAILED:
        case DBUT_S_SEEK_FROM_FAILED:
        case DBUT_S_GET_OFFSET_FAILED:
	     /* bkioReadBuffered: lseek error %d on file %n at %l */
             MSGN_CALLBACK(pcontext, bkMSG045, errorStatus,
                           pbkiotbl->buffered_filePointer, 
	     	           offset << blklog, pbkiotbl->fname);
             return -1;
        default:
             break;
     }

     return bytesRead >> blklog;

}  /* end bkioReadBuffered */


/* PROGRAM:  bkioWriteUnBuffered - write the specified number of bytes
 *      to one of the database files, starting at the specified
 *      Block offset.
 *
 * RETURNS: blockswritten on sucess
 *         -1 on failure
*/
LONG
bkioWriteUnBuffered(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* I/O tables structure */
        bkiostats_t  *pbkiostats, /* pointer for io statistics */
        gem_off_t     offset,     /* BLOCK offset in the file */
        TEXT         *pbuf,       /* pointer to the buffer */
        int           size,       /* length of buffer in BLOCKS */
        int           blklog)     /* blklog used for offset, size */
{
    int 	bytesWritten;
    int         errorStatus;
    LONG        retWrite;
    psc_bit_t   bytesToWrite = size << blklog;

#ifdef TRACE_BKIO
        MSGD_CALLBACK( pcontext, "%L Write to fileDescriptor: %d numBytes:%i",
                       pbkiotbl->unbuffered_filePointer, bytesToWrite);
#endif
    retWrite = utWriteToFile(pbkiotbl->unbuffered_filePointer, 
                             offset << blklog,
                             pbuf, bytesToWrite, 
                             &bytesWritten, &errorStatus);

    switch(retWrite)
    {
        case DBUT_S_SUCCESS:
             pbkiostats->bkio_unbufwrites++;
             break;
        case DBUT_S_SEEK_TO_FAILED:
        case DBUT_S_SEEK_FROM_FAILED:
        case DBUT_S_GET_OFFSET_FAILED:
	     /* bkioWrite: lseek error %d on file %n at %l */
             MSGN_CALLBACK(pcontext, bkMSG044, errorStatus,
                           pbkiotbl->unbuffered_filePointer, 
	                   offset << blklog, pbkiotbl->fname);
             return -1;
        default:
             break;
     }

     return bytesWritten >> blklog;

}  /* end bkioWriteUnBuffered */


/* PROGRAM:  bkioReadUnBuffered - read the specified number of bytes
 *      to one of the database files, starting at the specified
 *      Block offset.
 *
 * RETURNS: bytesread on sucess
 *         -1 on failure
*/
LONG
bkioReadUnBuffered(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* I/O tables structure */
        bkiostats_t  *pbkiostats, /* pointer for io statistics */
        gem_off_t     offset,     /* BLOCK offset in the file */
        TEXT         *pbuf,       /* pointer to the buffer */
        int           size,       /* length of the buffer in BLOCKS */
        int           blklog)     /* blklog used for offset and size */

{
    int 	bytesRead;
    int         errorStatus;
    LONG        retRead;
    psc_bit_t   bytesToRead = size << blklog;

#ifdef TRACE_BKIO
        MSGD_CALLBACK( pcontext, "%L Read from fileDescriptor: %d numBytes: %i",
                       pbkiotbl->unbuffered_filePointer, bytesToRead);
#endif
    retRead = utReadFromFile(pbkiotbl->unbuffered_filePointer, 
                             offset << blklog,
                             pbuf, bytesToRead, 
                             &bytesRead, &errorStatus);

    switch(retRead)
    {
        case DBUT_S_SUCCESS:
             pbkiostats->bkio_unbufreads++;
             break;
        case DBUT_S_SEEK_TO_FAILED:
        case DBUT_S_SEEK_FROM_FAILED:
        case DBUT_S_GET_OFFSET_FAILED:
	     /* bkioReadBuffered: lseek error %d on file %n at %l */
             MSGN_CALLBACK(pcontext, bkMSG045, errorStatus,
                           pbkiotbl->unbuffered_filePointer, 
	     	           offset << blklog, pbkiotbl->fname);
             return -1;
        default:
             break;
     }

     return bytesRead >> blklog;

}  /* end bkioReadUnBuffered */

#endif


#if DG5225


/* PROGRAM:  bkioWriteDirectDGUX - write the specified number of bytes
 *      to one of the database files, starting at the specified
 *      Block offset.
 *
 * RETURNS: blockswritten on sucess
 *         -1 on failure
 */
LONG
bkioWriteDirectDGUX(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* I/O tables structure */
        bkiostats_t  *pbkiostats, /* pointer for io statistics */
        LONG          offset,     /* BLOCK offset in the file */
        TEXT         *pbuf,       /* pointer to the buffer */
        int           size,       /* length of buffer in BLOCKS */
        int           blklog)     /* blklog for offset and size */
{
    LONG	blocksWritten;

    pbkiostats->bkio_unbufwrites++;

    blocksWritten =  dg_unbuffered_write (pbkiotbl->unbuffered_filePointer, pbuf, 
                       (offset << blklog)/512, 
                       (unsigned int)(size << blklog)/512 );

    return (blocksWritten * 512) >> blklog;

}  /* end bkioWriteDirectDGUX */


/* PROGRAM:  bkioReadDirectDGUX - read the specified number of bytes
 *      to one of the database files, starting at the specified
 *      Block offset.
 *
 * RETURNS: blocks read on sucess
 *         -1 on failure
 */
LONG
bkioReadDirectDGUX(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* I/O tables structure */
        bkiostats_t  *pbkiostats, /* pointer for io statistics */
        LONG          offset,     /* BLOCK offset in the file */
        TEXT         *pbuf,       /* pointer to the buffer */
        int           size,       /* length of buffer in BLOCKS */
        int           blklog)     /* blklog for offset and size */
{
    LONG	blocksRead;

    pbkiostats->bkio_unbufreads++;

    blocksRead =  dg_unbuffered_read (pbkiotbl->unbuffered_filePointer, pbuf, 
                    (offset << blklog)/512, 
                    (unsigned int)(size << blklog)/512 );

    return (blocksRead * 512) >> blklog;

}  /* end bkioReadDirectDGUX */


/* PROGRAM:  bkioOpenDGUX -  Open database files in the requested mode
 *
 * RETURNS: SUCCESS - BKIO_OPEN_OK 
 *          FAILURE -
 */
LONG
bkioOpenDGUX(
         dsmContext_t *pcontext,
         bkiotbl_t    *pbkiotbl,      /* pbkiotbl structure */
         bkiostats_t  *bkiostats,     /* stats for promon */
         LONG          fileIOflags,   /* constructed by ORing utfio.h flags */
         TEXT         *fileName,           /* name of file to open */
         int           openFlags,          /* open flags */
         int           createPermissions)  /* creation permissions mask */
             
{            
    dbcontext_t *pdbcontext = p_dbctl;
    dbshm_t *pdbpub = pdbcontext->pdbpub;
    fileHandle_t  fileDescriptor = INVALID_FILE_HANDLE; /* default is error */
    int           errno_s = errno;


    /* if bkioInit() hasn't been called yet, then complain */
    if (!pbkiotbl)
    {
        /* bkioWrite: called before io table initialized */
        MSGN_CALLBACK(pcontext, bkMSG060, fileIndex);
        return BKIO_ENXIO;
    }

    fileDescriptor = utOsOpen ((TEXT *)fileName, openFlags, createPermissions, fileIOflags, &errno_s);

    if (fileDescriptor == INVALID_FILE_HANDLE)
    {
        if (fileIOflags & AUTOERR)
        {
            MSGN_CALLBACK(pcontext, bkMSG002, fileName, errno_s);
            bkioFreeIotbl(pcontext, pbkiotbl);
        }

        /* put back errno for use by caller  */
        errno = errno_s;

        switch (errno_s)
        {
             case EEXIST:               /*  File exists */
                return BKIO_EEXISTS;
             case ENOENT:               /* No such file */
                return BKIO_ENOENT;
             case EMFILE:               /* Too many files open */
                return BKIO_EMFILE;
             case EACCES:               /* Permission denied */
                return BKIO_EACCES;
             default:                   /* Other error, see errno.h */
                MSGN_CALLBACK(pcontext, bkMSG002, (TEXT *)fileName,errno_s);
                return errno_s;
        }
    }
    else
    {
        if (fileIOflags & OPEN_RELIABLE)
        {
           pbkiotbl->unbuffered_filePointer = fileDescriptor;
        }
        else
        {
           pbkiotbl->buffered_filePointer = fileDescriptor;
        }

        if ( pcontext->pdbcontext->pdbpub->directio )
           bkioSaveFd( pcontext, pbkiotbl,
                       fileName, fileDescriptor,
                       UNBUFIO );

        /* dont pass file descriptor */
        utclex(fileDescriptor);

        /* put back errno for use by caller  */
        errno = errno_s;
        return BKIO_OPEN_OK;
    }

}  /* end bkioOpenDGUX */

#endif /* DG5225 */


#if OPSYS==WIN32API

/* PROGRAM:  bkioWriteUnBufferedWIN32 - write the specified number of bytes
 *      to one of the database files, starting at the specified
 *      Block offset.
 *
 * RETURNS: blockswritten on sucess
 *         -1 on failure
 */
LONG
bkioWriteUnBufferedWIN32(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* I/O tables structure */
        bkiostats_t  *pbkiostats, /* for io statistics */
        gem_off_t     offset,     /* BLOCK offset in the file */
        TEXT         *pbuf,       /* pointer to the buffer */
        int           size,       /* length in BLOCKS */
        int           blklog)     /* blklog for offset,size */
{
    psc_bit_t	bytesWritten;
    psc_bit_t   bytesToWrite = size << blklog;
    LONG        retWrite;
    int         errorStatus;

#ifdef TRACE_BKIO
        MSGD_CALLBACK( pcontext, "%L Write to fileDescriptor: %d numBytes: %i",
                       pbkiotbl->unbuffered_filePointer, bytesToWrite);
#endif
    retWrite = utWriteToFile(pbkiotbl->unbuffered_filePointer, 
                             offset << blklog,
                             pbuf, bytesToWrite, 
                             &bytesWritten, &errorStatus);

    switch(retWrite)
    {
        case DBUT_S_SUCCESS:
             pbkiostats->bkio_unbufwrites++;
             break;
        case DBUT_S_SEEK_TO_FAILED:
        case DBUT_S_SEEK_FROM_FAILED:
        case DBUT_S_GET_OFFSET_FAILED:
	     /* bkioWrite: lseek error %d on file %n at %l */
             MSGN_CALLBACK(pcontext, bkMSG044, errorStatus,
                           pbkiotbl->unbuffered_filePointer, 
	                   offset << blklog, pbkiotbl->fname);
             return -1;
        default:
             break;
     }

     return ( (LONG)(bytesWritten >> blklog));

}  /* end bkioWriteUnBufferedWIN32 */


/* PROGRAM:  bkioReadUnBufferedWIN32 - read the specified number of bytes
 *      to one of the database files, starting at the specified
 *      Block offset.
 *
 * RETURNS: bytesread on sucess
 *         -1 on failure
 */
LONG
bkioReadUnBufferedWIN32(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* I/O tables structure */
        bkiostats_t  *pbkiostats, /* pointer for io statistics */
        gem_off_t     offset,     /* BLOCK offset in the file */
        TEXT         *pbuf,       /* pointer to the buffer */
        int           size,       /* length of the buffer in BLOCKS */
        int           blklog)     /* blklog used for offset and size */
{
    psc_bit_t	bytesRead;
    psc_bit_t   bytesToRead = size << blklog;
    LONG        retRead;
    int         errorStatus;

#ifdef TRACE_BKIO
        MSGD_CALLBACK( pcontext, "%L Read from  fileDescriptor: %d numBytes",
                       pbkiotbl->unbuffered_filePointer, bytesToRead);
#endif
    retRead = utReadFromFile(pbkiotbl->unbuffered_filePointer, 
                             offset << blklog,
                             pbuf, bytesToRead, 
                             &bytesRead, &errorStatus);

    switch(retRead)
    {
        case DBUT_S_SUCCESS:
             pbkiostats->bkio_unbufreads++;
             break;
        case DBUT_S_SEEK_TO_FAILED:
        case DBUT_S_SEEK_FROM_FAILED:
        case DBUT_S_GET_OFFSET_FAILED:
	     /* bkioReadBuffered: lseek error %d on file %n at %l */
             MSGN_CALLBACK(pcontext, bkMSG045, errorStatus,
                           pbkiotbl->unbuffered_filePointer, 
	     	           offset << blklog, pbkiotbl->fname);
             return -1;
        default:
             break;
     }

     return ( (LONG)(bytesRead >> blklog) );

}  /* end bkioReadUnBufferedWIN32 */


/* PROGRAM:  bkioReadBufferedWIN32 - read the specified number of bytes
 *      to one of the database files, starting at the specified
 *      Block offset.
 *
 * RETURNS: bytesread on sucess
 *              -1 on failure
 */
LONG
bkioReadBufferedWIN32(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* I/O tables structure */
        bkiostats_t  *pbkiostats, /* pointer for io statistics */
        gem_off_t     offset,     /* BLOCK offset in the file */
        TEXT         *pbuf,       /* pointer to the buffer */
        int           size,       /* length of the buffer in BLOCKS */
        int           blklog)     /* blklog used for offset amd size */
{
    psc_bit_t	bytesRead;
    psc_bit_t   bytesToRead = size << blklog;
    LONG        retRead;
    int         errorStatus;

#ifdef TRACE_BKIO
        MSGD_CALLBACK( pcontext, "%L Read from fileDescriptor: %d numBytes: %i",
                       pbkiotbl->buffered_filePointer, bytesToRead);
#endif
    retRead = utReadFromFile(pbkiotbl->buffered_filePointer, 
                             offset << blklog,
                             pbuf, bytesToRead, 
                             &bytesRead, &errorStatus);

    switch(retRead)
    {
        case DBUT_S_SUCCESS:
             pbkiostats->bkio_bufreads++;
             break;
        case DBUT_S_SEEK_TO_FAILED:
        case DBUT_S_SEEK_FROM_FAILED:
        case DBUT_S_GET_OFFSET_FAILED:
	     /* bkioReadBuffered: lseek error %d on file %n at %l */
             MSGN_CALLBACK(pcontext, bkMSG045, errorStatus,
                           pbkiotbl->buffered_filePointer, 
	     	           offset << blklog, pbkiotbl->fname);
             return -1;
        default:
             break;
     }

     return ( (LONG)(bytesRead >> blklog) );

}  /* end bkioReadBufferedWIN32 */


/* PROGRAM:  bkioWriteBufferedWIN32 - write the specified number of bytes
 *      to one of the database files, starting at the specified
 *      Block offset.
 *
 * RETURNS: blockswritten on sucess
 *              -1 on failure
 */
LONG
bkioWriteBufferedWIN32(
        dsmContext_t *pcontext,
        bkiotbl_t    *pbkiotbl,   /* I/O tables structure */
        bkiostats_t  *pbkiostats, /* for io statistics */
        gem_off_t     offset,     /* BLOCK offset in the file */
        TEXT         *pbuf,       /* pointer to the buffer */
        int           size,       /* length of the buffer in BLOCKS */
        int           blklog)     /* blklog used for offset amd size */
{
    psc_bit_t	bytesWritten;
    psc_bit_t   bytesToWrite = size << blklog;
    LONG        retWrite;
    int         errorStatus;

#ifdef TRACE_BKIO
        MSGD_CALLBACK( pcontext, "%L Write to fileDescriptor: %d numBytes: %i",
                       pbkiotbl->buffered_filePointer, bytesToWrite);
#endif
    retWrite = utWriteToFile(pbkiotbl->buffered_filePointer, 
                             offset << blklog,
                             pbuf, bytesToWrite, 
                             &bytesWritten, &errorStatus);

    switch(retWrite)
    {
        case DBUT_S_SUCCESS:
             pbkiostats->bkio_bufwrites++;
             break;
        case DBUT_S_SEEK_TO_FAILED:
        case DBUT_S_SEEK_FROM_FAILED:
        case DBUT_S_GET_OFFSET_FAILED:
	     /* bkioWrite: lseek error %d on file %n at %l */
             MSGN_CALLBACK(pcontext, bkMSG044, errorStatus,
                           pbkiotbl->buffered_filePointer, 
	                   offset << blklog, pbkiotbl->fname);
             return -1;
        default:
             break;
     }

     return ( (LONG)(bytesWritten >> blklog) );

}  /* end bkioWriteBufferedWIN32 */


/* PROGRAM:  bkioOpenWIN32 -  Open database files in the requested mode
 *
 * RETURNS: SUCCESS - BKIO_OPEN_OK 
 *          FAILURE -
 */
LONG
bkioOpenWIN32(
         dsmContext_t *pcontext, 
         bkiotbl_t    *pbkiotbl,      /* pbkiotbl structure */
         bkiostats_t  *bkiostats,     /* stats for promon */
         gem_off_t     fileIOflags,   /* constructed by ORing utfio.h flags */
         TEXT         *fileName,           /* name of file to open */
         int           openFlags,          /* open flags */
         int           createPermissions)  /* creation permissions mask */

{
        fileHandle_t     fileDescriptor;      /* default is error */
        int             errno_s = errno;

        DWORD           fdwAccess;
        DWORD           fdwAttrsAndFlags = FILE_FLAG_RANDOM_ACCESS;
        DWORD           fdwCreate;

    /* if bkioInit() hasn't been called yet, then complain */
    if (!pbkiotbl)
    {
        /* bkioWrite: called before io table initialized */
        MSGN_CALLBACK(pcontext, bkMSG064);
        return BKIO_ENXIO;
    }



    fdwAccess = GENERIC_READ;
    if ((openFlags & O_WRONLY) || (openFlags & O_RDWR))
                fdwAccess |= GENERIC_WRITE;


        if (fileIOflags & OPEN_RELIABLE)
                fdwAttrsAndFlags |= FILE_FLAG_WRITE_THROUGH;


    fdwAttrsAndFlags |= FILE_ATTRIBUTE_NORMAL;
    if (openFlags & O_CREAT)
        {
        if (openFlags & O_TRUNC)
                {
                fdwCreate = CREATE_ALWAYS;
                }
        else
                {
                fdwCreate = CREATE_NEW;
                }
        }
    else
        {
        if (openFlags & O_TRUNC)
                {
                fdwCreate = TRUNCATE_EXISTING;
                }
        else
                {
                fdwCreate = OPEN_EXISTING;
                }
        }

    if (createPermissions != -1)
        {
        if (createPermissions & _S_IWRITE)
                {
                 fdwAttrsAndFlags |= FILE_ATTRIBUTE_NORMAL;
                }
        else
                {
                 fdwAttrsAndFlags |= FILE_ATTRIBUTE_READONLY;
                }
        }

        if ((fileDescriptor = CreateFile((TEXT *)fileName,
                                 fdwAccess,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL, fdwCreate, fdwAttrsAndFlags,
                                 NULL)) == INVALID_HANDLE_VALUE)
        {
            /* Find out what the failure number was */
            errno = GetLastError();

            /* Save errno so message subsystem does hose it */
            errno_s = errno;
            if (fileIOflags & AUTOERR)
            {
                MSGN_CALLBACK(pcontext, bkMSG002, fileName, errno_s);
            }

            /* put errno back so the caller can display it */
            errno = errno_s;
            switch (errno_s)
            {
                case ERROR_FILE_EXISTS:           /* File exists */
                   return BKIO_EEXISTS;
                case ERROR_FILE_NOT_FOUND:
                case ERROR_PATH_NOT_FOUND:        /* No such file */
                   return BKIO_ENOENT;
                case ERROR_TOO_MANY_OPEN_FILES:   /* Too many open files */
                   return BKIO_EMFILE;
                case ERROR_ACCESS_DENIED:         /* Permission denied */
                default:
                    return BKIO_EACCES;
            }
        }
      if (fileIOflags & OPEN_RELIABLE)
      {
          pbkiotbl->unbuffered_filePointer = fileDescriptor;
      }
      else
      {
          pbkiotbl->buffered_filePointer   = fileDescriptor;
      }

        if ( pcontext->pdbcontext->pdbpub->directio )
           bkioSaveFd( pcontext, pbkiotbl,
                       fileName, fileDescriptor,
                       UNBUFIO );

      /* put back errno for use by caller  */
      errno = errno_s;

      return BKIO_OPEN_OK;

}  /* end bkioOpenWIN32 */

#endif  /* WIN32API */


#if OPSYS == (UNIX) && !DG5225
/* PROGRAM:  bkioOpenUNIX -  Open database files in the requested mode
 *
 * RETURNS: SUCCESS - BKIO_OPEN_OK 
 *          FAILURE -
 */
LONG
bkioOpenUNIX(
         dsmContext_t *pcontext,
         bkiotbl_t    *pbkiotbl,      /* pbkiotbl structure */
         bkiostats_t  *bkiostats _UNUSED_,     /* stats for promon */
         gem_off_t     fileIOflags,   /* constructed by ORing utfio.h flags */
         TEXT         *fileName,           /* name of file to open */
         int           openFlags,          /* open flags */
         int           createPermissions)  /* creation permissions mask */

{
        fileHandle_t     fileDescriptor = INVALID_FILE_HANDLE;
        int             errno_s = errno;



    /* if bkioInit() hasn't been called yet, then complain */
    if (!pbkiotbl)
    {
        /* bkioWrite: called before io table initialized */
        MSGN_CALLBACK(pcontext, bkMSG064); 
        return BKIO_ENXIO;
    }

    /* Are default permission masks specified?       */
    fileDescriptor = utOsOpen ((TEXT *)fileName, openFlags,
                               createPermissions, fileIOflags, &errno_s);

#ifdef TRACE_BKIO
    MSGD_CALLBACK( pcontext, "%L Filename: %s, openFlags: %i, ioFlags: %d, fileDescriptor: %d ",
                       fileName, openFlags, fileIOflags, fileDescriptor );
#endif
    if (fileDescriptor == INVALID_FILE_HANDLE)
    {
        if (fileIOflags & AUTOERR)
        {
            MSGN_CALLBACK(pcontext, bkMSG002, fileName, errno_s);
            bkioFreeIotbl(pcontext, pbkiotbl);
        }
        /* put back errno for use by caller  */
        errno = errno_s;
        switch (errno_s)
        {
             case EEXIST:               /*  File exists */
                return BKIO_EEXISTS;
             case ENOENT:               /* No such file */
                return BKIO_ENOENT;
             case EMFILE:               /* Too many files open */
                return BKIO_EMFILE;
             case EACCES:               /* Permission denied */
                return BKIO_EACCES;
             default:                   /* Other error, see errno.h */
                MSGN_CALLBACK(pcontext, bkMSG002, (TEXT *)fileName,errno_s);
                return errno_s;
        }
    }
    else
    {
    
        if (fileIOflags & OPEN_RELIABLE)
        {
           pbkiotbl->unbuffered_filePointer = fileDescriptor;
        }
        else
        {
           pbkiotbl->buffered_filePointer = fileDescriptor;
        }

        if ( pcontext->pdbcontext->pdbpub->directio )
           bkioSaveFd( pcontext, pbkiotbl,
                       fileName, fileDescriptor,
                       UNBUFIO );

        /* set file to close on exec */
        utclex(fileDescriptor);

        /* put back errno for use by caller  */
        errno = errno_s;
        return BKIO_OPEN_OK;
    }

}  /* end bkioOpenUNIX */

#endif   /* UNIX && !DG5225 */

/************************************************************************* 
 * THIS IS THE END OF THE READ()/WRITE()/OPEN() FUNCTIONALITY, NEXT UP IS 
 * DETERMINING IF THE FILE PASSED IS ON A LOCAL FILE SYSTEM OR NOT
 *
 * PORTING NOTE:  PLEASE TRY TO KEEP THIS FILE NEAT!  IF YOU NEED
 *                TO CREATE A NEW WAY TO CHECK IS A FILE IS ON AN
 *                NFS DRIVE, THEN MAKE A COPY OF AN EXISTSING
 *                bkioTestLocalFile() FUNCTION AND MAKE YOUR MODS
 *                THERE.  THANKS...
 *                
 */

    
/* These machines don't currently support NFS or remote file systems */

#if OPSYS==WIN32API
int
bkioTestLocalFile(
        dsmContext_t *pcontext,
        TEXT         *pfullpath,      /* Absolute path of the file */
        TEXT         *pdirname)       /* parent directory */
{
   int   DriveType = 0;

   if ((pfullpath[1] == ':') && isalpha(pfullpath[0]))
      DriveType = utGetDriveType(pfullpath[0]);
   else
   {
      /* If there is a colon somwhere in ther string, */
      /* Assume NLM drive which is a remote drive     */
      if (scan(pfullpath, (TEXT) ':') != NULL)
         return (0);
      /* If we do not have a <letter>:, we know we have the       */
      /* default drive, but the path MUST BE FULLY QUALIFIED or   */
      /* it is a bug, so we only allow path names that begin with */
      /* a slash.                                                 */
      if ((pfullpath[0] != '\\') && (pfullpath[0] != '/'))
         return (0);
      /* Assume that the drive is the default drive   */
      DriveType = utGetDriveType((TEXT)('A' + _getdrive() - 1));
   }

   if (DriveType & LOCAL_DRIVE)
      return (1);

   if (DriveType & REMOTE_DRIVE)
      return (0);

   MSGN_CALLBACK(pcontext, utMSG013, pfullpath[0]);
   return (0); /* Assume Remote Drive */

}  /* end bkioTestLocalFile */

#endif  /* WIN32API */

/* IBMRIOS has a constant MNT_NFS that can be with stat() */
#if IBMRIOS

int
bkioTestLocalFile(
        dsmContext_t *pcontext,
        TEXT         *pfullpath,      /* Absolute path of the file */
        TEXT         *pdirname)       /* parent directory */
{
struct stat *statbuf, buffer;
       int   ret;

    statbuf = &buffer;
    
    ret = stat((psc_rtlchar_t *)pfullpath, statbuf);
    if (ret < 0)
    {
        /* now lets try parent directory */
        ret = stat((psc_rtlchar_t *)pdirname, statbuf);
        if (ret < 0)
        {
            /* we have errored on both the file and the parent directory
               let's print a message and return the error.
            */
            /* stat failed on both %s and %s, errno = %d */
            MSGN_CALLBACK(pcontext, bkMSG093, pfullpath,pdirname,errno);
            return (ret);
        }
    }

    /* check to see if stat thinks the file is an NFS file */
    if (statbuf->st_vfstype == MNT_NFS || statbuf->st_vfstype == MNT_NFS3)
    {
	/* it's NFS mounted */
	ret = 0;
    }
    else
    {
	/* The file is local */
	ret = 1;
    }

    return (ret);

}  /* end bkioTestLocalFile */

#endif  /* IBMRIOS */


/* The following machines support a statvfs() call */
#if SUN45 || MOTOROLAV4 || TANDEM || SGI || DG5225 || ALPHAOSF || UNIX486V4

int
bkioTestLocalFile(
        dsmContext_t *pcontext,
        TEXT         *pfullpath,      /* Absolute path of the file */
        TEXT         *pdirname)       /* parent directory */
{
struct statvfs *statbuf, buffer;
       int      ret;

    statbuf = &buffer;
    
    ret = statvfs((const char *)pfullpath, statbuf);
    if (ret < 0)
    {
        /* now lets try parent directory */
        ret = statvfs((const char *)pdirname, statbuf);
        if (ret < 0)
        {
            /* we have errored on both the file and the parent directory
               let's print a message and return the error.
            */
            /* stat failed on both %s and %s, errno = %d */
            MSGN_CALLBACK(pcontext, bkMSG093, pfullpath,pdirname,errno);
            return (ret);
        }
    }

    /* check to see if stat thinks the file is an NFS file */
#if ALPHAOSF
    if (strcmp(statbuf->f_basetype,"mfs")==0)
    {
       /* treat memory file system like remote */
       ret = 0; 
    }
    else
#endif
    if (strncmp(statbuf->f_basetype,"nfs", 3)==0)
    {
        /* it's nfs */
        ret = 0;
    }
    else 
    if (strncmp(statbuf->f_basetype,"NFS", 3)==0)
    {
        /* it's nfs */
        ret = 0;
    }
    else 
    {
        /* it's local */
        ret = 1;
    }

    return (ret);

}  /* end bkioTestLocalFile */

#endif  /* SUN45 || MOTOROLAV4 || TANDEM || SGI */


/* The following machines support a statfs() and sysfs() calls */
#if UNIX386 || MIPS3000 

int
bkioTestLocalFile(
        dsmContext_t *pcontext,
        TEXT         *pfullpath,      /* Absolute path of the file */
        TEXT         *pdirname)       /* parent directory */
{
struct statfs      *statbuf, buffer;
       int         ret;
       int         fstypnum;

    statbuf = &buffer;
    
    ret = statfs((char *)pfullpath, statbuf, sizeof(struct statfs), 0);
    if (ret < 0)
    {
        /* now lets try parent directory */
        ret = statfs((char *)pdirname, statbuf, sizeof(struct statfs), 0);
        if (ret < 0)
        {
            /* we have errored on both the file and the parent directory
               let's print a message and return the error.
            */
            /* stat failed on both %s and %s, errno = %d */
            MSGN_CALLBACK(pcontext, bkMSG093, pfullpath,pdirname,errno);
            return (ret);
        }
    }

    /* now lets check to see if the file is on NFS */
    fstypnum = sysfs(GETFSIND, "NFS");
    if (fstypnum == statbuf->f_fstyp)
    {
      /* it's NFS */
      return (0);
    }
    /* check lowercase values for mips */
    fstypnum = sysfs(GETFSIND, "nfs");
    if (fstypnum == statbuf->f_fstyp)
    {
        /* it's NFS */
        return (0);
    }
    else
    {
        /* it's local */
        return (1);
    }

}  /* end bkioTestLocalFile */

#endif  /* UNIX386 || MIPS3000 */

#if SUN4
int
bkioTestLocalFile(
        dsmContext_t *pcontext,
        TEXT         *pfullpath,      /* Absolute path of the file */
        TEXT         *pdirname)       /* parent directory */
{
    FILE  *pmyhandle;
    struct mntent *pmntent;
    int   matchlen = 0;
    int   index = 0;
    int   local = 1;
    TEXT  *prealpathname;	/* needed if the path is on an automounted 
				   file-system. */
    TEXT   *ret;

    prealpathname = utmalloc(MAXPATHLEN);

    ret = (TEXT *)realpath(pfullpath,prealpathname);
    
    if (ret != PNULL)
    {
       /* use the given path because the realpath call failed */
        bufcop(prealpathname, pfullpath, sizeof(pfullpath));
    }

    pmyhandle = setmntent(MOUNTED,"r");
    if ( pmyhandle == NULL )
    {
        MSGN_CALLBACK(pcontext,  utMSG177, errno);
        local = -1;
        goto fatal;
    }
    while((pmntent = getmntent(pmyhandle)) != NULL )
    {
        if( (index = stidx(prealpathname,pmntent->mnt_dir,(GBOOL)1 )) )
        {
            if( stlen(pmntent->mnt_dir) > matchlen )
            {
                matchlen = stlen(pmntent->mnt_dir );
                if( stpcmp(pmntent->mnt_type,MNTTYPE_NFS) == 0 )
                    local = 0;
                else
                    local = 1;
            }
        }
    }
    endmntent( pmyhandle );
fatal:

    utfree(prealpathname);
    return(local);

}  /* end bkioTestLocalFile */

#endif /* SUN4 */

#if HP825

int
bkioTestLocalFile(
        dsmContext_t *pcontext,
        TEXT         *pfullpath,      /* Absolute path of the file */
        TEXT         *pdirname)       /* parent directory */
{
struct stat      *statbuf, buffer;
       int         ret,               /* return for stat() */
                   nfsType2,          /* return for nfs2 type fs */
                   nfsType3;          /* return for nfs3 type fs */

    statbuf = &buffer;

    ret = stat((char *)pfullpath, statbuf);
    if (ret < 0)
    {
        /* now lets try parent directory */
        ret = stat((char *)pdirname, statbuf);
        if (ret < 0)
        {
            /* we have errored on both the file and the parent directory
               let's print a message and return the error.  */
            /* stat failed on both %s and %s, errno = %d */
            MSGN_CALLBACK(pcontext, bkMSG093, pfullpath,pdirname,errno);
            return (ret);
        }
    }

    /* get the id's for the two nfs types.
       hpux 10.20 has only type of "nfs"
       hpux 11    has type of "nfs" or "nfs3"
       09/17/98
     */
    nfsType2 = sysfs(GETFSIND,"nfs"); 
    nfsType3 = sysfs(GETFSIND,"nfs3"); 

    if (statbuf->st_fstype == nfsType2 || statbuf->st_fstype == nfsType3)
    {
        /* it's NFS */
        ret = 0;
    }
    else
    {
        /* it's local */
        ret = 1;
    }

    return (ret);

}  /* end bkioTestLocalFile */

#endif  /* HPUX */
/* The following machines support a statfs() and sysfs() calls */
#if SCO
int
bkioTestLocalFile(
        dsmContext_t *pcontext,
        TEXT         *pfullpath,      /* Absolute path of the file */
        TEXT         *pdirname)       /* parent directory */
{
 struct statfs      *statbuf, buffer;
        int         ret;
        int         fstypnum;

     statbuf = &buffer;

     ret = statfs((char *)pfullpath, statbuf,sizeof (struct statfs),0);
     if (ret < 0)
     {
 	/* now lets try parent directory */
 	ret = statfs((char *)pdirname, statbuf,sizeof (struct statfs),0);
 	if (ret < 0)
 	{
 	    /* we have errored on both the file and the parent directory
 	       let's print a message and return the error.
 	    */

 	    /* stat failed on both %s and %s, errno = %d */
            MSGN_CALLBACK(pcontext, bkMSG093, pfullpath,pdirname,errno);
 	    return (ret);
 	}
     }

     /* now lets check to see if the file is on NFS */
     fstypnum = sysfs(GETFSIND, "NFS");
     if (fstypnum == statbuf->f_fstyp)
     {
       /* it's NFS */
       return (0);
     }
     else
 	return (1);
}
#endif

#if LINUXX86
int
bkioTestLocalFile(
        dsmContext_t *pcontext,
        TEXT         *pfullpath,      /* Absolute path of the file */
        TEXT         *pdirname)       /* parent directory */
 {
 struct statfs      *statbuf, buffer;
        int         ret;
 
     statbuf = &buffer;

     ret = statfs(pfullpath, statbuf);
     if (ret < 0)
     {
 	/* now lets try parent directory */
        ret = statfs(pdirname, statbuf);
 	if (ret < 0)
 	{
 	    /* we have errored on both the file and the parent directory
 	       let's print a message and return the error.
 	    */

 	    /* stat failed on both %s and %s, errno = %d */
            MSGN_CALLBACK(pcontext, bkMSG093, pfullpath,pdirname,errno);
 	    return (ret);
 	}
     }
     /* now lets check to see if the file is on NFS */
     if (statbuf->f_type == NFS_SUPER_MAGIC)
     {
       /* it's NFS */
        return (0);
     }
    else
      {
 	return (1);
      }
 }
#endif

/* PROGRAM: bkioFileInBlocks - obtain file size in blocks.
 *
 *
 *
 * RETURNS: int -(0)  - success
 *               (-1) - error - either the file does not exist or
 *                              system error.
 */
int
bkioFileInBlocks(
        TEXT         *pname,
        ULONG        *bkiofilesize,
        LONG          blklog)      
{
        gem_off_t utfilesize = 0;/* filesize from utflen() */
        int ret = 0;

    if ((ret = utflen (pname, &utfilesize)) == 0 )
    {
        *bkiofilesize = (ULONG)(utfilesize >> blklog);
        return ret;
    }
    else
        ret = -1;

   *bkiofilesize = (ULONG)(utfilesize >> blklog);
    return ret;

}  /* end bkioFileInBlocks */


