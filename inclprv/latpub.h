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

#ifndef LATPUB_H
#define LATPUB_H

#define MTSANITYCHECK   1	/* for extra general sanity checks   */
                                /* There is code in latctl.c that is */
                                /* activated by this flag and macros */
                                /* in latprv.h.                      */

struct dbcontext;
struct latch;
struct usrctl;
struct dsmContext;

/* spinlock_t per machine */
#if (SEQUENT || SEQUENTPTX)
    typedef     slock_t     spinlock_t;
#elif (DECSTN)
    typedef     int         spinlock_t;
#elif (OPSYS==WIN32API || OPSYS==OS2)
    typedef int             spinlock_t;
#else
    /* All other machine types */
    typedef LONG            spinlock_t;
#endif


DSMVOID latnap		(struct dsmContext *pcontext, int napTime);
DSMVOID latlatch		(struct dsmContext *pcontext, int latchId);
DSMVOID latunlatch		(struct dsmContext *pcontext, int latchId);
DSMVOID latfrlatch		(struct dsmContext *pcontext, int latchId,
                         COUNT *platchDepth);
DSMVOID latrelatch		(struct dsmContext *pcontext, int latchId,
                         COUNT latchDepth);
DSMVOID latlkmux		(struct dsmContext *pcontext, int muxId, int muxNo);
DSMVOID latfrmux		(struct dsmContext *pcontext, int muxId, int muxNo);
DSMVOID latswaplk		(struct dsmContext *pcontext);
DSMVOID latcanwait		(struct dsmContext *pcontext);
DSMVOID latUsrWait		(struct dsmContext *pcontext);
DSMVOID latLockTimeout	(struct dsmContext *pcontext, struct usrctl *pusr,
			 LONG currTime);

int  latLockWait	(struct dsmContext *pcontext, int waitCode,
			 int msgNum, TEXT *pfile, 
			 TEXT *puname, TEXT *ptty);
DSMVOID latUsrWake		(struct dsmContext *pcontext, struct usrctl *pu,
			 int whichQueue);
int  latpoplocks	(struct dsmContext *pcontext, struct usrctl *pu);
int  latSemClear	(struct dsmContext *pcontext);
DSMVOID latusrwake(struct usrctl *pu, int whichQueue);
DSMVOID latseto		(struct dsmContext *pcontext);
int  latGetStatMode	(struct dsmContext *pcontext);
DSMVOID latSetStats	(struct dsmContext *pcontext, int statMode);
DSMVOID latalloc		(struct dsmContext *pcontext);
ULONG latSetCount	(struct dsmContext *pcontext, int cnttype,
			 int code, int action); 
COUNT latGetUsrCount	(struct dsmContext *pcontext, int action);
LONG latGetSizeofStruct (DSMVOID);
DSMVOID latPrivLatch       (struct dsmContext *pcontext, struct latch *ptheLatch);
DSMVOID latFreePrivLatch   (struct dsmContext *pcontext, struct latch *ptheLatch);
int  latPrivFastLock    (struct dsmContext *pcontext, spinlock_t *pfastLock,
                         int wait);
DSMVOID latFreePrivFastLock(struct dsmContext *pcontext, spinlock_t *pfastLock);

#if OPSYS==WIN32API
DSMVOID latCreateMutexes	(struct dsmContext *pcontext);
DSMVOID latOpenMutexes	(struct dsmContext *pcontext);
#endif /* WIN32API */

/********************************************************************/
/* Public Structures And Typedef(s)                                 */
/********************************************************************/

/* action codes for latGetUsrCount() and latSetCount() */
#define LATGET   0     /* Retrieve current value */
#define LATINCR  1     /* Increment current value */
#define LATDECR  2     /* Decrement current value */

/* latch types */

#define MT_LT_SPIN	0	/* spinlock */
#define MT_LT_SSPIN	1	/* spinlock and stats */
#define MT_LT_QUEUE	2	/* latch with queue and semaphore */
#define MT_LT_SQUEUE	3	/* latch with queue and semaphore and stats */

/* future */

#define MT_LT_SEM	4	/* latch with semaphore only */
#define MT_LT_SSEM	5	/* latch with semaphore only and stats */
#define MT_SOFT_S	1	/* soft lock share lock bit */
#define MT_SOFT_IX	2	/* soft lock intend exclusive lock bit */
#define MT_SOFT_X	4	/* soft lock exclusive lock bit */

