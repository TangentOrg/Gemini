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

#ifndef SHMPRV_H
#define SHMPRV_H

#include "shmpub.h"

#include <sys/types.h>

#if OPSYS==UNIX
#include <sys/ipc.h>
#include <sys/shm.h> 
#endif

#if OPSYS == WIN32API
typedef LONG key_t;
#endif

/* Shared pointer DATATYPES */
#define PRIVATE    -2
#define MAXPRIVSLT  10           /* slot reserved for private pointers */

#if UNIX64
/* very large shared memory segments (1 Gb) */
#define OFSTMASK    (LONG)0x3FFFFFFF    /* offset mask */
 
#else
/* big shared memory segments (128 mb) */
#define OFSTMASK    (LONG)0x07FFFFFF   /* offset mask */
#endif /* if UNIX64 */

#define MAX_SHARED_SEGMENTS (MAXSGTBL - MAXPRIVSLT - 1)

SEGHDR	{
	    LONG    magic;	/* label this as a PROGRESS shm segment	*/
	    int     shmid;	/* shmid used to access this segment	*/
	    int	    shmvers;	/* version number, see SHMVERS		*/
	    QSEGMNT qthisseg;	/* base address of this segment		*/
	    QSEGMNT qnextseg;	/* address of next segment same database*/
	    COUNT   segno;	/* 0, 1, 2, 3, etc for segs for 1 db	*/
				/* only used for 1st segment of database*/
	    COUNT   numsegs;	/* Total number of segments created	*/
	    QTEXT   qmisc;	/* for use by the sh mem user program	*/
	    QSTPOOL qpool;	/* points to pool header in this segmnt	*/
	    QPIDLIST qidlist;	/* points to list of pids using segmnts	*/
            LONG    segsize;    /* size of segment */
	    TEXT    name[MAXUTFLEN+1]; /* char string to identify db	*/
            int     qnextshmid; /* shmid used to access next segment    */
	};

#define SEGMAGIC        -559030611L   /* seghdr.magic   */

/* if you change SHMVERS, be sure to look in /usr/rdln/rh/utshm.h       */
/* to be sure you don't take a value being used by some other version   */
/* Convention for assigning numbers is:

        version 5.2:            3
        version 6.2:            6xx
        version 6.3:            63xx
        version 7.0 to 7.2:     70xx
        version 7.3A            7301 +
        version 7.3B:           7331 +
        version 7.4:            7400 +
        version 8.0:            8001 +
	version 9.0:            9000 +
        64-bit platform:    64000000 + version specific number
        etc.
*/

#if UNIX64
#define SHMVERS64       64000000
#else
#define SHMVERS64       0
#endif

/* 9040 qmandctl is now a hash table */
/* 9041 added lru chain to om cache          */
/* 9042 Additional locking members for dynamic */
/* 9043 skipped in order to differentiate from 90b */
/* 9044 lock strength counts in lock */
/* 9045 shared memory segment size change for alphaosf */
/* 9046 restore time sync code for bi cluster aging */
/* 9046 shm seg size 128mb for all platforms */
/* 9047    txe optimizations                    */
/* 9048  64-bit rl dependency counter         */
/* 9049 Change in max size of shared memory segment table  */
#define SHMVERS         (9052 + SHMVERS64)


SEGMNT	{
	    SEGHDR  seghdr;	/* control into to identify the segment	*/
	    LONG    abk;	/* STPOOL compatible storage area	*/
	};

PIDLIST	{
	    int     maxpids;	/* number of slots in the table		*/
	    int	    pid[1];	/* table of pids			*/
	};

SHMDSC {
            TEXT        *pname;   /* the name used to attach the segmnt */
            PIDLIST     *pidlist; /* the PIDLIST inside first segment   */
            psc_user_number_t numusrs; /* how big to allocate the pidlist    */
            psc_user_number_t pidslot; /* where in PIDLIST is MY pid         */
            COUNT        numsegs; /* total number of segments attached  */
            TEXT         unused[2];
            SEGMNT      *pfrstseg;/* points to the first segment attchd */
            SEGMNT      *plastseg;/* points to the last segment attchd  */
            STPOOL      *ppool;   /* points to STPOOL in 1st segment    */
            LONG         shmarea; /* amount of space avail in each seg  */
            LONG         segsize; /* size of each segment of shm */
            LONG         shmid;   /* the first shmid to attach to  */
        };

/* If you change MAXSEGSIZE, you must also change stuff in shmpub.h */
#if UNIX64
#define MAXSEGSIZE 0x40000000   /* 1 GB */
#else
#define MAXSEGSIZE 0x08000000   /* 128 megabytes (134217728 bytes) */
#endif

/* Function prototypes for procedures used within shm sub-system only */

TEXT * shmAttachSegment    (struct dsmContext *pcontext, int shmid,
                            TEXT *psegin);

int shmattsegs             (struct dsmContext *pcontext, SHMDSC *pshmdsc);

DSMVOID shmapid               (struct dsmContext *pcontext, SHMDSC *pshmdsc,
                            SEGMNT *pseg, int numusrs);

int
shmCreateSegment           (struct dsmContext *pcontext, SHMDSC *pshmdsc,
                            int    idchar);

int shmDeleteSegment(struct dsmContext *pcontext,
                     TEXT *pname,
		     int  idchar);

DSMVOID shmDeleteSegments     (struct dsmContext *pcontext, SHMDSC *pshmdsc);

DSMVOID
shmDetachSegment(struct dsmContext *pcontext, SEGMNT *pseg);

DSMVOID
shmDetachSegments(struct dsmContext *pcontext, SHMDSC *pshmdsc);

/* BUM - because of reference to shmcrseg in st.c the following define
         is needed.                                                    */
#define shmFormatSegment shmcrseg

LONG *
shmFormatSegment(struct dsmContext *pcontext, SHMDSC *pshmdsc);

LONG shmSegArea (struct dbcontext *pdbcontext);

int shmGetSegmentId(struct dsmContext *pcontext,
                    TEXT *pname, /* the database name or other file name*/
		    int  idchar, /*differentiates segments for same db  */
		    int  noentmsg);/* if 0, dont print ENOENT error msg	*/

DSMVOID
shmmsg(struct dsmContext *pcontext,
       int   error, 	   /* the value of errno when the error occurred*/
       TEXT  *function,    /* "attach", "create", "get", etc	        */
       TEXT  *pname,       /* the dbname or other identifier            */
       key_t  skey);	   /* the key identifying the segment	        */

int
shmpid(SHMDSC *pshmdsc); 	/* describes the shmemory to put it in */

ULONG
shmPutSegmentAddress(struct dsmContext *pcontext, SEGADDR segaddr,
	    COUNT segno, LONG size, shmTable_t *pshsgctl);

int shmqpid          (struct dsmContext *pcontext, SEGMNT *pseg);

TEXT *
shmVerifySegment(struct dsmContext *pcontext, int shmid);

DSMVOID shmrpid(SHMDSC	*pshmdsc);

/* The following are OPSYS specific prototypes for shmcr.c */
#if OPSYS==WIN32API
DSMVOID	utmkosnm		(TEXT *prefix, TEXT *pname, TEXT *buf,
				 int len, int id);
int	utshmAddNamedSegment	(TEXT *SegmentName, int SegmentId, ULONG data);
int	utshmGetNamedSegment	(TEXT *SegmentName);
HANDLE	utshmGetUserData	(int segid);
int	utshmFreeNamedSegment	(TEXT *SegmentName);
int	utshmFreeSegmentId	(int SegmentId);

#endif  /* #if OPSYS==WIN32API */

#endif  /* SHMPRV_H         */


