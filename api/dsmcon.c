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

#define  PDSMSTGLOB      /* Allocation of the DSM Storage Pool */

#include <stdarg.h>      /* MUST BE INCLUDED BEFORE dstd.h (ALPHA OS) */
#include <stdio.h>  

#include "dbconfig.h"
#include "utospub.h"
#include "dbcontext.h"
#include "dbcode.h"      /* PROCODE */
#include "dbpub.h"
#include "drpub.h"       /* TEMPORARY: for drsigs call */
#include "drmsg.h"
#include "utmsg.h"
#include "dsmpub.h"
#include "bkcrash.h"
#include "rlai.h"
#include "stmprv.h"
#include "stmpub.h"      /* stor */
#include "latpub.h"
#include "latprv.h"
#include "rlai.h"        /* for aictl */
#include "rlprv.h"       /* for rlctl */
#include "usrctl.h"     
#include "utfpub.h"
#include "shmprv.h"
#include "bkpub.h"
#include "utspub.h"      /* stncop */
#include "utmpub.h"      /* utMsgReadMessages */
#include "utepub.h"      /* utDetermineVersion */
#include "uttpub.h"      /* for utConvertBytes */


#if OPSYS==WIN32API
#include <stdlib.h>
extern int fWin95;
HANDLE hWin95Mutex = NULL;
#endif

#include <setjmp.h>

LOCALF int dbContextDelete(dsmContext_t *pcontext);

LOCALF int dbContextInit(dbcontext_t **ppdbcontext);

LOCALF dsmStatus_t dbContextWriteOptions(dsmContext_t *pcontext);

#if LINUXX86
int readsymbolfile(const char *pname);
#endif /* LINUXX86 */

int
utMsgDefaultCallback(
    DSMVOID                 *param,
    dsmContext_t         *pcontext,
    TEXT                 *plogPrefix,
    int                   msgNumber,
    va_list              *args);

GLOBAL latch_t dsmContextLatch;    /* per process p1dsmContext latch */
GLOBAL dsmContext_t *p1dsmContext = 0; /* list of contexts for this process */

#if OPSYS == WIN32API
/* PROGRAM: dsmLookupEnv  - find environment key and returns its current value
 *
 * Parameters:	findKey   - environment key to be queried for a value
 *		keyValue  - holds value found for environment key
 *
 * RETURNS:
 * DSM_S_SUCCESS               - key was value and query for value succeeded
 * DSM_S_DSMLOOKUP_KEYINVALID  - An key was passed to look up
 * DSM_S_DSMLOOKUP_QUERYFAILED - query for registry/env value of key failed
 * DSM_S_DSMLOOKUP_INVALIDSIZE - query for key failed, target to small 
 */
dsmStatus_t
dsmLookupEnv(
	char	*findKey,
	char	*keyValue,
	DWORD	 dwBufferSize)
{
    /* base key in registry for install dir */
    HKEY    hBaseKey = HKEY_CURRENT_USER;
    /* triton key in registry for install dir */
    LPTSTR  lpKeyName      = BASEKEY_PROGRESS_TRITON;

    HKEY    hKey;                /* key handle for opened registry key */
    DWORD   dwValueType;         /* type of registry key*/
    LONG    lOpenKeyStatus = 0;  /* return status for registry key open call */
    LONG    lQueryKeyStatus = 0; /* return status for registry query key call */
    TEXT    *envTemp;            /* temporary holder for getenv() result */

    int     isNT;                /* return for DetermineVersion */
    TEXT    *envValue = NULL;    /* uppercased value for getenv on win9x */
    TEXT    *szTriton = "Triton_";
    dsmStatus_t returnStatus;    /* return value for this function */


    /* Were we passed a valid key for lpKeyName */
    if ((strcmp(findKey, TRITON_INSTALL_DIR))  &&
        (strcmp(findKey,TRITON_CACHE_DIR)))
    {
        /* we didnt get a valid key for lpKeyName */
        returnStatus = DSM_S_DSMLOOKUP_KEYINVALID;
	goto functionExit;
    }

    /* Open the registry key with read and query security masks*/
    lOpenKeyStatus = RegOpenKeyEx (hBaseKey, lpKeyName, 0, KEY_READ,&hKey); 
 
    /* Did we succeed in getting a key to query for a value? */
    if (lOpenKeyStatus == ERROR_SUCCESS)
    {
        /* query registry to get value of the findKey.                */
        /* note that dwBufferSize has been initialized to MAX_PATH +1 */
        /* this ensures compatibility with win9x and is updated       */
        /* when the call succeeds.                                    */
        lQueryKeyStatus = RegQueryValueEx( 
                                        hKey, 
                                        findKey, 
                                        0, 
                                        &dwValueType,
                                        keyValue,
                                        &dwBufferSize);

        /* were we successful in querying for a value */
	if (lQueryKeyStatus != ERROR_SUCCESS)
	{
	    /* query failed, close the handle to the open registry key */
	    RegCloseKey( hKey );
            goto finalTry;
	}

        /* close the handle to the open registry key */
        RegCloseKey( hKey );
	returnStatus = DSM_S_SUCCESS;
    }
    else
    {
finalTry:
	/*  Opening the  registry key failed.  See if the environment */
        /*  variable is set. Is this NT or win9x ?                    */
        isNT = utDetermineVersion();
	strcat(szTriton,findKey);
	
        /* is this win32 or NT? */
        if (isNT == 0)
        {
            /* its win9x, use all caps for env variable */
	    stcopy(envValue,szTriton);
  	    if (caps(envValue) != stlen(envValue))
     	    {
		returnStatus = DSM_S_DSMLOOKUP_QUERYFAILED;
		goto functionExit;
  	    }
        }

        if ((envTemp = (TEXT *)getenv(envValue ? envValue : szTriton)) != NULL)
        {
  	        /* getenv found a value, put it in the OUT parameter */
            stcopy(keyValue,envTemp);
  	    returnStatus = DSM_S_SUCCESS;
        }
        else
        {
            keyValue = NULL;
   	    switch(lQueryKeyStatus)
	    {
		case ERROR_MORE_DATA:
		    returnStatus = DSM_S_DSMLOOKUP_INVALIDSIZE;
		    break;
		default:
		    returnStatus = DSM_S_DSMLOOKUP_QUERYFAILED;
	  	    break;
             }
        }
    }

functionExit:
    return returnStatus;

}  /* end dsmLookupEnv */

#endif  /* OPSYS == WIN32API */

/* PROGRAM: dsmGetVersion - Returns the library version
 *
 *
 *
 * RETURNS: int
 */
int
dsmGetVersion()
{
    return DSM_API_VERSION;
}

/* PROGRAM: dsmContextCopy    - Create a new context and copy the
 *                              specified pieces of the source context
 *                              into it.
 *                              
 *                              
 *
 *  
 * RETURNS:
 * DSM_S_SUCCESS
 * 
 * 
 */
dsmStatus_t DLLEXPORT
dsmContextCopy(dsmContext_t *psource,
               dsmContext_t **pptarget,
               dsmMask_t    copyMode)
{
    dsmStatus_t  rc;


    if( (copyMode & DSMCONTEXTUSER) && !(copyMode & DSMCONTEXTDB))
    {
        /* Can't copy user context without also copying the
           database context.                                  */
        return DSM_S_INVALID_CONTEXT_COPY_MODE;
    }
        
    rc = dsmContextCreate(pptarget);
    if( rc )
    {
        goto failed;
    }

    if( copyMode & DSMCONTEXTBASE )
    {
        (*pptarget)->pmessageParm  = psource->pmessageParm;
        (*pptarget)->msgCallback   = psource->msgCallback;
        (*pptarget)->exitCallback  = psource->exitCallback;
    }
    
    
    if( copyMode & DSMCONTEXTDB )
    {
        (*pptarget)->pdbcontext = psource->pdbcontext;
    }
    else if( copyMode & DSMCONTEXTUSER)
    {
        (*pptarget)->pusrctl = psource->pusrctl;
        (*pptarget)->connectNumber = psource->connectNumber;
    }
    else
    {
        rc = DSM_S_INVALID_CONTEXT_COPY_MODE;
        goto failed;
    }
    
  failed:
    return rc;

}  /* end dsmContextCopy */


