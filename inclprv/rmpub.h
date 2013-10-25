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

#ifndef RMPUB_H
#define RMPUB_H
#include "dsmpub.h"
#include "shmpub.h"
#include "bmpub.h"

/***********************************************************/
/*** rmpub.h - record manager Public Function Prototypes ***/
/***********************************************************/

/************************************************************
*** rm is the record manager, which is the interface
***  between the buffer and block managers.
***
*** calls:
*** rmFetch  finds a record given its dbkey
*** rmCreate creates a new record and dbkey
*** rmDelete deletes the record of a given dbkey
*** rmUpdateRecord replaces a record with a new version
************************************************************/

#define RMOK     0      /* OK */
#define RMNOTFND -1     /* dbkey does not exist */
#define RMEXISTS -2     /* dbkey already exists */

/*************************************************/
/*** lock request types for rmCreate           ***/
/*************************************************/
#define RM_LOCK_NOT_NEEDED  0 /* record lock should not be applied */
#define RM_LOCK_NEEDED      1 /* record lock must be applied to first frag */

/*************************************************/
/*** Record manager Public Function Prototypes ***/
/*************************************************/

struct  bkbuf;
struct  dsmContext;
struct  rmBlock;

DBKEY   rmCreate	(struct dsmContext *pcontext, ULONG area,
			 LONG table, TEXT *prec, COUNT size, 
                         UCOUNT lockRequest);

int	rmDelete	(struct dsmContext *pcontext, LONG table,
			 DBKEY dbkey, TEXT *pname);

DSMVOID	rmFormat	(struct dsmContext *pcontext, struct bkbuf *pbk,
                         ULONG area);

int	rmFetchRecord	(struct dsmContext *pcontext, ULONG area, DBKEY dbkey, 
			 TEXT *pto, COUNT maxsize, int cont);

DSMVOID  rmFree          (struct dsmContext *pcontext, ULONG area,
                          bmBufHandle_t bufHandle, struct rmBlock *pbk,
                          COUNT *pdelRecSize);

int	rmUpdateRecord	(struct dsmContext *pcontext, dsmTable_t table,
                         ULONG area, DBKEY dbkey,
			 TEXT *prec, COUNT size);

#endif  /* #ifndef RMPUB_H */

