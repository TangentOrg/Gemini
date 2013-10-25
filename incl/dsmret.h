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

#ifndef DSMRET_H
#define DSMRET_H

/*************************************************************/
/* This include file should contain all error codes          */
/* returned from the dsm api.  All new status codes should   */
/* be added by following the dsm status code convention.     */
/*************************************************************/
/* STATUS CODE CONVENTION                                    */
/* The range -20000 to -30000 is reserved for DSM error      */
/*  returns.                                                 */
/* Status codes break down as follows:                       */
/*    DSM_S_STATUSNAME          -NNNNNN                      */                 
/*************************************************************/

/*************************************************************/
/* Public status codes for dsmblob.c                         */
/*************************************************************/

#define DSM_S_BLOBOK        000000L        /* All is ok */
#define DSM_S_BLOBNOMORE   -100001L        /* No more content to the blob */
#define DSM_S_BLOBBADCNTX  -100002L        /* The dsmBlobCx_t is trash */
#define DSM_S_BLOBLIMIT    -100003L        /* v1 implementation limit */
#define DSM_S_BLOBTOOBIG   -100004L        /* Blob won't fit in supplied buffer */
#define DSM_S_BLOBNOMEMORY -100005L        /* utmalloc failed */
#define DSM_S_BLOBDNE      -100006L        /* Blob Does Not Exist */
#define DSM_S_BLOBBAD      -100007L        /* Blob is corrupt */

/*************************************************************/
/* Public status codes for dsmseq.c                          */
/*************************************************************/

#define DSM_S_SEQBAD       -100008L        /* _Sequence is corrupt */
#define DSM_S_SEQCYC       -100009L        /* Sequence can't cycle */
#define DSM_S_SEQNOTFND    -100010L        /* Sequence does not exist */
#define DSM_S_SEQFULL      -100011L        /* Sequence table is full */
#define DSM_S_SEQDUP       -100012L        /* Sequence number already exists */
#define DSM_S_SEQRANGE     -100013L        /* Sequence value out of range */

#define DSM_S_SUCCESS 0
#define DSM_S_FAILURE -1

/* Error Return Codes used in CX layer.   */
#define DSM_S_NOCURSOR -2
#define DSM_S_KEYNOTFOUND -3
#define DSM_S_NOTFOUND -3
#define DSM_S_BADLOCK -4
#define DSM_S_DUPLICATE -5

/* Common 4GL error codes */

#define DSM_S_IXDLOCK       -1210   /* ret code from ixgetmt, ixgenf, ixdelf */
#define DSM_S_TSKGONE       -1211   /* ret code from tskchain */
#define DSM_S_RQSTQUED      -1216   /* ret code from lk routines            */
#define DSM_S_RQSTREJ       -1217   /* ret code from lk, ixfnd(LKNOWAIT),etc*/
#define DSM_S_CTRLC         -1218   /* db wait interrupted by ctrl-c        */

/* codes from CXpub.h */
#define DSM_S_IXDUPKEY         1  /* from ixgen if key already exists     */
#define DSM_S_IXINCKEY        -2  /* from ixgen or ixfnd if incomplete    */
                                  /* key was passed                       */

/* codes from dbpub.h */
#define DSM_S_FNDAMBIG        -1213 /* ixfnd: find found more than 1 match    */
#define DSM_S_FNDFAIL         -1214 /* ixfnd: not found                       */
#define DSM_S_ENDLOOP         -1215 /* ret code from dbnxt at end of loop     */
 
#define DSM_S_AHEADORNG -1220 /* ret code from dbnxt if ahead of range  */
#define DSM_S_LKTBFULL  -1221 /* ret code from lklock                   */
#define DSM_S_NOSTG     -1222 /* server has insufficient storage (om.c) */
#define DSM_S_RECTOOBIG -1223 /* attempt to write big rec (rm.c) */
#define DSM_S_CURSORERR -1224 /* no more available cursors , increase -c */
#define DSM_S_FDDUPKEY  -1235   /* record with that key already exists   */ 
#define DSM_S_FDSEQFULL -1241   /* max no of sequences exceeded */
#define DSM_S_FDSEQCYC  -1242   /* sequence cycled but cycle ok false */

