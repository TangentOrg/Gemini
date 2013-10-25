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

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include <stdlib.h>
#include "dbconfig.h"

#include "bkpub.h"
#include "dbcontext.h"
#include "dbpub.h"
#include "dbmsg.h"
#include "sempub.h"
#include "shmprv.h"
#include "shmpub.h"
#include "stmsg.h"
#include "utmsg.h"
#include "utfpub.h"
#include "utmpub.h"
#include "utspub.h"


#if OPSYS==UNIX || OPSYS==WIN32API
#if OPSYS == UNIX
#include <unistd.h>
#endif /* OPSYS==UNIX */
#include <errno.h>
#endif /* OPSYS==UNIX || OPSYS==WIN32API */

#if (OPSYS==UNIX) && !IBMRIOS
extern int edata;		/* end of initialized data */
#endif

#if OPSYS==WIN32API
#include <windows.h>
#endif /* WIN32API */

#if OPSYS==WIN32API

/* PROGRAM: shmGetSegmentId - get the shmid of an existing
 * shared memory segment
 * RETURNS: -1 = error; >=0 = the shmid of the segment
 */
int
shmGetSegmentId(
        dsmContext_t *pcontext, 
        TEXT *pname,	/* the database name or other file name		*/
        int  idchar,	/* differentiates multiple segments for same db	*/
        int  noentmsg) 	/* if 0, dont print ENOENT error msg		*/
{
    HANDLE  hmap;
    char    szShareMem[MAXUTFLEN];
    char    szFile[MAXUTFLEN];
    LPVOID	lpmem;

    utapath(szFile, MAXUTFLEN, pname, "");
    utmkosnm("sharemem.", szFile, szShareMem, MAXUTFLEN, idchar);

    hmap = OpenFileMapping(
			   FILE_MAP_WRITE | FILE_MAP_READ, /* Read/Write */
			   FALSE,             /* Not inherited     */
			   szShareMem);

    if (!hmap)
    {
    	DWORD dwErr = GetLastError();

    	switch (dwErr) 
	{
            case  ERROR_FILE_NOT_FOUND:
                if (noentmsg)
                {
                    MSGN_CALLBACK(pcontext, dbMSG008, pname);
                }
                return -1;

	    default:
	      if (noentmsg)
              {
		  /* Unable to attach shared memory %s, error %d */
		  MSGN_CALLBACK(pcontext, utMSG063, szShareMem, dwErr);
              }
	      return -1;
	}
    }

    lpmem = MapViewOfFile(hmap,
			  FILE_MAP_READ | FILE_MAP_WRITE, 
			  0, 0, 0);   /* Map the entire range into memory */

    if (lpmem == 0)
    {
    	/* Unable to attach shared memory %s, error %d */
    	MSGN_CALLBACK(pcontext, utMSG063, szShareMem, 0);
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
#endif /* OPSYS==WIN32API */

#if OPSYS==UNIX

/* PROGRAM: shmGetSegmentId - get the shmid of an existing
 * shared memory segment
 *
 * RETURNS: -1 = error; >=0 = the shmid of the segment
 */
int
shmGetSegmentId(
        dsmContext_t *pcontext,
        TEXT         *pname, /* the database name or other file name*/
        int           idchar _UNUSED_, /*differentiates segments for same db */
        int           noentmsg)/* if 0, dont print ENOENT error msg	*/
{
    key_t	skey =-1;
    int		shmid;

    /* get the key */
/*    skey = semKey(pname, idchar); */
    if (skey == -1) 
    	return -1;
    
    shmid = shmget(skey, 0, 0);
    if (shmid >= 0) 
    	return shmid;   /* successful		*/

    if (noentmsg || errno != ENOENT)
	shmmsg(pcontext, errno, (TEXT *)"get", 
	       pname, skey);/* print an error message */
    
    return -1;
}
#endif /* OPSYS==UNIX */

#if OPSYS==UNIX
#if ALPHAOSF
/* Since we use ld -D 0x14000000 to place .data inside the 32 bit
   part of the address space, the default 64 bit start address for
   .data should be available.  axp_shmaddr gets incremented after
   each use */
char *axp_shmaddr = (char *)0x140000000;
#endif

/* PROGRAM: shmAttachSegment - attach an existing shared mem segment
 *
 * RETURNS: NULL=error, non-NULL=address the seg was attached at
 */
TEXT *
shmAttachSegment(
	dsmContext_t	*pcontext,
	int		 shmid,   /* shared mem id returned by
				  shmGetSegmentId or utshmcr             */
	TEXT		*psegin) /* attach at this address, if null then at
				  any address                             */
{
    TEXT	*pseg;		/*  the return from shmat */
    TEXT * pedata;		/* address of end of data */
    TEXT *pbrk = 0;		/* address of current sbrk() */
    VLMsize_t minhpsiz;		/* min heap size (bytes) */
    VLMsize_t curhpsiz;		/* current heap size */
    VLMsize_t incr;			/* size of heap change  */
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    

    /* ON machines that use the SYS V implementation of shared memory,
       shared memory is map right above the heap.  In order that we
       have the proper space for dynamic allocation, the heap size must
       be temporalily increased to cause shared memory to mapped high
       enough in memory  */

    if(pdbcontext->argheapsize)
    {
	minhpsiz = (VLMsize_t)pdbcontext->argheapsize  * 1024;
	pbrk = (TEXT *)sbrk(0);
#if IBMRIOS
        curhpsiz = (ULONG)( pbrk);
#else
        pedata =(TEXT*)&edata;
	curhpsiz = (VLMsize_t)( pbrk - pedata);
#endif
	if (curhpsiz < minhpsiz)
	{
	    incr = minhpsiz - curhpsiz;
	    if((long)sbrk(incr) < 0)
	        /* -hs parameter too large */
		MSGN_CALLBACK(pcontext, utMSG051,pdbcontext->argheapsize);
	}
    }

#if SUN45
    /* the SHM_SHARE_MMU parameter speeds up access on Solaris systems! */
    pseg = (TEXT *)shmat((int)shmid,(char *)psegin, SHM_SHARE_MMU);

    /* if we failed because SHM_SHARE_MMU is an invalid argument, then
       try to attach the old way */
    if ( pseg == (TEXT *) -1 && errno == EINVAL)
    {
	pseg = (TEXT *)shmat(shmid, psegin, 0 );
    }
#else
#if ALPHAOSF
    /* try to force segment into VLM address range */
    pseg = (TEXT *)shmat((int)shmid,(char *)axp_shmaddr, 0);
    /* if explicit attach fails, try attach at default */
    if ( pseg == (TEXT *) -1)
    	pseg = (TEXT *)shmat((int)shmid, 0, 0);
#else
    pseg = (TEXT *)shmat((int)shmid,(char *)psegin, 0);
#endif  /* ALPHAOSF */
#endif  /* SUN45 */

    if(pdbcontext->argheapsize)
	brk(pbrk);		/* reset brk address so malloc can use it */
        
    if (pseg != (TEXT *)-1) return pseg;	/* successful		*/

    switch (errno)
    {
        case EINVAL:
            /* pdbcontext->usertype == 0 is for single user */
            if (((pdbcontext->usertype != BROKER) &&
                (pdbcontext->usertype != BATCHUSER) &&
                (pdbcontext->usertype != 0)))
            {
                MSGD_CALLBACK(pcontext,
     "%BSYSTEM ERROR: Can't attach shared memory with segment_id: %l for %s",
                              shmid, pdbcontext->pdbname);
               MSGN_CALLBACK(pcontext, dbMSG008, pdbcontext->pdbname);
            }
 
            if (psegin)
                MSGN_CALLBACK(pcontext, utMSG028);
 
            break;
        case ENOMEM:
        case EAGAIN:
            MSGN_CALLBACK(pcontext, utMSG030);
            break;
        case EMFILE:
            MSGN_CALLBACK(pcontext, utMSG029);
            break;
        default:
            shmmsg(pcontext, errno, (TEXT *)"attach",
                   PNULL, 0);/* print an error message */
            break;
    }

    return NULL;
}
#endif /* OPSYS==UNIX */

#if OPSYS==UNIX || OPSYS==WIN32API

/* PROGRAM: shmVerifySegment - attach a segment, and verify that it has
 *          our magic number in it.
 *
 * RETURNS: the address where the segment was attached
 */
TEXT *
shmVerifySegment(
	dsmContext_t	*pcontext,
	int		 shmid)	/* attach this segment number	*/
{
	SEGMNT	*pseg;

#if OPSYS==WIN32API
    pseg = (SEGMNT *)shmid;
#endif

#if OPSYS==UNIX
    pseg = (SEGMNT *)shmAttachSegment(pcontext, shmid, PNULL);
#endif
    if (pseg == NULL) return NULL;

    if (pseg->seghdr.magic != SEGMAGIC)
    {   MSGN_CALLBACK(pcontext, utMSG037);
	shmDetachSegment(pcontext, pseg);
	return NULL;
    }

    return (TEXT *)pseg;
}
#endif  /* OPSYS==UNIX || OPSYS==WIN32API */


/* PROGRAM: shmattsegs - used by non-broker to attach to existing sh memory 
 *
 * RETURNS: 0 = OK, non-0 = non-OK
 */
int
shmattsegs(
	dsmContext_t	*pcontext,
	SHMDSC		*pshmdsc)  /* fill in this descriptor structure	*/
{	
	dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	COUNT	 segno=0;
	int	 shmid = -1;
	COUNT	 numsegs;
	SEGMNT	*pfrstseg = (SEGMNT *)0;
	SEGMNT	*pseg;
	PIDLIST *pidlist = (PIDLIST *)0;

#if OPSYS==UNIX
        bkIPCData_t *pbkIPCData = NULL;
#endif

    /* there is always at least 1 segment */
    numsegs = 1;

#if OPSYS==UNIX || OPSYS==WIN32API
    for(segno=0; segno < numsegs; segno++)
    {
#if OPSYS==UNIX
        if (segno == 0)
        {
          /* Get storage for pkIPCData */
            pbkIPCData = (struct bkIPCData *)utmalloc(sizeof(bkIPCData_t));

            stnclr(pbkIPCData, sizeof(bkIPCData_t));

            /* Make sure we got the storage */
            if (pbkIPCData == (bkIPCData_t *)0)
            {
                /* We failed to get storage for pbkIPCData */
                MSGN_CALLBACK(pcontext, stMSG006);
                return -1;
            }
            
            /* initiailize pbkIPCData */
            pbkIPCData->NewSemId = -1;
            pbkIPCData->OldSemId = -1;
            pbkIPCData->NewShmId = -1;
            pbkIPCData->OldShmId = -1;

            /* Get the first shm id out of the .lk file    */
            if (bkGetIPC(pcontext, pshmdsc->pname, pbkIPCData))
            {
                /* Failed to get a shm id out of the mstrblk */
                shmid = -1;
            }
            else
            {
                shmid = pbkIPCData->NewShmId;
            }
        }
#else
	shmid = shmGetSegmentId(pcontext, pshmdsc->pname,
				segno, 1/* print all err msgs */);
#endif
	if (shmid == -1) 
        {
#if OPSYS==UNIX
            /* pdbcontext->usertype == 0 is for single user */
            if (((pdbcontext->usertype != BROKER) &&
                 (pdbcontext->usertype != BATCHUSER) &&
                 (pdbcontext->usertype != 0))) 
            {
                MSGN_CALLBACK(pcontext, dbMSG008, pdbcontext->pdbname);
            }

            /* clean up pbkIPCData */
            utfree((TEXT *)pbkIPCData);
#endif
            return -1;
        }
        /* #92-07-09-013 CLB - Test for shmid == -1 needed for OS2 2.0 */

	pseg = (SEGMNT *)shmVerifySegment(pcontext, shmid);

	if (pseg == NULL) 
        {
            /* avoid SEGV in shmPutSegmentAddress */
#if OPSYS==UNIX
            /* Clean up from getting the shmid */
            utfree((TEXT *)pbkIPCData);
#endif
            return -1;
        }

#if OPSYS==UNIX || OPSYS==WIN32API
	shmPutSegmentAddress(pcontext, (SEGADDR)pseg,segno,0, 
			     pdbcontext->pshsgctl);
#endif

	if (pseg->seghdr.shmvers != SHMVERS)
	{   
           MSGN_CALLBACK(pcontext,utMSG036, (int)pseg->seghdr.shmvers, SHMVERS);
#if OPSYS==UNIX
           /* Clean up from getting the shmid */
           utfree((TEXT *)pbkIPCData);
#endif
	    return -1;
	}

#if ALPHAOSF
	/* increment VLM attach address */
	axp_shmaddr += pseg->seghdr.segsize;
#endif
	/* the first segment contains inportant info */
	if (segno == 0)
	{   
            pfrstseg = pseg;
	    numsegs = pseg->seghdr.numsegs;
	    pidlist = XPIDLIST(pdbcontext, pseg->seghdr.qidlist);
#if OPSYS==UNIX
           /* Get the next shmid to connect to */
            shmid = pseg->seghdr.qnextshmid;
#endif
	}
#if OPSYS==UNIX
        else
        {
            /* Get the next shmid to connect to */
            shmid = pseg->seghdr.qnextshmid;
        }
#endif
    }
#endif /* OPSYS==UNIX || OPSYS==WIN32API */

    /* dont fill in these variables until all segments are attached  */
    /* successfully, because otherwise, the shutdown code (shdelsegs)*/
    /* will blow up miserably					     */
    pshmdsc->pidlist = pidlist;
    pshmdsc->numsegs = numsegs;
    pshmdsc->pfrstseg= pfrstseg;
    pshmdsc->plastseg= pseg;
    pshmdsc->ppool   = XSTPOOL(pdbcontext, pfrstseg->seghdr.qpool);

    /* put my pid into the list of users using this shared memory */
    shmpid(pshmdsc);
#if OPSYS==UNIX
    /* Clean up from getting the shmid */
    utfree((TEXT *)pbkIPCData);
#endif

    return 0;
}

#if OPSYS==WIN32API
/* PROGRAM: shmDetachSegment - detach a shared memory segment
 *
 * RETURNS: DSMVOID
 */
DSMVOID
shmDetachSegment(
	dsmContext_t	*pcontext,
	SEGMNT		*pseg) 	/* detach the segment at this address */
{
    HANDLE hMemObj;

    /* Release the shared memory, and remove references to it from
       the tracking table ( in utshm.c ) */
    hMemObj = (HANDLE)utshmGetUserData ( (int)pseg );
    utshmFreeSegmentId ( (int)pseg );
    UnmapViewOfFile(pseg);
    if ( hMemObj != 0 && hMemObj != (HANDLE)-1 )
       CloseHandle( hMemObj );
}
#endif /* OPSYS==WIN32API */


#if OPSYS==UNIX
/* PROGRAM: shmDetachSegment - detach a shared memory segment */
DSMVOID
shmDetachSegment(
	dsmContext_t	*pcontext _UNUSED_,
	SEGMNT		*pseg)  /* detach the segment at this address */
{
    shmdt((char *)pseg);
}
#endif /* OPSYS==UNIX */


/* PROGRAM: shmDetachSegments - release the shared memory associated
   with a db */
DSMVOID
shmDetachSegments(
	dsmContext_t	*pcontext,
	SHMDSC		*pshmdsc) /* describes the segments to release */
{
	dbcontext_t *pdbcontext = pcontext->pdbcontext;
	SEGMNT	*pnextseg;
	SEGMNT	*pseg;
    
    if (pshmdsc == NULL) return;	/* never initialized at all */
    if (pshmdsc->pfrstseg == NULL) return; /* already released	    */

    /* remove my pid from the list of shared memory users */
    shmrpid(pshmdsc);
    
    /* extract needed info, then NULL out pfrstseg to prevent	*/
    /* infinite loops and memory violations			*/
    pseg = pshmdsc->pfrstseg;
    pshmdsc->pfrstseg = NULL;

#if OPSYS==UNIX || OPSYS==WIN32API
    /* detach all the segments first */
    for(;;pseg = pnextseg)
    {
	pnextseg = XSEGMNT(pdbcontext, pseg->seghdr.qnextseg);
#if OPSYS==UNIX
        if (pdbcontext->usertype & BROKER) 
        {
            shmDeleteSegment(pcontext,pshmdsc->pname,pseg->seghdr.shmid);
        }
#endif
	shmDetachSegment(pcontext, pseg);
	if (pseg == pshmdsc->plastseg) break;
    }
#endif
}
