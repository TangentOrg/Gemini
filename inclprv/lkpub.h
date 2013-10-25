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

#ifndef LKPUB_H
#define LKPUB_H

#include "shmpub.h" /* needed for SHPTR */
#include "latpub.h" /* temp for mt renames */

#include "dsmdtype.h"    /* typedefs for dsm api */
struct dsmContext;
struct usrctl;
/*************************************************************/
/* Public Function Prototypes for lkmu.c                     */
/*************************************************************/
/* BUM Work Around */
#define lkrmtask svremt         
#define lkwaketsk svwaket        
#define lkwakeusr slwakeu       
#define lkwakeup slwake       
#define lkunchain slunchain 
#define lkrmwaits remwaits  
#define lktskchain tskchain 
#define lkservcon servgone  
/* End of BUM Work Around */

DSMVOID lkrmtask           (struct dsmContext *pcontext, struct usrctl *premusr);
int lkwaketsk           (struct dsmContext *pcontext, LONG taskn);
DSMVOID lkwakeusr          (struct dsmContext *pContext, struct usrctl *pusr,
                         int type);
DSMVOID lkwakeup           (struct dsmContext *pcontext, QUSRCTL *pqanchor,
                         DBKEY value, int type);
DSMVOID lkunchain          (struct dsmContext *pcontext, int latchNo,
                         struct usrctl  * pusr, QUSRCTL *pqhead, QUSRCTL *pqtail);
LONG lkresync           (struct dsmContext *pContext);
DSMVOID lkrmwaits          (struct dsmContext *pcontext);

#ifdef TRITONDB
/* This should be automaticaly done rather than exposed! */
int DLLEXPORT lktskchain(struct dsmContext *pcontext);
#else
int lktskchain          (struct dsmContext *pcontext);
#endif

int lkservcon           (struct dsmContext *pcontext);  

/* Public Structures and Defines */
/*****************************************************************/
/**                                                             **/
/**                      LOCKING TABLE                          **/
/**                                                             **/
/**                                                             **/
/*****************************************************************/

#define LKDEFLT 100     /* default initial # of locks on free list      */


#define LK_DEFHASH  31  /* default lock table hash prime */
#define LKHASH LK_DEFHASH

/* order is important here - see mulkrecd.c and nsamon.c */

#define LK_RECLK     0 /* Record locks */
#define LK_TABLELK   1 /* Table lockds */
#define LK_PURLK     2 /* Purged record locks */
#define LK_RGLK      3 /* Record-get locks */
#define LK_MAXTYPE   3
#define LK_NUMTYPES  4 /* number of different types of locks we have */


/* definition of the identifier used to lock an object (table or record) */

typedef struct  lockId
{
    DBKEY       dbkey;          /* row id                          */
    LONG        table;          /* table the locked row is in.     */
}lockId_t;

typedef struct lockCounters
{
        COUNT shareLocks;       /* # of current share lcks for object */
        COUNT exclusiveLocks;   /* # of current exclusive lcks for object */
} lockCounters_t;
 
typedef union refCounts
{
    ULONG          referenceCount;  /* table lock counter */
    lockCounters_t lockCounts;      /* record lock strength counters */
} refCounts_t;

typedef struct  lck
{
    QLCK        lk_qnext;      /* shared pointer to next lck entry */
    lockId_t    lk_id;         /* identifier for the locked object  */
    QUSRCTL     lk_qusr;       /* shared pointer to owner */
#if 0
    LONG        lk_trid;       /* transaction that owns the lock  */
#endif
    QLCK        qself;         /* self-referential shared ptr */
    UCOUNT      lk_prevf;      /* prev flags for queued req. used by lkcncl */
    UCOUNT      lk_curf;       /* current flags */
    refCounts_t lk_refCount;   /* union containing lk strength counts */
} lck_t;

#define LCK     struct lck

