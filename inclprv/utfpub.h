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

#ifndef UTFPUB_H
#define UTFPUB_H

#include "dbutret.h"
#include "utospub.h"


#define UTFEXISTS       0

/* */
DLLEXPORT int utfabspath (TEXT *pathname);

DLLEXPORT int  utfchk(TEXT    *name,
                      int      mode);

DLLEXPORT int  utfcmp(TEXT    *f1,
                      TEXT    *f2);

DLLEXPORT int  utflen(TEXT *pname, gem_off_t *utfilesize);

DLLEXPORT TEXT * utfpath(TEXT *path_name);

DLLEXPORT DSMVOID utfpcat(TEXT    *pdirbuf,
                       TEXT    *pname);

DLLEXPORT int  utfpsep(TEXT *pdirbuf);

DLLEXPORT TEXT *utfcomp (TEXT    *path_name);

DLLEXPORT int utcmpsuf (TEXT *pname, TEXT *psuffix);

DLLEXPORT int utfcomplen (TEXT *name);


DLLEXPORT TEXT *utfstrip(TEXT *ptr,
               TEXT *psuffix);

DLLEXPORT TEXT *utfsufix(TEXT *pname,
               TEXT *psuffix);

#if OPSYS == WIN32API || OPSYS == UNIX

DLLEXPORT int utfCompressPath(TEXT *pfullpath);
DLLEXPORT int utfCompressPair(TEXT *ps,
                              TEXT *parent);
DLLEXPORT int utfCompressSlash(TEXT *pfullpath);
#if OPSYS == WIN32API

DLLEXPORT DSMVOID utfChgSlash(TEXT *pname);

#endif /* OPSYS == WIN32API */
#endif /* OPSYS == WIN32API || OPSYS == UNIX */


/* prototypes for utfilesys.c */

DLLEXPORT int utIsDir(TEXT *pname);

DLLEXPORT TEXT *utapath( TEXT *fullpath,    /* fully qualified path name */
               int  pathlen,	  /* length of answer buffer */
	           TEXT *filename,    /* file name, possibly qualified */
	           TEXT *suffix);	  /* suffix for file name */

DLLEXPORT TEXT * utmypath(TEXT *pfullpath, 
                          TEXT *pdatadir,
                          TEXT *prelname,
                          TEXT *psuffix);


DLLEXPORT int utpwd (TEXT	*pdir,	/* answer goes here */
		   int	 len,	/* length of answer area */
		   int	drvno);	/* drive number */

DLLEXPORT LONG utMakePathname(TEXT  *buffer,
                             int   buflen,
                             TEXT  *pstr1,
                             TEXT  *pstr2);

COUNT utgetdrv          (DSMVOID);
COUNT utgetdir          (int drvno, TEXT *outptr);

#if OPSYS == WIN32API
DLLEXPORT int utdrvstr(int  drvno,
             TEXT *pdir);

DLLEXPORT int utGetDriveType (TEXT  DriveLetter);

/* constants for utGetDriveType() */
#define UNKNOWN_DRIVE     0
#define REMOVABLE_DRIVE   1
#define FIXED_DRIVE       2
#define LOCAL_DRIVE       4
#define REMOTE_DRIVE      8
#define SUBSTITUTED_DRIVE 16

#endif

#if OPSYS == UNIX
DSMVOID utpwdSetPath(TEXT * path,
                  int len);
#endif


/* utfio.c */

#define DEFLT_PERM	-1	/* create file w/ default permissions mask */
				/* else use given mode */

#define AUTOERR		0x0001	/* display error message */
#define CHG_OWNER	0x0002	/* change owner on creat/open */
#define OPEN_CREATE	0x0004	/* create file on open */
#define OPEN_SUPOP	0x0008	/* no new version when file is created */

/* one and only one of the next 3 flags below must be used */
#define OPEN_SEQL	0x0010	/* normal sequential open */
#define OPEN_BLOCK	0x0020	/* block/buffered method */
#define OPEN_RELIABLE	0x0040	/* "synced"/unbuffered method */

#define OPEN_LARGEFILE  0x0080  /* Open large file >2gb  */

/* next 2 flags are for specific #if OPSYS == VMS use */
#if OPSYS == VMS
#define	VMSQIO		OPEN_RELIABLE
#define	VMSBIO		OPEN_BLOCK
#else
#define VMSQIO		0x0000
#define VMSBIO		0x0000
#endif

#endif /* UTFPUB_H */



