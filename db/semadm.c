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
 *  - clean up includes
 *  - What is the #define _MAX_PATH 260 there for?  Should it be moved?
 *    or #include'd? or changed to MAX_PATH?
 *    moved into some context structure or somehow elimated.
 *  - check places with USE_V82_CODE conditinal compilation and fix
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include <stdlib.h>

#include "dbconfig.h"
#include "sempub.h"
#include "semprv.h" 

#include "dbcontext.h"
#include "dbmsg.h"
#include "dbpub.h"
#include "dsmpub.h"
#include "latpub.h"
#include "stmpub.h"
#include "stmprv.h"
#include "usrctl.h" 
#include "utfpub.h"

#include <errno.h>

#if OPSYS==UNIX
#include <unistd.h>
#endif

#if OPSYS==WIN32API
#include "shmpub.h"
#include "utmsg.h"
#endif
#include "shmprv.h"  /* for utmkosnm */

/* LOCAL Function Prototypes for semadm.c */

#if OPSYS==WIN32API
LOCALF int semTabCr(dsmContext_t *pcontext);
#endif

#if OPSYS==WIN32API

/*
 * This code was copied from progress V8.2 code in mtsem.c by
 * by Brent Sadler on 5-30-97.  It has not been integrated yet or
 * verified in any way.  We may want to restructure the code
 * before integration, particularly where global variables are
 * concerned.
 *
 * The SecurityAttributes function in v8 provides the following
 * service in V8:
 *  - it provides a consistent default across all processes for the
 *    NT security attributes parameter, used when accessing shared
 *    memory and semaphores (and other NT objects?).
 *
 * Also, the SecurityAttributes function seems to be called from various
 * places in the V8 code and we need to examine how to migrate this code
 * into the V9 code.
 *
 * Also note that I use the USE_V82_CODE conditional in other places in
 * in the code to indicate that I'm not sure which way the code should
 * read at this point in time.
 */
#include "utf.h"
/* NT Security Implementation
   Should be a C++ implementation

   Here is how it works:
   Init:                                SecurityClear();
   Add Users/Groups:    SecurityAdd("administrators");
                                                SecurityAdd("veasy");
   Create SD                    SecurityCreate();
   Use Pointer                  SecurityPSD();
*/

#define         MAX_LIST        4   /* Max number of calls to SecurityAdd */
#define         MAX_LEN         64  /* Max Length of Security Tokens per user */
#define         MAX_BUF         256 /* Max Length of New Secutity Descriptor */
DWORD                   dwGroupSIDCount = 0;  /* Number of group or users added */
DWORD                   dwDACLLength = 0;
PSECURITY_DESCRIPTOR    pAbsSD;
PSID                    pGroupSID[MAX_LIST];
BYTE                    dataGroupSID[MAX_LIST][MAX_LEN];
PACL                    pNewDACL = NULL;
BYTE                    dataNewDACL[MAX_BUF];
BYTE                    dataAbsSD[MAX_BUF];

int                     nErr = -1;
PSECURITY_ATTRIBUTES    pSD = NULL;
SECURITY_ATTRIBUTES             SA;

LOCALF          InitSecurityAttributes();
LOCALF          SecurityClear();
LOCALF          SecurityAdd(char * szName);
LOCALF          SecurityCreate();


/* Function Implementation */
PSECURITY_ATTRIBUTES            
SecurityAttributes()
{
    if (nErr == -1)
    {
        if (utWinGetOSVersion() == ENV32_WINNT)
            InitSecurityAttributes();
        else
            nErr = 0;                           /* No Security Needed */
    }
    return pSD;
}


int 
InitSecurityAttributes()
{
    GBOOL    bOK = FALSE;
    nErr = 0;
    
    /* Access is defined */
    SecurityClear();
    bOK = TRUE;
    if (SecurityCreate() == ERROR_SUCCESS)
    /* New security is ok! */
    {
	SA.nLength  = sizeof(SECURITY_ATTRIBUTES);
	SA.lpSecurityDescriptor = pAbsSD;
	SA.bInheritHandle = TRUE;
	pSD = &SA;
	/*  It is now enabled! */
    }
    return bOK;
}

