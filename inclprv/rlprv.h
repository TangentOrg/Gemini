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

#ifndef RLPRV_H
#define RLPRV_H

/**********************************************************************
*
*	C Header:		rlprv.h
*	Subsystem:		1
*	Description:	
*	%created_by:	richt %
*	%date_created:	Tue Dec 13 12:16:23 1994 %
*
**********************************************************************/

#include "shmpub.h"         /* For definition of q pointers   */
#include "rltlpub.h"        /* For definition of rltlCounter_t */

#ifndef RLDISP
#define RLDISP 0
#endif

/* Biggest note we can read during logical undo (rlrej) or
   during recovery. The biggest notes are the ones generated
   when the transaction table is written out at the beginning
   of a cluster, determined by the number of currently active
   transactions. */

#if SHMEMORY
#define MAXRLNOTE 30720	/* large, deluxe size */
#else
#define MAXRLNOTE 8192	/* small, memory saving size */
#endif

#include "bmpub.h"  /* needed for BKTBL */
#include "rlpub.h"

/* return codes from rlcltry */
#define RLTHRESHOLD    -1
#define RLDIE	        1
#define RLRETRY	        2

/* time values for rlTestAdjust */
#define RL_INVALID_TIME_VALUE  -1
#define RL_NO_TIME_VALUE       -2 

/* the rl cursor for rlrdnxt and rlrdprv */
#define RLCURS struct rlcurs
struct rlcurs {
        LONG    noteBlock;      /* block number containing curr note    */ 
        LONG    noteOffset;     /* offset within block of curr note     */
        LONG    nextBlock;      /* block number containing next note    */ 
        LONG    nextOffset;     /* offset within block of next note     */
	TEXT	*passybuf;	/* put note here if it spans .bi block	*/
        LONG    rlnextCluster;  /* rlrdnxt only: block of next cluster  */
	LONG	rlcounter;	/* rlrdnxt only: rlctr of next cluster	*/
	COUNT	notelen;	/* length of the rlnote + variable data	*/
	COUNT	status;		/* after each call, 0 or EOF		*/
	TEXT	buftype;	/* where is the note, see below		*/
	UCOUNT	asbuflen;	/* length of assembly buffer            */
	};

/* values for buftype */
#define RLBLKBUF	1
#define RLASSYBUF	2

/* time interval to wait after a sync() before it is safe */
#define RLFIXAGE 60

/* .bi file block declarations */

#define RLCLHDRLEN (sizeof(RLBLKHDR) + sizeof(RLCLHDR))

/* every .bi block has this header, should be multiple of 4 bytes */
#define RLBLKHDR struct rlblkhdr
struct rlblkhdr {
	LONG	rlcounter;	/* sequentially assigned counter	*/
	COUNT	rlhdrlen;	/* size of header of this block		*/
	COUNT	rlused;		/* bytes used in block including header	*/
	LONG	rlblknum;       /* block number for sanity checks */
        LONG    checkSum;       /* checksum for detecting non-atomic
				   bi block writes.                     */
        LONG    spare1;         /* For future use guaranteed to be zero */
	};

/* the first block of each cluster has this following the block header	*/
/* it should always be a multiple of 4 bytes long			*/
#define RLCLHDR	struct rlclhdr
struct rlclhdr {
	LONG	rlnextBlock;	/* lseek block of next clstr in ring	*/
	LONG	rlprevBlock;	/* lseek block of prev clstr in ring	*/
	LONG	rlclose;	/* time this cluster was closed		*/
        LONG    rlrightBlock;   /* lseek block of rightmost cluster     */
                                /* which contains PUNDO notes pointing  */
                                /* at me                                */
	LONG	rlrghtctr;	/* rlcounter of rightmost cluster which
				   contains PUNDO notes pointing at me	*/
	LONG	rlbiopen;	/* last time .bi was opened, must match
				   mstrblk->biopen.  Only in 1st cluster*/
        LONG	rlmaxtrn;	/* size of the live transaction table	*/
	LONG	rlclsize;	/* Size of clusters in this .bi file	*/
				/* NB: done like this for compatibility	*/
				/* with v5:				*/
				/*	n = 16384: 16384 bytes		*/
				/*	n < 16384: n * 16384 bytes	*/

