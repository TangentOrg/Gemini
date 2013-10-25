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

#ifndef DSMPUB_H
#define DSMPUB_H

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(WIN32)
#define OPSYS WIN32API
#else
#include "gem_config.h"
#endif
#include "pscdtype.h"

#include "dsmret.h"   /* dsmAPI return codes */
#include "dsmdef.h"   /* dsmAPI mode and flag definitions */
#include "dsmdtype.h" /* dsmAPI type definitions */

/* library version number.  Needs to be incremented for every
   API change */
#define DSM_API_VERSION 2

/* For use in C++ by SQL engine, define entire include file to have "C linkage" . */
/* This is technique used by all standard .h sources in /usr/include,...          */
#ifdef  __cplusplus
extern "C" {
#endif


/**************************************************************
 * This header file contains all the public function prototypes
 * used in support of the dsmAPI. (listed alphabetically by .c)
 *************************************************************/

/* Collect the pointers to all of the structures which
 * constitute the database context for a data storage manager
 * request into the dsmContext structure.
 */
DSMVOID  DLLEXPORT uttrace();
/*************************************************************/
/* Public Function Prototypes for dsmarea.c                  */
/*************************************************************/

dsmStatus_t DLLEXPORT dsmAreaCreate( 
              dsmContext_t 	*pcontext, 	/* IN database context */ 
              dsmBlocksize_t	blockSize, 	/* IN are block size in bytes */
	      dsmAreaType_t	areaType, 	/* IN type of area */
	      dsmArea_t		areaNumber,	/* IN area number */
              dsmRecbits_t	areaRecbits,    /* IN # of record bits */
              dsmText_t        *areaName);      /* IN name of new area */

dsmStatus_t DLLEXPORT dsmAreaDelete(
              dsmContext_t 	*pcontext,	/* IN database context */ 
              dsmArea_t		area);		/* IN area to delete */


dsmStatus_t DLLEXPORT dsmAreaNew(
              dsmContext_t 	*pcontext,     /* IN database context */
	      dsmBlocksize_t	blockSize,     /* IN are block size in bytes */
	      dsmAreaType_t	areaType,      /* IN type of area */
	      dsmArea_t		*pareaNumber,  /* OUT area number */
              dsmRecbits_t	areaRecbits,   /* IN # of record bits */
              dsmText_t        *areaName);     /* IN name of new area */

dsmStatus_t DLLEXPORT dsmExtentCreate( 
                dsmContext_t	*pcontext,	/* IN database context */
               	dsmArea_t 	area,		/* IN area for the extent */
	        dsmExtent_t	extentId,	/* IN extent id */
	        dsmSize_t 	initialSize,	/* IN initial size in blocks */
	        dsmMask_t	extentMode,	/* IN extent mode bit mask */
	        dsmText_t	*pname);	/* IN extent path name */

dsmStatus_t DLLEXPORT dsmExtentDelete(
                dsmContext_t	*pcontext,	/* IN database context */ 
                dsmArea_t	area);		/* IN area to delete */

dsmStatus_t DLLEXPORT  dsmExtentUnlink(dsmContext_t *pcontext);

dsmStatus_t DLLEXPORT  dsmAreaClose(dsmContext_t *pcontext, dsmArea_t area);
dsmStatus_t DLLEXPORT  dsmAreaOpen(dsmContext_t *pcontext, dsmArea_t area, int refresh);

/* flags for dsmAreaFlush */
#define FLUSH_BUFFERS  0x00000001
#define FREE_BUFFERS   0x00000002
#define FLUSH_SYNC     0x00000004

dsmStatus_t DLLEXPORT  dsmAreaFlush(dsmContext_t *pcontext, dsmArea_t area, int flags);

/*************************************************************/
/* Public Function Prototypes for dsmblob.c                  */
/*************************************************************/

dsmStatus_t DLLEXPORT dsmBlobGet( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob,         /* IN blob descriptor */
    dsmText_t           *pName);        /* IN reference name for messages */

dsmStatus_t DLLEXPORT dsmBlobPut( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob,         /* IN blob descriptor */
    dsmText_t           *pName);        /* IN reference name for messages */

dsmStatus_t  DLLEXPORT dsmBlobUpdate( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob,         /* IN blob descriptor */
    dsmText_t           *pName);        /* IN reference name for messages */

dsmStatus_t  DLLEXPORT dsmBlobDelete( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob,         /* IN blob descriptor */
    dsmText_t           *pName);        /* IN reference name for messages */

dsmStatus_t  DLLEXPORT dsmBlobStart( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob);        /* IN blob descriptor */

dsmStatus_t  DLLEXPORT
dsmRowCount(
        dsmContext_t    *pcontext,
        dsmTable_t       tableNumber,
        ULONG64          *prows);


dsmStatus_t  DLLEXPORT dsmBlobEnd( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob);        /* IN blob descriptor */

dsmStatus_t  DLLEXPORT dsmBlobUnlock( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmBlob_t           *pblob);        /* IN blob descriptor */


/*************************************************************/
/* Public Function Prototypes for dsmcon.c                   */
/*************************************************************/

int DLLEXPORT dsmGetVersion();

dsmStatus_t DLLEXPORT dsmContextCopy( 
        dsmContext_t  *psource,   /* IN  database context */
        dsmContext_t **pptarget,  /* OUT database context */
        dsmMask_t      copyMode); /* IN  what to copy     */

dsmStatus_t DLLEXPORT dsmContextCreate(dsmContext_t **pcontext); /* OUT database context */
dsmStatus_t DLLEXPORT dsmContextDelete(dsmContext_t *pcontext); /* IN/OUT database context */
dsmStatus_t DLLEXPORT dsmContextGetLong( 
    dsmContext_t        *pcontext,  /* IN/OUT database context  */
    dsmTagId_t          tagId,      /* IN Tag identifier        */
    dsmTagLong_t        *ptagValue);/* IN tag value             */

dsmStatus_t DLLEXPORT dsmContextGetString( 
    dsmContext_t        *pcontext,  /* IN/OUT database context */
    dsmTagId_t          tagId,      /* Tag identifier          */
    dsmLength_t         tagLength,  /* Length of string        */
    dsmBuffer_t         *ptagString);  /* tagValue              */

dsmStatus_t DLLEXPORT dsmContextSetLong( 
    dsmContext_t        *pcontext,  /* IN/OUT database context  */
    dsmTagId_t          tagId,      /* IN Tag identifier        */
    dsmTagLong_t        tagValue);  /* IN tag value             */

dsmStatus_t DLLEXPORT dsmContextSetString( 
    dsmContext_t        *pcontext,    /* IN/OUT database context */
    dsmTagId_t          tagId,        /* IN Tag identifier       */
    dsmLength_t         tagLength,    /* IN Length of string     */
    dsmBuffer_t         *ptagString); /* IN tagValue             */

dsmStatus_t DLLEXPORT dsmContextSetVoidFunction( 
    dsmContext_t         *pcontext,  /* IN/OUT database context */
    dsmTagId_t           tagId,      /* Tag identifier          */
#if 1
    int (*msg_callback)());
#else
    dsmTagVoidFunction_t tagValue);  /* Tag value               */
