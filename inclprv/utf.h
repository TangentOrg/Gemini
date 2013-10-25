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

#ifndef UTF_H
#define UTF_H

struct utfFindInfo;

/******************************************************************/
/*** utf.h - include file for file utilities                    ***/
/******************************************************************/

/* mode parm for utfchk and utffnd */
#define UTFEXISTS       0
#define UTFEXEC         1
#define UTFWRITE        2
#define UTFREAD         4
#define UTFCHECKLIB     16  /* Indicates whether libraries should be */
                            /* searched.  Items found in libraries   */
                            /* are reported as found iff the query   */
                            /* is *NOT* EXEC or WRITE */
#define UTFOSSEARCH     32  /* OS-SEARCH mode, accept dirs etc       */
#define UTFDEADLINKS    64  /* if set, we'll check for dead symbolic links */

/* path parm for utffnd */
#define DLC             1
#define PROPATH         2
#define DLCDB           3

/* Maximum number of suffixes to pass to utffnd */
#define UTF_MAX_SUFFIX  20

/* entry points */

int  utfchk();  /* utfchk ( name, mode );       check to see if a file
                   both exists and is accesible as desired */
int  utDirAccess(); /* utDirAccess ( name, mode ); check dir access */
int  utffnd(TEXT *, UCOUNT, TTINY, TEXT *,
                   TEXT *, UCOUNT, COUNT *, struct utfFindInfo *);
int  utffnddll(TEXT *, UCOUNT, TTINY, TEXT *,
                   TEXT *, UCOUNT, COUNT *, struct utfFindInfo *,
                      struct globcntxt *);
     /* utffnd: use the
                   indicated path to locate file and verify access.
                   returns full path name in expname */
TEXT *  utfcomp(TEXT *);  /* utfcomp ( path_name ); returns pointer to
                           first character of final component of
                           path name */

int  utfOsFilename(TEXT *, TEXT *, int );

TEXT *utfpath(TEXT *);
TEXT *utfDLC(TEXT *);

TEXT * utDisplayType(int);
TEXT * utWhatWindowSys();

/* 
 * This structure is used to pass status information about a utffnd 
 * result back to the caller.
 */
struct utfFindInfo
{
    ULONG size;   /* Size of file */
    ULONG offset; /* For library, the offset where the file starts */
    int   LibDesc; /* For library, the open file descriptor for the lib */
    ULONG lastmtime; /* time of last modification */
    TEXT *libaddress; /* For memory-mapped library, address of lib */
};
        
/* utftmp.c   */

int utftmpo(TEXT *tname);

/*FILE * utfstmp(TEXT *tname, char	*mode); */

VOID utftmpc(int, TEXT	*);

/* utfio.c  */

int utfflsh(int);


/* utckdbnm.c */
int utckdbnm(TEXT *, int, BOOL);
int utckdbname(TEXT *, int, BOOL, TEXT *);

/* utosfunc.c */
#if OPSYS==WIN32API
VOID utSetWindowSys(TEXT *);
VOID utClearWindowSys(VOID);
#endif

#if OPSYS==WIN32API
DWORD utWinGetOSVersion(VOID);
int   utWinCheckForMultiSession(VOID);

/*  utWinGetOSVersion() return values */
#define ENV_UNKNOWN     0
#define ENV32_WIN95     1       /* 32-bit App on WIN95 */
#define ENV32_WINNT     2       /* 32-bit App on WINNT */
#define ENV16_WIN3X     3       /* 16-bit App on WIN3x */
#define ENV16_WIN95     4       /* 16-bit App on Win95 */
#define ENV16_WINNT     5       /* 16-bit App on WINNT */
#endif

/* utf.c */ 
VOID utfdelsuffix(TEXT*);

#endif /* UTF_H */
