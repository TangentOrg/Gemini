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
#include "shmpub.h"
#include "dsmpub.h"   /* for DSM API */
#include "dbcontext.h"
#include "dbpub.h"
#include "dbprv.h" 
#include "ompub.h"
#include "usrctl.h"
#include "objblk.h"
#include <setjmp.h>

/****   END  LOCAL  FUNCTION  PROTOTYPES   ****/

/* PROGRAM: dsmTableScan - Scan an area for records.  It is assumed
 *                         the area only contains rows for one table
 *                         
 *
 *
 * RETURNS: DSM_S_SUCCESS unqualified success
 *          - DSM_S_RECORD_BUFFER_TOO_SMALL
 *          - DSM_S_LKTBFULL
 *          - DSM_S_CTRLC  for lock wait timeout
 */
dsmStatus_t
dsmTableScan(
            dsmContext_t    *pcontext,        /* IN database context */
	    dsmRecord_t     *precordDesc,     /* IN/OUT row descriptor */
	    int              direction,       /* IN next, previous...  */
	    dsmMask_t        lockMode,        /* IN nolock share or excl */
            int              repair)          /* repair the rm chain   */
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */
 
    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmTableScan");
        goto done;
    }

    returnCode = dbTableScan(pcontext,precordDesc,direction,lockMode,repair);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return (returnCode);

}

/* PROGRAM: dsmTableAutoIncrement - Increments the table sequence number and
 *                                  returns its value.
 */
dsmStatus_t
dsmTableAutoIncrement(dsmContext_t *pcontext, dsmTable_t tableNumber,
                      ULONG64 *pvalue, int update)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmTableScan");
        goto done;
    }

    returnCode = dbTableAutoIncrement(pcontext, tableNumber, pvalue,update);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return (returnCode);
}

/* PROGRAM: dsmTableAutoIncrementSet -  Sets the table sequence number
 */
dsmStatus_t
dsmTableAutoIncrementSet(dsmContext_t *pcontext, dsmTable_t tableNumber,
                      ULONG64 value)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmTableScan");
        goto done;
    }

    returnCode = dbTableAutoIncrementSet(pcontext, tableNumber, value);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return (returnCode);
}

/* PROGRAM: dsmTableReset - Rebuilds all of tables indexes and space allocation
                             chains.
*/
dsmStatus_t DLLEXPORT
dsmTableReset(dsmContext_t *pcontext, dsmTable_t tableNumber, int numIndexes,
               dsmIndex_t   *pindexes)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmTableRepair");
        goto done;
    }
    returnCode = dbTableReset(pcontext, tableNumber, numIndexes, pindexes);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return (returnCode);
}

/* PROGRAM: dsmTableStatus - returns the table status flag  
                            
*/
dsmStatus_t DLLEXPORT
dsmTableStatus(dsmContext_t *pcontext, dsmTable_t tableNumber, 
               dsmMask_t *ptableStatus) 
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;
    objectBlock_t *ptableObjectBlock;
    bmBufHandle_t  tableObjectHandle;
    dsmArea_t      areaNumber;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmTableStatus");
        goto done;
    }
    returnCode = omIdToArea(pcontext,DSMOBJECT_MIXTABLE,tableNumber,tableNumber,
                    &areaNumber);
    if (returnCode)
      goto done;

    tableObjectHandle = bmLocateBlock(pcontext, areaNumber,
                         BLK2DBKEY(BKGET_AREARECBITS(areaNumber)), TOREAD);
    ptableObjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,
                                                          tableObjectHandle);
    *ptableStatus = ptableObjectBlock->objStatus;
    bmReleaseBuffer(pcontext,tableObjectHandle);
done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return(returnCode);
}
  
