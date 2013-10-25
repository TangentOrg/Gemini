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

/*
 * The Object Cache Manager (om) procedures.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"

#include "bkpub.h"
#include "bmpub.h"
#include "cxpub.h"
#include "cxprv.h"
#include "dbcode.h"
#include "dbcontext.h"
#include "dbmsg.h"
#include "dsmpub.h"
#include "keypub.h"
#include "kypub.h"
#include "latpub.h"
#include "lkpub.h"
#include "mstrblk.h"
#include "ompub.h"
#include "omprv.h"
#include "recpub.h"
#include "rlpub.h"           /* So can include rlprv.h   */
#include "rlprv.h"           /* For warm start flag      */
#include "rmpub.h"
#include "scasa.h"
#include "stmpub.h"
#include "stmprv.h"
#include "utfpub.h"
#include "utspub.h"
#include "usrctl.h"

LOCAL  stpool_t  omTempObjectPool;

LOCALF 
DSMVOID
omFind(
	dsmContext_t	*pcontext,
	ULONG		 objectType,
        dsmObject_t      associate,
        dsmObject_t	 objectNumber,
	omObject_t       **ppPrevious,
	omObject_t       **ppObject);

LOCALF dsmStatus_t
omGet(   dsmContext_t  *pcontext,
         ULONG         objectType,
         COUNT         objectNumber,
         dsmObject_t   tableNum,
	 omDescriptor_t *pObjectDesc);

LOCALF DSMVOID
omDolru (
	dsmContext_t	*pcontext,
	omObject_t	*pentry);

LOCALF omObject_t *
omGetLru(dsmContext_t *pcontext);

LOCALF omObject_t *
omRemoveFromChain(dsmContext_t *pcontext,
                  ULONG		 objectType, 
                  dsmObject_t    tableNum,
                  COUNT		 objectNumber);

/* PROGRAM: omCacheSize - Calculate the amount of storage needed
 *                        for the object manager cache.
 *
 * Returns: size of storage needed in bytes.
 */
LONG
omCacheSize(dsmContext_t *pcontext)
{
    dbshm_t     *pdbshm = pcontext->pdbcontext->pdbpub;
    LONG   cacheSize;

    if (pdbshm->argomCacheSize > 65535)
    {
        pdbshm->argomCacheSize = 65535;
    }
    else
    {
        pdbshm->argomCacheSize =
              ((pdbshm->argomCacheSize <= 0) ? 512 : pdbshm->argomCacheSize);
    }

    pdbshm->omHashPrime = dbGetPrime(pdbshm->argomCacheSize / 4 );
    if( pdbshm->omHashPrime > pdbshm->argomCacheSize )
    {
        pdbshm->argomCacheSize = pdbshm->omHashPrime;
    }

    cacheSize = sizeof(omCtl_t);
    cacheSize += (pdbshm->omHashPrime - 1) * sizeof(QOM_OBJECT);
    cacheSize += pdbshm->argomCacheSize * sizeof(omObject_t);

    return cacheSize;

}  /* end omCacheSize */


/* PROGRAM: omAllocate - Allocate object cache structures.       
 *
 *
 * RETURNS: 0
 *          DSM_S_NOSTG
 */
dsmStatus_t
omAllocate( dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t         *pdbshm     = pdbcontext->pdbpub;
    STPOOL   *pdbpool;
    omCtl_t  *pomCtl;
    LONG     cacheEntries;
    LONG     hashPrime;
    omObject_t *pEntry;
    LONG     i;

    pdbpool = XSTPOOL(pdbcontext, pdbshm->qdbpool);

    pomCtl = (omCtl_t *)stGet(pcontext, pdbpool,omCacheSize(pcontext));

    if ( pomCtl == NULL )
    {
	MSGN_CALLBACK(pcontext, dbMSG035);
	return DSM_S_NOSTG;
    }

    cacheEntries = pdbshm->argomCacheSize;
    hashPrime = pdbshm->omHashPrime;

    pdbshm->qomCtl = P_TO_QP(pcontext, pomCtl);
    pdbcontext->pomCtl = pomCtl;
    pomCtl->omHash = hashPrime;
    pomCtl->omNfree = cacheEntries;
    pomCtl->qomFreeList = (QOM_OBJECT)P_TO_QP(pcontext, &pomCtl->qomTable[hashPrime]);

    pEntry = XOMOBJECT(pdbcontext, pomCtl->qomFreeList); 

    for ( i = 0 ; i < cacheEntries; i++, pEntry++ )
    {
	pEntry->qNextObject = P_TO_QP(pcontext, pEntry + 1);
	pEntry->qself = P_TO_QP(pcontext, pEntry);
    }
    /* Null terminate the free list   */
    pEntry--;
    pEntry->qNextObject = 0;
    return DSM_S_SUCCESS;

}  /* end omAllocate */


/* PROGRAM: omCreate - Add the input object to the object cache.
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_NOSTG
 */