typedef struct  lkctl
{
        QLCK    lk_qfree;       /* free chain anchor */
        int     lk_hwm;         /* lock table hwm */
        int     lk_cur;         /* lock table entries in use */
        LONG    lk_hash;        /* hash prime */
        int     tableLocksOn;   /* 1 - if table locks are being requested */
        LONG    numTableLocks;
        LONG    timeOfLastTableLock;

        /* index to first anchor of each lock entry type */

        LONG   lk_chain[LK_NUMTYPES];

        /* Must be last - actual size determine by value of lk_hash */

        QLCK    lk_qanchor[1];  /* active chain anchors */
} lkctl_t;

#define LKCTL   struct lkctl

#ifndef LKNOLOCK

/* lock status structure - used to save previous lock status. */
/* Used to restore al lock to its previous state. */
struct lkstat
{
        UCOUNT           lk_prevf;
        UCOUNT          lk_curf;
} ;

#define LKSTAT  struct lkstat

/* lock flags. LKIS, LKIX, LKSHARE, LKSIX, LKEXCL, LKTYPE must not be changed.
   ascending order implies a stronger lock */
/* LKNOLOCK, LKSHARE, LKSIX, LKEXCL, LKNOWAIT are also used as parms to lk */

#define LKNOLOCK 0      /* passed when a lock mode indication
                                   is required and no locking is desired */
#define LKIS    1       /* Intent to get share locks on records          */
#define LKIX    2       /* Intent to get exclusive locks on records      */
#define LKSHARE 4       /* if not queued, this is a share lock, else
                           a share lock request.                         */
#define LKSIX   8       /* Share lock on table with intent to get exclusive
                           record locks.                                 */

#define LKEXCL  16       /* if not queued, this is an exclusive lock,
                           else an exclusive lock request. */

#define LKTYPE  31       /* mask for extracting lock type */

#define LKNOWAIT 32     /* return RECLKED immediately if
                                   requested dbkey not available */
#define LKAUTOREL 64    /* request for a record lock with auto-release */

#endif  /* ifndef LKNOLOCK */

#define LKUPGRD 32       /* this is/was an upgrade request */

#define LKLIMBO 64       /* this is a "limbo" lock. it will not be released
                           until database task end. */

#define LKQUEUED 128     /* this is a queued request */

#define LKNOHOLD 256    /* set with LKQUEUED. when queued request is
                           granted, child will request another, possibly
                           different lock. if the lock requested is
                           different, delete the queued lock */
#define LKPURGED 512    /* this is a purged lock entry */

#define LKINTASK 1024    /* record may be updated*/

#define LKEXPIRED 2048   /* lock in the process of being timed out */

#define LKFULLFLAG 0x7000  /* used as a parm to nssresy only */

#define LK_NO_TABLE_LOCK_TIME 1800 /* If no table locks have been acquired
                                 in this many seconds then turn off
                                 table locking flag which stops record
                                 lockers from getting intent locks on
                                 tables.                               */

/* shared memory locking macros */

/* The cast of dbk to ULONG needs to be done to ensure that the result of
   the modulus operation is positive when dbk is an expression like
   table + recid where table is a schema table and thus a negative number */
#define MU_HASH_CHN(dbk,grp)(((ULONG)(dbk) % plkctl->lk_hash) + \
                             plkctl->lk_chain[(grp)])
#define MU_HASH_PRG(usr)(((usr) % plkctl->lk_hash) + plkctl->lk_chain[LK_PURLK])

#if SHMEMORY

#define MU_LOCK_LOCK_TABLE()    latlatch (pcontext, MTL_LKF)
#define MU_UNLK_LOCK_TABLE()    latunlatch (pcontext, MTL_LKF)

#define MU_LOCK_LOCK_CHAIN(c)                           \
  if (pcontext->pdbcontext->pdbpub->argspin &&          \
      pcontext->pdbcontext->pdbpub->argmux)             \
        if(((c) >= pcontext->pdbcontext->plkctl->lk_chain[LK_TABLELK]) && \
           ((c) < pcontext->pdbcontext->plkctl->lk_chain[LK_TABLELK+1])) \
            latlatch(pcontext,MTL_LKP);                 \
        else latlkmux (pcontext, MTM_LKH, (c) & MTM_LKH_MASK);\
  else latlatch (pcontext, MTL_LHT)