      /* The next two items are needed to keep track of physical backout
         in case we crash *during* physical backout. Then we can resume
         it where we left off rather than undoing the partial backout
         and then redoing the whole thing. Note that this affects the
         cluster reuse logic (see rlcl.c) */

      LONG    rlpbkbBlock;    /* lseek block of last physical backout  
                                 begin note seen during warm start. 0 if
                                 none seen or if backout completed.   */
      LONG    rlpbkbOffset;   /* lseek offset portion of last physical 
                                 backout begin note address.          */
      LONG    rlpbkcnt;       /* number of operations undone after the
                                 last physical backout began. zero if
                                 the physical backout was completed. */
      LONG    rlSyncCP;       /* flag to determine if this cluster was opened
                                 by a synchronous checkpoint */
	};

#define RLBLK struct rlblk
struct rlblk {
	RLBLKHDR  rlblkhdr;	/* all rl blocks have this		*/
	RLCLHDR   rlclhdr;	/* only cluster header blocks use this	*/
	TEXT	  rlblkdata[1]; /* place holder for data -
				   actual size = BlockSize - RLCLHDRLEN
				*/
	};

/* before image buffer table entry */
#define RLBF struct rlbftbl

/* values for bufstate */

#define RLBSLOOSE	0
#define RLBSFREE	1
#define RLBSDIRTY	2
#define RLBSIN		3
#define RLBSOUT		4

/* one of these for each bi buffer. There must be at least 3 bi buffers
   because the code assumes that the pointer to the current bi buffer
   and the input buffer is always valid */

struct rlbftbl
{
    COUNT	bufstate;/* loose, free, dirty, out, in */
    QRLBF	qlink;   /* chain thru all bi buffers */
    LONG64	dpend;	 /* bi dependency counter for this bi block */
    LONG64	sdpend;  /* starting dependency counter for this block */
    QRLBF	qnext;	 /* next (higher) dependency */
    QRLBF	qprev;	 /* previous (lower) dependency */
    BKTBL	rlbktbl; /* bktbl for block */
    BFTBL	rlbftbl; /* bftbl needed by bkread/bkwrite for buffer */
    QRLBF	qself;
};

/* txe lock structure        */
typedef struct txeLock
{
    LONG   theLock;          /* >0 means locked                      */
    LONG   spare;
    LONG   totalConcurrentLocks;
    LONG   currentInQ;
    LONG   totalCurrentInQ;
}txeLock_t;

/* central control structure */
struct rlctl {
	/* describe the current open cluster. Not valid if no opn clstr	*/
	LONG	rlcurr;    /* lseek block of current cluster header	*/
	LONG	rlclused;  /* bytes used in current cluster less overhd */
        LONG    rlblOffset; /* current offset in current rl block       */
        LONG    rlblNumber; /* lseek block number of current output block */
        LONG    rlnxtBlock; /* contains the next note to be written     */
        LONG    rlnxtOffset; /* next note will be written at this loc   */
	/* other important cluster control info */
	LONG	rlcounter; /* largest rlcounter in the .bi file		*/
	LONG	rllastBlock;  /* lseek Block of last note written	*/
	LONG	rllastOffset; /* lseek offset of last note written	*/
	LONG	rlsize;    /* size of the .bi file			*/
	LONG	rltype1;   /* points to leftmost cluster in the file	*/
	LONG	rlcap;	   /* capacity of 1 cluster less header overhead*/
	LONG	rlclbytes; /* cluster size in bytes			*/
	/* blocksize information derived from master block              */
	LONG	rlbiblksize; /* block size in bytes                     */

	/* warm restart information */
	GTBOOL	rlwarmstrt;/* ON if warmstart is in progress		*/
        COUNT	rlmaxtrn;  /* size of the live transaction table	*/
	LONG	rltype2;   /* rightmost clstr when warm restart began	*/
	LONG	rltype3;   /* leftmost clstr created during warm restart*/
	LONG	rlTestAdjust; /* variable to adjust time used for tests */
	LONG	rlcrnctr;  /* Obsolete */
	LONG	rlcrntaddr;/* Obsolete */
	LONG	rlcrntctr; /* Obsolete */
	LONG	rlpbkout;  /* lseek address of current physical backout -
			      0 - if not doing backout			*/
        LONG    rlRedoBlock; /* block after last note processed during redo */
        LONG    rlRedoOffset;/* offset after within previous block */
	LONG	rlbiblklog; /* log2 of blocksize for efficient manipulation */

