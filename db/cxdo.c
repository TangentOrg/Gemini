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
#include "dbpub.h"
#include "rlpub.h"

#include "dbcontext.h"

#include "usrctl.h"
#include "cxmsg.h"
#include "keypub.h"     /* for constants LOWRANGE ... */
#include "cxpub.h"
#include "cxprv.h"
#include "cxdoundo.h"
#include "utspub.h"     /* for sct, xlng, etc */
#include "statpub.h"

/**** LOCAL FUNCTION PROTOTYPE DEFINITIIONS ****/

LOCALF int cxCharBinSearch( TEXT tofind, TEXT *pfirst, int tblsz );

/**** END LOCAL FUNCTION PROTOTYPE DEFINITIONS ****/

/*******************TEMPORARY FOR DEBUG
#define CXDEBUG 1
*****/

/************************************************************/
/** cxinsnt - insert a string into an index block          **/
/** cxremnt - remove a string from an index block          **/
/** cxbnote - b-image an entire block before deleting or   **/
/**             drastically modifying it (eg split)        **/
/************************************************************/

/***************************************************************/
/* PROGRAM: cxDoSplit -- split an overflowed index block in half 
 *
 * RETURNS: 0
 */

int
cxDoSplit(
        dsmContext_t    *pcontext,
	dsmTable_t table, /* Table number the index is on  */
        FINDP   *pfndp,   /* parameter block for the FIND */
        CXBAP   *pbap,    /* block access params at the next higher level. */
                          /* Its descrption is put here                    */
        int     leftents) /* number of entry left in the left block by split*/
{
        CXBLK     *pixblk;      /* the idx block to be split */
        TEXT      *pos;
        TEXT      *pend;
        COUNT      len1, len2;
        CXSPLIT    note;
        int        cs;
        TEXT    tmp[MAXDBBLOCKSIZE+MAXKEYSZ];

    pixblk = pfndp->pblkbuf;

    /* get a firm grip on both blocks */
    /**** the in use count guaranties that the old block is there */
    /* get new block - the right block  */
    cxGetNewBlock(pcontext, table, pfndp->pkykey->index,
		  pfndp->pkykey->area,pbap);

    /* decide the split point */
    pos = pfndp->position; /* split position */
    len1 = pos - pixblk->ixchars;

    len2 = pixblk->IXLENENT - len1;
    INITNOTE( note, RL_CXSP1, RL_PHY );
    INIT_S_CXSPLIT_FILLERS( note );	/* bug 20000114-034 by dmeyer in v9.1b */

    bufcop( &(note.ixhdr1), &(pixblk->ix_hdr), sizeof(note.ixhdr1) );
    bufcop( &(note.ixhdr2), &(pixblk->ix_hdr), sizeof(note.ixhdr2) );
    note.extracs = 0;

/*    if (len2 && *pos)    if key in split position is compressed */ 
    cs = CS_GET(pos);
    if (len2 && cs)     /* if key in split position is compressed */
    {

        /* the 1st entry in the new block is compressed, we must
        have the missing part in the note to allow decompression */
/*        note.extracs = *pos; */
        note.extracs = cs;
        pend = pixblk->ixchars + pixblk->IXLENENT;
        if ((MAXIXCHRS - pixblk->IXLENENT) < note.extracs)
        {
            /* no space in the block, use tmp area */
            bufcop(&tmp[0], pos, len2);
            bufcop(&tmp[0] + len2, pfndp->pinpkey, note.extracs);
            pos = &tmp[0];
        }
        else
        {
            /* there is space to attach the extra at end of the block data */
            bufcop(pend, pfndp->pinpkey, note.extracs);
        }
    }
    note.ixhdr1.ih_lnent = len1;
    note.ixhdr2.ih_lnent = len2 + note.extracs;
    note.ixhdr1.ih_nment = leftents;
    note.ixhdr2.ih_nment -= leftents;

    /* Do the original block */
    rlLogAndDo (pcontext, (RLNOTE *)&note, pfndp->bufHandle,
                 (COUNT)(len2+note.extracs), (TEXT *)pos);

    /* and then the new block */
    note.rlnote.rlcode = RL_CXSP2;
    rlLogAndDo (pcontext, (RLNOTE *)&note, pbap->bufHandle,
                 (COUNT)(len2+note.extracs), (TEXT *)pos);

    return(0);

}  /* cxDoSplit */


/*********************************************/
/* PROGRAM: cxClearBlock - wipe an index block clean
 *
 * RETURNS: DSMVOID
 */
/*ARGSUSED*/
DSMVOID
cxClearBlock(
        dsmContext_t    *pcontext,
        RLNOTE          *pnote _UNUSED_,
        BKBUF            *pixblk,
        COUNT           data_len _UNUSED_,
        TEXT            *pdata _UNUSED_)         /* Passed but not used */
{

    stnclr( (TEXT*)(&((CXBLK *)pixblk)->ix_hdr)
          , BLKSPACE(pcontext->pdbcontext->pdbpub->dbblksize)
          );

}  /* cxClearBlock */


