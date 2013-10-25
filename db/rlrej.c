
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

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "rmdoundo.h"
#include "sedoundo.h"
#include "shmpub.h"
#include "bkdoundo.h"
#include "bkpub.h"
#include "bmpub.h"
#include "cxpub.h"
#include "cxprv.h"
#include "dbcontext.h"
#include "dbpub.h"  /* for DB_EXBAD */
#include "latpub.h"
#include "rlpub.h"
#include "rlprv.h"
#include "rlmsg.h"
#include "omprv.h"
#include "stmpub.h"
#include "stmprv.h"
#include "tmtrntab.h"
#include "tmdoundo.h"          /* transaction and save note structure */
#include "tmpub.h"             /* public function prototypes */
#include "usrctl.h"
#include "utmsg.h"
#include "utspub.h"

/* rlrej - dynamic transaction backout */

/* PROGRAM: rlrej - logical undo for a transaction
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlrej(
	dsmContext_t	*pcontext,
	LONG		transnum,     /* transacion number */
	dsmTxnSave_t  	*psavepoint,  /* savepoint number to rollback */ 
	int		force)        /* 1 if force reject after 
                                         Ready-to-Commit*/
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
	usrctl_t    *pusr = pcontext->pusrctl;
    	RLNOTE	    *prlnote;
	COUNT	    dlen;
	TEXT	    *pdata;
	RLCURS	    rlcurs;
	TEXT	    *passybuf;
        LONG        recordType;     /* record type extracted from note */
        LONG        noteRlvalueBlock;     /* block extracted from note */
        LONG        noteRlvalueOffset;    /* block extracted from note */
        ULONG       noteSavePoint;  /* savept from note */
        LONG        scanBlock;
        LONG        scanOffset;
        dsmStatus_t rc;

TRACE3_CB(pcontext,  
          "rlrej transnum=%l, uc_lstblk=%l uc_lstoff %l",
          transnum, pcontext->pusrctl->uc_lastBlock, 
          pcontext->pusrctl->uc_lastOffset)

    if (transnum < 1)
    {
	/* SYSTEM ERROR: Undo trans %l invalid transaction id*/
        MSGN_CALLBACK(pcontext, rlMSG090, transnum);
	return;
    }
    
    /* check for negative blocks and offsets */
    if ( (pusr->uc_lastBlock < 0) ||
         (pusr->uc_lastOffset < 0) )
    {
	/* SYSTEM ERROR: Undo trans %l with invalid bi address  */
        MSGN_CALLBACK(pcontext, rlMSG091,
                      transnum, pusr->uc_lastBlock);
	return;
    }
    if (pusr->uc_txelk)
    {
        /* This is a logical transaction undo. But the last logical
           operation was not completed, so it has to be undone physically.
           But we can't do that. So we have to shut down the database
           because crash recovery will not work otherwise.
 
           This is not supposed to happen. But a bug or fatal error in
           the wrong place might, so we do the check here to be safe */
 
        pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext, utFTL030);
    }

    /* bug 93-01-21-025: allocate assembly buffer with stRent */
    passybuf = stRent (pcontext, pdsmStoragePool, (unsigned)MAXRLNOTE);

    /* init the cursor	for backwards scan */

    rlinitp (pcontext, &rlcurs, pusr->uc_lastBlock, 
             pusr->uc_lastOffset, passybuf);

    /* Iterate on notes until transaction begin is found or error occurs */
    for(;;)
    {

	MT_LOCK_MTX ();
        MT_LOCK_BIB ();
	prlnote = rlrdprv(pcontext, &rlcurs);

	/* skip the useless notes */
	if (   !(xct((TEXT *)&prlnote->rlflgtrn) & RL_LOGBI) ||
	        (xlng((TEXT *)&prlnote->rlTrid) != transnum)   )
        {
	    MT_UNLK_BIB ();
	    MT_UNLK_MTX ();
	    continue;
        }
	switch (prlnote->rlcode)
        {
        case RL_RCOMM:
            MT_UNLK_BIB ();
	    MT_UNLK_MTX ();
	    if (!force)
	    {
                pusr->uc_2phase |= TPLIMBO;

		/* Cannot reject r-comm trans of user on tty */
                FATAL_MSGN_CALLBACK(pcontext, rlFTL003,
                            pusr->uc_usrnum, XTEXT(pdbcontext, pusr->uc_qttydev));
	    }
	    break;
        case RL_TMSAVE:
            /* Save off info from the note for later processing without
               the benefit of latch protection */
            recordType = ((SAVENOTE *)prlnote)->recordtype; 
            noteSavePoint = ((SAVENOTE *)prlnote)->savepoint; 
            noteRlvalueBlock = xlng((TEXT *)&((RLMISC *)prlnote)->rlvalueBlock);
            noteRlvalueOffset = 
                 xlng((TEXT *)&((RLMISC *)prlnote)->rlvalueOffset);
            
            MT_UNLK_BIB ();
            MT_UNLK_MTX ();

            if (recordType == TMSAVE_TYPE_SAVE)
            {
                /* Mark the end of a save point set */
                /* Remember the address of the bi note containing the 
                   savepoint begin and put it in the rollback note. */
                rc = tmMarkUnsavePoint(pcontext, noteRlvalueBlock,
                                        noteRlvalueOffset);

                /* Check to see if we are finished rolling back to the
                   specified savepoint or do we need to continue */
                if (( psavepoint != (dsmTxnSave_t *)PNULL) &&
                    (*psavepoint == noteSavePoint))  
                    goto rtrn;	/* transaction rolled back to savepoint */

                /* We may need to continue until we find the savepoint */
            }
            break;

	case RL_TBGN:
            MT_UNLK_BIB ();
	    MT_UNLK_MTX ();
            goto rtrn;	/* transaction all backed out */

	case RL_JMP:
            scanBlock = xlng((TEXT *)&((RLMISC *)prlnote)->rlvalueBlock);
            scanOffset = xlng((TEXT *)&((RLMISC *)prlnote)->rlvalueOffset);
            rlinitp (pcontext, &rlcurs, scanBlock, scanOffset, PNULL);
            MT_UNLK_BIB ();
	    MT_UNLK_MTX ();
	    break;

	default:
            if (rlcurs.buftype != RLASSYBUF)
            {
                /* make sure it is in the assembly buffer, cause other users
	           may use the buffer too and read something else into it */

	        bufcop (rlcurs.passybuf, (TEXT *)prlnote, rlcurs.notelen);
                prlnote = (RLNOTE *)(rlcurs.passybuf);
            }
            MT_UNLK_BIB ();
	    MT_UNLK_MTX ();

            dlen = rlcurs.notelen - prlnote->rllen;
            if (dlen > 0) pdata = (TEXT *)(prlnote) + prlnote->rllen;
            else pdata = (TEXT *)NULL;

	    /* call undo dispatcher */

	    rlundo (pcontext, prlnote, dlen, pdata);
	    break;
	}
    }