int 
SecurityClear()
{
    int i;
    
    dwGroupSIDCount = 0;
    dwDACLLength = sizeof(ACL);
    
    for(i = 0; i < MAX_LIST; pGroupSID[i++] = NULL);
    
    return TRUE;
}


int 
SecurityAdd(char * szName)
{
    DWORD   dwRet, cbSID, dwDomainSize;
    SID_NAME_USE    snuGroup;
    TCHAR   szDomain[80];
    
    if (dwGroupSIDCount > MAX_LIST)
        dwRet = ERROR_TOO_MANY_NAMES;
    else if (szName == NULL)
        dwRet = ERROR_INVALID_DATA;
    else
    {
        cbSID   = MAX_LEN;
        pGroupSID[dwGroupSIDCount] = (PSID) &dataGroupSID[dwGroupSIDCount][0];
        dwDomainSize = 80;
        if (!LookupAccountName(NULL, szName, pGroupSID[dwGroupSIDCount], 
                                   &cbSID, szDomain, &dwDomainSize, &snuGroup))
        {
            pGroupSID[dwGroupSIDCount] = NULL;
            dwRet = GetLastError();
        }
        else
        {
            dwDACLLength += (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD) 
                         + GetLengthSid(pGroupSID[dwGroupSIDCount]) );
            dwGroupSIDCount++;
            dwRet = ERROR_SUCCESS;
        }
    }
    
    return dwRet;
}


int SecurityCreate()
{
    DWORD dwError = ERROR_SUCCESS, i;
    if (dwDACLLength > MAX_BUF)
        dwError = ERROR_INSUFFICIENT_BUFFER;
    
    if (dwGroupSIDCount)
        pNewDACL = (PACL) dataNewDACL; 
    else
        pNewDACL = NULL;
    
    pAbsSD = (PSECURITY_DESCRIPTOR) dataAbsSD; 
    
    if (!InitializeSecurityDescriptor(pAbsSD, SECURITY_DESCRIPTOR_REVISION))
        dwError = GetLastError();
    
    if (dwGroupSIDCount)
        if (!InitializeAcl(pNewDACL, dwDACLLength, ACL_REVISION))
            dwError = GetLastError();
        
        /* Now Create the New DACL  */
        for(i = 0; (i < dwGroupSIDCount) && (dwError == ERROR_SUCCESS); i++)
        {
            if (!AddAccessAllowedAce(pNewDACL, ACL_REVISION, 
                                     GENERIC_ALL, pGroupSID[i]) )
                dwError = GetLastError();
        }
        
        if (dwError == ERROR_SUCCESS) 
        {
            if (dwGroupSIDCount)
            {
                if ( !IsValidAcl(pNewDACL) )
                    dwError = GetLastError();
            }
            
            if (!SetSecurityDescriptorDacl(pAbsSD, TRUE, pNewDACL, FALSE))
                dwError = GetLastError();
            
            if (!IsValidSecurityDescriptor(pAbsSD))
                dwError = GetLastError();
        }
        
        return dwError;
}

#endif  /* OPSYS==WIN32API */


#if OPSYS==UNIX
/* PROGRAM: semCreate - create the semaphores used for a database 
 *
 * RETURNS: 0=OK, non-0 = non-OK                                
 * PARAMETERS:
 *   dbcontext   *dbcontext          create the sems for this database
 *   bkIPCData_t *pbkIPCData         holds semid
 */
