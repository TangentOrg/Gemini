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

#ifndef BMPUB_H
#define BMPUB_H

/**********************************************************/
/* bmpub.h - Public function prototypes for BM subsystem. */
/**********************************************************/

struct bktbl;
struct dbcontext;
struct dsmContext;
struct stpool;
struct usrctl;

/*********************************************************
 * Include public structure definitions here
 */

#define BMBUM_H

#include "bmctl.h"

#undef BMBUM_H

typedef struct bkbuf void_bm_bkbuf_t;
typedef QBKTBL bmBufHandle_t; 

/*********************************************************/
/* Public function prototypes for bmapw.c                */
/*********************************************************/
DSMVOID	bkApw		(struct dsmContext *pcontext, ULONG curTime, 
                         ULONG *plastScanTime, LONG *pcurHashNum,
                         LONG *pextraScans, LONG *pnumSkipped,
                         LONG64 *pbkApwLastBi);
DSMVOID	bkapwbegin	(struct dsmContext *pcontext);
DSMVOID	bkapwend	(struct dsmContext *pcontext);

/*********************************************************/
/* Public function prototypes for bmbuf.c                */
/*********************************************************/
DSMVOID 	bmbufo		(struct dsmContext *pcontext);
int             bmBackupClear   (struct dsmContext *pcontext);
DSMVOID 	bmCancelWait 	(struct dsmContext *pcontext);
DSMVOID 	bmFlushMasterblock 	(struct dsmContext *pcontext);

#define FLUSH_ALL_AREAS (ULONG)0
DSMVOID 	bmFlushx	(struct dsmContext *pcontext, ULONG area, int synchronous);

int  	bmGetPrivROBuffers
			(struct dsmContext *pcontext,
			 UCOUNT numBuffersRequested);
DSMVOID 	bmfpbuf		(struct dsmContext *pcontext, struct usrctl *pusr);
DSMVOID 	bmGetBufferPool (struct dsmContext *pcontext, struct bktbl *pbktbl);
DSMVOID 	bmGetBuffers	(struct dsmContext *pcontext, struct bktbl *pbktbl,
			 struct stpool *ppool, int nbufs);
DSMVOID 	bmLockDownMasterBlock
			(struct dsmContext *pcontext);
DSMVOID 	bmAlign 	(struct dsmContext *pcontext, TEXT **ppbuf);
DSMVOID 	bmMarkModified	(struct dsmContext *pcontext, bmBufHandle_t bufHandle,
			 LONG64 bireq);
DSMVOID 	usrbufo		(struct dsmContext *pcontext);
int     bmBackupOnline  (struct dsmContext *pcontext);
int 	bmPopLocks 	(struct dsmContext *pcontext, struct usrctl *pusr);
bmBufHandle_t
	bmFindBuffer	(struct dsmContext *pcontext, ULONG area,
			 DBKEY  blkdbkey);

bmBufHandle_t 
        bmLocateBlock	(struct dsmContext *pcontext, ULONG area, DBKEY dkey,
			 int action);
bmBufHandle_t 
        bmLockBuffer	(struct dsmContext *pcontext, ULONG area, DBKEY dkey,
			 int action, LONG wait);
struct bkbuf *
        bmGetBlockPointer (struct dsmContext *pcontext,
                           bmBufHandle_t bufHandle);

ULONG bmGetArea		(struct dsmContext *pcontext, bmBufHandle_t bufHandle);

DSMVOID bmUpgrade		(struct dsmContext *pcontext, bmBufHandle_t bufHandle );

DSMVOID bmDownGrade	(struct dsmContext *pcontext, bmBufHandle_t bufHandle );

DSMVOID bmReleaseBuffer	(struct dsmContext *pcontext, bmBufHandle_t bufHandle);

DSMVOID bmAreaFreeBuffers(struct dsmContext *pcontext, ULONG area);

#endif /* ifndef BMPUB_H  */