#endif

dsmStatus_t DLLEXPORT dsmContextGetVoidPointer(  
    dsmContext_t         *pcontext,  /* IN/OUT database context */
    dsmTagId_t            tagId,     /* IN Tag identifier       */   
    dsmVoid_t            **pptagValue); /* OUT tagValue         */

dsmStatus_t DLLEXPORT dsmContextSetVoidPointer( 
    dsmContext_t         *pcontext,  /* IN/OUT database context */
    dsmTagId_t            tagId,     /* Tag identifier          */   
    dsmVoid_t            *ptagValue);/* tagValue                */

dsmStatus_t DLLEXPORT dsmMsgnCallBack(
    dsmContext_t         *pcontext,     /* IN database context           */
    int                   msgNumber,    /* IN message number for lookup  */
    ...);                               /* IN variable length arg list   */

dsmStatus_t DLLEXPORT dsmFatalMsgnCallBack(
    dsmContext_t         *pcontext,     /* IN database context           */
    int                   msgNumber,    /* IN message number for lookup  */
    ...);                               /* IN variable length arg list   */

dsmStatus_t DLLEXPORT dsmMsgdCallBack(
    dsmContext_t         *pcontext,     /* IN database context              */
    ...);                               /* IN variable length arg list      */ 

dsmStatus_t DLLEXPORT dsmMsgtCallBack(
    dsmContext_t         *pcontext,     /* IN database context              */
    ...);                               /* IN variable length arg list      */