int
semCreate(
        dsmContext_t    *pcontext,
        bkIPCData_t     *pbkIPCData)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        int      semid;
        int      ret;
                           /* 1 extra semaphore for shutdown module */
        int      numsems = pdbcontext->pdbpub->argnusr + FRSTUSRSEM;
        SEMDSC  *psemdsc;
        semid_t	*psemids;
	int	 semsize;
	ULONG	 semset;
	ULONG	 semperset;
	ULONG	 i;
    
    if (pdbcontext->argnoshm) return 0;

    psemdsc = (SEMDSC *)stGet(pcontext, pdbcontext->privpool, sizeof(SEMDSC));

    psemdsc->pname = pdbcontext->pdbname;

    psemids = (semid_t *)stGet(pcontext, 
                               XSTPOOL(pdbcontext, pdbcontext->pdbpub->qdbpool),
                               sizeof(semid_t) * pdbcontext->pdbpub->numsemids);

    pdbcontext->psemids = psemids;

    pdbcontext->pdbpub->qsemids = (QSEMIDS)P_TO_QP(pcontext, psemids);

    /* if we have more than 1 semaphore set, then we need to
       allocate a bunch of sets and keep track of how large each one
       is. */
    if (pdbcontext->pdbpub->numsemids > 1)
    {
        numsems = FRSTUSRSEM+4;

        /* allocate the login semaphore set */
        semid = semCrSet(pcontext, psemdsc->pname, 'U',
                    &numsems, FRSTUSRSEM+4);
        if (semid < 0) 
        {
            /* since this is the first set we tried to create then
               just return */
            return -1;
        }

        /* and change the owner name to myself */
        semChgOwn(pcontext, semid, getuid());

        /* to properly init the sempahore, set it to 1, then decrement it with 
           SEM_UNDO.  Then set a different flag to indicate that as soon as it 
           is unlocked again, it is ready to be used  */
        if (   (ret=semSet(pcontext, semid, LOGSEM, 1))
            || (ret=semAdd(pcontext, semid, LOGSEM, -1, SEM_UNDO, SEM_RETRY))
            || (ret=semSet(pcontext, semid, RDYSEM, 1)))
            return ret;

        /* Store semid fo write to mstrblk */
        pbkIPCData->NewSemId = semid;
        pdbcontext->pdbpub->loginSemid = semid;

        psemdsc->numsems = numsems;
        psemdsc->semid = semid;
        pdbcontext->psemdsc = psemdsc;  /* the semaphore now exists */
        pcontext->logsemlk = 1;       /* the LOGSEM is locked */

        /* now allocate all the rest of the sets */
        for (i = 0, psemids = pdbcontext->psemids; 
            i < pdbcontext->pdbpub->numsemids; i++, psemids++)
        {
            psemids->id = -1;
        }
 
        numsems = pdbcontext->pdbpub->argnusr;

        /* figure out how many semaphores per set we want to use. */
        semperset = (int)((numsems + (pdbcontext->pdbpub->numsemids -2))
                           / (pdbcontext->pdbpub->numsemids -1));
 
        semset = 0;
        psemids = pdbcontext->psemids;
        while (semset < (pdbcontext->pdbpub->numsemids-1))
        {
            semsize = semperset;
 
            /* create the new one */
            semid = semCrSet(pcontext, psemdsc->pname, 'U', &semsize, semperset);
            if (semid < 0)
            {
                /* we need to cleanup all the existing semaphore sets
                   before we return, else we'll leave strays around */
                semDelete(pcontext);
                return -1;
            }
 
            /* and change the owner name to myself */
            semChgOwn(pcontext, semid, getuid());
 
            /* put the semaphore id in the appropiate spot in shared memory */
            psemids->size = semsize;
            psemids->id = semid;
 
            /* keep track of how many semaphores we have used */
            psemdsc->numsems += semsize;
 
            /* on to the next semaphore set */
            semset++;
            psemids++;
        }
    }
    else
    {
        /* Only using 1 semaphore set for the entire system */
        semid = semCrSet(pcontext, psemdsc->pname, 'U',
                        &numsems, MIN(numsems, FRSTUSRSEM+4));
        if (semid < 0)
        {
            /* we need to cleanup all the existing semaphore sets
               before we return, else we'll leave strays around */
            semDelete(pcontext);
            return -1;
        }
 
        /* and change the owner name to myself */
        semChgOwn(pcontext, semid, getuid());
 
        /* to properly init the sempahore, set it to 1,
           then decrement it with SEM_UNDO.  Then set a different
           flag to indicate that as soon as it is unlocked again,
           it is ready to be used  */
        if (   (ret=semSet(pcontext, semid, LOGSEM, 1))
            || (ret=semAdd(pcontext, semid, LOGSEM, -1, SEM_UNDO, SEM_RETRY))
            || (ret=semSet(pcontext, semid, RDYSEM, 1)))
            return ret;
 
        /* Store semid in the masterblock */
        pbkIPCData->NewSemId = semid;
        pdbcontext->pdbpub->loginSemid = semid;
 
        pdbcontext->psemids->size = numsems;
        pdbcontext->psemids->id = semid;
 
        psemdsc->numsems = numsems;
        psemdsc->semid = semid;
        pdbcontext->psemdsc = psemdsc;   /* the semaphore now exists */
        pcontext->logsemlk = 1;        
    }
    return 0;
}