rtrn:
    /* bug 93-01-21-025: free assembly buffer with stVacate */
    stVacate (pcontext, pdsmStoragePool, passybuf);
    return ;
}

/* PROGRAM: rlundo - Main dispatcher to undo a logical bi note
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlundo(
	dsmContext_t	*pcontext,
	RLNOTE		*prlnote,      /* points to the bi note	*/  
	COUNT		 dlen,
	TEXT		*pdata)
{
  TRACE1_CB(pcontext, "%Lrlrej: undoing note code=%t", prlnote->rlcode);

  if(pcontext->pdbcontext->prlctl->rlwarmstrt)
    if(xlng((TEXT *)&prlnote->rlArea) == DSMAREA_TEMP)
      return;

    switch (prlnote->rlcode)
    {
    case RL_RMCR:   rmUndoLogicalCreate(pcontext, (rmRecNote_t *)prlnote,
	                                dlen, pdata);
		    break;

    case RL_RMDEL:  rmUndoLogicalDelete(pcontext, (rmRecNote_t *)prlnote,
			                dlen, pdata);
		    break;

    case RL_RMCHG:
		    /* bug 92-11-04-039: dont need extra parameter */
                    rmUndoLogicalChange(pcontext, (rmChangeNote_t *)prlnote); 
                    break;

    case RL_RCOMM:
    case RL_TABRT:
    case RL_TBGN:
		    break;

    case RL_SEASN:
    case RL_SEADD:
    case RL_SEUPD:
    case RL_SEDEL:
    		    seqUndo(pcontext, prlnote);
		    break;

    case RL_IXDEL:
    case RL_CXREM:
    case RL_CXINS:
    case RL_IXUKL:
    case RL_IXFILC:
    case RL_CXTEMPROOT:
    case RL_IXKIL:
    case RL_IXDBLK:
   	 	    cxUndo(pcontext, prlnote, dlen, pdata);
                    break;

    case RL_IDXBLOCK_COPY:
    case RL_IDXBLOCK_UNCOPY:
                    /* RL_IDXBLOCK_UNCOPY is marked RL_PHY so it
                       shdn't be encountered here  just added to remind there
                       is a pair of these notes */
                    cxUndoBlockCopy( pcontext, prlnote, dlen, pdata); 
                    break;

    case RL_BKAREA_ADD:
      bkAreaRemoveDo(pcontext,prlnote,NULL,0,NULL);
                    break;
    case RL_BKAREA_DELETE:
                    break;

    case RL_OBJECT_CREATE:
    case RL_OBJECT_UPDATE:
   	 	    omUndo(pcontext, prlnote);
                    break;

    case RL_BKEXTENT_CREATE:
                    break;
    case RL_BKEXTENT_DELETE:
                    break;
    case RL_ROW_COUNT_UPDATE:
        bkRowCountUpdate(pcontext,xlng((TEXT *)&prlnote->rlArea),
			 ((bkRowCountUpdate_t *)prlnote)->table,
                         !(int)((bkRowCountUpdate_t *)prlnote)->addIt);
        break;

    default:
        /* "%GUnrecognized undo code <code>%d." */
        FATAL_MSGN_CALLBACK(pcontext, rlFTL015, prlnote->rlcode,
                            xct((TEXT *)&prlnote->rlflgtrn));
        break;
    }
}
