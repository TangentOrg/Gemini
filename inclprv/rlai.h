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

#ifndef RLAI_H
#define RLAI_H

/* RLAI.H - structures and definitions for .ai file */

#include "rlpub.h"

#define AIHDR	struct aihdr
AIHDR {
	COUNT	aihdrlen;	/* nbr of bytes in this header		*/
	COUNT	aiused;		/* nbr of bytes used in this block	*/
	ULONG	aiupdctr;  	/* update ctr to know if update was made*/
	LONG	aiblknum;	/* block address of this block */
	LONG    ainew;          /* date AIMAGE BEGIN or NEW was run     */
	};

/* the first block in the ai file has a larger header than the rest	*/
/* keep this a multiple of 4 bytes long					*/
#define AIFIRST	struct aifirst
AIFIRST {
	AIDATES	aidates;	/* 16 bytes of ai date stamps		*/
	LONG	lstmod;		/* date db and/or ai file was last chgd */
	LONG    aiblksize;	/* blocksize of ai file in bytes	*/
	LONG    aiextstate;     /* After image extent state. See below  */
	ULONG   aifirstctr;     /* update ctr of first note in ai extent*/
                                /* 0 if regular ai file.                */
	LONG    aieof;          /* Set to offset + 1 of last byte       */
	                        /* written to ai file.  Used only for   */
	                        /* after image extents. Value for this  */
	                        /* variable is set when state changes   */
                                /* from busy to full.                   */
	LONG    aiversion;      /* aiversion MUST be the last member
				   of this structure.                   */
	};

/* version 0 of AIHDR      */
#define AIHDRv0	struct aihdrv0
AIHDRv0 {
	COUNT	aihdrlen;	/* nbr of bytes in this header		*/
	COUNT	aiused;		/* nbr of bytes used in this block	*/
	LONG	aiupdctr;  	/* update ctr to know if update was made*/
	LONG	aiblknum;	/* block address of this block */
	};

/* Version 0 of AIFIRST      */
#define AIFIRSTv0 struct aifirstv0
AIFIRSTv0 {
	AIDATES	aidates;	/* 16 bytes of ai date stamps		*/
	LONG	lstmod;		/* date db and/or ai file was last chgd */
	LONG	aiversn;	/* guaranteed to be 0			*/
	LONG    aiextstate;     /* After image extent state. See below  */
	LONG    aifirstctr;     /* update ctr of first note in ai extent*/
                                /* 0 if regular ai file.                */
	LONG    aieof;          /* Set to offset + 1 of last byte       */
	                        /* written to ai file.  Used only for   */
	                        /* after image extents. Value for this  */
	                        /* variable is set when state changes   */
                                /* from busy to full.                   */
	};

/*
 * aiversion info:
 *
 * Version 0 7.2
 * Version 1 7.3  Changed for ainew in AIHDR and aiblksize in AIFIRST
 */
#define AIVERSION   1

/* Definitions for values of after image extent state (aiextstate)      */
#define AIEXTNONE   0           /* Regular ai file - not an ai extent   */
                                /* of a multi-volume database.          */
#define AIEXTBUSY   1           /* Extent currently logging after images*/
#define AIEXTEMPTY  2           /* Empty ai extent - ready for reuse    */
#define AIEXTFULL   4           /* Extent full - needs to be archived   */

/* Flag values passed to rlaiswitch                                     */
#define RLAI_SWITCH 0           /* Switch from full fixed len extent
				   to next extent.                      */
#define RLAI_NEW    1           /* Aimage new or Online backup          */
#define RLAI_TRY_NEXT 2         /* Can't extend var len extent so       
                                   try next extent.                     */

/* Return values for rlaiswitch                                         */
#define RLAI_CANT_SWITCH   1    /* Next extent is not empty             */
#define RLAI_CANT_EXTEND   2    /* No space to extend var len extent    */

/* TODO: with variable size blocks this structure doesn't represent actual
   block.  Maybe we should remove aidata portion and just keep header? */


#define AIBLK	struct aiblk
AIBLK {
	AIHDR	aihdr;
	AIFIRST	aifirst;
	TEXT	aidata[1]; /* placeholder for data - 
			      actual size=(DbBlockSize-sizeof(AIHDR)-sizeof(AIFIRST))
			   */
	};

#define AIBUF           struct aibuf

AIBUF
{
        BKTBL    aibk;          /* bktbl for one buffer */
        QAIBUF   aiqnxtb;       /* pointer to next buffer in ring */
        ULONG    aisctr;        /* first ai note number in this buffer */
        ULONG    ailctr;        /* last ai note number in this buffer */
#if SHMEMORY
        QAIBUF    qself;
#endif
};

AICTL {
	BFTBL	 aibftbl;	/* bftbl control str for the ai buffer	*/
	BKTBL	 aibktbl;	/* bktbl control str for the ai buffer	*/
				/* interesting variables in this str are:
				aictl.aibktbl.bt_dirty-on if blk modified
				aictl.aibktbl.bt_bidpn-bi dependency ctr
				aictl.aibktbl.bt_raddr-raw addr of blk
				aictl.aibktbl.bt_offst-lseek addr of blk
				aictl.aibktbl.bt_pbuf-ptr to buffer	*/
	LONG	 ailoc;		/* lseek addr current input block	*/
				/* the last note in the ai file		*/
	LONG	 aisize;	/* lseek addr of first byte beyond	*/
				/* physical EOF of the ai file		*/
	COUNT	 aiofst;	/* offset within current ai block	*/
	TEXT	 aimode;	/* see #defines below			*/
	GTBOOL	 aion;		/* 1 if ai is open for OUTPUT		*/
	GTBOOL	 aiaudit;	/* audit notes to .ai file		*/
	UCOUNT   aiFileType;    /* TL or AI file                        */
	ULONG    aiArea;        /* Area currently logging notes.        */

	ULONG	 aiupdctr;	/* last update counter value */
	ULONG	 aiwritten;	/* last update counter value written to disk */

        /* buffer write wait queue */

	COUNT	nWaiters;	/* no. of users waiting for write       */
	QUSRCTL qwrhead;        /* first user in write queue            */
	QUSRCTL qwrtail;        /* last user in write queue             */

        int     aiwpid;         /* pid of ai writer */
        QUSRCTL qaiwq;          /* wait queue for ai writer */
        BKTBL   aiwbktbl;       /* bktbl str for writing the ai buffer  */
        QAIBUF  aiqcurb;        /* current output buffer */
        QAIBUF  aiqwrtb;        /* next buffer to write */
        QAIBUF  aiqbufs;        /* ai buffer ring */

	/* TODO:  move these to an appropriate place when checking in   */
	/* blocksize information derived from master block              */
	LONG    aiblksize;      /* ai blocksize in bytes                */
	LONG    aiblklog;       /* log2 of blocksize for manipulation   */
        LONG    aiblkmask;      /* block mask for ai blocksize          */
	};

/* values for aictl.aimode */
#define AICLOSED	0	/* ai manager is not open	*/
#define AIINPUT		1	/* ai open for input, (rollfwd)	*/
#define AIOUTPUT	2	/* ai open for output		*/

#endif  /* RLAI_H */







