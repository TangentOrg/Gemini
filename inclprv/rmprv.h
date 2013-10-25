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

#ifndef RMPRV_H
#define RMPRV_H

/*----------------------------------------------*/
/* Bug 20000114-034				*/
/*						*/
/*	Filler were added to make it possible	*/
/*	to initialize structure space that was	*/
/*	previously being created by all the	*/
/*	compilers in order to satisfy the	*/
/*	required alignment.  The fillers	*/
/*	do not impact the size of the structs	*/
/*	or the placement of the data within	*/
/*	the structures, and should therefore	*/
/*	be transparent to existing notes on	*/
/*	disk.					*/
/*						*/
/*	The macros INIT_<type>_FILLERS are 	*/
/*	added so that the new fillers can	*/
/*	be initialized without the local code	*/
/*	having to know what they are.		*/
/*						*/
/*----------------------------------------------*/

struct dsmContext;

#include "bkpub.h"  /* needed for bkhdr in BLOCK typedef */
#include "rlpub.h"  /* needed for RLNOTE structures */
#include "bmpub.h"  /* needed for bmBufHandle_t  */

/********************************************************/
/*** Private  control structures for record manager   ***/
/********************************************************/

#define SPLITSAVE 25	/* space to reserve when splitting record */
#define MINRSIZE sizeof(DBKEY)	/* minimum record size */
#define SZBLOCK (sizeof(rmBlock_t) - sizeof(ULONG))

/* Maximum number of record per block must fit in  a TTINY */
#define RECSPERBLK(recbits)   ((COUNT)1 << (recbits))

/* interpretation of block.dir entries */
#define RMBLOB   0x40000000
#define SPLITREC 0x20000000 /* more fragments follow */
#define CONT     0x10000000 /* this is a continuation */
#define HOLD     0x08000000 /* dbkey deleted by a task */
#define RMOFFSET 0x0000ffff /* offset of record in block */

typedef struct rmBlock
{       BKHDR    rm_bkhdr;      /* block header */
        COUNT    numdir;        /* num alloc directory entries */
        COUNT    freedir;       /* num free directory entries */
        LONG     free;          /* free space */
        ULONG    dir[1];        /* directory entries */
} rmBlock_t;

/*------------------------------------------*/
/* Recovery log note for .bi and .ai files. */
/*------------------------------------------*/

typedef struct rmNextFragNote
{       RLNOTE	rlnote;
        DBKEY	nxtfrag;	/* dbkey of next fragment	*/
        DBKEY	prevval;	/* its previous value		*/
        COUNT	recnum;		/* Which record in the rm block	*/
	COUNT	filler2;	/* bug 20000114-034 by dmeyer in v9.1b */
} rmNextFragNote_t;

#define INIT_S_RMNXTFRG_FILLERS( rm ) (rm).filler2 = 0;


/*--------------------------------*/
/* Differences list for RMCHGNOT. */
/*--------------------------------*/

typedef struct rmDiffEntry
{       COUNT	oldOffset;	/* where in old record change begins */
        COUNT	newOffset;	/* where in new record change begins */
        COUNT	oldLen;		/* length of old data to be changed */
        COUNT	newLen;		/* length of new data to be changed */
} rmDiffEntry_t;


typedef struct rmDiffList
{       COUNT	numDifs;	/* number of difference entires to follow */
				/* diff entries go here */
} rmDiffList_t;


typedef struct rmCtl
{
        ULONG      area;          /* area block is in             */
        bmBufHandle_t bufHandle;  /* Handle to buffer in buffer pool */
        rmBlock_t *pbk;           /* ptr to block                 */
        TEXT      *prec;          /* ptr to record (fragment)     */
        DBKEY      holdtask;      /* task number holding dbkey    */
        DBKEY      nxtfrag;       /* dbkey of next record fragmnt */
        COUNT      recnum;        /* record entry in block        */
        ULONG      offset;        /* record offset                */
        LONG       recsize;       /* size of record (fragment)    */
        LONG       space;         /* space used in block          */
        ULONG      flags;         /* SPLITREC, CONT, and/or HOLD  */
        ULONG      cont;          /* this is continuation fragmnt */
} rmCtl_t;


/*----------------------------------------------*/
/* Recovery log note for .bi and .ai files.	*/
/*----------------------------------------------*/

typedef struct rmRecNote
{	RLNOTE	rlnote;
	ULONG	flags;		/* SPLITREC, CONT, and/or HOLD	*/
	COUNT	recnum;		/* 0 to RECSPERBLK		*/
	COUNT	recsz;		/* RL_RMCR notes: size of this plus all
					remaining fragments
				   RL_RMDEL notes size of this plus all
					preceeding fragments	*/
	dsmTable_t table;
	DBKEY	nxtfrag;	/* dbkey of next fragment	*/
} rmRecNote_t;

/*      FIXFIX: KLUGE KLUGE KLUGE
        NOTE: recnum and recsz have to be in the same place as in
        rmRecNote above. This is because rmdorem and rmdoins expect a
        rmRecNote to be passed to them.
*/
typedef struct rmChangeNote
{       RLNOTE      rlnote;
        ULONG       flags;
        COUNT       recnum;         /* Which record in the rm block */
        COUNT       recsz;          /* old record length */
        COUNT       newsz;          /* new rcord length */
        dsmTable_t  table;          /* table number or row             */
        LONG        difLen;         /* difference list total length */
                                    /* difference list goes here after */
} rmChangeNote_t;

/************************************************************/
/*** rmprv.h - record manager Private Function Prototypes ***/
/************************************************************/

int	rmDeleteRec (struct dsmContext *pcontext,dsmTable_t table,
                     ULONG area, DBKEY dbkey);

int     rmFitCheck (struct dsmContext *pcontext, rmBlock_t *pbk,
		    COUNT size, COUNT recnum);

int     rmFetchDir (struct dsmContext *pcontext, rmBlock_t *pbk, ULONG area);

int     rmLocate   (struct dsmContext *pcontext, ULONG area, DBKEY dbkey,
			 rmCtl_t *prmctl, int action);

DSMVOID    rmRCtl     (struct dsmContext *pcontext, rmBlock_t *pbk,
		    COUNT recnum, rmCtl_t *prmctl);

/* rm Virtual System Table private function prototypes */
DBKEY rmCreateVirtual (struct dsmContext *pcontext,
			COUNT table, TEXT *pRecord, COUNT size);
int   rmDeleteVirtual (struct dsmContext *pcontext,
			COUNT table, DBKEY dbkey);
int   rmFetchVirtual  (struct dsmContext *pcontext,
			COUNT table, DBKEY dbkey, TEXT *pRecord, COUNT size);
int   rmUpdateVirtual (struct dsmContext *pcontext,
			COUNT table, DBKEY dbkey, TEXT *pRecord, COUNT size);

#endif  /* ifndef RMPRV_H */
