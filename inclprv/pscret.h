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

#ifndef PSCRET_H
#define PSCRET_H 1

/* general widely used return codes */
/* WARNING -- When making up new error codes, check for conflicts in */
/*  dbmgr.h  fdmgr.h  dstd.h 					     */
#define PRO_S_SUCCESS        0      /* zero means unqualified success    */

/* The following error codes should really have the PRO_S_ prefix but
   their use is too pervasive in the code to fix right now.          */

#define IXDLOCK         -1210   /* ret code from ixgetmt, ixgenf, ixdelf */
#define TSKGONE         -1211   /* ret code from tskchain */
#define RQSTQUED        -1216   /* ret code from lk routines            */
#define RQSTREJ         -1217   /* ret code from lk, ixfnd(LKNOWAIT),etc*/
#define CTRLC           -1218   /* db wait interrupted by ctrl-c        */

/* The convention for adding definitions for new status (error) 
   return values should be S_<meaningful name> */

/* The range -20000 to -30000 is reserved for DSM error returns.  */

/* Status control based on Areas     */
/*************************************/
#define PRO_S_AREA_NOT_INIT    -20005    /* bkAreaDescPtr == NULL */
#define PRO_S_AREA_NULL        -20006    /* Passed Area Not Found */
#define PRO_S_AREA_NO_EXTENT   -20007    /* No extents in area    */
#define PRO_S_AREA_NUMBER_TOO_LARGE  -20008 /* Not that many area slots in db*/
#define PRO_S_AREA_ALREADY_ALLOCATED -20009
#define PRO_S_INVALID_EXTENT_CREATE  -20010 /* Attempt to create something other
					       than the next extent.            */
#define PRO_S_NO_SUCH_OBJECT         -20011 /* omFetch called for an object
					       that does not exist in the
					       database.                        */

/* return status for dsmLookupEnv    */
/*************************************/
#define PRO_S_DSMLOOKUP_REGOPENFAILED -20012  /* failed to open registry */
#define PRO_S_DSMLOOKUP_QUERYFAILED   -20013  /* query on key failed     */
#define PRO_S_DSMLOOKUP_KEYINVALID    -20014  /* invalid key requested   */
#define PRO_S_DSMLOOKUP_INVALIDSIZE   -20015  /* invalid size for key    */

#define PRO_S_DSMCONTEXT_ALLOCATION_FAILED -20016 /* strent of dsmContext
                                                     failed */
#define  PRO_S_DSMBADCONTEXT          -20017  /* Bad context passed to
                                                 dsmContext Create        */
#define PRO_S_DSM_NO_USER_CONTEXT     -20018  /* dsmContextSet called for
                                                 setting a user level
                                                 parameter but there is no
                                                 user context.             */
#define PRO_S_DSM_INVALID_TAG         -20019  /* Invalid tag id passed to
                                                 dsmSetContext             */
#define PRO_S_DSM_INVALID_TAG_VALUE   -20020  /* Invalid tag value passed to
                                                 dsmSetContext             */
#define PRO_S_DSM_USER_STILL_CONNECTED -20021 /* dsmContextDelete called
                                                 with non-null user control */
#define PRO_S_DSM_DB_ALREADY_CONNECTED -20022

#define PRO_S_DSM_INVALID_CONTEXT_COPY_MODE -20023 /* dsmContextCopy invalid
                                                      copyMode mask         */

#endif /* PSCRET_H */