dsmStatus_t
dsmThreadSafeEntry(
    dsmContext_t         *pcontext);

dsmStatus_t
dsmThreadSafeExit(
    dsmContext_t         *pcontext);

dsmStatus_t
dsmEntryProcessError(
    dsmContext_t         *pcontext,     /* IN database context    */
    dsmStatus_t           status,       /* IN the error status value */
    dsmText_t            *caller);

dsmStatus_t DLLEXPORT dsmContextWriteOptions( dsmContext_t         *pcontext);    /* IN database context */

#if OPSYS == WIN32API
/* this here for now out of convienence.  later it should all move */
dsmStatus_t DLLEXPORT dsmLookupEnv(char   *findKey,    /* Registry key to query for value */ char   *keyValue,   /* OUT value for registry key      */
             DWORD  keySize);    /* Size expected ofkeyValue        */

#endif /* OPSYS == WIN32API */


/*************************************************************/
/* Public Function Prototypes for dsmcursr.c                 */
/*************************************************************/

dsmStatus_t DLLEXPORT dsmCursorCreate( 
        dsmContext_t *pcontext, /* IN/OUT database context */
        dsmTable_t    table,    /* IN table associated with cursor */
        dsmIndex_t    index,    /* IN index associated with cursor */
        dsmCursid_t  *pcursid,  /* OUT created cursor id */
        dsmCursid_t  *pcopy);   /* IN cursor id to copy */

dsmStatus_t DLLEXPORT dsmCursorFind( 
        dsmContext_t *pcontext,         /* IN database context */
        dsmCursid_t  *pcursid,          /* IN/OUT cursor */
        dsmKey_t     *pkeyBase,         /* IN base key of bracket for find */
        dsmKey_t     *pkeylimit,        /* IN limit key of bracket for find */
        dsmMask_t     findType,         /* IN DSMPARTIAL DSMUNIQUE ...      */
        dsmMask_t     findMode,         /* IN find mode bit mask */
        dsmMask_t     lockMode,         /* IN lock mode bit mask */
        dsmObject_t  *pObjectNumber,    /* OUT object number the index is on */
        dsmRecid_t   *precid,           /* OUT recid found */
        dsmKey_t     *pKey);            /* OUT [optional] key found */

struct cxcursor;

dsmStatus_t DLLEXPORT dsmCursorGet( 
        dsmContext_t *pcontext,      /* IN database context */
        dsmCursid_t   cursid,        /* IN cursor id to find */
        struct cxcursor  **pcursor); /* OUT pointer to cursor structure */

dsmStatus_t DLLEXPORT dsmCursorDelete( 
        dsmContext_t  *pcontext,        /* IN database context */
        dsmCursid_t   *pcursid,         /* IN  pointer to cursor to delete */
        dsmBoolean_t  deleteAll);       /* IN delete all cursors indicator */


/*************************************************************/
/* Public Function Prototypes for dsmindex.c                 */
/*************************************************************/

dsmStatus_t DLLEXPORT dsmIndexCompact(  
        dsmContext_t  *pcontext,           /* IN database context */
        dsmIndex_t    index,               /* IN index to be compressed */
        dsmTable_t    table,               /* IN table associated idx */
        ULONG         percentUtilization); /* IN % required space util. */

dsmStatus_t DLLEXPORT
dsmIndexRowsInRange(
        dsmContext_t  *pcontext,           /* IN database context */
        dsmKey_t      *pbase,              /* IN bracket base     */
        dsmKey_t      *plimit,             /* IN bracket limit    */
        dsmObject_t    tableNum,
        float         *pctInrange);        /* Out percent of index in bracket */