/* PROGRAM: semRdyLog - called by BROKER when LOGSEM is ready for use  
 *
 * RETURNS: 0=OK, non-0 = non-OK                                     
 */
int
semRdyLog(dsmContext_t  *pcontext)      /* which database */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        SEMDSC  *psemdsc = pdbcontext->psemdsc;
        int      ret;
    
    /* these statements prevent infinite loops */
    if ( pdbcontext->psemdsc == NULL ||
         pcontext->logsemlk == 0)
        return -1;

    pcontext->logsemlk = 0;

    if (   (ret = semSet(pcontext, psemdsc->semid, RDYSEM, 2))
        || (ret = semAdd(pcontext, psemdsc->semid, LOGSEM, 1, SEM_UNDO, SEM_NORETRY)))
        return ret;

    return 0;
}

/* PROGRAM: semGetId - get the semaphore already created by broker 
 * RETURNS: 0=OK, non-0 = non-OK                                   
 */
int
semGetId(dsmContext_t *pcontext)  /* which database */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        int      semid;
        SEMDSC  *psemdsc;
    
    psemdsc = (SEMDSC *)stGet(pcontext, pdbcontext->privpool, sizeof(SEMDSC));

    psemdsc->pname = pdbcontext->pdbname;

    semid = semGetSet(pcontext, psemdsc->pname, 'U', 0/*no msg if ENOENT */);
    if (semid < 0)
    {   /* semGetSet printed error message for all errors except ENOENT */
        if ((errno==ENOENT) || (errno == EINVAL)) 
        {   
            MSGN_CALLBACK(pcontext, dbMSG008,pdbcontext->pdbname,errno);
            pdbcontext->psemdsc = NULL;
        }
        return -1;
    }

    pdbcontext->psemids = XSEMIDS(pdbcontext, pdbcontext->pdbpub->qsemids);

    psemdsc->semid = semid;
    pdbcontext->psemdsc = psemdsc;
    return 0;
}

/* PROGRAM: semDelete - delete a semaphore - to be called by broker 
 *
 * RETURNS:
 */
DSMVOID
semDelete(dsmContext_t *pcontext)
{       
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        SEMDSC  *psemdsc = pdbcontext->psemdsc;
        semid_t *psemids;
        ULONG    i;
    
    /* these statements prevent infinite loops */
    if (psemdsc == NULL) return;
    pdbcontext->psemdsc = NULL;

    semClear(pcontext, psemdsc->pname, psemdsc->semid);

    if (pdbcontext->pdbpub->numsemids > 1)
    {
        for (i=0, psemids = pdbcontext->psemids; 
                   i < pdbcontext->pdbpub->numsemids; i++, psemids++)
        {
            semClear(pcontext, psemdsc->pname, psemids->id);
        }
    }
}

