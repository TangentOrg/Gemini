
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

/*
 * To Do:
 *  - semAdd on NT uses a different interface than UNIX.  It doesn't add to
 *    semaphore and let the semaphore do it's work.  really hacky.
 *  - Both semAdd and semValue on NT don't use the ID/INDEX method of
 *    indicating which semaphore to use, causing function prototype to be
 *    different for nt (ULONG instead of INT).
 *  - change operating specific calls to include OS prefix:  semValueNT vs
 *    semValueUX (for nt vs unix)
 *  - check places with USE_V82_CODE conditional compilation and fix
 *  - check new functions in this module w/ ntsem.c; V8 code eliminated ntsem.c
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "bkpub.h"     /* for bkGetIPC() */
#include "sempub.h"
#include "semprv.h"
#include "shmpub.h"    /* for struct bkIPCData */

#include "utmpub.h"
#include "utspub.h"

#define USE_V82_CODE 1

#if OPSYS==UNIX

#include "dbcontext.h"
#if !(NOTSTSET || NCR486)
#include <sys/types.h>
#endif

#include <errno.h>

#include "stmsg.h"
#include "utmsg.h"
#include "svmsg.h"
#include "usrctl.h"


#if !MIPSABI
union   semun {
        int val;
        struct semid_ds *buf;
        ushort array[1];
        };
#endif

#include <sys/msg.h>

/* Local Function Prototypes */
LOCALF DSMVOID semErMsg    (dsmContext_t *pcontext, int error,
                         char *function, TEXT *pname);  

/*****************************************************************/
/**                                                             **/
/**     utsem - routines to access the System V semaphores      **/
/**     ALL semaphores are identified using keys constructed    **/
/**     from the database inode and an identifier character,    **/
/**     using the ftok routine described in stdipc(3).          **/
/**                                                             **/
/*****************************************************************/

/* PROGRAM: semErMsg - send error message describing why a 
 *                   semaphore operation request is denied.
 *
 * RETURNS: None
 *
 */
LOCALF DSMVOID
semErMsg(
        dsmContext_t    *pcontext,
        int      error,    /* errno, passed in case msg subsystem changes it */
        char    *function, /* "get", "create", etc                      */
        TEXT    *pname)    /* the dbname used to make the key           */
{
    switch (error) {
                 /* invalid semaphore id */
    case EINVAL: MSGN_CALLBACK(pcontext, utMSG033); break;
                 /* semaphore operation permission denied */
    case EACCES: MSGN_CALLBACK(pcontext, utMSG034); break;
        }
    /* unable to <acc/cr> semaphore set */
    MSGN_CALLBACK(pcontext, utMSG016, function, pname, " ", error);
    errno = error;      /* in case changed by msg subsystem */
}

/* PROGRAM: semClear - remove the identified semaphore set from the system
 *
 * RETURNS: int set to either (0) for success or -1 for an
 *          access or permission error.
 */
int
semClear(
        dsmContext_t    *pcontext _UNUSED_,
        TEXT            *pname _UNUSED_, /* dbname or other identifying file name */
        int              semid)   /* extra character to allow multiple sems*/
{
    union       semun    semarg;

    /* Verify the semid is valid */
    if (semid < 0)
    {
      return -1;
    }

    return semctl(semid, 0, IPC_RMID, semarg);
}

/* PROGRAM: semSet - set the semval field for the indicated semaphore.
 * 
 * RETURNS: 0=OK, non-0=non-OK                                           
 */
int
semSet(
       dsmContext_t     *pcontext,
       int               semid,         /* the semaphore id             */
       int               semnum,        /* semaphore number within set  */
       int               value)         /* desired value                */
{
    int         ret;
    union semun semarg;
    
    semarg.val = value;
    ret = semctl(semid, semnum, SETVAL, semarg);
    if (ret < 0)
    {
      semErMsg(pcontext, errno, (char *)"set", PNULL);
    }

    return ret;
}

