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

#ifndef RECPUB_H
#define RECPUB_H

#include "svcspub.h"

/* For use in C++ by SQL engine, define entire include file to have "C linkage" . */
/* This is technique used by all standard .h sources in /usr/include,...          */
#ifdef  __cplusplus
extern "C" {
#endif

/* macro for inline extraction of 2 byte integers from byte stream */
#define EXCOUNT(p)(((COUNT)((*(TEXT *)(p)))<<8)+((COUNT)(*((TEXT *)(p)+1))))

/*******************/
/* Limit Constants */
/*******************/
#define RECMAXRECORDSIZE 32000
 
/*******************************/
/* definitions of status codes */
/*******************************/
#define RECSUCCESS        0
#define RECTOOSMALL      -1
#define RECFLDNOTFOUND   -2
#define RECFLDTRUNCATED  -3
#define RECBADYEAR       -4
#define RECBADMON        -5
#define RECBADDAY        -6
#define RECBADMODE       -7
#define RECBADDAYSIZE    -8
#define RECINVALIDTYPE   -9
#define RECFAILURE      -16

/* skip table info */
#define SKIPUNIT 16  /* each entry in the skip table controlls 16 fields */
#define SKIPPFXLEN (1+sizeof(COUNT)) /* size of skip table prefix */
#define SKIPENTLEN sizeof(COUNT)     /* size of skip table entry */
 
/* other field info */
#define LONGPFXLEN (1+sizeof(COUNT)) /* size of long field prefix */
#define VECTPFXLEN (1+sizeof(COUNT)) /* size of vector field prefix */

/* first field info */
#define REC_FLD1EXT  8  /* extent of the first field */
#define REC_TBLVERS  1  /* 1st in extent is table schema version of record */
#define REC_TBLNUM   2  /* 2nd is table number */
#define REC_FLDCOUNT 3  /* 3rd is number of physical fields in record*/

/****************************************/
/* special codes for field prefix bytes */
/****************************************/
 
#define ENDREC   255    /* 0xff */
#define FREEPOS  254    /* 0xfe */
#define VUNKNOWN 253    /* 0xfd */
#define VHIGH    252    /* 0xfc */
#define VLOW     251    /* 0xfb */
#define VECTOR   250    /* 0xfa */
#define TODAY    249    /* 0xf9 */
#define BLOB_RES_1  235 /* 0xeb */
#define BLOB_RES_2  234 /* 0xea */
#define UNFETCHED_N 233 /* 0xe9 */      /* for field-list records */
#define UNFETCHED_1 232 /* 0xe8 */      /* for field-list records */
#define SKIPTBL  231    /* 0xe7 */
#define LONGFLD  230    /* 0xe6 */
#define PRO_MINLONG 230    /* 0xe6 */

/****  end special codes for field prefix bytes ****/

/* Data type structures */

typedef LONG recStatus_t;
typedef COUNT recTable_t;

typedef struct recDouble
{
    LONG highValue;
    LONG lowValue;
}  recDouble_t;

/***  rec.c - general record manipulation public function prototypes ***/

DSMVOID recBufMove		(TEXT *ps, TEXT *pe, int amt);

TEXT *recChrCopy	(TEXT *t, TEXT chr, int n);


recStatus_t DLLEXPORT recRecordNew(
                         recTable_t tableId, TEXT *pRecord, ULONG maxSize,
                         ULONG *precSize, ULONG numFields, ULONG schemavers);

recStatus_t DLLEXPORT recRecordInit(
                         recTable_t tableId, TEXT *pRecord, ULONG maxSize,
                         ULONG *precSize, ULONG numFields);

recStatus_t DLLEXPORT recAddColumns(
                         TEXT *pRecord, ULONG maxSize, ULONG *precSize,
                         ULONG numFields);

TEXT *recFieldFirst     (TEXT *pRecord);

TEXT *recFieldNext      (TEXT *pField);

TEXT *recFindField      (TEXT *pRecord, ULONG fieldNum);

TEXT *recFindArrayElement (TEXT *pField, ULONG ArrayIdx);

recStatus_t recSkipUpdate(TEXT *pRecord, ULONG maxSize, ULONG *precSize,
                         ULONG *pdelta);

ULONG recFieldLength    (TEXT *pField);

ULONG recFieldPrefix    (TEXT *pField);

ULONG recFieldSize      (TEXT *pField);

TEXT *recFieldShift     (TEXT *pRecord, ULONG recSize, TEXT *pField,
                         COUNT amount);

DSMVOID recSkipShift       (TEXT *pRecord, TEXT *pField, COUNT amount);

ULONG recUpdate         (TEXT *pRecord, ULONG recSize, ULONG maxSize,
                         TEXT *pField, TEXT *pValue);

ULONG recUpdateArray    (TEXT *pRecord, TEXT *pArray, ULONG recSize,
                         ULONG maxSize, TEXT *pField, TEXT *pValue);
COUNT recFieldCount     (TEXT   *pRecord);


/***  recformt.c - record formatting public function prototypes ***/

ULONG  DLLEXPORT recFormat(
               TEXT *pRecord, ULONG maxSize, ULONG nulls);

ULONG  DLLEXPORT recFormatARRAY (
                         TEXT *pRecord, ULONG recSize, ULONG maxSize,
                         TEXT *pField, ULONG numEntries);

ULONG  DLLEXPORT recGetArraySize( ULONG numEntries);


TEXT *recFormatTINY     (TEXT *pBuffer, ULONG bufSize, svcTiny_t value,
                         ULONG indicator);

TEXT *recFormatSMALL    (TEXT *pBuffer, ULONG bufSize, svcSmall_t value,
                         ULONG indicator);

TEXT *recFormatLONG     (TEXT *pBuffer, ULONG bufSize, svcLong_t value,
                         ULONG indicator);

TEXT *recFormatBIG      (TEXT *pBuffer, ULONG bufSize, svcBig_t bigValue,
                         ULONG indicator);

DSMVOID recFormatBYTES    (TEXT *pBuffer, ULONG bufSize, TEXT *pValue,
                         ULONG length, ULONG indicator);

TEXT *recFormatDECIMAL  (TEXT *pBuffer, ULONG bufSize, svcDecimal_t *pValue,
                         ULONG indicator);

TEXT *recFormatFLOAT    (TEXT *pBuffer, ULONG bufSize, svcFloat_t *pValue,
                         ULONG indicator);

TEXT *recFormatDOUBLE   (TEXT *pBuffer, ULONG bufSize, svcDouble_t *pValue,
                         ULONG indicator);

recStatus_t recFormatDATE
                        (TEXT *pBuffer, ULONG bufSize, svcDate_t *pValue,
                         ULONG indicator);

TEXT *recFormatTIME     (TEXT *pBuffer, ULONG bufSize, svcTime_t *pValue,
                         ULONG indicator);

recStatus_t recFormatTIMESTAMP
                        (TEXT *pBuffer, ULONG bufSize, svcTimeStamp_t *pValue,
                         ULONG indicator);


recStatus_t recExpandDate(svcDate_t *peDate, LONG storeDate);


/***  recget.c - general field retrieval public function prototypes */

COUNT DLLEXPORT recGetTableNum (
                 TEXT *ps);


recStatus_t DLLEXPORT recGetField  (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcDataType_t *psvcDataType);

recStatus_t DLLEXPORT recGetTINY   (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcTiny_t *pfieldValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetSMALL  (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcSmall_t *pfieldValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetLONG   (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcLong_t *pfieldValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetBIG    (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcBig_t *pbigValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetBYTES  (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcByteString_t *pfieldValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetDECIMAL(
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcDecimal_t *pfieldValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetFLOAT  (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcFloat_t *pfieldValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetDOUBLE (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcDouble_t *pfieldValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetDATE   (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcDate_t *pfieldValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetTIME   (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcTime_t *pfieldValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetTIMESTAMP (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcTimeStamp_t *pfieldValue, ULONG *pindicator);

recStatus_t DLLEXPORT recGetBOOL   (
                           TEXT *precordBuffer, ULONG fieldNum,ULONG arrayIndex,
                           svcBool_t *pfieldValue, ULONG *pindicator);


/***  recput.c - general field insertion public function prototypes **/


recStatus_t DLLEXPORT recPutField     (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG arrayIndex,
                         svcDataType_t *psvcDataType);

recStatus_t DLLEXPORT recPutTINY (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcTiny_t fieldValue, ULONG indicator);

recStatus_t DLLEXPORT recPutSMALL (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcSmall_t fieldValue, ULONG indicator);

recStatus_t DLLEXPORT recPutLONG  (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcLong_t fieldValue, ULONG indicator);

recStatus_t DLLEXPORT recPutBIG   (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcBig_t bigValue,
                         ULONG indicator);

recStatus_t DLLEXPORT recPutBYTES (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcByteString_t *pfieldValue, ULONG indicator);

recStatus_t DLLEXPORT recPutBOOL  (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcBool_t fieldValue, ULONG indicator);

recStatus_t DLLEXPORT recPutDECIMAL (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcDecimal_t *pfieldValue, ULONG indicator);

recStatus_t DLLEXPORT recPutFLOAT (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcFloat_t fieldValue, ULONG indicator);

recStatus_t DLLEXPORT recPutDOUBLE(
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcDouble_t fieldValue, ULONG indicator);

recStatus_t DLLEXPORT recPutDATE  (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcDate_t *pfieldValue, ULONG indicator);

recStatus_t DLLEXPORT recPutTIME  (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcTime_t *pfieldValue, ULONG indicator);

recStatus_t DLLEXPORT recPutTIMESTAMP (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG ArrayIndex,
                         svcTimeStamp_t *pfieldValue, ULONG indicator);

recStatus_t DLLEXPORT recInitArray  (
                         TEXT *precordBuffer, ULONG maxSize, ULONG *precSize,
                         ULONG fieldNumber, ULONG arrayExtent);
recStatus_t DLLEXPORT recSetTableNum(
                         TEXT *pRecord, ULONG maxSize, ULONG *precSize,
                       svcLong_t tableid);

/* For use in C++ by SQL engine, define entire include file to have "C linkage" . */
/* This is technique used by all standard .h sources in /usr/include,...          */
#ifdef  __cplusplus
}
#endif

#endif  /* ifndef RECPUB_H */

