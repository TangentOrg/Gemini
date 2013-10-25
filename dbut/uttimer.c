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

/* Table of Contents - timer utility functions:
 *
 * utevt     - Requests timer control to turn pflag on after secs (UNIX)
 * utsleep   - Sleeps for secs seconds (UNIX).
 * utnap     - sleep for some number of milliseconds.
 * utcncel   - cancel all pending events which set the passed flag         
 * utwait    - waits for an already defined event to occur
 * utrdusclk - read the microsecond clock
 *
 */

/*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#if OPSYS==UNIX
/* must come before dbconfig.h in order to avoid conflicts with DFILE */
#include <stdio.h>
#endif


#include "dbconfig.h"
#include "dbutret.h"
#include "uttpub.h"

#if OPSYS == UNIX
#include <sys/time.h>
#if !HPUX
#include <sys/select.h>
#endif
#endif

#if OPSYS==WIN32API
#include "latpub.h"
#endif

#if OPSYS==UNIX 
#include <signal.h>
#include <unistd.h>
#endif


#if NAP_TYPE==NAP_POLL
#include <sys/poll.h>
#if !ALPHAOSF && !IBMRIOS && !HPUX
int poll(struct pollfd *,unsigned long nfds,int timeout);
#endif  /* ALPHAOSF */
#endif  /* NAP_POLL */

#if OPSYS==UNIX 

/*********************************************/
/* structures to hold pending timer requests */
/*********************************************/

struct timrrqst {unsigned  tr_secs;     /* secs to wait before flag evt */
                 GTBOOL    *ptr_evt;     /* pts to bool to set after time*/
                };
struct timrctl  {COUNT            numpend;    /* number of pending evts */
                 struct timrrqst *plastrqst; /* pts past pending evt    */
                 struct timrrqst  tmrqst[MAXTIMR];  /* pending evts     */
                };


LOCAL struct timrctl _timrctl;
LOCAL struct timrctl *p_timrctl = &_timrctl;
LOCAL int timer_initted = 0;

/*************************************************************************/
/* PROGRAM: utevt - Requests timer control to turn pflag on after secs
* 
* RETURNS: DBUT_S_SUCCESS
*          DBUT_S_TOO_MANY_PENDING_TIMERS
*          DBUT_S_INVALID_WAIT_TIME
*/
LONG
utevt(int    secs,
      GTBOOL *pflag)
{
    unsigned		 usecs = secs;
    unsigned		 prevsecs;
    unsigned		 nextsecs;
    struct timrrqst	*ptimrrqst;
    struct timrctl	*ptimrctl = p_timrctl;
    
    /* initialization done for first call only*/
    if (!timer_initted)
    {
        timer_initted = 1;
        memset(ptimrctl, 0, sizeof(struct timrctl));
        ptimrctl->plastrqst = ptimrctl->tmrqst;
    }
    
    /* request to wait for 0 seconds? */
    if (secs==0)
    {
        *pflag = 1;
        return DBUT_S_INVALID_WAIT_TIME;
    }
    
    /* is the table too full? */
    if (ptimrctl->numpend >= MAXTIMR)
    {
        return DBUT_S_TOO_MANY_PENDING_TIMERS;
    }
    
    /* save the current status of the clock, reduce the current pending */
    /* request's waiting time, & in the unlikely event that it went to	*/
    /* zero, call the alarm handler to flag and remove it		*/
    if (ptimrctl->numpend != 0)
        if ( (ptimrctl->tmrqst[0].tr_secs=alarm((unsigned)0)) == 0)
        utalsig(0);	/* call alarm handler */
    
    /* scan the table to find out where the new event belongs */
    for(prevsecs=0, nextsecs=0, ptimrrqst = ptimrctl->tmrqst;
    ptimrrqst < ptimrctl->plastrqst;
    prevsecs=nextsecs, ptimrrqst++)
    {
        /* if request is bigger than this table entry, keep scanning */
        nextsecs += ptimrrqst->tr_secs;
        if (usecs > nextsecs)
            continue;
        
        /* found where new request belongs between two others, make room */
        ptimrrqst->tr_secs = nextsecs - usecs;	 /* update to be difference*/
        bufcop((TEXT *)(ptimrrqst+1), (TEXT *)ptimrrqst,
            (TEXT *)ptimrctl->plastrqst - (TEXT *)ptimrrqst);
        break;
    }
    /* now build the new timer request entry */
    ptimrrqst->tr_secs = usecs-prevsecs; /*always keep diff between evts*/
    ptimrrqst->ptr_evt = pflag;
    ptimrctl->numpend++;
    ptimrctl->plastrqst++;
    
    /* and remember to wake up for nearest event */
    alarm(ptimrctl->tmrqst[0].tr_secs);

    return DBUT_S_SUCCESS;
}

/**********************************************************/
/** PROGRAM: utcncel - cancel all pending events	 **/
/**	      which set the passed flag			 **/
/**********************************************************/

