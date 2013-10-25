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

/* Table Contents - device read/write utility functions:
 * 
 *  utSeekToOffset    - Move file pointer to specified offset.
 *  utSeekFromCurrent - Move file pointer to specified offset from end.
 *  utSeekFromOffset  - Move file pointer to specified offset from end
 *  utGetOffset       - Return the offset into the file.
 *  utReadFromFile    - Read from specified offset for # of bytes.
 *  utReadFromFileSeq - Read from current file pointer location for # of bytes.
 *  utTruncateFile    - Moves file pointer from EOF to current position.
 *  utWriteToFile     - Write # bytes at specified offset.
 *  utWriteToFileSeq  - Write # bytes at current file pointer location.
 *  utLockLkFile      - Lock a region of the .lk file.
 *  utUnlockLkFile    - Unlock a region of the .lk file.
 */

/*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#if OPSYS == UNIX
/* must come before dbconfig.h in order to avoid conflicts with DFILE */
#include <stdio.h>
#endif

#include "dbconfig.h"
#if defined(PRO_REENTRANT) && OPSYS == UNIX
#include <pthread.h>
#endif
#include "dbutret.h"     /* return status' for dbut */
#include "utospub.h"     /* for utSeekTo and utOs calls */
#include "utlru.h"

#if OPSYS == UNIX
#include <unistd.h>
#include <fcntl.h>
#endif

#include <errno.h>

LOCALF gem_off_t utSeekToOffset(fdHandle_t  fd,   /* the file descriptor */
                    gem_off_t       bkaddr,         /* offset in file */
                    int           *pErrno);       /* system errno IN/OUT */

LOCALF gem_off_t utGetOffset(fdHandle_t fd, int *pErrno);

LOCALF gem_off_t utSeekFromCurrent(fdHandle_t fd,  /* the file descriptor */
                       gem_off_t      bkaddr,        /* offset in file */
                       int          *pErrno);      /* system errno IN/OUT */

LOCALF gem_off_t utSeekFromOffset(fdHandle_t fd,   /* the file descriptor */
                      gem_off_t       bkaddr,         /* offset in file */
                      int          *pErrno);       /* system errno IN/OUT  */


/* This section begins all UNIX only operations, following this section are 
   all of the WIN32API operations. Be careful no to intermix them. */
#if OPSYS == UNIX 

/* PROGRAM: utSeekToOffset - move file pointer to specified offset
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: offset
 *          DBUT_S_SEEK_TO_FAILED
 */
LOCALF gem_off_t
utSeekToOffset(fdHandle_t   fd,     /* the file descriptor */
               gem_off_t    offset,  /* offset in file */
               int	    *pErrno) /* system errno IN/OUT */
{
    os_off_t ret;		/* utility return code */
    int    retrycnt = 0;

    while ( (ret = lseek(fd, offset, SEEK_SET)) == (os_off_t)-1 &&
	   errno == EINTR && retrycnt++ < 100);

    if (ret == -1)
    {
        *pErrno = errno;
        return DBUT_S_SEEK_TO_FAILED;
    }
    return (gem_off_t)ret;
}


/* PROGRAM: utSeekFromCurrent - move file pointer to specified offset from end
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: offset
 *          DBUT_S_SEEK_FROM_FAILED
 */
LOCALF gem_off_t
utSeekFromCurrent(fdHandle_t    fd,	  /* the file descriptor */
                 gem_off_t      offset,   /* offset from current in file */
                 int           *pErrno)   /* system errno IN/OUT */
{
    os_off_t ret;		  /* return from system call */
    int   retrycnt = 0;
		
    while ( (ret = lseek(fd, offset, SEEK_CUR)) == (os_off_t)-1 &&
	   errno == EINTR && retrycnt++ < 100);

    if (ret == -1)
    {
        *pErrno = errno;
        return DBUT_S_SEEK_FROM_FAILED;
    }
    return (gem_off_t)ret;
}


/* PROGRAM: utSeekFromOffset - move file pointer to specified offset from end
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: offset
 *          DBUT_S_SEEK_FROM_FAILED
 */
LOCALF gem_off_t
utSeekFromOffset(fdHandle_t fd,	/* the file descriptor */
                 gem_off_t     offset,   /* offset in file */
                 int	      *pErrno)  /* system errno IN/OUT */
{
    os_off_t ret;		  /* return from system call */
    int   retrycnt = 0;
		
    while ( (ret = lseek(fd, offset, SEEK_END)) == (os_off_t)-1 &&
	   errno == EINTR && retrycnt++ < 100);

    if (ret == -1)
    {
        *pErrno = errno;
        return DBUT_S_SEEK_FROM_FAILED;
    }
    return (gem_off_t)ret;
}


/* PROGRAM: utGetOffset - return the offset into the file
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: offset
 *          DBUT_S_GET_OFFSET_FAILED
 */
LOCALF gem_off_t
utGetOffset(fdHandle_t fd, int *pErrno)
{
    os_off_t ret;		/* utility return code */
    int     retrycnt = 0;
		

    while ( (ret = lseek(fd, 0L, SEEK_CUR)) == (os_off_t)-1 &&
	   errno == EINTR && retrycnt++ < 100 );

    if (ret == -1)
    {
       *pErrno = errno;
        return DBUT_S_GET_OFFSET_FAILED;
    }

    return (gem_off_t)ret;
}


/* PROGRAM: utReadFromFile - read from specified offset for # of bytes
 *
 * MT SAFE: This is a thread safe interface if PRO_REENTRANT is define.
 *          Note however that the correct locking mutex is
 *          supplied by the caller as part of the fileHandle
 *          or thread safeness is NOT ensured.
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_READ_FROM_FAILED
 *          DBUT_S_SEEK_TO_FAILED
 */
