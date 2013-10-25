
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
#include "lkpub.h"
#include "ompub.h"
#include "rlpub.h"
#include "rmpub.h"
#include "rmprv.h"
#include "rmmsg.h"
#include "seqpub.h"
#include "usrctl.h"
#include "uttpub.h"

#include <setjmp.h>

#ifndef FDDUPKEY
#define FDDUPKEY        -1235   /* record with that key already exists   */
#endif

/*#define S_DEBUG*/

/* PROGRAM: dsmSeqCreate        - Create a new sequence generator
 *
 * RETURNS: status code received from rmGetRecord or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dsmSeqCreate(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmSeqParm_t        *pseqparm)      /* IN Sequence definition parameters */
{
    seqParmBlock_t       seqParm;
    int                  ret;
    dsmStatus_t          returnCode;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmSeqCreate");
        goto done;
    }

    seqParm.seq_recid     = 0;
    seqParm.seq_initial   = pseqparm->seq_initial;
    seqParm.seq_min       = pseqparm->seq_min;
    seqParm.seq_max       = pseqparm->seq_max;
    seqParm.seq_increment = pseqparm->seq_increment;
    seqParm.seq_num       = (COUNT)pseqparm->seq_num;  /* -1 or particulat # */
    seqParm.seq_cycle     = pseqparm->seq_cycle;
    seqParm.seq_name[0]   = '\0';               /* Have seqCreate make up the name */

    ret = seqCreate (pcontext, &seqParm);
    if (!ret)
        pseqparm->seq_num = seqParm.seq_num;
    else
        pseqparm->seq_num = -1;

    returnCode = ret;
    if (returnCode == DSM_S_SEQCYC)
        returnCode = DSM_S_SEQRANGE;

done:
    dsmThreadSafeExit(pcontext);

    return(returnCode);

}  /* end dsmSeqCreate */
   

/* PROGRAM: dsmSeqDelete        - Remove a sequence generator
 *
 * RETURNS: status code received from rmGetRecord or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dsmSeqDelete(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmSeqId_t           seqId)         /* IN Sequence identifier */
{
    int                  ret;
    dsmStatus_t          returnCode;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmSeqDelete");
        goto done;
    }

    ret = seqRemove (pcontext, seqId);

    returnCode = ret;

done:
    dsmThreadSafeExit(pcontext);

    return(returnCode);

}  /* end dsmSeqDelete */


/* PROGRAM: dsmSeqInfo          - Return sequence generator information
 *
 * RETURNS: status code received from rmGetRecord or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dsmSeqInfo(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmSeqParm_t        *pseqparm)      /* OUT Sequence parameters */
{
    seqParmBlock_t       seqParm;
    int                  ret;
    dsmStatus_t          returnCode;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmSeqInfo");
        goto done;
    }

    seqParm.seq_num = (COUNT)pseqparm->seq_num;
    ret = seqInfo (pcontext, &seqParm);

    pseqparm->seq_recid      = seqParm.seq_recid;
    pseqparm->seq_max        = seqParm.seq_max;
    pseqparm->seq_min        = seqParm.seq_min;
    pseqparm->seq_increment  = seqParm.seq_increment;
    pseqparm->seq_initial    = seqParm.seq_initial;
    pseqparm->seq_cycle      = seqParm.seq_cycle;

    returnCode = ret;

done:
    dsmThreadSafeExit(pcontext);
 
    return(returnCode);

}  /* end dsmSeqInfo */


/* PROGRAM: dsmSeqGetValue        - Return either the next or current value
 *
 * RETURNS: status code received from rmGetRecord or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dsmSeqGetValue(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmSeqId_t           seqId,         /* IN Sequence identifier */
    GTBOOL                current,       /* IN == 0 -> NEXT, != 0 -> CURRENT */
    dsmSeqVal_t         *pValue)        /* OUT The sequence value */
{
    int                  ret;
    dsmStatus_t          returnCode;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmSeqGetValue");
        goto done;
    }

    if (current)
        ret = seqNumCurrentValue (pcontext, seqId, pValue);
    else
        ret = seqNumNextValue (pcontext, seqId, pValue);

    returnCode = ret;

done:
    dsmThreadSafeExit(pcontext);
 
    return(returnCode);

}  /* end dsmSeqGetValue */


/* PROGRAM: dsmSeqSetValue        - Set the current value
 *
 * RETURNS: status code received from rmGetRecord or other procedures
 *          which this procedure invokes.
 */
dsmStatus_t
dsmSeqSetValue(
    dsmContext_t        *pcontext,      /* IN database context */
    dsmSeqId_t           seqId,         /* IN Sequence identifier */
    dsmSeqVal_t          value)         /* IN The 'new' current value */
{
    int                  ret;
    dsmStatus_t          returnCode;

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmSeqSetValue");
        goto done;
    }

    ret = seqNumSetValue (pcontext, seqId, value);

    returnCode = ret;
    if (returnCode == DSM_S_SEQCYC)
        returnCode = DSM_S_SEQRANGE;

done:
    dsmThreadSafeExit(pcontext);
 
    return(returnCode);

}  /* end dsmSeqSetValue */