/* PROGRAM: dsmContextCreate  - allocates a context structure to be
 *                              used for subsequent DSMAPI calls.
 *                              
 *
 *  
 * RETURNS:
 * DSM_S_SUCCESS
 * DSM_S_DSMCONTEXT_ALLOCATION_FAILED
 * 
 */
dsmStatus_t DLLEXPORT
dsmContextCreate(dsmContext_t **ppContext)
{
    dsmContext_t *pcurrContext;
    dsmContext_t *pcontext;

    if( *ppContext != NULL )
    {
        return DSM_S_DSMBADCONTEXT;
    }
    
    /* Since there is no exit handling set up yet (because there is no
     * dsmcontext yet)  we will call the NON-FATAL version of stRent.
     * If the space allocation fails, the call will receive an error
     * return status.  It is up to the caller to handle it properly.
     */ 
    *ppContext = (dsmContext_t *)stRentIt(*ppContext, pdsmStoragePool,
                        sizeof(dsmContext_t), 0);

    if( *ppContext == NULL )
        return  DSM_S_DSMCONTEXT_ALLOCATION_FAILED;

    pcontext = *ppContext;

    PL_LOCK_DSMCONTEXT(&dsmContextLatch)

    /* chain new context into list of contexts */
    if (!p1dsmContext)
    {
        p1dsmContext = *ppContext;
    }
    else
    {
        for (pcurrContext = p1dsmContext;
             pcurrContext->pnextContext;
             pcurrContext = pcurrContext->pnextContext);
        pcurrContext->pnextContext = *ppContext;
    }
    pcontext->msgCallback = utMsgDefaultCallback;
    
    PL_UNLK_DSMCONTEXT(&dsmContextLatch)

    return DSM_S_SUCCESS;

}  /* end dsmContextCreate */


/* PROGRAM: dsmContextDelete  - allocates a context structure to be
 *                              used for subsequent DSMAPI calls.
 *                              
 *
 *  
 * RETURNS:
 * DSM_S_SUCCESS
 * DSM_S_USER_STILL_CONNECTED
 * 
 */
dsmStatus_t DLLEXPORT
dsmContextDelete(dsmContext_t *pcontext)
{
    dsmStatus_t   rc = DSM_S_SUCCESS;
    dsmContext_t  *pcurrContext;
    
    if( pcontext->pusrctl != NULL )
    {
        return DSM_S_USER_STILL_CONNECTED;
    }

    /* TODO: if and when dbContextDelete actually does something,
     *       we will need a private latch on pcontext->pdbcontext->useCount
     *       to avoid race conditions with other threads.
     */
    if ( pcontext->pdbcontext->useCount == 0 )
    {
        rc = dbContextDelete(pcontext);
    }

    if (rc == DSM_S_SUCCESS)
    {
        /* remove context from list of contexts */
        PL_LOCK_DSMCONTEXT(&dsmContextLatch)

        if (p1dsmContext == pcontext)
            p1dsmContext = pcontext->pnextContext;
        else
        {
            for (pcurrContext = p1dsmContext;
                 pcurrContext->pnextContext != pcontext;
                 pcurrContext = pcurrContext->pnextContext);

            pcurrContext->pnextContext = pcontext->pnextContext;
        }

        PL_UNLK_DSMCONTEXT(&dsmContextLatch)

        /* Since we are vacating the pcontext itself, pass 0 as the
         * reference dsm context
         */
        stVacate(0, pdsmStoragePool,(TEXT *)pcontext);
    }

    return rc;

}  /* end dsmContextDelete */


/* PROGRAM: dsmContextGetLong - Returns the value for the input tag
 *
 * RETURNS:
 */
dsmStatus_t DLLEXPORT
dsmContextGetLong(
    dsmContext_t        *pcontext,  /* IN/OUT database context */
    dsmTagId_t          tagId,      /* Tag identifier          */   
    dsmTagLong_t        *ptagValue)   /* tagValue                */
{
    dsmStatus_t  rc = 0;
    dbcontext_t *pdbcontext;
    dbshm_t     *pdbshm;
    
    if( pcontext == NULL )
    {
        /* Invalid context          */
        rc =  DSM_S_DSMBADCONTEXT;
        goto errorReturn;
    }

    if( pcontext->pdbcontext == NULL )
    {
        rc = DSM_S_DSMBADCONTEXT;
        goto errorReturn;
    }

    if( tagId >= DSM_TAGUSER_BASE && pcontext->pusrctl == NULL )
    {
        rc = DSM_S_NO_USER_CONTEXT;
        goto errorReturn;
    }

    pdbcontext = pcontext->pdbcontext;
    pdbshm     = pdbcontext->pdbpub;
 
    switch (tagId)
    {
      /**** pdbcontext (TAGDB) type tags ****/
      case DSM_TAGDB_ACCESS_ENV:
        *ptagValue = pdbcontext->accessEnv;
        break;

      case DSM_TAGDB_ACCESS_TYPE:
        *ptagValue = pdbcontext->usertype;
        break;

      case DSM_TAGDB_MODULE_RESTRICTIONS:
        *ptagValue = pcontext->pdbcontext->modrestr;
        break;


      /**** pdbshm (TAGDB) type tags ****/
      case DSM_TAGDB_BROKER_TYPE:
        *ptagValue = pdbshm->brokertype;
        break;

      case DSM_TAGDB_SHUTDOWN_PID:
        *ptagValue = pdbshm->shutdnpid;
        break;

      case DSM_TAGDB_SHUTDOWN_INFO:
        *ptagValue = pdbshm->shutdnInfo;
        break;

      case DSM_TAGDB_SHUTDOWN_STATUS:
        *ptagValue = pdbshm->shutdnStatus;
        break;

      case DSM_TAGDB_USRFL: /*do we have any user records*/
        *ptagValue = (dsmTagLong_t) (pdbshm->br_usrfl);
        break;

      case DSM_TAGDB_SHUTDOWN:
        *ptagValue = pdbshm->shutdn;
        break;

      case DSM_TAGDB_INUSE:
        *ptagValue = pcontext->pdbcontext->inservice;
        break;

      /**** pusrctl (TAGUSER) type tags ****/
      case DSM_TAGUSER_LOCK_TIMEOUT:
        *ptagValue = pcontext->pusrctl->uc_lockTimeout;
        break;

      case DSM_TAGDB_GET_USRNUM:
        *ptagValue = pcontext->pusrctl->uc_usrnum;
        break;

      case DSM_TAGUSER_BLOCKLOCK:
            /* Determines if this user will wait or queue for record locks. */
            *ptagValue = pcontext->pusrctl->uc_block ? 1 : 0;
            break;
      case DSM_TAGUSER_TX_NUMBER:
            /* Return the current transaction number for the user   */
            *ptagValue = pcontext->pusrctl->uc_task;
            break;
      case DSM_TAGUSER_PID:
           *ptagValue = pcontext->pusrctl->uc_pid;
           break;

      default:
        /* Invalid tag                                         */
        rc = DSM_S_INVALID_TAG;
        break;
    }
    
errorReturn:
    
    return rc;

}  /* end dsmContextGetLong */


/* dsmContextGetString - Return the specified tag value. 
 *
 * RETURNS:
 *
 */
