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

/**********************************************************************
*
*	C Source:		tmdoundo.c
*	Subsystem:		tm
*	Description:	        Action routines for do/undo
*	%created_by:	richt %
*	%date_created:	Tue Jan 10 12:04:11 1995 %
*
**********************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"

#include "dbcontext.h"
#include "dsmpub.h"
#include "latpub.h"
#include "mstrblk.h"
#include "rlpub.h"
#include "tmdoundo.h"
#include "tmmgr.h"
#include "tmmsg.h"
#include "tmprv.h"
#include "tmtrntab.h"
#include "usrctl.h"  /* temp for DSMSETCONTEXT */
#include "utspub.h"
#include "uttpub.h"

/* PROGRAM: tmrmrcm - remove the ready to commit note from the table.
 *
 * RETURNS: nothing
 */
DSMVOID
tmrmrcm (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,	        /* note */
	struct bkbuf	*pblk _UNUSED_,	  /* Not used - should be null */
	COUNT		 dlen _UNUSED_,   /* Not used - should be 0 */
	TEXT		*pdata _UNUSED_)  /* Not used - should be null */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    TXRC     *ptxrc;
    TRANTABRC *ptrantabrc = (TRANTABRC *)pdbcontext->ptrcmtab;
    

TRACE1_CB(pcontext, "tmrmrcm trans=%l", ((TMNOTE *)pnote)->xaction)

    if (ptrantabrc)
    {
        ptxrc = ptrantabrc->trnrc +
                                 tmSlot(pcontext, ((TMNOTE *)pnote)->xaction);
 
        if (ptxrc->trnum)
        {
	    stnclr(ptxrc,(int)sizeof(TXRC));
	    ptrantabrc->numrc--;
	}
    }
}

/* PROGRAM: tmadd - add a transaction to the in-memory transaction table
 *
 * RETURNS: nothing
 */
DSMVOID
tmadd(
	dsmContext_t	*pcontext,
	RLNOTE		*pLogNote,      /* note */
	struct bkbuf    *pblk _UNUSED_,	  /* Not used - should be null */
	COUNT		 dlen _UNUSED_,	  /* Not used - should be 0 */
	TEXT		*pdata _UNUSED_)  /* Not used - should be null */
{
	LONG		localTrid;
	TX		*pTranEntry;
	TMNOTE          *pnote = (TMNOTE *)pLogNote;

    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
TRACE1_CB(pcontext, "tmadd trans=%l", pnote->xaction)

    localTrid = pnote->xaction;
    pTranEntry = tmEntry(pcontext, localTrid);
 
    MT_LOCK_TXT ();

    stnclr((TEXT *)pTranEntry, (int)sizeof(TX) - sizeof(txPlus_t));

    pTranEntry->txstate = TM_TS_ACTIVE;
    pTranEntry->transnum  = localTrid;
    pTranEntry->rlcounter = pnote->rlcounter;
    pTranEntry->txtime = pnote->timestmp;
    pTranEntry->usrnum = pnote->usrnum;

    pdbcontext->ptrantab->numlive++;
    pdbcontext->pmstrblk->mb_lasttask = pnote->lasttask;

    MT_UNLK_TXT ();

}  /* end tmadd */


/* PROGRAM: tmrem - remove a transaction from the in-memory transaction table
 *
 * RETURNS: nothing
 */
DSMVOID
tmrem (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,	           /* note */
	struct bkbuf	*pblk,		/* Not used - should be null */
	COUNT		 dlen,	           /* Not used - should be 0 */
	TEXT		*pdata)	           /* Not used - should be null */
#if MSC
#pragma optimize("", off)
#endif
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
	LONG		localTrid;
	TX		*pTranEntry;

TRACE1_CB(pcontext, "tmrem trans=%l", ((TMNOTE *)pnote)->xaction)

    localTrid = ((TMNOTE *)pnote)->xaction;
    pTranEntry = tmEntry(pcontext, localTrid);
 
    MT_LOCK_TXT ();

    pdbcontext->ptrantab->numlive--;

    stnclr((TEXT *)pTranEntry, (int)sizeof(TX));

    /* Remove the ready-to-commit entry from the rcomm table. This
       can happen only during warmstart. It can happen because of a
       RL_TEND in roll forward or RL_TBGN in roll backward.
    */
    tmrmrcm(pcontext, pnote, pblk, dlen, pdata);

    MT_UNLK_TXT ();

}  /* end tmrem */


#if MSC
#pragma optimize("", on)
#endif

/* PROGRAM: tmdisp - display contents of tx begin and end notes
 *
 * RETURNS: nothing
 */
DSMVOID
tmdisp (
	dsmContext_t	*pcontext,
	RLNOTE		*pLogNote,       /* note */
	struct bkbuf	*pblk _UNUSED_,	 /* Not used - should be null	 */
	COUNT		 dlen,	         /* Not used, user's userid length*/
	TEXT		*pdata)	         /* Not used, users userid	 */
{
    TMNOTE   *pnote = (TMNOTE *)pLogNote;
    TEXT         timeString[TIMESIZE];
 
    /* Trid: %l %s */
    MSGN_CALLBACK(pcontext, tmMSG001, pnote->xaction,
            uttime((time_t *)&pnote->timestmp, timeString, sizeof(timeString)));
    if( dlen > 0 )
    {
	/* User Id: %s */
	MSGN_CALLBACK(pcontext, tmMSG009,pdata);
    }
}

/* PROGRAM: dolstmod - DO routine to change the master block lstmod field
 *
 * RETURNS: nothing
 */
DSMVOID
dolstmod(
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote,	          /* contains the new lstmod value*/
	BKBUF		*pblk,		  /* Points to the master block */
	COUNT		 dlen _UNUSED_,	  /* Not used - should be 0 */
	TEXT		*pdata _UNUSED_)  /* Not used - should be null */
{
    ((MSTRBLK *)pblk)->mb_chgd = 1;
    ((MSTRBLK *)pblk)->mb_lstmod = ((RLMISC *)pnote)->rlvalue;
}


/* PROGRAM: trcmadd - add a ready-to-commit note to the rcomm table
 *
 * RETURNS: nothing
 */
DSMVOID
trcmadd (
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,	          /* note */
	struct bkbuf	*pblk _UNUSED_,   /* not used - should be null */
	COUNT		 dlen _UNUSED_,   /* data length */
	TEXT		*pdata)	          /* coordinator's name and trans # */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
	LONG		localTrid;
	TX		*pTranEntry;
	TRANTABRC	*ptrantabrc = (TRANTABRC *)pdbcontext->ptrcmtab;
	TXRC		*ptxrc;

    localTrid = ((TMNOTE *)pnote)->xaction;
    pTranEntry = tmEntry(pcontext, localTrid);
 
    pTranEntry->txstate = TM_TS_PREPARED;

    if (ptrantabrc == NULL)
    {
	tmrcmalc(pcontext);
	ptrantabrc = pdbcontext->ptrcmtab;
    }
    
    ptxrc = ptrantabrc->trnrc + tmSlot(pcontext, localTrid);
    ptxrc->trnum = localTrid;
    ptxrc->crdtrnum = xlng(pdata);
    stcopy(ptxrc->crdnam,pdata + sizeof(LONG));
    ptrantabrc->numrc++;
}

