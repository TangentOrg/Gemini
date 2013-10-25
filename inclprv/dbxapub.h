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

#ifndef DBXAPUB_H
#define DBXAPUB_H

#include "dsmdtype.h"
#include "dsmxapub.h"
#include "shmpub.h"

typedef struct dbxaTransaction
{
    QXID       qnextXID;          /* Position independent pointer
                                     to the next transaction stucture
                                     in the free list   */
    dsmXID_t   xid;               /* The global transaction identifier */
    dsmTrid_t  trid;              /* Corresponding local transaction
                                     identifier                        */
    LONG       lastRLblock;       /* Block number in the bi area containing
                                     the last note written for this
                                     transaction                      */
    LONG       lastRLoffset;      /* Offset within above block to the
                                     end of the last note written for this
                                     transaction.                     */
    LONG       referenceCount;    /* Number of user contexts associated
                                     with this xid                    */
    LONG       numSuspended;     /* Number of suspended associations */
    LONG       flags;
#define  DSM_XA_ROLLBACK_ONLY   1 /* dsmXAend called with DSM_XA_TMFAIL */
#define  DSM_XA_RECOVERED       2 /* In ready to commit state after crash */
#define  DSM_XA_PREPARED        4 /* Transaction is in the prepared state */
    QXID       qself;                /* self-referential shared pointer */
}dbxaTransaction_t;    

dsmStatus_t
dbxaInit(dsmContext_t *pcontext);

LONG
dbxaGetSize(dsmContext_t *pcontext);

dbxaTransaction_t *dbxaXIDfind(dsmContext_t *pcontext, dsmXID_t *pxid);

dbxaTransaction_t *dbxaXIDget(dsmContext_t *pcontext);

int
dbxaCompare(dsmXID_t  *pxid1, dsmXID_t *pxid2);

dbxaTransaction_t *dbxaAllocate(dsmContext_t *pcontext);
DSMVOID
dbxaFree(dsmContext_t  *pcontext, dbxaTransaction_t *ptran);

dsmStatus_t
dbxaEnd(dsmContext_t *pcontext, dbxaTransaction_t *pxaTran, LONG flags);

#endif /* ifndef DBXAPUB_H  */











