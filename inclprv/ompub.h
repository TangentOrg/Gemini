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

#ifndef OMPUB_H
#define OMPUB_H

/**********************************************************/
/* ompub.h - Public function prototypes for OM subsystem. */
/*           The OM sub-system are the accessor functions */
/*           for the database manager object cache.       */
/**********************************************************/

#include "dsmpub.h"

/* Object types     */
#define OM_TABLE  DSMOBJECT_MIXTABLE
#define OM_INDEX  DSMOBJECT_MIXINDEX

struct dbcontext;

typedef struct omDescriptor
{
    ULONG    area;
    DBKEY    objectBlock;
    DBKEY    objectRoot;
    ULONG    objectAttributes;
    dsmObjectType_t objectAssociateType;
    dsmObject_t objectAssociate;
}omDescriptor_t;

typedef struct omTempObject
{
  DBKEY		firstBlock;		/* Pointer first block of the
					   temporary object.            */
  DBKEY		lastBlock;		/* Pointer to the last block
					   of the temporary object.     */
  LONG64        numRows;                /* Total rows in the object     */
  LONG64        autoIncrement;          /* Auto increment value         */
  struct omTempObject *pnext;           /* Pointer to next temporary
					   object for this user         */
  ULONG         numBlocks;              /* Total blocks in the temp object */
  dsmObjectAttr_t attributes;           /* Unique index or not          */
  dsmObject_t   tableId;                /* Table number                 */
  dsmObject_t   id;                     /* id for indexes same as above
					   for tables                   */
  dsmObject_t   type;
  dsmText_t     name[65];               /* object name                  */       }omTempObject_t;

LONG
omCacheSize(dsmContext_t *pcontext);

dsmStatus_t
omAllocate(dsmContext_t *pcontext);

dsmStatus_t
omCreate(dsmContext_t *pContext, 
	 ULONG objectType,           /* Index or Table  */
	 COUNT objectNumber,
	 omDescriptor_t *pObjectDesc);

dsmStatus_t
omDelete( dsmContext_t *pContext,
	  ULONG         objectType,  /* OM_INDEX or OM_TABLE  */
          dsmObject_t   tableNum,
	  COUNT         objectNumber);

dsmStatus_t
omFetch( dsmContext_t *pContext,
	 ULONG         objectType,
         COUNT         objectNumber,
         dsmObject_t   tableNum,
	 omDescriptor_t *pObjectDesc);

dsmStatus_t
omFetchTemp(
	dsmContext_t	*pcontext,
	ULONG		 objectType,
        COUNT		 objectNumber,
        dsmObject_t      associate,
	omDescriptor_t	*pObjectDesc);

omTempObject_t *omFetchTempPtr(	dsmContext_t	*pcontext,
				ULONG		 objectType,
				COUNT		 objectNumber,
				dsmObject_t      associate);

/* Object-id to area mapping function  */
dsmStatus_t omIdToArea( dsmContext_t *pContext, ULONG objectType, 
		       COUNT objectNumber, dsmObject_t  tableNum,
                       ULONG *pArea);

dsmStatus_t
omGetNewObjectNumber(dsmContext_t *pContext, ULONG objectType, 
                     dsmObject_t tableNum, COUNT *pObjectNumber);

dsmStatus_t 
omGetObjectRecordId(dsmContext_t *pContext, ULONG objectType, 
                    COUNT objectNumber, dsmObject_t tableNum,
		    dsmRecid_t   *pObjectRecordId);

dsmStatus_t
omGetNewTempObject(dsmContext_t *pcontext, dsmText_t *pname,
		   dsmObjectType_t type,
		   dsmObjectAttr_t objectAttr,
		   dsmObject_t  tableId,
		   dsmObject_t *pId);

DSMVOID
omNameToNum(dsmContext_t *pcontext, dsmText_t *pname,
	    dsmObject_t *pId);

DBKEY
omTempTableFirst(dsmContext_t *pcontext, dsmObject_t   id);

#endif /* ifndef OMPUB_H  */


