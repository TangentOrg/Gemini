
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
#include "dsmpub.h"
#include "rltlpub.h"
#include "rltlprv.h"
#include "dbcontext.h"
#include "bkaread.h"
#include "rlprv.h"
#include "rlmsg.h"
#include "stmpub.h"
#include "tmmgr.h"
#include "upmsg.h"
#include "rldoundo.h"
#include "utspub.h"

/* PROGRAM: rltlOpen - Set up the transaction logging sub-system 
 *
 * RETURNS:
 */
LONG rltlOpen( dsmContext_t *pcontext )
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    rltlCtl_t   *ptlctl;
    BKTBL       *pbktbl;
    mstrblk_t   *pmstr  = pcontext->pdbcontext->pmstrblk;
    
    /* allocate the ai control structure */
    ptlctl = (rltlCtl_t *)stGet(pcontext,
                                (STPOOL *)QP_TO_P(pdbcontext,
                                pdbcontext->pdbpub->qdbpool),
                                sizeof(rltlCtl_t));
    pdbcontext->pdbpub->qtlctl = (QTLCTL)P_TO_QP(pcontext, ptlctl);
    pdbcontext->ptlctl = ptlctl;

    /* Allocate a write buffer        */
    pbktbl = &ptlctl->tlBuffer;
    QSELF (pbktbl) = P_TO_QP(pcontext, pbktbl);

    ptlctl->writeLocation = pmstr->mb_tlwrtloc;
    ptlctl->writeOffset   = pmstr->mb_tlwrtoffset;

    pbktbl->bt_raddr = -1;
    pbktbl->bt_dbkey = ptlctl->writeLocation;
    pbktbl->bt_area = DSMAREA_TL;
    pbktbl->bt_ftype = BKTL;
    bmGetBufferPool(pcontext, pbktbl);
    bkRead(pcontext,pbktbl);
    
    return 0;
}
/* PROGRAM: rltlClose - Close the tl sub-system
 *
 */
LONG rltlClose(dsmContext_t *pcontext)
{
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    MT_LOCK_AIB();
    /* Flush the tl buffer.                */
    bkWrite(pcontext,&pdbcontext->ptlctl->tlBuffer,
            BUFFERED);
    
    /* Set tl valuse in the master block        */
    pdbcontext->pmstrblk->mb_tlwrtloc = pdbcontext->ptlctl->writeLocation;
    pdbcontext->pmstrblk->mb_tlwrtoffset = pdbcontext->ptlctl->writeOffset;
    MT_UNLK_AIB();
    bmFlushMasterblock(pcontext);

    
    return 0;
}

/* PROGRAM: rltlQon - Is 2pc enabled for this database.      
 *
 * RETURNS: 0 - no
 *          1 - yes
 */
LONG rltlQon( dsmContext_t *pcontext )
{
    if( pcontext->pdbcontext->pmstrblk->mb_lightai )
        return 1;
    else
        return 0;
}

/* PROGRAM: rltlOn - Enable 2pc on the database   */

LONG rltlOn(dsmContext_t *pcontext,
            TEXT          coordinator,     /* Coordinator flag */
            TEXT         *pnickName, /* db short hand name */
            int           begin)     /* 1 if 2phase begin
                                        0 if 2phase modify  */
{
    rltlOnNote_t  tlNote;
    bmBufHandle_t     mstrHandle;
    
    INITNOTE(tlNote,RL_TL_ON,RL_PHY);

    tlNote.coordinator = coordinator;
    bufcop(tlNote.nickName,pnickName,sizeof(tlNote.nickName));
    tlNote.begin = (TEXT)begin;
    
    rlTXElock(pcontext,RL_TXE_SHARE,RL_MISC_OP);
    mstrHandle = bmLocateBlock(pcontext, BK_AREA1, 
                        BLK1DBKEY(BKGET_AREARECBITS(DSMAREA_SCHEMA)),
                               TOMODIFY);
    
    rlLogAndDo(pcontext, &tlNote.rlnote, mstrHandle, (COUNT)0, PNULL );

    bmReleaseBuffer(pcontext, mstrHandle);
    rlTXEunlock(pcontext);
    bmFlushMasterblock(pcontext);

    return 0;
}

/* PROGRAM: rltlOn - Enable 2pc on the database - the do routine     
 *
 * RETURNS:
 *          
 */

