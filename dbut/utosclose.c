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

/* Table of Contents - device close utility functions:
 *
 * utOsClose    - O/S independent file close.
 * utCloseFiles - Close all file except 0 1 and 2
 * utOsCloseTemp - O/S independent temp file close.
 * utclex       - To set a file to close on exec
 *
 */

/*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#if defined(PRO_REENTRANT) && OPSYS==UNIX
#include <pthread.h>
#endif
#include "utmpub.h"
#include "utospub.h"
#include "utspub.h"
#include "utlru.h"

#if OPSYS==UNIX
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#endif

#if OPSYS==WIN32API
#include <io.h>
#endif

#if PSC_TABLESIZE == PSC_GETRLIMIT
#include <sys/resource.h>
#endif


/* PROGRAM: utclex - to set a file to close on exec
 *
 * RETURNS: DSMVOID
 */
DSMVOID
utclex(fileHandle_t fileHandle)     /* the file descriptor */
{
#if OPSYS==UNIX

    fdHandle_t fd;

#ifdef PRO_REENTRANT
    fd = fileHandle->fh_fd;
#else
    fd = fileHandle;
#endif

#if UTCLEXI
 
    ioctl(fd, FIOCLEX, (char *)NULL);
 
#else
 
    fcntl(fd, F_SETFD, 1);
 
#endif
   /* nothing happens on WINNT */
#endif
}

/* PROGRAM: utOsCloseTemp - utility to close and delete a temp file (close)
 *
 * RETURNS: DSMVOID
 */
DSMVOID
utOsCloseTemp (
        fileHandle_t  fileHandle,
        TEXT         *tname _UNUSED_,
        TEXT         *pTmpDir)
{
    int     retClose;  /* return for utOsClose */
    TEXT    *ps;
 
    if (fileHandle != INVALID_FILE_HANDLE)
    {
        retClose = utOsClose(fileHandle, 0);
    }
    ps = term(pTmpDir);
    unlink((char *) pTmpDir);
    *ps = '\0';     /* reset pTmpDir */
}

#if OPSYS == WIN32API

/* PROGRAM: utOsFlush - Flish out OS buffers for the file
 *
 * RETURNS: return from O/S close
 *          -1 on error
 */
int
utOsFlush ( fileHandle_t fileHandle)
{

    FlushFileBuffers(fileHandle);
    return 0;
}

/* PROGRAM: utOsClose - O/S independent file close
 *
 * RETURNS: return from O/S close
 *          -1 on error
 */
int
utOsClose (fileHandle_t hFile, UCOUNT ioFlags)
{
    int	rc = -1;	/* default is error */
    GBOOL bCloseRet;     /* return for CloseHandle */  

    bCloseRet = CloseHandle (hFile);
    /* if bCloseRet == TRUE, rc = , else rc = -1 */
    rc = (bCloseRet ? 0 : -1);
    return rc;
}
#endif

#if OPSYS == UNIX

/* PROGRAM: utOsFlush - Flish out OS buffers for the file
 *
 * RETURNS: return from O/S close
 *          -1 on error
 */
int
utOsFlush ( fileHandle_t fileHandle)
{
    int	rc = -1;	/* default is error */

#ifdef PRO_REENTRANT
    /* flush file handle before closing */
    rc = fdatasync(fileHandle->fh_fd);
#else
    rc = fdatasync(fileHandle);
#endif
    return rc;
}

/* PROGRAM: utOsClose - O/S independent file close
 *
 * RETURNS: return from O/S close
 *          -1 on error
 */
int
utOsClose(fileHandle_t fileHandle,
          UCOUNT       mode _UNUSED_)
{
    int	rc = -1;	/* default is error */


#ifdef PRO_REENTRANT
    /* no need to close the file if it fell out of the cache */
    if (fileHandle->fh_state == FH_OPEN)
    {
        rc = close (fileHandle->fh_fd);
#ifdef FDCACHE_DEBUG
        utWalkFdCache(fh_anchor, "BEGIN:utOsClose");
#endif
        utRemoveLru(fh_anchor, fileHandle);
#ifdef FDCACHE_DEBUG
        utWalkFdCache(fh_anchor, "END:utOsClose");
#endif
    }

    pthread_mutex_destroy(&fileHandle->fh_ioLock);

    utfree((TEXT *)fileHandle);
#else
    rc = close (fileHandle);
#endif

    return rc;
}

/* PROGRAM: utCloseFiles - Close all file except 0 1 and 2
 *
 *  finds out the maximum numbers of files a process can have open
 *  then closes them.  This is a useful thing to do at startup so
 *  we dont't start out with any extraneaus file descriptors
 *
 * RETURNS: DSMVOID
 */
DSMVOID
utCloseFiles()
{
    LONG    numfds,
            openFds;

    numfds = utGetTableSize();

    /* close all files except stdin, stdout, and stderr */
    for (openFds=3; openFds<numfds; openFds++)
    {
        close(openFds);
    }

    return;
}

/* PROGRAM: utGetTableSize - return the total number of files a process 
 *                           can have open 
 * RETURNS: DSMVOID
 */
LONG
utGetTableSize()
{
    LONG    numfds;

#if PSC_TABLESIZE == PSC_GETRLIMIT
    struct rlimit rlp;
#endif

    /* figure out the largest file descriptor we can have */

#if PSC_TABLESIZE == PSC_GETDTABLESIZE

    numfds = getdtablesize();

#else  /* PSC_TABLESIZE == PSC_GETDTABLESIZE */
#if PSC_TABLESIZE == PSC_GETRLIMIT

    numfds = getrlimit(RLIMIT_NOFILE, &rlp);
    numfds = rlp.rlim_max;

#else  /* PSC_TABLESIZE == PSC_GETRLIMIT */

    PSCGRONK; how do you get the maximum numbers of files a process can
              have open?
#endif  /* PSC_TABLESIZE == PSC_GETRLIMIT */
#endif  /* PSC_TABLESIZE == PSC_GETDTABLESIZE */

    return numfds;
}

#endif  /* UNIX */