#define MU_UNLK_LOCK_CHAIN(c)                        \
  if (pcontext->pdbcontext->pdbpub->argspin &&       \
      pcontext->pdbcontext->pdbpub->argmux)          \
        if(((c) >= pcontext->pdbcontext->plkctl->lk_chain[LK_TABLELK]) && \
           ((c) < pcontext->pdbcontext->plkctl->lk_chain[LK_TABLELK+1])) \
            latunlatch(pcontext,MTL_LKP);                     \
        else latfrmux (pcontext, MTM_LKH, (c) & MTM_LKH_MASK);\
  else latunlatch (pcontext, MTL_LHT)

/* Note - lock all chains is dependant upon the fact that
   the mux locks used by latlkmux is covered by the MTL_LKH latch.
   Change the latch used to cover the chain mux locks and you
   need to change the macros below.                            */
#define MU_LOCK_ALL_LOCK_CHAINS() latlatch (pcontext, MTL_LHT)
#define MU_UNLK_ALL_LOCK_CHAINS() latunlatch (pcontext, MTL_LHT)
      
#define MU_LOCK_PURG_CHAIN(pc)  latlatch (pcontext, MTL_LKP)
#define MU_UNLK_PURG_CHAIN(pc)  latunlatch (pcontext, MTL_LKP)

#else   /* SHMEMORY */

#define MU_LOCK_LOCK_TABLE()
#define MU_UNLK_LOCK_TABLE()

#define MU_LOCK_LOCK_CHAIN(c)
#define MU_UNLK_LOCK_CHAIN(c)

#define MU_LOCK_PURG_CHAIN(pc)
#define MU_UNLK_PURG_CHAIN(pc)

#endif  /* SHMEMORY */

/*****************************************************************/
/* External Function Prototypes for lkrecd.c                     */
/*****************************************************************/
DSMVOID lkinit             (struct dsmContext *pcontext);
int lklocky             (struct dsmContext *pcontext, lockId_t *plockId,
                         int lockmode, int lockgroup, TEXT *pname,
                         LKSTAT *plkstat);
int lklock              (struct dsmContext *pcontext, lockId_t *plockId,
                         int lockmode, TEXT *pname);
DSMVOID lkrels             (struct dsmContext *pcontext, lockId_t *plockId);
DSMVOID lkrelsy            (struct dsmContext *pcontext, lockId_t *plockId,
                         int lockgroup);
DSMVOID lkpurge            (struct dsmContext *pcontext, lockId_t *plockId);
DSMVOID lktend             (struct dsmContext *pcontext, ULONG64 lockChainMask);
DSMVOID lkrend             (struct dsmContext *pcontext, int flag);
DSMVOID lkcncl             (struct dsmContext *pcontext);
DSMVOID lkcheck            (struct dsmContext *pcontext, lockId_t *plockId,
                         struct lck **pplck);
DSMVOID lkrestore          (struct dsmContext *pcontext, lockId_t *plockId,
                         dsmMask_t lockmode);
DSMVOID lkclrels           (struct dsmContext *pcontext, lockId_t *plockId);
int lkwait              (struct dsmContext *pcontext, int msgNum, TEXT *pname);
int lkrclk              (struct dsmContext *pcontext, lockId_t *plockId,
                         TEXT *pname);
int lkrglock            (struct dsmContext *pcontext, lockId_t *plockId,
                         int lockmode);
DSMVOID lkrgunlk           (struct dsmContext *pcontext, lockId_t *plockId);
int lklockx             (DBKEY dbkey, int lockmode, TEXT *pname);
int lkCheckTrans        (struct dsmContext *pcontext, LONG taskn, int lockmode);
dsmStatus_t lkAdminLock (struct dsmContext *pcontext, dsmObject_t objectNumber,
                         dsmArea_t objectArea, dsmMask_t lockMode);
dsmStatus_t lkAdminUnlock(struct dsmContext *pcontext, dsmObject_t objectNumber,
                         dsmArea_t objectArea);

DSMVOID lkTableLockCheck(struct dsmContext *pcontext);

#endif /* LKPUB_H */