	/* dependency control info to make .bi writes happen before .db */
	LONG64   rldepend;  /* incremented each time a .db is modified*/
	LONG64   rlwritten; /* copied from rldepend each time .bi written*/

	/* cluster timing control */
	LONG	rlsystime; /* current system clock time			*/
	LONG	rlbasetime;/* systime-basetime=run time since creat(.rl)*/
	LONG	rlclstime; /* run time when last cluster was closed	*/

	/* misc */
	GTBOOL	rlopen;    /* ON if rl manager has been initialized	*/
	UCOUNT	rlmtflag;  /* RL_MTBGN if rlmtbgn was called		*/

        /* input and output buffers */
        QBKTBL  qintbl;   /* always points at the input bktbl           */
        QBKTBL  qouttbl;  /* always points at the output bktbl          */
        QRLBLK  qoutblk;  /* always points at the output buffer */
        BKTBL   rltbl[2]; /* 2 bktbls, 1 for input, 1 for output        */
        BFTBL   rlbftbl[2]; /* 2 bftbls, 1 for input, 1 for output      */
 
        LONG     rlpunctr;      /* ctr of clstr hdr containing last PUNDO*/

	/* input and output buffers */

	COUNT	numBufs;  /* number of buffers */
	LONG	rlnxtblk; /* likely byte address for next buffer */
        QRLBF	qpool;    /* chain thru all buffers qlink pointers */
	QRLBF	qfree;    /* unused buffer chain */
	QRLBF	qoutbf;   /* points to output rlbuff */
	QRLBF	qinbf;    /* points to input rlbuff */
	QRLBLK  qinblk;   /* points to input buffer	*/
	COUNT	numDirty; /* number of dirty buffers */
	QRLBF	qmodhead; /* oldest modified buffer */
	QRLBF	qmodtail; /* newest modified buffer */

        /* buffer read wait queue */

        QUSRCTL	qrdhead;  /* first in read queue */
	QUSRCTL qrdtail;  /* last in read queue */

        /* buffer write wait queue */

	LONG	numWaiters; /* number of processes in wait queue */
	QUSRCTL qwrhead;  /* first in write queue */
	QUSRCTL qwrtail;  /* last in write queue */

	int	 abiwpid;	/* pid of async. bi writer */
        ULONG    spare1;
        ULONG    spare2;
        QUSRCTL  qtxehead;  /* users waiting for tx end lock */
        QUSRCTL  qtxetail;
        LONG     txeMaxUpdateLocks; /* Max concurrent update locks on txe  */
        LONG     txeMaxCommitLocks; /* Max concurrent commit locks on txe  */
        txeLock_t txeLocks[RL_TXE_MAX_LOCK_TYPES + 1];
        LONG     txeStop;   /* stop granting any txe locks.                */
        ULONG    spare3;
        LONG     txeLock;   /* tx end lock: 0 = free, -1 = excl, else share */
    /*  Every time a txe Update lock is granted the total locks currently
        granted are added to this variable.                                 */
        LONG     txeConcurrentUpdateLocks;
        LONG     txeUpdateLocksInQ;
    /*  Every time a txe Update lock is queued the total locks currently
        queued are added to this variable.                                  */
        LONG     txeTotalUpdateLocksInQ;
    /*  Every time a txe Commit lock is granted the total locks currently
        granted are added to this variable.                                 */
        LONG     txeConcurrentCommitLocks;
        LONG     txeCommitLocksInQ;
    /*  Every time a txe Commit lock is queued the total locks currently
        queued are added to this variable.                                  */
        LONG     txeTotalCommitLocksInQ;
        LONG     txeRequestsByOp[RL_NUMBER_OF_OPS];
        LONG     txeQuedByOp[RL_NUMBER_OF_OPS];
        LONG     txeHoldingTimeByOp[RL_NUMBER_OF_OPS];
        LONG     txeLockTimeByOp[RL_NUMBER_OF_OPS];

