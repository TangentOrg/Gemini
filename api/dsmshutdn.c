/*****************************************************************/
/* Copyright (c) 1984-1998 by Progress Software Corporation  */
/*                                                               */
/* All rights reserved.  No part of this program or document     */
/* may be reproduced in any form or by any means without         */
/* permission in writing from Progress Software Corporation.     */
/*****************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "dbcontext.h"
#include "dbpub.h" 
#include "dsmpub.h"

#include "usrctl.h"
#include "utspub.h"

#include <setjmp.h>

IMPORT latch_t dsmContextLatch;    /* per process p1dsmContext latch */
IMPORT dsmContext_t *p1dsmContext; /* list of contexts for this process */

/* PROGRAM: dsmShutdownMarkUser - mark a particular user to die
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_FAILURE
 */
dsmStatus_t
dsmShutdownMarkUser(
        dsmContext_t     *pcontext,   /* IN database context */
        psc_user_number_t userNumber) /* IN the usernumber */
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode = DSM_S_FAILURE;
    usrctl_t      *pusr;

    /* test for valid user Number */
    if (userNumber > pdbcontext->pdbpub->argnusr)
        return DSM_S_FAILURE;

    pdbcontext->inservice++; /* postpone signal handler processing */
 
    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    returnCode = dsmThreadSafeEntry(pcontext);
    if (returnCode != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmShutdownMarkUser");
        goto done;
    }
 
    MT_LOCK_USR ();

    /* mark the user */
    pusr = pcontext->pdbcontext->p1usrctl + userNumber;
    pusr->usrtodie = 1;

    MT_UNLK_USR ();

    returnCode = DSM_S_SUCCESS;

done:
    dsmThreadSafeExit(pcontext);
 
    pdbcontext->inservice--;

    return (returnCode);

}  /* end dsmShutdownMarkUser */


/* PROGRAM: dsmShutdownUser - disconnect all users associated with this srvctl
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_FAILURE
 */
dsmStatus_t
dsmShutdownUser(
        dsmContext_t     *pcontext,   /* IN database context */
        psc_user_number_t server)     /* IN the server identifier */
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dbshm_t       *pdbshm     = pdbcontext->pdbpub;
    dsmContext_t  *pcurrContext;
    dsmStatus_t    returnCode = DSM_S_FAILURE;
    dsmStatus_t    rtcBusy = 0;
    usrctl_t      *pusr;

    /* test for valid server Number */
    if (server > pdbshm->argnsrv)
        return DSM_S_FAILURE;

    pdbcontext->inservice++; /* postpone signal handler processing */
 
    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    returnCode = dsmThreadSafeEntry(pcontext);
    if (returnCode != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmShutdownUser");
        goto done;
    }
 
    PL_LOCK_DSMCONTEXT(&dsmContextLatch)

    for (pcurrContext = p1dsmContext;
         pcurrContext;
         pcurrContext = pcurrContext->pnextContext)
    {
        /* we will NOT disconnect ourself this way. */
        if (pcurrContext == pcontext)
            continue;

        pusr = pcurrContext->pusrctl;

        /* TODO: test if this user is owned by the server specified */
        if (pusr && (pusr->usrtodie || pcurrContext->pdbcontext->pdbpub->shutdn) )
        {
            if ( PL_TRYLOCK_USRCTL(&pusr->uc_userLatch) )
            {
                /* we got the lock on this user -
                 * Let's see if it still needs our services.
                 */
                if (pusr->usrinuse && 
                    (pusr->uc_connectNumber == pcontext->connectNumber) &&
                    (pusr->usrtodie || pcurrContext->pdbcontext->pdbpub->shutdn))
                {
                    /* disconnect the user */
                    returnCode = dbUserDisconnect(pcurrContext, DSMNICEBIT);
                }
                /* Assume dbUserDisconnect did the PL_UNLK_USRCTL unless
                 * dbUserDisconnect failed.
                 */
                if (returnCode)
                {  
                    if (pcurrContext->pusrctl)
                        PL_UNLK_USRCTL(&pcurrContext->pusrctl->uc_userLatch)
                    goto done1;
                }
                pcurrContext->pusrctl = 0;
            }
            else
            {
                /* we couldn't get the latch -
                 * communicate busy user to caller
                 */
                rtcBusy = DSM_S_USER_BUSY;
            }
        }
    }
 
    if (rtcBusy)
        returnCode = rtcBusy;
    else
        returnCode = DSM_S_SUCCESS;

done1:
    PL_UNLK_DSMCONTEXT(&dsmContextLatch)

done:
    dsmThreadSafeExit(pcontext);
 
    pdbcontext->inservice--;

    return (returnCode);

}  /* end dsmShutdownUser */