dsmStatus_t
omCreate(
	dsmContext_t	*pcontext, 
	dsmObjectType_t	 objectType,           /* Index or Table  */
	dsmObject_t	 objectNumber,
	omDescriptor_t	*pObjectDesc)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    LONG    hashChain;
    omCtl_t *pomCtl;
    omObject_t *pNewObject,*pObject, *pPrevious;
    STPOOL  *pdbpool;
    ULONG    i;

    pomCtl = pdbcontext->pomCtl;

    hashChain = OM_HASH(objectType,pObjectDesc->objectAssociate,objectNumber);

    MT_LOCK_OM();

    pomCtl->misses++;
    /* Search the hash chain                        */
    omFind(pcontext,objectType,pObjectDesc->objectAssociate,
	   objectNumber,&pPrevious,&pObject);

    /* See if we can get a cache node from the free list.   */
    pNewObject = XOMOBJECT(pdbcontext, pomCtl->qomFreeList);
    
    if( pNewObject )
    {
	pomCtl->omNfree--;
	pomCtl->qomFreeList = pNewObject->qNextObject;
    }
    else if (pdbcontext->arg1usr )
    {
	/* Can just allocate more to the free list    */
	pdbpool = XSTPOOL(pdbcontext, pdbshm->qdbpool);
	pomCtl->omNfree = 16;
	pNewObject = (omObject_t *)stGet(pcontext, pdbpool,pomCtl->omNfree * sizeof(omObject_t));
	if ( pNewObject == NULL )
	{
	    pomCtl->omNfree = 0;
	    MSGN_CALLBACK(pcontext, dbMSG036);
	    return DSM_S_NOSTG;
	}
	pomCtl->qomFreeList = P_TO_QP(pcontext, pNewObject);

	for ( i = 0 ; i < pomCtl->omNfree; i++, pNewObject++ )
	{
	    pNewObject->qNextObject = P_TO_QP(pcontext, pNewObject + 1);
	    pNewObject->qself = P_TO_QP(pcontext, pNewObject);
	}
	/* Null terminate the free list   */
	pNewObject--;
	pNewObject->qNextObject = 0;
	pNewObject = XOMOBJECT(pdbcontext, pomCtl->qomFreeList);
	pomCtl->omNfree--;
	pomCtl->qomFreeList = pNewObject->qNextObject;
    }
    else 
    {
        pNewObject = omGetLru(pcontext);
        /* We need to refind the cache entry  just in case the
           lru entry was on this hash chain, that could effect things */
	omFind(pcontext,objectType,pObjectDesc->objectAssociate,
	       objectNumber,&pPrevious,&pObject);
    }

    if( pPrevious == NULL )
    {
	/* Add new cache entry at the head of the chain.    */
	pNewObject->qNextObject = pomCtl->qomTable[hashChain];
	pomCtl->qomTable[hashChain] = QSELF(pNewObject);
    }
    else
    {
	if( pPrevious != pNewObject &&
            pPrevious->qNextObject != QSELF(pNewObject))
	{
	    pNewObject->qNextObject = pPrevious->qNextObject;
	    pPrevious->qNextObject = QSELF(pNewObject);
	}
    }
    /* At last fill in the entry                                  */
    pNewObject->objectType = objectType;
    pNewObject->objectNumber = objectNumber;
    pNewObject->area = pObjectDesc->area;
    pNewObject->objectBlock = pObjectDesc->objectBlock;
    pNewObject->objectRoot = pObjectDesc->objectRoot;
    pNewObject->objectAttributes = pObjectDesc->objectAttributes;
    pNewObject->objectAssociateType = pObjectDesc->objectAssociateType;
    pNewObject->objectAssociate = pObjectDesc->objectAssociate;
    omDolru(pcontext,pNewObject);
    
    MT_UNLK_OM();

    return DSM_S_SUCCESS;

}  /* end omCreate */


/* PROGRAM: omDelete - Remove the specified object from the object cache.
 *
 * RETURNS: DSM_S_SUCCESS is successfully deleted
 *          DSM_S_FAILURE otherwise
 */
dsmStatus_t
omDelete(
	dsmContext_t *pcontext,
	ULONG         objectType,  /* OM_INDEX or OM_TABLE  */
        dsmObject_t   tableNum,
	COUNT         objectNumber)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    omCtl_t *pomCtl;
    omObject_t *pFree,*pObject;
    dsmStatus_t   rc;
    
    pomCtl = pdbcontext->pomCtl;


    MT_LOCK_OM();

    pObject = omRemoveFromChain(pcontext,objectType,tableNum,objectNumber);
    
    if(pObject)
    {
	/* Put entry on the free list */
	pomCtl->omNfree++;
	pFree = XOMOBJECT(pdbcontext, pomCtl->qomFreeList);
	pomCtl->qomFreeList = QSELF(pObject);
	if( pFree )
	{
	    pObject->qNextObject = QSELF(pFree);
	}
	else
	{
	    pObject->qNextObject = 0;
	}
    }

    MT_UNLK_OM();

    rc = DSM_S_SUCCESS;

    return rc;

}  /* end omDelete */


/* PROGRAM: omFetch - Get the object descriptor for the object specified
 *                     by objectType and ObjectNumber.
 *
 * RETURNS: DSM_S_SUCCESS or
 *          retcode from omGet
 *          retcode from omCreate
 */
dsmStatus_t
omFetch(
	dsmContext_t	*pcontext,
	ULONG		 objectType,
        COUNT		 objectNumber,
        dsmObject_t      associate,
	omDescriptor_t	*pObjectDesc)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    omCtl_t *pomCtl;
    omObject_t *pObject;
    dsmStatus_t rc;
    RLCTL      *prlctl;

    prlctl = pdbcontext->prlctl;

    if( !cxIxrprGet() && prlctl && prlctl->rlwarmstrt &&
        /* Ok to call after physical backout of crash recovery
           is complete, that is during logical undo where
           rlwarmstrt is 3                                  */
        prlctl->rlwarmstrt != 3 )
    {
	FATAL_MSGN_CALLBACK(pcontext, dbFTL004); 
	/* can't call me during crash recovery */
    }
    /* Siphon off temp table requests              */
    if(SCTBL_IS_TEMP_OBJECT(objectNumber))
    {
      return (omFetchTemp(pcontext,objectType,objectNumber,associate,
		       pObjectDesc));
    }
    pomCtl = pdbcontext->pomCtl;
    
    pObjectDesc->area = 0;

    /* Lock the cache                               */
    MT_LOCK_OM();
    
    pomCtl->references++;
    if( pomCtl->references == 0 )
    {
        /* We must have overflowed the counter so reset misses also */
        pomCtl->references = 1;
        pomCtl->misses = 0;
    }
    
    /* Search the hash chain                        */
    omFind(pcontext,objectType,associate,objectNumber,NULL,&pObject);
    if(pObject)
    {
      pObjectDesc->area = pObject->area;
      pObjectDesc->objectBlock = pObject->objectBlock;
      pObjectDesc->objectRoot  = pObject->objectRoot;
      pObjectDesc->objectAttributes = pObject->objectAttributes;
      pObjectDesc->objectAssociateType = pObject->objectAssociateType;
      pObjectDesc->objectAssociate     = pObject->objectAssociate;
      omDolru(pcontext,pObject);
      MT_UNLK_OM();
      return DSM_S_SUCCESS;
    }
    /* Object not found in cache we'll have to read it from disk */
    MT_UNLK_OM();
    
