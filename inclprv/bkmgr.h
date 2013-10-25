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

#ifndef BKMGR_H
#define BKMGR_H


/*****************************************************************/
/**								**/
/**	bkmgr.h - describes structures and routines for		**/
/**	physical block management, including buffer pool	**/
/**	management. Refer to bkfr.h for other routines for	**/
/**	managing disk space.					**/
/**								**/
/*****************************************************************/


/*****************************************************************/
/**								**/
/**	bkhdr structure - forms the first sixteen bytes of	**/
/**	every physical block in a PROGRESS database.		**/
/**								**/
/**	bk_lastw is used in conjunction with master block	**/
/**	fields to provide support for incremental backup.	**/
/**								**/
/*****************************************************************/

#if !PSC_ANSI_COMPILER || defined(BKBUM_H)

#include "shmpub.h"

typedef struct bkhdr
{
		DBKEY	bk_dbkey;	/* dbkey of block		*/
		TEXT	bk_type;	/* owner of the block		*/
		TEXT	bk_frchn;	/* identifies which chain if	*/
					/* block is free, see bkfr.h	*/
		UCOUNT	bk_incr;	/* for incremental backup	*/
                LONG    bk_checkSum;    /*                              */
		DBKEY	bk_nextf;	/* next block on free chain	*/
		LONG64	bk_updctr;	/* date/time of last write	*/
} bkhdr_t;
#define BKHDR struct bkhdr

/* values for bk_type, indicate current use of block: */
#define MASTERBLK 1	/* block is the master block */
#define IDXBLK	  2	/* block is an index block */
#define RMBLK	  3	/* block is a data block */
#define FREEBLK   4	/* block is free */
#define IXANCHORBLK 5   /* block is an index dbkey list */
#define SEQBLK    6     /* block is a set of sequence generator values */
#define EMPTYBLK  7     /* block above the database high water mark    */
/* New blocks as of v8 for ASA */
#define PARMBLK   8     /* database parameter block  */
#define AREABLK   9    /* area block  */
#define OBJDIRBLK 10    /* object directory block */
#define EXTLSTBL  11    /* extent list block    */
#define OBJECTBLK 12    /* Object block         */
#define CONTROLBLK 13   /* Control block         */

#define EXTHDRBLK 254   /* Extent header block       */
#define RESBLK    255   /* Reserved               */

/* this is the block overlay passed around in bkrec */
typedef struct bkbuf
{
	struct	bkhdr	 bk_hdr;	/* block header */
		TEXT	 bkdata[1];	/* place holder for data in block 
					   actual size is determined at runtime
					   = DataBaseBlockSize - sizeof(bkhdr) 
					*/
} bkbuf_t;

#define BKBUFSIZE(blksize) (blksize)
#define BLKSPACE(blksize) (blksize - sizeof(struct bkhdr))

#define BKBUF struct bkbuf

#define BKDBKEY bk_hdr.bk_dbkey
#define BKTYPE	bk_hdr.bk_type
#define BKFRCHN bk_hdr.bk_frchn
#define BKINCR	bk_hdr.bk_incr
#define BKNEXTF bk_hdr.bk_nextf
#define BKUPDCTR bk_hdr.bk_updctr

#define RECMASK(recbits) ~(~0L << recbits)
#define BLKNUM_MASK(recbits) (~0L << recbits)

/******************************************************
 The following 2 macros exist ONLY for drconv89.c -
  they should be moved out of here to avoid confusion.
 ******************************************************/
         /* dbkey of block 1 */
#define BLK1DBK (1L << (COUNT)pcontext->pdbcontext->pdbpub->recbits)
        /* dbkey of block 2 */
#define BLK2DBK (2L << (COUNT)pcontext->pdbcontext->pdbpub->recbits)
        /* dbkey of block 3 */
#define BLK3DBK (3L << (COUNT)pcontext->pdbcontext->pdbpub->recbits)

#define BLK1DBKEY(recbits) (1L << (COUNT)recbits)  /* dbkey of block 1 */
#define BLK2DBKEY(recbits) (2L << (COUNT)recbits)  /* dbkey of block 2 */
#define BLK3DBKEY(recbits) (3L << (COUNT)recbits)  /* dbkey of block 3 */

#define BLKSZMASK (~0L << VERSBITS)

#define BKGET_BLKSIZE(pmstr)  ((pmstr)->mb_blockSize)

#define BKGET_AREARECBITS(areaNumber) \
  (COUNT)(BK_GET_AREA_DESC(pcontext->pdbcontext, (areaNumber))->bkAreaRecbits)

/* parameter for bkaddr */
#define RAWADDR    0
#define NORAWADDR  1

/* parameter for bkloc */
#define TOMODIFY     1 /* read and modify (exclusive lock) */
#define TOREAD       2 /* read only (share lock) */
#define TOBACKUP     3 /* read with intention to backup (exclusive lock) */
#define TOREADINTENT 4 /* read with intention to modify (protected share) */
#define TOCREATE     5 /* create a block in buffer pool without reading  */

#define IX_INDEX_MAX 32  /* total number of indexes on a single table */
#define IX_NAME_LEN  200 /* total possible bytes <db>.<table>.<index>\0 */

#define IX_ANCHOR_CREATE  1 
#define IX_ANCHOR_DELETE  2 
#define IX_ANCHOR_UPDATE  3 

typedef struct ixAnchors
{
    dsmText_t    ixName[IX_NAME_LEN];
    DBKEY        ixRoot;
} ixAnchors_t;

typedef struct ixAnchorBlock
{
    struct bkhdr bk_hdr;       /* standard block header */
    ixAnchors_t  ixRoots[IX_INDEX_MAX];
} ixAnchorBlock_t;


#else
    "bkmgr.h must be included via bkpub.h."
#endif  /* #if !(PSC_ANSI_COMPILER || defined(BKBUM_H) */

#endif  /* BKMGR_H */
