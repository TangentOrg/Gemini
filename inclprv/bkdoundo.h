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

#ifndef BKDOUNDO_H
#define BKDOUNDO_H


/**********************************************************************/
/* bkdoundo.h - Private do/undo function prototypes for BK subsystem. */
/**********************************************************************/

/*************************************************************/
/* Private do/undo  Function Prototypes                      */
/*************************************************************/

#include "bkpub.h"
#include "rlpub.h"

DSMVOID    bkAreaAddDo     (struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID    bkAreaRemoveDo  (struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkbotdo		(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkbotmdo	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkbotmundo	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkbotundo	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkdoxtn		(struct dsmContext *pcontext, RLNOTE *prlnote,
			 bkbuf_t *pmstr, COUNT dlen, TEXT *pdata);

DSMVOID	bk2ebdo		(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bk2ebundo	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bk2emdo		(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bk2emundo	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkExtentCreateDo (struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkExtentRemoveDo (struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkFormatBlockDo	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkFormatBlockUndo (struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkfrbl1		(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkfrbl2		(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkfrbulk	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkfrmlnk	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkfrmulk	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkHwmDo		(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pmstr, COUNT dlen, TEXT *pdata);

DSMVOID	bkMakeBlockDo	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkWriteAreaBlockDo
			(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID	bkWriteObjectBlockDo
			(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT dlen, TEXT *pdata);

DSMVOID
bkRowCountUpdateDo(dsmContext_t *pcontext, RLNOTE *pnote, BKBUF *pblk,
                   COUNT  dlen, TEXT *pdata);

/* PROGRAM - bkRowCountUpdateUndo */
DSMVOID
bkRowCountUpdateUndo(dsmContext_t *pcontext, RLNOTE *pnote, BKBUF *pblk,
                   COUNT  dlen, TEXT *pdata);


/*************************************************************/
/* Private do/undo  Function Prototypes for bkrepl.c        */
/*************************************************************/

DSMVOID	bkrpldo		(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT data_len, TEXT *pdata);

DSMVOID	bkrplundo	(struct dsmContext *pcontext, RLNOTE *pnote,
			 bkbuf_t *pblk, COUNT data_len, TEXT *pdata);  
#endif /* ifndef BKDOUNDO_H  */







