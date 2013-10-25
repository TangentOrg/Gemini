
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
#include "bkpub.h"
#include "rlpub.h"
#include "ixmsg.h"
#include "dsmpub.h"
#include "dbcontext.h"
#include "usrctl.h"
#include "rlprv.h"
#include "ompub.h"
#include "cxpub.h"
#include "cxprv.h"
#include "dbmgr.h" /* needed for FNDFAIL definition */
#include "geminikey.h"
#include "scasa.h"
#include "utspub.h"


/****************************************************************/
/* PROGRAM: cxUndo -- logical backout for both dynamic and crash
 *          This program does a logical undo of an ix operation
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxUndo (
        dsmContext_t    *pcontext,
        RLNOTE          *pnote,
        COUNT            dlen,
        TEXT            *pdata)

{
    CXNOTE  *pKeyNote = (CXNOTE *)pnote;
    bmBufHandle_t       bufHandle;         /* Buffer handle for root */
    cxblock_t           *pBlock;        
    int  ret;
    dsmObject_t          tableNum;
    TEXT                 *pos;
    int                   ks;
#ifdef VARIABLE_DBKEY
    int     len;
    DBKEY   dbkey;
#endif  /* VARIABLE_DBKEY */
    
    AUTOKEY(k, KEYSIZE + FULLKEYHDRSZ + sizeof(DBKEY))
        
    switch(pnote->rlcode) {

    case RL_IXDEL:
    case RL_CXREM:
            /* a logical cxixdel note contains the key ditem intact */
            stnclr(&k.akey, sizeof(dsmKey_t));
            cxKeyToStruct(&k.akey, pdata);
            k.akey.area = xlng((TEXT *)&pnote->rlArea);
            k.akey.root = pKeyNote->indexRoot;
            tableNum = pKeyNote->tableNum;
	    if(pcontext->pdbcontext->prlctl->rlwarmstrt)
	      if(SCTBL_IS_TEMP_OBJECT(tableNum))
		return;
            k.akey.unique_index = pKeyNote->indexAttributes & DSMINDEX_UNIQUE ;
            k.akey.word_index = pKeyNote->indexAttributes & DSMINDEX_WORD ;
            
#ifdef VARIABLE_DBKEY
/*====large keys=============================== */
            pos = &k.akey.keystr[0];
            ks = xct(&pos[KS_OFFSET]);
            pos += FULLKEYHDRSZ + ks;  /* past the key, to the info byte */
            len = *(pos - 1) & 0x0f;
 	        gemDBKEYfromIdx(pos - len, &dbkey);
 	        if(((CXNOTE *)pnote)->tagOperation == INDEX_TAG_OPERATION)
 	        {
 	            dbkey -= *pos;
 	            dbkey += ((CXNOTE *)pnote)->sdbk[sizeof((CXNOTE *)pnote)->sdbk - 1];
	        }
	    ks -= len;
	    sct(&pos[KS_OFFSET],ks);
            k.akey.keyLen = ks;
            ret = cxAddNL(pcontext, &k.akey, dbkey, PNULL, tableNum);
#else
            k.akey.keyLen = k.akey.keystr[1] -= 3;
            /* cxAddEntry without locking the record*/
            ret = cxAddNL(pcontext, &k.akey, xlng(pKeyNote->sdbk), PNULL,
                          tableNum);
#endif  /* VARIABLE_DBKEY */
                                  /* -1 for don't use alt index */

            if (ret == DSM_S_RQSTQUED)
            {
                /* cxundo IXDEL, DSM_S_RQSTQUED unexpected */
                FATAL_MSGN_CALLBACK(pcontext, ixFTL021);
            }
            if (ret != 0)
            {
               /* if failure is due to a duplicate then do not fatal,just report the error */
               if ( ret == 1 )
               { 
                    MSGN_CALLBACK(pcontext, IXMSG004, k.akey.index );
               }
               else
               {
                    FATAL_MSGN_CALLBACK(pcontext, ixFTL015, ret);
               }
            }
            break;
             
    case RL_CXINS:
            /* a logical cxixgen note contains the key ditem intact */
            stnclr(&k.akey,sizeof(dsmKey_t));
            cxKeyToStruct(&k.akey, pdata);
            tableNum = pKeyNote->tableNum;
            k.akey.area = xlng((TEXT *)&pnote->rlArea);
            k.akey.root = pKeyNote->indexRoot;
            k.akey.unique_index = pKeyNote->indexAttributes & DSMINDEX_UNIQUE ;
            k.akey.word_index = pKeyNote->indexAttributes & DSMINDEX_WORD ;
	    k.akey.unknown_comp = (dsmBoolean_t)cxKeyUnknown(k.akey.keystr);
#ifdef VARIABLE_DBKEY
/*====large keys=============================== */
            pos = &k.akey.keystr[0];
            ks = xct(&pos[KS_OFFSET]);
            pos += FULLKEYHDRSZ + ks;  /* past the key, to the info byte */
            len = *(pos - 1) & 0x0f;
	    gemDBKEYfromIdx(pos - len, &dbkey);
	    if(((CXNOTE *)pnote)->tagOperation == INDEX_TAG_OPERATION)
	    {
	      dbkey -= *pos;
	      dbkey += ((CXNOTE *)pnote)->sdbk[sizeof((CXNOTE *)pnote)->sdbk - 1];
	    }
/*====Rich's change=============================== */
/*            dbkey = xlng(((CXNOTE *)pnote)->sdbk); */
/*	    len = k.akey.keystr[k.akey.keystr[1] + ENTHDRSZ - 1] & 0x0f; */
/* 	    gemDBKEYfromIdx(&k.akey.keystr[k.akey.keystr[1] + ENTHDRSZ - len], */
/* 			    &dbkey); */
 	    /* In the case of a dbkey tag then we must get the low order byte
	       of the dbkey from the note and subtract out gemDBKEYfromIdx added in */
/* 	    if(((CXNOTE *)pnote)->cs == 0xff) */
/* 	    { */
/* 	      dbkey -= k.akey.keystr[k.akey.keystr[1] + ENTHDRSZ]; */
/* 	      dbkey += ((CXNOTE *)pnote)->sdbk[sizeof((CXNOTE *)pnote)->sdbk - 1]; */
/* 	    } */
/*------------------------------------------------ */
	    ks -= len;
	    sct(&pos[KS_OFFSET],ks);
            k.akey.keyLen = ks;
            ret = cxDeleteNL(pcontext, &k.akey, dbkey, IXLOCK, tableNum, PNULL);
#else
            /* -3 because key will be prepared in cxKeyPrepare where the
               dbkey will be added to the key and the length of the key will be 
               adjusted to account for this.                                     */
            k.akey.keyLen = k.akey.keystr[1] -= 3;
            ret = cxDeleteNL(pcontext, &k.akey, xlng(((CXNOTE *)pnote)->sdbk),
                             IXLOCK, tableNum, PNULL);
#endif  /* VARIABLE_DBKEY */
            
            if (ret != 0) 
            {
               /* if can't be deleted because not there, then do not fatal 
                  just report the error                                  */
               if ( ret == FNDFAIL )
               {
                    MSGN_CALLBACK(pcontext, IXMSG005, k.akey.index );       
               }
               else
                   FATAL_MSGN_CALLBACK(pcontext, ixFTL013, ret);
            }  
            break;

    case RL_IXUKL:
    case RL_IXFILC:
        /* Re get the root of the index it might have changed
           99-02-15-029  */
#if 0   /* cxKill not needed for gemini, the index will be
	   removed when the file containing it is deleted.   */
            omDescriptor_t omDesc;
            tableNum = ((IXFINOTE *)pnote)->table;
            ret = omFetch(pcontext, DSMOBJECT_MIXINDEX,
			  ((IXFINOTE *)pnote)->ixnum, tableNum,
                       &omDesc);
    
            cxKill(pcontext, ((IXFINOTE *)pnote)->ixArea,
                   omDesc.objectRoot,
                   ((IXFINOTE *)pnote)->ixnum, 0, tableNum, DSMAREA_INVALID );

            omDelete(pcontext, DSMOBJECT_MIXINDEX, tableNum,
                     ((IXFINOTE *)pnote)->ixnum);
#endif
            break;

    case RL_CXTEMPROOT:
            
            /* acquire the block and make sure we don't try to kill an
               index in use by some other index                       */
            bufHandle = bmLocateBlock(pcontext, ((IXFINOTE *)pnote)->ixArea, 
                                      ((IXFINOTE *)pnote)->ixdbk, TOMODIFY);
            pBlock = (cxblock_t *)bmGetBlockPointer(pcontext, bufHandle);

            if ((int)(pBlock->BKTYPE) != IDXBLK  || 
                (pBlock->IXNUM != ((IXFINOTE *)pnote)->ixnum))
            {
                /* we don't expect this to happen */
                bmReleaseBuffer(pcontext, bufHandle);
                break;
            }

            /* we will phyiscally remove the block so we will never see
               it in logical undo again                                */
            cxDeleteBlock (pcontext, bufHandle, (UCOUNT) (RL_PHY),
                          ((IXFINOTE *)pnote)->ixdbk);
            bmReleaseBuffer(pcontext, bufHandle);
            break;

    case RL_IXKIL:
    case RL_IXDBLK:
            cxUnKill(pcontext, pnote, dlen, pdata);
            break;

    default:
            FATAL_MSGN_CALLBACK(pcontext, ixFTL005, pnote->rlcode );
    }
}

