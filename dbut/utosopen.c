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

/* Table of Contents - device open utility functions:
 *
 * utCheckFname - check for invalid characters in file name.
 * utOsCreat    - O/S independent file create/open.
 * utOsOpen     - O/S independent file open.
 * utOsOpenTemp - O/S independent temp file open.
 *
 */

/*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include <stdlib.h>
#include "dbconfig.h"
#if defined(PRO_REENTRANT) && OPSYS==UNIX
#include <pthread.h>
#endif
#include "utfpub.h"      /* CHG_OWNER, DEFLT_PERM, OPEN_RELIABLE */
#include "utmpub.h"
#define FH_ANCHOR_GLOB 1  /* allocate the fh_anchor pointer */
#include "utospub.h"     /* for utOsOpen, INVALID_FILE_HANDLE */
#include "utspub.h"      /* for scan() */
#include "utlru.h"

#if OPSYS==WIN32API
#include "share.h"
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <io.h>
#define CREATMODE S_IREAD | S_IWRITE
#endif  /* OPSYS == WIN32API */

#include <errno.h>
#include <fcntl.h>

#if OPSYS == UNIX
#define CREATMODE 0666
#include <unistd.h>
#if SOLARIS
#include <sys/stat.h>
#endif /* SOLARIS */
#endif /* OPSYS == UNIX */


/* PROGRAM: utCheckFname - check for invalid characters in file name
 *
 * RETURNS: SUCCESS - DBUT_S_SUCCESS 
 *          FAILURE - DBUT_S_INVALID_FILENAME
 */
LONG
utCheckFname(TEXT *pFileName)
{
  TEXT     *ptr = pFileName;

  while(*ptr)
  {
#if OPSYS==WIN32API
    if (*ptr == '/')
      *ptr = '\\';
#else
    if (*ptr == '\\')
      *ptr = '/';
#endif
    ++ptr;
  }

  ptr = pFileName;
  while(*ptr)
  {
#if OPSYS==WIN32API
    if (scan ((TEXT *)"?*><|\t\n", *ptr))
#else
    if (scan ((TEXT *)"?*\t\n", *ptr))
#endif
    {
        return(DBUT_S_INVALID_FILENAME);
    }
    ++ptr;
  }
  return(DBUT_S_SUCCESS);

}  /* end utCheckFname */


/* PROGRAM: utOsOpenTemp - O/S independent temp file create/open.
 *          Displays error message if AUTOERR is set.
 *
 * RETURNS: SUCCESS - valid handle/fd
 *          FAILURE - invalid handle/fd
 */
fileHandle_t
utOsOpenTemp(
        TEXT  *tname,
        TEXT  *pTmpDir,
        GTBOOL  unlinkTempDB,
        int   *perrorStatus)
{
    fileHandle_t  fileHandle;
    TEXT          *ps = term(pTmpDir);
 
#if OPSYS == WIN32API
    tname[8] = '\0';
#endif    /* OPSYS == WIN32API */
 
    /* Create fully qualified name before calling mktemp*/
    stcopy(ps, tname);
    mktemp((char *)pTmpDir);    /* LBIXXXXX -> LBI99999 */
    stcopy(tname, ps);          /* Update temp name prefix */
 
    if ((fileHandle = utOsCreat((TEXT *) pTmpDir, CREATMODE,
                        0, perrorStatus)) == INVALID_FILE_HANDLE)
        return fileHandle;
 
    utOsClose(fileHandle, 0);
    if ((fileHandle = utOsOpen((TEXT *) pTmpDir, OPEN_RW, DEFLT_PERM,
                       OPEN_SEQL, perrorStatus)) == INVALID_FILE_HANDLE)
        return fileHandle;
 
    utclex(fileHandle);

    if (unlinkTempDB)
        unlink((char *) pTmpDir);

    *ps = '\0'; /* reset pTmpDir */

    return fileHandle;

}  /* end utOsOpenTemp */

/* PROGRAM: utInitFdCache - Initialize the fd LRU anchor
 *
 * RETURNS: DSMVOID
 */
DSMVOID
utInitFdCache(ULONG numEntries)
{
#if OPSYS == UNIX
    fh_anchor = (lruAnchor_t *)utmalloc(sizeof(lruAnchor_t));
    stnclr(fh_anchor, sizeof(lruAnchor_t));
    utLruInit(fh_anchor, numEntries);
#endif
}

#if OPSYS == UNIX

/* PROGRAM: utOsCreat - O/S independent file create/open.
 *	    Displays error message if AUTOERR is set.
 *
 * RETURNS: return from O/S creat/open or -1 on error.
 */