/******************************************************/
/* PROGRAM: cxDeleteBlock - delete an index block wholesale
 *
 * RETURNS: 0
 */
int
cxDeleteBlock (
        dsmContext_t    *pcontext,
        bmBufHandle_t    bufHandle,
        UCOUNT           mode,
        DBKEY            rootdbk)
{
    dbcontext_t*  pdbcontext = pcontext->pdbcontext;
    IXDBNOTE note;
    CXBLK *pixblk;

    pixblk = (CXBLK *)bmGetBlockPointer(pcontext, bufHandle );
    /* if on the lock chain, cant put on free chain yet */
    if (pixblk->BKFRCHN == LOCKCHN)

    {   /* flag the block empty, cxDeleteLocks will delete the block later */
        return 0;
    }

    if ( pdbcontext->pidxstat &&
         (pdbcontext->pdbpub->isbaseshifting ==0 ) &&
         pixblk->IXNUM >= pdbcontext->pdbpub->argindexbase &&
         pixblk->IXNUM < pdbcontext->pdbpub->argindexbase +
                         pdbcontext->pdbpub->argindexsize )
        ((pdbcontext->pidxstat)[ pixblk->IXNUM  - 
                             pdbcontext->pdbpub->argindexbase ]).blockdelete++;

    /* fill in the note */
    ((RLNOTE*)&note)->rlcode = RL_IXDBLK;
    ((RLNOTE*)&note)->rllen = sizeof( IXDBNOTE );
    note.rootdbk = rootdbk;
    sct((TEXT *)&(((RLNOTE*)&note)->rlflgtrn),  mode);

    /* clear the block */
    rlLogAndDo(pcontext, (RLNOTE *)&note, bufHandle,
             (COUNT)(sizeof(pixblk->ix_hdr)+pixblk->IXLENENT),
             (TEXT *) &pixblk->ix_hdr);

    /* and return it to the free chain */
    bkfradd(pcontext, bufHandle, FREECHN);
    return 0;

}  /* cxDeleteBlock */


/*****************************************************/
/* PROGRAM: cxDoEndSplit - second half of index block split
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxDoEndSplit (
        dsmContext_t    *pcontext _UNUSED_,
        RLNOTE          *pIxNote,
        BKBUF           *pblock,
        COUNT            data_len,
        TEXT            *pdata)
{
    CXSPLIT      *pnote = (CXSPLIT *)pIxNote;
    CXBLK *pixblk = (CXBLK *)pblock;

    TEXT    *pos;
    int      extracs;
    int      hs, ks, is;

  bufcop ( &pixblk->ix_hdr, &pnote->ixhdr2, sizeof(pixblk->ix_hdr) );
  pixblk->IXTOP = 0;
  extracs = pnote->extracs;
  if(extracs)
  {
    int newhs;

    pos = pixblk->ixchars;
/* TOFIX: can't pdata be in a special format? what is it? a bi-note? then it
 *      should be 2 bytes complete length, 1 byte info-length. A regular key?
 *      then it should be possible to be a large key too.
 * I asume it's a regular index-entry (i.e. ENTHDRSZ or LRGENTHDRSZ)!
 */
/*    *pos++ = 0; */
/*    *pos++ = *(pdata+1) + extracs; */
/*    *pos++ = *(pdata+2); */

/*     the extra for the key expansion is at the end of the data */ 
/*    bufcop ( pos, pdata + data_len - extracs, extracs);  expand the key */ 
/*    pos += extracs; */
/*    bufcop ( pos, pdata + ENTHDRSZ, data_len - extracs - ENTHDRSZ ); */
    
    ks = KS_GET(pdata) + extracs;
    is = IS_GET(pdata);
    CS_KS_IS_SET( pos
		  , (ks > MAXSMALLKEY)
                , 0
                , ks 
                , is
                );
    newhs = HS_GET(pos);
    pos += newhs;
    /* the extra for the key expansion is at the end of the data */
    bufcop ( pos, pdata + data_len - extracs, extracs); /* expand the key */
    pos += extracs;
    hs = HS_GET(pdata);
    /* If header of the expanded key increases from small key format
       to large key format then add this difference into the bytes
       used count in the index block header.                       */
    if ( newhs > hs )
      pixblk->ix_hdr.ih_lnent += (newhs - hs);

    bufcop ( pos, pdata + hs, data_len - extracs - hs );
  }
  else
      bufcop ( pixblk->ixchars, pdata, data_len );

}  /* cxDoEndSplit */


/*********************************************************/
/* PROGRAM: cxDoDelete - remove an entry from an index block
 *
 * RETURNS: DSMVOID
 */