        QUSRCTL  qbiwq;     /* biw writer wait queue */
        LONG     rlpbkbBlock; /* block number of last physical backout begin 
				seen during physical redo phase of warmstart */
        LONG     rlpbkbOffset; /* offset within last physical backout block */
	LONG     rlpbnundo; /* number of notes undone after physical backout
				began. 0 if physical backout ended */
	/* TODO: disable/remove before checkin */
	LONG     rlbiblkmask; /* block mask for bi blocksize */
        LONG     rlClockError; /* seconds of rl clock error */
	};

/* Recovery notes for the rl subsystem           */
/* note to save memory table contents */
#if 0
typedef struct memnote {
    RLNOTE	rlnote;
    COUNT	tmsize;
    LONG	rlpbkout;
    LONG	lasttask;
    LONG        aiwrtloc;
    ULONG       aiupdctr;
    LONG        ainew;
} MEMNOTE;
#endif

typedef struct memnote {
    RLNOTE	rlnote;
    /* The next two fields are one byte quantities so that they can
       be referenced without aligning the note.                         */
    TTINY       numMemnotes;     /* Number of notes needed to record the
				    tx table.                            */
    TTINY       noteSequence;    /* Sequence number of this note 
				    1..numMemnotes                      */
    COUNT	tmsize;
    LONG        rlpbkout;   
    LONG	lasttask;
    UCOUNT      numLive;          /* Number of active tx's at checkpoint */
    UCOUNT      numThisNote;      /* Number of tx entries in this note.  */
    LONG        aiwrtloc;
    ULONG       aiupdctr;
    LONG        ainew;
    ULONG       tlWriteBlockNumber; /* Block number of current write postion
                                       in the tl area.                 */
    ULONG       tlWriteOffset;      /* Offset within the block         */
    ULONG       spare[2];           /* un-used guaranteed to be zero   */
} MEMNOTE;

/* note to save ready to commit tx table */
typedef struct rctabnote
{
    RLNOTE     rlnote;
    TTINY      numRcNotes;      /* Number of notes needed to record the
                                   ready to commit table.                   */
    TTINY      noteSequence;    /* Sequence number of this note
                                    1..numMemnotes                          */
    COUNT      numrc;           /* Number of ready to commit txs            */
    UCOUNT     numThisNote;     /* Number of rctx entries in this note.     */
} RCTABNOTE;

/* note used when ai extent switch occurs */
typedef struct aiextnote
{
    RLNOTE     rlnote;
    LONG       rlpbkout;   
    LONG       lasttask;
    ULONG      aiupdctr;
    COUNT      numlive;          /* Number of live transactions */
    COUNT      numrc;            /* Number of ready to commit txs */
    TEXT       extent;
    LONG       ainew;            /* Time of extent switch       */
}AIEXTNOTE;




struct dsmContext;

/* We are in the middle of writing the checkpoint note. This value
   gets put into the rlcounter of the cluster header.  Once all the 
   checkpoint notes are written out, the rlcounter value will be
   updated with the correct value.  During recovery, if a cluster
   is found in this state, the checkpoint notes will be rewritten
   and this cluster will be called the right most cluster. */
#define RL_CHECKPOINT -1

/* Private entry point declares */

int      rlAgeLimit		(struct dsmContext *pcontext);

DSMVOID rlaiclean			(struct dsmContext *pcontext);
		
DSMVOID rlaicls			(struct dsmContext *pcontext);
	      
gem_off_t rlaielen			(struct dsmContext *pcontext, ULONG area);
	       
DSMVOID rlaieof			(struct dsmContext *pcontext);
	      
DSMVOID rlaiextwt			(struct dsmContext *pcontext, int redo);

DSMVOID rlaiflsx			(struct dsmContext *pcontext);

ULONG rlaiGetNextArea		(struct dsmContext *pcontext, ULONG areaSlot);

LONG  rlaiGetLastArea		(struct dsmContext *pcontext, ULONG *pareaSlot);

int rlaimv			(struct dsmContext *pcontext, TEXT *pdata,
				 COUNT len, LONG  updctr, LONG64  bidpend);
	     
LONG rlaiNumAreas		(struct dsmContext *pcontext);

int rlainote			(struct dsmContext *pcontext, RLNOTE *prlnote);
	     