DSMVOID
utcncel(GTBOOL *pflag)
{
    struct timrrqst	*ptimrrqst;
    struct timrctl	*ptimrctl = p_timrctl;
     unsigned		 addsecs;

    if (ptimrctl->numpend == 0)
        return;  /* no events pending */

    /* save the current status of the clock, reduce the current pending */
    /* request's waiting time, & in the unlikely event that it went to	*/
    /* zero, call the alarm handler to flag and remove it		*/

    if ( (ptimrctl->tmrqst[0].tr_secs = alarm((unsigned)0)) == 0)
	    utalsig(0);  /* call alarm handler */

    /* scan table for event that sets pflag */

    for(ptimrrqst = ptimrctl->tmrqst;
	ptimrrqst < ptimrctl->plastrqst;
	ptimrrqst++)

	if (ptimrrqst->ptr_evt == pflag)
	{
	    /* delete event, fix up subsequent events times */
	    addsecs = ptimrrqst->tr_secs;
	    bufcop( (TEXT *)ptimrrqst, (TEXT *)(ptimrrqst+1),
		    (TEXT *)ptimrctl->plastrqst - (TEXT *)ptimrrqst);
	    ptimrctl->plastrqst--;
	    ptimrctl->numpend--;
	    for ( ;
		 ptimrrqst < ptimrctl->plastrqst;
		 ptimrrqst++)
		     ptimrrqst->tr_secs += addsecs;
	    break;
	}

    /* and remember to wake up for nearest event */
    if (ptimrctl->numpend)
	alarm(ptimrctl->tmrqst[0].tr_secs);

}  /* end utcncel */

#ifndef utalsig
/**************************************************************/
/** PROGRAM: dbut_utalsig - Internal use only. It receives control**/
/**		whenever the internal timer rings.	     **/
/**************************************************************/
DSMVOID
dbut_utalsig(int signum)
{
    struct timrctl	*ptimrctl = p_timrctl;
    struct timrrqst	*ptimrrqst;

#if USE_SIGNAL
    /* We don't ever expect to be here anymore since sigaction is
       the norm.  Generate a compile error should we get here, 1-7-98 */

    gronk;
    if (signum != 0)
        drSigResAlm ();

#endif /* if USE_SIGNAL */

    ptimrrqst = ptimrctl->tmrqst;  /* point to first request in table */
    ptimrrqst->tr_secs = 0;		/* it has expired */

    /* eliminate and flag all expired requests from the table  */
    while(ptimrctl->numpend > 0 && ptimrrqst->tr_secs == 0)
    {
	/* set flag */
	*ptimrrqst->ptr_evt = 1;

	ptimrctl->plastrqst--;
	ptimrctl->numpend--;
	bufcop( (TEXT *)ptimrrqst, (TEXT *)(ptimrrqst+1),
		(TEXT *)ptimrctl->plastrqst - (TEXT *)ptimrrqst);
    }

    /* restart the clock again */
    if (   ptimrctl->numpend > 0
	&& signum != 0)	/* signum==0 is an internal call from utevt */
			/* who will restart the timer himself	    */

	alarm(ptimrrqst->tr_secs);

}  /* end dbut_utalsig */
#endif

/**************************************************************/
/** PROGRAM: utalsig - Internal use only. It receives control**/
/**		whenever the internal timer rings.	     **/
/**************************************************************/
DSMVOID
utalsig(int signum)
{
    struct timrctl	*ptimrctl = p_timrctl;
    struct timrrqst	*ptimrrqst;

#if USE_SIGNAL
    /* We don't ever expect to be here anymore since sigaction is
       the norm.  Generate a compile error should we get here, 1-7-98 */

    gronk;
    if (signum != 0)
        drSigResAlm ();

#endif /* if USE_SIGNAL */

    ptimrrqst = ptimrctl->tmrqst;  /* point to first request in table */
    ptimrrqst->tr_secs = 0;		/* it has expired */

    /* eliminate and flag all expired requests from the table  */
    while(ptimrctl->numpend > 0 && ptimrrqst->tr_secs == 0)
    {
	/* set flag */
	*ptimrrqst->ptr_evt = 1;

	ptimrctl->plastrqst--;
	ptimrctl->numpend--;
	bufcop( (TEXT *)ptimrrqst, (TEXT *)(ptimrrqst+1),
		(TEXT *)ptimrctl->plastrqst - (TEXT *)ptimrrqst);
    }

    /* restart the clock again */
    if (   ptimrctl->numpend > 0
	&& signum != 0)	/* signum==0 is an internal call from utevt */
			/* who will restart the timer himself	    */

	alarm(ptimrrqst->tr_secs);

}  /* end utalsig */


/*******************************************************************/
/** PROGRAM: utwait - waits for an already defined event to occur **/
/**          must call utevt before calling utwait                **/
/*******************************************************************/
DSMVOID
utwait(GTBOOL *pflag)
{
    while(*pflag==0)
        pause();
}


#endif  /* UNIX */


