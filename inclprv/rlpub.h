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

#ifndef RLPUB_H
#define RLPUB_H

#include "bmpub.h"       /* For definition of bmBufHandle_t  */
#include "dsmdtype.h"    /* typedefs for dsm api */

struct dsmContext;
struct mstrblk;

#define  RL_FLUSH_ALL_BI_BUFFERS   0x7fffffffffffffff

/**********************************************************************/
/***  rlpub.h - #include file for callers of the rl facility        ***/
/**********************************************************************/

struct rlnote {
		TEXT	rllen;	  /* caller: length excluding data area	*/
		TEXT	rlcode;   /* caller: note code			*/

	/*
		WARNING: Access the following fields only with sct()-xct() and 
		    slng()-xlng() for COUNT and LONG respectively. NEVER
		    access directly since correct boundary is not guaranteed.
		ALSO:  After psc_user_number_t is changed to 32 bits,
		    rlTxSlot should switch places with rlflgtrn for better
		    structure packing.
	*/
		COUNT    rlflgtrn; /* All 16 bits available since rlTxSlot
				      field was added to the note header */
		LONG    rlTrid;   /* transaction id                     */
                ULONG   rlArea;   /* storage containing the block.      */
		DBKEY	rldbk;    /* rl: dbkey of db block being chgd	*/
		LONG64	rlupdctr; /* rl: update counter of that block	*/
	      };

/* values for rlFlags  may be OR'd together almost arbitrarily */
#define RL_LOGBI	 0x0001 /* This is a logical change note        */
#define RL_PHY		 0x0002	/* This is a physical change note	*/
#define RL_UNPHY         0x0004 /* This is a undo a physical change note*/

#define RLNOTE struct rlnote

#define INITNOTE(n,c,f) (n).rlnote.rllen = sizeof(n);\
           (n).rlnote.rlcode = c;\
           (n).rlnote.rlArea = 0;\
           sct( (TEXT *)&((n).rlnote.rlflgtrn), (COUNT)f); \
           (n).rlnote.rlTrid = 0;

struct bkbuf;

#define RL_DO_UNDO_PROTO \
        struct dsmContext *pcontext, RLNOTE *pnote, \
        struct bkbuf *pbkbuf, COUNT len, TEXT *pdata


/* constants for transaction begin and transaction end lock requests */
#define RL_TXE_SHARE    1
#define RL_TXE_UPDATE   2
#define RL_TXE_COMMIT   3
#define RL_TXE_EXCL     4
#define RL_TXE_MAX_LOCK_TYPES 4

/* Logical database operation types -- for statistics gathering on
   the TXE lock.                                                      */
#define RL_RECORD_CREATE_OP 0
#define RL_RECORD_UPDATE_OP 1
#define RL_RECORD_DELETE_OP 2
#define RL_KEY_ADD_OP       3
#define RL_KEY_DELETE_OP    4
#define RL_SEQ_UPDATE_OP    5
#define RL_MISC_OP          6
#define RL_NUMBER_OF_OPS    7   /* IF YOU ADD AN OP BE SURE TO INCREASE
                                   THIS VALUE.   */

/* #define the possibly physical note codes */
#define RL_NULL     0              /* no-op */
/* Index notes */
#define RL_IXMSTR   1              /* new index master */
#define RL_IXINS    2              /* insert index entry */
#define RL_IXREM    3              /* remove index entry */
#define RL_IXSP1    4              /* 1st note of index block split */
#define RL_IXSP2    5              /* 2nd note of index block split */
#define RL_IXDBLK   6              /* delete entire index block */
#define RL_IXIBLK   7              /* install entire block */

/* bk notes */
#define RL_BKREPL   8              /* generic string replace */
#define RL_BKFRM    9              /* master block changes for frrem */
#define RL_BKFRB   10              /* database block changes  for frrem*/
#define RL_BKFAM   11              /* master block changes for fradd */
#define RL_BKFAB   12              /* database block changes for fradd */
/* in memory table note */
#define RL_INMEM    13             /* compressed trantab */

/* transaction begin/end notes */
#define RL_TBGN     14             /* log+phy */
#define RL_TEND     15             /* log+phy */

/* backout notes */
#define RL_PUNDO    16             /* physical rollback note */
#define RL_PBKOUT   17             /* start phys. backout marker */
#define RL_PBKEND   18             /* completed phys. backout marker */
#define RL_LRA      19             /* Jump note for log. backout */

/* #define the purely logical note codes */
#define RL_IXGEN    20             /* logical index entry insertion */
#define RL_IXDEL    21             /* logical index entry deletion */
#define RL_IXFILC   22             /* create a new index file */
#define RL_IXKIL    23             /* remove an index file */
#define RL_IXUKL    24             /* unkill an index file */