/*ARGSUSED*/
DSMVOID
cxDoDelete (
        dsmContext_t    *pcontext _UNUSED_,
        RLNOTE          *pIxNote,
        BKBUF           *pblock,
        COUNT            data_len _UNUSED_,
        TEXT            *pdata _UNUSED_)        /* Not used */
{
    CXNOTE      *pnote  = (CXNOTE *)pIxNote;
    CXBLK       *pixblk = (CXBLK *)pblock;

    TEXT        *pos    = (TEXT*)&pixblk->ixchars[0] + pnote->offset;
    TEXT        *delpos = pos;
    TEXT        *pend;
    TEXT        *posis;
    TEXT        *plist;
    UCOUNT       extrcmpr;
/*    UCOUNT       size, ks, is; */
    UCOUNT       size, ks, is, cs, hs;
    int          i;
    int	         smallDeletedKey;
#if CXDEBUG
    struct  bktbl   *pbktbl;
    struct  bftbl   *pbftbl;
#endif

/*    if ((pixblk->IXBOT) */
/*    && (pnote->cs == 0xff) && (*(pos+2) > (TEXT)1)) */
/*    {    leaf block, tag operation and more than 1 tag exists */ 
/*        pos++; */
/*        ks = *pos++; */
/*        posis = pos;  save position of is */ 
/*        is = *pos++; */
/*        pos += ks;       to info */ 
    CS_KS_IS_HS_GET(pos);
    if ( (pixblk->IXBOT)
      && (pnote->tagOperation == INDEX_TAG_OPERATION) 
      && (is > 1)
       )
    {   /* leaf block, tag operation and more than 1 tag exists */
        pos  += hs;
        posis = pos - 1; /* save position of is */
        pos  += ks;      /* to info */

        plist = pos;
        if (is == 33)
        {
            /* it is a bit map */
#ifdef VARIABLE_DBKEY
/*            cxDeleteBit(pos, pnote->sdbk[sizeof(DBKEY) - 1]);    set off the tag bit */ 
        	/* set off the tag bit */
            cxDeleteBit(pos, pnote->sdbk[sizeof(pnote->sdbk) - 1]);
#else
            cxDeleteBit(pos, pnote->sdbk[3]);   /* set off the tag bit */
#endif  /* VARIABLE_DBKEY */
            if (*pos == 32)
            {
                cxToList(pos);    /* convert from bit map to byte list */
                i = 32;
            }
            else
                goto done;
        }
        else
        {
                /* delete byte in its respective place in the list */
#ifdef VARIABLE_DBKEY
/*                i = cxCharBinSearch((TEXT)pnote->sdbk[sizeof(DBKEY) - 1], plist, is); */
                i = cxCharBinSearch((TEXT)pnote->sdbk[sizeof(pnote->sdbk) - 1], plist, is);
#else
                i = cxCharBinSearch((TEXT)pnote->sdbk[3], plist, is);
#endif  /* VARIABLE_DBKEY */
        }
        pos = plist + i;                    /* to insert position */
        bufcop(pos, pos + 1, pixblk->IXLENENT
/*                - (pnote->offset +ENTHDRSZ +ks +i));  make room for 1 byte */ 
                - (pnote->offset + hs + ks + i)); /* make room for 1 byte */

        pixblk->IXLENENT--;
        *posis = is - 1;
    }   /* leaf block, tag operation and more than 1 tag exists */
    else
    {   /* not leaf block or not  tag operation or only 1 tag exists */
        extrcmpr = pnote->extracs;
        /* Compute the length of the deletion */
        /* take into account that the next key may have to be expanded */
        /* caculate ks, is */
/*        ks = *(pos + 1); */
/*        is = *(pos + 2); */
/*        size = ENTHDRSZ + ks + is; length of the deleted entry.*/ 
        size = hs + ks + is; /* length of the deleted entry. */
        pos += size;    /* step to the next entry */

        pend = pixblk->ixchars + pixblk->IXLENENT;
        pixblk->IXLENENT -= size; /* update the used space */
        if (pixblk->IXLENENT != pnote->offset)
        {       /* there is a next entry */
            if (extrcmpr)
            {   /* next key's compression changed */
/*                *delpos++ = *pos++ - extrcmpr;   new CS */ 
/*                *delpos++ = *pos++ + extrcmpr;   new KS */ 
/*                *delpos++ = *pos++;              move is */ 
                /* 1. step: remember whether the deleted key was a smallkey
                 */
                smallDeletedKey = (hs == ENTHDRSZ);
                
                /* 2. step: get the new sizes (i.e. the sizes of the next key)
                 * and advance the pos-pointer */
                CS_KS_IS_HS_GET(pos);
                pos += hs;

                /* 3. step: if deleted key was small and next key is large
                 * of vice versa, we need to move the bytes to be decompressed
                 * a little bit
                 */
                if (smallDeletedKey > (hs == ENTHDRSZ))
                {       /* small deleted key, large next key */
                    bufcop( delpos+LRGENTHDRSZ, delpos+ENTHDRSZ, extrcmpr); 
                }
                else if (smallDeletedKey < (hs == ENTHDRSZ))
                {       /* large deleted key, small next key */
                    bufcop( delpos+ENTHDRSZ, delpos+LRGENTHDRSZ, extrcmpr);
                }

                /* 4. step: assign the new values to position of the deleted
                 * key - making sure to have the large header if needed. And 
                 * advance the delpos-pointer
                 */
                CS_KS_IS_SET(delpos,(hs==LRGENTHDRSZ)
                            ,cs - extrcmpr,ks + extrcmpr,is);
                delpos += (hs + extrcmpr); /* compr. was handled in step 3 */

		/* Rich's fix                 delpos += extrcmpr;  expand the prefix of the key */ 
                pixblk->IXLENENT += extrcmpr; /* update the used space */

            }   /* next key's compression changed */

            /* move the rest of the block to overlap the deleted entry */
            /* at this point delpos is the "to" address and pos is "from" */
            bufcop(delpos, pos, pend - pos);

        }       /* there is a next entry */

    }   /* not leaf block or not  tag operation or only 1 tag exists */

 done:
    pixblk->IXNUMENT--;

}  /* cxDoDelete */