#ifdef VST_ENABLED
    if (SCTBL_IS_VST(objectNumber) &&
       ((objectType == DSMOBJECT_MIXTABLE) || (objectType == DSMOBJECT_MIXINDEX)) )
    {
        pObjectDesc->area = DSMAREA_INVALID;
        pObjectDesc->objectBlock = 0;
        pObjectDesc->objectRoot = 0;
        pObjectDesc->objectAssociate = objectNumber;
        if (objectType == DSMOBJECT_MIXTABLE)
        {
            pObjectDesc->objectAssociateType = DSMOBJECT_MIXINDEX;
            pObjectDesc->objectAttributes = 0;
        }
        else 
        {
            pObjectDesc->objectAssociateType = DSMOBJECT_MIXTABLE;
            pObjectDesc->objectAttributes    = DSMINDEX_UNIQUE;
        }
 
        return DSM_S_SUCCESS;
    }
#endif  /* VST ENABLED */

    rc = omGet(pcontext, objectType, objectNumber, associate, pObjectDesc);
    if( rc == DSM_S_SUCCESS )
    {
	/* Cache the object.                                     */
	rc = omCreate(pcontext, objectType, objectNumber, pObjectDesc);
    }
    return rc;

}  /* end omFetch */

/* PROGRAM: omFind - searches the hash table for the specified object.
   It is ASSUMED that the caller locks the OM latch.   */
LOCALF 
DSMVOID
omFind(
	dsmContext_t	*pcontext,
	ULONG		 objectType,
        dsmObject_t      associate,
        dsmObject_t	 objectNumber,
	omObject_t       **ppPrevious,
	omObject_t       **ppObject)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    LONG    hashChain;
    omCtl_t *pomCtl;
    omObject_t *pObject;

    pomCtl = pdbcontext->pomCtl;
    if(ppPrevious)
      *ppPrevious = NULL;
    *ppObject = NULL;

    /* Get the hash chain the object is on          */
    hashChain = OM_HASH(objectType,associate,objectNumber);
    
    /* Search the hash chain                        */
    for( pObject = XOMOBJECT(pdbcontext, pomCtl->qomTable[hashChain]);
	pObject; pObject = XOMOBJECT(pdbcontext, pObject->qNextObject))
    {
	if( pObject->objectType == objectType &&
	    pObject->objectAssociate == associate &&
	    pObject->objectNumber == objectNumber)
	{
	  *ppObject = pObject;
	  break;
	}
	if( pObject->objectType > objectType )
	    /* Object is not in the cache        */
	    break;

	if(pObject->objectType == objectType &&
	   pObject->objectAssociate > associate)
	    /* Object is not in the cache        */
	    break;
           
	if(pObject->objectType == objectType &&
	   pObject->objectAssociate == associate &&
	   pObject->objectNumber > objectNumber)
	    /* Object is not in the cache        */
	    break;
	if(ppPrevious)
	  *ppPrevious = pObject;
    }

    return;
}  /* end omFetch */


/* PROGRAM: omBuildKey - Build the search key for doing a unique
 *          find on the _Object table.
 *
 * NOTE: This function is obsolete. It remains due to a requirement
 *       of upixrpr.c which is locked for a while.
 *
 * RETURNS: 0 success
 *         -1 failure
 */
dsmStatus_t
omBuildKey(
	dsmContext_t	*pcontext,
	dsmKey_t	*pKey,
	dsmObjectType_t	 objectType,
        dsmObject_t      tableNum,
	dsmObject_t	 objectNumber)
{
    dbcontext_t       *pdbcontext = pcontext->pdbcontext;
    svcDataType_t component[5];
    svcDataType_t *pcomponent[5];
    COUNT         maxKeyLength = 64; /* SHOULD BE A CONSTANT! */


    /**** Build the index for the _Object Table ****/

    /* SCIDX_OBJID is the index number of the _Object-Id index
     * It is a 3 component index consisting of
     * _Db-recid _Object-type _Object-number
     */
    pKey->index    = SCIDX_OBJID;
    pKey->keycomps = 4;
    pKey->ksubstr  = 0;
 
    /* First component - _Object._Db-recid */
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = pdbcontext->pmstrblk->mb_dbrecid;

    /* Second component _Object._Object-type */
    component[1].type            = SVCLONG;
    component[1].indicator       = 0;
    component[1].f.svcLong       = (LONG)objectType;

    /* Third  component - _Object._Object-associate */
    component[2].type            = SVCSMALL;
    component[2].indicator       = 0;
    component[2].f.svcSmall      = tableNum;

    /* Fourth  component - _Object._Object-number */
    component[3].type            = SVCSMALL;
    component[3].indicator       = 0;
    component[3].f.svcSmall       = objectNumber;

    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];
    pcomponent[2] = &component[2];
    pcomponent[3] = &component[3];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, pKey) )
        return DSM_S_FAILURE;
 
    return DSM_S_SUCCESS;

}  /* end omBuildKey */


/* PROGRAM: omGetObjectRecord - 
 *
 * RETURNS  0 - success
 *              DSM_S_RMNOTFND
 */
