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

#ifndef RMDOUNDO_H
#define RMDOUNDO_H

#include "rmprv.h"
/**********************************************************************/
/* rmdoundo.h - Private do/undo function prototypes for RM subsystem. */
/**********************************************************************/

struct bkbuf;
struct rlnote;

DSMVOID	rmDoChange	(struct dsmContext *pcontext, struct rlnote *pchgnote,
			 struct bkbuf *pbk, COUNT difLen, TEXT *pDifL);

DSMVOID	rmDoInsert	(struct dsmContext *pcontext, struct rlnote *prmrecnt,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	rmDoNextFrag	(struct dsmContext *pcontext, struct rlnote *prmnxtfnt,
			 struct bkbuf *pbk, COUNT len, TEXT  *pdata);

DSMVOID	rmDoDelete	(struct dsmContext *pcontext, struct rlnote *prmrecnt,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	rmUndoChange	(struct dsmContext *pcontext, struct rlnote *pchgnote,
			 struct bkbuf *pbk, COUNT difLen, TEXT *pDifL);

DSMVOID	rmUndoNextFrag	(struct dsmContext *pcontext, struct rlnote *prmnxtfnt,
			 struct bkbuf *pbk, COUNT len, TEXT *pdata);

/****/

DSMVOID    rmBeginLogicalUndo (struct dsmContext *pcontext);

DSMVOID    rmEndLogicalUndo   (struct dsmContext *pcontext);

DSMVOID    rmUndoLogicalCreate
			(struct dsmContext *pcontext, rmRecNote_t *prmrecnt,
			 COUNT reclen, TEXT *precord);

DSMVOID    rmUndoLogicalDelete
			(struct dsmContext *pcontext, rmRecNote_t *prmrecnt,
			 COUNT reclen, TEXT *precord);

DSMVOID    rmUndoLogicalChange
			 (struct dsmContext *pcontext, rmChangeNote_t *pchgnote);

#endif /* #ifndef RMDOUNDO_H  */
