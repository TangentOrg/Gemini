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
#include <errno.h>
#include "dbconfig.h"

#if SHMEMORY

#include "shmpub.h"
#include "shmprv.h"
#include "sempub.h"
#include "dbcontext.h"
#include "dbmsg.h"
#include "dsmpub.h"
#include "utmsg.h"

#include "utfpub.h"
#include "uttpub.h"

#if OPSYS==WIN32API
#include <windows.h>
#endif /* WIN32API */

#if OPSYS==WIN32API

/* PROGRAM: shmDeleteSegment - delete a shared memory segment */
int shmDeleteSegment(dsmContext_t *pcontext,
                     TEXT *pname, /* the database name or other file name*/
		     int  idchar) /* id number for segment within db     */
{
    char 	szShareMem[MAXUTFLEN];
    char 	szFile[MAXUTFLEN];
    LPVOID 	lpmem;
    HANDLE	hmap;

    utapath(szFile, MAXUTFLEN, pname, "");
    utmkosnm("sharemem.", szFile, szShareMem, MAXUTFLEN, idchar);
    if ((lpmem = (LPVOID)utshmGetNamedSegment(szShareMem)) == (LPVOID)-1)
    	return -1;

    hmap = (HANDLE)utshmGetUserData((int)lpmem);

    /*
     * Remove table reference to the segment name
     */
    utshmFreeNamedSegment(szShareMem);
    UnmapViewOfFile(lpmem);
    
    if (hmap != NULL && hmap != INVALID_HANDLE_VALUE)
    	CloseHandle(hmap);

    return 0;
}

#endif /* OPSYS==WIN32API */

#if OPSYS==UNIX 

/* PROGRAM: shmDeleteSegment - delete a shared memory segment */
int shmDeleteSegment(dsmContext_t  *pcontext,
                     TEXT *pname _UNUSED_, /*database name or other file name*/
		     int  shmid) /* id number for segment within db     */
{
    int   rc;
    
    if (shmid < 0) return shmid;   /* bad */

    rc = shmctl(shmid, IPC_RMID, (struct shmid_ds *)NULL);
    if(rc)
    {
        LONG localErrno = errno;
        
        MSGD_CALLBACK(pcontext,
       "%LSYSTEM ERROR: removing shared memory with segment_id: %l errno = %l",
                      (LONG)shmid,localErrno);
    }
    else
        MSGD_CALLBACK(pcontext,
                      "%LRemoved shared memory with segment_id: %l",
                      shmid );
    return rc;
}
#endif /* OPSYS==UNIX */

#if OPSYS==UNIX || OPSYS==WIN32API
/* PROGRAM: shmDeleteSegments - delete the shared memory associated 
   with a db */
DSMVOID
shmDeleteSegments(
        dsmContext_t *pcontext,
        SHMDSC       *pshmdsc)/* describes the segments to delete */
{
	int	 segno;
	int	 numsegs;

#if OPSYS==UNIX
       SEGMNT *pseg = NULL;
       int shmid;
#endif


    if (pshmdsc == NULL) return;	/* never initialized at all */
    
    /* The broker wont delete the shared memory until all users */
    /* have detached it.  This prevents unpleasant effects	*/
    while(   pshmdsc
	  && pshmdsc->pfrstseg
	  && shmqpid(pcontext, pshmdsc->pfrstseg) > 0)
	utsleep(5);


#if OPSYS==UNIX
    /* the broker should then delete the segments */
    for(segno=0, numsegs=pshmdsc->numsegs; segno < numsegs; segno++)
    {
        /* get the correct shmid to for the segment to delete */
        if (segno == 0)
        {
            pseg = (pshmdsc->pfrstseg);
            shmid = pseg->seghdr.shmid;
        }
        else
        {
            shmid = pseg->seghdr.qnextshmid;
        }
        /* delete the segment */
        shmDeleteSegment(pcontext,pshmdsc->pname, shmid);
    }
#else
    /* the broker should then delete the segments */
    for(segno=0, numsegs=pshmdsc->numsegs; segno < numsegs; segno++)
	shmDeleteSegment(pcontext,pshmdsc->pname, segno);
#endif

}
#endif /* OPSYS==UNIX || OPSYS==WIN32API */

#endif /* SHMEMORY */




