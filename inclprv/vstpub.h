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

#ifndef VSTPRV_H
#define VSTPRV_H

struct dsmContext;

#include "scasa.h"   /* TODO: Remove once scasa can be included in rm.c
                      * - we moved the macros from here to there!
                      */

#define FLDBUFSIZE 64   /* default field buffer size */

typedef union {
    TEXT *textField;
    LONG  longField;
} fieldValue_t;

/* vst housekeeping and function dispatch */
LONG  vstFillNextField	(TEXT *pRecord, int fieldType,
			 fieldValue_t fieldValue,
                         ULONG *pRecSize,
			 ULONG maxSize, TEXT **ppField);
ULONG
vstFillNextArrayField 	(TEXT *pRecord, int fieldType,
			 fieldValue_t fieldValue,
			 ULONG arrayIndex, ULONG recSize, ULONG maxSize,
			 TEXT *pArrayField);

LONG  vstFetch		(struct dsmContext *pcontext, LONG tableNumber,
			 DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstGetLastKey	(struct dsmContext *pcontext, LONG tableNumber);
DBKEY vstCreate		(struct dsmContext *pcontext, LONG tableNumber,
			 TEXT *pRecord, ULONG size);
LONG  vstDelete		(struct dsmContext *pcontext, LONG tableNumber,
			 DBKEY dbkey);
LONG  vstUpdate		(struct dsmContext *pcontext, LONG tableNumber,
			 DBKEY dbkey, TEXT *pRecord, ULONG size);


/* vst connect table operations */
DBKEY vstConnectCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstConnectDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstConnectFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstConnectGetLast(struct dsmContext *pcontext);
LONG  vstConnectUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst master block table operations */
DBKEY vstMasterBlockCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstMasterBlockDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstMasterBlockFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstMasterBlockGetLast(struct dsmContext *pcontext);
LONG  vstMasterBlockUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database Status table operations */
DBKEY vstDbStatusCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstDbStatusDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstDbStatusFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstDbStatusGetLast(struct dsmContext *pcontext);
LONG  vstDbStatusUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database Buffer table operations */
DBKEY vstBfStatusCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstBfStatusDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstBfStatusFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstBfStatusGetLast(struct dsmContext *pcontext);
LONG  vstBfStatusUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database Server table operations */
DBKEY vstLoggingCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstLoggingDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstLoggingFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstLoggingGetLast(struct dsmContext *pcontext);
LONG  vstLoggingUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database Server table operations */
DBKEY vstServerCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstServerDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstServerFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstServerGetLast(struct dsmContext *pcontext);
LONG  vstServerUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database Server table operations */
DBKEY vstStartupCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstStartupDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstStartupFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstStartupGetLast(struct dsmContext *pcontext);
LONG  vstStartupUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database Segment table operations */
DBKEY vstSegmentCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstSegmentDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstSegmentFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstSegmentGetLast(struct dsmContext *pcontext);
LONG  vstSegmentUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database Filelist table operations */
DBKEY vstFileListCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstFileListDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstFileListFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstFileListGetLast(struct dsmContext *pcontext);
LONG  vstFileListUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database User Lock table operations */
DBKEY vstUserLockCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstUserLockDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstUserLockFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstUserLockGetLast(struct dsmContext *pcontext);
LONG  vstUserLockUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database Lock table operations */
DBKEY vstLockCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstLockDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstLockFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstLockGetLast(struct dsmContext *pcontext);
LONG  vstLockUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database Transaction table operations */
DBKEY vstTransactionCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstTransactionDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstTransactionFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstTransactionGetLast(struct dsmContext *pcontext);
LONG  vstTransactionUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/********* ACTIVITY *******************/
LONG  vstActSummaryFetch     (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActSummaryGetLast   (struct dsmContext *pcontext);
DBKEY vstActSummaryCreate    (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActSummaryDelete    (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActSummaryUpdate    (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActServerFetch      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActServerGetLast    (struct dsmContext *pcontext);
DBKEY vstActServerCreate     (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActServerDelete     (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActServerUpdate     (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActBufferFetch      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActBufferGetLast    (struct dsmContext *pcontext);
DBKEY vstActBufferCreate     (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActBufferDelete     (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActBufferUpdate     (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActPageWriterFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActPageWriterGetLast(struct dsmContext *pcontext);
DBKEY vstActPageWriterCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActPageWriterDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActPageWriterUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActBiLogFetch       (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActBiLogGetLast     (struct dsmContext *pcontext);
DBKEY vstActBiLogCreate      (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActBiLogDelete      (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActBiLogUpdate      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActAiLogFetch       (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActAiLogGetLast     (struct dsmContext *pcontext);
DBKEY vstActAiLogCreate      (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActAiLogDelete      (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActAiLogUpdate      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActLockFetch        (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActLockGetLast      (struct dsmContext *pcontext);
DBKEY vstActLockCreate       (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActLockDelete       (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActLockUpdate       (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);


LONG  vstBlockFetch          (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstBlockGetLast        (struct dsmContext *pcontext);
DBKEY vstBlockCreate         (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstBlockDelete         (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstBlockUpdate         (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActSpaceFetch       (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActSpaceGetLast     (struct dsmContext *pcontext);
DBKEY vstActSpaceCreate      (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActSpaceDelete      (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActSpaceUpdate      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActIndexFetch       (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActIndexGetLast     (struct dsmContext *pcontext);
DBKEY vstActIndexCreate      (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActIndexDelete      (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActIndexUpdate      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActRecordFetch      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActRecordGetLast    (struct dsmContext *pcontext);
DBKEY vstActRecordCreate     (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActRecordDelete     (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActRecordUpdate     (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActOtherFetch       (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActOtherGetLast     (struct dsmContext *pcontext);
DBKEY vstActOtherCreate      (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActOtherDelete      (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActOtherUpdate      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstUserIOFetch         (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstUserIOGetLast       (struct dsmContext *pcontext);
DBKEY vstUserIOCreate        (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstUserIODelete        (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstUserIOUpdate        (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstUserLockReqFetch    (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstUserLockReqGetLast  (struct dsmContext *pcontext);
DBKEY vstUserLockReqCreate   (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstUserLockReqDelete   (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstUserLockReqUpdate   (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActCheckpointFetch  (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActCheckpointGetLast(struct dsmContext *pcontext);
DBKEY vstActCheckpointCreate (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActCheckpointDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActCheckpointUpdate (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActIOTypeFetch      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActIOTypeGetLast    (struct dsmContext *pcontext);
DBKEY vstActIOTypeCreate     (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActIOTypeDelete     (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActIOTypeUpdate     (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

LONG  vstActIOFileFetch      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstActIOFileGetLast    (struct dsmContext *pcontext);
DBKEY vstActIOFileCreate     (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstActIOFileDelete     (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstActIOFileUpdate     (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst Database License table operations */
LONG  vstLicenseFetch        (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstLicenseGetLast      (struct dsmContext *pcontext);
DBKEY vstLicenseCreate       (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstLicenseDelete       (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstLicenseUpdate       (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst table statistics operations */
LONG  vstTableStatFetch      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstTableStatGetLast    (struct dsmContext *pcontext);
DBKEY vstTableStatCreate     (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstTableStatDelete     (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstTableStatUpdate     (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
 
/* vst index statistics operations */
LONG  vstIndexStatFetch      (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstIndexStatGetLast    (struct dsmContext *pcontext);
DBKEY vstIndexStatCreate     (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstIndexStatDelete     (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstIndexStatUpdate     (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst latch statistics operations */
LONG  vstLatchFetch          (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstLatchGetLast        (struct dsmContext *pcontext);
DBKEY vstLatchCreate         (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstLatchDelete         (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstLatchUpdate         (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
 
/* vst resrc queue statistics operations */
LONG  vstResrcFetch          (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstResrcGetLast        (struct dsmContext *pcontext);
DBKEY vstResrcCreate         (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstResrcDelete         (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstResrcUpdate         (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
 
/* vst txe statistics operations */
LONG  vstTxeFetch            (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);
DBKEY vstTxeGetLast          (struct dsmContext *pcontext);
DBKEY vstTxeCreate           (struct dsmContext *pcontext, TEXT *pRecord, ULONG size);
LONG  vstTxeDelete           (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstTxeUpdate           (struct dsmContext *pcontext, DBKEY dbkey, TEXT *pRecord, ULONG size);

/* vst statistics arrays base access/modify  operations */
DBKEY vstStatBaseCreate (struct dsmContext *pcontext,
                         TEXT *pRecord, ULONG size);
LONG  vstStatBaseDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstStatBaseFetch  (struct dsmContext *pcontext, DBKEY dbkey,
                         TEXT *pRecord, ULONG size);
DBKEY vstStatBaseGetLast(struct dsmContext *pcontext );
LONG  vstStatBaseUpdate (struct dsmContext *pcontext, DBKEY dbkey,
                         TEXT *pRecord, ULONG size);
 
/* vst User Status table operations */
DBKEY vstUserStatusCreate (struct dsmContext *pcontext,
                           TEXT *pRecord, ULONG size);
LONG  vstUserStatusDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstUserStatusFetch  (struct dsmContext *pcontext, DBKEY dbkey,
                           TEXT *pRecord, ULONG size);
DBKEY vstUserStatusGetLast(struct dsmContext *pcontext);
LONG  vstUserStatusUpdate (struct dsmContext *pcontext, DBKEY dbkey,
                           TEXT *pRecord, ULONG size);
 
/* vst User connection table operations */
DBKEY vstMyConnectionCreate (struct dsmContext *pcontext,
                           TEXT *pRecord, ULONG size);
LONG  vstMyConnectionDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstMyConnectionFetch  (struct dsmContext *pcontext, DBKEY dbkey,
                           TEXT *pRecord, ULONG size);
DBKEY vstMyConnectionGetLast(struct dsmContext *pcontext);
LONG  vstMyConnectionUpdate (struct dsmContext *pcontext, DBKEY dbkey,
                           TEXT *pRecord, ULONG size);

/* vst Area statistics table operations */
DBKEY vstAreaStatCreate (struct dsmContext *pcontext,
                           TEXT *pRecord, ULONG size);
LONG  vstAreaStatDelete (struct dsmContext *pcontext, DBKEY dbkey);
LONG  vstAreaStatFetch  (struct dsmContext *pcontext, DBKEY dbkey,
                           TEXT *pRecord, ULONG size);
DBKEY vstAreaStatGetLast(struct dsmContext *pcontext);
LONG  vstAreaStatUpdate (struct dsmContext *pcontext, DBKEY dbkey,
                           TEXT *pRecord, ULONG size);
 
#endif  /* ifndef VSTPRV_H */
