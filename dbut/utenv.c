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

/* Table of Contents - environment specific utility functions:
 *
 * utgethostname      - get the current hostname and place it into
 *                      the passed buffer.
 * utgetpid           - Returns the process id for the current process.
 * uttty              - get ttyname
 * utGetStdHandleType - gets handle for stdin, stdout, stderr (NT only).
 * utDetermineVersion - Determine whether this is NT or win9x
 * utuid              - determine user's id
 * utCheckTerm        - this checks to see if the process has a terminal
 * utgetenvwithsize   - Get the environment variable
 *
 */

/*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include <stdlib.h>

#include "dbconfig.h"
#include "dbutret.h"
#include "uttpub.h"
#include "utepub.h"
#include "utspub.h"

#if OPSYS == UNIX

#include <errno.h>
#include <unistd.h>

/* ain't this a beauty.  comes from def of DFILE */
#ifdef FILE
#undef FILE
#endif
#include <pwd.h>

#endif  /* UNIX */

#include <sys/stat.h>

#define MAXSUBP 20

#if 0
struct subptbl {int     subppid;        /* child's process id, keep
                                           at top of structure */
                int     subpofd;        /* file descriptor for output
                                           pipe for writing to child,
                                           else -1 */
                GTBOOL   subpwait;       /* 0 if we have not yet seen
                                           termination of process */
                GTBOOL   subpused;       /* 0 if this entry avail    */
               };


/* reserve storage for subptbl */
LOCAL struct subptbl   subptbl[MAXSUBP+1]; /* last slot is reserved */
#if 0
LOCAL struct subptbl *plastsubp = subptbl+MAXSUBP;
#endif
#endif


/* PROGRAM: utgethostname - get the current hostname and place it into the
 *                          passed buffer.
 *
 * RETURNS: 0 in success,
 *         -1 on error.
 */
int
utgethostname(TEXT *pbuf, 	/* pointer to buffer to put the hostname */
              int   len)    /* length of the buffer */
{
    int    ret;

#if OPSYS == WIN32API
    BOOLEAN retGetHost;
#endif

#if OPSYS==UNIX

    ret = gethostname((char  *)pbuf,len);   

#elif OPSYS==WIN32API

    retGetHost = GetComputerName(pbuf, &len);

    ret = retGetHost ? 0 : -1;
    
#else
    GRONK; How do you get the hostname on this system??
#endif

    return (ret);
}

/* PROGRAM: utgetpid - Returns the process id for the current process
 *
 *    This function uses the OS getprocessid call 
 *    (getpid, GetCurrentProcesId, ... call to returns the id number 
 *    for the running process.
 *
 * RETURNS: process id
 */
ULONG
utgetpid()
{
    ULONG    pid;

#if OPSYS==UNIX

    pid = getpid();   

#elif OPSYS==WIN32API

    pid = GetCurrentProcessId();

#else
    GRONK; How do you get a PID on this system??
#endif

   return (pid);
}


/* PROGRAM: utsetuid - downgrade the userid and groupid from root */
/*              used by modules which execute as root temporarily */
/*              then downgrade themselves to ordinary users       */
 
DSMVOID
utsetuid()
{
#if OPSYS==UNIX
    setuid(getuid());
    /*  setgid(getgid()); DO NOT downgrade groupid, used for
        CONNECT statement in some environments to gain raw db access */
#endif
}


/* PROGRAM: uttty - get ttyname
 *
 * RETURNS: ttyname
 */
TEXT *
uttty(
        int   fildes,
        TEXT *terminalDevice _UNUSED_, /* used if _REENTRANT is defined */
        int   len _UNUSED_, /* used if _REENTRANT is defined */
        int   *sysErrno)
{
 
#if OPSYS==UNIX
    TEXT  *ptty;

#ifdef _REENTRANT
#if SOLARIS
    /* Solaris version returns a pointer */
    ptty = (TEXT *)ttyname_r(fildes, (psc_rtlchar_t *)terminalDevice, len);
#else
    /* POSIX version returns an int */
    if (ttyname_r(fildes, (psc_rtlchar_t *)terminalDevice, len) != 0)
    {
        ptty = NULL;
    }
    else
    {
        ptty = terminalDevice;
    }
#endif
#else
    ptty = (TEXT *)ttyname(fildes);
#endif

    if (!ptty)
       *sysErrno = errno;
     
    return(ptty);
#endif

#if OPSYS==WIN32API
    DWORD  dwType = utGetStdHandleType(fildes);
    *sysErrno = 0;
 
    if ((dwType == FILE_TYPE_DISK) ||
        (dwType == FILE_TYPE_PIPE))
        return (PNULL);
 
    return("CON:");
#endif /* WIN32API */

}  /* end uttty */