/*********************************************************/
/* PROGRAM: cxDoInsert - insert an entry into an index block
 *
 * RETURNS: DSMVOID
 */
/*ARGSUSED*/
DSMVOID
cxDoInsert (
        dsmContext_t    *pcontext _UNUSED_,
        RLNOTE          *pIxNote,
        BKBUF           *pblock,
        COUNT           data_len _UNUSED_,
        TEXT            *pdata)
{
    CXNOTE      *pnote = (CXNOTE *)pIxNote;
    CXBLK *pixblk = (CXBLK *)pblock;

    TEXT         *pos = (TEXT*)&pixblk->ixchars[0] + pnote->offset;
    TEXT         *posis;
    TEXT         *plist;
    UCOUNT       extrcmpr;
    UCOUNT       size, cs, ks, is, hs;
    int          i;

/*    if ((pixblk->IXBOT) */
/*    && (pnote->cs == 0xff)) */
/*    {    leaf block and tag operation */ 
/*        pos++; */
/*        ks = *pos++; */
/*        posis = pos;  save position of is */ 
/*        is = *pos++; */
/*    	pos += ks;	 to info */ 
    if ( (pixblk->IXBOT)
      && (pnote->tagOperation == INDEX_TAG_OPERATION) ) 
    {   /* leaf block and tag operation */
        CS_KS_IS_HS_GET(pos);
        pos  += hs;
        posis = pos - 1; /* save position of is */
        pos  += ks;      /* to info */

        if (is == 33)
        {
#ifdef VARIABLE_DBKEY
/*            cxAddBit(pos, pnote->sdbk[sizeof(DBKEY) - 1]);      insert the tag as a bit */ 
            cxAddBit(pos, pnote->sdbk[sizeof(pnote->sdbk) - 1]);      /*insert the tag as a bit */
#else
            cxAddBit(pos, pnote->sdbk[3]);      /*insert the tag as a bit */
#endif  /* VARIABLE_DBKEY */
        }
        else
        {  /* info not bit-map */
            /* add byte in its respective place in the list */
            plist = pos;
#ifdef VARIABLE_DBKEY
/*            i = cxCharBinSearch((TEXT)pnote->sdbk[sizeof(DBKEY) - 1], plist, is); */
            i = cxCharBinSearch((TEXT)pnote->sdbk[sizeof(pnote->sdbk) - 1], plist, is);
#else
            i = cxCharBinSearch((TEXT)pnote->sdbk[3], plist, is);
#endif  /* VARIABLE_DBKEY */
            pos = plist + i;                    /* to insert position */
            bufcop(pos + 1, pos, pixblk->IXLENENT
/*                - (pnote->offset +ENTHDRSZ +ks +i));  make room for 1 byte */ 
                - (pnote->offset + hs + ks + i)); /* make room for 1 byte */

#ifdef VARIABLE_DBKEY
/*            *pos = pnote->sdbk[sizeof(DBKEY) - 1]; */
            *pos = pnote->sdbk[sizeof(pnote->sdbk) - 1];
#else
            *pos = pnote->sdbk[3];
#endif  /* VARIABLE_DBKEY */
            *posis = is + 1;
            pixblk->IXLENENT++;

            if (is == 32) /* was 32 and became 33 now */
            {
                /*convert byte list to a 32 bytes bit map
                preceeded by a bit count*/
                cxToMap(plist);
            }

        }  /* info not bit-map */

    }   /* leaf block and tag operation */
    else
    {   /* not leaf block or not tag operation */
        cs  = pnote->cs;  /* how much to compress */
/*        is = pdata[2]; */
        is = xct(&pdata[IS_OFFSET]);
        ks = xct(&pdata[KS_OFFSET]) - cs;
        extrcmpr = pnote->extracs;
/*        size = pdata[1] + is + ENTHDRSZ - cs; */
        hs = (ks <= MAXSMALLKEY) ? ENTHDRSZ : LRGENTHDRSZ;
        size = ks + is + hs;

        if (pixblk->IXLENENT != pnote->offset)
        {
            if (extrcmpr)
            {   /* next entry is to be compressed some more */
/*                *(pos + 2 + extrcmpr) = *(pos + 2);  move IS */ 
/*                *(pos + 1 + extrcmpr) = *(pos + 1) - extrcmpr; new KS */ 
/*                *(pos + extrcmpr) = *pos + extrcmpr;   new CS */ 
                if (*pos == LRGKEYFORMAT)
                {
		            *(pos + 5 + extrcmpr) = *(pos + 5); /* move IS */
                    sct(pos + 3 + extrcmpr, xct(pos + 3) - extrcmpr); /* KS */
                    sct(pos + 1 + extrcmpr, xct(pos + 1) + extrcmpr); /* CS */
		            *(pos + extrcmpr) = *pos; /* move LRGKEYFORMAT */
                }
                else
                {
		            *(pos + 2 + extrcmpr) = *(pos + 2); /* move IS */
		            *(pos + 1 + extrcmpr) = *(pos + 1) - extrcmpr;/* new KS */
		            *(pos + extrcmpr) = *pos + extrcmpr;  /* new CS */
                }
            }   /* next entry is to be compressed some more */
        }

        /* make space for the insertion */
        bufcop(pos+size, pos+extrcmpr,
                        pixblk->IXLENENT - pnote->offset - extrcmpr);


        if ( ks <= MAXSMALLKEY )
        {
            *pos++ = (TEXT)cs;                    /* cs of inserted key */
            *pos++ = (TEXT)ks;                    /* ks = key size - cs */
            *pos++ = (TEXT)is;
        }
        else
        {
            /* set key's sizes and forward pointer */
            *pos = LRGKEYFORMAT;
            sct(pos + 1,cs);
            sct(pos + 3,ks);
            pos[5]=is;
            pos += hs;
        }
	/* insert the key */
	bufcop(pos, &pdata[FULLKEYHDRSZ] + cs, ks);
        pos += ks;   /* point to info */
             
        /* insert the data */

#ifdef VARIABLE_DBKEY
/*           bufcop(pos, &pnote->sdbk[sizeof(DBKEY) - is], is); */
            bufcop(pos, &pnote->sdbk[sizeof(pnote->sdbk) - is], is);
#else
            bufcop(pos, &pnote->sdbk[4 - is], is);
#endif  /* VARIABLE_DBKEY */

        pixblk->IXLENENT += size - extrcmpr;
    }
    pixblk->IXNUMENT++;

}  /* cxDoInsert */


