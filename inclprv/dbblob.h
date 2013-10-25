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

#ifndef DBBLOB_H
#define DBBLOB_H

/* dsmblob private #define's & typedef's */


/* These two values (0xea and 0xeb) are reserved in enterprise bffldc.h */
#define DSMBLOBSEG      0xeb    /* Segment table record */
#define DSMBLOBDATA     0xea    /* Blob data record */

#define DSMBLOBDONTCARE 0xFF    /* Used for dsmBlobFet --> don't check indicator */

typedef struct dbBlobSegHdr
{
  TEXT         segid;
  COUNT        nument;
  DBKEY        dbkey;
  LONG         length;
} dbBlobSegHdr_t;

typedef struct dbBlobSegEnt
{
  COUNT        seglength;
  DBKEY        segdbkey;
} dbBlobSegEnt_t;

#define SEGHDR_LEN (sizeof(dbBlobSegHdr_t))
#define SEGENT_LEN (sizeof(dbBlobSegEnt_t))

#define MAXSEGENT       530

#ifdef B_DEBUG
#undef DSMBLOBMAXLEN
#define DSMBLOBMAXLEN   1024L

#undef MAXSEGENT
#define MAXSEGENT       20

#endif /* B_DEBUG */

#define BLOBMAXSEGTAB   (SEGHDR_LEN + SEGENT_LEN * MAXSEGENT)




dsmStatus_t dbBlobprmchk( dsmContext_t *pcontext, dsmBlob_t *pBlob,  
                                  dsmMask_t op, xDbkey_t *pDbkey); 

dsmStatus_t dbBlobFetch( dsmContext_t *pcontext, xDbkey_t *pDbkey, 
                                 dsmBuffer_t *pRecord, LONG maxLen,      
                                 LONG indicator, dsmText_t *pName);

dsmStatus_t dbBlobUpdSeg( dsmContext_t *pcontext, xDbkey_t *pDbkey,     
                                  dsmDbkey_t nextSeg, dsmLength_t length,   
                                  dsmBuffer_t *pSegTab, dsmText_t *pName);

dsmStatus_t dbBlobPutSeg( dsmContext_t *pcontext, xDbkey_t *pDbkey,
                                  dsmBlob_t *pBlob, dsmLength_t segLen,
                                  LONG nent, dsmBuffer_t *pSegTab,  
                                  dsmText_t *pName);
dsmStatus_t dbBlobDelSeg( dsmContext_t *pcontext, dsmBlob_t *pBlob,
                          xDbkey_t *pDbkey, LONG ent, 
                          dsmBuffer_t *pSegTab, dsmText_t *pName);

dsmStatus_t dbBlobTrunc( dsmContext_t *pcontext, dsmBlob_t *pBlob,
                                 xDbkey_t *pDbkey, dsmBuffer_t *pSegTab,
                                 dsmLength_t newLength);

dsmStatus_t dbBlobEndSeg( dsmContext_t *pcontext, dsmBlob_t *pBlob);

dsmStatus_t dbBlobGet( dsmContext_t *pcontext, dsmBlob_t *pBlob,
                              dsmText_t *pName);
dsmStatus_t dbBlobPut( dsmContext_t *pcontext, dsmBlob_t *pBlob,
                              dsmText_t *pName);
dsmStatus_t dbBlobUpdate( dsmContext_t *pcontext, dsmBlob_t *pBlob,
                                 dsmText_t *pName);
dsmStatus_t dbBlobDelete( dsmContext_t *pcontext, dsmBlob_t *pBlob,
                                 dsmText_t *pName);
dsmStatus_t dbBlobStart( dsmContext_t *pcontext, dsmBlob_t *pBlob);
dsmStatus_t dbBlobEnd( dsmContext_t *pcontext, dsmBlob_t *pBlob);
dsmStatus_t dbBlobUnlock( dsmContext_t *pcontext, dsmBlob_t *pBlob);


#endif /* #ifndef DBBLOB_H */