/* Id numbers for latches

   Except for USR, SCC and MTX, latches may only be held for very short time
   periods. Usually, they should be released within the same function in
   which they are locked. In no case should I/O operations be performed
   while holding any latch other than MTX. */

#define MTL_0     0      /* skipped - do not use */
#define MTL_MTX   1      /* BI allocation and micro-transactions */
#define MTL_USR   2      /* usrctl and miscellaneous other structures */
#define MTL_OM    3      /* latches object cache                      */
#define MTL_BIB   4      /* rlctl, bi buffers, etc */
#define MTL_SCC   5      /* schema cache */
#define MTL_LKP   6      /* lock table purge chains */
#define MTL_GST   7      /* shared memory global storage pool */
#define MTL_TXT   8      /* transaction table */
#define MTL_LHT   9      /* lock hash table and chains */
#define MTL_SEQ  10      /* sequence generator */
#define MTL_AIB  11      /* after image buffers */
#define MTL_TXQ  12      /* transaction end/begin lock queue */
#define MTL_BIW  13      /* modified/free bi buffer list */

#define MTL_LKF  14      /* lock table free chain */
#define MTL_BFP  15      /* bkctl */
#define MTL_BHT  16      /* buffer pool hash table */
#define MTL_PWQ  17      /* page writer write queue */
#define MTL_AIW  18      /* next ai buffer to write, last ai buffer written */
#define MTL_CPQ  19      /* buffer pool checkpoint queue */

/* These must be contiguous - see bkbuf.c */
#define MTL_LRU  20      /* buffer pool general lru chain */
#define MTL_LR2  21      /* buffer pool secondary lru chain */
#define MTL_LR3  22      /* buffer pool index lru chain */
#define MTL_LR4  23      /* buffer pool unused lru chain */
#define MTL_LRLAST 23    /* last one */

/* These must all be contiguous - see nsamon and bkbufo */
#define MTL_BF1  24      /* buffer pool bktbl's and buffers 1 */
#define MTL_BF2  25      /* buffer pool bktbl's and buffers 2 */
#define MTL_BF3  26      /* buffer pool bktbl's and buffers 3 */
#define MTL_BF4  27      /* buffer pool bktbl's and buffers 4 */
#define MTL_BF5  28      /* buffer pool bktbl's and buffers 5 */
#define MTL_BF6  29      /* buffer pool bktbl's and buffers 6 */
#define MTL_BF7  30      /* buffer pool bktbl's and buffers 7 */
#define MTL_BF8  31      /* buffer pool bktbl's and buffers 8 */
#define MTL_BFLAST 31    /* last buffer pool latch */

#define MTL_MAX  31      /* last one */


#define MTM_LKH  0	/* lock table hash chain mux */
#define MTM_BPB  1	/* buffer pool bktbl mux */
#define MTM_MAX  1	/* last one */
/* *********************************************************************** */

#if SHMEMORY
#define MTM_MAXLKH	128 /* has to be power of 2 - see masks below */
#define MTM_MAXBPB	128 /* has to be power of 2 - see masks below */
#else
#define MTM_MAXBPB	0
#define MTM_MAXLKH	0
#endif

#define MTM_LKH_MASK  0x7f /* see MTM_MAXLKH above */
#define MTM_BPB_MASK  0x7f /* see MTM_MAXBPB above */

#define MTM_TOTAL (MTM_MAXBPB+MTM_MAXLKH)

/* some macros for the latches - see also lkctl.h and bkctl.h */

#if SHMEMORY

/* lock a latch */

#define	MT_LATCH(x)      latlatch (pcontext, (x))
#define	MT_LOCK_MTX()	 latlatch (pcontext, MTL_MTX)
#define	MT_LOCK_USR()	 latlatch (pcontext, MTL_USR)
#define	MT_LOCK_OM()	 latlatch (pcontext, MTL_OM)
#define	MT_LOCK_BIB()	 latlatch (pcontext, MTL_BIB)
#define	MT_LOCK_SCC()	 latlatch (pcontext, MTL_SCC)
#define	MT_LOCK_GST()	 latlatch (pcontext, MTL_GST)
#define	MT_LOCK_TXT()	 latlatch (pcontext, MTL_TXT)
#define	MT_LOCK_AIB()	 latlatch (pcontext, MTL_AIB)
#define	MT_LOCK_SEQ()	 latlatch (pcontext, MTL_SEQ)
#define	MT_LOCK_TXQ()	 latlatch (pcontext, MTL_TXQ)
#define	MT_LOCK_BIW()	 latlatch (pcontext, MTL_BIW)
#define	MT_LOCK_AIW()	 latlatch (pcontext, MTL_AIW)

