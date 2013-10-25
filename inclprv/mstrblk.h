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
#ifndef MSTRBLK_H
#define MSTRBLK_H

/*****************************************************************/
/**								**/
/**	Every PROGRESS database contains a "master block"	**/
/**	which in turn contains control information for the	**/
/**	database. This master block is always the first		**/
/**	logical data block, with a dbkey of BLK1DBK.		**/
/**								**/
/**	This master block is read into the buffer pool at	**/
/**	initialization and stays in storage for the entire	**/
/**	session. At the end of every transaction the		**/
/**	contents of the master block are forced back out	**/
/**	to disk by a call to bkflshmb.				**/
/**								**/
/**	The in-core copy of the database master block is	**/
/**	pointed to by the external variable pmstrblk.		**/
/**								**/
/**	While a single-file PROGRESS database contains		**/
/**	exactly one master block, a multi-file PROGRESS		**/
/**	database contains at least three master blocks.		**/
/**								**/
/**	The first block of the structure file is a special	**/
/**	master block which alerts progress that a multi-	**/
/**	volume file is being opened and contains paramaters	**/
/**	such as the number of data and bi extents.		**/
/**								**/
/**	Each extent of a multi-volume file, be it a data or	**/
/**	bi extent, contains an extent master block which is	**/
/**	used both to verify that all of the extents for a	**/
/**	multi-volume database are consistent and to prevent	**/
/**	an individual file from a multi-volume file from	**/
/**	being opened as a single-file database.			**/
/**								**/
/**	The actual database master block for a multi-volume	**/
/**	database is the second physical block of the first	**/
/**	data extent.						**/
/**								**/
/*****************************************************************/

#define EMPTYDATE 463860476	/* date/time of last schema change
				   set in empty database by the scdbg
				   utility. it is 9/12/84 2PM. */
#define VERS6DATE 617740379	/* date/time of last schema change
				   set in vers6 database by scdbg
				   uitilty. it is 7/31/89, and only
				   for _file _db _index _field _index-f*/

#define MAXIXTB		32	/* maximum number of blocks which may
				   contain index anchors */

#include "rlpub.h"      /* For definition of AIDATES    */
#include "rltlpub.h"    /* For defintion of rltlCounter_t */

#if PSC_ANSI_COMPILER
#include "bkpub.h"      /* For definition of struct bkhdr */
#else
#include "bkmgr.h"
#endif


typedef struct mstrblk {struct bkhdr bk_hdr;
/*
NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE

	All the fields in the mster block are supposed to be aligned
	properly. COUNTs begin at even offsets and LONGs begin
	at offsets which are a multiple of 4. there are a couple of
	padding fields for this.

	If you add something, make sure you get the alignment right!

NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
*/
		ULONG	mb_dbvers;	/* version number of database,
					   Negative value for multi-file
					   structure, data or bi files,
					   see dbvers.h */
                ULONG   mb_blockSize;   /* Database block size      */ 
                ULONG   mb_dbMinorVers; /* Database minor version # */
                ULONG   mb_clientMinorVers; /* Client minor version # */
		DBKEY	mb_seqblk;	/* dbkey of sequence number block */
		DBKEY   mb_dbrecid;     /* database key of local db record
                                           useful for schema operations */
                DBKEY   mb_objectRoot;  /* Root block for the index
					   on the _Object table.        */
                dsmDbkey_t areaRoot;        /* index root block for
                                              _Area-Number */
                dsmDbkey_t areaExtentRoot;  /* index root block for
                                              _AreaExtent-Area */
		COUNT	mb_dbstate;	/* current state of database, as
					   defined below */
		GTBOOL	mb_tainted;	/* flags databases which are known
					   to be damaged */
		GTBOOL	mb_flags;	/* integrity indicators, defined
					   below */
		LONG	mb_lasttask;	/* number of last database task
					   given out */
		/* a-image and before-image control info */
		AIDATES	mb_ai;		/* 16 bytes of ai date fields	*/
		LONG	mb_lstmod;	/* date db or ai was last chged	*/
		LONG	mb_aiwrtloc;	/* logical eof of ai file	*/
		ULONG	mb_aictr;	/* ai file block update ctr	*/
		TEXT	mb_aiflgs;	/* see #defines below		*/
		TEXT	mb_spare7;      /* 	*/
		UCOUNT	mb_spare8;	/*  */
		LONG	mb_biopen;	/* date bi file was last opnd	*/
		LONG	mb_biprev;	/* prev value of mb_biopen	*/
		LONG	mb_rltime;	/* latest recorded rl time	*/
		COUNT	mb_rlclsize;	/* current bi cluster size in	*/
					/* units of 16k. 0 same as 1	*/
					/* for V5 compatibility		*/
		COUNT   mb_bhvolno;     /* Used by restore utility      */
		GTBOOL	mb_aisync;	/* ON if sync i/o for ai file	*/
		GTBOOL	mb_chgd;	/* ON if db chgd since last bkup*/
		TEXT	mb_bistate;	/* see #defines below		*/
		GTBOOL   mb_spare9;	/*        */
		TEXT	mb_spare10;    /*        */
		TEXT	mb_spare11;	/*   */
		UCOUNT	mb_aiseq;	/* ai file sequence number
					   increments when new ai file started,
					   reset on db restore */
		UCOUNT	mb_buovl;	/* amount to adjust overlap, taking
					   failed backups into account. */
		UCOUNT	mb_buseq;	/* successful backup sequence number
					   same as BKINCR if last was good*/
		LONG	mb_crdate;	/* date and time of database
					   creation */
		LONG	mb_oprdate;	/* date and time of most recent
					   database open */
		LONG	mb_oppdate;	/* date and time of previous
					   database open, used to detect
					   non-monotonic time */
		LONG	mb_fbdate;	/* date and time of most recent
					   full backup of database */
		LONG	mb_ibdate;	/* date and time of most recent
					   incremental backup */
		LONG	mb_ibseq;	/* incremental backup sequence no. */
		COUNT	mb_dbcnt;	/* number of data extents for a
					   multi-volume structure file
					   master block, else zero */
		COUNT	mb_bicnt;	/* number of bi extents for a
					   multi-volume structure file
					   master block, else zero */
		/*DEMO counter and time stamp*/
		COUNT	mb_democt;	/* number of demo tries left */
		COUNT	mb_flag;	/* so far only used for demo vrs*/

		/* two phase commit stuff */
                TEXT    mb_nicknm[12];  /* Must be > MAXNICKNM */
                TEXT    mb_lightai;     /* is ai used only as a 2phase TL ? */
                TEXT    mb_crd;         /* 1 if has a coord priority        */
                TEXT    mb_pad2[2];	/* to align next field */
                ULONG   spare[2];       /* Use to be Off-line TL log dates  */
                LONG    mb_tlwrtloc;    /* logical eof of off-line TL file  */
                LONG    mb_tlwrtoffset; /* eof within block.                */
                ULONG   spare2[2];      /* Use to be mb_tlctr               */
		COUNT	mb_biblksize;	/* Before image blocksize (kb), 0
					   means same as database */
		COUNT	mb_aiblksize;	/* After image blocksize (kb), 0
					   means same as database */
		UCOUNT  mb_aicnt;       /* Number of ai extents          */
		UCOUNT  mb_aibusy_extent; /* Current ai extent that is the
                                              busy extent */

		LONG	mb_shutdown;    /* true shutdown. The blocks are
                                           are guaranteed to be on disk
                                           and not in the UNIX buffer pool */
		LONG	mb_timestamp;	/* schema time stamp		*/
                ULONG   mb_shmid;       /* shm id for broker  */
                ULONG   mb_semid;       /* sem id for broker  */
}mstrblk_t;