dsmStatus_t DLLEXPORT
dsmIndexStatsGet(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* IN index component number */
        LONG64        *prowsPerKey);       /* IN rows per key value */

dsmStatus_t DLLEXPORT
dsmIndexStatsPut(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* IN index component # */
        LONG64        rowsPerKey);         /* IN rows per key value */

dsmStatus_t DLLEXPORT dsmKeyCreate( 
        dsmContext_t  *pcontext,   /* IN database context */
	dsmKey_t      *pkey,       /* IN the key to be inserted in the index*/
	dsmTable_t   table,        /* IN the table the index is on.         */
	dsmRecid_t   recordId,     /* IN the recid of the record the 
			              index entry is for         */
	dsmText_t    *pname);	   /* IN refname for Rec in use msg    */

dsmStatus_t DLLEXPORT dsmKeyDelete(   
        dsmContext_t *pContext,
	dsmKey_t *pkey,            /* IN the key to be inserted in the index*/
	COUNT   table,             /* IN the table the index is on.         */
	dsmRecid_t recordId,       /* IN the recid of the record the 
			              index entry is for           	*/
	dsmMask_t    lockmode,     /* IN lockmode            */
	dsmText_t    *pname);	   /* refname for Rec in use msg           */

dsmStatus_t DLLEXPORT 
dsmObjectNameToNum(dsmContext_t *pcontext,   /* IN database context */
		   dsmBoolean_t  tempTable,  /* IN 1 - if temporary object */
		   dsmText_t  *pname,          /* IN the name */
		   dsmObject_t         *pnum);  /* OUT number */

/*************************************************************/
/* Public Function Prototypes for dsmobj.c                   */
/*************************************************************/

dsmStatus_t  DLLEXPORT dsmObjectCreate( 
            dsmContext_t       *pContext,   /* IN database context */
	    dsmArea_t 	       area,        /* IN area */
	    dsmObject_t	       *pobject,    /* IN/OUT for object id */
	    dsmObjectType_t    objectType,  /* IN object type */
	    dsmObjectAttr_t    objectAttr,  /* IN object attributes */
            dsmObject_t        associate,   /* IN object number of associate
                                               object */
            dsmObjectType_t    associateType, /* IN type of associate object
                                                 usually an index           */
            dsmText_t          *pname,      /* IN name of the object */
	    dsmDbkey_t         *pblock,     /* IN/OUT dbkey of object block */
	    dsmDbkey_t         *proot);     /* OUT dbkey of object root */

dsmStatus_t  DLLEXPORT dsmObjectDelete(
            dsmContext_t       *pContext,  /* IN database context */
	    dsmObject_t	       object,     /* IN object id */
	    dsmObjectType_t    objectType, /* IN object type */
	    dsmMask_t          deleteMode, /* IN delete mode bit mask */
            dsmObject_t        tableNum,   /* IN object number to use
                                               for sequential scan of tables*/
	    dsmText_t          *pname);    /* IN reference name for messages */

dsmStatus_t  DLLEXPORT dsmObjectInfo( 
    dsmContext_t       *pContext,          /* IN database context */
    dsmObject_t         objectNumber,      /* IN object number    */
    dsmObjectType_t     objectType,        /* IN object type      */
    dsmObject_t         associate,         /* IN Associate object number */
    dsmArea_t           *parea,            /* OUT area object is in */
    dsmObjectAttr_t     *pobjectAttr,      /* OUT Object attributes    */
    dsmObjectType_t     *passociateType,   /* OUT Associate object type  */
    dsmDbkey_t          *pblock,           /* OUT dbkey of object block  */
    dsmDbkey_t          *proot);           /* OUT dbkey of index root block */
    
dsmStatus_t  DLLEXPORT dsmObjectLock( 
            dsmContext_t       *pContext,  /* IN database context */
	    dsmObject_t	       object,     /* IN object id */
	    dsmObjectType_t    objectType, /* IN object type */
	    dsmRecid_t         recid,      /* IN object type */
	    dsmMask_t          lockMode,   /* IN delete mode bit mask */
            COUNT              waitFlag,   /* IN client:1(wait), server:0 */
	    dsmText_t          *pname);    /* IN reference name for messages */

