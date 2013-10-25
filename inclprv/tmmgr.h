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

#ifndef TMMGR_H
#define TMMGR_H

/*************/
/** tmmgr.h **/
/*************/

#include "dsmdtype.h"  /* typedefs for dsm api */

/* opaque pointer for tpctl */
struct tx;
struct dsmContext;

int     tmadddlcok      (struct dsmContext *pcontext, LONG localTrid);

int     tmremdlcok      (struct dsmContext *pcontext, LONG localTrid);

DSMVOID	tmchkdelay	(struct dsmContext *pcontext, int reset);

int	tmdtask		(struct dsmContext *pcontext, LONG localTrid);
DSMVOID 	tmdlg		(struct dsmContext *pcontext, int mode);

LONG	tmctask		(struct dsmContext *pcontext);

LONG	tmnum		(struct dsmContext *pcontext, ULONG txSlot);
ULONG	tmSlot		(struct dsmContext *pcontext, LONG trId);
struct tx *tmEntry		(struct dsmContext *pcontext, LONG trId);

DSMVOID	tmrej		(struct dsmContext *pcontext, int force,
                         dsmTxnSave_t *pSavePoint);
int     tmend           (struct dsmContext *pcontext, int accrej,
			 int *ptpctl, int service);
DSMVOID    tmdoflsh        (struct dsmContext *pcontext);

DSMVOID    tmstrt          (struct dsmContext *pcontext);

/* parm to tmend */
#define TMACC 65	/* commit the transaction (accept it) */
#define TMREJ 82	/* roll back the transaction (reject) */
#define PHASE1 22	/* Phase 1 of the commit, are you ready to commit? */
#define PHASE1q2 23	/* Query the result of Phase 1.  */


#define TP_COMMIT 1
#define TP_ABORT  2
 
#endif  /* #ifndef TMMGR_H */