/****************************************************/
/* PROGRAM: cxDoMasterInit - init a new index master block
 *
 * RETURNS: DSMVOID
 */
/*ARGSUSED*/
DSMVOID
cxDoMasterInit (
        dsmContext_t    *pcontext _UNUSED_,
        RLNOTE          *pIxNote,
        BKBUF           *pblock,
        COUNT            dlen _UNUSED_,       /* Not used */
        TEXT            *pdata _UNUSED_)      /* Not used */
{
    IXNEWM    *pnote = (IXNEWM *)pIxNote; 
    CXBLK     *pixblk = (CXBLK *)pblock;

    pixblk->IXTOP       = 1;
    pixblk->IXBOT       = pnote->ixbottom;
    pixblk->IXNUM       = pnote->ixnum;
    pixblk->ix_hdr.ih_lockCount = -1;
    /*** the new index DUMMY entry */
    pixblk->IXLENENT    = ENTHDRSZ;     /* dummy entry is 3 bytes at the leaf */
    pixblk->IXNUMENT    = 1;    /* count the dummy entry */
    pixblk->ixchars[0]  = 0;    /* cs */

    *(pixblk->ixchars+1) = 0;   /* ks */
    *(pixblk->ixchars+2) = 0;   /* length of dbkey part */

}  /* cxDoMasterInit */


/****************************************************/
/* PROGRAM: cxDoStartSplit - first half of index block split
 *
 * RETURNS: DSMVOID
 */
/*ARGSUSED*/
DSMVOID
cxDoStartSplit (
        dsmContext_t    *pcontext _UNUSED_,
        RLNOTE          *pnote,
        BKBUF           *pixblk,
        COUNT            data_len _UNUSED_,
        TEXT            *pdata _UNUSED_)        /* Not used */
{

  ((CXBLK *)pixblk)->IXLENENT = ((CXSPLIT *)pnote)->ixhdr1.ih_lnent;
  ((CXBLK *)pixblk)->IXNUMENT = ((CXSPLIT *)pnote)->ixhdr1.ih_nment;
  ((CXBLK *)pixblk)->IXTOP = 0;

}  /* cxDoStartSplit */