#if OPSYS == WIN32API

#include "utf.h"

/* PROGRAM: utGetStdHandleType - gets handle for stdin, stdout, stderr (NT only).
 *
 * Returns: handle            - success
 *          FILE_TYPE_UNKNOWN - invalid handle
 */
DWORD
utGetStdHandleType(int i)
{
    HANDLE hStdH = INVALID_HANDLE_VALUE;
 
    switch (i)
    {
        case 0:
            hStdH = (HANDLE) GetStdHandle(STD_INPUT_HANDLE);
            break;
        case 1:
            hStdH = (HANDLE) GetStdHandle(STD_OUTPUT_HANDLE);
            break;
        case 2:
            break;
        default:
            hStdH = (HANDLE) GetStdHandle(STD_ERROR_HANDLE);
            break;
    }
 
    if (hStdH == INVALID_HANDLE_VALUE)
        return (FILE_TYPE_UNKNOWN);
    else
        return (GetFileType(hStdH));
} /* utGetStdHandleType */

/* PROGRAM:    DetermineVersion - Determine whether this is NT or win9x
 * PARAMETERS: NONE
 * RETURNS:    0 - indicates win9x
 *             1 - indicates winNT
 */
int
utDetermineVersion()
{
    int is_nt = 0;  /* holder for return value */
    LONG vwd;       /* return for GetVersion() */
 
    /* High bit off indicates NT */
    vwd = (GetVersion() & 0x80000000);
    is_nt = (vwd == 0);
    return is_nt;
}

/* PROGRAM: utWinGetOSVersion
**
** Purpose: Detects which windows operating systems is current running on
**
** Input  : VOID
**
** Returns:
**              0 - ENV_UNKNOWN
**              1 - ENV32_WIN95  (32-bit app on WIN95)
**              2 - ENV32_WINNT  (32-bit app on WINNT)
**              3 - ENV16_WINNT  (16-bit app on WIN3X)
**              4 - ENV16_WIN95  (16-bit app on WIN95)
**              5 - ENV16_WINNT  (16-bit app on WINNT)
**/

DWORD
utWinGetOSVersion(VOID)
{
    DWORD dwVersion = GetVersion();
    OSVERSIONINFO osver;

    osver.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
    if (!GetVersionEx(&osver))
        return(ENV_UNKNOWN);

    switch (osver.dwPlatformId)
    {
        case VER_PLATFORM_WIN32_WINDOWS:
            return (ENV32_WIN95);
        case VER_PLATFORM_WIN32_NT:
            return (ENV32_WINNT);
        default:
            return (ENV_UNKNOWN);
    }
}

#endif /* OPSYS == WIN32API */


/* PROGRAM: utuid - determine user's id (pw_name)
 *
 * RETURNS: TEXT *userid
 */
TEXT *
utuid(
        TEXT *userId _UNUSED_, /* used if _REENTRANT is defined */
        int   len _UNUSED_) /* used if _REENTRANT is defined */
{
 
#if OPSYS==UNIX
    struct passwd *ptmp;
 
#if defined(_REENTRANT) && !defined(SCO)

    struct passwd pwd;

#if IBMRIOS || (HPUX && (defined(_PTHREADS_DRAFT4) || defined(HPUX1020)))
    int ret;
    ret = getpwuid_r(getuid(), &pwd, (psc_rtlchar_t *)userId, len);
    ptmp = &pwd;
#elif ALPHAOSF || HPUX || UNIX486V4
    getpwuid_r(getuid(), &pwd, (psc_rtlchar_t *)userId, len, &ptmp);
#else
    ptmp = getpwuid_r(getuid(), &pwd, (psc_rtlchar_t *)userId, len);
#endif
    return (ptmp ? (TEXT *)userId : (TEXT *)"");
 
#else
    ptmp = getpwuid(getuid());
 
    return (ptmp ? (TEXT *)ptmp->pw_name : (TEXT *)"");
 
#endif

#endif
 
#if OPSYS==WIN32API
    TEXT *userid = (TEXT*)"";
 
    userid = (TEXT *) getenv("USER");
    if (userid == NULL)
    {
        /* SMM 96-04-02-028: NT naturally sets USERNAME */
        userid = (TEXT *) getenv("USERNAME");
    }
    if (userid == NULL)
    {
        userid = TEXTC "";
    }

    return userid;
#endif
 
}

