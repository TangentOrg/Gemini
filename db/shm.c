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

/**********************************************************************
*
*	C Source:		shm.c
*	Subsystem:		1
*	Description:	
*	%created_by:	richt %
*	%date_created:	Fri Nov 18 10:37:55 1994 %
*
**********************************************************************/

/* dbshmem.c - Shared memory routines which are specific to our dbmanager */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#if SHMEMORY

#include "dbcontext.h"
#include "dsmpub.h"
#include "mtmsg.h"
#include "sempub.h"
#include "shmprv.h"
#include "shmpub.h"
#include "stmpub.h"
#include "stmprv.h"
#include "utmsg.h"

#include <errno.h>


/* PROGRAM: shmAttach - Attach to an existing set of shared mem segments */
/* RETURNS: 0=OK, non=0 = not OK					*/
int
shmAttach(dsmContext_t *pcontext)	/* attach for this database */
{
	dbcontext_t *pdbcontext = pcontext->pdbcontext;
	SHMDSC	*pshmdsc;

    pshmdsc = (SHMDSC *)stGet(pcontext, pdbcontext->privpool, sizeof(SHMDSC));
    pshmdsc->pname   = pdbcontext->pdbname;	

    /* attach all the segments */
    if (shmattsegs(pcontext, pshmdsc) != 0) return -1;
    pdbcontext->pshmdsc = pshmdsc;

    /* find out where the dbcontext is (the anchor for all shared memory info) */
    if (pdbcontext->privdbpub)
    {	stVacate(pcontext, pdsmStoragePool, (TEXT *)pdbcontext->pdbpub);
	pdbcontext->privdbpub = 0;
    }
    pdbcontext->pdbpub = (dbshm_t *)XTEXT(pdbcontext, pshmdsc->pfrstseg->seghdr.qmisc);

    return 0;
}
#if OPSYS != VMS
/* PROGRAM: shmDetach - Detach a set of shared memory segments */
DSMVOID
shmDetach(dsmContext_t *pcontext)     /* detach the ones for this database */
{
	dbcontext_t *pdbcontext = pcontext->pdbcontext;
	SHMDSC	*pshmdsc = pdbcontext->pshmdsc;

    pdbcontext->pshmdsc = NULL;	/* stop infinite loops */
    if (pshmdsc)
	shmDetachSegments(pcontext, pshmdsc);
}
/* PROGRAM: shmDelete - Delete a set of shared memory segments */
DSMVOID
shmDelete(dsmContext_t *pcontext) 	/* delete the ones for this database */
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    SHMDSC	*pshmdsc = pdbcontext->pshmdsc;

    pdbcontext->pshmdsc = NULL;	/* stop infinite loops */
    if (pshmdsc)
    {
	shmDetachSegments(pcontext, pshmdsc);
    }
}
#endif

/* PROGRAM: shmCreate - Called by broker to create shared memory for db */
/* RETURNS: 0=OK, non=0 = not OK                                        */
/* Parameters: dbcontext_t *pdbcontext         describes which database */
/*             bkIPCData_t *pbkIPCData         holds shmid data         */ 
int
#if OPSYS==UNIX
shmCreate(dsmContext_t *pcontext, bkIPCData_t *pbkIPCData) 
#else
shmCreate(dsmContext_t *pcontext) 
#endif
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    int         shmid;
    int         numsegs;
    int         segno;
    int         segsNeeded = 0;
    SHMDSC      *pshmdsc;
    SEGMNT      *pseg;
    VLMsize_t   total_share;    /* total amount of shared memory needed */
    VLMsize_t   alloc_share;    /* total amount of shared mem allocated */
    
    pshmdsc = (SHMDSC *)stRent(pcontext, pdbcontext->privpool, sizeof(SHMDSC));
    
    /* the shared memory's name is the database name */
    pshmdsc->pname   = pdbcontext->pdbname;
    pshmdsc->numusrs = pdbpub->argnusr;
    pseg = NULL;
    
    /* check if the shared memory already exists and if it is in use */
#if OPSYS == UNIX
    /* UNIX uses the mstrblk to hold possible previous shmids */
    shmid = pbkIPCData->OldShmId;
#else
    /* WIN32API uses the dbname to identify shmids */
    shmid = shmGetSegmentId(pcontext, pshmdsc->pname, 
        0, 0/*dont print msg if ENOENT */);