/* PROGRAM: semValue - read the value of a semaphore    
 * 
 * RETURNS: >=0 is value of semaphore, <0 is error      
 */
int
semValue(
         dsmContext_t    *pcontext,
         int      semid,         /* the semaphore id             */
         int      semnum)        /* semaphore number within set  */
{
    int      ret;
    union   semun    semarg;
    
    ret = semctl(semid, semnum, GETVAL, semarg);
    if (ret < 0)
    { 
      semErMsg(pcontext, errno, (char *)"get value of", PNULL);
    }

    return ret;
}

/* PROGRAM: semCrSet - create a semaphore set 
 * 
 * RETURNS: <0 error, >=0 is the semid       
 */
int
semCrSet(
        dsmContext_t    *pcontext,
        TEXT            *pname,   /* dbname or other identifying file name */
        char             idchar _UNUSED_, /* extra character to allow multiple sems*/
        int             *pnumsems, /* number of sems desired, return actual#*/
        int              minsems)   /* minimum number of sems acceptable */
{
    int      gotsems = *pnumsems;
    int      semid;
    
    
    /* try getting fewer and fewer sems until succeed */
    do {
        semid = semget(IPC_PRIVATE, gotsems, 0666+IPC_EXCL+IPC_CREAT);
    } while(   semid == -1
        && (errno == EINVAL || errno == ENOSPC)
        && --gotsems >= minsems);
    
    if (semid < 0)
    {
      switch (errno)
      {
        case EINVAL:    MSGN_CALLBACK(pcontext, utMSG031); 
	                break;

        case ENOSPC:    MSGN_CALLBACK(pcontext, utMSG032);
	                break;

        default:        semErMsg(pcontext, errno, (char *)"create", pname);
                        break;
       }
       return -1;
    }
    
    if (gotsems < *pnumsems)
    {
      MSGN_CALLBACK(pcontext, utMSG023, gotsems);
      *pnumsems = gotsems;
    }
    return semid;
}

/* PROGRAM: semGetSet - get an existing semaphore set 
 * 
 * RETURNS: <0 error, >=0 is the semid               
 */
int
semGetSet(
        dsmContext_t    *pcontext,
        TEXT            *pname,   /* dbname or other identifying file name */
        char             idchar _UNUSED_, /* extra character to allow multiple sems*/
        int              noentmsg)  /* if 0, suppress ENOENT error message */
{
        int            semid;
        union   semun  semarg;        /* used to check status of sem set */
        bkIPCData_t    *pbkIPCData;   /* used to hold semid */
    
    /* Get storage for pbkIPCData */
    pbkIPCData = (bkIPCData_t *)utmalloc(sizeof(bkIPCData_t));

    /* Make sure we got the storage */
    if (pbkIPCData == (bkIPCData_t *)0)
    {
        /* We failed to get storage for pbkIPCData */
        MSGN_CALLBACK(pcontext, stMSG006);
        return -1;
    }

    /* initialize structure correctly */
    stnclr(pbkIPCData,sizeof(bkIPCData_t));

    /* Note that pname and idchar are dropped on the floor here */
    if (bkGetIPC(pcontext, pcontext->pdbcontext->pdbname, pbkIPCData))
    {
        /* Failed to read sem id from db mstrblk */
        utfree ((TEXT *)pbkIPCData);
        return -1;
    }

    if ((pbkIPCData->NewSemId == -1) && (pbkIPCData->NewShmId == -1))
    {
        /* this is a hack, but needs to be set to bubble up correctly */
        errno = ENOENT;
    }

    semid = pbkIPCData->NewSemId;


    if (semid < 0)
    {
        if (noentmsg || errno != ENOENT)
        {
            semErMsg(pcontext, errno, (char *)"get", pname);
        }
    }
    {
        /* check and see if this is an existing semaphore or not */
        /* if its not, semctl will return -1 and errno == EINVAL */
        if (semctl(semid, 0, GETVAL, semarg) == -1)
        {
            /* the semctl operation failed, this is a bad semid */
            semid = -1;
        }
    }

    /* Clean up after pbkIPCData */
    utfree((TEXT *)pbkIPCData);

    return semid;
}