LONG
utReadFromFile (fileHandle_t  fileHandle,  /* the file handle */
                gem_off_t     offset,      /* offset in file */
                TEXT	      *pbuf,       /* the buffer */
                psc_bit_t     len,         /* bytes to read */
                int           *pbytesRead, /* amount read IN/OUT */
                int	      *pErrno)     /* system errno IN/OUT */
{
   gem_off_t retlseek;	/* return code */
   LONG	 ret;		/* return code */
   int   retrycnt=0;
		
   fdHandle_t    fd;
   fileHandle_t  closeHandle = (fileHandle_t)0;

#ifdef PRO_REENTRANT
   pthread_mutex_lock((pthread_mutex_t *)&fileHandle->fh_ioLock);

   /* Make sure the hander we are writing to is open */
   if (fileHandle->fh_state == FH_CLOSED)
   {
       fileHandle->fh_fd = open(fileHandle->fh_path, fileHandle->fh_oflag, 
                                              fileHandle->fh_createMask);
       if (fileHandle->fh_fd == -1)
       {
           *pErrno = errno;
           return -1;
       }
       fileHandle->fh_state = FH_OPEN;
   }

   /* bring this to the top of the LRU chain and evict is necessary */
#ifdef FDCACHE_DEBUG
   utWalkFdCache(fh_anchor, "BEGIN:readFromFile");
#endif
   closeHandle = (fileHandle_t)utDoLru(fh_anchor, fileHandle);
#ifdef FDCACHE_DEBUG
   utWalkFdCache(fh_anchor, "END:readFromFile");
#endif
   if (closeHandle)
   {
#ifdef FDCACHE_DEBUG
       printf("EVICT:readFromFile %s\n",closeHandle->fh_path);
#endif
       /* had to evict, close the evicted fd and mark it's state */
       pthread_mutex_lock((pthread_mutex_t *)&closeHandle->fh_ioLock);
       close(closeHandle->fh_fd);
       closeHandle->fh_state = FH_CLOSED;
       pthread_mutex_unlock((pthread_mutex_t *)&closeHandle->fh_ioLock);
   }

   fd = fileHandle->fh_fd;
#else
   fd = fileHandle;
#endif

    retlseek = utSeekToOffset(fd, offset, pErrno);

    if (retlseek != DBUT_S_SEEK_TO_FAILED)
    {
        while ( (*pbytesRead = read(fd, pbuf, len)) == -1 &&
	   errno == EINTR && retrycnt++ < 100);

        if ((*pbytesRead == -1) && (errno == EINTR) )
              printf("\nread interrupted 100 times!\n");

        *pErrno = errno;
        ret = ((*pbytesRead != (int)len) ? DBUT_S_READ_FROM_FAILED
                                       : DBUT_S_SUCCESS);
    }
    else
    {
        ret = retlseek;
    }

#ifdef PRO_REENTRANT
    pthread_mutex_unlock((pthread_mutex_t *)&fileHandle->fh_ioLock);
#endif

    return ret;

}  /* end utReadFromFile (UNIX thread safe version) */


/* PROGRAM: utTruncateFile - Moves file pointer from EOF to current position
 *          Note that this function uses ftruncate(), a UNIX system
 *          call. The WIN32API version of utTruncate() implements a
 *          WIN32API only system call. 
 *
 * MT SAFE: This is a thread safe interface if PRO_REENTRANT is defined.
 *          Note however that the correct locking mutex is
 *          supplied by the caller as part of the fileHandle
 *          or thread safeness is NOT ensured.
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_WRITE_TO_FAILED
 */
LONG
utTruncateFile (fileHandle_t  fileHandle,     /* file handle */
                gem_off_t  offset,         /* offset in file */
                int           *pErrno)        /* system error */
{
   LONG        ret = DBUT_S_WRITE_TO_FAILED;  /* return code */
   fdHandle_t  fd;                            /* file handle */
   fileHandle_t  closeHandle = (fileHandle_t)0;

#ifdef PRO_REENTRANT
   pthread_mutex_lock((pthread_mutex_t *)&fileHandle->fh_ioLock);

   /* Make sure the hander we are writing to is open */
   if (fileHandle->fh_state == FH_CLOSED)
   {
       fileHandle->fh_fd = open(fileHandle->fh_path, fileHandle->fh_oflag, 
                                              fileHandle->fh_createMask);
       if (fileHandle->fh_fd == -1)
       {
           *pErrno = errno;
           return -1;
       }
       fileHandle->fh_state = FH_OPEN;
   }

   /* bring this to the top of the LRU chain and evict is necessary */
#ifdef FDCACHE_DEBUG
   utWalkFdCache(fh_anchor, "Begin:truncateFile");
#endif
   closeHandle = (fileHandle_t)utDoLru(fh_anchor, fileHandle);
#ifdef FDCACHE_DEBUG
   utWalkFdCache(fh_anchor, "Begin:truncateFile");
#endif
   if (closeHandle)
   {
       /* had to evict, close the evicted fd and mark it's state */
#ifdef FDCACHE_DEBUG
       printf("EVICT:truncateFile %s\n",closeHandle->fh_path);
#endif
       pthread_mutex_lock((pthread_mutex_t *)&closeHandle->fh_ioLock);
       close(closeHandle->fh_fd);
       closeHandle->fh_state = FH_CLOSED;
       pthread_mutex_unlock((pthread_mutex_t *)&closeHandle->fh_ioLock);
   }

   fd = fileHandle->fh_fd;
#else
   fd = fileHandle;
#endif

    /* Do the truncation                                                   
       NOTE: a zero value indicates success                           
             a non-zero value indicates failure */ 
    ret = ((ftruncate(fd, offset) != 0) ? DBUT_S_WRITE_TO_FAILED
				   : DBUT_S_SUCCESS);
    *pErrno = errno;

#ifdef PRO_REENTRANT
    pthread_mutex_unlock((pthread_mutex_t *)&fileHandle->fh_ioLock);
#endif

    return ret;

}  /* end utTruncateFile (UNIX thread safe version) */


/* PROGRAM: utReadFromFileSeq - read from current file pointer location
 *                              for # of bytes
 *
 * MT SAFE: This is a thread safe interface if PRO_REENTRANT is defined.
 *          Note however that the correct locking mutex is
 *          supplied by the caller as part of the fileHandle
 *          or thread safeness is NOT ensured.
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_READ_FROM_FAILED
 */
