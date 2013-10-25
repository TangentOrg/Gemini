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

#ifndef CXDOUNDO_H
#define CXDOUNDO_H

/******************************************************************************/
/* cxdoundo.h -Private do/undo functoin function prototypes for CX subsystem. */
/******************************************************************************/

struct bkbuf;
struct rlnote;

/*************************************************************/
/* Private do/undo  Function Prototypes for cxdo.c           */
/*************************************************************/
/* define newname oldname */
#define  cxDoDelete   cxdeldo 

DSMVOID     cxClearBlock   (struct dsmContext *pcontext, struct rlnote *pnote,
			  struct bkbuf *pixblk, COUNT data_len, TEXT *pdata);

DSMVOID     cxDoDelete     (struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pixblk, COUNT data_len, TEXT *pdata);

DSMVOID     cxDoInsert     (struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pixblk, COUNT data_len, TEXT *pdata);

DSMVOID     cxDoStartSplit (struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pixblk, COUNT data_len, TEXT *pdata);

DSMVOID     cxDoEndSplit   (struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pixblk, COUNT data_len,  TEXT *pdata);

DSMVOID     cxUndoStartSplit (struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pixblk, COUNT data_len,  TEXT *pdata);

DSMVOID     cxInvalid      (struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pixblk, COUNT dlen, TEXT *pdata);

DSMVOID     cxDoMasterInit (struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pixblk, COUNT dlen, TEXT *pdata);

DSMVOID     cxUndoClearBlock (struct dsmContext *pcontext, struct rlnote *pnote,
			 struct bkbuf *pixblk, COUNT data_len, TEXT *pdata);

DSMVOID     cxDoCompactLeft(struct dsmContext *pcontext, struct rlnote *pnote,
                         struct bkbuf *pixblk, COUNT data_len, TEXT *pdata);

DSMVOID     cxDoCompactRight(struct dsmContext *pcontext, struct rlnote *pnote,
                         struct bkbuf *pixblk, COUNT data_len, TEXT *pdata);

DSMVOID     cxUndoCompactLeft(struct dsmContext *pcontext, struct rlnote *pnote,
                         struct bkbuf *pixblk, COUNT data_len, TEXT *pdata);

DSMVOID     cxUndoCompactRight(struct dsmContext *pcontext, struct rlnote *pnote,
                         struct bkbuf *pixblk, COUNT data_len, TEXT *pdata);

#endif /* ifndef CXDOUNDO_H  */