DSMVOID rltlOnDo(
        dsmContext_t	*pcontext,
	RLNOTE		*pnote,	          /* contains the new lstmod value*/
	BKBUF		*pblk,		  /* Points to the master block */
	COUNT		 dlen _UNUSED_,   /* Not used - should be 0 */
	TEXT		*pdata _UNUSED_)  /* Not used - should be null */ 
{
    bkAreaDesc_t  *pbkAreaDesc;
    LONG          rc = 0;
    rltlCtl_t     *ptlctl;
    rltlBlockHeader_t *ptlBlock;
    mstrblk_t     *pmstr = (mstrblk_t *)pblk;
    rltlOnNote_t  *ptlNote = (rltlOnNote_t *)pnote;

    if (ptlNote->begin)
    {
        pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, DSMAREA_TL);
        if( pbkAreaDesc == NULL )
        {
            MSGN_CALLBACK(pcontext,rlMSG134);
            MSGD_CALLBACK(pcontext,"%g2phase commit not enabled");
        }
        rc = rltlOpen(pcontext);
        if( rc )
        {
            MSGD_CALLBACK(pcontext,"%BCould not open tl sub-system");
            MSGD_CALLBACK(pcontext,"%g2phase commit not enabled");
        }
    
        ptlctl = pcontext->pdbcontext->ptlctl;
        ptlctl->writeOffset = sizeof(rltlBlockHeader_t);
        ptlBlock = XTLBLK(pcontext->pdbcontext,ptlctl->tlBuffer.bt_qbuf);
        ptlBlock->headerSize = ptlctl->writeOffset;
        ptlBlock->used = ptlctl->writeOffset;
        ptlctl->tlBuffer.bt_dbkey = 0;
        bkaddr (pcontext, &ptlctl->tlBuffer,1);
        bkWrite(pcontext,&ptlctl->tlBuffer,BUFFERED);
        pmstr->mb_tlwrtloc = 0;
        pmstr->mb_tlwrtoffset = ptlctl->writeOffset;
        pmstr->mb_lightai = (TEXT)1;
    }
    
    pmstr->mb_crd = ptlNote->coordinator;
    bufcop(pmstr->mb_nicknm,ptlNote->nickName,sizeof(pmstr->mb_nicknm));
           
    return;
}

/* PROGRAM: rltlSeek - Initialize tl sub-system to start logging
 * at the specified position.
 *
 */
LONG rltlSeek(dsmContext_t  *pcontext, ULONG blockNumber, ULONG offset)
{
    rltlCtl_t   *ptlctl = pcontext->pdbcontext->ptlctl;

    MT_LOCK_AIB();

    ptlctl->writeLocation = blockNumber;
    if( offset )
        ptlctl->writeOffset = offset;
    else
        ptlctl->writeOffset = sizeof(rltlBlockHeader_t);

    ptlctl->tlBuffer.bt_dbkey = blockNumber;
    bkRead(pcontext,&ptlctl->tlBuffer);
    MT_UNLK_AIB();
    
    return 0;
}

/* PROGRAM: rltlCheckBlock - Sanity check rl block 
 *
 * RETURNS: 0 - no
 *          1 - yes
 */
LONG rltlCheckBlock(dsmContext_t *pcontext, BKTBL *pbktbl)
{
    ULONG        blknum;
    int          buflen;
    LONG         smashed;
    bkftbl_t	*pbkftbl;
    rltlBlockHeader_t *ptlBlock;
    
    /* which extent table entry block is in */
    pbkftbl = XBKFTBL(pcontext->pdbcontext, pbktbl->bt_qftbl);
    
    /* determine the length of this type of buffer based on blocksize */
    buflen = pbkftbl->ft_blksize;

    /* get the block number from the buffer */
    blknum = pbktbl->bt_dbkey;
    
    /* set up the validate the header info */
    smashed = 0;
    ptlBlock = (rltlBlockHeader_t *)XBKBUF(pcontext->pdbcontext,
                                           pbktbl->bt_qbuf);


    /* validate regular block header size */
    if ((ptlBlock->headerSize != 0) &&
        (ptlBlock->headerSize != sizeof (rltlBlockHeader_t)))
    {
        /* header is invalid */
        smashed += 1;
    }

    /* validate used value against header length and buffer length */
    if ((ptlBlock->used > buflen) ||
        (ptlBlock->used < ptlBlock->headerSize))
    {
        /* header is invalid */
        smashed += 2;
    }

    /* if block is invalid then shutdown */
    if (smashed)
    {
        pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext, rlFTL077, blknum, smashed);
    }   

    /* store the block number in the header */
    ptlBlock->blockNumber = blknum;

    return 0;
}

/* PROGRAM - rltlSpool  - Spool a note into the tl buffer
 *
 */
LONG rltlSpool(dsmContext_t *pcontext, dsmTrid_t trid)
{
    rltlCtl_t  *ptlctl = pcontext->pdbcontext->ptlctl;
    rltlBlockHeader_t *pblock;
    dsmTrid_t   *pnextTrid;

    if ( ptlctl == NULL )
        /* Can be called during redo phase of crash recovery
           after a 2phase end operation.                      */
        return 0;
    

    pblock = XTLBLK(pcontext->pdbcontext,ptlctl->tlBuffer.bt_qbuf);
    MT_LOCK_AIB();
    if( ptlctl->writeOffset == RLTL_BUFSIZE )
    {
        /* Write the current buffer                 */
        /* But we have to flush bi and ai buffers first.   */
        MT_UNLK_AIB();
        rlbiflsh(pcontext,RL_FLUSH_ALL_BI_BUFFERS);
        MT_LOCK_AIB();
        rlaiflsx(pcontext);
        bkWrite(pcontext,&ptlctl->tlBuffer,BUFFERED);
        ptlctl->writeLocation++;
        if( ptlctl->writeLocation >= (ULONG)bkflen(pcontext,DSMAREA_TL) )
        {
            ptlctl->writeLocation = 0;
        }
        ptlctl->writeOffset = sizeof(rltlBlockHeader_t);
        ptlctl->tlBuffer.bt_dbkey = ptlctl->writeLocation;

        bkaddr(pcontext,&ptlctl->tlBuffer,1);

        pblock->headerSize = ptlctl->writeOffset;
        pblock->used = ptlctl->writeOffset;
        pblock->blockNumber = ptlctl->writeLocation;
    }
    pnextTrid = (dsmTrid_t *)((TEXT*)pblock + ptlctl->writeOffset);
    *pnextTrid = trid;
    ptlctl->writeOffset += sizeof(dsmTrid_t);
    pblock->used = ptlctl->writeOffset;

    MT_UNLK_AIB();

    return 0;
}
/* PROGRAM - rltlScan - Scan the tl file looking for a given trid.
 *
 */