fileHandle_t
utOsCreat(
        TEXT   *pFileName,
        int     createMask,
        UCOUNT  ioFlags,
        int    *perrorStatus)
{
    int oflag;
    
    fdHandle_t    fd = INVALID_FD_HANDLE; /* default is error */
    fileHandle_t  fileHandle = INVALID_FILE_HANDLE;
    fileHandle_t  closeHandle = (fileHandle_t)0;

    /* check for invalid characters in file name */
    if (utCheckFname(pFileName) == DBUT_S_INVALID_FILENAME)
    {
      return (INVALID_FILE_HANDLE);
    }
    
    
    oflag = O_CREAT | O_WRONLY | O_TRUNC;
    
    fd = open((char *)pFileName, oflag,
              createMask);

    *perrorStatus = errno;
    if ((fd == INVALID_FD_HANDLE) && (ioFlags & AUTOERR))
	return (INVALID_FILE_HANDLE);

    /* programs which execute in super-user mode should pass
       CHG_OWNER to cause the ownership of the file to be set
       to the real userid
    */
    if (ioFlags & CHG_OWNER)
    {
	chown((char *)pFileName, getuid(), getgid());
    }

#ifdef PRO_REENTRANT
    fileHandle = (fileHandle_t)utmalloc(sizeof(struct fileHandle));
    if (!fileHandle)
        fileHandle = INVALID_FILE_HANDLE;
    else
    {
        pthread_mutexattr_t attr;
        stnclr(fileHandle, sizeof(struct fileHandle));

        fileHandle->fh_fd = fd;
        pthread_mutexattr_init(&attr);
        pthread_mutex_init(&fileHandle->fh_ioLock, &attr);
        pthread_mutexattr_destroy(&attr);

        fileHandle->fh_state = FH_OPEN;
        fileHandle->fh_oflag = oflag;
        fileHandle->fh_createMask = createMask;
        bufcop(fileHandle->fh_path, pFileName, stlen(pFileName));
#ifdef FDCACHE_DEBUG
        utWalkFdCache(fh_anchor, "BEGIN:utOsCreate");
#endif
        closeHandle = (fileHandle_t)utDoLru(fh_anchor, fileHandle);
#ifdef FDCACHE_DEBUG
        utWalkFdCache(fh_anchor, "END:utOsCreate");
#endif
        if (closeHandle)
        {
#ifdef FDCACHE_DEBUG
            printf("EVICT:utOsCreate %s\n",closeHandle->fh_path);
#endif
            /* had to evict, close the evicted fd and mark it's state */
            pthread_mutex_lock((pthread_mutex_t *)&closeHandle->fh_ioLock);
            close(closeHandle->fh_fd);
            closeHandle->fh_state = FH_CLOSED;
            pthread_mutex_unlock((pthread_mutex_t *)&closeHandle->fh_ioLock);
        }
    }
#else
    fileHandle = fd;
#endif

    return fileHandle;
}


/* PROGRAM: utOsOpen- O/S independent file open.
 *	    Displays error message if AUTOERR is set.
 *
 * RETURNS: Error   - INVALID_FILE_HANDLE
 *          Success - valid handle/descriptor
 */
fileHandle_t
utOsOpen(
        TEXT   *pFileName,
        int     openFlags,
        int     createMask,
        UCOUNT  ioFlags, 
        int    *perrorStatus)
{
    fdHandle_t    fd = INVALID_FD_HANDLE; /* default is error */
    fileHandle_t  fileHandle = INVALID_FILE_HANDLE;
    fileHandle_t  closeHandle = (fileHandle_t)0;

    *perrorStatus = 0;
    /* check for invalid characters in file name */
    if (utCheckFname(pFileName) == DBUT_S_INVALID_FILENAME)
    {
        return (INVALID_FILE_HANDLE);
    }

    if (ioFlags & OPEN_RELIABLE)
    {
#if defined(O_DSYNC)
	openFlags |= O_DSYNC;
#else
	openFlags |= O_SYNC;
#endif
    }
    
    /* bug 95-11-13-028 cap. Need to handle if system call gets interrupted */
    /* before it is done. */
    while (1)
    {
      if (createMask == DEFLT_PERM)
      { 
	fd = open ((char *)pFileName, openFlags);
      }
      else
      {
	fd = open ((char *)pFileName, openFlags, createMask);
      }
      if (fd == INVALID_FD_HANDLE)
      {
          /* file didnt open, check error */
          if ((errno == EINTR) || (errno == EAGAIN))
          {
              continue; /* open() interrupted so try again */
          }
          *perrorStatus = errno;
      }
#ifdef PRO_REENTRANT
      else
      {
          fileHandle = (fileHandle_t)utmalloc(sizeof(struct fileHandle));
          if (!fileHandle)
              fileHandle = INVALID_FILE_HANDLE;
          else
          {
              pthread_mutexattr_t attr;
              stnclr(fileHandle, sizeof(struct fileHandle));

              fileHandle->fh_fd = fd;
              pthread_mutexattr_init(&attr);
              pthread_mutex_init(&fileHandle->fh_ioLock, &attr);
              pthread_mutexattr_destroy(&attr);

              fileHandle->fh_state = FH_OPEN;
              fileHandle->fh_oflag = openFlags;
              fileHandle->fh_createMask = createMask;
              bufcop(fileHandle->fh_path, pFileName, stlen(pFileName));
#ifdef FDCACHE_DEBUG
              utWalkFdCache(fh_anchor, "BEGIN:utOsOpen");
#endif
              closeHandle = (fileHandle_t)utDoLru(fh_anchor, fileHandle);
#ifdef FDCACHE_DEBUG
              utWalkFdCache(fh_anchor, "END:utOsOpen");
#endif
              if (closeHandle)
              {
#ifdef FDCACHE_DEBUG
                  printf("EVICT:utOsOpen %s\n",closeHandle->fh_path);
#endif
                  /* had to evict, close the evicted fd and mark it's state */
                  pthread_mutex_lock((pthread_mutex_t *)&closeHandle->fh_ioLock);
                  close(closeHandle->fh_fd);
                  closeHandle->fh_state = FH_CLOSED;
                  pthread_mutex_unlock((pthread_mutex_t *)&closeHandle->fh_ioLock);
              }
          }
      }
#else
      fileHandle = fd;
#endif
      break;
    }

    return fileHandle;
}
#endif


