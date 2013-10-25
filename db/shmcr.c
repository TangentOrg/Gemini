
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

/*****************************************************************/
/**								**/
/**	shm - routines to access the System V shared memory	**/
/**	Shared memory sets are identified using keys constructed**/
/**	from the database inode and an identifier character,	**/
/**	using the ftok routine described in stdipc(3).		**/
/**								**/
/*****************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include <stdlib.h>

#include "dbconfig.h"

#if SHMEMORY

#include "dbcontext.h"
#include "dbmsg.h"
#include "dsmpub.h"
#include "sempub.h"
#include "semprv.h"
#include "shmprv.h"
#include "shmpub.h"
#include "stmpub.h"
#include "stmprv.h"
#include "utmsg.h"
#include "utfpub.h"
#include "utspub.h"
#include "uttpub.h"

#if OPSYS==UNIX
#include <errno.h>
#include <unistd.h>
#endif

#if OPSYS==WIN32API
#include <errno.h>
#include <windows.h>
#endif /* WIN32API */

/* size of largest segment we were able to get */
static LONG mssize = MAXSEGSIZE; /* accessible by dbenv1 or when MT_LOCK_GST */

/* PROGRAM: utmkosnm -
 *
 * RETURNS: DSMVOID
 */
#if OPSYS==WIN32API
DSMVOID
utmkosnm(
	     TEXT    *prefix,
	     TEXT    *pname,
	     TEXT    *buf,
	     int      len,
	     int      id)
{
    TEXT *p, *pch;

    stnclr(buf, len);
    pch = buf;
    pch = stcopy(pch, prefix);

#if OPSYS==WIN32API
    /* [johnls] */
    for (p = pname; *p && (int)(pch - buf) != len; pch++, p++)
    {
	if (*p == ':' && (*(p+1) == '\\' || *(p+1) == '/'))
	    p++;

	if (*p == '/' || *p == '\\' || *p == ':')
	    *pch = '.';
	else
	    *pch = tolower(*p);
    }
    *pch = 0;
#else
    pch = stncop(pch, pname, MIN(8, len-11-stlen(prefix)));
#endif
    if (id >= 0)
    {  
        pch = stcopy(pch, ".");
        utitoa(id, pch);
    }
} 
#endif /* OS2 || WIN32API */


#if OPSYS==WIN32API

/*
 * Support Functions to provide shared memory segment to segment name mapping.
 *
 * All the following code was needed to support shmDeleteSegment(), which frees
 * segments based on segment name, not the originally returned shared
 * memory ID.  The problem with shmDeleteSegment() is that to convert a segment name
 * to the shared memory segment id, the (to use the NT terminology) shared
 * memory must be "Opened".  This increments the access count, on existing
 * shared memory.  Which is not what we want to do with shmDeleteSegment().  Our
 * desire is to free (decrement the lock count) the shared memory.
 *
 * NOTE: This code does not actually create or free shared memory.  It
 *       just provides tracking functionality for created shared memory.
 *
 * NOTE: This table reflects only the current processes shared memory
 */

#ifndef _MAX_PATH
#define _MAX_PATH  260
#endif

typedef struct 
{
   int   SegmentId;
   ULONG UserData;              /* Place to hang user data */
   TEXT  SegmentName[80];
} SHMINFO;

int ShmInfoEntries = 0;
int ShmInfoLength  = 0;
SHMINFO *lpShmInfo = (SHMINFO *)NULL;

/*
 * PROGRAM: utshmAddNamedSegment
 * Purpose: Add a named segment and base address for the segment to the
 *          local segment table.  (Segment MUST NOT already exist)
 * Params : SegmentName - Name for the segment to add
 *          SegmentId   - Id associated with the segment name ( >=0 )
 *                        (For NT this is the base address, OS2 the seg id)
 *          data        - 32 bit param to hang user defined data off the
 *                        memory tracking table.
 *
 * Returns: 0 on success, !0 on failure
 */
