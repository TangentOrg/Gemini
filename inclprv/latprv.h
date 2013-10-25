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

#ifndef LATPRV_H
#define LATPRV_H

/* for future use - ghb */

#define MT_HAS_LATCH_SEMAPHORES 0 /* latches do not have own semaphores */
#define MT_HAS_QUEUE_SEMAPHORE  0 /* we do not have a semaphore for queues */
#define MT_HAS_SPINLOCKS  1 /* we can have spinlocks */

#include "latpub.h"
#include "shmpub.h"     /* Needed for QUSRCTL */
#include "dbpub.h"      /* needed for DB_EXBAD */
#include "dbconfig.h"   /* for WAITCNTLNG     */

int	tstset(spinlock_t *pmt);
int	tstclr(spinlock_t *pmt);

/* latch size and alignment per machine */
#if OPSYS==MPEIX
#define MT_LATCH_SIZE   128
#define MT_SPIN_ALIGN   16
#endif  /* MPEIX */

#if MIPS3000
#define MT_LATCH_SIZE   128
#define MT_SPIN_ALIGN   4
#endif /* MIPS3000 */

#if HP825
#define MT_LATCH_SIZE	128
#define MT_SPIN_ALIGN	16
#endif


#if OPSYS==OS2
#define MT_LATCH_SIZE   0
#define MT_SPIN_ALIGN   4
#endif /* OS2 || WIN32API */

#if ALPHAOSF
/* the following are required for good bus performance */
#define MT_LATCH_SIZE   128             /* distance between spinlocks */
#define MT_SPIN_ALIGN   MT_LATCH_SIZE   /* latch alignment requirement */
#endif

/* If these were not set above, then we set them now */

#ifndef MT_LATCH_SIZE
#define MT_LATCH_SIZE	128	/* distance between spinlocks */
#endif

#ifndef MT_SPIN_ALIGN
#define MT_SPIN_ALIGN	4	/* latch alignement requirement */
#endif


#if OPSYS==MPEIX
#include <sys/sem.h>   /* includes <sys/ipc.h> */
#include <sys/shm.h>
#define MT_TASDEFINED 1
/* Note "not" tstset; see note at HP825 */
#define MT_TEST_AND_SET(pLock)  (!tstset (pLock))
#define MT_CLEAR_LOCK(pLock)    (tstclr (pLock))
#define MT_IS_LOCKED(pLock)     (!test_lock (pLock))
#define MT_IS_FREE(pLock)       (test_lock (pLock))
#endif  /* MPEIX */


#if MIPS3000
#define _STDIO_ 1
#define MT_TASDEFINED 1
#define MT_TEST_AND_SET(pLock)      (tstset (pLock))
#define MT_CLEAR_LOCK(pLock)        (tstclr (pLock))
#define MT_IS_LOCKED(pLock)         (*(pLock) != 0)
#endif /* MIPS3000 */


#if NOTSTSET || MIPSTAS
/* no test and set available. We should use a semaphore for this.
   AUXSEM would be suitable. Or we could give each latch its own
   semaphore if we wanted to */
#define MT_TASDEFINED 1
#define MT_TEST_AND_SET(pLock)	gronk notas 
#define MT_CLEAR_LOCK(pLock)	gronk notas 
#define MT_IS_LOCKED(pLock)	gronk notas 
#define MT_IS_FREE(pLock)	gronk notas 
#endif


#if SEQUENT || SEQUENTPTX
    /* Sequent 386/486. S_CLOCK is an inline assembly code macro */
#undef COUNT /* to allow the sequent parallel.h to redefines it */ 
#include <parallel/parallel.h>
#undef COUNT /* undo the sequent parallel.h redefined COUNT */ 
#define COUNT   short   /* restore to dstd.h value: counting -32k to +32k*/
#define MT_TASDEFINED 1
#define MT_TEST_AND_SET(pLock)	(S_CLOCK (pLock) == 0)
#define MT_CLEAR_LOCK(pLock)	(*(pLock)) = 0
#define MT_IS_LOCKED(pLock)	(*(pLock) != 0)
#define MT_IS_FREE(pLock)	(*(pLock) == 0)
#endif /* SEQUENT || SEQUENTPIX */

 
#if OPSYS==VMS
    /* DEC C generates in-line test and set bit instruction by
        use of the branch on bit set and set (or clear and clear)
        interlocked built in functions */