/* PROGRAM: semRelease - release a semaphore 
 *
 * RETURNS:
 *
 */
DSMVOID
semRelease(dsmContext_t *pcontext _UNUSED_)
{   /* NO NEED TO RELEASE A SEMAPHORE ON UNIX */
}

/* PROGRAM: semUnlkLog - Unlock the LOGSEM login service semaphore 
 *
 * RETURNS:
 */
int
semUnlkLog(dsmContext_t *pcontext)      /* which database ? */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        SEMDSC      *psemdsc = pdbcontext->psemdsc;
        int          ret = 0;
    
    /* these statements prevent infinite loops */
    if ( psemdsc == NULL || pcontext->logsemlk == 0)
        return 0;

    pcontext->logsemlk--;

    if (pcontext->logsemlk == 0)
        /* this is the true unlock */
        if ((ret = semAdd(pcontext, psemdsc->semid, LOGSEM, 1, SEM_UNDO, 
                         SEM_NORETRY)))
        {
            pdbcontext->psemdsc = NULL;
        }
    return ret;
}

/* PROGRAM: semLockLog - Lock the LOGSEM login service semaphore 
 *          This program keeps a pushdown stack of locks        
 *          use semUnlkLog to pop the stack and eventually unlock       
 * RETURNS: 0=OK, non-0 = not OK                                
 */
int
semLockLog(dsmContext_t *pcontext)      /* which database ? */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        SEMDSC  *psemdsc;
        int      semid;
        int      val;
        int      ret;

    if ((pdbcontext->usertype & DBSHUT) && pdbcontext->argforce)
        return 0;
    psemdsc = pdbcontext->psemdsc;
    if (psemdsc == NULL) return -1;

    if (pcontext->logsemlk == 0)
    {   /* it isnt currently locked, so lock it now */
        semid = psemdsc->semid;

        /* keep retrying until BROKER finishes creating the semaphore */
        for(;;)
        {
            ret = semAdd(pcontext, semid, LOGSEM, -1, SEM_UNDO, SEM_RETRY);
            if (ret < 0)
            {
                if (errno == EINTR) continue;
                pdbcontext->psemdsc = NULL;
                dbExit(pcontext, 0, DB_EXBAD);
            }

            val = semValue(pcontext, semid, RDYSEM);
            if (val==0)
            {   /* The broker created the semaphore but didnt have */
                /* a chance to lock it, so unlock and try again    */
                ret = semAdd(pcontext, semid, LOGSEM, 1, SEM_UNDO, SEM_NORETRY);
                if (ret)
                {
                    MSGD_CALLBACK(pcontext,
                        "%BsemLockLog_1: semAdd ret = %d",ret);
                    pdbcontext->psemdsc = NULL;
                    dbExit(pcontext, 0, DB_EXBAD);
                }
                continue;
            }

            else if (val == 1)
            {   /* The broker died during initialization */
                errno = 0;
                MSGN_CALLBACK(pcontext, dbMSG008,pdbcontext->pdbname);
                ret = semAdd(pcontext, semid, LOGSEM, 1, SEM_UNDO, SEM_NORETRY);
                pdbcontext->psemdsc = NULL;
                dbExit(pcontext, 0, DB_EXNOSERV);
            }

            /* val is 2, broker finished initialization */
            /* and the semphore is successfully locked  */
            else if (val == 2)
                break;
            else
            {
                MSGD_CALLBACK(pcontext,"%BsemLockLog_2: semValue val = %d",val);
                dbExit(pcontext, 0, DB_EXBAD);
            }
        }

    }

    pcontext->logsemlk++;
    return 0;
}

/* PROGRAM: semCleanup - Cleans up orphaned semaphores 
 *
 * RETURNS: DSMVOID
 */