int
utshmAddNamedSegment(TEXT *SegmentName, int SegmentId, ULONG data )
{
  SHMINFO *temp;

  if (ShmInfoEntries == ShmInfoLength)
  {
    temp = lpShmInfo;
    if ((lpShmInfo = (SHMINFO *)realloc( temp,
                    sizeof(SHMINFO)*(ShmInfoLength + 10))) == (SHMINFO *)NULL)
    {
      lpShmInfo = temp;
      return -1;
    }
    ShmInfoLength += 10;
  }

  /*
   * Add the Segment Info to the table
   */
  lpShmInfo[ ShmInfoEntries ].SegmentId = SegmentId;
  lpShmInfo[ ShmInfoEntries ].UserData  = data;
  stcopy(lpShmInfo[ ShmInfoEntries ].SegmentName, SegmentName);
  ShmInfoEntries++;

  return 0;
}

/*
 * PROGRAM: utshmGetNamedSegment
 * Purpose: Find a specific named segment and return its segment id
 * Params : SegmentName - Name of the segment to search for
 * Returns: SegmentId associated with the segment name
 *          -1 if the segment name was not registered
 */
int
utshmGetNamedSegment(TEXT * SegmentName)
{
  int i;

  for (i=0;i<ShmInfoEntries;i++)
    if (!stcomp(SegmentName, lpShmInfo[i].SegmentName))
      return lpShmInfo[i].SegmentId;

  return -1;
}

/*
 * PROGRAM: utshmGetUserData
 * Purpose: Find a specific segment ID and return its associated user data
 * Params : SegmentID - 
 * Returns: UserData specified in utshmAddNamedSegment().
 *          -1 if the segment name was not registered
 */
HANDLE
utshmGetUserData(int segid)
{
  int i;

  for (i=0;i<ShmInfoEntries;i++)
    if (segid == lpShmInfo[i].SegmentId)
      return (HANDLE)lpShmInfo[i].UserData;

  return (HANDLE)-1;
}

/*
 * PROGRAM: utshmFreeNamedSegment
 * Purpose: Remove a named segment from the table
 * Params : SegmentName -- The name of the segment to remove
 * Returns: 0 on success, !0 on failure
 */
int
utshmFreeNamedSegment(TEXT *SegmentName)
{
  int i,j;

  for (i=0;i<ShmInfoEntries;i++)
    if (!stcomp(SegmentName, lpShmInfo[i].SegmentName))
    {
      for (j=i+1; j<ShmInfoEntries; j++)
        lpShmInfo[j-1] = lpShmInfo[j];

      if (--ShmInfoEntries == 0)
      {
        ShmInfoLength = 0;
        free(lpShmInfo);
        lpShmInfo = (SHMINFO *)NULL;
      }

      return 0;
    }

  return -1;
}

/*
 * PROGRAM: utshmFreeSegmentId
 * Purpose: Remove a segment (using the segment id )from the table
 * Params : SegmentIde -- The id of the segment to remove
 * Returns: 0 on success, !0 on failure
 */
int
utshmFreeSegmentId(int SegmentId)
{
  int i,j;

  for (i=0;i<ShmInfoEntries;i++)
    if (SegmentId == lpShmInfo[i].SegmentId)
    {
      for (j=i+1; j<ShmInfoEntries; j++)
        lpShmInfo[j-1] = lpShmInfo[j];

      if (--ShmInfoEntries == 0)
      {
        ShmInfoLength = 0;
        free(lpShmInfo);
        lpShmInfo = (SHMINFO *)NULL;
      }

      return 0;
    }

  return -1;
}