LONG
utReadFromFileSeq (fileHandle_t  fileHandle,  /* the file handle */
                   TEXT	         *pbuf,       /* the buffer */
                   psc_bit_t     len,         /* bytes to read */
                   int           *pbytesRead, /* amount read IN/OUT */
                   int	         *pErrno)     /* system errno IN/OUT */
{
   LONG        ret;                           /* return code */
   int         retrycnt = 0;                  /* read retry counter */
   fdHandle_t  fd;                            /* file handle */
   fileHandle_t  closeHandle = (fileHandle_t)0;

#ifdef PRO_REENTRANT
   pthread_mutex_lock((pthread_mutex_t *)&fileHandle->fh_ioLock);

   /* Make sure the hander we are writing to is open */
   if (fileHandle->fh_state == FH_CLOSED)
   {
       fileHandle->fh_fd = open(fileHandle->fh_path, fileHandle->fh_oflag, 
                                              fileHandle->fh_createMask);
       if (fileHandle->fh_fd == -1)
       {
           *pErrno = errno;
           return -1;
       }
       fileHandle->fh_state = FH_OPEN;
   }

   /* bring this to the top of the LRU chain and evict is necessary */
#ifdef FDCACHE_DEBUG
   utWalkFdCache(fh_anchor, "BEGIN:readFromFileSEQ");
#endif
   closeHandle = (fileHandle_t)utDoLru(fh_anchor, fileHandle);
#ifdef FDCACHE_DEBUG
   utWalkFdCache(fh_anchor, "END:readFromFileSEQ");
#endif
   if (closeHandle)
   {
#ifdef FDCACHE_DEBUG
       printf("EVICT:readFromFileSEQ %s\n",closeHandle->fh_path);
#endif
       /* had to evict, close the evicted fd and mark it's state */
       pthread_mutex_lock((pthread_mutex_t *)&closeHandle->fh_ioLock);
       close(closeHandle->fh_fd);
       closeHandle->fh_state = FH_CLOSED;
       pthread_mutex_unlock((pthread_mutex_t *)&closeHandle->fh_ioLock);
   }

   fd = fileHandle->fh_fd;
#else
   fd = fileHandle;
#endif

    /* retry the write 100 times before giving up */
    while ((*pbytesRead = read(fd, pbuf, len)) == -1 &&
       errno == EINTR && retrycnt++ < 100);

    if ((*pbytesRead == -1) && (errno == EINTR))
	  printf("\nread interrupted 100 times!\n");

    *pErrno = errno;
    ret = ((*pbytesRead != (int)len) ? DBUT_S_READ_FROM_FAILED
                                     : DBUT_S_SUCCESS);

#ifdef PRO_REENTRANT
    pthread_mutex_unlock((pthread_mutex_t *)&fileHandle->fh_ioLock);
#endif

    return ret;

}  /* end utReadFromFileSeq (UNIX thread safe version) */


/* PROGRAM: utWriteToFile - write # bytes at specified offset
 *
 * MT SAFE: This is a thread safe interface if PRO_REENTRANT is define.
 *          Note however that the correct locking mutex is
 *          supplied by the caller as part of the fileHandle
 *          or thread safeness is NOT ensured.
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_WRITE_TO_FAILED
 */
LONG
utWriteToFile(fileHandle_t  fileHandle,   	   /* the file descriptor */
              gem_off_t	    offset,        /* offset in file */
              TEXT	   *pbuf,          /* the buffer */
              psc_bit_t     len,	   /* bytes to write */
              int          *pbytesWritten, /* bytes written */
              int          *pErrno)        /* system errno IN/OUT */
{
    LONG ret = DBUT_S_SUCCESS; /* return code */
    gem_off_t retlseek;	/* return code */
    int  retrycnt=0;
    fdHandle_t    fd;
   fileHandle_t  closeHandle = (fileHandle_t)0;

#ifdef PRO_REENTRANT
   pthread_mutex_lock((pthread_mutex_t *)&fileHandle->fh_ioLock);

   /* Make sure the hander we are writing to is open */
   if (fileHandle->fh_state == FH_CLOSED)
   {
       fileHandle->fh_fd = open(fileHandle->fh_path, fileHandle->fh_oflag, 
                                            fileHandle->fh_createMask);
       if (fileHandle->fh_fd == -1)
       {
           *pErrno = errno;
           return -1;
       }
       fileHandle->fh_state = FH_OPEN;
   }

#ifdef FDCACHE_DEBUG
   utWalkFdCache(fh_anchor, "BEGIN:WritetoFile");
#endif
   /* bring this to the top of the LRU chain and evict is necessary */
   closeHandle = (fileHandle_t)utDoLru(fh_anchor, fileHandle);
#ifdef FDCACHE_DEBUG
   utWalkFdCache(fh_anchor, "END:WritetoFile");
#endif
   if (closeHandle)
   {
#ifdef FDCACHE_DEBUG
       printf("EVICT:WritetoFile %s\n",closeHandle->fh_path);
#endif
       /* had to evict, close the evicted fd and mark it's state */
       pthread_mutex_lock((pthread_mutex_t *)&closeHandle->fh_ioLock);
       close(closeHandle->fh_fd);
       closeHandle->fh_state = FH_CLOSED;
       pthread_mutex_unlock((pthread_mutex_t *)&closeHandle->fh_ioLock);
   }

   fd = fileHandle->fh_fd;
#else
   fd = fileHandle;
#endif
		
    retlseek = utSeekToOffset(fd, offset, pErrno);
    if (retlseek == DBUT_S_SEEK_TO_FAILED)
    {
        ret = retlseek;
        goto fatal;
    }

retry:
    *pbytesWritten = write(fd, pbuf, len);
 
    if ((psc_bit_t)*pbytesWritten != len)
    {
/* We get EINTR errno when we received a signal while doing a synchronous
   write.  For some OSs the write fails once and subsequently succeeds,
   while on others it forever after returns -1 with errno set to EINTR,
   thus the retry count */
           if( ++retrycnt < 100 && errno == EINTR && ret == -1)
           {
               goto retry;
           }
           *pErrno = errno;
           ret = DBUT_S_WRITE_TO_FAILED;
    }
    else
    {
        ret = DBUT_S_SUCCESS;
    }

fatal:

#ifdef PRO_REENTRANT
    pthread_mutex_unlock((pthread_mutex_t *)&fileHandle->fh_ioLock);
#endif

    return ret;

}  /* end utWriteToFile (UNIX thread safe version) */



