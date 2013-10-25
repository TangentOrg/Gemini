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

#ifndef BMCTL_H
#define BMCTL_H

#if defined(BMBUM_H)

/*****************************************************************/
/**                                                             **/
/**     bmctl.h - describes buffer control blocks. There        **/
/**     is one buffer control block for each disk block         **/
/**     buffer. These buffers are allocated in the general      **/
/**     storage pool by bkbufo.                                 **/
/**                                                             **/
/**     The buffers are managed as a pool on an LRU basis.      **/
/**     Each buffer can only be obseved in one of three         **/
/**     possible states:                                        **/
/**                                                             **/
/**     Empty. In this state the buffer contains no defined     **/
/**             data and is available for any use.              **/
/**                                                             **/
/**     Containing non-updated disk block. In this state the    **/
/**             buffer contains a particular disk block from    **/
/**             a PROGRESS database. This block is identified   **/
/**             by its block dbkey. bt_pftbl and bt_offst       **/
/**             are filled in to expedite writing if the        **/
/**             block is later modified. Additionally,          **/
/**             bt_raddr is filled in if the block is from      **/
/**             a standard UNIX file and raw I/O has not been   **/
/**             deactivated by the -i or -r options.            **/
/**                                                             **/
/**     Containing an updated disk block. In this state the     **/
/**             buffer contains a block whose contents are      **/
/**             (assumed to be) different from the disk         **/
/**             version of the same block. A buffer may         **/
/**             only reach this state from the previous         **/
/**             state, and normally returns to the previous     **/
/**             state after it has been written back to disk.   **/
/**             In this state, bt_tmask indicates the par-      **/
/**             ticular database tasks which have modified      **/
/**             the block, bt_bidpn indicates how much of the   **/
/**             bi file must be written before this block       **/
/**             can be written.                                 **/
/**                                                             **/
/**     Buffers can actually have two other transitory states.  **/
/**     These are reading and writing. The reading state can    **/
/**     can only be entered from the empty or unmodified        **/
/**     states and leaves the buffer in the unmodified state.   **/
/**     The writing state can only be entered from the          **/
/**     modified state and returns the buffer to the            **/
/**     unmodified state.                                       **/
/**                                                             **/
/*****************************************************************/

#include "shmpub.h"

#define MINBUFS 10      /* minimum number of buffers allowed */
#define XTRABUFS 2      /* bkbufo always gets 2 more than requested */
#define MAXBUFS 125000000  /* arbritrary                               */

#define DFTBUFPU 8      /* default number of shared buffers per user */
#define DFTBUFPR 0      /* default number of non-shared private buffers */
#define DFTTPRPU 2      /* default number of private buf. per user
                           to reserve in the total (in argtpbuf).
                           we assume that only 1/4 of the users will
                           select private buffers each using 8 buffers,
                           it is 2 per every user. */
#define DFTBUFS 20      /* default number of buffers for single-user etc */
#define MINBUFPR 4      /* minimum number of non-shared private buffers */

#define SHRDPOOL       (SHPTR)-1        /* identifies a shared memory pool */

/* Numbers for chain links */

#define BK_LINK_LRU     0       /* MUST BE FIRST: links for lru chain */
#define BK_LINK_APW     1       /* links for apw chain */
#define BK_LINK_CKP     2       /* links for checkpoint chain */
#define BK_LINK_MOD     3       /* links for backup chain */
#define BK_LINK_BKU     4       /* links for backup chain */

#define BK_NUM_LINKS    5       /* How many chains can a block be on at once */

/* lru chain anchor */

#define BKLINK struct bklink


typedef struct bklink
{
    QBKTBL      bt_qNext;       /* q pointer to next in chain or NULL */
    QBKTBL      bt_qPrev;       /* q pointer to previous in chain or NULL */
} bklink_t;

