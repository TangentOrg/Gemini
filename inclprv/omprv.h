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

#ifndef OMPRV_H
#define OMPRV_H

/**********************************************************/
/* omprv.h -                                              */
/**********************************************************/

#include "dsmpub.h"
#include "ompub.h"
#include "shmpub.h"
#include "rlpub.h"

/* Object cache entry                                      */
typedef struct omObject
{
    dsmObjectType_t objectType;              /* Index, Table, ...     */
    dsmObject_t     objectNumber;            /* Index number, table #, ... */
    dsmArea_t       area;                    /* Area containing object    */
    DBKEY           objectBlock;
    DBKEY           objectRoot;              /* Root blocks for indexes   */
    dsmObjectAttr_t objectAttributes;        /* Unique or word index      */
    dsmObject_t     objectAssociate;         /* object id of associate
                                                object.                   */
    dsmObjectType_t objectAssociateType;     /* Usually an index          */
    QOM_OBJECT      qNextObject;             /* Next object in hash/free  */
    QOM_OBJECT      qlruNext;                /* Lru chain next pointer    */
    QOM_OBJECT      qlruPrev;                /* Lru chain prev pointer    */
    ULONG           spare[2];                /* Couple of spares for future
                                                use.                      */
    QOM_OBJECT      qself;                   /* Self-referential pointer  */
} omObject_t;

typedef struct omChain
{
    QOM_OBJECT  qhead;      /* head of chain */
    QOM_OBJECT  qtail;      /* tail of chain */
    ULONG       numOnChain; 
    ULONG       spare[2];
} omChain_t;


typedef struct omCtl
{
    ULONG       omHash;                  /* hash prime			*/
    ULONG       omNfree;                 /* Number of free omObjects	*/
    omChain_t   omLruChain;              /* Lru chain of cache entries	*/
    QOM_OBJECT  qomFreeList;             /* Pointer to start of free list */
    ULONG       references;              /* Cache requests.               */
    ULONG       misses;                  /* requests not found in cache   */
    ULONG       spare[4];                /* some spares for future use.   */
    QOM_OBJECT  qomTable[1];             /* Table allocated at run time */
} omCtl_t;


typedef struct objectNote
{
    RLNOTE          rlnote;
    dsmObjectType_t objectType;
    dsmObject_t     objectNumber;
    dsmObject_t     tableNum;
    dsmArea_t       areaNumber;
}  objectNote_t;

#define INIT_S_OBJECTNOTE_FILLERS( ob ) (ob).filler1 = 0; /* bug 20000114-034 by dmeyer in v9.1b */

#define OM_HASH(objectType,objectAssociate,objectNumber) \
    ((((objectType - 1) << 15) + (objectAssociate * (objectType - 1)) + objectNumber) % pomCtl->omHash)
dsmStatus_t
omBuildKey(dsmContext_t *pContext,
	   dsmKey_t *pKey, dsmObjectType_t objectType,
           dsmObject_t tableNum, dsmObject_t objectNumber);

DSMVOID omUndo (dsmContext_t *pcontext, RLNOTE *pnote);

#endif /* ifndef OMPRV_H  */









