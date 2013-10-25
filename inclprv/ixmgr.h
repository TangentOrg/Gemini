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

/***************************************************************/
/***  ixmgr.h   ---   external entrys for the index manager  ***/
/***************************************************************/

/***************************************************************
*** summary:
***	ix	is the index manager.  Its entries are:
***
***	ixfilc	create a new index.
***************************************************************/
/* REMOVED for BUM work */
#if !PSC_ANSI_COMPILER
#include "cxmgr.h"


/* number of cursors */
#define MINCURS		10
#define MAXCURS         999     /* max for non-shared memory only */
#define MAXCURSENT	20	/* absolute maximum value for cursor depth */
#define DFTCURSENT	8	/* default value for cursor depth */
#define DFTCURSS	20	/* default for single user */
#define DFTCURSM	60	/* default for multi  user */
#define DFTCURSPUSR     4       /* default # cursors per user */

#define IXDUPKEY	 1	/* from ixgen if key already exists	*/
#define IXINCKEY	-2	/* from ixgen or ixfnd if incomplete	*/
				/* key was passed			*/
/* mode parms for ixfnd */
#define FINDFRST	1	/* find the first match and return it	*/
#define FINDSHRT	4	/* not all comps supplied		*/
#define FINDGAP		8	/* find ent bfr 1st match,		*/
				/* ixfld uses FINDFRST+FINDGAP		*/

/* lockmode parm for ixdel */
#define IXNOLOCK	0
#define IXLOCK		1
#define	IXROLLF		4	/* logical IXDEL during roll forward.
				   Put a lock on the index entry but don't
				   delete it yet. */

DBKEY
ixmstr();

/*
int
ixfilc();	ixnum = ixfilc(ixunique);
		int	ixunique 1=the index file will be a unique key idx
		COUNT	ixnum	is returned with the new index file number
DSMVOID
ixkill();	ixkill(pc, ixnum);
		COUNT   ixnum   is the number of the index file to be
				deleted.
DSMVOID
ixseto();	ixseto(pc);
			opens the index manager, initialize ixctl etc.
DSMVOID
ixsetj();	ixsetj(pc); reopen the index mgr after a task backout

DSMVOID
ixsetc();	ixsetc(pc);
			closes the index manager.
int
ixfnd();	ret = ixfnd(pc, pkey, pcursid, pdbkey);
		struct key	*pkey	points to the key to be searched
		CURSID		*pcursid points to a cursor id. If the
					 cursorid is NULLCURSOR, one is
					 assigned and returned.
		DBKEY		*pdbkey  points to a dbkey variable.  The
					 dbkey of the found record is
					 returned.
		int		 fndmode FINDFRST, find the first match
		int	ret	return value is:
				  0 - not found.
				  1 - unique entry found.
				 >1 - several matching substr entries.
int
ixgen();	ixgen(pc, pkey, dbkey, auxindex );
		struct key	*pkey	must be same key passed to ixfnd
		DBKEY		 dbkey  variable with new dbk
			returns 0 = gen suceeded ok.
				1 = entry already exists (unique only)
			       -1 = key is too long.
DSMVOID
ixdel();	ixdel(pc, pcursid);
		CURSID		*pcursid must be same retnd from ixfnd
				 the indicated entry will be deleted
int
ixnxt();	ret = ixnxt(pc, pcursid, prangeid, pdbkey);
		CURSID *pcursid	  is the cursor to be advanced.
		CURSID *prangeid  is optional, it is the end of range
					cursor or NULLCURSID.
		DBKEY  *dbkey	  the dbkey of the next data record is
				  returned here.  0 is returned if end of
				  range or file is reached.
		int	ret	  is returned 0 - cursor advanced ok
					      1 - end.
		if ret != 0 then ixnxt reached the end and automatically
		deactivates *pcursid and *prangeid
DSMVOID
ixdlc();	ixdlc(pc, pcursid);
		CURSID *pcursid   will be deactivated, and *pcursid will be
				  set to NULLCURSID
*/
#endif