LOCALF dsmStatus_t
omGetObjectRecord(
	dsmContext_t	*pcontext,
	dsmRecid_t	 recordId,
	omDescriptor_t	*pObjectDesc,
	COUNT		*pObjectNumber)
{
    TEXT  recordBuffer[256];
    int   recordSize;
    ULONG indicator;
    DBKEY   dbkey;
    
    recordSize = rmFetchRecord(pcontext, DSMAREA_SCHEMA, recordId, recordBuffer,
			    sizeof(recordBuffer),0);
    if( recordSize > (int)sizeof(recordBuffer))
    {
	MSGN_CALLBACK(pcontext, dbMSG037, (LONG)recordSize);
	return DSM_S_RMNOTFND;
    }
    
    /* Fill in the object descriptor from the fields of the record.  */
    if( pObjectNumber )
    {
	/* Extract the object number    */
        if ( recGetSMALL(recordBuffer, SCFLD_OBJNUM,(ULONG)0, pObjectNumber, 
                        &indicator) || (indicator == SVCUNKNOWN) )
	    goto badReturn;
    }
    if( pObjectDesc == NULL )
	return DSM_S_SUCCESS;

    /* extract the area                            */
    if ( recGetLONG(recordBuffer, SCFLD_OBJAREA,(ULONG)0, 
                    (svcLong_t *)&pObjectDesc->area, 
                    &indicator) || (indicator == SVCUNKNOWN) )
       goto badReturn;
   
    /* extract the objectBlock                         */

    if (recGetBIG(recordBuffer, SCFLD_OBJBLOCK,(ULONG)0, 
                  &dbkey, &indicator) ||
                  (indicator == SVCUNKNOWN) )
	goto badReturn;
    pObjectDesc->objectBlock = dbkey;
    /* extract the objectRoot                        */
    if ( recGetBIG(recordBuffer, SCFLD_OBJROOT,(ULONG)0,
                   &dbkey, &indicator) ||
                   (indicator == SVCUNKNOWN) )
	goto badReturn;
    pObjectDesc->objectRoot = dbkey;
    /* extract the object attributes                 */
    if ( recGetLONG(recordBuffer, SCFLD_OBJATTR,(ULONG)0,
                    (svcLong_t *)&pObjectDesc->objectAttributes, &indicator) ||
                    (indicator == SVCUNKNOWN) )
	goto badReturn;

    /* extract the object associate object id               */
    if( recGetSMALL(recordBuffer, SCFLD_OBJASSOC,(ULONG)0,
                   &pObjectDesc->objectAssociate,
                   &indicator))
        goto badReturn;
    if ( indicator )
    {
        pObjectDesc->objectAssociate = DSMOBJECT_INVALID;
    }
    /* extract the object associate object type      */
    if( recGetLONG(recordBuffer, SCFLD_OBJATYPE,(ULONG)0,
                   (svcLong_t *)&pObjectDesc->objectAssociateType,
                   &indicator))
        goto badReturn;

    if( indicator )
        pObjectDesc->objectAssociateType = 0;
    
    return DSM_S_SUCCESS;

  badReturn:

    MSGN_CALLBACK(pcontext, dbMSG038); /* recGet call failed */
    return DSM_S_RMNOTFND;

}  /* end omGetObjectRecord */

    
/* PROGRAM: omGet   -  Read the specified object record.
 *
 * RETURNS: 0 for success
 *          retcode from omGetObjectRecordId
 *          retcode from omGetObjectRecord
 *          DSM_S_NO_SUCH_OBJECT
 */
LOCALF dsmStatus_t
omGet(
	dsmContext_t  *pcontext,
	ULONG          objectType,
	COUNT          objectNumber,
        dsmObject_t      tableNum,
	omDescriptor_t *pObjectDesc)
{
    dsmRecid_t objectRecordId;
    dsmStatus_t  rc;

    
    if ( pcontext->pdbcontext->dbcode == PROCODET )
    {
	/* Objects should always be cached for temp-tables.    */
	rc = DSM_S_NO_SUCH_OBJECT;
	return rc;
    }

    rc = omGetObjectRecordId(pcontext,objectType, objectNumber,
                             tableNum, &objectRecordId);

    if( rc )
	return rc;

    rc = omGetObjectRecord(pcontext, objectRecordId, pObjectDesc, NULL );

    if ( rc == DSM_S_SUCCESS )
    {
        if( pObjectDesc->area == DSMAREA_CONTROL && 
            objectType == DSMOBJECT_MIXINDEX )
        {
            if( objectNumber == SCIDX_AREA )
                pObjectDesc->objectRoot = pcontext->pdbcontext->pmstrblk->areaRoot;
            else if( objectNumber == SCIDX_EXTAREA )
                pObjectDesc->objectRoot =
                    pcontext->pdbcontext->pmstrblk->areaExtentRoot;
        }
    }
    
    return rc;

}  /* end omGet */


/* PROGRAM: omGetNewObjectNumber - Returns an unused object number
 *                                 for the given object type.
 *
 * RETURNS: 0 for success
 *          DSM_S_NO_SUCH_OBJECT or
 *          DSMS_S_FAILURE       for failure
 */
