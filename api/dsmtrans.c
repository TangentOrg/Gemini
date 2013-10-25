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

#include "dbcontext.h"
#include "dsmpub.h"
#include "tmmgr.h"
#include "tmpub.h"
#include "tmdoundo.h"
#include "usrctl.h"
#include "dbcode.h"

#include <setjmp.h>


/* PROGRAM: dsmTransaction - commit or rollback a transaction or savepoint.
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_BADSAVE - invalid savepoint passed.
 *          DSM_S_TRANSACTION_ALREADY_STARTED - transaction already started.
 *          DSM_S_NO_TRANSACTION - a transaction operation request
 *                                 was issued prior to a start request.
 *          DSM_S_INVALID_TRANSACTION_CODE - invalid transaction code.
 *          DSM_S_FAILURE
 */
dsmStatus_t
dsmTransaction(
	dsmContext_t  *pcontext,   /* IN/OUT database context */
        dsmTxnSave_t  *pSavePoint, /* IN/OUT savepoint        */
        dsmTxnCode_t  txnCode)     /* IN function code       */
{
    int returnCode;
    int  forceAbort;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    usrctl_t     *pusr       = pcontext->pusrctl;

    pdbcontext->inservice++;

    if (pdbcontext->prlctl == (RLCTL *)NULL)
    {
        /* Logging disabled */
        if (pdbcontext->dbcode != PROCODET)
        {
            /* Not a temp-table database */
	    return DSM_S_SUCCESS;
	}
    }

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        /* If the user is marked to die it was decided to allow the
         * transaction operation to completed for 4gl compatibility */
        if ( ( (returnCode == DSM_S_USER_TO_DIE) &&
               (pcontext->pdbcontext->accessEnv == DSM_4GL_ENGINE) ) == 0 )
        {   
            returnCode = dsmEntryProcessError(pcontext, returnCode,
                          (TEXT *)"dsmTransaction");
            goto done;
        }
    }

    returnCode = DSM_S_SUCCESS; /* assume success */

    if( txnCode == DSMTXN_START )
    {

        /* Check to make sure that we are not trying to start a 
           transaction on a task that already has a transaction */
        if (pusr->uc_task)
        {
            returnCode = DSM_S_TRANSACTION_ALREADY_STARTED;
            goto done;
        }

        /* Transaction begin starts an implicit savepoint of 1 */
        tmstrt(pcontext);
    }
    else if ( txnCode == DSMTXN_COMMIT )
    {
        /* Check to make sure a transaction start was issued prior */
        if (pusr->uc_task == 0)
        {
            returnCode = DSM_S_NO_TRANSACTION;
            goto done;
        }

        returnCode = tmend(pcontext, TMACC, NULL, 1 );
    }
    else if ( txnCode == DSMTXN_SAVE )
    {
        /* Make sure a transaction begin has been executed */
        if ( !pSavePoint || (*pSavePoint < (DSMTXN_SAVE_MINIMUM + 1)) )
        {
            returnCode = DSM_S_BADSAVE;
            goto done;
        }

        /* Mark the savepoint specified by the user */
        returnCode = tmMarkSavePoint(pcontext, pSavePoint);
    }
    else if ( txnCode == DSMTXN_ABORT || txnCode == DSMTXN_FORCE ||
              txnCode == DSMTXN_UNSAVE )
    {
        if( txnCode == DSMTXN_FORCE )
            forceAbort = 1;
        else
            forceAbort = 0;

        /* Check to make sure a transaction start was issued prior */
        if (pusr->uc_task == 0)
        {
            returnCode = DSM_S_NO_TRANSACTION;
            goto done;
        }

        /* Rollback the transaction and unmark the savepoint */
        if (pdbcontext->accessEnv == DSM_SQL_ENGINE)
        {
            if (!pSavePoint || (*pSavePoint > pcontext->pusrctl->uc_savePoint) )
            {
                returnCode = DSM_S_BADSAVE;
                goto done;
            }

            tmrej(pcontext, forceAbort, pSavePoint);
        }
        else
            tmrej(pcontext, forceAbort, (dsmTxnSave_t *)PNULL);

        if( !pSavePoint )  
        {
            /* savepoint pointer should not be null */
            returnCode = DSM_S_BADSAVE;
            goto done;
        }

        if( *pSavePoint == 0 )
            returnCode = tmend(pcontext, TMREJ, NULL, 1);

    }
    else if ( txnCode == DSMTXN_ABORTED )
    {
        /* Check to make sure a transaction start was issued prior */
        if (pusr->uc_task == 0)
        {
            returnCode = DSM_S_NO_TRANSACTION;
            goto done;
        }

        returnCode = tmend(pcontext, TMREJ, NULL, 1 );
    }
    else if ( txnCode == DSMTXN_PHASE1 )
    {
        /* Check to make sure a transaction start was issued prior */
        if (pusr->uc_task == 0)
        {
            returnCode = DSM_S_NO_TRANSACTION;
            goto done;
        }

        returnCode = tmend(pcontext, PHASE1, NULL, 1 );
    }
    else if (txnCode == DSMTXN_PHASE1Q2)
    {
        /* Check to make sure a transaction start was issued prior */
        if (pusr->uc_task == 0)
        {
            returnCode = DSM_S_NO_TRANSACTION;
            goto done;
        }

        returnCode = tmend(pcontext, PHASE1q2,NULL, 1);
    }
    else
    {
        /* Indicate that an invalid txnCode was passed to dsmTransaction */
        returnCode = DSM_S_INVALID_TRANSACTION_CODE;
    }
    
done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return((dsmStatus_t) returnCode);

}  /* end dsmTransaction */