/* PROGRAM: semChgOwn - change the owner of the semaphore 
 *
 * RETURNS: None
 *
 */
DSMVOID
semChgOwn(
        dsmContext_t    *pcontext,
        int              semid, /* change this semaphore set */
        int              uid)   /* change to this userid     */
{
        struct semid_ds buf;
        union semun arg;

    arg.buf = &buf;

    if (semctl(semid, 0, IPC_STAT, arg) < 0)
    {
      semErMsg(pcontext, errno, (char *)"read status of", PNULL);
      return;
    }

#if TOWER800 /* Implementation bug in OS 010300 of UnixV T32_800 */
    arg.buf->sem_perm.gid = getgid();
#else
    arg.buf->sem_perm.uid = uid;
#endif
    if (semctl(semid, 0, IPC_SET, arg) < 0)
    {
      semErMsg(pcontext, errno, (char *)"change owner of", PNULL);
      return;
    }
}

/* PROGRAM: semAdd - add or subtract from a semaphore value,
 *  possibly waiting
 * If value is negative, this call will wait if necessary
 * until the semaphore's value is greater than or equal
 * to the absolute value of the value argument.
 *
 * RETURNS: 0=OK, non-0=non-OK                                          
 */
int
semAdd(dsmContext_t     *pcontext,
        int      semid,    /* semaphore's unique id             */
        int      semnum,   /* semaphore number within set       */
        int      value,    /* alter value                       */
        int      undo,     /* may be SEM_UNDO                   */
        int      plsretry) /* if SEM_RETRY, then retry if interrupt*/
{
        struct  sembuf   sembuf[1];
        int     ret;
        dbcontext_t *pdbcontext = pcontext->pdbcontext;

    /* initialize semaphore operation fields */
    sembuf[0].sem_num = semnum;
    sembuf[0].sem_op  = value;
    sembuf[0].sem_flg = undo;

    /* alter indicated semaphore, retrying if interrupted */
    for(;;)
    {
       ret = semop(semid, sembuf, 1);
       if (ret >= 0)
       {
	 return 0; /* successful */
       }

       switch (errno)
       {
         case EINTR:             /* interrupted, retry */
           if (plsretry)
	   {
	     continue;
	   }
           return -1;

         case ENOSPC:            /* too many users used undo */
           if (sembuf[0].sem_flg == SEM_UNDO)
           {   /* increase SEMMNU paramemter */
	     MSGN_CALLBACK(pcontext, utMSG021);
	     sembuf[0].sem_flg = 0; /* no UNDO */
	     continue; /* retry without undo */
	   }

         case EINVAL: 
         case EIDRM:
           if ((pdbcontext->pmtctl) && 
	       (semnum == LOGSEM) &&
	       (NULL != pdbcontext->psemdsc) && 
	       (semid == pdbcontext->psemdsc->semid))
           {
	     return 0; /* User wants to quit after Broker died */
	   }
	   if (pdbcontext->pmtctl)
           {
	     /* Semaphore id %d was removed */
	     MSGN_CALLBACK(pcontext, utMSG040, semid);
	     /* The server has disconnected */
	     MSGN_CALLBACK(pcontext, svMSG025);
	     return -1;
	   }
	 default: semErMsg(pcontext, errno, (char *)"semAdd", PNULL); return -1;
        }
    }
} 




#if NOTSTSET

/* PROGRAM: semtst - functionally equivalent to tstset() and tstclr()
 * - used for architectures that do not support tstset instructions:
 * value == +1 
 * - tstset() -- returns 0 = tstset successful, 1 = busy;
 * value == -1
 * - tstclr() -- release semaphore 
 * RETURNS: int - (0) for success
 *                (1) for error
 */
