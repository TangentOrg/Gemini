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

#ifndef UTOSPUB_H
#define UTOSPUB_H
 
#ifdef PRO_REENTRANT
#include <prothread.h>
#endif

#if defined(DBUT_LARGEFILE) || _FILE_OFFSET_BITS == 64
typedef LONG64 gem_off_t;
#else
typedef LONG gem_off_t;
#endif

#define UT_DELETE(filename) unlink(filename)

#ifndef FILE_HANDLE_DEFINED
#define FILE_HANDLE_DEFINED
 
/* typedef the data types for file descriptors and file handles       */


#if OPSYS == UNIX
#include "utlru.h"
#ifdef FH_ANCHOR_GLOB
lruAnchor_t    *fh_anchor = (lruAnchor_t *)0;
#else
lruAnchor_t    *fh_anchor;
#endif
/* On UNIX systems, the file handle is to be an int fd.
 * If we are to be thread safe on these UNIX system, then
 * the fileHande is a structure containing the int fd and an ioLock.
 */
typedef int fdHandle_t;
#ifdef PRO_REENTRANT
struct fileHandle
{
    struct fileHandle  *fh_next;
    struct fileHandle  *fh_prev;
    fdHandle_t          fh_fd;
    pthread_mutex_t     fh_ioLock;
    int                 fh_state;    /* 1 = OPEN, 0 = CLOSED */
    int                 fh_oflag;    
    int                 fh_createMask; 
    char                fh_path[MAXPATHN];
};
typedef struct fileHandle *fileHandle_t;
#define FH_CLOSED       0
#define FH_OPEN         1

#else
typedef int     fileHandle_t;
#endif
#endif
 
#if OPSYS == WIN32API
typedef HANDLE  fileHandle_t;
typedef HANDLE  fdHandle_t;
#endif
 
#endif /* FILE_HANDLE_DEFINED */

#if OPSYS == WIN32API
#define  INVALID_FILE_HANDLE INVALID_HANDLE_VALUE
#define  INVALID_FD_HANDLE   INVALID_HANDLE_VALUE
#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#endif /* ifndef INVALID_SET_FILE_POINTER */
#else
#define  INVALID_FILE_HANDLE (fileHandle_t)-1
#define  INVALID_FD_HANDLE   (fdHandle_t)-1
#endif  /* OPSYS == WIN32API */

/* constants for turning autoerr on and off */
#define AUTO_ERROR_OFF 0
#define AUTO_ERROR_ON  1

/* define open modes for utsOsOpen */

#include <fcntl.h>


#if OPSYS != WIN32API
typedef off_t os_off_t;
#define O_BINARY (0)
#define O_TEXT   (0)
#endif /* OPSYS != WIN32API */

/* O_RDONLY 0x0000
   O_WRONLY 0x0001
   O_RDWR   0x0002
   O_APPEND 0x0008
   O_CREATE 0x0100
   O_TRUNC  0x0200 
*/

#define OPEN_R             (O_RDONLY | O_BINARY)
#define OPEN_W             (O_WRONLY | O_BINARY)
#define OPEN_RW            (O_RDWR   | O_BINARY)
#define OPEN_RW_APPEND     (O_RDWR   | O_BINARY | O_APPEND)
#define OPEN_W_APPEND_TEXT (O_WRONLY | O_APPEND | O_TEXT)
#define OPEN_RW_TRUNC      (O_RDWR   | O_BINARY | O_TRUNC)
#define CREATE_RW          (O_RDWR   | O_BINARY | O_CREAT)
#define CREATE_RW_APPEND   (O_RDWR   | O_BINARY | O_CREAT | O_APPEND)
#define CREATE_RW_TRUNC    (O_RDWR   | O_BINARY | O_CREAT | O_TRUNC)

/* Code to create a non-executable file */