dsmStatus_t DLLEXPORT
dsmContextGetString(
    dsmContext_t        *pcontext,  /* IN/OUT database context */
    dsmTagId_t          tagId,      /* Tag identifier          */
    dsmLength_t         tagLength,  /* Length of string        */
    dsmBuffer_t         *ptagString)  /* tagValue              */
{
    dsmStatus_t         rc = 0;
    
    if( pcontext == NULL )
        /* Invalid context          */
        return DSM_S_DSMBADCONTEXT;

    if( pcontext->pdbcontext == NULL )
    {
        return DSM_S_DSMBADCONTEXT;
    }

    if( tagId >= DSM_TAGUSER_BASE && pcontext->pusrctl == NULL )
    {
        return DSM_S_NO_USER_CONTEXT;
    }

    if( tagId == DSM_TAGDB_DBNAME )
    {
        stncop(ptagString, pcontext->pdbcontext->pdbname, tagLength);
    }
    else if (tagId == DSM_TAGUSER_NAME)
    {
        stncop(ptagString,
               XTEXT(pcontext->pdbcontext,pcontext->pusrctl->uc_qusrnam),
               tagLength);
    }
    else if ( tagId == DSM_TAGUSER_PASSWORD )
    {
        /* Not currently implemented but accepted through the
           DSM interface                                      */
        ;
    }           
    else
    {
        rc = DSM_S_INVALID_TAG;
    }
    
    return rc;

}  /* end dsmContextGetString */

    
/* PROGRAM: dsmContextSetLong - Initializes a context with the tagValue
 * for the parameter identified by the tagId.  A context is initialized
 * after it is created.  A context can be intialized more than once
 * and the initializations are additive.
 *
 * RETURNS:
 */