#ifdef PRO_REENTRANT
#if HP825
#define	PL_LOCK_USRCTL(puserLatch)
#define	PL_TRYLOCK_USRCTL(puserLatch) 0
#else
#define	PL_LOCK_USRCTL(puserLatch)    latPrivFastLock(pcontext, puserLatch, 1);
#define	PL_TRYLOCK_USRCTL(puserLatch) latPrivFastLock(pcontext, puserLatch, 0)
#define	PL_LOCK_MSG(ptheLatch)        latPrivLatch(pcontext, ptheLatch);
#endif
#define	PL_LOCK_CURSORS(ptheLatch)    latPrivLatch(pcontext, ptheLatch);
#define	PL_LOCK_STPOOL(ptheLatch)     latPrivLatch(pcontext, ptheLatch);
#define PL_LOCK_DBCONTEXT(ptheLatch)  latPrivLatch(pcontext, ptheLatch);
#define PL_LOCK_EXTBUFFER(ptheLatch)  latPrivLatch(pcontext, ptheLatch);
#define PL_LOCK_TREELIST(ptheLatch)  latPrivLatch(pcontext, ptheLatch);
#define PL_LOCK_RMUNDO(ptheLatch)     latPrivLatch(pcontext, ptheLatch);
#define PL_LOCK_DSMCONTEXT(ptheLatch) latPrivLatch(pcontext, ptheLatch);
#else
#if HP825
#define	PL_LOCK_USRCTL(puserLatch)
#define	PL_TRYLOCK_USRCTL(puserLatch) 0
#else
#define	PL_LOCK_USRCTL(puserLatch)    latPrivFastLock(pcontext, puserLatch, 1);
#define	PL_TRYLOCK_USRCTL(puserLatch) latPrivFastLock(pcontext, puserLatch, 0)
#endif
#define PL_LOCK_MSG(ptheLatch)
#define	PL_LOCK_CURSORS(ptheLatch)
#define PL_LOCK_STPOOL(ptheLatch)
#define PL_LOCK_DBCONTEXT(ptheLatch)
#define PL_LOCK_EXTBUFFER(ptheLatch)
#define PL_LOCK_TREELIST(ptheLatch)
#define PL_LOCK_RMUNDO(ptheLatch)
#define PL_LOCK_DSMCONTEXT(ptheLatch)
#endif

/* unlock latch */

#define	MT_UNLATCH(x)    latunlatch (pcontext, (x))
#define	MT_UNLK_MTX()	 latunlatch (pcontext, MTL_MTX)
#define	MT_UNLK_USR()	 latunlatch (pcontext, MTL_USR)
#define	MT_UNLK_OM()	 latunlatch (pcontext, MTL_OM)
#define	MT_UNLK_BIB()	 latunlatch (pcontext, MTL_BIB)
#define	MT_UNLK_SCC()	 latunlatch (pcontext, MTL_SCC)
#define	MT_UNLK_GST()	 latunlatch (pcontext, MTL_GST)
#define	MT_UNLK_TXT()	 latunlatch (pcontext, MTL_TXT)
#define	MT_UNLK_AIB()	 latunlatch (pcontext, MTL_AIB)
#define	MT_UNLK_SEQ()	 latunlatch (pcontext, MTL_SEQ)
#define	MT_UNLK_TXQ()	 latunlatch (pcontext, MTL_TXQ)
#define	MT_UNLK_BIW()	 latunlatch (pcontext, MTL_BIW)
#define	MT_UNLK_AIW()	 latunlatch (pcontext, MTL_AIW)