/* notes for rm */
#define RL_RMCR	    25		   /* create a record (fragment) */
#define RL_RMDEL    26		   /* delete a record (fragment) */

/* jump note */
#define RL_JMP	    27		   /* link togthr notes in same xaction	*/

#define RL_XTDB     28		   /* physically extend the .db		*/
#define RL_FILC     29             /* add a fil to the database		*/
#define RL_LSTMOD   30		   /* change the mb_lstmod field	*/

#define RL_RMNXTF   32		   /* Insert next fragment pointer in rm */

/* notes related to 2nd (push) RMCHN to prevent clogging */
#define RL_BK2EM   33              /* master block top RMCHN to end chain */
#define RL_BK2EB   34              /* database blk top RMCHN to end chain */
#define RL_BKMBOT  35              /* master block add blk to bottom RMCHN*/
#define RL_BKBBOT  36              /* database blk add blk to bottom RMCHN*/
#define RL_RMCHG   37              /* change a record (fragement */

/* two-phase-commit notes */
#define RL_RCOMM   38              /* Ready to commit note. */
#define RL_TABRT   39              /* Abort transaction note. */
#define RL_CTEND   40              /* Coordinator's TEND      */
#define RL_RTEND   41              /* 2phase recovery TEND    */

/* Compressed index notes */
#define RL_CXINS    42              /* insert index entry */
#define RL_CXREM    43              /* remove index entry */
#define RL_CXSP1    44              /* 1st note of index block split */
#define RL_CXSP2    45              /* 2nd note of index block split */
#define RL_CXROOT   46              /* TEMPORARY to convert old root */

/* AI extent switch note   */
#define RL_AIEXT    47

/* Sequence number notes */

#define RL_SEASN    48	/* assign sequence no */
#define RL_SEINC    49	/* increment current value */
#define RL_SESET    50	/* set current value */
#define RL_SEADD    51	/* add sequence */
#define RL_SEDEL    52	/* delete sequence */
#define RL_SEUPD    53	/* update sequence */

/* Note for logging table of tx's in ready to commit state */

#define RL_RCTAB    54  /* Ready to commit tx table */

/* Note for allocating a new database block              */
#define RL_BKFORMAT 55
#define RL_BKMAKE   56
#define RL_BKHWM    57

/* Notes for storage areas and extents                */
#define RL_BKAREA_ADD           58
#define RL_BKEXTENT_CREATE      59  /* Create/write extent block */
#define RL_BKWRITE_AREA_BLOCK   60  /* write area block */
#define RL_BKWRITE_OBJECT_BLOCK 61  /* write object block */

/* Notes for save points within transactions */
#define RL_TMSAVE               62  /* log+phy */

/* Notes foro object chache modifications */
#define RL_OBJECT_CREATE        63  /* object was added to the om cache */
#define RL_OBJECT_UPDATE        64  /* and object was updated           */

/* More notes for storage areas and extents                */
#define RL_BKAREA_DELETE        65
#define RL_BKEXTENT_DELETE      66 

#define RL_IDXBLOCK_COPY        67
#define RL_IDXBLOCK_UNCOPY      68

#define RL_CXCOMPACT_LEFT       69  /* move entries to left blk */
#define RL_CXCOMPACT_RIGHT      70  /* adjust entries to left in right blk */

/* Note for creation of temporary indices for use by dbmgr */
#define RL_CXTEMPROOT           71  /* logical create a temp index file */

#define RL_TL_ON                72  /* enable 2pc transaction logging    */

#define RL_ROW_COUNT_UPDATE     73
#define RL_AUTO_INCREMENT       74 /* Note for auto increment column of a table */
#define RL_AUTO_INCREMENT_SET   75


#define AIDATES struct aidates
AIDATES {
	LONG	aibegin;	/* date AIMAGE BEGIN utility was run	*/
	LONG	ainew;		/* date AIMAGE BEGIN or NEW was run	*/
	LONG	aigennbr;	/* how many .ai files since last BEGIN	*/
	LONG	aiopen;		/* last date db and ai file were opened */
	};

/* For the time being the REPL note will be defined here */
struct bkrplnote {
  RLNOTE  rlnote;
  COUNT   offset;
  TEXT    data[2];   /* bug 20000222-023 by dwalz in v9.1b          */
                     /* Make the size of this field whatever will   */
                     /* fill it to a 32-bit boundary.  Otherwise    */
                     /* bkReplace will miscalculate the length      */
                     /* of the non-variable part of this note and   */
                     /* Purify will complain about unitialized      */
                     /* memory when a later routine thinks the      */
                     /* length of the note is longer than it really */
                     /* is.                                         */
};