#if OPSYS==WIN32API
LONG
utevt(int    secs,
      GTBOOL *pflag)
{
    return 0;
}

DSMVOID
utcncel(GTBOOL *pflag)
{
}

#endif  /* OPSYS==WIN32API */


/*************************************************************************/
/* PROGRAM: utsleep - Sleeps for secs seconds.
*	     This replaces sleep(secs) which
*	     which cannot be used if this timer manager is being used
*
* RETURNS: DSMVOID
*/
DSMVOID
utsleep(int secs)
{
#ifndef PRO_REENTRANT
    GTBOOL	flag = 0;
#endif
    
#if OPSYS==UNIX 
#ifdef PRO_REENTRANT
    struct      timespec timeSpec;
#endif
#endif
    
    if (secs==0)
        return;

#if OPSYS==UNIX 

#ifdef PRO_REENTRANT
    /* Perform a thread specific sleep */
    timeSpec.tv_sec  = secs;
    timeSpec.tv_nsec = 0;
    nanosleep(&timeSpec, (struct timespec *)0);

#else   /* do it the old way with signals */

    utevt(secs, &flag);	/* create an event */
    while(flag==0)
        pause();	/* wait for it	   */

#endif  /* PRO_REENTRANT */
#endif /* UNIX */

#if OPSYS==WIN32API
    
    /*
    * Let the system see what the rest of the world is doing
    * once every 30 seconds or so.
    */
    while (secs > 0)
    {
        Sleep(MIN(secs,30) * 1000);
        secs -= 30;

    }
#endif  /* WIN32API */

}  /* end utsleep */


/* PROGRAM: utnap - sleep for some number of milliseconds. Not available
*	if NAP_TYPE==NAP_NONE in dstd.h (very small number of machines).
*
* RETURNS: DSMVOID
*/
DSMVOID
utnap (int milliSeconds)
{
    int	ret = 0;
    
#if NAP_TYPE==NAP_SELECT && BSDSOCKETS
    struct timeval waitTime;
#endif

#if NAP_TYPE==NAP_POLL
    struct pollfd	fds;
#endif


#if ASSERT_ON
    if (milliSeconds == 0) printf("utnap %i", milliSeconds);
#endif
    /*************************/
    
   /* Use one of the methods below to sleep for a short time **************** */
    
#if NAP_TYPE==NAP_POLL
#define NAP_DEFINED 1
    /* Use poll when available. It is the preferred over select because
    some systems do not have select in the kernel unless the networking
    option is installed and that adds a lot of other stuff, so people
    who don't need networking don't always install it.
    Poll is available on newer System V's and some Berzerkely */
    
    fds.fd = -1;
    ret = poll (&fds, 1L, (int)milliSeconds);
#endif
    
    /* **************** */
    
#if NAP_TYPE==NAP_SELECT 
#define NAP_DEFINED 1
    /* If poll is not available then use select when the select is
    native to the OS. See note above on poll.
BEWARE: select is broken on some systems. Notably AIX. */
    
#if BSDSOCKETS
    waitTime.tv_usec = (LONG)(milliSeconds % 1000) * 1000;
    waitTime.tv_sec = milliSeconds / 1000;
    ret = select (0L, (DSMVOID *)NULL, (DSMVOID *)NULL, (DSMVOID *)NULL,
                  (struct timeval *)&waitTime);
#endif
    
#if MCPSOCKETS
    ret = select (0, PNULL, PNULL, (LONG)milliSeconds);
#endif
        
#endif /* NAP_SELECT */
    
    /* **************** */
    
#if NAP_TYPE==NAP_NAPMS
#define NAP_DEFINED 1
    /* This is the least preferred mthod because napms is unreliable,
    undocumented and unsupported. Sometimes it is the only method
    available so we have no choice. It can usually be found in the
    "curses" library when available. It does not always work */
    
    napms ((int)milliSeconds);
#endif
    
    /* **************** */
    
#if NAP_TYPE==NAP_OS && OPSYS==WIN32API
#define NAP_DEFINED 1
    Sleep((DWORD)milliSeconds);
#endif
    
    /* **************** */
    
    /* *** One of the above must have been chosen *** */
#ifndef NAP_DEFINED
    printf("Gronk: no utnap");
#endif

}  /* end utnap */


#if HAS_US_TIME

#if SUN4 || HP825
/* use the gettimeofday function. */
 
#include <sys/time.h>
 
/* PROGRAM: utrdusclk - read the microsecond clock
 *
 * RETURNS:
 *              Unsigned 32 bit time in microseconds from some unknown
 *              time in the past.
 *              Rolls over in about 34.1 min
 */
ULONG
utrdusclk ()
{
        struct  timeval t;
        ULONG   us;
 
    gettimeofday (&t, PNULL);
 
    us = (((ULONG)t.tv_sec & (ULONG)2047) * (ULONG)1000000) + (ULONG)t.tv_usec;

    return (us);

}  /* end utrdusclk */

#endif /* SUN4 || HP825 */

#endif /* HAS_US_TIME */

