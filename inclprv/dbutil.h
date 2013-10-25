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
#ifndef DBUTIL_H
#define DBUTIL_H

/* dbutil.h - schema definitions structures */

#define SCDBG 0 /* Empty db generation mode */
#define FTADD 1 /* Fast-track convert mode */
#define CONV  2 /* Convert 5 to 6 mode */

#define AREASTAT struct areastat /* entry for each area */
typedef struct areastat
{
        COUNT    version;
        ULONG    number;        /* area number */
        ULONG    type;          /* Type of area */
#if 0
        DBKEY    block;         /* area Block number */
        COUNT    attrib;
        COUNT    system;
        ULONG    blocksize;
        COUNT    recbits;
#endif
        ULONG    extents;       /* Number of extents */
        char    *name;          /* name of area */
} areastat_t;

#define FILSTAT struct filstat /* entry for each schema file */
struct filstat			
{
#if defined (__STDC__) && __STDC__ || defined(__cplusplus)
	const char	*filename;	/* file name */
#else
	char	*filename;	/* file name */
#endif
	int	filnum;		/* file number */
	int	gnumkey;	/* number of keys */
	int	numkcomp;	/* number of key components */
	int	numkfld;	/* number of key fields */
	int	numfld;		/* number of fields */
	DBKEY	fildbk;		/* dbkey of file record */
	DBKEY   templdbk;	/* dbkey of file's template rec*/
        ULONG   areaNumber;     /* Storage area for table's data rows */
#if defined (__STDC__) && __STDC__ || defined(__cplusplus)
        const char    *table_type;    /* "T" - table, "V" - view (SQL92) */
#else
        char    *table_type;    /* "T" - table, "V" - view (SQL92) */
#endif
}; 

#define SYSVIEWSTAT struct sysviewstat /* entry for each schema SQL92 view */
typedef struct sysviewstat			
{	char	*viewname;	/* view name */
	char	*viewtext;	/* view text == SQL text string defining view*/
} sysviewstat_t ;


#define FLDSTAT struct fldstat /* entry for each schema field */
struct fldstat		
{	int	 filidx;	/* index of file (above).
                                 * == -1 if NOT referencing any file.
                                 * -1 used in dbstat for _default index's
                                 * "phantom" key field (new for SQL92 + 4GL).
                                 */
	char	*fldname;	/* ptr to static field name */
	int	 gdtype;	/* internal data type */
	int	 gposition;	/* position in record */
	int	 gmand;		/* mandatory */
	int	 system;	/* system field */
	char	*ginit;		/* ptr to static initial value */
	char	*gformat;	/* ptr to static input/print format */
	DBKEY	 gdbkey;	/* dbkey of field record */
	int      extent;        /* extent */
	char	*fldlabel;	/* ptr to static field label */
	char	*fldhelp;	/* ptr to static field help */
        int      gcase;         /* 1 = Case Sensitive */
	char	*base_fldname;	/* name of base field for a view column.
                                 * Base field must be another entry in fldstat
                                 * which view column is based on, and which
                                 * usually gives basic attributes == fldstat
                                 * entries.
                                 * Base field attributes can be overridden by
                                 * new fldstat entries for view column.
                                 * Null if field is not a column in a view.
                                 */
        char    *base_filname;   /* name of file for base field, since
                                 * field name itself may not be unique.
                                 * Null if field is not a column in a view.
                                 */
}; 
#define KEYSTAT struct keystat
struct keystat
{	int	file;		/* index of file in file table */
	char	*name;		/* ptr to static name */
	int	uniq;		/* unique */
	int	prim;		/* primary key */
	int	numc;		/* number of key components */
	COUNT	gidxnum;	/* index manager number */
	int	kcidx;		/* start index in kcstat below */
        ULONG   areaNumber;
}; 
#define KCSTAT struct kcstat
struct kcstat
{	int	seq;		/* sequence number */
        /*** int	fldidx;***/ 		/* index in fld */
        /*** fldidx has been replaced by key_filname, key_fldname
         *** use below, to identify the key component's field.
         ***/
	int	substr;		/* substring matching is ok */

        char    *key_filname;   /* name of file for key field, since
                                 * field name itself may not be unique.
                                 */
	char	*key_fldname;	/* name of key field for index.
                                 * Key field must be an entry in fldstat.
                                 */
};

#define SCCHG struct scchg 
struct scchg
{        int    file;   /* file # */
         int    numf;   /* numfields added */
         int    ixnum;  /* primary index # */
         int    fstart; /* where in fld struct do they start */
};

#if 0 /* !PSC_ANSI_COMPILER */
/* Some function headers for functions in upscdbg and upscutl.c */
DSMVOID scFixVect (struct ci *, int, FILSTAT *, COUNT, FLDSTAT *, COUNT);
int  scInstallTrans (TEXT *, TEXT *);
#endif /* 0 */



#endif  /* ifndef DBUTIL_H */
