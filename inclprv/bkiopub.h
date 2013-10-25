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

#ifndef BKIOPUB_H
#define BKIOPUB_H

#include "dbutpub.h"

/* public function prototypes for the bkio layer */

#if defined(PRO_REENTRANT) && OPSYS == UNIX
#include <pthread.h>
#endif

struct dsmContext;

/****************************************************/
/* Public structure definitions for bkio subsystem  */
/* see bkftbl.h for how this is used.               */
/****************************************************/

typedef struct bkiostats
{
    ULONG    bkio_bufwrites;       /* num of buffered writes on this file */
    ULONG    bkio_unbufwrites;     /* num of unbuffered writes on this file */
    ULONG    bkio_bufreads;        /* num of buffered reads on this file */
    ULONG    bkio_unbufreads;      /* num of unbuffered reads on this file */
} bkiostats_t;

/* values for opening a file */
#define BUFIO		1	/* open a file buffered */
#define BUFONLY		2	/* file has a buffered fd only */
#define BOTHIO		3	/* file has both a buf and unbuf fd */
#define UNBUFIO		4	/* open a file unbuffered */
#define UNBUFONLY	5	/* file has an unbuf fd only */

typedef struct bkiotbl bkiotbl_t;
typedef struct stpool bkio_stpool_t;

#ifndef FILE_HANDLE_DEFINED
#define FILE_HANDLE_DEFINED

/* typedef the data types for file descriptors and file handles       */
#if OPSYS == UNIX
typedef int fdHandle_t;
#ifdef PRO_REENTRANT
struct fileHandle
{
    fdHandle_t      fd;
    pthread_mutex_t ioLock;
};
typedef struct fileHandle *fileHandle_t;
#else
typedef int     fileHandle_t;
#endif

#elif OPSYS == WIN32API
typedef HANDLE  fileHandle_t;
typedef HANDLE  fdHandle_t;
#endif

#endif /* FILE_HANDLE_DEFINED */

/******************************************/
/* The following functions are for bkiotop.c */
/******************************************/

DSMVOID	bkioFlush	(struct dsmContext *pcontext, bkiotbl_t *pbkiotbl);
int	bkioClose	(struct dsmContext *pcontext, bkiotbl_t *pbkiotbl,
                         UCOUNT mode);
DSMVOID    bkioCopyBufferedToUnbuffered
                        (struct dsmContext *pcontext, bkiotbl_t *pbkiotbl);
DSMVOID	bkioFreeIotbl	(struct dsmContext *pcontext, bkiotbl_t *pbkiotbl);
fileHandle_t bkioGetFd	(struct dsmContext *pcontext, bkiotbl_t *pbkiotbl,
                         LONG mode);
bkiotbl_t *bkioGetIoTable
                        (struct dsmContext *pcontext, bkio_stpool_t *pool);
int	bkioGetUnbufFd	(struct dsmContext *pcontext, bkiotbl_t *pbkiotbl);
int	bkioIsLocalFile	(struct dsmContext *pcontext, TEXT *pfullpath);
LONG    bkioOpen        (struct dsmContext *pcontext, bkiotbl_t *pbkiotbl, 
                         bkiostats_t *bkiostats, gem_off_t fileIOflags,
                         TEXT *fileName,int openFlags,
                         int createPermission);
DSMVOID    bkioSaveFd      (struct dsmContext *pcontext, bkiotbl_t *pbkiotbl,
                         TEXT *pname, fileHandle_t fd, int mode);
DSMVOID	bkioSync	(struct dsmContext *pcontext);
LONG	bkioRead	(struct dsmContext *pcontext, bkiotbl_t *pbkiotbl,
                         bkiostats_t *pbkiostats,
                         gem_off_t offset, TEXT *pbuf, int size,
                         int blklog, int buffmode);
DSMVOID    bkioUpdateCounters
                        (struct dsmContext *pcontext, bkiostats_t *ptarget,
                         bkiostats_t *psource);
LONG	bkioWrite	(struct dsmContext *pcontext, bkiotbl_t *pbkiotbl,
                         bkiostats_t *pbkiostats,
                         gem_off_t offset, TEXT *pbuf, int size,
                         int blklog, int buffmode);


/******************************************************/
/* Public Function Prototypes for bkiolk.c            */
/******************************************************/

int	 bkioCreateLkFile	(struct dsmContext *pcontext,
                                 int mode, TEXT *dbname, int dispmsg);
int	 bkioVerifyLkFile	(struct dsmContext *pcontext,
                                 int pid, TEXT *dbname);
DSMVOID	 bkioRemoveLkFile	(struct dsmContext *pcontext,
                                 TEXT *dbname);
int	 bkioTestLkFile		(struct dsmContext *pcontext,
                                 TEXT *dbname);
int      bkioUpdateLkFile	(struct dsmContext *pcontext,
                                 int mode, int pid, TEXT *phost, TEXT *dbname);


/******************************************************/
/* Public Defines for bkiolk.c                        */
/******************************************************/
#define BKIO_LKSNGL        1
#define BKIO_LKMULT        2
#define BKIO_LKNOHOST      3
#define BKIO_LKTRUNC       4
#define BKIO_LKSTARTUP     5

/******************************************************/
/* Public Defines for bkio.c                          */
/******************************************************/

/* This determines the number of slots available in bkioFunctions[] for */
/* file I/O functions */

#define BKIO_LAST_FUNCTION      5

/* These are the modes available for file I/O in the bkio layer */
#define BKIO_WRITE_BUFFERED     0
#define BKIO_READ_BUFFERED      1
#define BKIO_WRITE_UNBUFFERED   2
#define BKIO_READ_UNBUFFERED    3
#define BKIO_OPEN               4
#define BKIO_CLOSE              5
#define BKIO_SYNC               6

/* Return codes for bkioOpen() and bkioOpen<OPSYS>         */
#define BKIO_OPEN_OK            0       /* success */
#define BKIO_EINVAL            -1       /* invalid file name characters */
#define BKIO_ENOENT            -2       /* no such file */
#define BKIO_EMFILE            -3       /* too many open files */
#define BKIO_EACCES            -4       /* permission denied */
#define BKIO_EEXISTS           -5       /* file exists */
#define BKIO_ENXIO             -6       /* bad i/o table pointer */


/******************************************************/
/* Public Function Prototypes for bkio.c              */
/******************************************************/
int     bkioFileInBlocks    (TEXT *pname, ULONG *bkiofilesize, LONG blklog);      
#endif /* ifndef BKIOPUB_H  */
