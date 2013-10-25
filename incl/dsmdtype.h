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

#ifndef DSMDTYPE_H
#define DSMDTYPE_H

/************************************************************* 
 * This header file contains all the necessary data type
 * definitions used in support of the dsmAPI.
 *************************************************************/

#include <setjmp.h>

typedef DBKEY dsmRecid_t;   /* type for record identifier within area */
typedef COUNT dsmTable_t;   /* type for table numbers.                */
typedef ULONG dsmLength_t;  /* unsigned length                        */
typedef TTINY dsmBuffer_t;  /* type for buffer of bytes.              */
typedef LONG  dsmStatus_t;  /* type for status return from procedures */
typedef TEXT  dsmText_t;
typedef ULONG dsmMask_t;    /* type for bit masks                     */
typedef	GTBOOL dsmBoolean_t; /* 1 is true, 0 is false */
typedef DSMVOID  dsmVoid_t;    /* type for opaque pointer                */
typedef UCOUNT dsmRecbits_t; /* type for number of rec bits per area */

struct dbcontext;
struct ditem;
struct usrctl;
struct txnctl;
struct dsmVoid;
struct rmundo;

typedef struct dsmEnv
{
    int     jumpBufferValid;
    jmp_buf jumpBuffer;    /* setjump saved environment struct */
} dsmEnv_t;

typedef struct dsmContext
{
    struct  dbcontext *pdbcontext; /* database */
    struct  usrctl    *pusrctl;   /* user     */
    struct  txnctl    *ptxnctl;   /* transaction */
    ULONG   connectNumber;        /* Connect sequence number 
                                   * prevents use of stale context 
                                   */
    struct  dsmVoid   *pmessageParm; /* Set by caller of DSM for
                                        message sub-system call back */
    int  (*msgCallback)();        /* call back for msgn, msgd etc.   */
    DSMVOID (*exitCallback)();       /* call back for exit processing   */
    int     logsemlk;             /* number of locks I have on log semaphore */
    dsmEnv_t savedEnv;            /* setjump saved environment struct */
    struct rmundo *prmundo;       /* Assembly buffer for multiblock rec undo */
    struct dsmContext *pnextContext;
    int     noLogging;            /* Set if this connection doesn't
                                     require recovery logging       */
    TEXT   *puserName;            /* name of the connecting user */
} dsmContext_t;

typedef struct dsmRecord
{
    dsmRecid_t    recid;           /* recid of record  */
    dsmTable_t    table;           /* table number of table containing record*/
    dsmBuffer_t   *pbuffer;        /* pointer to record buffer.              */
    dsmLength_t   recLength;       /* length of the record      */
    dsmLength_t   maxLength;       /* length of what pbuffer points to */
} dsmRecord_t;


typedef ULONG dsmArea_t;	/* area id */

typedef ULONG dsmAreaType_t;		/* area type */

typedef	ULONG dsmExtent_t;		/* extent id */

typedef	COUNT dsmObject_t;              /* type for object identifiers */
typedef ULONG dsmObjectType_t;		/* storage object type */
typedef	ULONG dsmObjectAttr_t;          /* type for object Attributes */

typedef DBKEY dsmDbkey_t;	/* dbkey value */
typedef DBKEY dsmBlock_t;	/* block number */
typedef ULONG dsmBlocksize_t; 	/* blocksize in bytes */
typedef ULONG dsmSize_t;	/* size in blocks */

typedef COUNT dsmIndex_t;			/* index id */
typedef COUNT dsmCursid_t;

/* TODO: The following cursor structure needsto be completed
 *       along with the completion of the cursor DSM API.
 */
typedef struct dsmCursor
{
#if 0
    struct qrmsg qrmsg;       /* save query msg - why is this here? */
#endif
    dsmBuffer_t	*pkey;        /* ptr to current key */
    dsmArea_t	 areaNumber;  /* cursor's area number */
    dsmBlock_t	 blockNumber; /* current block number */
    ULONG	 updateCtr;   /* ? */
    dsmIndex_t	 indexNumber; /* cursor index number */
    UCOUNT	 offset;      /* ? */
    UCOUNT	 flags;       /* ? */
    TEXT	 keyAreaSize; /* size of area allocated for key */
    TEXT	 substrSize;  /* used for BEGINS and USING w/ ABBREV */
} dsmCursor_t;

