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

#ifndef SEQPRV_H
#define SEQPRV_H

#include "bkpub.h"  /* needed for BKHDR  */
#include "rlpub.h"  /* needed for RLNOTE */
#include "seqpub.h"

struct dsmContext;


#define SEQFILE  -67
#define IDXSEQ   1025   /* _sequence indexed by _Db-recid and _Seq-name */

/* static _sequence file fields */

#define SEQNAME   2      /* sequence name */
#define SEQNUM    3      /* sequence number */
#define SEQINIT   4      /* Initial value */
#define SEQINCR   5      /* increment */
#define SEQMIN    6      /* Minimum value - for seq num */
#define SEQMAX    7      /* Maximum value - for seq num */
#define SEQCYCOK  8      /* Cycle OK ? */
#define SEQMISC   9
#define SEQDBKEY  10
#define SEQUSERMISC 11   /* Misc for user use. Progress will not    (SQL92)
                          * use. Not really for SQL.
                          */


/*************************************************************/
/*** seqprv.h -- sequence number manager Private Interface ***/
/*************************************************************/

/* Recovery log note definitions                            */

/* note used to add sequences */
typedef struct seqAddNote
{
    RLNOTE	rlnote;
    COUNT	seqno;		/* which sequence number */
    LONG	oldval;		/* old current value */
    LONG	newval;		/* new (initial) value */
} seqAddNote_t;

/* note used to assign sequence numbers */
typedef struct seqAsnNote
{
    RLNOTE	rlnote;
    COUNT	seqno;		/* which sequence number */
    LONG	oldval;		/* old sequence value */
} seqAsnNote_t;

/* note used to delete sequences */
typedef struct seqDelNote
{
    RLNOTE	rlnote;
    COUNT	seqno;		/* which sequence number */
    DBKEY	recid;		/* recid of _Sequence record */
    LONG	oldval;		/* old value in case we undo add */
    LONG	initial;	/* initial value */
    LONG	min;		/* minimum value */
    LONG	max;		/* maximum value */
    LONG	increment;	/* increment value */
    TEXT	cycle;		/* 0 if not allowed to roll over */
    TEXT	name[MAXNAM+1];	/* sequence name */
} seqDelNote_t;

/* note used to update sequences */
typedef struct seqUpdNote
{
    RLNOTE	rlnote;
    COUNT	seqno;		    /* which sequence number */
    DBKEY	recid;		    /* recid of _Sequence record */
    		/* These are the old values	*/
    LONG	initial;	    /* initial value */
    LONG	min;	    	    /* minimum value */
    LONG	max;		    /* maximum value */
    LONG	increment;	    /* increment value */
    TEXT	cycle;		    /* 0 if not allowed to roll over */
    TEXT	name[MAXNAM+1];	    /* sequence name */
    		/*  These are the new values	*/
    LONG	new_initial;	    /* initial value */
    LONG	new_min;	    /* minimum value */
    LONG	new_max;	    /* maximum value */
    LONG	new_increment;	    /* increment value */
    TEXT	new_cycle;	    /* 0 if not allowed to roll over */
    TEXT	new_name[MAXNAM+1]; /* sequence name */
} seqUpdNote_t;

/* note used to set current sequence value */
typedef struct seqSetNote_t
{
    RLNOTE	rlnote;
    COUNT	seqno;		/* which sequence number */
    LONG	oldval;		/* */
    LONG	newval;		/* */
} seqSetNote_t;

/* note used to increment sequences - can not be undone */
typedef struct seqIncNote
{
    RLNOTE	rlnote;
    COUNT	seqno;		/* which sequence number */
    LONG	newval;		/* value after increment */
} seqIncNote_t;

/* sequence table entry */
typedef struct seqTbl
{
    COUNT     seq_nxhash;         /* index of next entry with same hash */
    COUNT     seq_vhash;          /* hash of name - here to speed lookups */
    DBKEY     seq_recid;          /* recid of _Sequence record */
    LONG      seq_initial;        /* initial value */
    LONG      seq_min;            /* minimum value */
    LONG      seq_max;            /* maximum value */
    LONG      seq_increment;      /* increment value */
    COUNT     seq_num;            /* which entry in sequence block */
    TEXT      seq_cycle;          /* 0 if not allowed to roll over */
    TEXT      seq_name[MAXNAM+1]; /* sequence name */
} seqTbl_t;

/* layout of sequence value database block */
typedef struct seqBlock
{
    BKHDR     bk_hdr;                 /* standard block header */
    LONG      seq_values[1];          /* sequence number values */
} seqBlock_t;

/* sequence manager control block */
typedef struct seqCtl
{
    COUNT     seq_nfree;             /* number of available entries */
    COUNT     seq_latch;             /* which latch used for locking */
    COUNT     seq_hash[SEQHASHP];    /* hash table */
    seqTbl_t  seq_setbl[1];          /* cached sequence entries */
} seqCtl_t;


/************************************************************/
/*** Sequence number manager Private Function Prototypes  ***/
/************************************************************/

DSMVOID	seqInsert	(struct dsmContext *pcontext,
			 seqCtl_t *pseqctl, seqTbl_t *pseqtbl);
UCOUNT	seqHashString	(struct dsmContext *pcontext, TEXT *pbuf,
			 int len, COUNT modulus);


#endif /* #ifndef SEQPRV_H */