dsmStatus_t DLLEXPORT
dsmContextSetLong(
    dsmContext_t        *pcontext,  /* IN/OUT database context */
    dsmTagId_t          tagId,      /* Tag identifier          */   
    dsmTagLong_t        tagValue)   /* tagValue                */
{
    dsmStatus_t         rc = 0;
    dbcontext_t        *pdbcontext;
    dbshm_t            *pdbshm;
    crashtest_t        *pcrashtest;
    

    if( pcontext == NULL )
        /* Invalid context          */
        return DSM_S_DSMBADCONTEXT;

    if( pcontext->pdbcontext == NULL )
    {
        /* Allocate a database context structure     */
        rc = dbContextInit(&pcontext->pdbcontext);
    }

    pdbcontext = pcontext->pdbcontext;
    pdbshm     = pdbcontext->pdbpub;

    if( tagId >= DSM_TAGUSER_BASE && pcontext->pusrctl == NULL )
    {
        rc = DSM_S_NO_USER_CONTEXT;
        goto errorReturn;
    }
 
    switch (tagId)
    {
        case DSM_TAGCONTEXT_NO_LOGGING:
          pcontext->noLogging = tagValue;
          /* Issue a synchronous checkpoint
             when turning recovery logging on or off  */
          rlSynchronousCP(pcontext);
          break;

        /**** pdbcontext (TAGDB) type tags ****/
        case DSM_TAGDB_ACCESS_ENV:
            switch (tagValue)
            {
                case DSM_4GL_ENGINE: 
                case DSM_SQL_ENGINE:
                case DSM_JAVA_ENGINE:
                case DSM_JUNIPER_BROKER:
                    pdbcontext->accessEnv = (ULONG)tagValue;
                    break;
                default:
                    rc = DSM_S_INVALID_TAG_VALUE;
                    goto errorReturn;
            }
            break;
        case DSM_TAGDB_ACCESS_TYPE:
            switch (tagValue)
            {
                case DSM_ACCESS_SHARED:
                    pdbcontext->usertype = SELFSERVE;
                    /* turn off bi threshold and bi stall */
                    pdbshm->bithold = 0;
                    pdbshm->bistall = 0;
                    break;
                case DSM_ACCESS_STARTUP:
                    pdbcontext->usertype = BROKER;
                    break;
                case DSM_ACCESS_SERVER:
                    pdbcontext->usertype = DBSERVER;
                    break;
                case DSM_ACCESS_SINGLE_USER:
                    pdbcontext->usertype = 0;
                    pdbcontext->arg1usr = 1;
                    pdbcontext->argnoshm = 1;
                    pdbshm->bistall = 0;
                    break;
                default:
                    rc = DSM_S_INVALID_TAG_VALUE;
                    goto errorReturn;
            }
            break;
        case DSM_TAGDB_MODULE_RESTRICTIONS:
            pdbcontext->modrestr = (ULONG)tagValue; 
            break;

 
      case DSM_TAGDB_TABLE_BASE:
        pdbcontext->pdbpub->argtablebase =  (dsmTable_t)tagValue;
        break;
 
      case DSM_TAGDB_TABLE_SIZE:
        pdbcontext->pdbpub->argtablesize = (dsmTable_t)tagValue;
        break;
 
      case DSM_TAGDB_INDEX_BASE:
        pdbcontext->pdbpub->argindexbase =  (dsmIndex_t)tagValue;
        break;
 
      case DSM_TAGDB_INDEX_SIZE:
        pdbcontext->pdbpub->argindexsize = (dsmIndex_t)tagValue;
        break;
 
        case DSM_TAGDB_MIN_CLIENTS_PER_SERVER: /* (-Mi) */
            pdbcontext->minsv = (int)tagValue;
            break;
        case DSM_TAGDB_MIN_SERVER_PORT: /* (-minport) */
            pdbcontext->minport = (COUNT)tagValue;
            break;
        case DSM_TAGDB_MAX_SERVER_PORT: /* (-maxport) */
            pdbcontext->maxport = (COUNT)tagValue;
            break;
        case DSM_TAGDB_MAX_SERVERS_PER_PROTOCOL: /* (-Mp) */
            pdbcontext->maxsvprot = (COUNT)tagValue;
            break;
        case DSM_TAGDB_MAX_SERVERS_PER_BROKER: /* (-Mpb) */
            pdbcontext->maxsvbrok = (COUNT)tagValue;
            break;
        case DSM_TAGDB_NO_SHM:         /* (-noshm) */
            pdbcontext->argnoshm = 1;
            break;
        case DSM_TAGDB_FORCE_ACCESS: /* (-F) */
            pdbcontext->confirmforce = 1;
            pdbcontext->argforce = 1;
            break;
        case DSM_TAGDB_SERVER_ID: /* */
            pdbcontext->serverId = (int)tagValue;
            break;
        case DSM_TAGDB_UNLINK_TEMPDB: /* -z3 */
            pdbcontext->unlinkTempDB = tagValue ? 0 : 1;
            break;
#if ASSERT_ON || ASSERT_Z3 /* ignore the -z3 parameter if not ASSERT_ON */
        case DSM_TAGDB_ZNFS: /* -z3 */
            pdbcontext->znfs = tagValue ? 1 : 0;
            break;
#endif

        /**** pdbshm (pdbpub) type tags ****/
        case DSM_TAGDB_MAX_CLIENTS: /* (-Ma) */
            pdbshm->argmaxcl = (int)tagValue;
            break;
        case DSM_TAGDB_MAX_SERVERS: /* (-Mn) */
            pdbshm->argnsrv = (int)tagValue; /* max number of servers + Broker */
            pdbshm->argnusr = pdbshm->argnsrv + pdbshm->argnclts; /*clnts+srvr*/
            break;
        case DSM_TAGDB_MSG_BUFFER_SIZE: /* (-Mm) */
            pdbshm->argsdmsg = (int)tagValue;
            break;
        case DSM_TAGDB_AI_BUFFERS: /* (-aibufs) */
            pdbshm->argaibufs = (COUNT)tagValue;
            break;
        case DSM_TAGDB_AI_STALL: /* (-aistall) */
            pdbshm->aistall = 1;
            break;
        case DSM_TAGDB_BI_BUFFERS: /* (-aibufs) */
            pdbshm->argbibufs = (COUNT)tagValue;
            break;
        case DSM_TAGDB_BI_BUFFERED_WRITES: /* (-r and -R) */
            if (tagValue)
                pdbshm->useraw = 0; /* buffered bi writes (-r) */
            else
            {
                /* if caller doesn't specify no-integrity and specifies -R then
                 * we will set the bi writes unbuffered - otherwise
                 * since the caller specified no-integrity,
                 * we ignore -R and make the bi writes buffered.
                 */
                if (pdbshm->fullinteg)
                {
                   /* caller wants INTEGRITY (default)
                    * specifically assign unbuffered bi writes (-R) (default)
                    */
                   pdbshm->useraw = 1;
                }
                else
                {
                    /* no-integrity forces -r (non-raw - buffered bi writes) */
                    pdbshm->useraw = 0;
                }

            }
            break;
        case DSM_TAGDB_BI_CLUSTER_AGE: /* (-G) */
            pdbshm->rlagetime = (int)tagValue;
            break;
        case DSM_TAGDB_BI_CLUSTER_SIZE: /* -bi */
            pdbcontext->rlclsize = (ULONG)tagValue;
            break;
        case DSM_TAGDB_FLUSH_AT_COMMIT: /* (-Mf) */
            pdbshm->tmenddelay = (COUNT)tagValue;
            break;
        case DSM_TAGDB_DB_BUFFERS: /* (-B) */
#if 0
            if (tagValue > MAXBUFS)
            {
                /* MSGN_CALLBACK(pcontext,drMSG197,tagValue,(LONG)MAXBUFS); */
                rc = DSM_S_FAILURE;
                break;
            }
#endif
            pdbshm->argbkbuf = (int)tagValue;
            break;
        case DSM_TAGDB_DIRECT_IO: /* (-directio) */
            pdbshm->directio = (int)tagValue;
            break;
        case DSM_TAGDB_HASH_TABLE_SIZE: /* (-hash) */
            pdbshm->arghash = (ULONG)tagValue;
            break;
        case DSM_TAGDB_PWQ_SCAN_DELAY: /* (-pwqdelay) */
            pdbshm->pwqdelay = (COUNT)tagValue;
            break;
        case DSM_TAGDB_PWQ_MIN_WRITE: /* (-pwqmin) */
            pdbshm->pwqmin = (COUNT)tagValue;
            break;
        case DSM_TAGDB_PW_SCAN_DELAY: /* (-pwsdelay) */
            pdbshm->pwsdelay = (COUNT)tagValue;
            break;
        case DSM_TAGDB_PW_MAX_WRITE: /* (-pwwmax) */
            pdbshm->pwwmax = (LONG)tagValue;
            break;
        case DSM_TAGDB_PW_SCAN_SIZE: /* (-pwscan) */
            pdbshm->pwscan = (LONG)tagValue;
            break;
        case DSM_TAGDB_SHMEM_OVERFLOW_SIZE: /* (-Mxs) */
            pdbshm->argxshm = (LONG)tagValue;
            break;
        case DSM_TAGDB_SPIN_AMOUNT: /* (-spin) */
#ifdef LOCK_RETRIES
            pdbshm->argspin = (LONG)tagValue;
#endif
            break;
        case DSM_TAGDB_MUX_LATCH: /* (-mux) */
            pdbshm->argmux = (LONG)tagValue;
            break;
        case DSM_TAGDB_MT_NAP_TIME: /* (-nap) */
            pdbshm->argnap = (COUNT)tagValue;
            break;
        case DSM_TAGDB_MAX_MT_NAP_TIME: /* (-npmax) */
            pdbshm->argnapmax = (COUNT)tagValue;
            break;
        case DSM_TAGDB_SHUTDOWN:
            switch (tagValue)
            {
              case 0:
              case DSM_SHUTDOWN_NORMAL:
              case DSM_SHUTDOWN_ABNORMAL:
                pdbshm->shutdn = (GTBOOL)tagValue;
                break;
              default:
                rc = DSM_S_INVALID_TAG_VALUE;
            }
            break;
        case DSM_TAGDB_CRASH_PROTECTION: /* (-i) */
            if (tagValue)
            {
                /* caller wants crash protection (default) */
                pdbshm->fullinteg = 1;
            }
            else
            {
                /* caller wants "NO-INTEGRITY" */
                pdbshm->fullinteg = 0;
                /* no-integrity forces -r (non-raw - buffered bi writes) */
                pdbshm->useraw = 0;
                /* turn off bi threshold and bi stall */
                pdbshm->bithold = 0;
                pdbshm->bistall = 0;
            }
            break;
        case DSM_TAGDB_MAX_USERS:      /* (-n) */
            /* NOTE: There is no licencing check done here! */
            pdbshm->argnclts = (int)tagValue; /* max number of clients + proshut */
            pdbshm->argnusr  = (int)(tagValue + pdbshm->argnsrv); /* clnts + srvrs */
            break;
        case DSM_TAGDB_MAX_LOCK_ENTRIES:  /* (-L) */
            pdbshm->arglknum = (LONG)tagValue;
            break;
        case DSM_TAGDB_SEMAPHORE_SETS:
            pdbshm->numsemids = (ULONG)tagValue;
            break;
        case DSM_TAGDB_SHUTDOWN_INFO:
            pdbshm->shutdnInfo = (int)tagValue;
            break;
        case DSM_TAGDB_SHUTDOWN_STATUS:
            pdbshm->shutdnStatus = (int)tagValue;
            break;

        case DSM_TAGDB_BI_THRESHOLD:
            /* Only set a bi threshold if running with full integrity.
               bi threshold can not be less than 3MB.
               We do the 3MB check in drargs as well */

            if (pdbshm->fullinteg && 
                (pdbcontext->arg1usr || pdbcontext->usertype == BROKER ))
            {
                pdbshm->bithold =
                   (tagValue == 0 || tagValue > 3) ? tagValue : 3;
            }
            break;
        case DSM_TAGDB_BI_STALL:
            /* only turn on bi stall if bi threshold is set
               and this a broker or is not single user */
            if (pdbshm->bithold && 
                (!pdbcontext->arg1usr || pdbcontext->usertype == BROKER))
               pdbshm->bistall = tagValue ? 1 : 0;
            break;

        case DSM_TAGDB_TT_WB_NUM:
            /* Word break rule number for temp table db only */
            pdbcontext->ttWordBreakNumber = (LONG)tagValue;
            break;

        case DSM_TAGDB_DEFAULT_LOCK_TIMEOUT:
            if( tagValue > 0 )
                pdbshm->lockWaitTimeOut = (LONG)tagValue;
            break;

        /**** usrctl (TAGUSER) type tags ****/
        case DSM_TAGUSER_LOCK_TIMEOUT:
            if( tagValue < 0 ) tagValue = 0;
            pcontext->pusrctl->uc_lockTimeout = (ULONG)tagValue;
            break;
        case DSM_TAGUSER_SQLHLI:
            pcontext->pusrctl->uc_sqlhli = (LONG)tagValue;
            break;
        case DSM_TAGUSER_CRASHWRITES:
            /* Set the number of database writes to crash after variable */
            if (pdbcontext->p1crashTestAnchor)
            {
                /* Locate the user of interest */
                pcrashtest = pdbcontext->p1crashTestAnchor + 
                                pcontext->pusrctl->uc_usrnum;
                pcrashtest->crash = tagValue;
            }
            break;
        case DSM_TAGUSER_CRASHSEED:
            /* Populate the random seed variable for crashing */
            if (pdbcontext->p1crashTestAnchor)
            {
                pcrashtest = pdbcontext->p1crashTestAnchor +
                     pcontext->pusrctl->uc_usrnum; 
                pcrashtest->cseed = tagValue;
            }
            break;
        case DSM_TAGUSER_CRASHBACKOUT:
            /* Set the crash during transaction backout variable */
            if (pdbcontext->p1crashTestAnchor)
            {
                pcrashtest = pdbcontext->p1crashTestAnchor +
                     pcontext->pusrctl->uc_usrnum;
                pcrashtest->backout = tagValue;
            }
            break;
        case DSM_TAGUSER_ROBUFFERS:
            /* Establish private read only buffers for this user. */
            bmGetPrivROBuffers(pcontext, (UCOUNT)tagValue);
            break;
        case DSM_TAGUSER_BLOCKLOCK:
            /* Determines if this user will wait or queue for record locks.
             * 1 = wait, 0 = queue */
            pcontext->pusrctl->uc_block = tagValue ? 1 : 0;
            break;

        default:
            /* Invalid tag   */
            rc = DSM_S_INVALID_TAG;
            break;
    }
    
errorReturn:
    
    return rc;

}  /* end dsmContextSetLong */