/* PROGRAM: utCheckTerm - this checks to see if the process has a terminal
 *            attached to it or not.  It uses fstat on fd 0, 1, and 2.
 *            If there is a terminal attached, the return gets
 *            set to 1, otherwise it is set to 0.
 *
 *            BE CAREFUL NOT TO PRINT ANY MESSAGES HERE because we should
 *            have no files open yet (including PROMSGS)
 *
 * RETURNS:  1 - Terminal attached, running in foreground
 *           0 - Terminal not attached, running in background
 */
int
utCheckTerm()
{
        int     ret;            /* used for return status from fstat */
        int     i;              /* a counter so we can loop over the 3 fds */
struct  stat    buf, *pbuf;     /* buffer used for stat */
        int     cnt=0;          /* cnt to see if all files are open */
        int     srvfgnd;
 
        pbuf = &buf;
 
        /* assume there is no terminal attached */
        srvfgnd=0;
 
        for (i=0; i< 3; i++)
        {
            ret = fstat(i,pbuf);
            if (ret == 0)
            {
                /* we must have a terminal */
                cnt++;
            }
        }
 
        /* if all 3 files are open, then we are in the foreground */
        if (cnt == 3)
        {
            srvfgnd = 1;
        }
        return srvfgnd;
}

#if OPSYS == WIN32API
/* PROGRAM: utgetenvwithsize: This function returns:
   ultimately utgetenv should be removed from dbut and
   all references to the dbut utgetenv should be replaced with
   utgetenvwithsize
   1: if failure,
   0: if successfull */

 
LONG
utgetenvwithsize( TEXT *pEnvVariableName, 
                  TEXT *pEnvVariableValue, 
                  LONG size)
{
    LONG sizeNeeded,
         returnCode;

    sizeNeeded = GetEnvironmentVariable( (LPCTSTR)pEnvVariableName, 
                                         (LPTSTR)pEnvVariableValue, 
                                         size);

    /* if GetEnvironmentVariable() returns zero, the env var was
       not found. */
    if ( sizeNeeded == 0)
    {
        returnCode = 1;  /* return failure, key not found in registry */
    }
    else
    {
        /* if we got a value from GetEnvironmentVariable() that was
           larger than the buffer as sized, return failure, otherwise
           return success */
        returnCode = (sizeNeeded > size) ? 1 : 0;
    }

    return returnCode;
}

#else

/* Program: utgetenvwithsize: This function returns:
   sizeneeded -- if it fails or
   size       -- if it succeeds */


LONG
utgetenvwithsize( TEXT *pEnvVariableName, 
                  TEXT *pEnvVariableValue, 
                  LONG size _UNUSED_)
{
     TEXT *pstr;

     pstr = (TEXT *)getenv((char *)pEnvVariableName) ;

     if ( !pstr )
          return 1;

     /*assumption: getenv returns a null terminated string*/
     p_stcopy( pEnvVariableValue, (TEXT *)getenv((char *)pEnvVariableName) );
     return 0; 
}
#endif


#if 0
/*
 *     PROGRAM: utdeadp -- called on a SIGPIPE interrupt.
 *             In this case a subprocess
 *             has closed the receive end of a pipe we are
 *             using to talk to the child. This routine
 *             tries to find any dead processes which have
 *             output file descriptors and closes any
 *             found. This avoids loops in Bell UNIX.
 */
 