/* values for mb_dbstate field */

/* one of the following values is set in the database master block: */
#define DBOPEN		1	/* database is open, used on disk */
#define DBCLOSED	2	/* database is closed, used on disk */
#define DBRECOV		4	/* database being recovered, value
				   is not important, tests are all for
				   the previous two */
#define DBRESTORE	8	/* restore underway */

/* The following is set in mstrblk in  conjunction with the above flags */
#define DBDSMVOID		16	/* database is empty of data
                                 * (created via prostrct create)
                                 * The database contains control area
                                 * and master block but NO meta-schema.
                                 */

/* one of these four flags is set in the extent header block: */
#define DBMSTR		32	/* this is a multi-file structure file */
#define DBMDB		64	/* this is a multi-file data file */
#define DBMBI		128	/* this is a mult-file bi file */
#define DBMAI           256     /* this is a multi-file ai file */
#define DBMTL           512     /* this is a mutli-file tl file */

/* one of these is set in database masterblock during db conversion work */
#define DBCONVDB        512     /* version conversion underway */
#define DBCONVCH       1024     /* character encoding conversion underway */
#define DBIXRPR        2048	/* index reconstruction under way  */


/* values for mb_flags and mb_tainted */
#define ARGFORCE 1	/* mb_tainted: -F option was used to skip recovery */
#define NOINTEG  2	/* mb_tainted: db crashed with -i no integrity	   */
			/* mb_flags: db currently running with -i	*/
#define NONRAW	 4	/* mb_tainted: db crashed with -r non-raw mode	   */
			/* mb_flags: db currently running with -r	*/
/*	DBRESTORE 8	   mb_tainted: db used after partial restore */
#define DBBACKUP 16	/* mb_flags: backup in progress
			   mb_tainted: backup not completed */
#define DBIXBUILD 32    /* mb_tainted: db indexes are invalid, must rebuild */
                        /* added to support collation table schema trap 7.3A */
#define DBTAINT64  64   /* unused bit */
#define DBTAINT128 128  /* unused bit */

/* values for mb_flag */
#define DEMOVERS  1
#define UNDECIDED 2

/* values for mb_aiflgs */
#define MBAIAUDIT 1

/* values for mb_bistate, wierd values to prevent accidental match with  */
/* garbage in the master block						 */
#define BITRUNC	0x13	/* bi file was truncated and ok if doesnt exist */
#define BIEXIST 0x14	/* bi file must exist				*/

#endif /* MSTRBLK_H */