/* PROGRAM: utWriteToFileSeq    - Write # bytes to a file at current file
 *                                pointer, then increment file pointer.
 *                                That is, write to the file in sequential
 *                                mode.
 * MT SAFE: This is a thread safe interface
 *          To use this function, the file must have been opened
 *          with the O_APPEND flag
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_DISK_FULL
 *          DBUT_S_WRITE_TO_FAILED
 *          DBUT_S_MAX_FILE_EXCEEDED
 */
LONG
utWriteToFileSeq(fileHandle_t   fileHandle,     /* the file descriptor */
                 TEXT           *pbuf,          /* the buffer */
                 psc_bit_t      len,            /* bytes to write */
                 int            *pbytesWritten, /* amount written */
                 int            *pErrno)        /* system errno IN/OUT */
{

    LONG       ret = DBUT_S_WRITE_TO_FAILED;    /* return code */
    int        retrycnt = 0;                    /* write retry counter */
    fdHandle_t fd;                              /* file handle */
    fileHandle_t  closeHandle = (fileHandle_t)0;
 
#ifdef PRO_REENTRANT
    pthread_mutex_lock((pthread_mutex_t *)&fileHandle->fh_ioLock);

   /* Make sure the hander we are writing to is open */
   if (fileHandle->fh_state == FH_CLOSED)
   {
       fileHandle->fh_fd = open(fileHandle->fh_path, fileHandle->fh_oflag, 
                                            fileHandle->fh_createMask);
       if (fileHandle->fh_fd == -1)
       {
           *pErrno = errno;
           return -1;
       }
       fileHandle->fh_state = FH_OPEN;
   }

   /* bring this to the top of the LRU chain and evict is necessary */
#ifdef FDCACHE_DEBUG
   utWalkFdCache(fh_anchor, "BEGIN:WriteToFileSEQ");
#endif
   closeHandle = (fileHandle_t)utDoLru(fh_anchor, fileHandle);
#ifdef FDCACHE_DEBUG
   utWalkFdCache(fh_anchor, "END:WriteToFileSEQ");
#endif
   if (closeHandle)
   {
#ifdef FDCACHE_DEBUG
       printf("EVICT:WriteToFileSEQ %s\n",closeHandle->fh_path);
#endif
       /* had to evict, close the evicted fd and mark it's state */
       pthread_mutex_lock((pthread_mutex_t *)&closeHandle->fh_ioLock);
       close(closeHandle->fh_fd);
       closeHandle->fh_state = FH_CLOSED;
       pthread_mutex_unlock((pthread_mutex_t *)&closeHandle->fh_ioLock);
   }

    fd = fileHandle->fh_fd;
#else
    fd = fileHandle;
#endif

    /* retry the write 100 times before giving up */
    while ((*pbytesWritten = write(fd, (char *)pbuf, len)) == -1 &&
           errno == EINTR && retrycnt++ < 100);

    *pErrno = errno;

    if (*pbytesWritten != (int)len)
    {
        switch (*pErrno)
        {
        case EFBIG:
            ret = DBUT_S_MAX_FILE_EXCEEDED;    /* Max file exceeded on UNIX */
        case ENOSPC:
            ret = DBUT_S_DISK_FULL;            /* Insufficient disk space */
        case EINTR:
            ret = DBUT_S_WRITE_TO_FAILED;
	}
    }
    else
    {
	ret = DBUT_S_SUCCESS;
    }


#ifdef PRO_REENTRANT
    pthread_mutex_unlock((pthread_mutex_t *)&fileHandle->fh_ioLock);
#endif

    return ret;

} /* end utWriteToFileSeq (UNIX thread safe version) */


/* for now, this function is a no-op */
 
int
utLockLkFile(fileHandle_t lkhandle _UNUSED_)
{
 
    return (0);
}
 
/* for now, this function is mostly a no-op */
 
int
utUnlockLkFile(fileHandle_t * lkhandle)
{
 
    utOsClose(*lkhandle, 0);
    *lkhandle = (fileHandle_t) 0;
    return (0);
}

#ifdef FDCACHE_DEBUG 
/* PROGRAM: utWalkFdCache - Walk the chain and dump out the values
 *
 * RETURNS: 0
 */
int
utWalkFdCache(lruAnchor_t * pAnchor, TEXT *p)
{
    fileHandle_t  ptr;
    fileHandle_t  phead;
    ULONG        i;

    printf("%s\n",p);
    if (!pAnchor)
    {
        printf("Anchor is empty\n");
        return 0;
    }

    printf("There are %ld entries left on the chain\n",pAnchor->entryCnt); 
    if (!pAnchor->phead)
    {
        printf("Chain is empty\n");
        return 0;
    }

    phead = (fileHandle_t)pAnchor->phead;
    printf("Head: %x, next: %x, prev: %x %s\n",phead,
            phead->fh_next, phead->fh_prev, phead->fh_path);
    i = 1;
    ptr = (fileHandle_t)phead->fh_next;
    while (ptr != phead)
    {
        i++;
        printf("[%ld]: %x, next: %x, prev: %x %s\n", i, ptr, ptr->fh_next, 
             ptr->fh_prev, ptr->fh_path);
        ptr = ptr->fh_next;
        if ((ptr->fh_next == ptr) && (ptr != phead))
        {
            printf("Chain is broken\n");
            break;
        }
    }
    printf("COMPLETE walk of chain\n");
    
    return 0;
}
#endif
#endif  /* OPSYS == UNIX */