DSMVOID
semCleanup(dsmContext_t *pcontext, SEGMNT  *pseg)
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	dbshm_t	    *pdbpub;
        semid_t	    *psemids;
        ULONG        i;

    /* need to get these values out of the fist shared memory
       structure since pdbpub has not been setup yet */
    pdbpub = (dbshm_t *)XTEXT(pdbcontext, pseg->seghdr.qmisc);

    pdbcontext->psemids = XSEMIDS(pdbcontext, pdbpub->qsemids);

    semClear(pcontext, pdbcontext->pdbname, pdbpub->loginSemid);

    if (pdbpub->numsemids > 1)
    {
        for (i=0, psemids = pdbcontext->psemids; 
                   i < pdbpub->numsemids; i++, psemids++)
        {
            semClear(pcontext, pdbcontext->pdbname, psemids->id);
        }
    }
    return;
}
#endif /* UNIX */


/* PROGRAM: semGetSemidSize - get the size of the semaphore id table
 *
 * RETURNS: ULONG
 */
ULONG
semGetSemidSize(dsmContext_t     *pcontext)
{
    return (pcontext->pdbcontext->pdbpub->numsemids * sizeof(semid_t));
}


#if OPSYS==WIN32API

#ifndef _MAX_PATH
#define _MAX_PATH MAXUTFLEN
#endif

/* PROGRAM: semCreate [63 NT PLF 12/9/93]                         
 * create the login semaphore used for a database 
 * RETURNS: 0=OK, non-0 = non-OK                                
 */
int 
semCreate(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int      numsems = pdbcontext->pdbpub->argnusr + FRSTUSRSEM;
    SEMDSC  *psemdsc;
    char     szSemaphore[MAXUTFLEN];
    char     szFile[MAXUTFLEN];
    HANDLE   hsem;
    
    if (pdbcontext->argnoshm) 
        return 0;
    
    /* Create a semaphore table for this process */
    if (semTabCr(pcontext) != 0) /* [63 NT PLF 12/9/93] */
        return -1;
    
    psemdsc = (SEMDSC *)stGet(pcontext, pdbcontext->privpool, sizeof(SEMDSC));
    utapath(szFile, MAXUTFLEN, pdbcontext->pdbname, "");
    utmkosnm("sem.", szFile, szSemaphore, sizeof(szSemaphore), -1);
    
    hsem = osCreateSemaphore(pcontext, szSemaphore);

    if (hsem == NULL)     /* CreateSemaphore Failed */
        return -1;

    psemdsc->numsems = numsems;

    /* store the actual handle in private memory.  The login semaphore
       is the last element of the array */
    psemdsc->semid = (ULONG)hsem;

    pdbcontext->psemdsc = psemdsc; /* the semaphore now officially exists */
    pcontext->logsemlk = 1;       /* the LOGSEM is locked */
    return 0;
}

/* PROGRAM: semUsrCr - NT create the users semaphores used for a database 
 * RETURNS: 0=OK, non-0 = non-OK                                
 */
int
semUsrCr(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    char         szSemaphore[MAXUTFLEN];
    char         szFile[MAXUTFLEN];
    HANDLE       hsem;
    int          i;
    usrctl_t    *pusr;
  
    /* For each usrctl_t buffer */
    for (pusr=pdbcontext->p1usrctl, i=0;
         i < pdbcontext->pdbpub->argnusr ;
         i++, pusr++)
    {
        utapath(szFile, MAXUTFLEN, pdbcontext->pdbname, "");
        utmkosnm("sem.", szFile, szSemaphore, sizeof(szSemaphore),
            (int)pusr->uc_usrnum);
        
        hsem = osCreateSemaphore(pcontext, szSemaphore);
        if(!hsem)
        {
            /* Unable to create a system semaphore <semname>%s, too many. */
            MSGN_CALLBACK(pcontext, utMSG054, szSemaphore);
            return (-1);
        }
        
        *(pdbcontext->phsem + pusr->uc_usrnum) = hsem;
        
        NTPRINTF("semUsrCr - SEM %s , HSEM = %d", szSemaphore, hsem);
    }
    return (0);
}