/* PROGRAM: cxUndoCompactLeft
 *
 * Returns: 
 *
 */
DSMVOID
cxUndoCompactLeft(dsmContext_t *pcontext _UNUSED_,
                  RLNOTE       *pIxNote,
                  BKBUF        *pblock,
                  COUNT         dataLength _UNUSED_,
                  TEXT         *pdata _UNUSED_)
{
    cxblock_t       *pcxblock  = (cxblock_t *)pblock;
    cxCompactNote_t *pcxnote   = (cxCompactNote_t *)pIxNote;

    pcxblock->IXLENENT = pcxnote->lengthOfEntries;
    pcxblock->IXNUMENT -= pcxnote->numMovedEntries;

}
/* PROGRAM: cxUndoCompactRight
 *
 * Returns: 
 *
 */
DSMVOID
cxUndoCompactRight(dsmContext_t *pcontext _UNUSED_,
                  RLNOTE        *pIxNote,
                  BKBUF         *pblock,
                  COUNT          dataLength,
                  TEXT          *pdata)
{
    cxblock_t       *pcxblock  = (cxblock_t *)pblock;
    cxCompactNote_t *pcxnote   = (cxCompactNote_t *)pIxNote;
    TEXT            *pdataEntry,
                    *pposEntry,
                    *pos,
                    *pRightPos;
    LONG             tempSize;
    int              xsize, 
/*                     infoSize = 4, */
                     offsetKeySize,
                     firstEntrySize,
                     offsetEntrySize;
    int	             hs,cs,is,cs2,hs2;

    pos       = pcxblock->ixchars,
    pRightPos = pcxblock->ixchars + pcxnote->offset;

    if (pcxblock->IXBOT)
    {
        /* this is a leaf blk */    
        /* move the data over to make room for the compressed key */
/*        bufcop(pRightPos + entryHeaderSize,  */
/*               pos + entryHeaderSize + pcxnote->cs,  */
/*               pcxblock->IXLENENT - (entryHeaderSize + pcxnote->cs)); */
        hs = HS_GET(pRightPos);
        cs = pcxnote->cs;
        bufcop(pRightPos + hs, pos + hs + cs, pcxblock->IXLENENT - (hs + cs));
    
        /* update the entry header at the offset */
/*        pRightPos[csOffset] = pcxnote->cs; */
/*        pRightPos[ksOffset] = pos[ksOffset] - pcxnote->cs; */
/*        pRightPos[isOffset] = pos[isOffset]; */
        CS_PUT(pRightPos, cs);
        KS_PUT(pRightPos, KS_GET(pos) - cs);
        IS_PUT(pRightPos, IS_GET(pos));
        
        /* put the moved entries back as is */
        bufcop(pos, pdata, pcxnote->moveBufferSize);

        /* update the num entries and entries length */
        pcxblock->IXLENENT = pcxnote->lengthOfEntries;
        pcxblock->IXNUMENT += pcxnote->numMovedEntries;
    }
    else
    {
        /* this is not a leaf blk */
        /* point to the uncompressed entry which will go at
           the offset */
        offsetKeySize = dataLength - pcxnote->moveBufferSize - pcxnote->cs;
        firstEntrySize = ENTRY_SIZE_COMPRESSED(pos);
/*        offsetEntrySize = entryHeaderSize + offsetKeySize + infoSize; */
        is = IS_GET(pos); /* IS of the first entry, will become that of the offset entry */
        hs = ((offsetKeySize + pcxnote->cs) > MAXSMALLKEY) ? LRGENTHDRSZ : ENTHDRSZ;
        offsetEntrySize = hs + offsetKeySize + is;

        /* are there more than one entry to move back to the offset? */
        if (pcxblock->IXNUMENT >= 2)
        {
            /* move the second entry beginning at the key, 
               recompressing it, till the end of the entries. 
               this makes room for the offset entry */

            /* move pos to the second entry */
            pos += firstEntrySize;

/*            tempSize = firstEntrySize + entryHeaderSize +  */
/*                                    pcxnote->csofnext - pos[csOffset]; */
            cs2 = CS_GET(pos); /* cs of the 2nd entry */
            hs2 = HS_GET(pos); /* hs of the 2nd entry */
            tempSize = firstEntrySize + hs2 + 
                                    pcxnote->csofnext - cs2;

            /* move to the second entry after the offset */
            pRightPos += offsetEntrySize;
            /* move the second entry after the offset and the remaining
               entries back.  the second entry's compression will be
               back as well */
/*            bufcop(pRightPos + entryHeaderSize,  */
/*                   pos + entryHeaderSize + pcxnote->csofnext - pos[csOffset],  */
            bufcop(pRightPos + hs2, 
                   pos + hs2 + pcxnote->csofnext - cs2, 
                   pcxblock->IXLENENT - tempSize);

            /* update the cs, ks, is of the 2nd entry after the offset */
/*            pRightPos[csOffset] = pcxnote->csofnext; */
/*            pRightPos[ksOffset] = (pos[csOffset] + pos[ksOffset]) - pcxnote->csofnext; */
/*            pRightPos[isOffset] = pos[isOffset]; */
            CS_PUT(pRightPos, pcxnote->csofnext);
            KS_PUT(pRightPos, cs2 + KS_GET(pos) - pcxnote->csofnext);
            IS_PUT(pRightPos, IS_GET(pos));

            /* move back to the beginnging of the offset entry and
               back to the beginning of the first entry */
            pos -= firstEntrySize;
            pRightPos -= offsetEntrySize;
        }

        /* put the compressed key back in at the offset */
/*        bufcop(pRightPos + entryHeaderSize,  */
        bufcop(pRightPos + hs, 
               pdata + pcxnote->moveBufferSize + pcxnote->cs,
               offsetKeySize);

        /* move the info, take the dbkey from the first entry and move
           to the offset entry */
/*        bufcop(pRightPos + entryHeaderSize + offsetKeySize,  */
/*               pos + entryHeaderSize + pos[ksOffset],  */
/*               pos[isOffset]); */
        bufcop(pRightPos + hs + offsetKeySize, 
               pos + HS_GET(pos) + KS_GET(pos), 
               is);

        /* update cs ks for the offset entry */
/*        pRightPos[csOffset] = pcxnote->cs; */
/*        pRightPos[ksOffset] = offsetKeySize; */
/*        pRightPos[isOffset] = pos[isOffset]; */
        CS_PUT(pRightPos, pcxnote->cs);
        KS_PUT(pRightPos, offsetKeySize);
        IS_PUT(pRightPos, is);

        /* put back in the moved entries, starting with the info of
           the first entry */
/*        tempSize = entryHeaderSize + pdata[ksOffset]; */
/*        bufcop(pos + firstEntrySize - pos[isOffset], */
/*               pdata + tempSize, */
/*               infoSize); */
        tempSize = HS_GET(pdata) + KS_GET(pdata);
        bufcop(pos + firstEntrySize - is,
               pdata + tempSize,
               IS_GET(pdata));
        if (pcxnote->numMovedEntries >=2)
        {
            /* the second entry in pdata is compressed based on the
               first entry in pdata.  It needs to be compressed based
               on the first entry in the block */
            /* position for handling the second entry */
/*            pdataEntry = pdata + tempSize + infoSize; */
            pdataEntry = pdata + tempSize + IS_GET(pdata);
            pposEntry = pos + firstEntrySize;
            xsize = 0;

            /* was the key compressed in pdata? */
/*            if (pdataEntry[csOffset] != 0) */
            cs = CS_GET(pdataEntry);
            hs = HS_GET(pdata);

            if (cs != 0)
            {
                /* key was compressed, move what was compressed 
                   taking into account the compressed part*/
/*                xsize = pdataEntry[csOffset] - 1; */
                xsize = cs - 1;
/*                bufcop(pposEntry + entryHeaderSize,  */
/*                       pdata + entryHeaderSize + 1, xsize); */
                bufcop(pposEntry + hs, pdata + hs + 1, xsize);
            }

            /* update cs, ks, and is of 2nd entry */
            /* if the first byte of the uncompressed entry in pdata is
               null and the second entry has a compression of >= 1
               the second entry in the blk has a compression of 1 */
/*            if (pdata[entryHeaderSize] == 0 && pdataEntry[csOffset] >= (TEXT)1)  */
/*                pposEntry[csOffset] = 1; */
/*            else */
/*                pposEntry[csOffset] = 0; */
            if (hs == LRGENTHDRSZ)
                *pposEntry = LRGKEYFORMAT;
            else
                *pposEntry = 0;

            if (pdata[hs] == 0 && cs >= (TEXT)1)
	    {
                CS_PUT(pposEntry, 1);
            }
	    else
	    {
                CS_PUT(pposEntry, 0);
	    }

/*            pposEntry[ksOffset] = pdataEntry[csOffset] + pdataEntry[ksOffset] -  */
/*                                  pposEntry[csOffset]; */
            KS_PUT(pposEntry, cs + KS_GET(pdataEntry) - CS_GET(pposEntry));

/*            pposEntry[isOffset] = pdataEntry[isOffset]; */
            is = IS_GET(pdataEntry);
            IS_PUT(pposEntry, is);

            /* move what is left out of pdata back into the blk */
/*            bufcop(pposEntry + entryHeaderSize + xsize,  */
/*                   pdataEntry + entryHeaderSize, */
/*                   pcxnote->moveBufferSize - */
/*                   (tempSize + infoSize + entryHeaderSize)); */
            bufcop(pposEntry + HS_GET(pposEntry) + xsize, 
                   pdataEntry + hs,
                   pcxnote->moveBufferSize -
                   (tempSize + is + hs));
               
        }
        /* update the num entries and entries length */
        pcxblock->IXLENENT = pcxnote->lengthOfEntries;
        pcxblock->IXNUMENT += pcxnote->numMovedEntries;
    }
}