/* This section begins all WIN32API only operations, following this section are 
   all of the UNIX operations. Be careful no to intermix them. */
#if OPSYS == WIN32API

extern int fWin95;

/* PROGRAM: utSeekToOffset - move file pointer to specified offset
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: offset
 *          DBUT_S_SEEK_TO_FAILED
 */
LOCALF gem_off_t
utSeekToOffset(fdHandle_t   fd,	     /* the file descriptor */
               gem_off_t	    offset,  /* offset in file */
               int	    *pErrno) /* system errno IN/OUT */
{
#ifdef DBUT_LARGEFILE
    LARGE_INTEGER theOffset;
    theOffset.QuadPart = offset;

    /* there is not an error which indicates the seek was interupted. */
    /* therefore there is no retry for WIN32API */
    theOffset.LowPart = SetFilePointer(fd, theOffset.LowPart,
                            &theOffset.HighPart, FILE_BEGIN);

    if ( (theOffset.LowPart == INVALID_SET_FILE_POINTER) &&
         (*pErrno = GetLastError()) != NO_ERROR )
    {
        theOffset.QuadPart = DBUT_S_SEEK_TO_FAILED;
    }

    return theOffset.QuadPart;
#else
    DWORD  dwSetPointerRet;  /* utility return code */

    /* there is not an error which indicates the seek was interupted. */
    /* therefor there is no retry for WIN32API */
    dwSetPointerRet = SetFilePointer(fd, offset, NULL, FILE_BEGIN);

    if (dwSetPointerRet == 0xFFFFFFFF)
    {
        *pErrno = GetLastError();
        dwSetPointerRet = DBUT_S_SEEK_TO_FAILED;
    }
    
    return dwSetPointerRet;
#endif
}


/* PROGRAM: utSeekFromCurrent - move file pointer to specified offset from end
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: offset
 *          DBUT_S_SEEK_FROM_FAILED
 */
LOCALF gem_off_t
utSeekFromCurrent(fdHandle_t fd,	 /* the file descriptor */
                 gem_off_t       offset,   /* offset from current in file */
                 int	       *pErrno)  /* system errno IN/OUT */
{
#ifdef DBUT_LARGEFILE
    LARGE_INTEGER theOffset;
    theOffset.QuadPart = offset;

    /* there is not an error which indicates the seek was interupted. */
    /* therefor there is no retry for WIN32API */
    theOffset.LowPart = SetFilePointer(fd, theOffset.LowPart,
                            &theOffset.HighPart, FILE_CURRENT);

    if ( (theOffset.LowPart == INVALID_SET_FILE_POINTER) &&
         (*pErrno = GetLastError()) != NO_ERROR )
    {
        theOffset.QuadPart = DBUT_S_SEEK_FROM_FAILED;
    }

    return theOffset.QuadPart;
#else
    DWORD  dwSetPointerRet;  /* utility return code */
		
    /* there is not an error which indicates the seek was interupted. */
    /* therefor there is no retry for WIN32API */
    dwSetPointerRet = SetFilePointer(fd, offset, NULL, FILE_CURRENT);

    if (dwSetPointerRet == 0xFFFFFFFF)
    {
        *pErrno = GetLastError();
         dwSetPointerRet = DBUT_S_SEEK_FROM_FAILED;
    }

    return dwSetPointerRet;
#endif
}


/* PROGRAM: utSeekFromOffset - move file pointer to specified offset from end
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: offset
 *          DBUT_S_SEEK_FROM_FAILED
 */
LOCALF gem_off_t
utSeekFromOffset(fdHandle_t fd,	/* the file descriptor */
                 gem_off_t      offset,   /* offset in file */
                 int	      *pErrno)  /* system errno IN/OUT */
{
#ifdef DBUT_LARGEFILE
    LARGE_INTEGER theOffset;
    theOffset.QuadPart = offset;

    /* there is not an error which indicates the seek was interupted. */
    /* therefor there is no retry for WIN32API */
    theOffset.LowPart = SetFilePointer(fd, theOffset.LowPart,
                            &theOffset.HighPart, FILE_END);

    if ( (theOffset.LowPart == INVALID_SET_FILE_POINTER) &&
         (*pErrno = GetLastError()) != NO_ERROR )
    {
        theOffset.QuadPart = DBUT_S_SEEK_FROM_FAILED;
    }

    return theOffset.QuadPart;
#else
    DWORD  dwSetPointerRet;  /* utility return code */

    /* there is not an error which indicates the seek was interupted. */
    /* therefor there is no retry for WIN32API */
    dwSetPointerRet = SetFilePointer(fd, offset, NULL, FILE_BEGIN);

    if (dwSetPointerRet == 0xFFFFFFFF)
    {
        *pErrno = GetLastError();
        dwSetPointerRet = DBUT_S_SEEK_FROM_FAILED;
    }

    return dwSetPointerRet;
#endif
}


/* PROGRAM: utGetOffset - return the offset into the file
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: offset
 *          DBUT_S_GET_OFFSET_FAILED
 */
LOCALF gem_off_t
utGetOffset(fdHandle_t fd, int *pErrno)
{
#ifdef DBUT_LARGEFILE
    LARGE_INTEGER theOffset;
    theOffset.QuadPart = 0;

    /* there is not an error which indicates the seek was interupted. */
    /* therefor there is no retry for WIN32API */
    theOffset.LowPart = SetFilePointer(fd, theOffset.LowPart,
                            &theOffset.HighPart, FILE_CURRENT);

    if ( (theOffset.LowPart == INVALID_SET_FILE_POINTER) &&
         (*pErrno = GetLastError()) != NO_ERROR )
    {
        theOffset.QuadPart = DBUT_S_GET_OFFSET_FAILED;
    }

    return theOffset.QuadPart;
#else
    DWORD  dwSetPointerRet;  /* utility return code */
		
    /* there is not an error which indicates the seek was interupted. */
    /* therefor there is no retry for WIN32API */
    dwSetPointerRet = SetFilePointer(fd, 0, NULL, FILE_CURRENT);

    if (dwSetPointerRet == 0xFFFFFFFF)
    {
       *pErrno = GetLastError();
        dwSetPointerRet = DBUT_S_GET_OFFSET_FAILED;
    }

    return dwSetPointerRet;
#endif
}