#define MT_TASDEFINED 1
#include <builtins.h>

#define MT_TEST_AND_SET(pLock)	(_BBSSI (0L, (pLock)))
#define MT_CLEAR_LOCK(pLock)	(_BBCCI (0L, (pLock)))
#define MT_IS_LOCKED(pLock)	(*(pLock) != 0)
#define MT_IS_FREE(pLock)	(*(pLock) == 0)
#endif /* VMS */


#if SGI
#include <mutex.h>
    /* Silicon graphics has system calls for these. see man pages */
#define MT_TASDEFINED 1
#define MT_TEST_AND_SET(pLock)  acquire_lock(pLock)
#define MT_CLEAR_LOCK(pLock)    release_lock(pLock)
#define MT_IS_LOCKED(pLock)     (stat_lock(pLock) != 0)
#define MT_IS_FREE(pLock)       (stat_lock(pLock) == 0)
#endif /* SGI */


#if HP825
    /* hp825 tstset retruns the opposite than expected. A value of 1 means
       unlocked and a value of 0 means locked !!!! */
#define MT_TASDEFINED 1
#define MT_TEST_AND_SET(pLock)	(!tstset (pLock))
#define MT_CLEAR_LOCK(pLock)	(tstclr (pLock))
#define MT_IS_LOCKED(pLock)	(!test_lock (pLock))
#define MT_IS_FREE(pLock)	(test_lock (pLock))
#endif /* HP825 */


#if DECSTN
    /* A Mips box, may or may not have tas */
#include <sys/lock.h>
#define MT_TASDEFINED 1
#define MT_TEST_AND_SET(pLock)	(atomic_op (ATOMIC_SET, (pLock)))
#define MT_CLEAR_LOCK(pLock)	(atomic_op (ATOMIC_CLEAR,(pLock)))
#define MT_IS_LOCKED(pLock)	gronk mtislocked /* (*(pLock) != 0) ????? */
#define MT_IS_FREE(pLock)	gronk mtisfree /* (*(pLock) == 0) ????? */
#endif  /* DECSTN */


#if IBMRIOS
#define MT_TASDEFINED 1
/* Structure to reference locking millicode  on AIX 4.x */
static struct
    {
        int w1;
        int w2;
        int w3;
    } check_lock_des = { 0x00003420, 0, 0 };
/* Structure to reference clear millicode on AIX 4.x */
static struct
    {
        int w1;
        int w2;
        int w3;
    } clear_lock_des = { 0x00003400, 0, 0 };
/* Function pointers for the test/set and test/clear calls */
LOCALF int (*csptr) () = (int (*)())(&check_lock_des);
LOCALF int (*clptr) () = (int (*)())(&clear_lock_des);
#define MT_TEST_AND_SET(pLock)  ((*csptr) (pLock, 0, 1))
#define MT_CLEAR_LOCK(pLock)    ((*clptr) (pLock, 0, 1))
#define MT_IS_LOCKED(pLock)     (*(pLock) != 0)
#define MT_IS_FREE(pLock)       (*(pLock) == 0)
#endif  /* IBMRIOS */


#if OPSYS==OS2
#define MT_TASDEFINED 1
#define MT_TEST_AND_SET(pLock)      ((*(pLock)) = 1)
#define MT_CLEAR_LOCK(pLock)        ((*(pLock)) = 0)
#define MT_IS_LOCKED(pLock)         (*(pLock) != 0)
#define MT_IS_FREE(pLock)	    (*(pLock) == 0)
#endif /* OS2 */

#if OPSYS==WIN32API
#define MT_TASDEFINED 1
#define MT_TEST_AND_SET(pLock)  (tstset (pLock))
#define MT_CLEAR_LOCK(pLock)    (tstclr (pLock))
#define MT_IS_LOCKED(pLock)     (*(pLock) != 0)
#define MT_IS_FREE(pLock)       (*(pLock) == 0)
#endif /* WIN32API */

