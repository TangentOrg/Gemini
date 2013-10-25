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

#ifndef BKPRV_H
#define BKPRV_H

/***********************************************************/
/* bkprv.h - Private function prototypes for BK subsystem. */
/***********************************************************/

/*** BUM CONSTANT DEFINITIONS (from syscall.h - for compilation purposes) ***/
/* The following definitions were extracted directly from syscall.h */
/* This needs cleanup, desperately. */

/*********************************************************/
/* Private structure definitions for the bk subsystem    */
/*********************************************************/

#define BKBUMPRV_H
#include "bkprvfr.h"
#undef BKBUMPRV_H

struct bkftbl;
struct dsmContext;


/*********************************************************/
/* Private Function Prototypes for bkopen.c              */
/*********************************************************/

int	bkOpenOneFile	(struct dsmContext *pcontext,
			 struct bkftbl *pbkftbl, int openFlags);

/*********************************************************/
/* Private Function Prototypes for bksubs.c              */
/*********************************************************/

LONG	bkxtnfile	(struct dsmContext *pcontext,
			 struct bkftbl *pbkftbl, LONG blocksWanted);

int	bkAreaTruncate	(struct dsmContext *pcontext);

dsmStatus_t bkExtentCreateFile (dsmContext_t *pcontext, struct bkftbl *pExtent,
			 ULONG *pinitialSize, dsmExtent_t extentNumber,
                         dsmRecbits_t areaRecbits, ULONG  areaType);

/*------------------------------------------------------*/
/* Recovery log note definitions for block operations.	*/
/* Note used to extend the database.			*/
/*------------------------------------------------------*/

typedef struct xtdbnote {
	RLNOTE	rlnote;
	ULONG	newtot;
} XTDBNOTE;


/*--------------------------------------------------------------*/
/* Note used to format next block above the hi-water mark.	*/
/*--------------------------------------------------------------*/

typedef struct bkformatnote
{
	RLNOTE	rlnote;		/* Standard note header       */
	TEXT	newtype;	/* new block type             */
	TEXT	filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
	COUNT	filler2;	/* bug 20000114-034 by dmeyer in v9.1b */
} BKFORMATNOTE;

#define INIT_S_BKFORMATNOTE_FILLERS( bk ) (bk).filler1 = 0; (bk).filler2 = 0;


/*------------------------------------------------------*/
/* Note used to materialize a block in the buffer pool.	*/
/*------------------------------------------------------*/

typedef struct bkmakenote
{
	RLNOTE	rlnote;
	ULONG	area;
	DBKEY	dbkey;
} BKMAKENOTE;


/*----------------------------------------------*/
/* Note to advance the database hi water mark.	*/
/*----------------------------------------------*/

typedef struct bkhwmnote
{
	RLNOTE	rlnote;
	ULONG	newhwm;
} BKHWMNOTE;


/*---------------------------------------------------------*/
/* Note for initializing an area in the area cache.        */
/*---------------------------------------------------------*/

typedef struct bkAreaNote
{
	RLNOTE	rlnote;		/* Standard note header		*/
	ULONG	area;		/* Area being added/deleted	*/
	ULONG	blockSize;	/* Block size in bytes of area	*/
	ULONG	areaType;	/* Data, BI, AI ...		*/
	UCOUNT	areaRecbits;	/* number of recbits for area	*/
	COUNT	filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
} bkAreaNote_t;

#define INIT_S_BKAREANOTE_FILLERS( bk ) (bk).filler1 = 0;


/*----------------------------------------------*/
/* Note for initializing / creating an extent.	*/
/*----------------------------------------------*/

typedef struct bkExtentAddNote
{
	RLNOTE	rlnote;
	ULONG	area;		/* Area to add extent to	*/
	ULONG	initialSize;	/* initial size in blocks	*/
	ULONG	extentNumber;	/* which extent in the area	*/
	ULONG	areaType;	/* DSMAREA_TYPE_xx		*/
	GTBOOL	extentType;	/* BKFIXED | BKUNIX | BKPART	*/
	TEXT	filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
	COUNT	filler2;	/* bug 20000114-034 by dmeyer in v9.1b */
} bkExtentAddNote_t;

#define INIT_S_XTDADDNOTE_FILLERS( bk ) (bk).filler1 = 0; (bk).filler2 = 0;


typedef struct bkWriteAreaBlockNote
{
	RLNOTE   rlnote;
	ULONG    areaNumber;
} bkWriteAreaBlockNote_t;

typedef struct bkWriteObjectBlockNote
{
	RLNOTE	rlnote;
	ULONG	areaNumber;
	BLKCNT	totalBlocks;
	DBKEY	hiWaterBlock;
	TEXT	initBlock;
} bkWriteObjectBlockNote_t;


#endif /* ifndef BKPRV_H  */

