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

#ifndef BKFTBL_H
#define BKFTBL_H


/*****************************************************************/
/**								**/
/**	bkftbl.h - file extent table. There is one table for	**/
/**	the data file(s) of a PROGRESS database and one table	**/
/**	for the bi file(s). These tables are built during	**/
/**	database open by bkseto. If a multi-file PROGRESS	**/
/**	database is opened, this information is extracted	**/
/**	from the file list (see bkflst.h) in the structure	**/
/**	file. If a single-file PROGRESS database is opened,	**/
/**	a pair of tables is built which identify standard	**/
/**	UNIX files with no length restrictions.			**/
/**								**/
/**	The two tables are terminated by an entry with a	**/
/**	zeroed ft_type field. They are anchored from the	**/
/**	pbkctl->bk_ftbl array, indexed by BKDATA or BKBI.	**/
/**								**/
/**	This table is used to convert from a database or bi	**/
/**	block number to a file and byte offset within that	**/
/**	file. If the file is a standard UNIX file and raw	**/
/**	I/O is active, the entry points to a raw I/O work	**/
/**	area which contains inode information for the file	**/
/**	as well as a pointer to a file system work area.	**/
/**								**/
/**	Note that the lengths and offsets are expressed in	**/
/**	bytes, converted by bkseto from the block units used	**/
/**	in the file list.					**/
/**								**/
/**	Also note that for each file extent, the offset field	**/
/**	is incremented at open to cause the extent master	**/
/**	block to be skipped, and the length is similarly	**/
/**	decremented.						**/
/**								**/
/*****************************************************************/

#if !PSC_ANSI_COMPILER || defined(BKBUM_H)

typedef struct bkftbl
{
	gem_off_t ft_curlen;	/* current (physical) length of
				   file, does not include extent
				   master blocks */
	gem_off_t ft_offset;	/* offset of data within file */
	ULONG    ft_blksize;    /* blocksize in bytes used in the file */
	LONG     ft_blklog;     /* log2 of blocksize to convert blocks/bytes */
	gem_off_t ft_xtndoff;	/* offset of start of last extend,
				   if any, zero to indicate no
				   extend is active */
	QTEXT	ft_qname;	/* pointer to name of file,
				   NULL for table stopper */
	QTEXT	ft_qrawio;	/* raw I/O routine work area,
				   only used for standard UNIX
				   files without the -r option */
	int	 ft_fdtblIndex;	/* index into the iotbl structure
				   This structure determines what type
                                   I/O routines are used to read and write */
	int      ft_openmode;   /* Mode the file was opened with
				   1-BUF, 2-UNBUF, 3-BUF and UNBUF */
	COUNT	 ft_xtndamt;	/* amount of most recent extend, if any */
	GTBOOL	 ft_type;	/* type of file, see defines below */
	TEXT	 ft_syncio;	/* type of I/O to be used, see below */

	QBKFTBL	 qself;		/* self ref shared pointer */

  bkiostats_t    ft_bkiostats;  /* status for bkio operations */

	ULONG	 ft_nwrites;	    /* number of writes since startup */
	ULONG	 ft_nreads;	    /* number of reads since startup */
	ULONG	 ft_nxtnds;	    /* number of extends since startup */
	COUNT	 ft_delete;	    /* flag for this extent to be deleted */
} bkftbl_t;

#define BKFTBL struct bkftbl

/* NOTE: see bk_pftbl in bkctl.h before changing BKETYPE */

/* these are also exclusive: */
#define BKUNIX	4	/* standard UNIX file */
#define BKPART	8	/* "raw" partition file */

#define BKSINGLE 16	/* single UNIX file database, bksetc constructed
			   file table for compatibility */

#define BKFIXED 32	/* fixed length file */

#else
    "bkftbl.h must be included via bkpub.h."
#endif  /* #if !PSC_ANSI_COMPILER || defined(BKBUM_H) */

#endif /* #ifndef BKTBL_H */
