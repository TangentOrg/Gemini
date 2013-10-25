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
#include "dsmpub.h"
#include "lkpub.h"
#include "ompub.h"
#include "usrctl.h"

#include <setjmp.h>

/**** LOCAL FUNCTION PROTOTYPES ****/


/**** END LOCAL FUNCTION PROTOTYPES ****/

/* PROGRAM: dsmCursorCreate - allocate a cursor for requested table and index
 *
 * dsmCursorCreate() allocates a cursor for the requested table and index
 * and returns the cursid in pcursid.  If pcopy is not NULL, then it is
 * expected to identify an existing cursor which is copied into the newly
 * created cursor (ignoring table and index) duplicateing all the
 * positioning information associatd with the existing cursor.
 *
 * A cursor is used to maintain a position within an index or table between
 * dsmCursorFind calls.  A cursor does not identify the range of values or
 * bracket associated with a find operation.
 *
 * NOTE: the initial implementation only supports index cursors
 * (position within an index) and requires a valid index value be provided.
 * Future implementations are expected to support passing zero as the
 * index number which would result in a table cursor (position within
 * a table object).
 *          
 * RETURNS: DSM_S_SUCCESS if success and cursorId will be set
 *          DSM_S_NOCURSOR if the cursorid to copy was not valid
 *          DSM_S_FAILURE if a new cursor cannot be created
 */
dsmStatus_t
dsmCursorCreate(
	dsmContext_t *pcontext,	/* IN/OUT database context */
	dsmTable_t    table,	/* IN table associated with cursor */
	dsmIndex_t    index,	/* IN index associated with cursor */
	dsmCursid_t  *pcursid,  /* OUT created cursor id */
	dsmCursid_t  *pcopy)   	/* IN cursor id to copy */
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    dsmStatus_t	 returnCode = DSM_S_FAILURE;

    pdbcontext->inservice++;  /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmCursorCreate");
        goto done;
    }

    returnCode = dbCursorCreate(pcontext, table, index, pcursid, pcopy);

done:
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;  /* re-allow signal handling */

    return(returnCode);

}  /* end dsmCursorCreate */


/* PROGRAM: dsmCursorFind - find a recid using a cursor
 *
 * Positions a cursor based on the current position provided by pcursid.
 * The bracket indicated by the two index keys pkeybase and pkeylimit
 * and the mode bits in findMode modify the positioning semantics.
 * dsmCursorFind() returns a recid into precid.  If the record is to be
 * locked, the lock information is provided in lockMode.  The key values
 * must be in the new index key format (V7+).  Subsequent calls to
 * dsmCursorFind() for a traditional cursor must provide the same
 * bracket information on each call.  However, it is perfectly
 * acceptable for the caller to change the bracket at any time.
 *          
 * RETURNS: DSM_S_SUCCESS if success and precid will be set
 */
dsmStatus_t
dsmCursorFind(
	dsmContext_t *pcontext,		/* IN database context */
	dsmCursid_t  *pcursid,		/* IN/OUT cursor */
	dsmKey_t     *pKeyBase,		/* IN base key of bracket for find */
	dsmKey_t     *pKeyLimit,	/* IN limit key of bracket for find */
        dsmMask_t     findType,         /* IN DSMUNIQUE, DSMPARTIAL ...     */
	dsmMask_t     findMode,		/* IN DSMFINDFIRST DSMFINDNEXT ... */
	dsmMask_t     lockMode,		/* IN lock mode bit mask */
        dsmObject_t  *pObjectNumber,    /* OUT the table/class the returned
                                         * recid is in                      */
	dsmRecid_t   *precid,		/* OUT recid found */
        dsmKey_t     *pKey)             /* OUT [optional] key found */
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    dsmStatus_t	 returnCode = DSM_S_FAILURE;

    pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmCursorFind");
        goto done;
    }

    returnCode = dbCursorFind(pcontext, pcursid, pKeyBase, pKeyLimit, 
        findType, findMode, lockMode, pObjectNumber, precid, pKey);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;  /* re-allow signal handling */    

    return(returnCode);

}  /* end dsmCursorFind */


/* PROGRAM: dsmCursorGet - get the cursor associated with a cursorid
 *
 * dsmCursorGet() returns a pointer to a cursor structure in pcursor
 * given a cursor id in cursid.  The cursor structure is part of the
 * public interface to DSMAPI.
 *
 * RETURNS: DSM_S_SUCCESS if success and pcursor will be filled in
 *          DSM_S_NOCURSOR when pcursor is NULL or invalid
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t
dsmCursorGet(
	dsmContext_t *pcontext,	/* IN database context */
	dsmCursid_t   cursid,   /* IN cursor id to find */
	struct cxcursor   **ppCursor)/* OUT pointer to cursor structure */
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    dsmStatus_t	 returnCode = DSM_S_FAILURE;

    pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmCursorGet");
        goto done;
    }

    returnCode = dbCursorGet(pcontext, cursid, ppCursor);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;  /* re-allow signal handling */

    return(returnCode);

}  /* end dsmCursorGet */


/* PROGRAM: dsmCursorDelete - deallocate the cursor indicated by pcursid
 *
 * dsmCursorDelete() deallocates the cursor indicated by cursorid.  
 * If deleteAll is true, then all cursors associated with the provided
 * context are deallocated.
 *
 * NOTE: much like the recid associated with a record, when a cursor is
 * deleted the resources associated with the cursor are returned to a
 * pool of cursors managed by the database storage manager.  This is why
 * the cursor id provided is cleared (so the caller "forgets" the cursor
 * id and does not attempt to reuse it).
 *
 * RETURNS: DSM_S_SUCCESS if success and pcursor will be filled in
 *          DSM_S_NOCURSOR when pcursor is NULL or invalid
 *          DSM_S_FAILURE on failure
 */
dsmStatus_t
dsmCursorDelete(
	dsmContext_t  *pcontext,	/* IN database context */
	dsmCursid_t   *pcursid,		/* IN/OUT pointer to cursor to delete */
	dsmBoolean_t  deleteAll)	/* IN delete all cursors indicator */
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    dsmStatus_t  returnCode;

    pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmCursorDelete");
        goto done;
    }

    returnCode = dbCursorDelete(pcontext, pcursid, deleteAll);
    
done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;  /* re-allow signal handling */

    return(returnCode);

}  /* end dsmCursorDelete */

