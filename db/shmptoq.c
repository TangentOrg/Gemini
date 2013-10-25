
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

#define DLLGLOB
#include "dbconfig.h"
#if SHMEMORY
#include "shmpub.h"
#include "shmprv.h"
#include "stmpub.h"
#include "stmprv.h"
#include "dbcontext.h"
#include "utmsg.h"

#include "dsmpub.h"
#include "usrctl.h"  /* temp for DSMSETCONTEXT */
#include "utspub.h"


/********************************************************************
  
  To allow multiple shared memory databases to connected by a self-
  serving client, it is necessary to allow each client to attach
  each database relative to its own virtual address space. 

  To facilitate this, the concept of shared pointers must be established.
  A shared pointer contains a index in a private segment table and
  a offset into the particular segment.  Whenever a process wants to
  refer to a element in shared memory, it has to transform a shared pointer
  into a real pointer.  Moreover, whenever a process wants to set
  a element is shared memory, it has to transform a real pointer into
  a shared pointer. (SEE shptr.h for pointer transformation )

  The segment table contains a list of segment addresses that are relative
  to the particular process' virtual address space.  To allow for
  quicker shared pointer -> real pointer transformation, the slot # of
  the segment table is encoded into the segment address. For example,
  if the segment address is 0x120000ff is located in slot #1 of the
  segment table, it will be stored as:

           segaddr    - (slot#<<24) = segment table entry
           0x120000ff - (1<<24) = 0x110000ff

  To handle NULL pointers, slot #0 is reserved and set to zero.
  Because it cannot always be determined whether data is private
  or public,it is necessary, to reserve MAXPRIVSLT slots in segment table
  for referencing private storage.  The use of public slots grow up, and
  the use of private slots grows down.

         
         SEGMENT TABLE

         +--------------+
	 |  reserved    | <- #0 used for NULL pointers 
         +--------------+
         |              |
	 |     ^        |
         |     |        |
         |     |        |
         |private slots | <- MAXPRIVSLT
         +--------------+  
	 | public slots |
         |     |        |
	 |     |        |
	 |     v        |
         |              |
         |              | <- MAXSGTBL
         +--------------+

***********************************************************************/


/* PROGRAM: shmPtoQP - convert real pointer to shared pointer 
 *
 * RETURNS: the shared pointer
 */
SHPTR
shmPtoQP(
        dsmContext_t *pcontext,
        SEGADDR       sgaddr)      /* segment address of real pointer */
{
    dbcontext_t          *pdbcontext   = pcontext->pdbcontext;
    SEGADDR          *psgtbl   = pdbcontext->pshsgctl->sgtbl;
    shmTable_t       *pshsgctl = pdbcontext->pshsgctl;
    SEGADDR           sgnorm;	/* normalized segment address */
    SEGADDR           asgtbl;	/* decoded segment address */
    SEGADDR           isgtbl;	/* index into segment table */
    SHPTR             qtr;	/* shared pointer */
    unsigned_long     i;	/* needs to be this size to work around
				   problems with shift  */
    SHPTR             seg;	/* segment portion of shptr */
    SHPTR             ofst;	/* offset portion of shptr */
    ULONG            *psglen = pdbcontext->pshsgctl->sglen;
                
    if(!pshsgctl)
    {
	/* pshsgctl not init`d before shmPtoQ call */
	FATAL_MSGN_CALLBACK(pcontext, utFTL017);
    }

    sgnorm = (sgaddr & ~OFSTMASK) ;

    /* handle NULL pointers */
    if(sgaddr == 0)
	return (SHPTR)NULL;
 
    /* if single-user or before shared memory has been created/attached
       use slot #0 */

    if(sgnorm == 0 && 
       (pdbcontext->pshsgctl->ipub == MAXPRIVSLT +1))
	isgtbl = 0;
    else    /* find the largest slot in segment table < sgaddr */
    {
	for(i=(ULONG)pshsgctl->ipub-1,isgtbl=0;i>(ULONG)pshsgctl->iprv;i--)
	{
	    asgtbl = psgtbl[i] + (i<<SHSGSHFT);
	    if ((sgaddr >= asgtbl) && (sgaddr <= (asgtbl + (psglen[i] - 1))) )
            {
                isgtbl = i;
                break;
            }
	}
	
        if (isgtbl == 0) /* if ptr is not within an existing segment */
            isgtbl = shmPutSegmentAddress(pcontext, sgaddr,(COUNT)PRIVATE,0L,
					  pdbcontext->pshsgctl);
    }
    	    
    /* create the proper shared pointer */
    seg = (isgtbl << SHSGSHFT) & ~ OFSTMASK;
    asgtbl = psgtbl[isgtbl] + (isgtbl<<SHSGSHFT);
    ofst = (sgaddr - asgtbl) & OFSTMASK;
    qtr = seg + ofst;
    
    return qtr;

}  /* end shmPtoQp */


