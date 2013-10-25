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

#ifndef BMPRV_H
#define BMPRV_H


/******************************************************/
/* private structure definitions for the bm subsystem */
/******************************************************/

#include "latpub.h"  /* temp for mt renames */

/* shared memory locking macros */

#if SHMEMORY

#define BK_LOCK_POOL() latlatch (pcontext, MTL_BFP)
#define BK_UNLK_POOL() latunlatch (pcontext, MTL_BFP)

#define BK_LOCK_APWQ() latlatch (pcontext, MTL_PWQ)
#define BK_UNLK_APWQ() latunlatch (pcontext, MTL_PWQ)

/* Only attemtp to lock LRU if the bk_lrulock lock has a value.
 */
#define BK_LOCK_LRU(pl) if ((pl)->bk_lrulock) \
                            latlatch (pcontext, (pl)->bk_lrulock)
#define BK_UNLK_LRU(pl) if ((pl)->bk_lrulock) \
                            latunlatch (pcontext, (pl)->bk_lrulock)

#define BK_LOCK_LR2() latlatch (pcontext, MTL_LR2)
#define BK_UNLK_LR2() latunlatch (pcontext, MTL_LR2)

#define BK_LOCK_HASH() latlatch (pcontext, MTL_BHT)
#define BK_UNLK_HASH() latunlatch (pcontext, MTL_BHT)

#define BK_LOCK_BKTBL(p) \
 if (pcontext->pdbcontext->pdbpub->argspin &&             \
     pcontext->pdbcontext->pdbpub->argmux)                \
    latlkmux (pcontext, MTM_BPB, (int)((p)->bt_muxlatch));\
  else latlatch (pcontext, (p)->bt_muxlatch)

#define BK_UNLK_BKTBL(p) \
  if (pcontext->pdbcontext->pdbpub->argspin &&           \
      pcontext->pdbcontext->pdbpub->argmux)              \
    latfrmux(pcontext, MTM_BPB, (int)((p)->bt_muxlatch));\
  else latunlatch (pcontext, (p)->bt_muxlatch)

#define BK_GET_BKTBL_LATCH(p) (int)((p)->bt_muxlatch)

#define BK_LOCK_BKTBL_BY_LATCH(n)\
  if (pcontext->pdbcontext->pdbpub->argspin && \
      pcontext->pdbcontext->pdbpub->argmux)    \
       latlkmux (pcontext, MTM_BPB, n);        \
  else latlatch (pcontext, n)

#define BK_UNLK_BKTBL_BY_LATCH(n)\
  if (pcontext->pdbcontext->pdbpub->argspin && \
      pcontext->pdbcontext->pdbpub->argmux)        \
       latfrmux (pcontext, MTM_BPB, n);        \
  else latunlatch (pcontext, n)

/* Lock and unlock a buffer chain */

#define BK_LOCK_CHAIN(pA) latlatch(pcontext, (pA)->bk_lockNumber)

#define BK_UNLK_CHAIN(pA) latunlatch(pcontext, (pA)->bk_lockNumber)

#else /* no shared memory */

#define BK_LOCK_POOL()
#define BK_UNLK_POOL()

#define BK_LOCK_APWQ()
#define BK_UNLK_APWQ()

#define BK_LOCK_LRU(p)
#define BK_UNLK_LRU(p)

#define BK_LOCK_LR2()
#define BK_UNLK_LR2()

#define BK_LOCK_HASH()
#define BK_UNLK_HASH()

#define BK_LOCK_BKTBL(p)
#define BK_UNLK_BKTBL(p)

#define BK_GET_BKTBL_LATCH(p)

#define BK_LOCK_BKTBL_BY_LATCH(l)
#define BK_UNLK_BKTBL_BY_LATCH(l)

#define BK_LOCK_CHAIN(pA)
#define BK_UNLK_CHAIN(pA)

#endif /* #if SHMEMORY */

struct bkanchor;
struct bkchain;
struct bkctl;
struct bktbl;
struct dsmContext;
struct usrctl;

