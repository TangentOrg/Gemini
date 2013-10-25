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

#ifndef SEDOUNDO_H
#define SEDOUNDO_H

/***************************************************************/
/*** sedoundo.h -- sequence number manager do/undo Interface ***/
/***************************************************************/

struct bkbuf;
struct dsmContext;
struct rlnote;

/* bi note do routines */

DSMVOID	seqDoAdd	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	seqDoAssign	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	seqDoDelete	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	seqDoIncrement	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	seqDoSetValue	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	seqDoUpdate	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);


/* bi note undo routines */

DSMVOID	seqUndo		(struct dsmContext *pcontext, struct rlnote *prlnote);

DSMVOID	seqUndoAdd	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	seqUndoAssign	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	seqUndoDelete	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	seqUndoSetValue	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

DSMVOID	seqUndoUpdate	(struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pbk, COUNT reclen, TEXT *precord);

#endif  /* #ifndef SEDOUNDO_H   */
