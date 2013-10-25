
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

#include "dbconfig.h"

#if SHMEMORY 

#include "shmpub.h"
#include "shmprv.h"
#include "stmpub.h"
#include "stmprv.h"
#include "dbcontext.h"

#include "dbpub.h"

#if OPSYS==UNIX
#include <unistd.h>
#endif

#if OPSYS==WIN32API
#include <process.h>  /* getpid() */
#endif

/* PROGRAM: shmapid - allocate a pidlist to hold list of user pids */
DSMVOID
shmapid(
        dsmContext_t *pcontext,
        SHMDSC       *pshmdsc, 	/* describes the shmemory to put it in	 */
	SEGMNT       *pseg,    	/* put the pidlist in this segment	 */
	int           numusrs) 	/* allocate this many slots in the table */
{
    /* get storeage in shared memory for the list of PIDs using it */
    /* NOTE, this allocs 1 extra slot because slot 0 will be skippd*/
    pshmdsc->pidlist = (PIDLIST *)
	stGet(pcontext, pshmdsc->ppool,
	      sizeof(PIDLIST) + numusrs*sizeof(pshmdsc->pidlist->pid));

    pseg->seghdr.qidlist = P_TO_QP(pcontext, pshmdsc->pidlist);
    pshmdsc->pidlist->maxpids = numusrs;
}

/* PROGRAM: shmpid - Put my pid into the list of pids using this sh mem  */
/* RETURNS: 0 = OK, non-0 = No more slots in the pid table		 */
int
shmpid(SHMDSC *pshmdsc) 	/* describes the shmemory to put it in */
{
    int          i;
    PIDLIST	*pidlist;

    pidlist = pshmdsc->pidlist;

    /* this skips slot 0, so that (pshmdsc->pidslot==0) can be used */
    /* as a flag to indicate that this user's pid isnt in pidlist   */
    for (i=1; pidlist->pid[i] > 0; i++)
	if (i > pidlist->maxpids) return -1;
    pidlist->pid[i] = getpid();
    pshmdsc->pidslot = i;
    return 0;
}


/* PROGRAM: shmqpid - Query whether any active pids use this sh mem	*/
/* RETURNS: The number of current live users				*/
int
shmqpid(
        dsmContext_t *pcontext,
        SEGMNT *pseg)    /* points to the segment with the pidlist */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
FAST	int	 i;
	int	 numusrs = 0;
	PIDLIST	*pidlist;
FAST	int	*ppid;

    /* skip pidlist->pid[0] because it isnt used */
    for(pidlist = XPIDLIST(pdbcontext, pseg->seghdr.qidlist),
	i=pidlist->maxpids, ppid=pidlist->pid+1; i>0; i--, ppid++)
	if (   *ppid > 0
	    && deadpid(pcontext, *ppid)==0 ) numusrs++;
    return numusrs;
}

/* PROGRAM: shmrpid - Remove my pid from the list of pids using sh mem */
DSMVOID
shmrpid(SHMDSC	*pshmdsc)  /* describes the shmemory containing the pidlist */
{
    if (   pshmdsc->pfrstseg == NULL
	|| pshmdsc->pidslot == 0) return;
    
    pshmdsc->pidlist->pid[pshmdsc->pidslot] = 0;
    pshmdsc->pidslot = 0;
}

#endif
