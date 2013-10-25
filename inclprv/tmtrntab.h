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

#ifndef TMTRNTAB_H
#define TMTRNTAB_H

#include "rlpub.h"         /* For definition of RLNOTE      */

/* Transaction states - future use */
#define TM_TS_DEAD       0 /* slot is available - must be equal to 0 */
#define TM_TS_ALLOCATED	 1 /* slot is allocated for trans about to start */
#define	TM_TS_ACTIVE     2 /* normal processing */
#define	TM_TS_PREPARING  3 /* about to enter phase 1 */
#define	TM_TS_PREPARED   4 /* now in phase 1 - ready to commit */
#define	TM_TS_COMMITTING 5 /* now in phase 2 */

/* Transaction flags */

#define TM_TF_UNDO       1 /* rolling back */

/* the next 4 are for future use */

#define	TM_TF_2PC        2  /* 2phase transaction */
#define TM_TF_INDOUBT    4 /* ready to commit and failed */
#define TM_TF_COORD      8 /* we are the local coordinator for this trans */
#define TM_TF_FORCE      16 /* manual override - roll back in-doubt trans */

/* Per transaction info */
/* txPlus is not written as part of the checkpoint note   */
typedef struct txPlus
{
    QXID        qXAtran;        /* Pointer to global transaction  */
    ULONG64     lockChains;     /* bit mask of lock chains containing
                                   locks for this tx              */
}txPlus_t;

typedef struct tx
{
    TEXT	txstate;	/* transaction state */
    TEXT	txflags;	/* transaction flags */
    psc_user_number_t	usrnum;		/* user number of transaction */
    LONG	transnum;	/* local transaction id */
    LONG	rlcounter;	/* rl counter when it started */
    LONG	txtime;		/* time transaction started */
    txPlus_t    txP;
} TX;

/* Global transaction info - allocate one of these at startup large enuf to
   hold the larger of:
     1) the current max number of concurrent transactions or
     2) the max in the recovery log.
  Calculate so: sizeof(TRANTAB) + (max_transactions-1)*sizeof(TX)
*/
TRANTAB
{
  UCOUNT maxtrn;
  UCOUNT numlive;
  LONG  trmask; /* determine how many right most bits of transnum are used
                   for the transaction table
                */
  TX    trn[1];
};

/* while we're here let's def a macro for this */
#define TRNSIZE(x) (sizeof(TRANTAB)+((x)-1)*sizeof(TX))

/* A table for additional info for 2phase ready to commit txs */
typedef struct txrc
{
    LONG    trnum;                 /* Loca transaction # */
    LONG    crdtrnum;              /* Coordiantor's transaction # */
    TEXT    crdnam[MAXNICKNM + 1]; /* Coordiantor's name */
} TXRC;

typedef struct trantabrc
{
    COUNT numrc;
    TXRC  trnrc[1];
}TRANTABRC;

#define TRNRCSIZE(x) (sizeof(TRANTABRC)+((x)-1)*sizeof(TXRC))

#endif /* #ifndef TMTRNTAB_H */