#ifdef PRO_REENTRANT

/* PROGRAM: utReadFromFile - read # bytes at specified offset
 *
 * MT SAFE: This is a thread safe interface
 *          To use this function, the device must have been opened
 *          with the FILE_FLAG_OVERLAPPED flag (only on NTFS - not on FAT)
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_READ_FROM_FAILED
 */
LONG
utReadFromFile(fileHandle_t fileHandle, /* the file handle */
                gem_off_t	   offset,      /* offset in file */
                TEXT	   *pbuf,       /* the buffer */
                psc_bit_t  len,         /* bytes to write */
                int        *pbytesRead,  /* amount read */
                int        *pErrno)     /* system errno IN/OUT */
{
  if (!fWin95)
  {
    GBOOL bReadFileReturn;      /* return for WriteFile */
    GBOOL bResult;              /* return for GetOverlappedResult */
    OVERLAPPED readOverlap;
    LONG       ret;
#ifdef DBUT_LARGEFILE
    LARGE_INTEGER theOffset;
    theOffset.QuadPart = offset;
#endif
		
retryRead:
    /* initialize the overlap structure */
#ifdef DBUT_LARGEFILE
    readOverlap.Offset = theOffset.LowPart;
    readOverlap.OffsetHigh = theOffset.HighPart;
#else
    readOverlap.Offset = offset;
    readOverlap.OffsetHigh = 0;
#endif
    readOverlap.hEvent = NULL;
 
    /* Begin the io operations and then get the status */
    bReadFileReturn = ReadFile(fileHandle, pbuf, len, NULL, &readOverlap);
    *pErrno = GetLastError();
   
    /* Did the io request get queued? */
    if ( (bReadFileReturn == FALSE) && 
         (*pErrno != ERROR_IO_PENDING) )
    {
        switch (*pErrno)
        {
         case ERROR_INVALID_USER_BUFFER:
         case ERROR_NOT_ENOUGH_MEMORY:
              /* the non-paged pool for outstanding io requests is full   */
              /* try again. always retry until there is room on the queue */
              goto retryRead;
         case ERROR_NOT_ENOUGH_QUOTA:
         default:
              ret = DBUT_S_READ_FROM_FAILED;
              *pbytesRead = -1;
              break;
        }
    }
    else
    {
        /* io request has queued and will complete later. GetOverlappedResult
           will block until the io completes and  will receive the amount
           of data transfered. The fourth parameter set to true tells
           GetOverlappedResult to wait until the io operation has completed
        */
retryWait:
        bResult = GetOverlappedResult(fileHandle, &readOverlap,
                                      pbytesRead, TRUE);
        if (bResult == FALSE)
        {
           *pErrno = GetLastError();
           if (*pErrno == ERROR_IO_PENDING)
           {
               /* io is still waiting to complete, wait again */
               /* loop until io completes */
               goto retryWait;
           }
           else
           {
               ret = DBUT_S_READ_FROM_FAILED;
               *pbytesRead = -1;
           }
        }
        else
        {
            ret = DBUT_S_SUCCESS;
        }
    }

    return ret;
  }
  else
  {
    /* Windows 9x only */
    LONG	 ret;
    GBOOL  bReadReturn;
    gem_off_t lSeekReturn;
		
    lSeekReturn = utSeekToOffset(fileHandle, offset, pErrno); 

    /* this requires the api change to take pbytesRead as in/out parm
       and the the function returns a LONG */
    if (lSeekReturn != DBUT_S_SEEK_TO_FAILED)
    {
        bReadReturn = ReadFile(fileHandle, (void *)pbuf, (DWORD)len, 
                               pbytesRead, NULL);
        
        if ((bReadReturn == FALSE) || ((psc_bit_t)*pbytesRead != len))
        {
            *pErrno = GetLastError();
            ret = DBUT_S_READ_FROM_FAILED;
        }
        else 
        {
           ret = DBUT_S_SUCCESS;
        }
    }
    else
    {
       ret = (LONG)lSeekReturn;
    }

    return ret;
  }
}  /* end utReadFromFile (WIN32 thread re-entrant version) */

/* PROGRAM: utReadFromFileSeq - read from current file pointer location
 *                              for # of bytes
 *
 * MT SAFE: THIS IS NOT A THREAD-SAFE INTERFACE - IT IS IN THE THREAD-SAFE
 *          CODE BECAUSE IT IS CURRENTLY ONLY CALLED AT INITIALIZATION AND
 *          DOESN'T NEED TO BE THREAD-SAFE.  THIS SHOULD BE MADE THREAD-SAFE
 *          IF IT IS CALLED FOR ANY OTHER REASON.
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_READ_FROM_FAILED
 */
LONG
utReadFromFileSeq(fileHandle_t fileHandle, /* the file descriptor */
                  TEXT        *pbuf,       /* the buffer */
                  psc_bit_t   len,         /* bytes to read */
                  int         *pbytesRead, /* amount read IN/OUT */
                  int	      *pErrno)     /* system errno IN/OUT */
{
   LONG	 ret = DBUT_S_READ_FROM_FAILED;
   GBOOL  bReadReturn;
		
        bReadReturn = ReadFile(fileHandle, (void *)pbuf, (DWORD)len, 
                               pbytesRead, NULL);
        
        if ((bReadReturn == FALSE) || ((psc_bit_t)*pbytesRead != len))
        {
            *pErrno = GetLastError();
            ret = DBUT_S_READ_FROM_FAILED;
        }
        else 
        {
           ret = DBUT_S_SUCCESS;
        }

   return ret;

}  /* end utReadFromFileSeq (WIN32 non-thread safe version) */

#else /* !PRO_REENTRANT */


/* PROGRAM: utReadFromFile - read from specified offset for # of bytes
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_READ_FROM_FAILED
 *          DBUT_S_SEEK_TO_FAILED
 */