/* PROGRAM: dsmContextSetString - Initializes a context with the tagValue
 * for the parameter identified by the tagId.  A context is initialized
 * after it is created.  A context can be intialized more than once
 * and the initializations are additive.
 *
 * RETURNS:
 *
 */
dsmStatus_t DLLEXPORT
dsmContextSetString(
    dsmContext_t        *pcontext,  /* IN/OUT database context */
    dsmTagId_t          tagId,      /* Tag identifier          */
    dsmLength_t         tagLength,  /* Length of string        */
    dsmBuffer_t         *ptagString)  /* tagValue              */
{
    dsmStatus_t         rc = 0;
    
    if( pcontext == NULL )
        /* Invalid context          */
        return DSM_S_DSMBADCONTEXT;

    if( pcontext->pdbcontext == NULL)
    {
        /* Allocate a database context structure     */
        rc = dbContextInit(&pcontext->pdbcontext);
    }

    switch (tagId)
    {
      case DSM_TAGDB_DBNAME:
        if( pcontext->pdbcontext->pbkctl != NULL )
        {
            /* We're already connected to this database so
               we can't change the name                     */
            return  DSM_S_DB_ALREADY_CONNECTED;
        }

        pcontext->pdbcontext->pdbname = (TEXT *)
            stGet(pcontext, pcontext->pdbcontext->privpool, tagLength + 1);
        stncop(pcontext->pdbcontext->pdbname, ptagString, tagLength);
        /* Open the log file as early as possible */
        dbLogOpen(pcontext, pcontext->pdbcontext->pdbname);
      break;
      
      case DSM_TAGDB_DATADIR:
          pcontext->pdbcontext->pdatadir = (TEXT *)
              stGet(pcontext, pcontext->pdbcontext->privpool, tagLength + 1);
          stncop(pcontext->pdbcontext->pdatadir,ptagString, tagLength);
      break;
      
      case DSM_TAGDB_PROTOCOL_NAME: /* (-N) */
        pcontext->pdbcontext->argsnet = (TEXT *)
            stGet(pcontext, pcontext->pdbcontext->privpool, tagLength + 1);
        stncop(pcontext->pdbcontext->argsnet, ptagString, tagLength);
        break;

      case DSM_TAGDB_HOST: /* (-H) */
        pcontext->pdbcontext->phost = (TEXT *)
            stGet(pcontext, pcontext->pdbcontext->privpool, tagLength + 1);
        stncop(pcontext->pdbcontext->phost, ptagString, tagLength);
        break;

      case DSM_TAGDB_SERVICE_NAME: /* (-S) */
        pcontext->pdbcontext->pserver = (TEXT *)
            stGet(pcontext, pcontext->pdbcontext->privpool, tagLength + 1);
        stncop(pcontext->pdbcontext->pserver, ptagString, tagLength);
        break;

      case DSM_TAGDB_COLLATION_NAME: /* (-cpcoll) */
#if 0
        pcontext->pdbcontext->CollationName = (TEXT *)
            stGet(pcontext, pcontext->pdbcontext->privpool, tagLength + 1);
        stncop(pcontext->pdbcontext->CollationName, ptagString, tagLength);
#endif
        break;

      case DSM_TAGDB_CODE_PAGE_NAME: /* (-cpdb) */
#if 0
        pcontext->pdbcontext->pCodePageName = (TEXT *)
            stGet(pcontext, pcontext->pdbcontext->privpool, tagLength + 1);
        stncop(pcontext->pdbcontext->pCodePageName, ptagString, tagLength);
#endif
        break;

      case DSM_TAGUSER_NAME:
#if 0
        rc = dsmUserSetName(pcontext,ptagString,
                            &pcontext->pusrctl->uc_qusrnam);
        if( rc == DSM_S_SUCCESS )
        {
            MSGN_CALLBACK(pcontext, drMSG642,
                 (LONG)pcontext->pusrctl->uc_usrnum,
                 ptagString);
        }
#else
        pcontext->puserName = ptagString;
#endif
        break;
    
      case DSM_TAGUSER_PASSWORD:
        /* Not currently implemented but accepted through the
           DSM interface                                      */
        break;

      case DSM_TAGDB_MSGS_FILE:
          pcontext->pdbcontext->pmsgsfile = (TEXT *)
              stGet(pcontext, pcontext->pdbcontext->privpool, tagLength + 1);
          stncop(pcontext->pdbcontext->pmsgsfile, ptagString, tagLength);
          /* load the message database */
          rc = utMsgReadMessages(pcontext);
      break;
      
      case DSM_TAGDB_SYMFILE:
#if LINUXX86
          if (!(readsymbolfile(ptagString)))
          {
              MSGN_CALLBACK(pcontext, drMSG688, ptagString);
          }
#endif /* LINUXX86 */
      break;
      
      default:
        rc = DSM_S_INVALID_TAG;
        break;
    }
    
    return rc;

}  /* end dsmContextSetString */


/* PROGRAM: dbContextInit - Allocate and intialize a database context structure
 *
 * RETURNS:
 */
LOCALF int
dbContextInit(dbcontext_t **ppdbcontext)
{
    dbcontext_t *pdbcontext;
    dbshm_t     *pdbshm;

    if ( *ppdbcontext == NULL)
    {
       pdbcontext = (dbcontext_t *)stRent((dsmContext_t*)NULL, 
                                    pdsmStoragePool, sizeof(dbcontext_t));
       pdbcontext->privpool = (STPOOL *)stRent((dsmContext_t*)NULL,
                                    pdsmStoragePool, sizeof(STPOOL));
    
       *ppdbcontext = pdbcontext;
    
       /* initialize segment table for pdbcontext */
       pdbcontext->pshsgctl = shmCreateSegmentTable(pdbcontext->privpool);
    
       pdbcontext->dbcode        = PROCODE;
       pdbcontext->pnextdb       = NULL;
       pdbcontext->argprbuf      = -1;   /* default are no private buffers */
       pdbcontext->minsv         = DSM_DEFT_MIN_CLIENTS_PER_SERVER;
       pdbcontext->argsnet       = (TEXT *)""; /* default for -N EXCELAN */
       pdbcontext->phost         = NULL;
       pdbcontext->pserver       = NULL;
       pdbcontext->bklkfd        = -1;
       pdbcontext->protovers     = 0;
       pdbcontext->argzallowdlc  = 0;
       pdbcontext->znfs          = 0;
       pdbcontext->unlinkTempDB  = 1;

       /* This sets the default code page.  DSM users should pass the
          correct value based on the charset for the db using
          dsmContextSetXXX.  The 4GL uses -charset to set this */ 
#if 0
       pdbcontext->pCpAttrib   = (TEXT *)&initCpTableData;
#endif

       pdbcontext->pdbpub = (dbshm_t *)stRent((dsmContext_t*)NULL,
                                          pdsmStoragePool,
                                          sizeof(dbshm_t));
       pdbcontext->privdbpub = 1;  /* have private dbpub in pgstpool */

       pdbshm = pdbcontext->pdbpub;
 
       pdbshm->argnusr    = DSM_DEFT_USRS + DSM_DEFT_NSERVERS + 1;
       pdbshm->rlagetime  = DSM_DEFT_LAGTIME;
       pdbshm->arglknum   = DSM_DEFT_LK_ENTRIES;
       pdbshm->argnclts   = DSM_DEFT_USRS + 1; /* add proshut */
       pdbshm->argmaxcl   = DSM_DEFT_MAX_CLIENTS;
       pdbshm->fullinteg  = 1;
       pdbshm->useraw     = 1;
       pdbshm->argnsrv    = DSM_DEFT_NSERVERS; /* 4 servers + 1 broker */
       pdbshm->argspin    = 1;        /* spinlocks on by default */
       pdbshm->argmux     = 1;        /* use muxlatches */
       pdbshm->argnap     = NAP_INIT; /* min mtnap time */
       pdbshm->argnapinc  = NAP_INCR; /* mtnap time increment */
       pdbshm->argnapmax  = NAP_MAX;  /* max mtnap time */
       pdbshm->argxshm    = -1;     /* excess shared memory size */

       /* iterations betw mtnap time increases */
       pdbshm->argnapstep = NAP_STEP;
       pdbshm->quietpoint  = 0;       /* Normal Processing State */
       pdbshm->light_ai    = 0;
       pdbshm->tmenddelay  = -1;
       pdbshm->pwqdelay    = DSM_DEFT_PWQDELAY;
       pdbshm->pwqmin      = DSM_DEFT_PWQMIN;
       pdbshm->pwsdelay    = DSM_DEFT_PWSDELAY;
       pdbshm->pwwmax      = DSM_DEFT_PWWMAX;
       pdbshm->argsdmsg    = MAXMSGSZ;
       pdbshm->argsdbuf    = STDBUFSZ;

       /* default depth of an index cursor */
       pdbshm->argmaxcs    = DSM_DEFT_CURSENT;

       /* number of bi buffers - code breaks if less */
       pdbshm->argbibufs   = DSM_DEFT_BIBUFS;
       pdbshm->argaibufs   = DSM_DEFT_AIBUFS; /* # of ai buffers if ai is on */

       /* Enable group commit as the default with a 1 milli-second delay. */
       pdbshm->groupDelay  = 0;
       pdbshm->argtpbuf    = -1;   /* no private buffers */
       pdbshm->configusers = -1;   /* no limit on users */
       pdbshm->currentusers = 0;
       pdbshm->dblic.userThreshold  = -1; /* value not set by user */
       pdbshm->dblic.reportInterval = -1; /* value not set by user */
       pdbshm->numsemids   = 1; 	/* number of sem sets */
       pdbshm->bithold     = 0; 	/* bi threshold setting  */
       pdbshm->bistall     = 0;         /* stall if threshold set */
    }

    return 0;

}  /* end dbContextInit */


