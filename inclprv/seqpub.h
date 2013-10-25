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

#ifndef SEQPUB_H
#define SEQPUB_H

/************************************************************/
/*** seqpub.h -- sequence number manager Public Interface ***/
/************************************************************/

struct dbcontext;
struct dsmContext;

/* function codes */

#if 1
#define MAX_SEQ_1K 250
#else
/* The magic number 250 is obtained from  */
#define MAX_SEQ_1K  ((1024 - sizeof(BKHDR)) / sizeof(LONG) - 2 )
#endif

/* max no of sequences allowed */
#define MAXSEQVALS(dbblocksize)  (MAX_SEQ_1K * ((dbblocksize) / (UCOUNT)1024))

#define SEQCURVAL    1	/* get current value */
#define SEQNXTVAL    2	/* get next value */
#define SEQSETVAL    3	/* set current value */
#define SEQHASHP    97  /* prime number for hash table */
#define SEQINVALID  -1  /* Invalid Hash value */

#define	SEQADD       4	/* add new entry to cache */
#define	SEQDEL       5	/* delete entry from cache */
#define	SEQUPD       6  /* update entry in cache */

/***********************************************************/
/*** sequence number manager Public control structures  ***/
/***********************************************************/

/* Parameter block for proSeqCache() */
typedef struct seqParmBlock
{
    DBKEY     seq_recid;	  /* recid of _Sequence record */
    LONG      seq_initial;	  /* initial value */
    LONG      seq_min;		  /* minimum value */
    LONG      seq_max;		  /* maximum value */
    LONG      seq_increment;	  /* increment value */
    COUNT     seq_num;		  /* which entry in sequence block */
    TEXT      seq_cycle;	  /* 0 if not allowed to roll over */
    TEXT      seq_name[MAXNAM+1]; /* sequence name */
} seqParmBlock_t;

/*******************************************************************/
/*** sepub.h -- sequence number manager Public service entry pts ***/
/*******************************************************************/

int	seqAllocate	(struct dsmContext *pcontext);

int	seqAssign	(struct dsmContext *pcontext);

int	seqCurrentValue	(struct dsmContext *pcontext, TEXT *pseqname,
			 LONG *pcurval);

int	seqNumCurrentValue
			(struct dsmContext *pcontext, COUNT seqnum,
			 LONG *pcurval);

LONG	seqGetCtlSize	(struct dsmContext *pcontext);

int	seqInit		(struct dsmContext *pcontext);

int	seqNextValue	(struct dsmContext *pcontext, TEXT *pseqname,
			 LONG *pnextval);

int	seqNumNextValue	(struct dsmContext *pcontext, COUNT seqnum,
			 LONG *pnextval);

int	seqRefresh	(struct dsmContext *pcontext, int funCode,
			 seqParmBlock_t *pseq_parm);

int	seqSetValue	(struct dsmContext *pcontext, TEXT *pseqname,
			 LONG newval);

int	seqNumSetValue	(struct dsmContext *pcontext,
			 COUNT seqnum, LONG newval);

int     seqCreate       (struct dsmContext *pcontext,
			 seqParmBlock_t *pseqparms);

int     seqRemove       (struct dsmContext *pcontext, COUNT seqnum);

int     seqInfo         (struct dsmContext *pcontext, 
			 seqParmBlock_t *pseqparms);

#endif /* ifndef SEQPUB.H  */

