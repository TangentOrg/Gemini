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

#ifndef USRCTL_H
#define USRCTL_H

/*****************************************************************/
/**                                                             **/
/**     usrctl.h - one copy of this structure for each user.    **/
/**     In the single user case, it is a local structure in     **/
/**     drdbset. In the server case, the usrctl array is        **/
/**     allocated in the general storage pool, after parameters **/
/**     have been processed. In either case, pusrctl points to  **/
/**     the first (only) usrctl and argnusr contains the number **/
/**     of them (1). The -n parameter may be used to tell       **/
/**     the server how many usrctl's to allocate. It must be	**/
/**     a positive number not greater than MAXUSR, if not       **/
/**     specified it defaults to DEFTUSR.                       **/
/**                                                             **/
/*****************************************************************/
struct omTempObject;

#include "shmpub.h"
#include "dbconfig.h"

#define UC_BSSIZE	32	/* number of entries in buffer lock stack */
#define UC_LSSIZE	32	/* number of entries in latch stack */

typedef struct usrctl 
{
    TEXT    uc_wait;       /* reason user is waiting, see #defs */
    TEXT    uc_latchwaked; /* set by waker when waiting on latch */
    TEXT    usrwake;       /* set by waker when waiting for lock or rsrc */
    TEXT    uc_flags1;     /* see #defines below */
    COUNT   uc_rsrccnt;    /* used for vms queueing */
    COUNT   uc_wlatchid;   /* id of latch waiting on */

    QUSRCTL uc_qwtnxt;     /* next in chain if queued for something */
    ULONG   waitArea;      /* area number when uc_wait1 contains dbkey */
    DBKEY   uc_wait1;      /* misc indicator for waiting user    */
    QLCK    uc_qwait2;     /* misc indicator for waiting user    */

    QUSRCTL uc_qwnxtl;     /* next in latch wait queue */
#if 1
    struct nmsgbuf *uc_ppfq; /* points to user's non-active prefetch requests */
    struct dbmsg *uc_psq;  /* points to user's entry in the suspension que */
#else
    QLCK    uc_qlkreq;     /* future - our last queued lock request */
    QLCK    uc_qlkconf;    /* future - the conflicting lock entry */
#endif
    QLCK    uc_qlkhead;    /* future - first lock held by this user */
    ULONG64 uc_lkmask;     /* which lock chains does lktend have to check */

    struct  nssusr *pnssusr; /* to sesion usr in nssctl.h, if any */
    QSRVCTL qsrvctl;       /* to this remote user's server, if any*/
    QTEXT   uc_qusrnam;    /* to user's id */
    QTEXT   uc_qttydev;    /* to user's tty addr */
    LONG    uc_task;       /* task# if one is active, else 0     */
    LONG    uc_scupd;      /* has schema been updated lately?    */
    int     uc_pid;        /* pid of the child/client process */
    int     uc_svpid;      /* future - pid of remote user's server */
    psc_user_number_t uc_svusrnum; /* future - user # of remote user's server */
    int     uc_schlk;      /* type of schema lock held by user   */
    LONG    uc_logtime;    /* time user logged in/connected */
    LONG    uc_lstaddr;    /* becoming obsolete with 2Gig project */
    UCOUNT  uc_usrtyp;     /* user's type see #define below */
    psc_user_number_t   uc_usrnum;     /* usrctl number, 0 to argnusr-1 */

    COUNT   uc_bufs;       /* number of private buffers for this user*/
    int     uc_semid;      /* id of semaphore user waits on */
#if OPSYS==WIN32API
    HANDLE  uc_semnum;     /* number of semaphore the user waits on */
#else
    psc_user_number_t   uc_semnum; /* number of semaphore the user waits on */
#endif
    LONG    waitcnt[WAITCNTLNG]; /* count of waits for all the lock types*/
    LONG    lockcnt[WAITCNTLNG]; /* count of locks for all the lock types*/
    TEXT    usrinuse;      /* 1 if user is active, else 0 */
    TEXT    usrtodie;      /* 1 if user is to be killed, else 0 */
    TEXT    resyncing;     /* 1 if resyncing - only used in mu.c */
    TEXT    ctrlc;         /* 1 if the user hit ctrlc - not used */

    TEXT    uc_2phase;     /* 2phase flags - defined below . */
    TEXT    uc_txblk;      /* type of trans begin lock (none, share, excl) */
    TEXT    uc_txelk;      /* type of trans end lock (none, share, excl) */
    TEXT    lktbovfw;      /* LKVFLOW - usr caused locktable overflow,
		              INMTREJ - allowed 2 extend for resync */

    QBKTBL  uc_blklock;    /* which block do we have X or U on */  
    LONG64  uc_lstdpend;   /* last bi note counter value */
    DBKEY    uc_rmdbkey;    /* rm block to allocate from for this user */

    ULONG   uc_latches;    /* 32 bit mask for what latches user holds */
    COUNT   uc_latsp;      /* latch lock stack index */
    COUNT   uc_bufsp;      /* buffer lock stack index */
    ULONG   uc_latstack[UC_LSSIZE]; /* latch stack */
    QBKTBL  uc_bufstack[UC_BSSIZE]; /* buffer lock stack */
    COUNT   uc_latfree;    /* non-zero means we are in the middle of a 
                              free/relatch pair */
    QUSRCTL   qself;	/* self-referential shared pointer */
    struct svr_icb *uc_psvicb;	/* to icb of the server */
    TEXT           *uc_savedpicb;/*saved icb ptr of 1st rec during ambig chk*/
    LONG    uc_saveddbk;	/* saved dbkey of 1st record during ambig chk */
    int     uc_savedrsz;	/* saved rec size of 1st rec during ambig chk */
    int     uc_savedcid;	/* saved cursor id while auxiliary is used */
    COUNT   uc_svcinum;		/* ci number for a check by the server */
    TEXT    uc_qflags;		/* query related flags */

    COUNT   uc_txecnt;	   /* txe lock depth */
    COUNT   uc_fill1;      /* spare */
    ULONG   uc_dbwork;     /* future - measure of useful work done in db */
    ULONG   uc_dbcost;     /* future - measure of cost of work done in db */

    short   uc_uid;	/* set but not used */
    short   uc_gid;	/* set but not used */
    LONG    uc_ailoc;      /* set but not used */
    LONG    uc_aictr;      /* set but not used */

    LONG    uc_shutloop;   /* shutdown loop counter */
    LONG    uc_prevlstaddr; /* Becoming obsolete with 2Gig project  */
    LONG    uc_numdbs;      /* shutdown; num of dbs connected to */
    COUNT   uc_inservice;   /* shutdown; if any db is inservice */
    ULONG   uc_lastAiCounter; /* mb_aictr value of last not written to
                                 the ai file for this user.            */
    ULONG   uc_lockTimeout; /* Time in seconds user is willing to wait
                               for locks when there is a lock conflict  */
    ULONG   uc_savePoint;   /* current savepoint within a transaction */
    psc_user_number_t   uc_wusrnum; /* if a user requests a resource or a
                                lock, but is refused with RQSTQUED, then this
                                is temporarily set to the uc_usrnum of the 
                                first user who holds a conflicting lock.
                                Its username/tty is then send back to the
                                requesting user by svpmtoc              */
    ULONG    uc_connectNumber; /* Connect sequence number 
                                * prevents usage of stale context
                                */
    LONG     uc_userLatch;  /* Latch on THIS usrctl
                               * (used for thread synchronization)
                               */
    GTBOOL    uc_loginmsg;     /* I printed a login message to .lg file    */
    GTBOOL    uc_heartBeat;
    COUNT    uc_beatCount;
    LONG     uc_sqlhli;       /* hli resync flag */
    LONG     uc_lastBlock;    /* last block of .bi file note */
    LONG     uc_lastOffset;   /* last offset of .bi file note */
    LONG     uc_prevlastblk; /* shutdown to see if any work is being done */
    LONG     uc_prevlastoff; /* shutdown to see if any work is being done */
#define      UC_TABLE_LOCKS 8 /* q pointers to up to 8 table locks for
                                this user.                             */   
    QLCK     uc_qtableLocks[UC_TABLE_LOCKS];

    LONG     uc_shrfindlkCt;
    LONG     uc_exclfindlkCt;

    /*** User Status Information ***/
    COUNT    uc_objectId;       /* object id */
    ULONG    uc_objectType;     /* object type */
    ULONG    uc_state;          /* state of the utility */
    ULONG    uc_counter;        /* status counter */
    ULONG    uc_target;         /* end value */
    TEXT     uc_operation[16];  /* operation name */

    /*** Sequential reader buffer information ***/
    UCOUNT   uc_ROBuffersMax;   /* Maximum private read only buffers allowed */
    QBKCHAIN uc_qprivReadOnlyLRU; /* qptr to private read only buffer LRU */

    COUNT    uc_lockinit;	/* 0 if we never got a lock, 1 othewise */
    GTBOOL    uc_block;          /* wait for lock rather than queue request */
    GTBOOL    filler1;           /* available field */

    /*** Lock timeout values */
    LONG     uc_timeLeft;       /* time remaining until lock times out */
    LONG     uc_endTime;        /* time that lock timeout will occur. */
    struct omTempObject *pfirstTtable; /* Pointer to users first temp table */
    struct omTempObject *pfirstTindex; /* Pointer to users first temp index */
 
    /*** values to maintain position when traversing lock table ***/
    struct lck *uc_plck;        /* pointer to lock in current chain */
    COUNT    uc_currChain;      /* lock chain being traversed */

    COUNT    filler2;           /* available field */
} usrctl_t;

