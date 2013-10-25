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

#ifndef BKFLST_H
#define BKFLST_H

/*****************************************************************/
/**								**/
/**	bkflst.h - file list. This list contains one entry	**/
/**	for each data or bi file comprising a multi-file	**/
/**	PROGRESS database.					**/
/**								**/
/**	The first block of a multi-file database structure	**/
/**	file is a special type of master block. That block	**/
/**	identifies the number of file list entries for the	**/
/**	database. The entries themselves are contained		**/
/**	entirely in the second block of the structure file.	**/
/**								**/
/**	Each entry contains a flag indicating its type		**/
/**	(bi or data), its maximum length if it is fixed-	**/
/**	length, its offset if it is a raw partition, and	**/
/**	its path name. This path name either identifies	a	**/
/**	standard UNIX file, or it identifies a character	**/
/**	special device.						**/
/**								**/
/**	When an database is opened, the file list         	**/
/**	entries are read into storage and converted into two	**/
/**	separate file extent tables, one for the data files	**/
/**	and one for the bi files. This involves scanning	**/
/**	the list twice, once to determine the number of each	**/
/**	type of entry and the total length of the names,	**/
/**	once to actually build the tables.			**/
/**								**/
/**	All length and offset fields are in 1024 byte units	**/
/**	must be even.						**/
/**								**/
/*****************************************************************/

#if !PSC_ANSI_COMPILER || defined(BKBUM_H)

typedef struct bkflst
{
		LONG	fl_maxlen;	/* maximum length of file,
					   zero if no restriction */
		LONG	fl_offset;	/* offset of data within
					   file */
                ULONG   fl_areaNumber;
                ULONG   fl_extentId;
                UCOUNT  fl_areaRecbits; /* # rec bits in this fl_areaNumber */
		GTBOOL	fl_type;	/* type of file, see bkftbl.h */
		TEXT	fl_name[MAXUTFLEN+1]; /* name of file */
		TEXT	fl_areaName[MAXPATHN+1]; /* area name of new file */
/* bug 92-04-15-014: delete pad character for dos */
} bkflst_t;


/* number of data blocks for fixed number of extents */
#define NUMFLBK(numext, blksize)    ((int)((numext) * sizeof(struct bkflst)/(blksize) ) + 1)

/* minimum number of blocks allowed in a fixed-length file */
#define MINFLEN 32

/* units of size and offset */
#define FLUNITS 1024

#else
    "bkflst.h must be included via bkpub.h."
#endif  /* #if !PSC_ANSI_COMPILER || defined(BKBUM_H) */

#endif /* #ifndef BKCRASH_H */