int rlaiopn			(struct dsmContext *pcontext, int aimode,
				 TEXT *pname, int execmode, int tmplog,
				 int retry);
	      
DSMVOID rlaiseek			(struct dsmContext *pcontext, LONG aipos);
	       
DSMVOID rlaisete			(struct dsmContext *pcontext);
	       
DSMVOID rlaiseto			(struct dsmContext *pcontext);
	       
/* rlaiutil.c */
/* BUM - change name of domark to rlaiDoMark */
/* PROGRAM: domark - mark the db as being backed up */
DSMVOID domark 	     ( struct dsmContext *pcontext );
	     
int rlaifull (struct dsmContext *pcontext, ULONG *pextnum);  

LONG rlaiestate (struct dsmContext *pcontext, ULONG  area );

int rlaiempty (struct dsmContext *pcontext, ULONG area );
		
ULONG rlaienum  (struct dsmContext *pcontext, int  *pExtentNumber, 
                 TEXT *pextpath );
	       
int rlailist ( struct dsmContext *pcontext );
	       
int rlaisingle ( TEXT *pname);  

DSMVOID rlainew   (struct dsmContext *pcontext);

DSMVOID rlaitrnc(struct dsmContext *pcontext, int waittime, int newblocksize);

int rlDetermineAiBlockSize(dsmContext_t *pcontext, int *pnewblocksize);

int rlairf(struct dsmContext *pcontext, int vbmode, int execmode, 
           ULONG endTime, ULONG endTx, int reTry, TEXT *pname );

DSMVOID rlaioff ( struct dsmContext *pcontext);
	      
DSMVOID rlaion( struct dsmContext *pcontext);

int rlaiUpdateAreaBlockSize(struct dsmContext *pcontext, ULONG areaNumber,
                            int blockSize);

int rlaiUpdateAreaBlockSizes(dsmContext_t *pcontext, int blockSize);

/* end rlaiutil.c */
	     
		
DSMVOID rlaiwait			(struct dsmContext *pcontext, DBKEY aidbkey);

DSMVOID rlaiwrt			(struct dsmContext *pcontext, RLNOTE *pnote,
				 COUNT  len, TEXT *pdata, LONG updctr,
				 LONG64 bidpend);
	      
DSMVOID rlaixtn			(struct dsmContext *pcontext);
	      
RLBLK * rlbird			(struct dsmContext *pcontext, LONG rlblock); 

DSMVOID rlbiout			(struct dsmContext *pcontext, RLBLK *prlblk); 

DSMVOID rlbiwait			(struct dsmContext *pcontext, DBKEY bidbkey,
				 int dontWait);

DSMVOID rlbiwrt			(struct dsmContext *pcontext, RLBLK *prlblk);

DSMVOID rlclcls			(struct dsmContext *pcontext, int reclose);

DSMVOID rlclins			(struct dsmContext *pcontext, LONG clusterBlock,
				 LONG nextCluster);

DSMVOID rlclopn			(struct dsmContext *pcontext, 
                                 LONG clusterHeader,
				 LONG syncCP);

DSMVOID rlclnxt			(struct dsmContext *pcontext, int syncCp);

LONG rlclxtn			(struct dsmContext *pcontext, LONG clusterBlock,
				 int  numToExtend, LONG *pnewCluster);

DSMVOID rlflsh			(struct dsmContext *pcontext, int closedb );

DSMVOID rlinitp			(struct dsmContext *pcontext, RLCURS *prlcurs,
				 LONG noteBlock, LONG noteOffset, 
                                 TEXT *passybuf);

int rlphase4			(struct dsmContext *pcontext, RLCURS *prlcurs);

int rllbk			(struct dsmContext *pcontext, RLCURS *prlcurs);

DSMVOID rlmemwt			(struct dsmContext *pcontext);

RLNOTE *rlrdprv			(struct dsmContext *pcontext, RLCURS *prlcurs);

DSMVOID rlundo			(struct dsmContext *pcontext, RLNOTE *prlnote,
				 COUNT dlen, TEXT *pdata);

LONG rlwrite			(struct dsmContext *pcontext, RLNOTE *prlnote,
				 COUNT dlen, TEXT *pdata);

int rlrollf			(struct dsmContext *pcontext, TEXT *assybuf);

#endif  /* RLPRV_H */