/* PROGRAM: shmCreateSegment - create a shared mem segment	*/
/* RETURNS: -1 = error; >=0 = the shmid of the segment	*/
int
shmCreateSegment(
        dsmContext_t *pcontext,
        SHMDSC       *pshmdsc,/* control blk for a series of shm segs */
        int           idchar)  /*id for multiple segments for same db  */
{

    LPVOID  lpmem;
    HANDLE  hmap;
    TEXT    *pname = pshmdsc->pname;
    char    szShareMem[MAXUTFLEN];
    char    szFile[MAXUTFLEN];

    pshmdsc->segsize = MIN(pshmdsc->segsize, MAXSEGSIZE);
    utapath(szFile, MAXUTFLEN, pname, "");
    utmkosnm("sharemem.", szFile, szShareMem, MAXUTFLEN, idchar);

    hmap = CreateFileMapping((HANDLE)0xFFFFFFFF,  /* Use the swap file    */
			     (SECURITY_ATTRIBUTES*)SecurityAttributes(0), 
                                                  /* Not inherited        */
			     PAGE_READWRITE,      /* Memory is Read/Write */
			     0L,                  /* Size Upper 32 Bits   */
			     (DWORD)pshmdsc->segsize,/* Size Lower 32 bits*/
			     szShareMem);

    if (!hmap)
    {
    	DWORD dwRet = GetLastError();
    
        switch (dwRet) 
	{
	  case ERROR_ALREADY_EXISTS:      /* Right now this case will never */
	    /* be TRUE. hmap is not NULL   */
	    /* if the mapping already existed */
	  default:
	    /* Unable to create shared memory %s, error %d */
	    MSGN_CALLBACK(pcontext, utMSG062, szShareMem, dwRet);
	    return -1;
	}
    }

    lpmem = MapViewOfFile(hmap,
			  FILE_MAP_WRITE | FILE_MAP_READ,
			  0, 0, (DWORD)pshmdsc->segsize);

#if 0 /* Your supposed to be able to do this */
	CloseHandle(hmap);
#endif
    if (lpmem == 0)
    {
    	/* Unable to create shared memory %s, error %d */
    	MSGN_CALLBACK(pcontext, utMSG062, szShareMem, 0);
    	return -1;
    }

    if (utshmAddNamedSegment(szShareMem, (int)lpmem, (ULONG)hmap))
    {
    	/* Failed, out of memory */
    	UnmapViewOfFile(lpmem);
    	return -1;
    }
    
    return (int)lpmem;
}

#endif  /* IF OPSYS==WIN32API  */


#if OPSYS==UNIX
/* PROGRAM: shmCreateSegment - create a shared mem segment	*/
/* RETURNS: -1 = error; >=0 = the shmid of the segment	*/
int
shmCreateSegment(
        dsmContext_t *pcontext,
        SHMDSC       *pshmdsc, /*control blk for a series of shm segs */
        int           idchar _UNUSED_) /* Key value for multiple segments within db */ 
{
    int	 shmid;
    TEXT	*pname = pshmdsc->pname;/*database name or other file name*/

    pshmdsc->segsize = MIN(pshmdsc->segsize, mssize);

#if ALPHAOSF
    /* -Mpte keeps VLM64 allocations in multiples of 8Mb for good performance */
    if (pcontext->pdbcontext->argMpte) while (pshmdsc->segsize >= 0x00800000)
    {
	shmid = shmget(IPC_PRIVATE,                 /* ensure unique id   */
                       (int)pshmdsc->segsize,       /* segment size       */
                       0666+IPC_EXCL+IPC_CREAT);    /* segment attributes */

	if (shmid >= 0) return shmid;   /* successful		*/

	/* "vm: gh-fail-if-no-mem=1" in /etc/sysconfigtab can raise ENOMEM */
	if (errno == EINVAL || errno == ENOMEM)
	{
	    /* the size is invalid, decrease it by 8MB and try again */
	    pshmdsc->segsize -= 0x00800000;
	    mssize = pshmdsc->segsize;
	    continue;
	}
	else break;
    }
    /* if this didn't work, go on and try it the old fashioned way. */
    mssize = pshmdsc->segsize = 0x00800000;
#endif
    do  
    {
        pshmdsc->segsize = (pshmdsc->segsize + 0xfff) & ~0xfff;

	shmid = shmget(IPC_PRIVATE,                 /* ensure unique id   */
                       (int)pshmdsc->segsize,       /* segment size       */
                       0666+IPC_EXCL+IPC_CREAT);    /* segment attributes */

	if (shmid >= 0) return shmid;   /* successful		*/
        if (errno == EINVAL)
	{
	    /* the size is invalid, decrease it  and try again */
	    pshmdsc->segsize = pshmdsc->segsize - 8192;
	    if (pshmdsc->segsize < 8 * 1024)
	    {
		/* The allowed shm segment size too small */
		MSGN_CALLBACK(pcontext, utMSG024);
		return -1;
	    }
	    mssize = pshmdsc->segsize;
	}
    }
    while (errno == EINVAL);

    if (errno == ENOMEM || errno == EAGAIN)
    {
        MSGN_CALLBACK(pcontext, utMSG026);
    }
    else
    {
        /* print an error message	*/
        shmmsg(pcontext, errno, (TEXT *)"create", pname, (key_t)shmid);
    }

    return -1;
}
#endif /* OPSYS==UNIX */