int
semtst(
        dsmContext_t    *pcontext,
        in               semid,         /* semaphore's unique id        */
        int              semnum,        /* semaphore number within set  */
        int              value)         /* alter value                  */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
        struct  sembuf   sembuf[1];
        int     ret;

    /* initialize semaphore operation fields */
    sembuf[0].sem_num = semnum;
    sembuf[0].sem_op  = value;
    sembuf[0].sem_flg = SEM_UNDO | IPC_NOWAIT;
    /* alter indicated semaphore, retrying if interrupted */
    for ( ;; )
    {
        ret = semop(semid, sembuf, 1);
        if (ret >= 0) return 0; /* successful */

        switch (errno) {
        case EAGAIN:            /* semaphore is busy */
        case EINTR:             /* interrupted, retry */
        case ENOSPC:            /* too many users used undo */
           return 1;
        case EINVAL:
           if (pdbcontext->pdbpub->shutdn && (pdbcontext->usertype & DBSHUT))
           {
               if (semnum==DBSEM &&
              XUSRCTL(pdbcontext, pdbcontext->pmtctl->qdbholder) == pcontext->pusrctl)
                   return 1;
                else
                   return 0;
           }
        default: 
           semErMsg(pcontext, errno, (char *)"semAdd", PNULL); 
           FATAL_MSGN_CALLBACK(pcontext, utFTL011);
        }
    }
} 
#endif 

#endif /* UNIX */

#if OPSYS==WIN32API
#include "windows.h"
#include "utmsg.h"
#include "svmsg.h"
#include "utf.h"
#include "usrctl.h"

/* PROGRAM: semAdd - NT set or clear, the given NT semnum
 *
 * RETURNS: 0=OK, non-0=non-OK  
 */
int
semAdd(
        dsmContext_t    *pcontext,
        ULONG    semid,         /* ignored for NT                       */
        HANDLE   semnum,        /* semaphore number                     */
        int      value,         /* if -1, set it, if 1, clear it        */
        int      undo,          /* may be SEM_UNDO                      */
        int      plsretry)      /* if SEM_RETRY, then retry if interrupt*/
{
  int    ret;
  DWORD  dwRet;

  if (value == 1)   
  {
       ret = osReleaseSemaphore(pcontext, semnum);
       if (ret)
       {
           /* Unable to clear semaphore in semAdd(), error %d */
           FATAL_MSGN_CALLBACK(pcontext, utFTL019, ret);
           return -1; /*FIX later MUST BE fatal*/
       }
  }
  else
  {
      for (;;)
      {
          dwRet = WaitForSingleObject(semnum, INFINITE); /* Wait for ever */
          switch(dwRet)
          {
              case 0:     /* Success - Got the semaphore */
                  NTPRINTF("semAdd - WTFSO(%d) - SUCCESS",semnum);
                  return 0;
              case WAIT_TIMEOUT:
                  NTPRINTF("semAdd - WTFSO(%d) - TIMEOUT",semnum);
                  if (plsretry) continue;
                  return -1;
              default:
                  MSGD_CALLBACK(pcontext, "%GsemAdd() NT System Error = %l",
                        GetLastError());
                  return -999;  /* Something went wrong */
          }
      }
  }    
  return 0;
}    

/* PROGRAM: semValue - read the value of a semaphore 
 * 
 * RETURNS: >=0 is value of semaphore, <0 is error 
 */
int
semValue(
        dsmContext_t    *pcontext,
        int              semid,        /* semid                */
        int              semnum)        /* unused for NT       */
{
  return 1;
}


/* PROGRAM: utsemset - set the semval field for the indicated semaphore.
 *
 * RETURNS: 0
 */