dsmStatus_t
omGetNewObjectNumber(
	dsmContext_t	*pcontext,
	ULONG		 objectType,
        dsmObject_t      tableNum,
	COUNT		*pObjectNumber)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    rc = DSM_S_SUCCESS;
    CURSID         cursorId = NULLCURSID;
    DBKEY          objectRecordId;
    dsmObject_t    previousObjectNumber,firstUserObjectNumber;
    dsmObject_t    lastUserObjectNumber;
    dsmObject_t    newObjectNumber;
    omDescriptor_t omDesc;

    svcDataType_t component[5];
    svcDataType_t *pcomponent[5];
    COUNT         maxKeyLength = 64;

    mstrblk_t     *pmstrblk = pdbcontext->pmstrblk;

    AUTOKEY(objectKeyLow, 64)
    AUTOKEY(objectKeyHi, 64)

    if( pdbcontext->dbcode == PROCODET )
    {
	/* Scan the object cache to find an available object number  */
	rc = DSM_S_SUCCESS;

        /* safely store the last object number used. lastUserObjectNumber
           will be used to terminate searching for a new object
           number when all of the object number slots are taken */
 
        PL_LOCK_DBCONTEXT(&pdbcontext->dbcontextLatch)
 
        lastUserObjectNumber = pdbcontext->lastObjectNumber;
 
        PL_UNLK_DBCONTEXT(&pdbcontext->dbcontextLatch)

        while( rc == DSM_S_SUCCESS )
	{
            PL_LOCK_DBCONTEXT(&pdbcontext->dbcontextLatch)

            pdbcontext->lastObjectNumber++;

            if ((pdbcontext->lastObjectNumber == lastUserObjectNumber) &&
               (objectType == DSMOBJECT_MIXINDEX ))
            {
                /* we've looped around and there were no holes in the
                   object slots, time to fail */
               rc = DSM_S_MAX_INDICES_EXCEEDED;
               break;
            }
	    else if (pdbcontext->lastObjectNumber == DSMTABLE_USER_LAST  ||
                     pdbcontext->lastObjectNumber <  DSMTABLE_USER_FIRST)
	    {
		pdbcontext->lastObjectNumber = DSMTABLE_USER_FIRST;
             
                /* if pdbcontext->lastObjectNumber was the initial
                   value when lastUserObjectNumber was set, it
                   should be reset to the first object number + 1.
                   note this is safe since we stillhave the dbcontext
                   latch */
                if (lastUserObjectNumber < DSMTABLE_USER_FIRST)
                    lastUserObjectNumber = DSMTABLE_USER_FIRST + 1;
	    }
	    newObjectNumber = pdbcontext->lastObjectNumber;

            PL_UNLK_DBCONTEXT(&pdbcontext->dbcontextLatch)

	    rc = omFetch(pcontext, objectType, newObjectNumber, 
                              tableNum, &omDesc );
	    if( rc == DSM_S_NO_SUCH_OBJECT )
            {
	        *pObjectNumber = newObjectNumber;
	    }
	}
	if( rc == DSM_S_NO_SUCH_OBJECT )
        {
	    rc = DSM_S_SUCCESS;
	}
	return rc;
    }
	    
    /* set up keys to define bracket  for scan of the object table */
    objectKeyLow.akey.area = DSMAREA_SCHEMA;
    objectKeyHi.akey.area  = DSMAREA_SCHEMA;
    objectKeyLow.akey.root = pmstrblk->mb_objectRoot;
    objectKeyHi.akey.root  = pmstrblk->mb_objectRoot;
    objectKeyLow.akey.unique_index = 1;
    objectKeyHi.akey.unique_index  = 1;
    objectKeyLow.akey.word_index   = 0;
    objectKeyHi.akey.word_index    = 0;

    /* First component - _Object._Db-recid */
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = (LONG)pmstrblk->mb_dbrecid;

    /* Second component _Object._Object-type */
    component[1].type            = SVCLONG;
    component[1].indicator       = 0;
    component[1].f.svcLong       = (LONG)objectType;

    /* Third component _Object._Object-type */
    component[2].type            = SVCLONG;
    component[2].indicator       = 0;
    component[2].f.svcLong       = (LONG)tableNum;

    /* Fourth  component - First _Object._Object-number */
    component[3].type          = SVCSMALL;
    component[3].indicator     = 0;
    firstUserObjectNumber = DSMTABLE_USER_FIRST;
    
    component[3].f.svcSmall = firstUserObjectNumber;


    if ( objectType == DSMOBJECT_MIXINDEX )
        lastUserObjectNumber = DSMINDEX_USER_LAST;
    else
        lastUserObjectNumber = DSMTABLE_USER_LAST;
    
    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];
    pcomponent[2] = &component[2];
    pcomponent[3] = &component[3];

    objectKeyLow.akey.index    = SCIDX_OBJID;
    objectKeyLow.akey.keycomps = 4;
    objectKeyLow.akey.ksubstr  = 0;

    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objectKeyLow.akey))
        return DSM_S_FAILURE;
    
    
    /* Third  component - Last _Object._Object-number */
    component[2].type         = SVCSMALL;
    component[2].indicator    = 0;
    component[2].f.svcSmall   = lastUserObjectNumber;

    objectKeyHi.akey.index    = SCIDX_OBJID;
    objectKeyHi.akey.keycomps = 4;
    objectKeyHi.akey.ksubstr  = 0;

    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objectKeyHi.akey))
        return DSM_S_FAILURE;

    rc = (dsmStatus_t)cxFind (pcontext, 
			      &objectKeyHi.akey,&objectKeyLow.akey,
			      &cursorId, SCTBL_OBJECT, 
                              (dsmRecid_t *)&objectRecordId,
			      DSMPARTIAL, DSMFINDLAST, LKNOLOCK);
    
    if( rc == DSM_S_ENDLOOP )
    {
        /* This is the first user object of its type --
           give it the first number */
        *pObjectNumber = firstUserObjectNumber;
        rc = DSM_S_SUCCESS;
        goto done;
    }

    rc = omGetObjectRecord(pcontext, objectRecordId, NULL, pObjectNumber);
    if( rc )
        goto done;

    if(( *pObjectNumber <   lastUserObjectNumber) &&
        (*pObjectNumber < (DSMTABLE_USER_LAST - 1)))
    {
        (*pObjectNumber)++;
        goto done;
    }
    
    previousObjectNumber = *pObjectNumber;
    
    while (rc == DSM_S_SUCCESS)
    {
        /* If we're here the highest object number has been given out so
           scan previous looking for a gap in the object numbers.   */
	rc = (dsmStatus_t)cxNext(pcontext, &cursorId, SCTBL_OBJECT,
                                 (dsmRecid_t *)&objectRecordId,
				 &objectKeyLow.akey, &objectKeyHi.akey,
				 DSMGETTAG,DSMFINDPREV,LKNOLOCK);
        if ( rc == DSM_S_SUCCESS )
        {
            
            rc = omGetObjectRecord(pcontext, objectRecordId,
                                   NULL, pObjectNumber);
            if( rc )
            {
                MSGN_CALLBACK(pcontext, dbMSG039, rc);
                goto done;
            }

            if( *pObjectNumber < (previousObjectNumber - 1))
            {
                /* Found a gap */
                (*pObjectNumber)++;
                goto done;
            }
            previousObjectNumber = *pObjectNumber;
                 
        }
    }
    