#define BKRPNOTE struct bkrplnote
#define BKRPSIZE( n ) (sizeof(BKRPNOTE)+(n)-1)

/* This note can be used for any miscellaneous purpose */
typedef struct rlmisc {
    RLNOTE	rlnote;
    LONG        rlvalue;         /* used only for time values */
    LONG	rlvalueBlock; 
    LONG	rlvalueOffset;
} RLMISC;


/* Public entry points to rl sub-system */

DSMVOID rlaifls			(struct dsmContext *pcontext);
	
int rlaiqon			(struct dsmContext *pcontext);

int rlaiswitch			(struct dsmContext *pcontext, int command,
                                 int switchMsg);

int rlaoff (struct mstrblk *pmstr);

DSMVOID rlwakaiw			(struct dsmContext *pcontext);

DSMVOID rlbiclean			(struct dsmContext *pcontext);

DSMVOID rlbiflsh			(struct dsmContext *pcontext, LONG64 depend);

DSMVOID rlbitrnc			(struct dsmContext *pcontext, int waittime,
				 int newclustersize, int *pnewblocksize);

int rlDetermineBiBlockSize	(dsmContext_t *pcontext, int *pnewblocksize);


LONG rlclcurr			(struct dsmContext *pcontext);

int rlGetClFill			(struct dsmContext *pcontext);

LONG rlGetBlockNum		(struct dsmContext *pcontext,
				 struct bkbuf *pblk);

DSMVOID rlInitRLtime               (struct dsmContext *pcontext);
DSMVOID rlLoadRLtime               (dsmContext_t *pcontext);

ULONG rlGetMaxActiveTxs		(struct dsmContext *pcontext);

int   rlGetWarmStartState	(struct dsmContext *pcontext);

LONG64 rlgetwritten		(struct dsmContext *pcontext);

DSMVOID
rlTXElock(dsmContext_t *pcontext, LONG lockMode, LONG operationType);

DSMVOID     rlTXEunlock(dsmContext_t *pcontext);

DSMVOID     rllktxe		(struct dsmContext *pcontext,
                                 int lockType, int opType);

DSMVOID rlmtxbegin			(struct dsmContext *pcontext);

DSMVOID rlmtxend			(struct dsmContext *pcontext);

DSMVOID rlphynt			(struct dsmContext *pcontext, RLNOTE *pnote,
				 struct bkbuf *pblk, COUNT dlen, TEXT *pdata);

DSMVOID rlLogAndDo			(struct dsmContext *pcontext, RLNOTE *pnote,
				 bmBufHandle_t bufHandle, COUNT dlen,
				 TEXT *pdata);

DSMVOID rlPutBlockNum		(struct dsmContext *pcontext,
				 struct bkbuf *pblk, LONG blknum);

int  rlTxeStart                 (struct dsmContext *pcontext);

int  rlTxeStop                  (struct dsmContext *pcontext);

DSMVOID rlMTXstart                 (struct dsmContext *pcontext, int optype);

DSMVOID rlwakbiw			(struct dsmContext *pcontext);

DSMVOID rlrej			(struct dsmContext *pcontext, LONG transnum,
				 dsmTxnSave_t *psavepoint, int  force);

DSMVOID rlsetc			(struct dsmContext *pcontext);

DSMVOID rlsetfl			(struct dsmContext *pcontext);

int rltoff			(struct mstrblk *pmstr);

int rlwtdpend			(struct dsmContext *pcontext, LONG64 dpend,
				 ULONG aiNoteCounter);

DSMVOID rlultxe			(struct dsmContext *pcontext);

DSMVOID rlWriteTxBegin		(struct dsmContext *pcontext);

DSMVOID rlSynchronousCP		(struct dsmContext *pcontext);

/*******************************************/
/* The following functions are for rlset.c */
/*******************************************/

int rlseto			(struct dsmContext *pcontext, int mode);

RLCTL * rlInit			(struct dsmContext *pcontext,
				 COUNT clusterSize, COUNT biBlockSize,
				 ULONG biArea);

dsmStatus_t
rlBackup(dsmContext_t *pcontext, dsmText_t *parchive);

dsmStatus_t rlBackupSetFlag(dsmContext_t *pcontext,
                            dsmText_t *ptarget,
                            dsmBoolean_t backupStatus);

dsmStatus_t rlBackupGetFlag(dsmContext_t *pcontext,
                            dsmText_t *ptarget,
                            dsmBoolean_t *pbackupStatus);

#endif /* RLPUB_H   */