/* PROGRAM: dbContextDelete - undoes what dbContextInit does
 *
 * RETURNS:
 */
LOCALF int
dbContextDelete(
        dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

#if 1
    if ( pdbcontext->useCount != 0 )
        return DSM_S_USER_STILL_CONNECTED;

    /* if pointer to dbcontext structure is valid */
    if (pdbcontext)
    {
        /* if pointer to privpool is valid */
        if (pdbcontext->privpool)
        {
            /* free the private pool before we give it back */
            stFree(pcontext, pdbcontext->privpool);
 
            stVacate(pcontext, pdsmStoragePool, (TEXT *)pdbcontext->privpool);
        }
 
        /* vacate dbpub only if it is private and valid */
        if (pdbcontext->privdbpub && pdbcontext->pdbpub)
        {
            stVacate(pcontext, pdsmStoragePool, (TEXT *)pdbcontext->pdbpub);
        }

        stVacate(pcontext, pdsmStoragePool, (TEXT *)pdbcontext);
    }

#endif

    return DSM_S_SUCCESS;

}  /* end dbContextDelete */


/* PROGRAM: dsmContextSetVoidFunction - Initializes a context with the tagValue
 * for the parameter identified by the tagId.  A context is initialized
 * after it is created.  A context can be intialized more than once
 * and the initializations are additive.
 *
 * RETURNS:
 */
dsmStatus_t DLLEXPORT
dsmContextSetVoidFunction(
    dsmContext_t         *pcontext,  /* IN/OUT database context */
    dsmTagId_t            tagId,      /* Tag identifier          */   
    dsmTagVoidFunction_t  tagValue)   /* tagValue                */
{
    dsmStatus_t         rc = DSM_S_SUCCESS;
    
    if( pcontext == NULL )
        /* Invalid context          */
        return DSM_S_DSMBADCONTEXT;

    if( pcontext->pdbcontext == NULL )
    {
        /* Allocate a database context structure     */
        rc = dbContextInit(&pcontext->pdbcontext);
    }

    if (!tagValue)
    {
        rc = DSM_S_INVALID_TAG_VALUE;
        goto errorReturn;
    }

    switch (tagId)
    {
      case DSM_TAGCONTEXT_MSG_CALLBACK:
        pcontext->msgCallback = tagValue;
        break;
      case DSM_TAGCONTEXT_EXIT_CALLBACK:
        pcontext->exitCallback = (DSMVOID (*)())tagValue;
        break;
      case DSM_TAGDB_ENSSARSY_CALLBACK:
        pcontext->pdbcontext->enssarsy_CallBack = (DSMVOID (*)())tagValue;
        break;
      default:
        /* Invalid tag                                         */
        rc = DSM_S_INVALID_TAG;
    }
    
errorReturn:
    
    return rc;

}  /* end dsmContextSetVoidFunction */


/* PROGRAM: dsmContextSetVoidPointer - Initializes a context with the tagValue
 * for the parameter identified by the tagId.  A context is initialized
 * after it is created.  A context can be intialized more than once
 * and the initializations are additive.
 *
 * RETURNS:
 */
dsmStatus_t DLLEXPORT
dsmContextSetVoidPointer(
    dsmContext_t         *pcontext,  /* IN/OUT database context */
    dsmTagId_t            tagId,     /* Tag identifier          */   
    dsmVoid_t            *ptagValue) /* tagValue                */
{
    dsmStatus_t  rc = 0;
    
    if( tagId == DSM_TAGCONTEXT_MSG_PARM )
    {
        pcontext->pmessageParm = ptagValue;
    }
    else
    {
        rc = DSM_S_INVALID_TAG;
    }
    
    return rc;

}  /* end dsmContextSetVoidPointer */


/* PROGRAM: dsmContextGetVoidPointer - Return pointer from context
 * specified by tagId.
 *
 * RETURNS:
 */
dsmStatus_t DLLEXPORT
dsmContextGetVoidPointer(
    dsmContext_t         *pcontext,  /* IN/OUT database context */
    dsmTagId_t            tagId,     /* IN Tag identifier          */   
    dsmVoid_t            **pptagValue) /* OUT tagValue                */
{
    dsmStatus_t  rc = 0;
    
    if( tagId == DSM_TAGCONTEXT_MSG_PARM )
    {
        *pptagValue = pcontext->pmessageParm;
    }
    else if( tagId == DSM_TAGDB_STORAGE_POOL )
    {
        *pptagValue = pdsmStoragePool;
    }
    else if ( tagId == DSM_TAGDB_WB_TABLE )
    {
        *pptagValue = pcontext->pdbcontext->pwtoktbl;
    }
    else
    {
        rc = DSM_S_INVALID_TAG;
    }
    
    return rc;

}  /* end dsmContextGetVoidPointer */

    

/* PROGRAM: dsmMsgnCallBack - establish argument list & issue message callback 
 *
 * RETURNS: DSM_S_SUCCESS
 */
dsmStatus_t
dsmMsgnCallBack(
    dsmContext_t         *pcontext,  /* IN database context    */
    int                   msgNumber, /* IN message number for lookup  */   
    ...)                             /* IN variable length arg list   */
{
    dsmStatus_t returnCode = DSM_S_FAILURE;
    TEXT        msgPrefix[16];
    va_list     args;

    if (pcontext && pcontext->msgCallback)
    {

        va_start(args, msgNumber);

        stnclr(msgPrefix,sizeof (msgPrefix));
        returnCode = dbLogAddPrefix(pcontext,msgPrefix);
        returnCode = pcontext->msgCallback(pcontext->pmessageParm,
                                           pcontext,
                                           msgPrefix,
                                           msgNumber,
                                           &args);

    }

    return returnCode;

}  /* end dsmMsgnCallBack */