#if OPSYS==UNIX

/* PROGRAM: shmChangeOwner - change the owner of the shared memory segment 
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
shmChangeOwner(
        dsmContext_t *pcontext, 
	int           shmid,	/* change the owner of this segment */
	int           uid)	/* change the owner to this uid	    */
{
	struct shmid_ds buf;

    if (shmctl(shmid, IPC_STAT, &buf) < 0)
    {	shmmsg(pcontext, errno, (TEXT *)"read status of", PNULL, 0);
	return;
    }

    buf.shm_perm.uid = uid;

    if (shmctl(shmid, IPC_SET, &buf) < 0)
    {	shmmsg(pcontext, errno, (TEXT *)"change owner of", PNULL, 0);
	return;
    }
}
#endif /* OPSYS==UNIX */

#if OPSYS==UNIX
/* PROGRAM: shmmsg - Print an error message based on errno */
DSMVOID
shmmsg(
        dsmContext_t *pcontext, 
        int           error,    /* the value of errno when the error occurred*/
        TEXT         *function _UNUSED_, /* "attach", "create", "get", etc       */
        TEXT         *pname,    /* the dbname or other identifier       */
        key_t         skey)     /* the key identifying the segment      */
{
    switch (error)
    {
      case ENOENT: FATAL_MSGN_CALLBACK(pcontext, utFTL022,skey);
      case EINVAL: FATAL_MSGN_CALLBACK(pcontext, utFTL023);
      case EACCES: FATAL_MSGN_CALLBACK(pcontext, utFTL024);
      case ENOSPC: MSGN_CALLBACK(pcontext, utMSG025);
	break;
      case EEXIST: MSGN_CALLBACK(pcontext, utMSG027,pname);
	break;
      default:
	FATAL_MSGN_CALLBACK(pcontext, utFTL018);
    }

    errno = error;	/* in case errno was modified by message subsystem */
}

#endif /* OPSYS==UNIX */


/* PROGRAM: shmFormatSegment - create a new shared memory segment
   and format it                                                     	*/
/* RETURNS: NULL = couldnt create the segment				*/
/*	non-NULL = address of the ABK which was put in the segment	*/
/* PARAMETERS:                                                          */
/*     SHMDSC  *pshmdsc          control blk for a series of shm segs   */
/*     dbcontext_t *pdbcontext   for this database                      */
LONG *
shmFormatSegment(
	dsmContext_t	*pcontext,
	SHMDSC		*pshmdsc)
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    LONG	 size;
    STPOOL	 tmppool;
    SEGMNT	*pseg;
    int		 shmid;
    int		 segno;

    segno = pshmdsc->numsegs;
    if (segno == 0)
    {	/* this is the first shared memory segment */
	/* create dummy stpool header. it will later be moved into 1st seg */
	stmIntpl(&tmppool);
	pshmdsc->ppool = &tmppool;
	tmppool.pshmdsc = pshmdsc;
    }