#if OPSYS == WIN32API
/* PROGRAM: utcreat - O/S independent file create/open.
 *	    Displays error message if AUTOERR is set.
 *
 * RETURNS: return from O/S creat/open or -1 on error.
 */
fileHandle_t
utOsCreat (
        TEXT    *pFileName,
        int      createMask,
        UCOUNT   ioFlags,
        int     *perrorStatus)
{
    fileHandle_t fileHandle = INVALID_FILE_HANDLE; /* default is error */

    /* check for invalid characters in file name */
    if (utCheckFname(pFileName) == DBUT_S_INVALID_FILENAME)
    {
      return (INVALID_FILE_HANDLE);
    }
    fileHandle = utOsOpen(pFileName, CREATE_RW_TRUNC, CREATE_DATA_FILE,
                  OPEN_SEQL, perrorStatus);

    return fileHandle;
}


/* PROGRAM: utOsOpen- O/S independent file open.
 *	    Displays error message if AUTOERR is set.
 *
 * RETURNS: Error   - INVALID_FILE_HANDLE
 *          Success - valid handle/descriptor
 */
fileHandle_t
utOsOpen(
        TEXT    *pFileName,
        int      openFlags,
        int      createMask,
        UCOUNT   ioFlags,
        int     *perrorStatus)
{
    DWORD fdwAccess        = GENERIC_READ;
    DWORD fdwAttrsAndFlags = FILE_FLAG_RANDOM_ACCESS;
    DWORD fdwCreate;

    fileHandle_t fileHandle = INVALID_FILE_HANDLE;	/* default is error */

    *perrorStatus = 0;
    /* check for invalid characters in file name */
    if (utCheckFname(pFileName) == DBUT_S_INVALID_FILENAME)
    {
        return (INVALID_FILE_HANDLE);
    }

    if ((openFlags & O_WRONLY) || (openFlags & O_RDWR))
    {
        fdwAccess |= GENERIC_WRITE;
    }

    /* Add write through flag for transaction files */
    if (ioFlags & OPEN_RELIABLE)
    {
		fdwAttrsAndFlags |= FILE_FLAG_WRITE_THROUGH;
    }

    /* Set create/open mode */        
    fdwAttrsAndFlags |= FILE_ATTRIBUTE_NORMAL;
    if (openFlags & O_CREAT)
    {
        if (openFlags & O_TRUNC)
        {
       	    fdwCreate = CREATE_ALWAYS;
        }
        else
        {
            /* assume O_EXCL by default */
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
    
    if (createMask != DEFLT_PERM)
    {
        if (createMask & _S_IWRITE)
        {
	    fdwAttrsAndFlags |= FILE_ATTRIBUTE_NORMAL;
        }
        else    
        {
	    fdwAttrsAndFlags |= FILE_ATTRIBUTE_READONLY;
        }
    }
    
    fileHandle = CreateFile((char *)pFileName, fdwAccess,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, 
   		    NULL, fdwCreate, fdwAttrsAndFlags, NULL);

    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        /*
         * file opened successfully.  If caller wants to append to the
         * file, then position at eof before returning.
         */
        if (openFlags & O_APPEND)
        {
            DWORD   dwPointer;

            /* set file-pointer to "eof + 0bytes" */
            dwPointer = SetFilePointer (fileHandle, 0, NULL, FILE_END);
            if (dwPointer == 0xFFFFFFFF)
            {
                /* something didn't work.  close file and return */
                utOsClose (fileHandle, 0);
                *perrorStatus = EACCES;         /* better error? */
                return (INVALID_FILE_HANDLE);
            }
        }
    }
    else /* (fileHandle == INVALID_HANDLE_VALUE) */
    {
        switch (GetLastError())
        {
            case ERROR_FILE_EXISTS:
            	*perrorStatus = EEXIST;
                break;
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            	*perrorStatus = ENOENT;
                break;
            case ERROR_TOO_MANY_OPEN_FILES:
            	*perrorStatus = EMFILE;
                break;
            case ERROR_ACCESS_DENIED:
            default:
            	*perrorStatus = EACCES;
        }
    }

    return fileHandle;
}
#endif /* OPSYS == WIN32API */