#if UNIX486V4 || SUN4 || SUN45 || SCO || ALPHAOSF || LINUX
#define MT_TASDEFINED 1
#define MT_TEST_AND_SET(pLock)  (tstset (pLock))
#define MT_CLEAR_LOCK(pLock)    (tstclr (pLock))
#define MT_IS_LOCKED(pLock)     (*(pLock) != 0)
#define MT_IS_FREE(pLock)       (*(pLock) == 0)
#endif /* UNIX486V4 || SUN4 || SUN45 || SCO || ALPHAOSF */



#ifndef MT_TASDEFINED
    /* none of the above - others which have working test-and-set subroutine
       which uses a value of 1 to mean locked and 0 to mean unlocked */
#define MT_TASDEFINED 1
#define MT_TEST_AND_SET(pLock)	(tstset (pLock))
#define MT_CLEAR_LOCK(pLock)	((*(pLock)) = 0)
#define MT_IS_LOCKED(pLock)	(*(pLock) != 0)
#define MT_IS_FREE(pLock)	(*(pLock) == 0)
#endif


/* The shared memory latch structure.

   There is an array of MTL_MAX of these in the mtctl structure, one
   for each of the 32 latches.

   The latches must be MT_LATCH_SIZE bytes long so that they will fall
   into different cache blocks. This is so that spins and locks for one
   latch will not affect the caching of other latches.

   In addition, on some machines there are alignment requirements for
   the locks, dictated by hardware constraints and the workings of
   test-and-set. For example, HP PA-RISC architecture requires that
   the locks be aligned to 16 byte boundaries.

   For these reasons, the latches must be the first thing in the mtctl
   structure, and they include padding if necessary. The mtctl structure
   is allocated in dbenv1() in dbenv.c and is aligned according to the
   value of MT_SPIN_ALIGN.

   Note that MT_LATCH_SIZE should be an integer multiple of MT_SPIN_ALIGN 
   to ensure proper alignment of test-and-set locks (or zero if there is
   no alignment requirement and the cache line is smaller than the latch
   structure).

   The latches are locked in the following ways:

	If the latch is a spinlock, then the latch is locked by doing a
	test-and-set on fastLock and spinning until it succeeds.

	If the latch is not a spinlock, then it is locked by locking
	fastLock with a test-and-set, but without spinning and checking
	the value of slowLock. If slowLock is 0 the latch is free and can
	be taken by setting slowLock to 1 and clearing fastLock. If the
	latch is not free, the process inserts itself in the wait queue
	for the latch, clears fastLock and waits on its semaphore.
	When the latch holder releases it, it wakes the first process
	in the wait queue and then clears fastLock.

	When -spin is specified, most latches are spinlocks. If -spin
	is not specified, no latches are spinlocks.
*/

typedef struct
{
    spinlock_t	fastLock;	/* we test-and-set on this. Must be aligned */
    LONG	slowLock;	/* if not spinlock, lock value is here */

    COUNT	latchId;	/* id of the latch, for convenience */
    psc_user_number_t holder;	/* user number of last latch holder */
    psc_user_number_t qholder;	/* user number of last queue holder */
    COUNT	ltype;		/* latch type */

    QUSRCTL	qhead;		/* usrctl of first waiter if not spinlock */
    QUSRCTL	qtail;		/* usrctl of last waiter if not spinlock */

    /* The following are for sanity checks */

    ULONG	mask;		/* bits for latches that you may hold if you
                                   try to get this one */
    ULONG	lockCnt;	/* number of times it was locked  - used by
                                   watchdog to detect dead latch owners */

    /* The following are used only if latch statistics are enabled */

    ULONG	busyCnt;	/* number of times it was busy when locking */
    ULONG	spinCnt;	/* total spin iterations */
    ULONG	waitCnt;	/* total spin timeouts or times queued */

    /* The following are used only if latch timing is enabled */

    ULONG	lockedTime;	/* for calculating duration of lock */
    ULONG	lockTime;	/* microseconds it was locked */
    ULONG	waitTime;	/* microseconds of wait or spin time */

    int		semId;		/* future */
    int		semNum;		/* future */

    COUNT       latchDepth;     /* lock count for each latch */
 
} latch_st;

typedef struct latch
{
    latch_st	l;		/* Actual latch structure */

#if MT_LATCH_SIZE > 0
    /* insert some padding so next latch begins at the correct spot
       (i.e. aligned correctly and in another cache line */

    TEXT	lpad[MT_LATCH_SIZE - sizeof(latch_st)];
#endif
} latch_t;