#define BKTBL struct bktbl
typedef struct bktbl
{
    QBKTBL   bt_qhash;  /* next entry in the same hash chain */
    ULONG    bt_curhsh; /* hash value of current contents */
    QBKTBL   bt_qnextb; /* next block - threads through all */
    QBKBUF   bt_qbuf;   /* shared pointer to buffer for with this entry */
    LONG     bt_usecnt; /* use count. 0 = not in use, else locked */
    UCOUNT   bt_latch;  /* which latch controls the bktbl entry */
    UCOUNT   bt_muxlatch; /* which multiplex latch controls the bktbl entry */

    DBKEY    bt_dbkey;  /* dbkey of current block, or 0 if none */
                        /* for .bi and .ai file, is lseek addr or -1 */
    ULONG    bt_area;   /* Database area for block in the buffer.          */
    QBKFTBL  bt_qftbl;  /* shrd ptr to file table entry of extent blk is in */
    LONG64   bt_offst;  /* byte offset within file extent for the block */
    LONG     bt_raddr;  /* byte address within raw dev for this block */
    LONG64   bt_bidpn;  /* bi note which must be written before block is */
    LONG     bt_chkpt;  /* checkpoint interval block was first modified in */
    LONG     bt_spare ; /* An unused long guaranteed to be zero          */

    QBKTBL   bt_qlrunxt;/* shared ptr to next in LRU chain */
    QBKTBL   bt_qlruprv;/* shared ptr to prev in LRU chain */

    QBKTBL   bt_qapwnxt;/* shared ptr to next in apw chain */
    QBKTBL   bt_qapwprv;/* shared ptr to prev in apw chain */

    QUSRCTL  bt_qfstuser;/* first user waiting for this block */
    QUSRCTL  bt_qlstuser;/* last user waiting for this block */
    QUSRCTL  bt_qcuruser;/* current owner of block */

    TTINY    bt_whichlru;/* which lru chain block is in */
    TEXT     bt_ftype;   /* which kind of file: BKDATA, BKBI, or BKAI */
    TEXT     bt_busy;    /* state: free, share, excl, backq, read, write */
    TEXT     bt_pad[1];  /* padding */
    struct               /* status bits */
    {
        unsigned  changed:  1; /* block has been changed */
        unsigned  chkpt:    1; /* block is scheduled for checkpoint */
        unsigned  writing:  1; /* block is being written to disk */
        unsigned  fixed:    1; /* block is fixed in memory and cannot move */

        unsigned  reading:  1; /* not used */
        unsigned  onlru:    1; /* not used */
        unsigned  cleaning: 1; /* not used */
        unsigned  apwq:     1; /* not used */
        unsigned  zzzz:     8; /* filler to make 16 bits total */
    } bt_flags;

    UCOUNT    bt_AccCnt; /* number of accesses since read into memory */
    UCOUNT    bt_UpdCnt; /* number of updates since read into memory */
    UCOUNT    bt_WrtCnt; /* number of times written to disk */
    UCOUNT    bt_ConCnt; /* number of lock conflicts (waits) */

    BKLINK  bt_Links[BK_NUM_LINKS]; /* links for chains block is on */

#if SHMEMORY
    QBKTBL    qself;     /* self-referential shared pointer */
#endif

    COUNT    bt_cycle;   /* times to recycle this block on lru chain */

    QUSRCTL  bt_qseqUser; /* if set, this is the owning seq reader usrctl */

/* MUST BE MULTIPLE OF 4 BYTES LONG */
}  bktbl_t;

/* bftbl is an array of structures, one for each buffer in the pool*/
/* it is the same for all the pools, the shared and non-shared */
/* this is going away soon */
#define BFTBL struct bftbl
typedef struct bftbl
{
    QBFTBL   qself;      /* self-referential shared pointer */
/** this structure size must be a multiply of 4 */
} bftbl_t;

#define BKDEBUG     1	/* do extra sanity checks */

/*****************************************************************/
/**								**/
/**	The following - describes information managed by the	**/
/**	physical block manager.					**/
/**								**/
/*****************************************************************/

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

typedef struct bkchain
{
    QBKTBL  bk_qhead;   /* head of chain */
    QBKTBL  bk_qtail;   /* tail of chain */
    ULONG   bk_nent;    /* number of blocks on chain */
    COUNT   bk_lrulock; /* which lock do we use for this chain */
} bkchain_t;

/* other chain anchors */

typedef struct bkanchor
{
    QBKTBL  bk_qHead;		/* pointer to head of circular chain */
    ULONG   bk_numEntries;	/* number of blocks on chain */
    COUNT   bk_linkNumber;	/* Which link entry in bktbl do we use */
    COUNT   bk_lockNumber;	/* Latch number to lock this chain */
} bkanchor_t;

#define BKANCHOR struct bkanchor

/* Data generated at checkpoints */

typedef struct bkcpdata
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
} bkcpdata_t;

#define BKCPDATA struct bkcpdata

#define BK_NUM_CPSLOTS 8	/* number of checkpoints to keep data for */
#define BK_CPSLOT_MASK 0x7	/* mask for computing array index */

typedef struct bkctl
{
    QBKAREADP bk_qbkAreaDescPtr; /* pointer to the array of area descriptors */

    QBKFSYS bk_qfsys;	/* chain of file system blocks for raw i/o */
    QBKTBL  bk_qbktbl;  /* cahin through all bktbl entries */

    LONG    bk_numbuf;	/* total number of database buffers */
    LONG    bk_numsbuf; /* number of secondary database buffers */
    LONG    bk_numibuf; /* number of database index buffers */
    LONG    bk_numgbuf; /* number of general purpose database buffers */
    LONG    bk_inuse;   /* number of buffers with blocks in them */

    LONG    bk_otime;	/* time the .db was opened */
    int     bk_dbfile;	/* fd for structure file, if multi file db,
			   else fd for db */

    GBOOL    bk_olbackup;/* online backup is active */
    DBKEY   bk_bunext;	/* obsolete */
    DBKEY   bk_bulast;	/* obsolete */
    ULONG   bk_buarea;  /* obsolete */
    UCOUNT  bk_buinc;	/* obsolete */
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

/* buffer pool hash function - used in bkbuf.c and nsamon.c */

#define BKHASHDBKEY(d, area, recbits)\
   (((DBKEY)((d) >> (COUNT)(recbits)) + (area) ) % (DBKEY)(pbkctl->bk_hash))

/* handy macro to access hash chain */

#define bkqhash(pdbcontext) ((pdbcontext)->pbkctl->bk_qhash)

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
    "bmctl.h must be included via bmpub.h."
#endif  /* #if defined(BMBUM_H) */

#endif /* #ifndef BMCTL_H */
