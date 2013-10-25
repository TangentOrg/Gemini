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

#ifndef BKCTL_H
#define BKCTL_H

#if !PSC_ANSI_COMPILER

#define BKDEBUG     1	/* do extra sanity checks */

/*****************************************************************/
/**								**/
/**	bkctl.h - describes information managed by the		**/
/**	physical block manager.					**/
/**								**/
/*****************************************************************/
#ifndef SHPTR
#include "shptr.h"
#endif

/* The lru chains */

#define BK_GNLRU        0      /* the general lru chain */
#define BK_IXLRU        1      /* the index block lru chain */
#define BK_SELRU        2      /* the secondary lru chain (for reports) */

#define BK_MAXLRU       4      /* max number of lru chains */

/* Numbers for other chains (numbers start after lru chain) */

#define BK_CHAIN_APW    3       /* anchor for apw chain */
#define BK_CHAIN_CKP    4       /* anchor for checkpoint chain */
#define BK_CHAIN_BKU    5       /* anchor for backup chain */
#define BK_CHAIN_MOD    6       /* anchor for backup chain */

#define BK_NUM_CHAINS   7       /* How many chain anchors are there all told */


/* lru chain anchor */

struct bkchain
{
    QBKTBL  bk_qhead;   /* head of chain */
    QBKTBL  bk_qtail;   /* tail of chain */
    ULONG   bk_nent;    /* number of blocks on chain */
    COUNT   bk_lrulock; /* which lock do we use for this chain */
};

/* other chain anchors */

struct bkanchor
{
    QBKTBL  bk_qHead;		/* pointer to head of circular chain */
    ULONG   bk_numEntries;	/* number of blocks on chain */
    COUNT   bk_linkNumber;	/* Which link entry in bktbl do we use */
    COUNT   bk_lockNumber;	/* Latch number to lock this chain */
};

#define BKANCHOR struct bkanchor

/* Data generated at checkpoints */

struct bkcpdata
{
    ULONG   bk_cpDirty;		/* buffers dirty at checkpoint */
    ULONG   bk_cpMarked;	/* buffers scheduled for checkpoint */
    ULONG   bk_cpFlushed;	/* buffers flushed at checkpoint */
    ULONG   bk_cpBegTime;	/* checkpoint begin time */
    ULONG   bk_cpEndTime;	/* time checkpoint queue emptied */
    ULONG   bk_cpNumDone;	/* number of buffers checkpointed */
    ULONG   bk_cpcWrites;	/* writes from ckp queue */
    ULONG   bk_cpaWrites;       /* writes from apw queue */
    ULONG   bk_cpsWrites;	/* writes from scan cycle */
    COUNT   bk_cpRate;		/* write rate used */
};

#define BKCPDATA struct bkcpdata

#define BK_NUM_CPSLOTS 8	/* number of checkpoints to keep data for */
#define BK_CPSLOT_MASK 0x7	/* mask for computing array index */

typedef struct bkctl
{
    QBKFTBL bk_qftbl[4];/* file table anchors for db, bi, and ai file(s).
			   3 must == BKETYPE in bkftbl.h */

    QBKFSYS bk_qfsys;	/* chain of file system blocks for raw i/o */
    QBKTBL  bk_qbktbl;  /* cahin through all bktbl entries */

    LONG    bk_numbuf;	/* total number of database buffers */
    LONG    bk_numsbuf; /* number of secondary database buffers */
    LONG    bk_numibuf; /* number of database index buffers */
    LONG    bk_numgbuf; /* number of general purpose database buffers */
    LONG    bk_inuse;   /* number of buffers with blocks in them */

    DBKEY   maxdbkey;   /* highest valid dbkey */
    LONG    bk_otime;	/* time the .db was opened */
    int     bk_dbfile;	/* fd for structure file, if multi file db,
			   else fd for db */

    GBOOL    bk_olbackup;/* online backup is active */
    DBKEY   bk_bunext;	/* dbkey of next block to back up */
    DBKEY   bk_bulast;	/* last block which will be backed up */
    UCOUNT  bk_buinc;	/* lowest BKINCR to back up */
    QBKTBL  bk_qbutail; /* obsolete */
    QBKTBL  bk_qbuhead; /* obsolete */

    QBKTBL  bk_qapwhead;/* obsolete */
    LONG    bk_apwcnt;  /* obsolete */

    LONG    bk_chkpt;   /* current checkpoint interval */
    LONG    bk_crlctr;  /* obsolete */
    LONG    bk_mkclast; /* obsolete */
    LONG    bk_prlctr;  /* obsolete */
    LONG    bk_dirtylast; /* obsolete */

    LONG    bk_mkcpcnt; /* number of blocks marked for checkpoint */
    LONG    bk_numdirty;/* approximate number of dirty blocks */
    COUNT   bk_numlru;  /* number of lru chains being used */
    COUNT   bk_lrumax;  /* limit on how far down lru chain to search */
    COUNT   bk_numapw;  /* number of apw's running */

    QUSRCTL bk_qwtnobuf;/* wait queue for users waiting for free buffer */

    struct bkchain bk_lru[BK_MAXLRU]; /* lru chains */

    BKANCHOR 	bk_Anchor[BK_NUM_CHAINS]; /* other chain anchors */
    BKCPDATA    bk_Cp[BK_NUM_CPSLOTS]; /* checkpoint data */
    LONG    bk_cpwrate; /* current checkpoint write rate (buffers / sec) */
    COUNT   bk_recycle; /* times to recycle IX blocks on lru chain */

    LONG    bk_fill1[4];/* extras */
    TEXT    bk_timing;  /* on if we are doing i/o timing */
    TEXT    bk_fill2[3];/* pad */
    LONG    bk_hash;	/* hash prime */
    QBKTBL  bk_qhash[1];/* hash table for block search - see bkseto #*/
			/* NOTE: bk_qhash MUST BE LAST because the size of
			   the array is computed at run time and added to
			   the end of this structure */
} bkctl_t;
#define BKCTL struct bkctl

/* DOS and others only allow 3 characters for an extention, so we can 
   only name file from .d1 to .d99 
*/
#if OPSYS==MS_DOS
#define MAXBKFDS 100	/* up to 100 open files per database */
#else
#define MAXBKFDS 256	/* up to 256 open files per database */
#endif

#define BKFDTBL struct bkfdtbl

struct bkfdtbl
{
    int		 numused;
    bkiotbl_t	*pbkiotbl[1];
};

#define BKHASHDBKEY(d) \
       (((DBKEY)((d) >> (COUNT)RECBITS)) % (DBKEY)(pbkctl->bk_hash))


/* handy macro to access hash chain */

#define bkqhash (p_dbctl->pbkctl->bk_qhash)

/* Buffer states (in pbktbl->bt_busy) - note: the order is important here
   because various places check for states less than some value */
 
#define BKSTFREE        0 /* block is available */
#define BKSTSHARE       1 /* block is locked for shared access */
#define BKSTINTENT	2 /* block is share locked, but with intent to modify */
#define BKSTEXCL        3 /* block is locked for exclusive access */
#define BKSTIO          4 /* block is being read from or written to disk */

#if DIRECTIO
/* buffers aligned on 16 byte boundary for direct i/o routines */

#define BK_BFALIGN	16
#else
/* buffers aligned to longword boundaries */

#define BK_BFALIGN	4
#endif

#else
    "bkctl.h is obsolete - include bmpub.h instead."
#endif  /* #if !PSC_ANSI_COMPILER */

#endif  /* #ifndef BKCRASH_H */