/*************************************************************/
/* Public typedefs for dsmseq.c                              */
/*************************************************************/

typedef COUNT   dsmSeqId_t;     /* Sequence ID number */

typedef LONG    dsmSeqVal_t;    /* Sequence generator value */

typedef struct dsmSeqParm
{
    DBKEY       seq_recid;      /* Not used */
    dsmSeqVal_t seq_initial;    /* Initial value */
    dsmSeqVal_t seq_min;        /* Minimum value */
    dsmSeqVal_t seq_max;        /* Maximum value */
    dsmSeqVal_t seq_increment;  /* Increment value */
    dsmSeqVal_t seq_num;        /* Assigned sequence number */
    GTBOOL       seq_cycle;    /* == 0 -> not allowed to cycle, != 0 -> allowed */
} dsmSeqParm_t;

/*************************************************************/
/* Public typedefs for dsmblob.c                             */
/*************************************************************/

typedef LONG dsmBlobOb_t;      /* Used to map to the appropriate area */

typedef DBKEY dsmBlobId_t;     /* Unique identifier of blob within blobObjNo */

typedef struct dsmBlobCx
{
    dsmLength_t blobOffset;     /* Position within blob of where data belongs */
    dsmMask_t   blobCntx1;      /* Lock and Get/Put/Upd context info */
    ULONG       blobCntx2;      /* */
} dsmBlobCx_t;

typedef struct dsmBlob
{
    dsmAreaType_t areaType; 	/* type of area (typically a constant) */
    dsmBlobOb_t blobObjNo;      /* Area map selector */
    dsmBlobId_t blobId;         /* Unique identifer within area */
    dsmLength_t segLength;      /* Length of this segment */
    dsmLength_t totLength;      /* Length of the whole blob */
    dsmLength_t bytesRead;      /* number of bytes read per call */
    dsmBlobCx_t blobContext;    /* Context information retained between calls */

    dsmBuffer_t *pBuffer;       /* Pointer to user buffer */
    dsmLength_t maxLength;      /* Size of user buffer */
} dsmBlob_t;

/* Following are types associated with transactions and savepoints. */

typedef LONG  dsmTrid_t;        /* transaction identifier */
typedef ULONG dsmTxnCode_t;	/* tx manager action code */

typedef ULONG dsmTxnSave_t; 		/* savepoint identifier */

typedef struct dsmKey
{
        dsmIndex_t      index;          /* key index id */
        COUNT           keycomps;       /* number of key components */
	dsmArea_t       area;           /* area the index is in     */
        dsmDbkey_t      root;           /* dbkey of root of the index   */	
        COUNT           keyLen;         /* Length of key string */
        dsmBoolean_t    unknown_comp;   /* key contains an unknown component */
        dsmBoolean_t    unique_index;   /* substring ok bit */
        dsmBoolean_t    word_index;     /* is this a word index */
        dsmBoolean_t    descending_key; /* substring ok bit */
        dsmBoolean_t    ksubstr;        /* substring ok bit */
        dsmText_t       keystr[1];      /* 1st byte of key in new key format */
} dsmKey_t;

/* allocate a key structure on the stack */
#define AUTOKEY(PFX,PAD) \
struct PFX \
{       dsmKey_t  akey; \
        TEXT      apad[PAD]; \
} PFX;

/* structure for dsmAdminUserStatus */
typedef struct dsmUserStatus
{
    psc_user_number_t  userNumber;  /* user number */
    dsmObject_t        objectNumber;
    dsmObjectType_t    objectType;
    ULONG              phase;        /* current phase of the utility */
    ULONG              counter;      /* counter representing status */
    ULONG              target;       /* end value of counter */
    dsmText_t          operation[16]; /* 16 bytes of description */
}  dsmUserStatus_t;

/* structure for dsmTableMove index array */
typedef struct dsmIndexList
{
    dsmIndex_t         indexNumber;
    dsmArea_t          indexAreaNew;
} dsmIndexList_t;

/*************************************************************/
/* Public Function Prototypes for dsmcon.c   */
/*************************************************************/
typedef  ULONG     dsmTagId_t;
typedef  LONG      dsmTagLong_t;
typedef  int (*dsmTagVoidFunction_t)();

#endif /* DSMDTYPE_H  */