#if OPSYS==UNIX || OPSYS==WIN32API
    shmid = shmCreateSegment(pcontext, pshmdsc, segno);
    if (shmid == -1) return NULL;
    /* #92-07-09-013 CLB - Test for shmid == -1 needed for OS2 2.0 */
#if OPSYS==WIN32API
    pseg = (SEGMNT *)shmid;
#endif 
#if OPSYS==UNIX
    /* change segment's owner from root to me */
    shmChangeOwner(pcontext, shmid, getuid());

    pseg = (SEGMNT *)shmAttachSegment(pcontext, shmid, NULL);
#endif
    if (!pseg)
    {
#if OPSYS == UNIX
	shmDeleteSegment(pcontext,pshmdsc->pname, shmid);
#else
	shmDeleteSegment(pcontext,pshmdsc->pname, segno);
#endif

	return NULL;
    }
#endif /* OPSYS==UNIX || OPSYS==WIN32API */

    /* add segment to private segment table */
#if OPSYS==UNIX || OPSYS==WIN32API
    /* BUM - Assume segno < 2^15 */
    shmPutSegmentAddress(pcontext, (SEGADDR)pseg,
                         (COUNT)segno,
                         pshmdsc->segsize, 
			 pdbcontext->pshsgctl);
#endif

#if ALPHAOSF
	/* increment VLM attach address */
	{ extern LONG axp_shmaddr;
	  axp_shmaddr += pshmdsc->segsize;
	}
#endif

    /* calculate amount of room available in the segment, initialize it */
    /* and hook the ABK within the segment to the STPOOL		*/
    size = (LONG)pshmdsc->segsize - (((ABK *)&pseg->abk)->area - (TEXT *)pseg);
    stIntabk(pcontext, (ABK *)&pseg->abk,pshmdsc->ppool,size);
    stLnk(pcontext, pshmdsc->ppool, (ABK *)&pseg->abk);

    if (segno==0)
    {
	/* get storage in shared memory for the stpool header */
	
	pshmdsc->ppool = (STPOOL *)stGetd(pcontext, &tmppool, sizeof(STPOOL));
        pseg->seghdr.qpool = P_TO_QP(pcontext, pshmdsc->ppool);
	bufcop((TEXT *)pshmdsc->ppool, (TEXT *)&tmppool, sizeof(STPOOL));

	/* allocate the pidlist */
	shmapid(pcontext, pshmdsc, pseg, pshmdsc->numusrs);

	/* record my PID as a user of this shared memory */
	shmpid(pshmdsc);

	pshmdsc->pfrstseg = pseg;
	pshmdsc->shmarea  = size;
#if OPSYS==UNIX
        pseg->seghdr.qnextshmid = shmid;
#endif
    }
    else
    {
	pshmdsc->plastseg->seghdr.qnextseg = (QSEGMNT)P_TO_QP(pcontext, pseg);
#if OPSYS==UNIX
        pshmdsc->plastseg->seghdr.qnextshmid = shmid;
#endif
    }

    pshmdsc->plastseg = pseg;

    /* rest of the misc initialization of the shared memory segment */
    pseg->seghdr.shmid = shmid;
    pseg->seghdr.segno = segno;
    pseg->seghdr.qthisseg = (QSEGMNT)P_TO_QP(pcontext, pseg);
    pseg->seghdr.qnextseg = 0;
    stncop(pseg->seghdr.name, pshmdsc->pname, MAXUTFLEN);
    pseg->seghdr.magic = SEGMAGIC;
    pseg->seghdr.shmvers = SHMVERS;
    pseg->seghdr.segsize = pshmdsc->segsize;

    pshmdsc->numsegs++;
    pshmdsc->pfrstseg->seghdr.numsegs = pshmdsc->numsegs;

    return &pseg->abk;
}

#endif /* IF SHMEMORY  */