dsmStatus_t  DLLEXPORT dsmObjectUnlock( 
            dsmContext_t       *pContext,  /* IN database context */
	    dsmObject_t	       object,     /* IN object id */
	    dsmObjectType_t    objectType, /* IN object type */
	    dsmRecid_t         recid,      /* IN object type */
	    dsmMask_t          lockMode);  /* IN delete mode bit mask */


dsmStatus_t DLLEXPORT dsmObjectUpdate( 
            dsmContext_t       *pcontext,   /* IN database context */
            dsmArea_t 	       area,        /* IN area */
            dsmObject_t	       object,      /* IN  for object id */
	    dsmObjectType_t    objectType,  /* IN object type */
            dsmObjectAttr_t    objectAttr,  /* IN object attributes */
            dsmObject_t        associate,   /* IN object number to use
                                               for sequential scan of tables*/
            dsmObjectType_t    associateType, /* IN object type of associate
                                                 usually an index           */

	    dsmDbkey_t         obectBlock,  /* IN dbkey of object block */
	    dsmDbkey_t         rootBlock,   /* IN dbkey of object root
                                             * - for INDEX: index root block
                                             * - 1st possible rec in the table.
                                             */
            dsmMask_t           pupdateMask);/* bit mask identifying which
                                                 storage object attributes are
                                                 to be modified. */
dsmStatus_t DLLEXPORT dsmObjectDeleteAssociate(
            dsmContext_t    *pcontext,        /* IN database context */
	    dsmObject_t      tableNum,        /* IN number of table */
            dsmArea_t       *indexArea);      /* OUT area for the indexes */

dsmStatus_t DLLEXPORT dsmObjectRename( 
            dsmContext_t   *pContext,     /* IN database context */
            dsmObject_t     tableNumber,  /* IN table number */
            dsmText_t      *pNewName,     /* IN new name of the object */
            dsmText_t      *pIdxExtName,  /* IN new name of the index extent */
            dsmText_t      *pNewExtName,  /* IN new name of the extent */
            dsmArea_t      *pIndexArea,   /* OUT index area */
            dsmArea_t      *pTableArea);  /* OUT table area */

    
/*************************************************************/
/* Public Function Prototypes for dsmrec.c                   */
/*************************************************************/
dsmStatus_t DLLEXPORT dsmRecordCreate ( 
        dsmContext_t  *pContext,     /* IN database context */
        dsmRecord_t   *pRecord);     /* IN/OUT record descriptor */

dsmStatus_t DLLEXPORT dsmRecordDelete ( 
        dsmContext_t *pContext,     /* IN database context */
        dsmRecord_t  *pRecord,      /* IN record to delete */
        dsmText_t    *pname);       /* IN msg reference name */

dsmStatus_t DLLEXPORT dsmRecordGet( 
        dsmContext_t *pContext,     /* IN database context */
        dsmRecord_t  *pRecord,      /* IN/OUT rec buffer to store record */
        dsmMask_t     getMode);     /* IN get mode bit mask */

dsmStatus_t DLLEXPORT dsmRecordUpdate( 
        dsmContext_t *pContext,     /* IN database context */
        dsmRecord_t  *pRecord,      /* IN new record to store */
        dsmText_t    *pName);       /* IN msg reference name */

typedef struct
{
  DBKEY lockid;
  TEXT  lockname[20];
  TEXT  locktype[10];
  TEXT  lockflags[10];
} GEM_LOCKINFO;

dsmStatus_t DLLEXPORT dsmLockQueryInit(
        dsmContext_t *pcontext,     /* IN database context */
        int           getLock);     /* IN lock/unlock lock table */

dsmStatus_t DLLEXPORT dsmLockQueryGetLock(
        dsmContext_t *pcontext,     /* IN database context */
        dsmTable_t    tableNum,     /* IN table number */
        GEM_LOCKINFO *plockinfo);   /* OUT lock information */

/*************************************************************/
/* Public Function Prototypes for dsmseq.c                   */
/*************************************************************/

dsmStatus_t DLLEXPORT dsmSeqCreate( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmSeqParm_t        *pseqparm);     /* IN Sequence definition parameters */
    