/* Codes from rmpub.h */
#define DSM_S_RMNOTFND -1     /* dbkey does not exist */
#define DSM_S_RMEXISTS -2     /* dbkey already exists */

/* New DSM status codes */
#define DSM_S_RECORD_BUFFER_TOO_SMALL -20000 /* buffer supplied to dsmRecordGet
  					        is not large enough to hold
					        the record.           */
#define DSM_S_DSMOBJECT_CREATE_FAILED -20001 /* dsmObjectCreate() failed */
#define DSM_S_DSMOBJECT_DELETE_FAILED -20002 /* dsmObjectDelete() failed */
#define DSM_S_DSMOBJECT_LOCK_FAILED   -20003 /* dsmObjectLock() failed */
#define DSM_S_DSMOBJECT_UNLOCK_FAILED -20004 /* dsmObjectUnlock() failed */
#define DSM_S_AREA_NOT_INIT           -20005 /* bkAreaDescPtr == NULL */
#define DSM_S_AREA_NULL               -20006 /* Passed Area Not Found */
#define DSM_S_AREA_NO_EXTENT          -20007 /* No extents in area    */
#define DSM_S_AREA_NUMBER_TOO_LARGE   -20008 /* Not that many area slots in db*/
#define DSM_S_AREA_ALREADY_ALLOCATED  -20009
#define DSM_S_INVALID_EXTENT_CREATE   -20010 /* Attempt to create something
                                               other than the next extent. */
#define DSM_S_NO_SUCH_OBJECT          -20011 /* omFetch called for an object
                                               that does not exist in the
                                               database.  */
#define DSM_S_DSMLOOKUP_REGOPENFAILED -20012  /* failed to open registry */
#define DSM_S_DSMLOOKUP_QUERYFAILED   -20013  /* query on key failed     */
#define DSM_S_DSMLOOKUP_KEYINVALID    -20014  /* invalid key requested   */
#define DSM_S_DSMLOOKUP_INVALIDSIZE   -20015  /* invalid size for key    */
 
#define DSM_S_DSMCONTEXT_ALLOCATION_FAILED -20016 /* strent of dsmContext
                                                     failed */
#define  DSM_S_DSMBADCONTEXT          -20017  /* Bad context passed to
                                                 dsmContext Create        */
#define DSM_S_NO_USER_CONTEXT         -20018  /* dsmContextSet called for
                                                 setting a user level
                                                 parameter but there is no
                                                 user context.             */
#define DSM_S_INVALID_TAG             -20019  /* Invalid tag id passed to
                                                 dsmSetContext             */
#define DSM_S_INVALID_TAG_VALUE       -20020  /* Invalid tag value passed to
                                                 dsmSetContext             */
#define DSM_S_USER_STILL_CONNECTED     -20021 /* dsmContextDelete called
                                                 with non-null user control */
#define DSM_S_DB_ALREADY_CONNECTED     -20022
 
#define DSM_S_INVALID_CONTEXT_COPY_MODE -20023 /* dsmContextCopy invalid
                                                      copyMode mask         */

#define DSM_S_BADCONTEXT              -20024 /* dsmContextCreate() failed */
#define DSM_S_NOWAIT                  -20025 /* dsmRecordCreate() failed */
#define DSM_S_BADSAVE                 -20026 /* dsmTransaction() failed */
#define DSM_S_TRANSACTION_ALREADY_STARTED -20027 /* transaction already
                                                    started. */
#define DSM_S_NO_TRANSACTION          -20028 /* a transaction operation 
                                             request was issued against 
                                             prior to a start request. */
#define DSM_S_INVALID_TRANSACTION_CODE -20029 /*  invalid transaction code. */

#define DSM_S_DSMOBJECT_INVALID_AREA -20030 /* Invalid area specified to
                                             * dsmObjecrCreate() */