done:
    
    cxDeactivateCursor(pcontext, &cursorId);
    if( rc == DSM_S_ENDLOOP )
        MSGN_CALLBACK(pcontext,dbMSG040); /* out of object numbers */
    else if ( rc != DSM_S_SUCCESS )
        MSGN_CALLBACK(pcontext,dbMSG041);
        
    return rc;

}  /* end omGetNewObjectNumber */

    
/* PROGRAM: omGetObjectRecordId - Return the recid of the _Object record
 *                                 for the given object number             
 *
 * RETURNS:  0 for success
 *           <0 for failure
 */
dsmStatus_t 
omGetObjectRecordId(
	dsmContext_t	*pcontext,
	ULONG		 objectType,
	COUNT		 objectNumber,
        dsmObject_t      tableNum,
	dsmRecid_t	*pObjectRecordId)
{
    dbcontext_t      *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t   rc;
    mstrblk_t    *pmstrblk = pdbcontext->pmstrblk;

    svcDataType_t component[5];
    svcDataType_t *pcomponent[5];
    COUNT         maxKeyLength = 64; /* SHOULD BE A CONSTANT! */

    AUTOKEY(objectKey, 64)
    AUTOKEY(keyHi, 64)

    /* set up key  for the find */
    objectKey.akey.area         = DSMAREA_SCHEMA;
    keyHi.akey.area             = DSMAREA_SCHEMA;
    objectKey.akey.root         = pmstrblk->mb_objectRoot;
    keyHi.akey.root             = pmstrblk->mb_objectRoot;
    objectKey.akey.unique_index = 1;
    keyHi.akey.unique_index     = 1;
    objectKey.akey.word_index   = 0;
    keyHi.akey.word_index       = 0;

    objectKey.akey.index       = SCIDX_OBJID;
    objectKey.akey.keycomps    = 4;
    objectKey.akey.ksubstr     = 0;

    /* First component - _Object._Db-recid */
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = (LONG)pmstrblk->mb_dbrecid;

    /* Second component _Object._Object-type */
    component[1].type            = SVCLONG;
    component[1].indicator       = 0;
    component[1].f.svcLong       = (LONG)objectType;

    /* Third  component - _Object._Object-associate */
    component[2].type            = SVCSMALL;
    component[2].indicator       = 0;
    component[2].f.svcSmall      = tableNum;

    /* Fourth  component - _Object._Object-number */
    component[3].type            = SVCSMALL;
    component[3].indicator       = 0;
    component[3].f.svcSmall      = objectNumber;

    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];
    pcomponent[2] = &component[2];
    pcomponent[3] = &component[3];

    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &objectKey.akey))
       return DSM_S_FAILURE;

    keyHi.akey.index           = SCIDX_OBJID;
    keyHi.akey.ksubstr         = 0;
    keyHi.akey.keycomps        = 5;

    /* Fifth component - SVCHIGHRANGE */
    component[4].indicator       = SVCHIGHRANGE;

    pcomponent[4] = &component[4];

    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &keyHi.akey))
        return DSM_S_FAILURE;

    /* Now find the recid of the object record.                    */
    rc = cxFind (pcontext, &objectKey.akey, &keyHi.akey, NULL, 
		 SCTBL_OBJECT, pObjectRecordId,
                  DSMUNIQUE, DSMFINDFIRST, LKNOLOCK);

    return rc;

}  /* end omGetObjectRecordId */


/* PROGRAM: omIdToArea - Given the input object-id(objectType + objectNumber)
 *                       return the area the object is in pArea.
 *
 * RETURNS: DSM_S_SUCCESS
 *          fatal exit if error
 */
dsmStatus_t
omIdToArea(
	dsmContext_t	*pcontext,
	ULONG		 objectType, 
	COUNT		 objectNumber,
        dsmObject_t      tableNum,
	ULONG		*pArea)
{
    dsmStatus_t   rc = DSM_S_SUCCESS;

    omDescriptor_t objDesc;
    
    /* all gemini schema tables (VST, AREA, etc) are in the 
       Schema Area */
    if (objectNumber < 0)
    {
      if(SCTBL_IS_TEMP_OBJECT(tableNum))
	*pArea = DSMAREA_TEMP;
      else
	*pArea = DSMAREA_SCHEMA;
    }
    else
    {
	rc = omFetch(pcontext, objectType, objectNumber,
                          tableNum, &objDesc);
	*pArea = objDesc.area;
    }
    return rc;

}  /* end omIdToArea */

