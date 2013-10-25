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

#ifndef TMDOUNDO_H
#define TMDOUNDO_H

#include "rlpub.h"   /* For RL_DO_UNDO_PROTO  */

struct dsmContext;

/*----------------------------------------------*/
/* Define the transaction start/end note.	*/
/*----------------------------------------------*/
/* Bug 20000114-034				*/
/*						*/
/*	Fillers were added to make it possible	*/
/*	to initialize structure space that was	*/
/*	previously being created by all the	*/
/*	compilers in order to satisfy the	*/
/*	required alignment.  These fillers	*/
/*	do not impact the size of the structure	*/
/*	or the placement of the data within	*/
/*	the structure, and should therefore	*/
/*	be transparent to existing notes on	*/
/*	disk.					*/
/*						*/
/*	The macro INIT_S_TMNOTE_FILLERS is 	*/
/*	being added so that the new fillers can	*/
/*	be initialized without the local code	*/
/*	having to know what they are.		*/
/*						*/
/*----------------------------------------------*/

typedef struct tmnote {
    RLNOTE	rlnote;
    LONG	xaction;
    LONG	rlcounter;
    LONG	timestmp;
    psc_user_number_t	usrnum;
    COUNT	filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
    LONG	lasttask;
    TEXT	accrej;
    TEXT	filler2;	/* bug 20000114-034 by dmeyer in v9.1b */
    COUNT	filler3;	/* bug 20000114-034 by dmeyer in v9.1b */
} TMNOTE;

#define INIT_S_TMNOTE_FILLERS( tm ) (tm).filler1 = 0; (tm).filler2 = 0; (tm).filler3 = 0;


typedef ULONG tmSaveType_t;             /* save point record type */
#define TMSAVE_TYPE_INVALID    0        /* invalid record type */
#define TMSAVE_TYPE_SAVE       1        /* Set a save point */
#define TMSAVE_TYPE_UNSAVE     2        /* Rollback a save point */

/*----------------------------------------------*/
/* Define the transaction start/end note.	*/
/*----------------------------------------------*/

typedef struct savenote {
    RLNOTE	  rlnote;
    tmSaveType_t  recordtype;
    ULONG	  savepoint;
    psc_user_number_t	  usrnum;
    LONG	  xaction;
    LONG          noteblock;
    LONG          noteoffset;
} SAVENOTE;

DSMVOID dolstmod(RL_DO_UNDO_PROTO);
DSMVOID tmadd(RL_DO_UNDO_PROTO);
DSMVOID tmrem(RL_DO_UNDO_PROTO);
DSMVOID tmdisp(RL_DO_UNDO_PROTO);
DSMVOID tmrmrcm(RL_DO_UNDO_PROTO);
DSMVOID trcmadd(RL_DO_UNDO_PROTO);

#endif  /* TMDOUNDO_H */