#endif
    
    /* Does shared memory already exist for this db server? */
    if (shmid != -1)
    {
        /* If we get here it appears that shared memory may still exist,
        let's make sure by trying to attach */
        pseg = (SEGMNT *)shmVerifySegment(pcontext, shmid);
        /* If pseg is NULL shared memory doesn't exist,
        let's insure this by setting shmid = -1 */  
        if (pseg == NULL) shmid = -1;
    }
    
    /* Shared memory does exist, is it in use? */
    if (shmid >= 0)
    {
        /* attach to the segment and verify that it has our magic number
        in it if we don't have a valid segment */
        if (pseg == NULL)
           pseg = (SEGMNT *)shmVerifySegment(pcontext, shmid);
        if (pseg == NULL)
        {
            /* If it does not have our magic number, bail out */
            MSGN_CALLBACK(pcontext, utMSG037);
            return -1;
        }
        /* If it does have our magic number, add the segment address to
        the segment table */
        shmPutSegmentAddress(pcontext, (SEGADDR)pseg,0,0,pdbcontext->pshsgctl);
        
        /* Do any active pids use this shared memory? */
        if (shmqpid(pcontext, pseg) > 0)
        {   /* yes, it is still in use */
            if (pdbcontext->argronly) /* if read only database */
            {
                /* Warning to read-only users */
                MSGN_CALLBACK(pcontext, mtMSG004);
                stVacate(pcontext, pdbcontext->privpool, (TEXT *)pshmdsc);
                return 0;
            }
            else
            {
                MSGN_CALLBACK(pcontext, utMSG044); /* shm already in use */
                return -1;
            }
        }
        
        /* it isnt in use, delete it */
        /* the broker should then delete the segments */
#if OPSYS==UNIX
        /* Before we delete the shared memory segments, get rid of
        the associated semaphores.  This will guarentee that
        the semaphores removed are the ones associated with this
        database */
        
        semCleanup(pcontext, pseg);
        
        for(segno=0, numsegs=pseg->seghdr.numsegs; segno < numsegs; segno++)
        {
            if (segno == 0)
            {
                /* if this is the first seg take the shmid */
                shmid = pseg->seghdr.shmid;
            }
            else
            {
                /* if this isn't the first seg take the next shmid */
                shmid = pseg->seghdr.qnextshmid;
            }
            
            shmDeleteSegment(pcontext,pshmdsc->pname, shmid);
            
        }
        shmDetachSegment(pcontext, pseg);

#else /* WIN32API */

        numsegs = pseg->seghdr.numsegs;
        shmDetachSegment(pcontext, pseg);
        for(segno=0; segno < numsegs; segno++)
        {
            if (shmDeleteSegment(pcontext,pshmdsc->pname, segno) != 0)
            {
                /* If we can't delete the segment some other non-progress
                process is holding it open */
                MSGN_CALLBACK(pcontext, utMSG121);
                return -1;
            }
        }

#endif
        
    }
    if (pdbcontext->argnoshm) 
    {
        stVacate(pcontext, pdbcontext->privpool, (TEXT *)pshmdsc);
        return 0;
    }
    
    /* get the total amount of shared memory wanted, attempt to allocate it */ 
    /* first, if only a subset is allocated, keep going */ 
    total_share = shmGetShmSize(pcontext, (LONG)pdbcontext->pdbpub->dbblksize);
    
    if ( total_share == 0 )
    {
        /* Decrease -B or -L */
        return -1;
    }
    
    for(alloc_share = 0; alloc_share < total_share;   
    /* add actual share allocated */
    alloc_share += pshmdsc->segsize)
    {
        /* when allocating multiple segments, don't allocate too much on */ 
        /* last segment, or exceed MAXSEGSIZE */
        VLMsize_t unalloc_share = total_share - alloc_share;
        pshmdsc->segsize = unalloc_share < MAXSEGSIZE   ? unalloc_share
            : MAXSEGSIZE;
        
        /* now, create a new segment and build a STPOOL for the database */
        /* this program will attempt to allocate pshmdsc->segsize */ 
        if (shmFormatSegment(pcontext, pshmdsc) == NULL)
        {
            goto errorReturn;
        }
        if (segsNeeded == 0)
        {
            /* First segment created will cause segsize to be
               set to less than MAXSEGSIZE if the SHMMAX kernel parm
               is less than the max we permit.                       */
            segsNeeded = total_share / pshmdsc->segsize;
            if (total_share % pshmdsc->segsize)
                segsNeeded++;
            if( segsNeeded > MAX_SHARED_SEGMENTS)
            {
                MSGD_CALLBACK(pcontext,
    "%BNeed %l shared memory segments which exceeds Progress maximum of %l",
                          segsNeeded, MAX_SHARED_SEGMENTS);

                goto errorReturn;
            }            
        }
    }
    
    /* relocate the dbpub structure into share memory */
    pdbpub = (dbshm_t *)stGet(pcontext, pshmdsc->ppool, sizeof(dbshm_t));
    bufcop(pdbpub, pdbcontext->pdbpub, sizeof(dbshm_t));
    
    /* Free the private copy of the dbshm */
    stVacate(pcontext, pdsmStoragePool, (TEXT *)pdbcontext->pdbpub);
    pdbcontext->privdbpub = 0;
    pdbcontext->pdbpub = pdbpub;
    pshmdsc->pfrstseg->seghdr.qmisc = (QTEXT)P_TO_QP(pcontext, pdbpub);
    pdbpub->qdbpool = P_TO_QP(pcontext, pshmdsc->ppool);
    
    pdbcontext->pshmdsc = pshmdsc;
    
#if OPSYS==UNIX
    /* store the shmid to be placed in mstrblk */
    pbkIPCData->NewShmId = pshmdsc->pfrstseg->seghdr.shmid;
#endif
    
    return 0;
errorReturn:
    if (pdbcontext->pshmdsc == NULL) 
        pdbcontext->pshmdsc = pshmdsc;
    shmDelete(pcontext);
    return -1;
}

/* Program: shmSegArea - returns size less ABK overhead of shared
                          memory segments.                           */
LONG
shmSegArea(dbcontext_t *pdbcontext)
{
    return pdbcontext->pshmdsc->shmarea;
}

#endif  /* if SHMEMORY */