/* PROGRAM: omDolru -  update the lru chain when referencing a cache entry
 *
 *      OM latch must be locked by the caller
 *      We make the specified cache entry the NEWEST one.
 *      The head of the circular chain is the OLDEST one.
 *      The one before that (the oldest one's previous) is the newest.
 *      Starting with the head (oldest), following the next links gets
 *      you progressively newer buffers. The newest buffer's next link
 *      points to the OLDEST buffer (the head of the chain).
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
omDolru (
	dsmContext_t	*pcontext,
	omObject_t	*pentry)
{
        dbcontext_t	*pdbcontext = pcontext->pdbcontext;
        omChain_t       *pLruChain = &pdbcontext->pomCtl->omLruChain;
	omObject_t	*pOldest;
	omObject_t	*pNewest;
	QOM_OBJECT      q;
	QOM_OBJECT      qNext;
	QOM_OBJECT      qPrev;
	omObject_t	*pNext;
	omObject_t	*pPrev;

    /* Make the entry the most recently used.    */

    if (pentry->qlruNext)
    {
	/* block is on lru chain. */ 

        qNext = pentry->qlruNext;
        if (pLruChain->qhead == qNext)
        {
	    /* Current buffer's next link points to the same buffer as
               the head does, so it is already the newest - do nothing */

	    return;
        }
        q = QSELF (pentry);
        if (pLruChain->qhead == q)
        {
	    /* it is now the oldest - make it the newest by rotating one.
               The newest is always the head's previous and the head is 
               always the oldest */

            pLruChain->qhead = qNext;
	    return;
        }
        /* disconnect from current spot. There must be at least three
	   entries on the chain because the one we want is not the most
	   recently used and it is not the least recently used. So we do
	   not have to check for the cases where we removed the first
	   or last entry */

        qPrev = pentry->qlruPrev;
        pNext = XOMOBJECT(pdbcontext, qNext);
        pPrev = XOMOBJECT(pdbcontext, qPrev);

        pNext->qlruPrev = qPrev;
        pPrev->qlruNext = qNext;

        /* Insert between newest and oldest. New entry becomes newest */

        pOldest = XOMOBJECT(pdbcontext, pLruChain->qhead);
	pNewest = XOMOBJECT(pdbcontext, pOldest->qlruPrev);

        pentry->qlruNext = QSELF (pOldest);
        pentry->qlruPrev = QSELF (pNewest);;

	pNewest->qlruNext = q;
        pOldest->qlruPrev = q;

	return;
    }
    /* Block not on chain so we add it between the newest and oldest */

    q = QSELF (pentry);
    if (pLruChain->qhead)
    {
        /* chain is not empty, insert between newest and oldest.
           New entry becomes newest. */

        pOldest = XOMOBJECT(pdbcontext, pLruChain->qhead);
	pNewest = XOMOBJECT(pdbcontext, pOldest->qlruPrev);

        pentry->qlruNext = QSELF (pOldest);
        pentry->qlruPrev = QSELF (pNewest);

	pNewest->qlruNext = q;
        pOldest->qlruPrev = q;

        pLruChain->numOnChain++;
	return;
    }
    /* New entry is the only one */

    pLruChain->numOnChain = 1;
    pLruChain->qhead = q;
    pentry->qlruNext = q;
    pentry->qlruPrev = q;

}  /* end bmdolru */

/* PROGRAM - omGetLru -- get the oldest object in the cache.
 */

LOCALF omObject_t *
omGetLru(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    omObject_t  *pOldestObject;
    
    /* Get least recently used cache entry                    */
    pOldestObject = XOMOBJECT(pdbcontext,
                              pdbcontext->pomCtl->omLruChain.qhead);

    /* Remove it from the chain its in.                       */
    pOldestObject = omRemoveFromChain(pcontext,pOldestObject->objectType,
                      pOldestObject->objectAssociate, 
                      pOldestObject->objectNumber);
    
    return pOldestObject;
}


/* PROGRAM - omRemoveFromChain
 */
LOCALF omObject_t *
omRemoveFromChain(dsmContext_t *pcontext,
                  ULONG		 objectType, 
                  dsmObject_t    tableNum,
                  COUNT		 objectNumber)
{
    omObject_t    *pObject, *pPrevious;
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    omCtl_t       *pomCtl = pdbcontext->pomCtl;
    LONG	   hashChain;

    hashChain = OM_HASH(objectType,tableNum,objectNumber);
    /* Search the hash chain                        */
    omFind(pcontext,objectType,tableNum,objectNumber,&pPrevious,&pObject);

    /* Remove object from the hash chain    */
    if( pObject )
    {
	if (pPrevious)
	{
	    pPrevious->qNextObject = pObject->qNextObject;
	}
	else
	{
	    pomCtl->qomTable[hashChain] = pObject->qNextObject;
	}    
    }
    return pObject;
}

/* PROGRAM - omGetNewTempObject
 */
dsmStatus_t
omGetNewTempObject(dsmContext_t *pcontext, dsmText_t *pname,
		   dsmObjectType_t type,
		   dsmObjectAttr_t objectAttr,
		   dsmObject_t  tableId,
		   dsmObject_t *pId)
{
  omTempObject_t	*ptempObj;
  omTempObject_t        *ptemp,*prev;
  dsmObject_t            id = SCTBL_TEMPTABLE_FIRST-1;
  usrctl_t              *pusr = pcontext->pusrctl;
  
  *pId = 0;

  ptempObj =(omTempObject_t *) stRentIt(pcontext,
						 &omTempObjectPool,
						 sizeof(omTempObject_t),
						 0);
  if(!ptempObj)
  {
    MSGD_CALLBACK(pcontext,
		  "%LStorage allocation failure for temporary table create");
    return DSM_S_FAILURE;
  }
  /* Get a new object number                           */
  if(type == DSMOBJECT_TEMP_TABLE)
  {
    for(ptemp = pusr->pfirstTtable,prev=NULL;
	ptemp;
	ptemp = ptemp->pnext)
    {
      if(ptemp->tableId > (id + 1))
	 /* Found a gap in ids use the gap */
	 break;
      id = ptemp->tableId;
      prev = ptemp;
    }
    id--;
    tableId = id;
  }
  else
  {
    for(ptemp = pusr->pfirstTindex, prev=NULL;
	ptemp;
	ptemp = ptemp->pnext)
    {
      if(tableId == ptemp->tableId)
      {
	for(;ptemp && tableId == ptemp->tableId;ptemp=ptemp->pnext)
	{
	  if(ptemp->id > (id + 1))
	    /* Found a gap in the ids use the gap */
	    break;
	  id = ptemp->id;
	  prev = ptemp;
	}
	break;
      }
      prev = ptemp;
    }
    id--;
  }
  if(id < SCTBL_TEMPTABLE_LAST)
  {
    stVacate(pcontext,&omTempObjectPool,(TEXT *)ptempObj);
    MSGD_CALLBACK(pcontext,"Can't create temp table, max of %l exceeded",
		  (LONG)(SCTBL_TEMPTABLE_FIRST - SCTBL_TEMPTABLE_LAST));
    return DSM_S_TEMPOBJECT_LIMIT_EXCEEDED;
  }

  if(prev)
  {
    ptempObj->pnext = prev->pnext;
    prev->pnext = ptempObj;
  }
  if(type == DSMOBJECT_TEMP_TABLE)
  {
    if(!pusr->pfirstTtable)
      pusr->pfirstTtable = ptempObj;
  }
  else
  {
    if(!pusr->pfirstTindex)
      pusr->pfirstTindex = ptempObj;
    ptempObj->firstBlock = cxCreateMaster(pcontext,DSMAREA_TEMP,
					  0,id, 1);
    ptempObj->lastBlock = ptempObj->firstBlock;
  }
  *pId = id;
  ptempObj->id = id;
  ptempObj->attributes = objectAttr;
  ptempObj->tableId = tableId;
  stncop(ptempObj->name,pname,sizeof(ptempObj->name));
  return 0;
}

