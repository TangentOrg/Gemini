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

#ifndef DRPRV_H
#define DRPRV_H

/*********************************************************/
/* drprv.h - Private interface for DR utility subsystem.  */
/*********************************************************/

struct dsmContext;
struct dbctl;
struct filstat;
struct fldstat;
struct kcstat;
struct keystat;
struct scchg;


/**************      BUM CONSTANT DEFINITIONS      *****************/
/*        (from syscall.h - for compilation purposes) ***/
/* The following definitions were extracted directly from syscall.h */
/* This needs cleanup, desperately. */

#if OPSYS==UNIX || OPSYS==MPEIX
#define OPENMODE  2     /* open for read/write random access */
#define OPENREAD  0     /* open for read only, random access */
#define OPENASCR  0     /* open for read only,ascii */
#define OPENWRITE 1     /* open for write only,random access */
#define CREATMODE 0666
#endif  /* UNIX or MPEIX */

#if OPSYS==VMS
#define OPENMODE  2     /* open for read/write random access */
#define OPENREAD  0     /* open for read only, random access */
#define OPENASCR  0     /* open for read only,ascii */
#define OPENWRITE 1     /* open for write only,random access */
#define CREATMODE 0     /* Default file protection */
#endif  /* VMS */

#if OPSYS==MS_DOS || OPSYS==WIN32API
#if MSC || WC386
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>
#define OPENMODE  O_RDWR | O_BINARY
#define OPENREAD  O_RDONLY | O_BINARY /* open for read only, random access */
#define OPENASCR  O_RDONLY         /* open for read only, random access */
#define OPENWRITE O_WRONLY | O_BINARY /* open for write only,random access */
#define CREATMODE S_IREAD | S_IWRITE
#endif  /*  MSC || WC386 */
#endif  /* MS_DOS || WIN32API */

#if OPSYS==OS2
#include <io.h>
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <process.h>
#define OPENMODE  O_RDWR | O_BINARY
#define OPENREAD  O_RDONLY | O_BINARY   /* open for read only, random access */
#define OPENASCR  O_RDONLY              /* open for read only, random access */
#define OPENWRITE O_WRONLY | O_BINARY   /* open for write only,random access */
#define CREATMODE S_IREAD | S_IWRITE
#endif /* OS2 */

#if OPSYS==BTOS
#if CTMSC
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>
#endif  /* CTMSC */
#define OPENMODE  2     /* open for read/write random access */
#define OPENREAD  0     /* open for read only, random access */
#define OPENASCR  0     /* open for read only,ascii */
#define CREATMODE 0 /* Default file protection - ignored */
#endif  /* OPSYS==BTOS */

#define LSEEKREL  1
#define FOPNREAD  "rb"

/**********  END OF BUM COMMENT  *****************************/

/*******************************************************************/
/* drprv.h - Private function prototypes for DR utility subsystem. */
/*******************************************************************/

/*************************************************************/
/* Private Function Prototypes for drconv67.c                */
/*************************************************************/
int	drconv67	(int dbtype);

/*************************************************************/
/* Private Function Prototypes for drbackup.c                */
/*************************************************************/
#if OPSYS == VMS
int	drbkopts 	(DSMVOID);
#else  /* OPSYS != VMS */
int	drbkopts 	(TEXT *parg);
#endif

int	drbackup 	(struct dsmContext *pcontext);

/*************************************************************/
/* Private Function Prototypes for drconvft.c                */
/*************************************************************/
DSMVOID	drconvft	(DSMVOID);
DSMVOID	check6		(DSMVOID);

/*************************************************************/
/* Private Function Prototypes for drgw67.c                  */
/*************************************************************/
int	scConvertHolder	(DSMVOID);

/*************************************************************/
/* Private Function Prototypes for drprutil.c                */
/*************************************************************/
DSMVOID	drprutil	(struct dsmContext *pcontext);

int	opendb		(TEXT *prunmode, int openmode, int recover,
			 int online);

/*************************************************************/
/* Private Function Prototypes for drrfutil.c                */
/*************************************************************/
DSMVOID	druarg		(TEXT *parg);
int	drrfutil	(struct dsmContext *pcontext,
                         struct dbctl *pclientdbctl);

/*************************************************************/
/* Private Function Prototypes for drschg67.c                */
/*************************************************************/
int	scchg67		(DSMVOID);

/*************************************************************/
/* Private Function Prototypes for drserv.c                  */
/*************************************************************/
DSMVOID	    doserve (DSMVOID);
DSMVOID        drSrvStartUpOptions (DSMVOID);

LONG	TritonServe (struct dsmContext **pContext);

/*************************************************************/
/* Private Function Prototypes for drut67.c                  */
/*************************************************************/
int	scfixmb		(DSMVOID);
int	scfinish	(DSMVOID);
int	scflfld		(int mode, struct filstat *fil, int numfils,
			 struct fldstat *fld, int numflds,
			 struct keystat *keystat, int numkeys,
			 struct scchg *scchg, int numchgs,
			 struct kcstat *kcstat, int numckeys,
			 int numnewfil, int numnewidx);

#endif  /* #ifndef DRPRV_H */
