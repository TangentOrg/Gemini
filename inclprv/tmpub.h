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

#ifndef TMPUB_H
#define TMPUB_H 1

struct dsmContext;

dsmStatus_t     tmMarkSavePoint(struct dsmContext *pcontext, 
                                dsmTxnSave_t *psavepoint);
dsmStatus_t     tmMarkUnsavePoint(struct dsmContext *pcontext, 
                                  LONG noteBlock, LONG noteOffset);

DSMVOID tmGetTxInfo (struct dsmContext *pcontext, LONG *pTid,
                  LONG *pSlot, ULONG *pTtime);

int tmInitMask(int maxusr);

DSMVOID
tmLockChainSet(dsmContext_t *pcontext, ULONG64 lkmask);

int tmNeedRollback (dsmContext_t *pcontext, dsmTrid_t localTrid);

#endif  /* #ifndef TMPUB_H */