/* PROGRAM - omNameToNum
 */
DSMVOID
omNameToNum(dsmContext_t *pcontext, dsmText_t *pname,
	    dsmObject_t *pId)
{
  omTempObject_t        *ptemp;

  for(ptemp=pcontext->pusrctl->pfirstTtable;
      ptemp;ptemp = ptemp->pnext)
  {
    if(stcomp(pname,ptemp->name) == 0)
    {
      *pId = ptemp->id;
      break;
    }
  }
  if(!ptemp)
  {
    /* Search the index chain for a match  */
    for(ptemp=pcontext->pusrctl->pfirstTindex;
      ptemp;ptemp = ptemp->pnext)
    {
      if(stcomp(pname,ptemp->name) == 0)
      {
	*pId = ptemp->id;
	break;
      }
    }
  }
  return;
}

/* PROGRAM: omFetchTempPtr -
 */
omTempObject_t *omFetchTempPtr(	dsmContext_t	*pcontext,
				ULONG		 objectType,
				COUNT		 objectNumber,
				dsmObject_t      associate)
{
  omTempObject_t	*ptemp;

  if(objectType == DSMOBJECT_MIXTABLE)
    ptemp = pcontext->pusrctl->pfirstTtable;
  else
    ptemp = pcontext->pusrctl->pfirstTindex;

  for(;ptemp;ptemp = ptemp->pnext)
  {
    if(ptemp->tableId == associate)
    {
      if(objectType == DSMOBJECT_MIXTABLE)
	break;
      else if(ptemp->id == objectNumber)
	break;
    }
  }
  return ptemp;
}

/* PROGRAM: omFetchTemp - Get the object descriptor for the object specified
 *                     by objectType and ObjectNumber.
 *
 * RETURNS: DSM_S_SUCCESS 
 */
dsmStatus_t
omFetchTemp(
	dsmContext_t	*pcontext,
	ULONG		 objectType,
        COUNT		 objectNumber,
        dsmObject_t      associate,
	omDescriptor_t	*pObjectDesc)
{
  omTempObject_t	*ptemp;

  ptemp = omFetchTempPtr(pcontext,objectType,objectNumber,associate);

  pObjectDesc->area = DSMAREA_TEMP;
  pObjectDesc->objectBlock = ptemp->lastBlock;
  pObjectDesc->objectRoot  = ptemp->firstBlock;
  pObjectDesc->objectAttributes = ptemp->attributes;
  pObjectDesc->objectAssociateType = 0;
  pObjectDesc->objectAssociate     = ptemp->tableId;
  
  return 0;
}

  
/* PROGRAM: omDeleteTemp - Delete the temporary objects associated
 *                         with the specified table number 
 *                     
 *
 * RETURNS: DSM_S_SUCCESS 
 */
dsmStatus_t
omDeleteTemp(dsmContext_t *pcontext, dsmObject_t tableNum)
{
  omTempObject_t	*ptemp,*prev=NULL;
  usrctl_t              *pusr=pcontext->pusrctl;

  ptemp = pusr->pfirstTindex;

  /* Delete the indexes    */
  for(;ptemp;)
  {
    if(ptemp->tableId == tableNum)
    {
      omTempObject_t *pcur;
      bkDeleteTempObject(pcontext,ptemp->firstBlock,ptemp->lastBlock,
			 ptemp->numBlocks);
      pcur = ptemp;
      ptemp = ptemp->pnext;
      stVacate(pcontext,&omTempObjectPool,(TEXT *)pcur);
    }
    else if(ptemp->tableId < tableNum)
      break;
    else
    {
      prev = ptemp;
      ptemp = ptemp->pnext;
    }
  }
  if(prev)
    prev->pnext=ptemp;
  else
    pusr->pfirstTindex = ptemp;
    
  ptemp = pusr->pfirstTtable;
  
  for(prev=NULL ;ptemp;ptemp = ptemp->pnext)
  {
    if(ptemp->tableId == tableNum)
    {
      bkDeleteTempObject(pcontext,ptemp->firstBlock,ptemp->lastBlock,
			 ptemp->numBlocks);
      if(prev)
      {
	prev->pnext=ptemp->pnext;
      }
      else
	pusr->pfirstTtable = ptemp->pnext;
      break;

      stVacate(pcontext,&omTempObjectPool,(TEXT *)ptemp);
    }
    prev = ptemp;
  }

  return 0;
}

/* PROGRAM: omTempObjectFirst - Return dbkey of first block of the 
   temporary object.                    */
DBKEY
omTempTableFirst(dsmContext_t *pcontext, dsmObject_t   id)
{

  omTempObject_t	*ptemp;

  ptemp = omFetchTempPtr(pcontext,DSMOBJECT_MIXTABLE,id,id);
  
  return ptemp->firstBlock;
} 