#if OPSYS == WIN32API
#include <sys\stat.h>
#define CREATE_DATA_FILE (S_IREAD | S_IWRITE)
#else
#define CREATE_DATA_FILE (0666)
#endif

/* prototypes for utosrw.c, os seek, read, write */
DLLEXPORT LONG utWriteToFile(fileHandle_t fileHandle,  /* the file handle */
                  gem_off_t       bkaddr,         /* offset in file */
                  TEXT         *pbuf,          /* the buffer */
                  unsigned      len,            /* bytes to write */
                  int          *bytesWritten,  /* amount written */
                  int          *pErrno);       /* system errno IN/OUT */

DLLEXPORT LONG utWriteToFileSeq(fileHandle_t fd, /* the file descriptor */
                 TEXT           *pbuf,           /* the buffer */
                 unsigned       len,             /* bytes to write */
                 int            *pbytesWritten,  /* amount written */
                 int            *pErrno);        /* system errno IN/OUT */

DLLEXPORT LONG utReadFromFile (fileHandle_t fileHandle,  /* file handle */
                    gem_off_t      bkaddr,         /* offset in file */
                    TEXT         *pbuf,          /* the buffer */
                    unsigned     len,            /* bytes to read */
                    int          *bytesRead,     /* amount read */
                    int          *pErrno);       /* system errno IN/OUT */

DLLEXPORT LONG utReadFromFileSeq (fileHandle_t fileHandle,  /* file handle */
                    TEXT         *pbuf,          /* the buffer */
                    unsigned     len,            /* bytes to read */
                    int          *pbytesRead,    /* amount read */
                    int          *pErrno);       /* system errno IN/OUT */

DLLEXPORT LONG utTruncateFile (fileHandle_t  fileHandle, /* file handle */
                               gem_off_t  offset,     /* offset in file */
                               int           *pErrno);   /* system errno */

DLLEXPORT DSMVOID utInitFdCache(ULONG numEntries);

/* prototypes for utosopen.c */

DLLEXPORT LONG utCheckFname(TEXT *pFileName);

DLLEXPORT fileHandle_t utOsOpenTemp(TEXT  *tname, 
                                    TEXT  *pTmpDir,
                                    GTBOOL  unlinkTempDB,
                                    int   *errorStatus);
  

DLLEXPORT fileHandle_t utOsOpen(TEXT *pFileName,   /* name of file to open */
                      int openFlags,     /* open flags */
                      int createMask,    /* creation permissions mask */
                      UCOUNT ioFlags,    /* ORing of utfio.h flags */
                      int *errorStatus);  /* errror holder, taken from errno */
 
DLLEXPORT fileHandle_t utOsCreat (TEXT *pFileName,  /* name of file to open */
                        int createMask,   /* creation permissions mask */
                        UCOUNT ioFlags,   /* ORing of utfio.h flags */
                        int *errorStatus); /* errror holder, taken from errno */

/* prototypes for utosclose.c */
DLLEXPORT DSMVOID utOsCloseTemp (fileHandle_t  fd,
               TEXT           *tname,
               TEXT           *pTmpDir);

DLLEXPORT int utOsClose (fileHandle_t hFile,    /* close this file descriptor */
           UCOUNT       mode); 

DLLEXPORT int utOsFlush (fileHandle_t hFile);   /* flush this file descriptor */

DLLEXPORT DSMVOID utclex(fileHandle_t fd);     /* the file descriptor */

DLLEXPORT DSMVOID utCloseFiles(DSMVOID);

DLLEXPORT LONG utGetTableSize();

DLLEXPORT int utLockLkFile(fileHandle_t lkhandle);

DLLEXPORT int utUnlockLkFile(fileHandle_t * lkhandle);

/*** utstack.c ***/
#ifndef WINNT
DLLEXPORT DSMVOID uttraceback(int fd);
#endif
DLLEXPORT DSMVOID uttrace();
DLLEXPORT DSMVOID utcore();


#endif /* UTOSPUB_H */