LONG
utdeadp ( )
{
   FAST    struct  subptbl *ptmp;
   LONG    ret;
 
    /* if any process is dead, process it */
    if ((ret = utwaitp ( (int *)plastsubp, 1))
         == DBUT_S_TOO_MANY_PENDING_TIMERS)
    {
        return ret;
    }
  
 
    /* locate any processes which have terminated, but which we have
       note yet processed a utwaitp call. If any exist with write
       file descriptors, close the file. */
    for ( ptmp=subptbl; ptmp<plastsubp; ptmp++ )
        if ( ptmp->subpwait && ptmp->subpofd != -1 )
        {   close ( ptmp->subpofd );
            ptmp->subpofd = -1;
        }

    return 0;
}  /* utdeadp */


/* PROGRAM: utwaitp - waits for subprocess to terminate.       */
 *  					                       *
 *  RETURNS: DSMVOID					       *
 *	utwaitp ( ppid, maxwait ); waits for the indicated     *
 *             subprocess to terminate. If maxwait is greater  *
 *             than zero it is the maximum amount of time      *
 *             to wait. Once the process ends (or maxwait      *
 *             is exceeded), the subprocess entry in the       *
 *             table is cleared. If the time limit is          *
 *             exceeded and we have a fd to talk to the        *
 *             process, we close the fd.                       *
 *                                                             *
/***************************************************************/
LONG
utwaitp ( int *ppid,			/* pointer to child pid */
	  int maxwait )			/* maximum time to wait */
 
{
 
FAST    struct  subptbl *psubptbl = (struct subptbl *)ppid;
FAST    struct  subptbl *ptmp;
FAST            int      pid;
                GTBOOL    waitflg = 0;
                int      waited = 0;
                LONG     ret;
 
#if UNIXBSD2
    /* no point setting a timer since it doesnt work */
#else
    /* set waiting limit, if desired */
    if ( maxwait > 0 )
    {
        if ((ret = utevt( maxwait , &waitflg))
                  == DBUT_S_TOO_MANY_PENDING_TIMERS)
        {
            return ret;
        }
    }
#endif  /* UNIXBSD2 */
 
    /* wait for subprocess to terminate or time to expire */
    while ( psubptbl->subpwait == 0 )
    {
        if (waitflg)
        {
            break; /* timer expired */
        }
#if UNIXBSD2
 
        if (maxwait > 0)
        {
            /* they want a finite wait, we will wait no more than 0 secs */
#if MIPS
 
            pid = wait2((union wait *) 0, WNOHANG);
 
#else  /* MIPS  */
#if SEQUENTPTX | SCO386
 
            pid = waitpid( 0, (int *) NULL, WNOHANG);
 
#else  /* SEQUENTPTX */
 
            {
                int napms = 1;
                int waitms = maxwait*1000;
 
                while (napms <= waitms)
                {
                   pid = wait3((union wait *) 0, WNOHANG, (struct rusage *) 0);
                   if ( 0 != pid )
                   {
                       /* the wait was successful */
                       waited = 1;
                       break;
                   }
                   utnap(napms);
                   napms = napms + 100;
                }
 
                /* if we timed out, lets try one last time */
                if (!waited)
                {
                   pid = wait3((union wait *) 0, WNOHANG, (struct rusage *) 0);
                }
 
            } /* block */
#endif  /* SEQUENTPTX */
#endif  /* MIPS */
            if (pid == 0)
            {
                break;
            }
        }
        else
        {
            /* infinite wait */
            pid = wait((int *) NULL);
        }
 
#else   /* UNIXBSD2 */
 
        /* wait until timer expires, infinite if no timer */
        pid = wait((int *) NULL);
 
#endif  /* UNIXBSD2 */
 
        if (pid < 0)
        {   if (errno == ECHILD) break; /* no children alive */
            continue;
        }
 
        /* find which subprocess just terminated */
        for(ptmp=subptbl; ptmp<plastsubp; ptmp++)
            if (pid == ptmp->subppid)  {ptmp->subpwait = 1;
                                        break;
                                       }
    }
 
    /* if time limit was established and not exceeded, cancel timer */
    if (maxwait > 0 && !waitflg)
        utcncel(&waitflg);
 
    /* cleanup the table entry */
    psubptbl->subppid  = 0;
    psubptbl->subpwait = 0;
    psubptbl->subpused = 0;
    return 0;
}
#endif