/*********************************************************/
/* PROGRAM: cxInvalid - TEMPORARY for new index debugging
 *
 * RETURNS: DSMVOID
 */
/*ARGSUSED*/
DSMVOID
cxInvalid(
        dsmContext_t    *pcontext,
        RLNOTE          *pnote _UNUSED_,
        BKBUF           *pixblk _UNUSED_,
        COUNT            dlen _UNUSED_,       /* Not used */
        TEXT            *pdata _UNUSED_)     /* Not used */
{

   /* do not make this a MSGN_CALLBACK it is TEMPORARY */
   /* MSGD_CALLBACK(pcontext, G invalid rl note "); */
   FATAL_MSGN_CALLBACK(pcontext, cxFTL005);

}  /* cxInvalid */


/*******************************************/
/* PROGRAM: cxUndoClearBlock - undo index block wipe
 *
 * RETURNS: DSMVOID
 */
/*ARGSUSED*/
DSMVOID
cxUndoClearBlock (
        dsmContext_t    *pcontext _UNUSED_,
        RLNOTE          *pnote _UNUSED_,
        BKBUF           *pixblk,
        COUNT            data_len,
        TEXT            *pdata)
{

  bufcop (&((CXBLK *)pixblk)->ix_hdr, pdata, data_len );

}  /* cxUndoClearBlock */


/*********************************************************/
/* PROGRAM: cxUndoStartSplit - undo first half of index block split
 *
 * RETURNS: DSMVOID
 */
/*ARGSUSED*/
DSMVOID
cxUndoStartSplit (
        dsmContext_t    *pcontext _UNUSED_,
        RLNOTE          *pIxNote,
        BKBUF           *pblock,
        COUNT            data_len _UNUSED_,
        TEXT            *pdata)
{
    CXSPLIT     *pnote = (CXSPLIT *)pIxNote;
    CXBLK       *pixblk = (CXBLK *)pblock;
         
    TEXT        *pos;

    pixblk->IXLENENT += pnote->ixhdr2.ih_lnent - pnote->extracs;
    pixblk->IXNUMENT += pnote->ixhdr2.ih_nment;
    pos = pixblk->ixchars + pnote->ixhdr1.ih_lnent;
    bufcop (pos, pdata, pnote->ixhdr2.ih_lnent - pnote->extracs);

    pixblk->IXTOP = pnote->ixhdr1.ih_top;    /* in case it was a root split */

}  /* cxUndoStartSplit */


/*********************************************************/
/* PROGRAM: cxCharBinSearch - binary search for a CHAR value
 *                      in an array characters.
 *
 * RETURNS: index of the found value - if found
 *      or the index where to insert the value - if not found.
 *                      
 */
/*ARGSUSED*/
LOCALF int
cxCharBinSearch (
        TEXT     tofind, /* value to search for */
        TEXT    *pfirst, /* first value in table to be compared */
        int      tblsz)  /* number of entries in the searched table */


{
FAST    TEXT   *pi;
FAST    int     lo;
FAST    int     hi;
FAST    int     i;

    lo = 0;

    hi = tblsz;

    /* binary search */
    while((hi - lo) >= 1)
    {
        i = (hi + lo) >> 1; /* split in a half */
        pi = pfirst + i;
        if (tofind == *pi)
        {
            return i;
        }
        if (tofind < *pi)
            hi = i;
        else
            lo = i + 1;
    }
    i = (hi + lo) >> 1; /* split in a half */
    return i;

}  /* cxCharBinSearch */


/*********************************************************/

/*********************************************************
 * PROGRAM: cxDoCompactLeft
 *
 * Returns: 
 *
 */
DSMVOID
cxDoCompactLeft(dsmContext_t *pcontext _UNUSED_,
                RLNOTE       *pIxNote,      /* cx compact note */
                BKBUF        *pblock,       /* cx block to be modified */
                COUNT         dataLength _UNUSED_, /* length of data to add */
                TEXT         *pdata)        /* data to add to blk */
{
    cxCompactNote_t *pCxNote   = (cxCompactNote_t *)pIxNote;
    cxblock_t       *pCxBlock  = (cxblock_t *)pblock;
    TEXT            *pPosition;
/*    int              entryHeaderSize = ENTHDRSZ, */
/*                     csOffset = 0, */
/*                     ksOffset = 1, */
/*                     isOffset = 2; */
    int              hs, cs, ks, is;
    

    /* set the position at which to add */
    pPosition = pCxBlock->ixchars + pCxNote->offset;

    /* put in cs, advance, put in ks, advance, put in the
       rest of the buffer, without the compressed out part of
       the first key */
/*    pPosition[csOffset] = pCxNote->cs; */
/*    pPosition[ksOffset] = pdata[ksOffset] - pCxNote->cs;  reset ks */ 
/*    pPosition[isOffset] = pdata[isOffset]; */

    CS_KS_IS_HS_GET(pdata);
    cs = pCxNote->cs;
    CS_KS_IS_SET(pPosition,(hs == LRGENTHDRSZ),cs,(ks - cs),is)


/*    bufcop(pPosition + entryHeaderSize, */
/*           pdata + entryHeaderSize + pCxNote->cs,  */
/*           (pCxNote->moveBufferSize - entryHeaderSize - pCxNote->cs)); */
    bufcop(pPosition + hs,
           pdata + hs + cs, 
           (pCxNote->moveBufferSize - hs - cs));
    
/*    pCxBlock->IXLENENT += (pCxNote->moveBufferSize - pCxNote->cs); */
    pCxBlock->IXLENENT += (pCxNote->moveBufferSize - cs);
    pCxBlock->IXNUMENT += pCxNote->numMovedEntries;

}