int 
semSet(pcontext,semid, semnum, value)
        dsmContext_t *pcontext;
        ULONG       semid;         /* the semaphore id                */
        HANDLE      semnum;        /* semaphore number within set        */
        int         value;         /* desired value                */
{
    GBOOL  fOK;
    DWORD dwLast = 0L;
    DWORD  dwRet;

    /* get the current value of the semaphore */
    fOK = ReleaseSemaphore(semnum, 1, &dwLast);
    if(!fOK)
    {
        /* Error in ReleaseSemaphore */
        MSGN_CALLBACK(pcontext, utMSG112,dwLast, GetLastError());
    }

    /* inorder to get the value, we needed to increase it by 1.
       dwLast holds the previous value, so increase it by 1 to 
       make it current. */
    dwLast++;

    if (value < dwLast)
    {
        while(dwLast > value)
        {
            dwRet = WaitForSingleObject(semnum,10000);  /* 10 Second wait */
            dwLast--;
            if (dwRet != WAIT_OBJECT_0)
            {
                /* Error clearing semaphore */
                MSGN_CALLBACK(pcontext, utMSG113,dwLast,GetLastError());
            }
        }
    }
    else if (dwLast < value)
    {
        fOK = ReleaseSemaphore(semnum, value - dwLast, &dwLast);
        if(!fOK)
        {
            /* Error ReleaseSemaphore */
            MSGN_CALLBACK(pcontext, utMSG112,dwLast,GetLastError());
        }
    }
    return 0;
}

/* PROGRAM: osOpenSemaphore
 * RETURNS: NULL error, HANDLE Success.
 *
 */
HANDLE 
osOpenSemaphore(char *semname)
{
    HANDLE  hOpenSem = 0;

    hOpenSem = OpenSemaphore(SEMAPHORE_ALL_ACCESS, /* All access bits set */
                             TRUE,         /* Child processes inherit handle */
                             semname);
   return(hOpenSem);
}

/* PROGRAM: osCloseSemaphore
 * RETURNS: non-zero error, zero Success.
 *
 */

int osCloseSemaphore(HANDLE hSem)
{
    return(!CloseHandle(hSem));
}

/* PROGRAM: osCreateSemaphore
 * RETURNS: NULL error, HANDLE Success.
 *          The semaphore is initialized with a value of 0 (NOT FLAGGED).
 */

HANDLE 
osCreateSemaphore(dsmContext_t *pcontext, char *semname)
{
    HANDLE hCreSem  = NULL;
    HANDLE hOpenSem = NULL;
    HANDLE hRet     = NULL;
    DWORD dwError   = 0L;
    /* Attempt to create the semaphore */

    hCreSem = CreateSemaphore(SecurityAttributes(0), /* default */
                              0,    /* Initial State (NOT FLAGGED) */
                              3,    /* Maximum value */
                              semname);

	dwError = GetLastError();

    /* If we failed becuase the semaphores already exist, then use them */
    if (hCreSem == (HANDLE)0)
    {
		if (dwError == ERROR_ALREADY_EXISTS)
		{
			hOpenSem = osOpenSemaphore(semname);
			if (hOpenSem)
			{
				/* return the semaphore that existed */
				hRet = hOpenSem;
			}
		}
		else
		{
			/* return NULL */
			hRet = NULL;
			MSGN_CALLBACK(pcontext, utMSG120, dwError);
		}
    }
    else
    {
        /* return the semaphore we created */
        hRet = hCreSem;
    }
    
	return hRet;
}

/*
 * PROGRAM: osReleaseSemaphore
 *
 * RETURNS: Zero(0) on success
 */
LONG 
osReleaseSemaphore(dsmContext_t *pcontext, HANDLE hSem)
{
    GBOOL  fOK;
    DWORD dwError = 0L;
    DWORD dwLast = 0L;

    /* increment the semaphore by 1 */
    fOK = ReleaseSemaphore(hSem, 1, &dwLast);
    if(!fOK)
    {
        dwError = GetLastError();
        /* Error incrementing semaphore */
        MSGN_CALLBACK(pcontext, utMSG114,dwLast,dwError);
        return dwError;
    }
    return 0L; /* Success */
}

#endif  /* OPSYS==WIN32API */