#ifdef PRO_REENTRANT
#if HP825
#define PL_UNLK_USRCTL(puserLatch)
#else
#define PL_UNLK_USRCTL(puserLatch) latFreePrivFastLock(pcontext, puserLatch);
#endif
#define PL_UNLK_MSG(ptheLatch)     latFreePrivLatch(pcontext, ptheLatch);
#define PL_UNLK_CURSORS(ptheLatch) latFreePrivLatch(pcontext, ptheLatch);
#define PL_UNLK_STPOOL(ptheLatch)  latFreePrivLatch(pcontext, ptheLatch);
#define PL_UNLK_DBCONTEXT(ptheLatch) latFreePrivLatch(pcontext, ptheLatch);
#define PL_UNLK_EXTBUFFER(ptheLatch) latFreePrivLatch(pcontext, ptheLatch);
#define PL_UNLK_TREELIST(ptheLatch) latFreePrivLatch(pcontext, ptheLatch);
#define PL_UNLK_RMUNDO(ptheLatch) latFreePrivLatch(pcontext, ptheLatch);
#define PL_UNLK_DSMCONTEXT(ptheLatch) latFreePrivLatch(pcontext, ptheLatch);
#else
#if HP825
#define PL_UNLK_USRCTL(puserLatch)
#else
#define PL_UNLK_USRCTL(puserLatch) latFreePrivFastLock(pcontext, puserLatch);
#endif
#define PL_UNLK_MSG(ptheLatch)
#define PL_UNLK_CURSORS(ptheLatch)
#define PL_UNLK_STPOOL(ptheLatch)
#define PL_UNLK_DBCONTEXT(ptheLatch)
#define PL_UNLK_EXTBUFFER(ptheLatch)
#define PL_UNLK_TREELIST(ptheLatch)
#define PL_UNLK_RMUNDO(ptheLatch)
#define PL_UNLK_DSMCONTEXT(ptheLatch)
#endif

/* unconditionally free a latch. but save the depth value so it can be
   reinstated by MT_RELOCK_xxx */

#define	MT_FREE_BIB(platchDepth)   latfrlatch (pcontext, MTL_BIB, platchDepth)
#define	MT_FREE_AIB(platchDepth)   latfrlatch (pcontext, MTL_AIB, platchDepth)

/* re-lock a latch that was previously freed */

#define	MT_RELOCK_BIB(latchDepth)   latrelatch (pcontext, MTL_BIB, latchDepth)
#define	MT_RELOCK_AIB(latchDepth)   latrelatch (pcontext, MTL_AIB, latchDepth)

/* test if holding a latch */

#define MTHOLDING(l) (pcontext->pusrctl && \
                      (pcontext->pusrctl->uc_latches & (1 << (l))))

/* release all latches held by a user   */

#define MT_POP_LOCKS(pcontext, pusr) latpoplocks(pcontext, pusr)

#else /* no shared memory, no latches either */

#define	MT_LATCH(x)
#define	MT_LOCK_MTX()
#define	MT_LOCK_USR()
#define MT_LOCK_OM()
#define	MT_LOCK_BIB()
#define	MT_LOCK_SCC()
#define	MT_LOCK_GST()
#define	MT_LOCK_TXT()
#define	MT_LOCK_AIB()
#define	MT_LOCK_SEQ()
#define	MT_LOCK_TXQ()
#define	MT_LOCK_BIW()
#define	MT_LOCK_AIW()
#define	MT_LOCK_USRCTL()
#define	MT_LOCK_MSG()

#define	MT_UNLATCH(x)
#define	MT_UNLK_MTX()
#define	MT_UNLK_USR()
#define MT_UNLK_OM()
#define	MT_UNLK_BIB()
#define	MT_UNLK_SCC()
#define	MT_UNLK_GST()
#define	MT_UNLK_TXT()
#define	MT_UNLK_AIB()
#define	MT_UNLK_SEQ()
#define	MT_UNLK_TXQ()
#define	MT_UNLK_BIW()
#define	MT_UNLK_AIW()
#define	MT_UNLK_USRCTL()
#define	MT_UNLK_MSG()

#define	MT_FREE_BIB(pl)
#define	MT_FREE_AIB(pl)

#define	MT_RELOCK_AIB(l)
#define	MT_RELOCK_BIB(l)

#define MTHOLDING(l)	(0)

#define MT_POP_LOCKS(x) (0)

#endif

#endif /* LATPUB_H */