/**********************************/
/* private prototypes for bmbuf.c */
/**********************************/
/* wrapper for bmPopBufferStack */
DSMVOID bmdecrement 	(struct dsmContext *pcontext,
			 struct bktbl *pbktbl);
DSMVOID bmPopBufferStack 	(struct dsmContext *pcontext, struct bktbl *pbktbl);
DSMVOID bmRemoveQ 		(struct dsmContext *pcontext, struct bkanchor *pAnchor,
			 struct bktbl *pbktbl);
DSMVOID bmEnqLru 		(struct dsmContext *pcontext, struct bkchain *pLruChain,
			 struct bktbl  *pbktbl);
DSMVOID bmFlush 		(struct dsmContext *pcontext, struct bktbl *pbktbl,
			 int mode);
DSMVOID bmEnqueue   	(struct bkanchor *pAnchor,
			 struct bktbl *pbktbl);

/* END of function prototypes */

/* numBuffers requested is in valid if > 64 or the sum total
 * already given out is greater than 25% of the -B buffer pool.
 */
#define BK_MAX_SEQ_BUFFERS(pdb)                         \
      ((ULONG)((pdb)->pdbpub->argbkbuf / 4))

#define BK_BAD_SEQ_BUFFERS(pdb, numBuffers)             \
     ( (numBuffers) > 64) ||                            \
     ( ((numBuffers) + (pdb)->pdbpub->numROBuffers) >   \
         (BK_MAX_SEQ_BUFFERS((pdb)) ) ? 1 : 0 )

#define BK_GET_SEQ_BUFFERS(pdb, numBuffers)                \
     if ((numBuffers) > 64) (numBuffers) = 64;             \
     if ( ((numBuffers) + (pdb)->pdbpub->numROBuffers) >   \
         (BK_MAX_SEQ_BUFFERS((pdb)) ) )                    \
        numBuffers = (BK_MAX_SEQ_BUFFERS((pdb)) - (pdb)->pdbpub->numROBuffers )


#define BKSTACKLOG  0	/* dont log buffer stack operations */

#if SHMEMORY
#define BK_HAS_APWS      1      /* we can have page writers */
#define BK_HAS_BSTACK    1      /* have a buffer lock stack */
#else

#define BK_HAS_APWS      0      /* NO page writers */
#define BK_HAS_BSTACK    0      /* NO buffer lock stack */
#endif

#if BK_HAS_BSTACK
/* we are keeping a lock stack */
/* macro to add an entry to the buffer lock stack */

#define BK_PUSH_LOCK(pu, pb) \
  if ((pu)->uc_bufsp >= UC_BSSIZE)\
     MSGD_CALLBACK(pcontext, (TEXT *)"%Gbuffer stack overflow");\
  (pu)->uc_bufstack[(pu)->uc_bufsp] = QSELF (pb);\
  (pu)->uc_bufsp++

/* macro to remove an entry from the block stack */

#define BK_POP_LOCK(pcontext, pb) bmPopBufferStack(pcontext, pb)

#else
/* we are not keeping a lock stack */

#define BK_POP_LOCK(bmPopBufferStack, pb)
#define BK_PUSH_LOCK(pu, pb)
#endif /* not BK_HAS_BSTACK */

/* block stack logging macros for debugging */

#if BKSTACKLOG
#define BK_LOG_POP(pu, pbk) \
   MSGD_CALLBACK(pcontext, (TEXT *)"%Lpop  %D %i", \
                 (pbk)->bt_dbkey, (pu)->uc_bufsp)

#define BK_LOG_PUSH(pu, pbk) \
  MSGD_CALLBACK(pcontext, (TEXT *)"%Lpush  %D %i", \
                (pbk)->bt_dbkey, (pu)->uc_bufsp)
#else
#define BK_LOG_POP(pu, pbk)
#define BK_LOG_PUSH(pu, pbk)
#endif

#endif /* #ifndef BMPRV_H */