/* PROGRAM: semRdyLog - OS2 called by BROKER when LOGSEM is ready for use  
 * 
 * RETURNS: 0=OK, non-0 = non-OK                                     
 */
int
semRdyLog(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    SEMDSC      *psemdsc = pdbcontext->psemdsc;
    int          ret = 0;
 
    /* these statements prevent infinite loops */
    if (pdbcontext->psemdsc == NULL || pcontext->logsemlk == 0) 
        return -1;
    
    ret = semUnlkLog(pcontext);
    if (ret)
        MSGD_CALLBACK(pcontext,"%LsemRdyLog: semUnlkLog ret = %d",ret);
    pcontext->logsemlk = 0;
 
    return 0;
}

/* PROGRAM: semGetId - NT get the login semaphore already created by broker 
 * 
 * RETURNS: 0=OK, non-0 = non-OK                                   
 */
int
semGetId(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    SEMDSC  *psemdsc;
    char     szSemaphore[MAXUTFLEN];
    HANDLE   hsem;
    char     szFile[MAXUTFLEN];
    int      ret;

    /* make the semaphore table. */
    if (semTabCr(pcontext) != 0)
        return -1;

    psemdsc = (SEMDSC *)stGet(pcontext, pdbcontext->privpool, sizeof(SEMDSC));
    psemdsc->pname = (TEXT *)utfcomp(pdbcontext->pdbname);
 
  /* check the "login" resource status ; ok to proceed ? */
    utapath(szFile, MAXUTFLEN, pdbcontext->pdbname, "");
    utmkosnm("sem.", szFile, szSemaphore, sizeof(szSemaphore), -1);

    hsem = osOpenSemaphore(szSemaphore);
    if (hsem == NULL)
    {
	ret = GetLastError();
	if (ret == ERROR_ACCESS_DENIED)
		MSGN_CALLBACK(pcontext, dbMSG032, pdbcontext->pdbname);
	else
	        MSGN_CALLBACK(pcontext, dbMSG008, pdbcontext->pdbname);
	return ret;	
    }
    hsem = OpenSemaphore(SEMAPHORE_ALL_ACCESS, TRUE, szSemaphore);
    if (hsem == NULL)
    {
        /* Unable to fine semaphore %s */
        MSGN_CALLBACK(pcontext, utMSG056, szSemaphore);
        return -1;
    }

    psemdsc->semid = (ULONG)hsem;
    pdbcontext->psemdsc = psemdsc;
    return 0;
}

/* PROGRAM: semUsrGet - NT get the users semaphores already created by broker
 * [63 NT PLF 12/9/93] 
 * This function creates a semaphore table for the usrctl_t [63 NT PLF 12/9/93] 
 * semaphores and loads the table with semaphore HANDLES  [63 NT PLF 12/9/93] 
 * RETURNS: 0=OK, non-0 = non-OK                                              
 */
int 
semUsrGet(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    char         szSemaphore[MAXUTFLEN];
    char         szFile[MAXUTFLEN];
    HANDLE       hsem;
    usrctl_t    *pusr;
    int          i;

    if(!pdbcontext->phsem)
    {
        /* Semaphore table has not been created */
        MSGN_CALLBACK(pcontext, utMSG111);
        return -1;
    }
    
    for (pusr = pdbcontext->p1usrctl, i=0;
         i < pdbcontext->pdbpub->argnusr ; 
         i++,pusr++)
    {
        utapath(szFile, MAXUTFLEN, pdbcontext->pdbname, "");
        utmkosnm("sem.", szFile, 
            szSemaphore, sizeof(szSemaphore), (int)pusr->uc_usrnum);

        hsem = osOpenSemaphore(szSemaphore);

        if (hsem == (HANDLE)0)
            return(-1);
        /* Store the semaphore in the newly created table */
        *(pdbcontext->phsem + pusr->uc_usrnum) = hsem;
    }
            
    return (0);
}