LONG
utReadFromFile (fileHandle_t fileHandle, /* the file descriptor */
                gem_off_t	     offset,     /* offset in file */
                TEXT	     *pbuf,      /* the buffer */
                psc_bit_t    len,        /* bytes to read */
                int          *pbytesRead,  /* amount read IN/OUT */
                int	     *pErrno)     /* system errno IN/OUT */
{
   LONG	 ret;
   GBOOL  bReadReturn;
   LONG  lSeekReturn;
		
    lSeekReturn = utSeekToOffset(fileHandle, offset, pErrno); 

    /* this requires the api change to take pbytesRead as in/out parm
       and the the function returns a LONG */
    if (lSeekReturn != DBUT_S_SEEK_TO_FAILED)
    {
        bReadReturn = ReadFile(fileHandle, (void *)pbuf, (DWORD)len, 
                               pbytesRead, NULL);
        
        if ((bReadReturn == FALSE) || ((psc_bit_t)*pbytesRead != len))
        {
            *pErrno = GetLastError();
            ret = DBUT_S_READ_FROM_FAILED;
        }
        else 
        {
           ret = DBUT_S_SUCCESS;
        }
    }
    else
    {
       ret = lSeekReturn;
    }

   return ret;
}


/* PROGRAM: utReadFromFileSeq - read from current file pointer location
 *                              for # of bytes
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_READ_FROM_FAILED
 */
LONG
utReadFromFileSeq(fileHandle_t fileHandle, /* the file descriptor */
                  TEXT        *pbuf,       /* the buffer */
                  psc_bit_t   len,         /* bytes to read */
                  int         *pbytesRead, /* amount read IN/OUT */
                  int	      *pErrno)     /* system errno IN/OUT */
{
   LONG	 ret = DBUT_S_READ_FROM_FAILED;
   GBOOL  bReadReturn;
		
        bReadReturn = ReadFile(fileHandle, (void *)pbuf, (DWORD)len, 
                               pbytesRead, NULL);
        
        if ((bReadReturn == FALSE) || ((psc_bit_t)*pbytesRead != len))
        {
            *pErrno = GetLastError();
            ret = DBUT_S_READ_FROM_FAILED;
        }
        else 
        {
           ret = DBUT_S_SUCCESS;
        }

   return ret;

}  /* end utReadFromFileSeq (WIN32 non-thread safe version) */

#endif  /* PRO_REENTRANT */


/* PROGRAM: utTruncateFile - Moves file pointer from EOF to current position
 *          Note that this function uses SetEndOfFile(), a WIN32API only system
 *          call. The UNIX version of utTruncate() implements a
 *          UNIX only system call. 
 *
 * MT SAFE: This is not a thread safe interface.
 *
 * RETURNS: non zero 
 *          zero
 */
LONG
utTruncateFile (fileHandle_t  fileHandle,        /* the file handle */
                gem_off_t  offset,            /* offset in file */
                int           *pErrno)           /* system error */
{
    LONG        ret = DBUT_S_WRITE_TO_FAILED;    /* return code */
    fdHandle_t  fd;                              /* file handle */
    gem_off_t   lSeekReturn;

    fd = fileHandle;

    lSeekReturn = utSeekToOffset(fd, offset, pErrno);
    if (lSeekReturn != DBUT_S_SEEK_TO_FAILED)
    {
        /* Do the truncation                                                   
           NOTE: a non-zero value indicates success                           
                 a zero value indicates failure */ 
        ret = ((SetEndOfFile(fd) == 0) ? DBUT_S_WRITE_TO_FAILED
				   : DBUT_S_SUCCESS);
        *pErrno = GetLastError();
    }
    return ret;

}   /* end utTruncateFile (WIN32 non-thread safe version) */


#ifdef PRO_REENTRANT

/* PROGRAM: utWriteToFile - write # bytes at specified offset
 *
 * MT SAFE: This is a thread safe interface
 *          To use this function, the device must have been opened
 *          with the FILE_FLAG_OVERLAPPED flag
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_WRITE_TO_FAILED
 */