/* *********************************************************************** */
/* muxlatches - each set is controlled by a latch. They are used only
   when -spin has been specified as a startup parameter */
 
typedef struct muxctl
{
    COUNT	latchId;	/* id no. of controlling latch */
    COUNT	nmuxes;		/* no of muxes in the set - 0 if not enabled */
    COUNT	base;		/* no of first mux in the set */
} muxctl_t;

typedef struct muxlatch
{
    TEXT        lock;           /* value of 0 = free, 1 = locked */
    TEXT        fill0;          /* placeholder */
    psc_user_number_t holder;         /* user number of muxlatch holder or -1 */
    ULONG       lockCnt;        /* number of times it was locked  - used by
                                   watchdog to detect dead owners */
} muxlatch_t;

/* *********************************************************************** */

/* BUM - FIXFIX comment:*/
/* BUM - The structure shmemctl should be renamed AND all of the pointers */
/* to it should also be renamed to reflect that this structure is for     */
/* latch control only.                                                    */
/* BUM - FIXFIX */

typedef struct shmemctl	/* shared memory control #*/
{
#if SHMEMORY
    latch_t	latches[MTL_MAX + 1]; /* MUST BE FIRST */
    LONG	waitcnt[WAITCNTLNG]; /* count of waits for all the lock types*/
    LONG	lockcnt[WAITCNTLNG]; /* count of locks for all the lock types*/
#endif
    LONG	slk_ctrs[5]; /* schema lock counters #*/
    LONG	unused_a;    /* to save the todays date for the .lg file*/
    LONG	tracecnt;
    DBKEY	unused2;
    int		argsdmsg;    /* not used */
    int		unused_b;    /* process id of broker */ 
    psc_user_number_t	usrcnt;      /* count of logged on users #*/
    GTBOOL	unused_c;    /* everybody must shutdown */
    GTBOOL	unused3;
    TEXT	mt_inited;   /* latches are initialized and usable */
#if SHMEMORY
    TEXT        mt_stats;    /* on if collecting latch statistics */
    TEXT        mt_timing;   /* on if collecting latch and i/o timing */
    ULONG	mt_spins;    /* spin limit before nap */
    muxctl_t	mt_muxctl[MTM_MAX + 1];
    muxlatch_t  mt_muxes[1]; /* MUST BE LAST - size determined at runtime */
#endif
} shmemctl_t;

/* *********************************************************************** */
/* Useful Macros */

#define MT_PUSH_LOCK(p,l)     ((p)->uc_latstack[p->uc_latsp++] = (l))
#define MT_POP_LOCK(p)        ((p)->uc_latsp--)
#define MT_MARK_LOCKED(p,l)   ((p)->uc_latches |= (1 << (l)))
#define MT_MARK_UNLOCKED(p,l) ((p)->uc_latches &= (~(1 << (l))))

/* BUM FIXFIX - msgd(s) need to be converted to msgn(s) */
#if MTSANITYCHECK
#define MT_CHECK_ID(p,l) if (((l) < 1) || ((l) > MTL_MAX))\
{  pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;\
   MSGD_CALLBACK(pcontext, (TEXT *)"11/16/00Lock id invalid", (l)); }

#define MT_CHECK_STACK(p,bot) if (((p)->uc_latsp < (bot)) ||\
 ((p)->uc_latsp >= UC_LSSIZE))\
{ pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;\
  MSGD_CALLBACK(pcontext, (TEXT *)"%Glock stack underflow %i", (p)->uc_latsp);\
}

#define MT_CHECK_TOP(p,l) \
   if (((p)->uc_latstack[(p)->uc_latsp - 1] & 0xFF) != (l))\
{  pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;\
   MSGD_CALLBACK(pcontext,\
           (TEXT *)"%Glock stack corrupt expected %i was %i sp %i", (l),    \
           (p)->uc_latstack[(p)->uc_latsp - 1] & 0xFF, (p)->uc_latsp - 1);  \
}

#else
#define MT_CHECK_ID(p,l)
#define MT_CHECK_STACK(p,bot)
#define MT_CHECK_TOP(p,l)
#endif

#endif /* LATPRV_H */