/* uc_qflags */
#define UC_BYCLIENT	1	/* server requests selection by client */
#define UC_AMBREFRSH	2	/* suspended amb chk requires record refresh */
#define UC_INUQFIND     4       /* in server selection on unique find */

/* uc_flags1 */

/* 2phase commit flags */
#define TPCRD    1       /* This user is the coordinator for the task */
#define TPRCOMM  2       /* Current task is ready to commit */
#define TPLIMBO  4       /* User and task in limbo state */
#define TPFORCE  8       /* Authorized to kill limbo task */
#define TPNO2PHS 16      /* This user does not run the 2phase protocol */
#define TPWAITNT 32      /* Remote client is Waiting for a note to be written */

/* for usrctl.uc_wait */
#define LATWAIT    0    /* latch waits, for stats only */

/*** long waits: may be very long - application dependent **/
/* TSKWAIT must be the highest numbered long wait */

#define RECWAIT    1    /* user is waiting for a record lock */
#define SCHWAIT    2    /* user is waiting for schema lock */
#define TSKWAIT    3    /* user is waiting for another task to end */

/*** short waits, too short for the user to notice ***/
/* These MUST be bigger than TSKWAIT: see mt.c */

#define IXWAIT	     4  /* not used anymore */
#define INTENTWAIT   4  /* waiting for intent upgrade on db buffer */
#define RGWAIT       5  /* waiting for a record get */
#define READWAIT     6  /* waiting for a .db block read */
#define WRITEWAIT    7  /* waiting for a .db block write */
#define BACKWAIT     8  /* waiting for .db block to be backed up */
#define SHAREWAIT    9  /* waiting for share lock on buffer */
#define EXCLWAIT    10  /* waiting on exclusive lock on buffer */
#define NOBUFWAIT   11  /* waiting for a shared buffer  */
#define COPYWAIT    12  /*** for stats only */
#define AIWWAIT     12  /* ai writer wait for work */
#define BIREADWAIT  13  /* waiting for a .bi block read */
#define BIWRITEWAIT 14  /* waiting for a .bi block write */
#define AIREADWAIT  15  /* waiting for a .ai block read */
#define AIWRITEWAIT 16  /* waiting for a .ai block write */
#define AUXWAIT     17  /* not used */
#define DBWAIT      18  /* not used */
#define MTWAIT      19  /* not used */
#define BIWWAIT     17  /* bi writer wait for work */
#define TXSWAIT     18  /* txe share lock wait     */
#define TXBWAIT     19  /* txe update mode lock wait */
#define TXEWAIT     20  /* waiting for txe commit mode lock */
#define TXXWAIT     21  /* waiting for txe exclusive mode lock */
/* If you add a wait code be sure to chane WAITCNTLNG in dbconfig.h */

#define DEADUSER   99   /*** for debug only                             */

#define INTMREJ    1    /* user is in the middle of tmrej               */
#define LKOVFLOW   2    /* Ruser cause lock table overflow              */

#ifdef USRGLOB
GLOBAL USRCTL   *pusrctl = NULL;
#if 1 /* Global pholder is going away... */
GLOBAL USRCTL  *pholder = NULL; /* if a user requests a resource or
                                a lock, but is refused with RQSTQUED, then
                                this is temporarily set pointing the the
                                first user who holds a conflicting lock.
                                That username/tty is then send back to the
                                requesting user by svpmtoc              */
#endif
#else
IMPORT USRCTL  *pusrctl;
#if 1 /* Global pholder is going away... */
IMPORT USRCTL  *pholder;
#endif
#endif

#define UC_PFDCTL	1  /* pfdctl = null  returns 0 */
#define UC_RESYNCING	2  /* pfdctl->resyncing != 0, returns 1 */
#define UC_FDISOPEN	4  /* fdisopen = 0 returns 0 */

#endif  /* #ifndef USRCTL_H */