/*********************************************************
 * PROGRAM: cxDoCompactRight
 *
 * Returns: 
 *
 */
DSMVOID
cxDoCompactRight(dsmContext_t *pcontext _UNUSED_,
                 RLNOTE       *pIxNote,      /* cx compact note */
                 BKBUF        *pblock,       /* cx block to be modified */
                 COUNT         dataLength _UNUSED_, /* length of data to add */
                 TEXT         *pdata)        /* data to add to blk */
{
    cxCompactNote_t *pCxNote   = (cxCompactNote_t *)pIxNote;
    cxblock_t       *pCxBlock  = (cxblock_t *)pblock;
    TEXT            *pPosition,
                    *pOffsetPosition;
    int              offsetKS,          /* key size of key remaining */
                     offsetCS,          /* cs of key remaining */
                     offsetEntrySize,   /* size of entry at offset */
                     firstEntrySize;    /* size of first entry in block */
    findp_t          findp;             /* find param structures for */
    LONG             lengthRemaining;   /* size of entries left to be moved */

/*        int      entryHeaderSize = ENTHDRSZ, */
/*                 csOffset = 0, */
/*                 ksOffset = 1, */
/*                 isOffset = 2; */
    int             hs, cs, ks, is, is1, hs0, delta;


    INIT_S_FINDP_FILLERS( findp ); /* bug 20000114-034 by dmeyer in v9.1b */

    /* get pointer to beginning of first entry and the beginning of
       the entry at the offset */
    pPosition = pCxBlock->ixchars;
    pOffsetPosition = pPosition + pCxNote->offset;
    
    /* the amount remaining in the block starting at the beginning of
       the first entry after the offset */
    lengthRemaining = pCxBlock->IXLENENT - pCxNote->offset;

    /* determine if this is a leaf or non-leaf blk */
    if (pCxBlock->IXBOT)
    {
        /* this is a leaf blk */
        /* move the remaing part of the right entry and the remaining 
           entries and then move the compressed part of the key */
/*        bufcop(pPosition + entryHeaderSize + pCxNote->cs,  */
/*               pOffsetPosition + entryHeaderSize, */
/*               lengthRemaining - entryHeaderSize); */
        hs0 = HS_GET(pPosition);
        CS_KS_IS_HS_GET(pOffsetPosition);
        cs = pCxNote->cs;
        bufcop(pPosition + hs + cs, 
               pOffsetPosition + hs,
               lengthRemaining - hs);

/*        bufcop(pPosition + entryHeaderSize, */
        bufcop(pPosition + hs,
               pdata + pCxNote->moveBufferSize,
/*               pCxNote->cs); */
               cs);

        /* set the size of the key uncompressed, and move the entry header */
/*        pPosition[csOffset] = 0; */
/*        pPosition[ksOffset] = pOffsetPosition[ksOffset] + pCxNote->cs; */
/*        pPosition[isOffset] = pOffsetPosition[isOffset]; */
        CS_KS_IS_SET(pPosition,(hs == LRGENTHDRSZ),0,(ks + cs),is)

        
        /* update the size and number of entries in the block */
/*        pCxBlock->IXLENENT -= (pCxNote->offset - pCxNote->cs); */
        pCxBlock->IXLENENT = lengthRemaining + cs + hs -hs0;
        pCxBlock->IXNUMENT -= pCxNote->numMovedEntries;
    }
    else
    {
        /* this is a non-leaf block */
#if 0   /* non large keys */
        /* store key size for first remaining entry a  */
        firstEntrySize = ENTRY_SIZE_COMPRESSED(pPosition);
        offsetEntrySize = ENTRY_SIZE_COMPRESSED(pOffsetPosition);

        /* update the old info with the new info */
        bufcop(pPosition + entryHeaderSize + pPosition[ksOffset],
               pOffsetPosition + entryHeaderSize + pOffsetPosition[ksOffset],
               pOffsetPosition[isOffset]);
#endif   /* non large keys */

        /* the 1st key is always small, all others may be large keys */
        firstEntrySize = ENTRY_SIZE_COMPRESSED(pPosition);
        CS_KS_IS_HS_GET(pOffsetPosition);  /* of the offset position */


        if ((pCxBlock->IXNUMENT - pCxNote->numMovedEntries) > 1)
        {
/*          pOffsetPosition += offsetEntrySize; */
            offsetEntrySize = hs + ks + is;

            /* the new 1st entry may be different than the old since their is and hs may change */
            delta = hs + is - pPosition[0] - pPosition[2];  /* size difference */
            if (delta > 0 && (pCxNote->offset + offsetEntrySize) < (firstEntrySize + delta))
            {
                /* need to make space for the new larger 1st entry */
                bufcop(pOffsetPosition + delta, pOffsetPosition,
                        pCxBlock->IXLENENT - pCxNote->offset - offsetEntrySize);
                pOffsetPosition += delta;
                firstEntrySize += delta;
            }
            /* update the old info with the new info */
            bufcop(pPosition + ENTHDRSZ + pPosition[1], pOffsetPosition + hs + ks, is);

            /* move to the next entry after pPositon and pOffsetPosition */
            pPosition += firstEntrySize + delta;
            pOffsetPosition += offsetEntrySize;

            /* update the cs and ks in the second entry in the blk.  This will
               start to to tranform the existing second entry to be the new 
               second entry */

            /* The key in the first entry of a block on a non-leaf level
               is always null.  Check it see if the first byte of the 
               uncompressed key at the offset is null and if so compress it 
               out for the "new" second entry. */
/*            if (pCxBlock->ixchars[entryHeaderSize] ==  */
            if (hs == LRGENTHDRSZ)
                pPosition[0] = LRGKEYFORMAT;
            else
                pPosition[0] = 0;

            if (pCxBlock->ixchars[ENTHDRSZ] ==  /* always small key */

                                 pdata[pCxNote->moveBufferSize])
            {
                CS_PUT(pPosition, 1); /* cs = 1 */
            }
            else
            {
                CS_PUT(pPosition, 0); /* cs = 0 */
            }
/*            offsetCS = pOffsetPosition[csOffset]; */
            offsetCS = CS_GET(pOffsetPosition);
/*            offsetKS = pOffsetPosition[ksOffset]; */
            offsetKS = KS_GET(pOffsetPosition);
/*            pPosition[ksOffset] = offsetCS + offsetKS - pPosition[csOffset]; */
            cs = CS_GET(pPosition);
            KS_PUT(pPosition, offsetCS + offsetKS - cs );
/*            pPosition[isOffset] = pOffsetPosition[isOffset]; */

	    is1 = IS_GET(pOffsetPosition);
            IS_PUT(pPosition, is1 /*IS_GET(pOffsetPosition)*/ );

        
            /* is the offset key compressed? */
            if (offsetCS == 0)
            {
                /* this was an uncompress key */
                /* move compressed part of key, info and remaining entries */
/*                bufcop(pPosition + entryHeaderSize,  */
/*                        pOffsetPosition + entryHeaderSize + pPosition[csOffset],  */
/*                       lengthRemaining - offsetEntrySize - entryHeaderSize); */
                bufcop(pPosition + HS_GET(pPosition), 
                        pOffsetPosition + HS_GET(pOffsetPosition) + cs,
                       lengthRemaining - offsetEntrySize - (HS_GET(pOffsetPosition) + cs));
                        

                /* update the size and number of entries in the block */        
                pCxBlock->IXLENENT -= (pCxNote->offset + offsetEntrySize - delta);
                pCxBlock->IXLENENT += firstEntrySize;
            }
            else
            {

                /* move compressed part of key, info and remaining entries */
/*                bufcop(pPosition + entryHeaderSize + (offsetCS - pPosition[csOffset]),  */
                bufcop(pPosition + HS_GET(pPosition) + (offsetCS - cs), 
/*                       pOffsetPosition + entryHeaderSize,  */
                       pOffsetPosition + HS_GET(pOffsetPosition), 
/*                       lengthRemaining - offsetEntrySize - entryHeaderSize); */
                       lengthRemaining - offsetEntrySize - HS_GET(pOffsetPosition));

                /* finish uncompressing the key in the second entry using
                   the uncompressed copy of the key from pdata */
                bufcop(pPosition + HS_GET(pPosition), 
                       pdata + pCxNote->moveBufferSize + cs,
                       offsetCS - cs);

                /* update the size and number of entries in the block */        
                pCxBlock->IXLENENT -= (pCxNote->offset + offsetEntrySize - delta);
                pCxBlock->IXLENENT += (firstEntrySize + (offsetCS - cs));
            }
        }
        else
        {
           /* there is only one entry left, set the total length  
              of entries in the blk to the lenght of that entry */

            /* update the old info with the new info */
            bufcop(pPosition + ENTHDRSZ + pPosition[1], pOffsetPosition + hs + ks, is);
            pPosition[2] = is;
            pCxBlock->IXLENENT = ENTRY_SIZE_COMPRESSED(pPosition);
        }
        pCxBlock->IXNUMENT -= (pCxNote->numMovedEntries);
    }    
}