/* PROGRAM: dsmFatalMsgnCallBack - establish argument list & issue
 *                             message callback, followed by dbExit. 
 *
 * RETURNS: DSM_S_SUCCESS
 */
dsmStatus_t
dsmFatalMsgnCallBack(
    dsmContext_t         *pcontext,  /* IN database context    */
    int                   msgNumber, /* IN message number for lookup  */   
    ...)                             /* IN variable length arg list   */
{
    dsmStatus_t returnCode = -1;
    TEXT        msgPrefix[16];
    va_list     args;

    if (pcontext && pcontext->msgCallback)
    {

       va_start(args, msgNumber);
        
        stnclr(msgPrefix,sizeof (msgPrefix));
        returnCode = dbLogAddPrefix(pcontext,msgPrefix);
        returnCode =  pcontext->msgCallback(pcontext->pmessageParm,
                                            pcontext,
                                           msgPrefix,
                                           msgNumber,
                                           &args);
    }

    dbExit(pcontext, DB_DUMP_EXIT, DB_EXBAD);
    return returnCode; /* never comes here */


}  /* end dsmFatalMsgnCallBack */


/* PROGRAM: dsmMsgdCallBack - establish argument list & issue message callback 
 *
 * RETURNS: DSM_S_SUCCESS
 */
dsmStatus_t
dsmMsgdCallBack(
    dsmContext_t         *pcontext,     /* IN database context    */
    ...)                                /* IN variable length arg list   */
{
    dsmStatus_t  returnCode = DSM_S_FAILURE;
    TEXT         msgPrefix[16];
    va_list      args;

    va_start(args, pcontext);
    
    if (pcontext && pcontext->msgCallback)
    {

        stnclr(msgPrefix,sizeof (msgPrefix) );
        returnCode = dbLogAddPrefix(pcontext,msgPrefix);
        returnCode = pcontext->msgCallback(pcontext->pmessageParm,
                                           pcontext,
                                           msgPrefix,
                                           0,
                                           &args);
    }
    
    return returnCode;

}  /* end dsmMsgdCallBack */


/* PROGRAM: dsmMsgtCallBack - establish argument list & issue message callback 
 *
 * RETURNS: DSM_S_SUCCESS
 */
dsmStatus_t
dsmMsgtCallBack(
    dsmContext_t         *pcontext,     /* IN database context    */
    ...)                                /* IN variable length arg list   */
{
    dsmStatus_t returnCode = DSM_S_FAILURE;
    TEXT        msgPrefix[16];
    va_list     args;

    if (pcontext && pcontext->msgCallback)
    {

        va_start(args, pcontext);

        stnclr(msgPrefix,sizeof (msgPrefix) );
        returnCode = dbLogAddPrefix(pcontext,msgPrefix);
        returnCode = pcontext->msgCallback(pcontext->pmessageParm,
                                           pcontext,
                                           msgPrefix,
                                           0,
                                           &args);
    }

    return returnCode;

}  /* end dsmMsgtCallBack */


/* PROGRAM: dsmThreadSafeEntry - validate and unlock the context's usrctl
 *
 * NOTE: This is NOT an exposed DSM API entry point. It is used internally
 *       by the dsm layer.
 *
 *       Also, to maintain the swiftness of processing this validation
 *       routine, we do no want to call any external functions
 *       (like MSGD_CALLBACK).  It is therefore the caller's
responsibility
 *       to handle the error message processing.
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_FAILURE
 */
dsmStatus_t
dsmThreadSafeEntry(
    dsmContext_t         *pcontext)     /* IN database context    */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t *pusr = pcontext->pusrctl;
    dsmStatus_t returnCode;

#if 0
    if (!pcontext->exitCallback)
    {
        /* No exit handling specified by the caller.
         * Perform exit processing via longjmp and bad rtc.
         */
        if ( setjmp(pcontext->savedEnv.jumpBuffer) )
        {
            /* we've longjmp'd to here.  Return to caller with error status */
            pcontext->savedEnv.jumpBufferValid = 0;
            returnCode = DSM_S_ERROR_EXIT;
            goto done;
        }
        pcontext->savedEnv.jumpBufferValid = 1;
    }
#endif
 
    if (!pusr)
    {
        returnCode = DSM_S_INVALID_USER;
        goto done;
    }
 
#if OPSYS==WIN32API
    if (fWin95)
    {
        if (!hWin95Mutex)
            hWin95Mutex = CreateMutex(NULL, FALSE, NULL);
        WaitForSingleObject(hWin95Mutex, INFINITE);
    }
#endif

    PL_LOCK_USRCTL(&pusr->uc_userLatch)

    if (!pusr->usrinuse ||
         pusr->uc_connectNumber != pcontext->connectNumber)
    {
        returnCode = DSM_S_INVALID_USER;
        goto done;
    }

    if (pdbcontext &&
        pdbcontext->pdbpub &&
        (pdbcontext->pdbpub->shutdn == DB_EXBAD) )
    {
        /* Don't disconnect if we are running as the juniper broker */
        if  (pdbcontext->accessEnv != DSM_JUNIPER_BROKER)
        {
            dbUserDisconnect(pcontext, DB_EXBAD);
            pcontext->pusrctl = NULL;
        }
        returnCode = DSM_S_SHUT_DOWN;
        goto done;
    }
 
    /* If user marked for disconnection then return error */
    if (pusr->usrtodie)
    {
        returnCode = DSM_S_USER_TO_DIE;
        goto done;
    }

    returnCode = DSM_S_SUCCESS;

done:

    return returnCode;
 
}  /* end dsmThreadSafeEntry */
 
 
/* PROGRAM: dsmThreadSafeExit - unlock the context's usrctl
 *
 * NOTE: This is NOT an exposed DSM API entry point. It is used internally
 *       by the dsm layer.
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_FAILURE
 */
dsmStatus_t
dsmThreadSafeExit(
    dsmContext_t         *pcontext)     /* IN database context    */
{
    usrctl_t *pusr = pcontext->pusrctl;
 
    if (pusr)
        PL_UNLK_USRCTL(&pusr->uc_userLatch)

#if OPSYS==WIN32API
    if (fWin95)
    {
        ReleaseMutex(hWin95Mutex);
    }
#endif

    pcontext->savedEnv.jumpBufferValid = 0;
    return DSM_S_SUCCESS;
 
}  /* end dsmThreadSafeExit */

 
/* PROGRAM: dsmEntryProcessError - process error from dsmThread safe entry
 *
 * NOTE: This is NOT an exposed DSM API entry point. It is used internally
 *       by the dsm layer.
 *
 * RETURNS: The transformed error status
 */
dsmStatus_t
dsmEntryProcessError(
    dsmContext_t         *pcontext,     /* IN database context    */
    dsmStatus_t           status,       /* IN the error status value */
    dsmText_t            *caller)       /* name of errant function (for msg) */
{

    /* The DSM_4GL_ENGINE is not currently prepared to handle anything
     * other than DSM_S_CTRLC
     */
    if (pcontext && pcontext->pdbcontext &&
        (pcontext->pdbcontext->accessEnv == DSM_4GL_ENGINE) )
    {
        return DSM_S_CTRLC;
    }

    switch(status)
    {
      case DSM_S_ERROR_EXIT:
        MSGN_CALLBACK(pcontext, drMSG643, caller, status);
        break;
      case DSM_S_SHUT_DOWN:
        MSGN_CALLBACK(pcontext, drMSG644, caller, status);
        break;
      case DSM_S_USER_TO_DIE:
      case DSM_S_INVALID_USER:
      default:
        MSGN_CALLBACK(pcontext, drMSG645, caller, status);
      break;
    }

    return status;

}  /* end dsmStatus_t */

/* PROGRAM: dsmContextWriteOptions - write database context settings 
 *
 *
 * RETURNS: DSM_S_SUCCESS        - success
 *          DSM_S_INVALID_USER   - context provided was not for broker
 */