dsmStatus_t DLLEXPORT dsmSeqDelete( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmSeqId_t           seqId);        /* IN Sequence identifier */

dsmStatus_t DLLEXPORT dsmSeqInfo( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmSeqParm_t        *pseqparm);     /* IN sequence id
                                         * OUT Sequence definition */
dsmStatus_t DLLEXPORT dsmSeqGetValue( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmSeqId_t           seqId,         /* IN Sequence identifier */
    GTBOOL                current,       /* IN == 0 -> NEXT, != 0 -> CURRENT */
    dsmSeqVal_t         *pValue);       /* OUT The sequence value */

dsmStatus_t DLLEXPORT dsmSeqSetValue( 
    dsmContext_t        *pcontext,      /* IN database context */
    dsmSeqId_t           seqId,         /* IN Sequence identifier */
    dsmSeqVal_t          value);        /* IN The 'new' current value */

/*************************************************************/
/* Public Function Prototypes for dsmtable.c                 */
/*************************************************************/

dsmStatus_t DLLEXPORT 
dsmTableAutoIncrement(dsmContext_t *pcontext, dsmTable_t tableNumber,
                      ULONG64 *pvalue,int update);

dsmStatus_t DLLEXPORT 
dsmTableAutoIncrementSet(dsmContext_t *pcontext, dsmTable_t tableNumber,
                      ULONG64 value);

dsmStatus_t DLLEXPORT
dsmTableStatus(dsmContext_t *pcontext, 
               dsmTable_t tableNumber,
               dsmMask_t *ptableStatus);

dsmStatus_t DLLEXPORT
dsmTableReset(dsmContext_t *pcontext, 
              dsmTable_t tableNumber, 
              int numIndexes,
              dsmIndex_t   *pindexes);

dsmStatus_t DLLEXPORT dsmTableScan(
            dsmContext_t    *pcontext,        /* IN database context */
	    dsmRecord_t     *precordDesc,     /* IN/OUT row descriptor */
	    int              direction,       /* IN next, previous...  */
	    dsmMask_t        lockMode,        /* IN nolock share or excl */
            int              repair);         /* IN repair rm chain     */

/*************************************************************/
/* Public Function Prototypes for dsmtrans.c                 */
/*************************************************************/
dsmStatus_t DLLEXPORT dsmTransaction( 
        dsmContext_t  *pcontext,          /* IN/OUT database context */
        dsmTxnSave_t  *pSavePoint,        /* IN/OUT savepoint           */
        dsmTxnCode_t  txnCode);           /* IN function code       */


/*************************************************************/
/* Public Function Prototypes for dsmuser.c                  */
/*************************************************************/

dsmStatus_t DLLEXPORT dsmUserConnect( 
        dsmContext_t *pContext, /* IN/OUT database context */
        dsmText_t    *prunmode, /* IN string describing use, goes in lg file  */
        int           mode);    /* bit mask with these bits:
                                   DBOPENFILE, DBOPENDB, DBSINGLE, DBMULTI,
                                   DBRDONLY, DBNOXLTABLES, DBCONVERS */

dsmStatus_t DLLEXPORT dsmUserDisconnect(
        dsmContext_t *pContext,    /* IN/OUT database context */
        int           exitflags);  /* may have NICEBIT, etc   */ 

dsmStatus_t DLLEXPORT dsmUserReset(
     dsmContext_t *pContext);   /* IN/OUT database context */

#if 0
dsmStatus_t DLLEXPORT dsmUserSetName(
        dsmContext_t *pContext,    /* IN database context */
        dsmText_t    *pUsername,   /* IN username to set */
        QTEXT        *pq);         /* OUT the qptr to the filled string */
#endif

dsmStatus_t DLLEXPORT dsmUserAdminStatus(
        dsmContext_t      *pcontext,     /* IN database context */
        psc_user_number_t  userNumber,   /* IN user number for info retrieval */
        dsmUserStatus_t   *puserStatus); /* OUT user status info structure */

/*************************************************************/
/* Public Function Prototypes for dsmscache.c                */
/*************************************************************/