LONG rltlScan(dsmContext_t *pcontext,
              dsmTrid_t trid,       /* Trid to scan for */
              int operation)        /* RLTL_OP_SCAN or RLTL_OP_COMMIT  */
{
    TEXT   readBuffer[RLTL_BUFSIZE];
    rltlBlockHeader_t *preadBlock;
    rltlCtl_t *ptlctl = pcontext->pdbcontext->ptlctl;
    BKTBL  tlbktbl;
    dsmTrid_t  *ptrid;

    stnclr(&tlbktbl, sizeof(BKTBL));
    preadBlock = (rltlBlockHeader_t *)readBuffer;
    
    /* Initialize a buffer control block for reading.  */
    tlbktbl.qself = P_TO_QP(pcontext, &tlbktbl);
    tlbktbl.bt_raddr=-1;
    tlbktbl.bt_area = DSMAREA_TL;
    tlbktbl.bt_ftype = BKTL;
    tlbktbl.bt_qbuf = (QBKBUF)P_TO_QP(pcontext,readBuffer);
    /* Copy the write buffer into our local buffer and
       scan that first.                                  */
    MT_LOCK_AIB();
    bufcop(readBuffer, XTLBLK(pcontext->pdbcontext,ptlctl->tlBuffer.bt_qbuf),
           RLTL_BUFSIZE);
    MT_UNLK_AIB();
    tlbktbl.bt_dbkey = preadBlock->blockNumber;
    do
    {
        /* Scan the block looking for a match on the trid */
        for(ptrid = (dsmTrid_t *)
                ((TEXT *)preadBlock + preadBlock->used - sizeof(dsmTrid_t));
            ((TEXT *)ptrid - (TEXT *)preadBlock) > preadBlock->headerSize;
            ptrid--)
        {
            if(*ptrid == trid && operation == RLTL_OP_COMMIT)
            {
                /* Transaction %l has committed */
	        MSGN_CALLBACK(pcontext,upMSG093, trid);
                return TP_COMMIT;
            }
            else if (operation == RLTL_OP_SCAN)
                MSGD_CALLBACK(pcontext," %l",*ptrid);
        }
        
        if(tlbktbl.bt_dbkey)
            tlbktbl.bt_dbkey = preadBlock->blockNumber - 1;
        else
            tlbktbl.bt_dbkey = bkflen(pcontext,DSMAREA_TL) - 1;

        bkRead(pcontext,&tlbktbl);
        if( preadBlock->used <= preadBlock->headerSize)
            /* Unused block -- must be eof                */
            break;
        
    }while(tlbktbl.bt_dbkey != ptlctl->writeLocation);
    
    /* Transaction %l was not committed */
    if(operation == RLTL_OP_COMMIT)
        MSGN_CALLBACK(pcontext,upMSG092, trid);

    return TP_ABORT;
}

/* PROGRAM - rltlSetEof in the tl control structure
 *           called after a bi file truncation
 */
LONG rltlSetEof(dsmContext_t *pcontext)
{
    rltlCtl_t   *ptlctl = pcontext->pdbcontext->ptlctl;
    mstrblk_t   *pmstr  = pcontext->pdbcontext->pmstrblk;

    ptlctl->writeLocation = pmstr->mb_tlwrtloc;
    ptlctl->writeOffset   = pmstr->mb_tlwrtoffset;

    return 0;
}

/*
 * PROGRAM: rltoff -- turns off 2 phase transaction logging and returns status
 *
 * This program turns off transaction logging by clearing the information in 
 * the masterblock used by the ai subsystems.  It does not write the
 * masterblock back out, so that must happen before it is really turned off.
 *
 * RETURNS: 0 if 2pc was turned off, 1 if it was never on in the first place
 */
int
rltoff(MSTRBLK *pmstr)
{
    /* clear information in masterblock for 2pc, except nickname */
    pmstr->mb_lightai = (TEXT) 0;
    pmstr->mb_crd = (TEXT) 0;

    if (pmstr->mb_nicknm[0] == (TEXT) 0)
    {
	/* 0 if logging is already off */
	return 1;
    }

    /* 2pc was on, clear nickname */
    pmstr->mb_nicknm[0] = (TEXT) 0;
    return 0;

} /* rltoff */
    