#define DSM_S_INVALID_USER           -20031 /* invalid userctl encountered
                                             * by dsmThreadSafeEntry */
#define DSM_S_NO_MSG_CALLBACKS       -20032 /* Attempt connect to database
                                               without establishing the msgn
                                               callback.                    */
#define DSM_S_ERROR_EXIT             -20033 /* a dsm operation has encountered
                                             * a fatal error. */
#define DSM_S_SHUT_DOWN              -20034 /* a dsm operation has been rejected
                                             * because the database is marked
                                             * for shut down. */
#define DSM_S_USER_TO_DIE            -20035 /* a dsm operation has been rejected
                                             * because the user is marked
                                             * for disconnect. */
#define DSM_S_USER_BUSY              -20036 /* Unable to disconnect a user
                                             * because it is operating within
                                             * dsm. */
#define DSM_S_WATCHDOG_RUNNING       -20037 /* An attempt was made to run the
                                             * watchdog code when another 
                                             * watchdog was runnning. */
#define DSM_S_INVALID_LOCK_MODE      -20038 /* Invalid lock mode specified */
#define DSM_S_INVALID_UNLOCK_REQUEST -20039 /* unlock request made when no 
                                             * existing lock (schema) */
#define DSM_S_SCHEMA_CHANGED         -20040 /* schema has changed since 
                                             * last schema lock request. */
#define DSM_S_INVALID_LOCK_REQUEST   -20041 /* lock request made when caller 
                                             * already has schema lock of
                                             * requested strength (schema) */
#define DSM_S_INVALID_FIELD_VALUE    -20042  /* invalid field value */ 
#define DSM_S_AREA_HAS_EXTENTS       -20043 /* Attempt to delete area
                                             * which contains extents. */
#define DSM_S_INVALID_ROOTBLOCK      -20044
#define DSM_S_INVALID_BLOCKSIZE      -20045
#define DSM_S_INVALID_AREANUMBER     -20046
#define DSM_S_INVALID_AREATYPE       -20047
#define DSM_S_INVALID_AREARECBITS    -20048
#define DSM_S_RECOVERY_ENABLED       -20049

/* INDEX_COMPACT */
#define DSM_S_REACHED_BLOCK_COUNT    -20050   /* Status block count reached */
#define DSM_S_REACHED_BOTTOM_LEVEL   -20051   /* bottom level of idx reached */
#define DSM_S_USER_CTLC_TASK         -20052   /* user abort with ctlc */
#define DSM_S_INVALID_TABLE_NUMBER   -20053   /* invalid table number to move */
#define DSM_S_INVALID_AREA_NUMBER    -20054   /* invalid area to move */
#define DSM_S_INVALID_INDEX_NUMBER   -20055   /* invalid index number given */

#define DSM_S_MAX_INDICES_EXCEEDED   -20056   /* too many idx's defined */

#define DSM_S_CANT_RENAME_VST        -20057   /* attempted to rename vst */
#define DSM_S_DB_IN_USE              -20058   /* db already in use */
#define DSM_S_CANT_CHECK_BACKUP      -20059   /* can't read backup bit in
                                               * mstrblk */
#define DSM_S_PARTIAL_BACKUP         -20060   /* attempt to use incomplete
                                               * backup */

/***** RESERVED RANGE FOR SERVERS IS 20100 to -20199 *****/
#define DSM_S_MAX_SERVERS_EXCEEDED   -20100 /* attempting to connect failed 
                                             * due to max # of servers reached
                                             */
#define DSM_S_MAX_USERS_EXCEEDED     -20101 /* attempting to client server 
                                             * connection when maxclients
                                             * has been reached */
#define DSM_S_LAST_SERVER_RETURN_STATUS -20199

#define DSM_S_OUT_OF_AREA_SLOTS      -20200
#define DSM_S_INDEX_ANCHOR           -20201
#define DSM_S_TEMPOBJECT_LIMIT_EXCEEDED -20202

#endif /* DSMRET_H  */