struct meta_filectl;
struct dl_vect;
struct mand;

dsmStatus_t DLLEXPORT dsmMandatoryFieldsGet(
           dsmContext_t *pcontext, 
           struct meta_filectl *pmetactl,    /* mand. table from shm    */
           dsmDbkey_t          fileDbkey,    /* dbkey of _file record   */
           int                 fileNumber,   /* file # of _file record  */
           dsmBoolean_t        fileKey,      /* dbkey/file# of _file    */
           dsmBoolean_t        copyFound,    /* flag to copy mand. flds */
           struct mand         *pmand);      /* storage for mand. array */
 
 
dsmStatus_t DLLEXPORT dsmMandatoryFieldsPut(
          dsmContext_t *pcontext, 
          struct meta_filectl *pInMetactl,    /* new metadata ctl for cache */
          struct meta_filectl *pOldmetactl,   /* old metadata ctl for cache */
          struct dl_vect      *pdl_vect_in,   /* next dl column vector */
          struct dl_vect      *pdl_col_in,    /* next dl columns list to use */
          int                 ndl_ent,        /* # delayed columns */
          int                 mandtblSize,    /* size of mand fld table */
          int                 useCache,       /* use shm cache ? */
          struct mand         *pmand);        /* storage for all mand flds */
 
dsmStatus_t dsmMandFindEntry(
                dsmContext_t         *pcontext,
                struct meta_filectl **ppmetactl, /* OUTPUT: metactl found */
                dsmDbkey_t           fileDbkey,  /* dbkey of _file record   */
                int                  fileNumber, /* file # of _file record  */
                dsmBoolean_t         fileKey);   /* dbkey/file# of _file    */

dsmStatus_t DLLEXPORT dsmSchemaGetStamp(
        dsmContext_t *pcontext,     /* IN database context */
        LONG         *pstamp);      /* OUT The schema stamp value */

dsmStatus_t DLLEXPORT dsmSchemaPutStamp(
        dsmContext_t *pcontext,    /* IN database context */
        LONG         *pstamp);     /* IN ptr to schema stamp */

/*************************************************************/
/* Public Function Prototypes for dsmshutdn.c                */
/*************************************************************/
dsmStatus_t DLLEXPORT dsmShutdownMarkUser(
        dsmContext_t     *pcontext,    /* IN database context */
        psc_user_number_t userNumber); /* IN the usernumber */

dsmStatus_t DLLEXPORT dsmShutdownUser(
        dsmContext_t     *pcontext,   /* IN database context */
        psc_user_number_t server);    /* IN the server identifier */

/*************************************************************/
/* Public Function Prototypes for dsmsrv.c                   */
/*************************************************************/
struct srvctl;
struct serverGroupId;

dsmStatus_t DLLEXPORT dsmSrvctlGetFields(
        dsmContext_t  *pcontext,        /* IN database context */
        int            srvOffset,       /* IN servers's srvctl index */
        struct srvctl *poutSrvctl);     /* IN/OUT location to put values */

dsmStatus_t DLLEXPORT dsmSrvctlSetFields(
        dsmContext_t  *pcontext,        /* IN database context */
        struct srvctl *pinSrvctl,       /* IN temporary srvctl to copy from */
        int            srvOffset,       /* IN server's srvctl index */
        dsmMask_t      fieldMask);      /* IN fields to be copied */

dsmStatus_t DLLEXPORT dsmSrvctlIncFields(
        dsmContext_t *pcontext,         /* IN database context */
        int           srvOffset,        /* IN server's srvctl index */
        dsmMask_t     fieldMask);       /* IN fields to be copied */

dsmStatus_t DLLEXPORT dsmSrvctlGetBest(
        dsmContext_t  *pcontext,        /* IN database context */
        int           *psrvOffset,      /* OUT chosen srvctl index */
        int            addClient);      /* IN - increment user count */

dsmStatus_t DLLEXPORT dsmSrvctlGetBestx(
        dsmContext_t  *pcontext,        /* IN database context */
        int           *psrvOffset,      /* OUT chosen srvctl index */
        int            addClient,       /* IN - increment user count */
        struct serverGroupId *pserverGroupId); /* IN server groupid to use */