/* PROGRAM: semLockLog - NT Lock the LOGSEM login service semaphore 
 *          This program keeps a pushdown stack of locks        
 *          use semUnlkLog to pop the stack and eventually unlock 
 * RETURNS: 0=OK, non-0 = not OK                                
 */
int 
semLockLog(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    DWORD    dwRet;
    SEMDSC  *psemdsc;
    
    if ((pdbcontext->usertype & DBSHUT) && pdbcontext->argforce)
        return 0;
    
    psemdsc = pdbcontext->psemdsc;
    if (psemdsc == NULL) 
        return -1;
    
    if (pcontext->logsemlk == 0)
    {
        dwRet = WaitForSingleObject((HANDLE)psemdsc->semid, INFINITE);
        if (dwRet != 0)
        {
            /* Unable to request semaphore, error %d*/
            MSGN_CALLBACK(pcontext, utMSG058, dwRet);
            dbExit(pcontext, 0, DB_EXBAD);
        }
    }
    
    pcontext->logsemlk++;
    return 0;
}

/* PROGRAM: semUnlkLog - Unlock the LOGSEM login service semaphore 
 *
 * RETURNS: 
 *
 */
int
semUnlkLog(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    SEMDSC  *psemdsc = pdbcontext->psemdsc;

    /* these statements prevent infinite loops */
    if (psemdsc == NULL || pcontext->logsemlk == 0) 
        return 0;

    if (osReleaseSemaphore(pcontext, (HANDLE)psemdsc->semid) != 0)
    {
        MSGN_CALLBACK(pcontext, utMSG059 /* Unable to clear semaphore. */);
        return -1;
    }
    pcontext->logsemlk--;
     
    /* FIX, FIX, FIX
    *
    * if (pcontext->logsemlk == 0)
    *     pdbcontext->psemdsc == NULL;
    */
    return 0;
}


/* PROGRAM: semDelete
 *
 *
 * RETURNS:
 *
 */ 
DSMVOID
semDelete(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    SEMDSC      *psemdsc = pdbcontext->psemdsc;
    int          i;
    usrctl_t    *pusr;
 
    /* these statements prevent infinite loops */
    if (psemdsc == NULL)
        return;

    osCloseSemaphore((HANDLE)psemdsc->semid);
    
    pdbcontext->psemdsc = NULL;
 
    if (pdbcontext->p1usrctl == NULL)
        return;
 
    if(pdbcontext->phsem)
    {
        /* Refer bug 90-10-15-07, execute for loop only by broker ?, based on */        /* relmod */
        for (pusr = pdbcontext->p1usrctl, i=0;
                pusr && i < pdbcontext->pdbpub->argnusr; i++,pusr++)
        {
            if (*(pdbcontext->phsem + i) == 0L)
                continue;
            /* Clear out the semaphore table */
            if (osCloseSemaphore(*(pdbcontext->phsem +i)))
                return;
        }
 
        free(pdbcontext->phsem);
        pdbcontext->phsem = NULL;
    }
}

/* PROGRAM: semTabCr [63 NT PLF 12/9/93]
 *           Create a new NT usr semaphore table.
 *           This routine sets the phsem field in the dbcontext struct.
 *  RETURNS: 0 == OK, non 0 == non-OK.
 */
LOCALF int
semTabCr(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int nSemTableSize;
   
    if(pdbcontext->phsem)
        return -1;     /* error, already created */

    /* Allocate a semaphore table */
    nSemTableSize = (sizeof(ULONG) * (pdbcontext->pdbpub->argnusr+1)) ;
    pdbcontext->phsem = (HANDLE *)malloc(nSemTableSize);
    
    if(!pdbcontext->phsem)
        return -1;
    else
        return 0; /* Success */
}

/* PROGRAM: semRelease - release a semaphore 
 *
 * RETURNS:
 *
 */
DSMVOID 
semRelease(dsmContext_t *pcontext)
{
        /* nothing for now */
}

#endif /* WIN32API */
