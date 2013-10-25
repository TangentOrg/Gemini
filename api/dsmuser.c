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

#include "cxpub.h"
#include "dbcontext.h"
#include "dbpub.h"  /* for dbenv... */
#include "dsmpub.h"
#include "dsmdtype.h"
#include "lkpub.h"  /* for lkresync() */
#include "mstrblk.h"
#include "usrctl.h"
#include "utspub.h"

/*** NOTE: This is a temporary change for drsig to work with SQL
 *         bug #99-03-22-003
 */
#include <setjmp.h>

/* PROGRAM: dsmUserConnect - connects a users and fills out the db context
 *          
 *          
 * RETURNS: 0 unqualified success
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t
dsmUserConnect(
	dsmContext_t *pcontext,	/* IN/OUT database context */
	dsmText_t    *prunmode,	/* string describing use, goes in lg file  */
	int           mode)    	/* bit mask with these bits:
				   DBOPENFILE, DBOPENDB, DBSINGLE, DBMULTI,
				   DBRDONLY, DBNOXLTABLES, DBCONVERS */
{
    dsmStatus_t	returnCode;
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++; /* "post-pone" signal handling  while in DSM API */

#if 1
    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */
#else
    if ( setjmp(pcontext->savedEnv.jumpBuffer) )
    {
        pcontext->savedEnv.jumpBufferValid = 0;
        /* we've longjmp'd to here.  Return to caller with error status */
        returnCode = DSM_S_ERROR_EXIT;
        goto done;
    }
    pcontext->savedEnv.jumpBufferValid = 1;
#endif

    /* avoid race condition where 2 threads logging in at once - dbenv MUST
     * complete before another thread can call dbenv3
     */
    PL_LOCK_DBCONTEXT(&pdbcontext->dbcontextLatch)

    pdbcontext->useCount++;
 
    if ( pdbcontext->useCount == 1 )
    {
        returnCode = dbenv(pcontext, prunmode, mode);
    }
    else
    {
        returnCode = dbenv3(pcontext);
    }
    PL_UNLK_DBCONTEXT(&pdbcontext->dbcontextLatch)

    if (returnCode && returnCode != DSM_S_DB_IN_USE)
        returnCode = DSM_S_FAILURE;

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;  /* re-allow signal handling */
    return(returnCode);

}  /* end dsmUserConnect */


/* PROGRAM: dsmUserDisconnect - disconnect a user
 *          
 *          
 * RETURNS: 0 unqualified success
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t
dsmUserDisconnect(
	dsmContext_t	*pcontext,	/* IN/OUT database context */
	int      	 exitflags)     /* may have DSMNICEBIT     */
{
    dsmStatus_t	returnCode;
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++; /* "post-pone" signal handling  while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    /* For dsmUserDisconnect, disconnecting the user IS allowed even
     * though shutdn == DB_EXBAD but if DSM_S_SHUT_DOWN then we assume
     * dsmThreadSafeEntry has already done the disconnect.
     */
    returnCode = dsmThreadSafeEntry(pcontext);
    if ( (returnCode == DSM_S_SUCCESS) ||
         (returnCode == DSM_S_USER_TO_DIE) ||
         (returnCode == DSM_S_CTRLC) )
    {
        returnCode = dbUserDisconnect(pcontext, exitflags);
        pcontext->pusrctl = NULL;

    }
    else if (returnCode && (returnCode != DSM_S_SHUT_DOWN) )
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                                          (TEXT *)"dsmUserDisconnect");
        goto done;
    }

done:
    pcontext->savedEnv.jumpBufferValid = 0;
    pdbcontext->inservice--;  /* re-allow signal handling */
    return(returnCode);

}  /* end dsmUserDisconnect */


/* PROGRAM: dsmUserReset - reset the user context
 *          
 *
 * RETURNS:   0 unqualified success
 *            DSM_S_FAILURE on failure
 */
dsmStatus_t
dsmUserReset(
	dsmContext_t	*pcontext)	/* IN/OUT database context */
{
    dsmStatus_t	returnCode;
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++; /* "post-pone" signal handling  while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmUserReset");
        goto done;
    }

    /* check if it is time to shut down*/
    if ((pdbcontext->usertype & SELFSERVE) &&
        (pdbcontext->pdbpub->shutdn || pcontext->pusrctl->usrtodie))
    {
        /* to resync user, remove usrctl, detach shm, etc */
        returnCode = dbUserDisconnect(pcontext, DB_NICE_EXIT);
#if 0
        pdbcontext->shmgone = 1 ; /* to avoid using shared memory */
#endif
    }
    else
        returnCode = lkresync(pcontext);

    if (returnCode)
        returnCode = DSM_S_FAILURE;

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;
    return(returnCode);

}  /* end dsmUserReset */


/* PROGRAM: dsmUserSetName - sets the user name associated w/current context
 *          
 *
 * RETURNS: 0 unqualified success
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t
dsmUserSetName(
	dsmContext_t	*pcontext,	/* IN database context */
	dsmText_t	*pUsername,	/* IN username to set */
        QTEXT		*pq)		/* OUT the qptr to the filled string */
{
    dsmStatus_t	returnCode;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    pdbcontext->inservice++; /* "post-pone" signal handling  while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmUserSetName");
        goto done;
    }

    returnCode = fillstr(pcontext, pUsername, pq);

    if (returnCode)
        returnCode = DSM_S_FAILURE;

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;  /* re-allow signal handling */
    return(returnCode);

}  /* end dsmUserSetName */


/* PROGRAM: dsmUserAdminStatus - sets the user name associated w/current context
 *          
 *
 * RETURNS: 0 unqualified success
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t
dsmUserAdminStatus(
        dsmContext_t      *pcontext,    /* IN database context */
        psc_user_number_t  userNumber,  /* IN user number for info retrieval */
        dsmUserStatus_t   *puserStatus) /* OUT user status info structure */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t    *pusr;

    /* find user number */
    if (userNumber > pdbcontext->pdbpub->argnusr)
        return DSM_S_INVALID_USER;

    pusr = pdbcontext->p1usrctl + userNumber;

    /* fill in structure */
    puserStatus->userNumber   = pusr->uc_usrnum;
    puserStatus->objectNumber = pusr->uc_objectId;
    puserStatus->objectType   = pusr->uc_objectType;
    puserStatus->phase        = pusr->uc_state;
    puserStatus->counter      = pusr->uc_counter;
    puserStatus->target       = pusr->uc_target;
    stncop(puserStatus->operation, pusr->uc_operation,
           sizeof(puserStatus->operation));

    return DSM_S_SUCCESS;

}  /* end dsmUserAdminStatus */