LONG
utWriteToFile(fileHandle_t fileHandle,         /* the file handle */
                gem_off_t	   offset,       /* offset in file */
                TEXT	   *pbuf,        /* the buffer */
                psc_bit_t  len,          /* bytes to write */
                int        *pbytesWritten,/* amount written */
                int        *pErrno)      /* address for errno */
{
  if (!fWin95)
  {
    GBOOL bWriteFileReturn;     /* return for WriteFile */
    GBOOL bResult;              /* return for GetOverlappedResult */
    OVERLAPPED writeOverlap;
    LONG       ret;
#ifdef DBUT_LARGEFILE
    LARGE_INTEGER theOffset;
    theOffset.QuadPart = offset;
#endif

retryWrite:
    /* initialize the overlap structure */
#ifdef DBUT_LARGEFILE
    writeOverlap.Offset = theOffset.LowPart;
    writeOverlap.OffsetHigh = theOffset.HighPart;
#else
    writeOverlap.Offset = offset;
    writeOverlap.OffsetHigh = 0;
#endif
    writeOverlap.hEvent = NULL;
 
    /* Begin the io operations and then get the status */
    bWriteFileReturn = WriteFile(fileHandle, pbuf, len, NULL, &writeOverlap);
    *pErrno = GetLastError();
   
    /* Did the io request get queued? */
    if ( (bWriteFileReturn == FALSE) && 
         (*pErrno != ERROR_IO_PENDING) )
    {
        switch (*pErrno)
        {
         case ERROR_INVALID_USER_BUFFER:
         case ERROR_NOT_ENOUGH_MEMORY:
              /* the non-paged pool for outstanding io requests is full   */
              /* try again. always retry until there is room on the queue */
              goto retryWrite;
         case ERROR_NOT_ENOUGH_QUOTA:
              /* TODO: fatal for now, should we try and increase the */
              /* working set on the fly? */
              ret = DBUT_S_WRITE_TO_FAILED;
              *pbytesWritten = -1;
              break;
         default:
              ret = DBUT_S_WRITE_TO_FAILED;
              *pbytesWritten = -1;
              break;
        }
    }
    else
    {
        /* io request has queued and will complete later GetOverlappedResult
           will block until the io completes and  will receive the amount
           of data transfered. The fourth parameter set to true tells
           GetOverlappedResult to wait until the io operation has completed
        */
retryWait:
        bResult = GetOverlappedResult(fileHandle, &writeOverlap,
                                      pbytesWritten, TRUE);
        if (bResult == FALSE)
        {
           *pErrno = GetLastError();
           switch (*pErrno)
           {
               case ERROR_IO_PENDING:
                   /* io is still waiting to complete, wait again */
                   /* loop until io completes */
                   goto retryWait;
               case ERROR_DISK_FULL:
               case ERROR_HANDLE_DISK_FULL:
               default:
                   ret = DBUT_S_WRITE_TO_FAILED;
                   *pbytesWritten = -1;
                   break;
           }
        }
        else
        {
            ret = DBUT_S_SUCCESS;
        }
    }
 
    return ret;
  }
  else
  {
    /* Windows 9x only */
    GBOOL bWriteFileReturn; /* return for WriteFile */
    gem_off_t retseek;      /* return for utSetFileOffset */
    LONG ret;              /* for return */ 

    retseek = utSeekToOffset(fileHandle, offset, pErrno);
 
    if (retseek == DBUT_S_SEEK_TO_FAILED)
    {
        ret = (LONG)retseek;
        goto writeExit;
    }
 
    bWriteFileReturn = WriteFile(fileHandle, pbuf, len, pbytesWritten, NULL);
   
    if ((bWriteFileReturn == FALSE) || ((psc_bit_t)*pbytesWritten != len))
    {
        *pErrno = GetLastError();
        ret = DBUT_S_WRITE_TO_FAILED;
    }
    else
    {
       ret = DBUT_S_SUCCESS;
    }
 
writeExit:
        return ret;
  }
}  /* end utWriteToFile  (WIN32 thread re-entrant version) */

#else  /* !PRO_REENTRANT */

/* PROGRAM: utWriteToFile - write # bytes at specified offset
 *
 * MT SAFE: This is not a thread safe interface
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_WRITE_TO_FAILED
 */
LONG
utWriteToFile(fileHandle_t fileHandle,     /* the file handle */
              gem_off_t offset,         /* offset in file */
              TEXT	   *pbuf,          /* the buffer */
              psc_bit_t    len,	           /* bytes to write */
              int          *pbytesWritten,  /* bytes to write */
              int          *pErrno)        /* system errno IN/OUT */
{
    GBOOL bWriteFileReturn; /* return for WriteFile */
    LONG retseek;          /* return for utSetFileOffset */
    LONG ret;              /* for return */ 
		

    retseek = utSeekToOffset(fileHandle, offset, pErrno);
 
    if (retseek == DBUT_S_SEEK_TO_FAILED)
    {
        ret = retseek;
        goto writeExit;
    }
 
    bWriteFileReturn = WriteFile(fileHandle, pbuf, len, pbytesWritten, NULL);
   
    if ((bWriteFileReturn == FALSE) || ((psc_bit_t)*pbytesWritten != len))
    {
        *pErrno = GetLastError();
        ret = DBUT_S_WRITE_TO_FAILED;
    }
    else
    {
       ret = DBUT_S_SUCCESS;
    }
 
writeExit:
        return ret;

}  /* end utWriteToFile (WIN32 non-thread safe version) */


/* PROGRAM: utWriteToFileSeq    - Write # bytes to a file at current file
 *                                pointer, then increment file pointer.
 *                                That is, write to the file in sequential
 *                                mode.
 * MT SAFE: This is not a thread safe interface
 *          To use this function, the file must have been opened
 *          with the O_APPEND flag
 *
 * RETURNS: DBUT_S_SUCCESS
 *          DBUT_S_DISK_FULL
 *          DBUT_S_WRITE_TO_FAILED
 *          DBUT_S_MAX_FILE_EXCEEDED
 */
LONG
utWriteToFileSeq(fileHandle_t   fileHandle,     /* the file descriptor */
                 TEXT           *pbuf,          /* the buffer */
                 psc_bit_t      len,            /* bytes to write */
                 int            *pbytesWritten, /* amount written */
                 int            *pErrno)        /* system errno IN/OUT */
{

    /* NOTE NOTE NOTE NOTE NOTE
     * the following code does not work well when two processes
     * are writting to the log file simultanously.  See the code
     * in jutil/ju/julog.c for more details.  eventually, the code in
     * julog.c should somehow be migrated to this module, where all
     * thread-safe os dependent code lives.
     */
    GBOOL bWriteFileReturn;     /* return for WriteFile */

    bWriteFileReturn = WriteFile(fileHandle, pbuf, len, pbytesWritten, NULL);
    *pErrno = GetLastError();

    if (bWriteFileReturn == FALSE)
    {
        switch (*pErrno)
        {
        case ERROR_DISK_FULL:
            *pbytesWritten = -1;
            return DBUT_S_DISK_FULL;        /* Insufficient disk space */
        default:
            *pbytesWritten = -1;
            return DBUT_S_WRITE_TO_FAILED;
        }
    }
    return DBUT_S_SUCCESS;

} /* end utWriteToFileSeq (WIN32 non-thread safe version) */

#endif /* PRO_REENTRANT */

int
utLockLkFile(fileHandle_t lkhandle)
{
    int       locked = 0;
 
    locked = LockFile(lkhandle,1024,0,4,0);
    return (locked);
}
 
int
utUnlockLkFile(fileHandle_t * lkhandle)
{
    int       unlocked = 0;
 
    unlocked = UnlockFile(*lkhandle,1024,0,4,0);
    unlocked = CloseHandle(*lkhandle);
    *lkhandle = (fileHandle_t) 0;
     return (unlocked);
}
 
#endif  /* OPSYS == WIN32API */