/* PROGRAM: shmCreateSegmentTable - initialize segment table for shared memory
 *                                  references
 *
 * RETURNS: Pointer to created segment table
 */
shmTable_t *
shmCreateSegmentTable(STPOOL *pstpool /* pointer to storage pool */)
{
    shmTable_t  *pshsgctl;	/* pointer to segment control */
    
    pshsgctl = (shmTable_t * )stGet(NULL, pstpool,sizeof(shmTable_t));
		/* Rich T. claims that we do not need pcontext here */

    stnclr((TEXT*)pshsgctl,sizeof(shmTable_t));
    pshsgctl->ipub = MAXPRIVSLT + 1; /* initialize start of public slots */
    pshsgctl->iprv = MAXPRIVSLT;     /* initialize start of private slots */
    
    return pshsgctl;
}


/* PROGRAM: shmPutSegmentAddress - add segment address to segment table 
 * segno indicated which slot in segment table to use:
 * PRIVATE - use slots reserved for private pointers 
 * # - use public slot indicated by segno
 *
 * RETURNS:  index into segment table
 */
ULONG
shmPutSegmentAddress(
        dsmContext_t  *pcontext,
        SEGADDR        segaddr,    /* segment address */
        COUNT          segno,      /* segment number = PRIVATE, or # */
        LONG           size,       /* size of the segment the slot points to */
        shmTable_t    *pshsgctl /* Ptr to segment address table       */
     )
{
    SEGADDR      isgtbl;	/* index into (VLM) segment table */
    
    switch (segno)
    {
      case PRIVATE:
	if(pshsgctl->iprv == 0)
	{
	    /* # of private slots in segment table exhausted */
	    FATAL_MSGN_CALLBACK(pcontext, utFTL016);
	    return 0;
	}
	isgtbl = pshsgctl->iprv--;
    	break;
      default:
	isgtbl = segno + MAXPRIVSLT + 1;
        /* if slot has not been used - set nxt public slot to proceeding
	   slot */
        if(pshsgctl->sgtbl[isgtbl] == 0)
            /* BUM - Assuming isgtbl < 2^15 */
	    if(pshsgctl->ipub <= (COUNT)isgtbl)
		pshsgctl->ipub = (COUNT)isgtbl + 1;
	break;
    }

    /* if PRIVATE slot - mask lower 3 bytes and encode slot #
       else encode slot # into segment address# */
    if(segno == PRIVATE)
    {
	pshsgctl->sgtbl[isgtbl] = 
	    (segaddr & ~OFSTMASK) - (isgtbl << SHSGSHFT);

        /* private slots are the largest possible size  */
	pshsgctl->sglen[isgtbl] = (1 << SHSGSHFT);  
    }
    else
    {
	pshsgctl->sgtbl[isgtbl] = segaddr - (isgtbl << SHSGSHFT);

        /* if the size was passed in, as in the case when the shared memory
           is first created, use that value */
        if (size > 0)
            pshsgctl->sglen[isgtbl] = (ULONG)size;
        else
            pshsgctl->sglen[isgtbl] = 
                (ULONG)((SEGMNT *)segaddr)->seghdr.segsize;
    }

    return (ULONG)isgtbl;
}

#endif				/* #if SHMEMORY */
