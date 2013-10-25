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

#ifndef STMPUB_H
#define STMPUB_H
 
/* Allocate a Global Storage Pool for DSM Use ONLY */
#ifdef PDSMSTGLOB
LOCAL  stpool_t  dsmpool;
GLOBAL stpool_t *pdsmStoragePool = &dsmpool;  /* general storage pool pointer */
#else
IMPORT struct stpool *pdsmStoragePool;  /* general storage pool pointer */
#endif

struct abk;  /* ABK */
struct ditem;   /* DITEM */
struct poolcounts;  /* POOLCOUNTS */
struct poolptrs;  /* POOLPTRS */
struct rentprfx;  /* RENTPRFX */
struct stpool;  /* STPOOL */
struct uic;  /* UIC */
struct stdiaginfo;
struct dsmContext;
 
/****** stm.c ******/
 
TEXT * stGet	(struct dsmContext *pcontext, struct stpool *pstpool, 
                 unsigned len);

TEXT * stGetIt	(struct dsmContext *pcontext, struct stpool *pstpool, 
                 unsigned len, GBOOL st_fatal);

TEXT * stGetd	(struct dsmContext *pcontext, struct stpool *pstpool, 
                 unsigned len); 

TEXT * stGetDirty(struct dsmContext *pcontext, struct stpool *pstpool,
                  unsigned len, GBOOL st_fatal);

DSMVOID stFree	(struct dsmContext *pcontext, struct stpool *pstpool); 

DSMVOID stClear	(struct dsmContext *pcontext, struct stpool *pstpool); 

#ifdef ST_MEMCHK
#ifdef ST_PURIFY
DSMVOID stFreeAllPUR	(struct stpool *pstpool); 
#endif /* ST_PURIFY */
#endif /* ST_MEMCHK */

/* stmInitPool may have been re-defined for diagnostics use */
#ifndef stmInitPool
DSMVOID stmInitPool	(struct dsmContext *pcontext, struct stpool *pstpool,
                         TTINY bits, BYTES abksize);
#endif
DSMVOID *stOpenDiagnostics(int (*pfunc)(TEXT *diagmsg));
DSMVOID stInitPoolDiagnostic (struct stpool *pstpool, TTINY bits, BYTES abksize,
                           TEXT *pfilname, LONG linenum);
DSMVOID stCloseDiagnostics(struct stdiaginfo *pstinfo);
DSMVOID stWriteDiagnostics(struct stpool *pstpool, int calledbystclear,
                        struct stdiaginfo *pstinfo);


TEXT * stRent	(struct dsmContext *pcontext, struct stpool *pstpool, 
                 unsigned len); 

TEXT * stRentIt (struct dsmContext *pcontext, struct stpool *pstpool,
                 unsigned len, GBOOL st_fatal);

TEXT * stRentd	(struct dsmContext *pcontext, struct stpool *pstpool, 
                 unsigned len); 

TEXT * stRentDirty(struct dsmContext *pcontext, struct stpool *pstpool,
                   unsigned len, GBOOL st_fatal);

#ifdef ST_MEMCHK
#ifdef ST_PURIFY
TEXT * stMallocPUR	(struct stpool *pstpool, unsigned len, GBOOL st_fatal);

#endif /* ST_PURIFY */
#endif /* ST_MEMCHK */

DSMVOID stVacate		(struct dsmContext *pcontext, struct stpool *pstpool, 
                         TEXT *parea); 

#ifdef ST_MEMCHK
#ifdef ST_PURIFY
DSMVOID stFreePUR		(struct stpool *pstpool, TEXT *parea); 
#endif /* ST_PURIFY */
#endif /* ST_MEMCHK */

struct stpool * stGetPoolPtr	(TEXT *parea);

int stScanPool	(struct stpool *pstpool, TEXT **ppabk, TEXT **pparea);


DSMVOID stIntabk	(struct dsmContext *pcontext, struct abk *pabk, 
                 struct stpool *pstpool, unsigned size);

DSMVOID stLnk	(struct dsmContext *pcontext, struct stpool *pstpool, 
                 struct abk *pabk);

DSMVOID stLnkx	(struct dsmContext *pcontext, struct stpool *pstpool, 
                 struct abk *pabk);

DSMVOID stFatal	(struct dsmContext *pcontext);

int stEnableChecking	(int flag);

#endif /* STMPUB_H */