dsmStatus_t
dsmContextWriteOptions(dsmContext_t    *pcontext)  /* IN database context */
{

    dbcontext_t *pdbcontext    = pcontext->pdbcontext;
    dsmStatus_t returnCode;

    pdbcontext->inservice++; /* postpone signal handler processing */
 
    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                                          (TEXT *)"dsmContextWriteOptions");
        goto done;
    }

    if (pdbcontext->usertype & BROKER)
    {
        returnCode = dbContextWriteOptions(pcontext);
    }
    else
    {
        returnCode = DSM_S_INVALID_USER;
    }

 
done:
 
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;

    return returnCode;

}  /* end dsmContextWriteOptions */

/* PROGRAM: dbContextWriteOptions - Log database server start up options
 *                              to .lg files
 *
 * RETURNS: DSMVOID
 */
LOCALF dsmStatus_t
dbContextWriteOptions(dsmContext_t *pcontext)
{

    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    usrctl_t    *pusrctl = pcontext->pusrctl;
    DOUBLE       bitholdBytes = 0;
    DOUBLE       bitholdSize = 0;
    TEXT         sizeId;
    TEXT         printBuffer[13];

#define ENABLED      "Enabled"
#define NOT_ENABLED  "Not Enabled"
#define RELIABLE     "Reliable"
#define NOT_RELIABLE "Not Reliable"

    if (pdbshm->argxshm == -1)
        pdbshm->argxshm = (16384 + (pdbshm->argnclts * 300) /1024);


    MSGN_CALLBACK(pcontext, drMSG299,XTEXT(pdbcontext, pusrctl->uc_qusrnam),
                  XTEXT(pdbcontext, pusrctl->uc_qttydev));

    MSGN_CALLBACK(pcontext, drMSG460,pdbshm->brokerpid);

    MSGN_CALLBACK(pcontext, drMSG133,pdbcontext->pdbname);

    if (pdbcontext->dbcode == PROCODE)
	MSGN_CALLBACK(pcontext, drMSG146,"PROGRESS");
     
    if (!pdbcontext->argforce)
	MSGN_CALLBACK(pcontext, drMSG159,NOT_ENABLED);
    else
	MSGN_CALLBACK(pcontext, drMSG159,ENABLED);

    if (pdbshm->directio)
        MSGN_CALLBACK(pcontext, drMSG162,ENABLED);
    else
        MSGN_CALLBACK(pcontext, drMSG162,NOT_ENABLED);

    MSGN_CALLBACK(pcontext, drMSG174,pdbshm->argbkbuf);
    MSGN_CALLBACK(pcontext, drMSG256,pdbshm->argxshm);
#if ALPHAOSF
    MSGN_CALLBACK(pcontext, drMSG388,pdbcontext->argMpte);
#endif
    MSGN_CALLBACK(pcontext, drMSG258, pdbshm->arglknum);
    MSGN_CALLBACK(pcontext, drMSG276, pdbshm->arghash);
    MSGN_CALLBACK(pcontext, drMSG277, pdbshm->argspin);
    MSGN_CALLBACK(pcontext, drMSG445, pdbshm->numsemids);

    if (!pdbshm->fullinteg)
	MSGN_CALLBACK(pcontext, drMSG278,NOT_ENABLED);
    else
	MSGN_CALLBACK(pcontext, drMSG278,ENABLED);

    MSGN_CALLBACK(pcontext, drMSG459, BKGET_BLKSIZE(pdbcontext->pmstrblk));

    MSGN_CALLBACK(pcontext, drMSG279,(pdbshm->tmenddelay));

    if (pdbshm->useraw == 1)
	MSGN_CALLBACK(pcontext, drMSG281,RELIABLE);
    else
        MSGN_CALLBACK(pcontext, drMSG281,NOT_RELIABLE);

    MSGN_CALLBACK(pcontext, drMSG282, pdbshm->rlagetime);
    MSGN_CALLBACK(pcontext, drMSG283, pdbcontext->prlctl->rlclbytes);
    MSGN_CALLBACK(pcontext, drMSG284, pdbcontext->prlctl->rlbiblksize);
    MSGN_CALLBACK(pcontext, drMSG285, pdbshm->argbibufs);
 
    /* Convert and display bithreshold size */
    if (pdbshm->bithold)
    {
        bitholdBytes =
             (DOUBLE)pdbshm->bithold * pdbcontext->prlctl->rlbiblksize;
        utConvertBytes(bitholdBytes, &bitholdSize, &sizeId);
        sprintf((char *)printBuffer, "%-5.1f %cBytes", bitholdSize, sizeId);
        MSGN_CALLBACK(pcontext, drMSG682, printBuffer); 
    }
    else
    {
        sprintf((char *)printBuffer, "%-5.1f Bytes", bitholdSize);
        MSGN_CALLBACK(pcontext, drMSG682, printBuffer); 
    }

    if (!pdbshm->bistall)
        MSGN_CALLBACK(pcontext, drMSG449); /* bistall not enabled */
    else
        MSGN_CALLBACK(pcontext, drMSG448); /* bistall enabled */


    if (!pdbshm->aistall)
	MSGN_CALLBACK(pcontext, drMSG287,NOT_ENABLED);
    else
	MSGN_CALLBACK(pcontext, drMSG287,ENABLED);

    if (pdbcontext->paictl != NULL)
        MSGN_CALLBACK(pcontext, drMSG288, pdbcontext->paictl->aiblksize);

    MSGN_CALLBACK(pcontext, drMSG289, pdbshm->argaibufs);
 
    if (rltlQon(pcontext))
    {
        MSGN_CALLBACK(pcontext, drMSG672);
    }

    MSGN_CALLBACK(pcontext, drMSG660, pdbshm->argomCacheSize);
    MSGN_CALLBACK(pcontext, drMSG290, pdbshm->argmaxcl);
    MSGN_CALLBACK(pcontext, drMSG291, pdbshm->argnsrv);
    MSGN_CALLBACK(pcontext, drMSG292, pdbcontext->minsv);
    MSGN_CALLBACK(pcontext, drMSG293, pdbshm->argnclts);
    
    if (pdbcontext->phost) 
        MSGN_CALLBACK(pcontext, drMSG294,pdbcontext->phost);
    else
        MSGN_CALLBACK(pcontext, drMSG294,NOT_ENABLED);

    MSGN_CALLBACK(pcontext, drMSG295,NOT_ENABLED);

    if (*pdbcontext->argsnet)
       MSGN_CALLBACK(pcontext, drMSG296, pdbcontext->argsnet);
    else
       MSGN_CALLBACK(pcontext, drMSG296,NOT_ENABLED);

    MSGN_CALLBACK(pcontext, drMSG297, pdbshm->brokerCsName );

    if (pdbcontext->pusrpfname)
       MSGN_CALLBACK(pcontext, drMSG300,pdbcontext->pusrpfname);
    else
       MSGN_CALLBACK(pcontext, drMSG300,NOT_ENABLED);

    if (pdbcontext->maxsvbrok)
        MSGN_CALLBACK(pcontext, drMSG404, pdbcontext->maxsvbrok);

    if (pdbcontext->minport)
        MSGN_CALLBACK(pcontext, drMSG405, pdbcontext->minport);

    if (pdbcontext->maxport)
        MSGN_CALLBACK(pcontext, drMSG406, pdbcontext->maxport);

    if(pdbcontext->pdbpub->maxxids)
        MSGN_CALLBACK(pcontext, drMSG689, pdbcontext->pdbpub->maxxids);
    
    if (pdbcontext->pshmdsc)
    {
        SHMDSC *pshmdsc = pdbcontext->pshmdsc;
        int     i;
        SEGMNT  *pseg = pshmdsc->pfrstseg;

        for (i = 0; i < pshmdsc->numsegs; i++)
        {
            /* Created shared memory segment with segment_id: */
            MSGN_CALLBACK(pcontext, utMSG156, pseg->seghdr.shmid);

            pseg = XSEGMNT(pdbcontext,pseg->seghdr.qnextseg);
        }
    }

   return DSM_S_SUCCESS;
    
}