dsmStatus_t DLLEXPORT dsmShutdown(
        dsmContext_t  *pcontext,        /* IN database context */
        int            exitFlags,
        int            exitCode);

dsmStatus_t DLLEXPORT dsmShutdownSet(
        dsmContext_t  *pcontext,        /* IN database context */
        int            flag);

dsmStatus_t DLLEXPORT dsmWatchdog(
        dsmContext_t  *pcontext);       /* IN database context */

dsmStatus_t DLLEXPORT dsmDatabaseProcessEvents(
        dsmContext_t  *pcontext);       /* IN database context */

dsmStatus_t DLLEXPORT dsmRLwriter( dsmContext_t *pcontext);

dsmStatus_t DLLEXPORT dsmAPW( dsmContext_t *pcontext);

dsmStatus_t DLLEXPORT dsmOLBackupInit(
        dsmContext_t *pcontext);    /* IN database context */

dsmStatus_t DLLEXPORT dsmOLBackupEnd(
        dsmContext_t *pcontext);    /* IN database context */

dsmStatus_t DLLEXPORT dsmOLBackupRL(
        dsmContext_t *pcontext,     /* IN database context */
        dsmText_t    *ptarget);     /* IN backup target path and filename */

dsmStatus_t DLLEXPORT dsmOLBackupSetFlag(
        dsmContext_t *pcontext,      /* IN database context */
        dsmText_t    *ptarget,       /* IN path to gemini db */
        dsmBoolean_t  backupStatus); /* IN turn backup bit on/off */


/*************************************************************/
/*                                                           */
/*************************************************************/
#if 0
/* THE MACRO DSMSETCONTEXT SHOULD NEVER BE USED AT OR BELOW THE DSM
 * LAYER. It is provided for the convenience of the callers of the DSM 
 * layer to localize the global context of the user.  Eventually all 
 * references to the globals p_dbctl and pusrctl within the Database 
 * Storage Manager should be replaced by passing a pointer to a dsmContext
 * to whatever routines need access to either the database control or the
 * user control.
 */
#define DSMSETCONTEXT(pContext, pdbcontextIn, pusr)             \
                       (pContext)->pdbcontext = (pdbcontextIn); \
                       (pContext)->pusrctl = (pusr);            \
                       (pContext)->ptxnctl = NULL;              \
                       (pContext)->connectNumber =              \
                                 (pusr) ? (pusr)->uc_connectNumber : 0
#endif
/**********************************************************************/
/* The following will soon be replaced by the definitions in dsmret.h */
/**********************************************************************/

/* Error Return Codes used in CX layor.   */
#define DSMSUCCESS 0
#define DSMFAILURE -1
#define DSMNOCURSOR -2
#define DSMKEYNOTFOUND -3
#define DSMNOTFOUND -3
#define DSMBADLOCK -4
#define DSMDUPLICATE -5
#define DSM_FULLKEYHDRSZ  6  /* 6 bytes are used in  memory key header
                              in cursors and other in memory structures
                              to store sizes as follows:
                           2 bytes - the  total size stored with sct
                           2 bytes - the  size of the key (ks) stored with sct
                           2 byte  - the  size of the info (is)stored with sct
                             The header of an entry in an index block is
                         different than this one */

/* For use in C++ by SQL engine, define entire include file to have "C linkage" . */
/* This is technique used by all standard .h sources in /usr/include,...          */
#ifdef  __cplusplus
}
#endif


/*************************************************************/
/* Private Function Prototypes for dsmobj.c                  */
/*************************************************************/
dsmStatus_t DLLEXPORT 
dsmObjectRecordCreate(
        dsmContext_t    *pcontext,
        dsmArea_t        areaNumber,
        dsmObject_t      objectNumber,
        dsmObjectType_t  assoicateType,
        dsmObject_t      associate,
        dsmText_t       *pname,
        dsmObjectType_t  objectType,
        dsmObjectAttr_t  objectAttr,
        dsmDbkey_t       objectBlock,
        dsmDbkey_t       objectRoot,
        ULONG            objectSystem);

#endif /* DSMPUB_H  */
