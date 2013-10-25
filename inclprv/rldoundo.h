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

#ifndef RLDOUNDO_H
#define RLDOUNDO_H

/**********************************************************************
*
*	C Header:		rldoundo.h
*	Subsystem:		1
*	Description:	
*	%created_by:	richt %
*	%date_created:	Mon Jan  9 17:53:10 1995 %
*
**********************************************************************/

/* Note definitions for memory state logged by rl subsystem */

#include "rlpub.h"

/* do/undo routine dispatch table entry  */
typedef struct rldisp
{
  char *rlname;
  DSMVOID (*do_it)(struct dsmContext *pcontext,RLNOTE *, BKBUF *, COUNT, TEXT *);
  DSMVOID (*undo_it)(struct dsmContext *pcontext,RLNOTE *,BKBUF *, COUNT, TEXT *);
  DSMVOID (*disp_it)(struct dsmContext *pcontext,RLNOTE *,BKBUF *, COUNT, TEXT *);
  ULONG noteCounter;
} rldisp_t;

/* Action routine dispatching macros */
#define RLDO( p, n, b, l, d )        \
        ((*prldisp[(n)->rlcode].do_it)   ( p, n, b, l, d ))
#define RLUNDO( p, n, b, l, d )      \
        ((*prldisp[(n)->rlcode].undo_it) ( p, n, b, l, d ))
#define RLDISPLAY( p, n, b, l, d )      \
        ((*prldisp[(n)->rlcode].disp_it) ( p, n, b, l, d ))

/* Function prototypes for all the action routines */

#include "rmdoundo.h"
#include "bkdoundo.h"
#include "cxdoundo.h"
#include "sedoundo.h"
#include "tmdoundo.h"

struct dsmContext;

DSMVOID rlVerifyTxTable(RL_DO_UNDO_PROTO, int checkRlCounters );

DSMVOID
dbTableAutoIncrementDo(RL_DO_UNDO_PROTO);
DSMVOID
dbTableAutoIncrementUndo(RL_DO_UNDO_PROTO);
DSMVOID
dbTableAutoIncrementSetDo(RL_DO_UNDO_PROTO);


DSMVOID rlmemchk(RL_DO_UNDO_PROTO);
DSMVOID rlmemrd(RL_DO_UNDO_PROTO);
DSMVOID rlaiextrd (RL_DO_UNDO_PROTO);
DSMVOID rlrctrd(RL_DO_UNDO_PROTO);
DSMVOID rlmemwrt(RL_DO_UNDO_PROTO);
DSMVOID rlnull(RL_DO_UNDO_PROTO);
DSMVOID rltlOnDo(RL_DO_UNDO_PROTO);

#endif /* RLDOUNDO_H  */
