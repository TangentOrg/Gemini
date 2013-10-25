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

#ifndef BKPRVFR_H
#define BKPRVFR_H

/*****************************************************************/
/**								**/
/**	bkprvfr.h - structures and routine descriptions for	**/
/**	database block chain handling routines.			**/
/**								**/
/**	There are three quite distinct types of FIFO chains	**/
/**	managed by bkfradd and bkfrrem. One is the free		**/
/**	chain, denoted by FREECHN, which is a list of all	**/
/**	the unused disk blocks in a PROGRESS database. This	**/
/**	chain is only concerned with space management.		**/
/**	Any block on the free chain has a bk_type value of	**/
/**	FREEBLK.						**/
/**								**/
/**	The second type of chain is the record manager free	**/
/**	chain. This is a list of disk blocks which have		**/
/**	available slots in the record directory header and	**/
/**	potentially enough space to begin a record. This	**/
/**	chain is maintained by rm as an optimization. Any	**/
/**	block on this chain has a bk_type field of RMBLK	**/
/**	and a bk_frchn value of RMCHN. If the rm chain does	**/
/**	not contain a suitable block for beginning a record,	**/
/**	then a block is moved from the free chain to the rm	**/
/**	chain. If space is left for another record on the	**/
/**	same block, it is left on the rm chain. Otherwise,	**/
/**	it is removed and has a bk_frchn value of NOCHN.	**/
/**	When a record is deleted from a block, the block is	**/
/**	moved to or placed on the free chain if there are	**/
/**	no other records in the block, else it is placed	**/
/**	on the rm chain if there is reasonable space left in	**/
/**	the block.						**/
/**								**/
/**	The third type of chain is the index lock chain.	**/
/**	There is a separate index lock chain for each		**/
/**	database task.						**/
/**		more needed on index lock chains		**/
/**								**/
/**	There are two routines involved in managing these	**/
/**	chains. Bkfrrem is used to move a block from one	**/
/**	chain to another. For example, rm uses bkfrrem to	**/
/**	move a block from the free chain to the rm chain	**/
/**	when more rm space is needed.				**/
/**								**/
/**	Bkfradd is used to place a non-chained block onto	**/
/**	one of the three chains. For example, if rm deletes	**/
/**	all of the records from a block, and the block is	**/
/**	not on the rm chain, bkfradd is used to place the	**/
/**	block on the free chain. If the block were on the	**/
/**	the rm chain, then rm would use bkfrrem to move the	**/
/**	block back to the free chain.				**/
/**								**/
/**	If bkfrrem is called to obtain a block from the free	**/
/**	chain and the free chain is empty, bkfrrem calls	**/
/**	bkxtn to extend the database. If this extend fails,	**/
/**	bkxtn FATAL's. Thus, for now anyway, bkfrrem callers	**/
/**	looking for free vblocks do not need to handle error	**/
/**	returns. I don't like this approach, especially given	**/
/**	the fact that bkxtn always FATAL's for the demo		**/
/**	version of PROGRESS.					**/
/**								**/
/**	For a record manager chain request or a lock chain	**/
/**	request, however, a NULL is returned if the requested	**/
/**	chain is empty.						**/
/**								**/
/*****************************************************************/

#ifdef BKBUMPRV_H

/* rl note structures for fradd and frrem */
#ifndef RLPUB_H
#include "rlpub.h"
#endif

struct bkfrmst
  {
    RLNOTE   rlnote;
    TEXT     chain;
    TEXT     unused;
    UCOUNT   lockidx;
    DBKEY    newfirst;
    DBKEY    oldfirst;
  };
#define BKFRMST struct bkfrmst

#define INIT_S_BKFRMST_FILLERS( bk ) (bk).unused = 0;


struct bkfrmbot
  {
    RLNOTE   rlnote;
    DBKEY    newlast;
    DBKEY    oldlast;
    TEXT     chain;
    TEXT     filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
    COUNT    filler2;	/* bug 20000114-034 by dmeyer in v9.1b */
  };
#define BKFRMBOT struct bkfrmbot

#define INIT_S_BKFRMBOT_FILLERS( bk ) (bk).filler1 = 0; (bk).filler2 = 0;



struct bkfrblk
  {
    RLNOTE   rlnote;
    DBKEY    next;
    TEXT     flag;
    TEXT     newtype;
    TEXT     oldtype;
    TEXT     filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
  };
#define BKFRBLK struct bkfrblk

#define INIT_S_BKFRBLK_FILLERS( bk ) (bk).filler1 = 0;


struct bkfrbbot
  {
    RLNOTE   rlnote;
    TEXT     oldtype;
    TEXT     filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
    COUNT    filler2;	/* bug 20000114-034 by dmeyer in v9.1b */
  };
#define BKFRBBOT struct bkfrbbot

#define INIT_S_BKFRBBOT_FILLERS( bk ) (bk).filler1 = 0; (bk).filler2 = 0;


/* top RMCHN block to end of the chain */
struct bk2endmst
  {
    RLNOTE   rlnote;
    DBKEY    top; 	/* top block before change */
    DBKEY    second; 	/* second from top before change */
    DBKEY    bot;	/* bottom block before the change */
  };
#define BK2ENDMST struct bk2endmst


struct bk2endblk
  {
    RLNOTE   rlnote;
    DBKEY    oldnext;
    DBKEY    newnext;
  };
#define BK2ENDBLK struct bk2endblk

#else
    "bkprvfr.h is private to the bksubsystem."
#endif  /* #ifndef BKBUMPRV_H */
#endif  /* #ifndef BKPRVFR_H */

