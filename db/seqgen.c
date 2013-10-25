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

#include "bkpub.h"  /* for BKBUF & to satisfy cxprv.h & rlmgr.h */
#include "bmpub.h"  /* for bmReleaseBuffer & bmLocateBlock calls */
#include "cxpub.h"  /* for FINDFRST */
#include "cxprv.h"  /* for CXNEXT constant */
#include "dbpub.h"  /* for ENDLOOP */
#include "dbcontext.h"  /* for p_dbctl GLOBAL */
#include "ditem.h"  /* needed for AUTODITM, INITDITM ... */
#include "dsmpub.h"
#include "ixmsg.h"
#include "kypub.h"  /* needed for kyinitdb, kyinsdb */
#include "keypub.h"
#include "latpub.h" /* needed for MT_LATCH_SEQ */
#include "lkpub.h"  /* needed for LKNOLOCK */
#include "recpub.h" /* needed for recRecordNew, recPut*, ... */
#include "rlpub.h"  /* for RLNOTE */
#include "seqpub.h"
#include "seqprv.h"
#include "sedoundo.h"
#include "semsg.h"
#include "stmpub.h"
#include "stmprv.h"
#include "usrctl.h"
#include "utspub.h"  /* needed for bfx call */
#include "uttpub.h" /* needed for hextoa */

#ifdef SETRACE
#define MSGD MSGD_CALLBACK
#endif

#ifndef FDDUPKEY
#define FDDUPKEY        -1235   /* record with that key already exists   */
#endif

/*** LOCAL FUNCTION PROTOTYPES ***/

LOCALF seqTbl_t *	seqFind		(dsmContext_t *pcontext,
					 seqCtl_t *pseqctl, TEXT *pname);

LOCALF seqTbl_t *	seqFindId	(dsmContext_t *pcontext,
					seqCtl_t *pseqctl, DBKEY recid);

LOCALF dsmStatus_t	seqLoad		(dsmContext_t *pcontext,
					seqTbl_t *pseqtbl, DBKEY recid);

/*** END LOCAL FUNCTION PROTOTYPES ***/


/* 256 byte lookup table for hashing. Each value from 0 to 255 occurs exactly
   once, in randomized order */

LOCAL UTEXT hashtable[] = {
 41, 223, 252, 167,  87,  75, 120,   9,  63, 142, 250, 141, 103,  12, 153, 170,
226, 158, 134, 109, 118, 228, 140, 145,  62,  45, 115, 204, 247, 221,  78,  69,
146,  60, 135, 126, 107, 218, 110, 148,  67, 159, 100,  52, 180,  71,   7, 183,
 46,  15, 182, 157, 172, 248, 123,  42, 104, 240, 214,   4, 219, 194, 190,  43,
212, 143,  99,  83,  36,  21, 229, 227, 155, 152,  29,  92, 197, 198,  58, 242,
130,  48,  56, 147,  68,   8,  11,  94, 119, 112, 114, 209, 128, 127, 251, 178,
222,  20, 171, 216, 237,  49,   5, 116,  70,  98,  19, 144, 236, 230, 206, 203,
 81, 245,  85, 213, 217,  10,  32,  93, 215,   3, 163, 173, 124,  80,  47, 117,
136, 210,  57, 101, 249,  77,  86, 108, 175, 111, 132,   6,  39, 162, 243, 177,
205, 105,  13,  27, 254,  14, 133, 191,  53,   1, 207,  17,  59, 241,  40,  37,
185,  26,  84, 106, 122, 169, 125, 186,  64, 211,  28, 233, 161, 246, 188,  34,
238, 139,  91, 224,  31,  22, 196,  76,  55, 138, 220, 151,  38,  25, 168,  73,
232, 165,  82, 156, 244, 154,  35, 225,  97, 176, 234,   2, 137,  74,  72, 149,
102,  96,  54, 166, 174, 235,  65, 187, 255,  16, 201, 199, 202, 253, 189, 164,
150, 113, 193, 195,  66,  44, 184, 160,  24,   0,  51,  61, 231, 179,  95, 239,
129, 181, 200,  89, 192,  79,  50,  23,  88,  33,  90,  30, 208, 131, 121,  18,
};


/* *********************************************************************
   *********************************************************************

 	miscellaneous private utility functions

   *********************************************************************
   ********************************************************************* */


/* PROGRAM: seqFind - find the corresponding entry in the sequence cache
 *		given a sequence name.
 *
 * RETURNS: NULL if no match
 *	    pointer to setbl entry if found
*/
LOCALF seqTbl_t *
seqFind (
	dsmContext_t	*pcontext, 
	seqCtl_t	*pseqctl,
	TEXT		*pname)
{
	seqTbl_t *pseqtbl;
	UCOUNT	  hash;
        /* declaration of "n" changed from UCOUNT to COUNT */
        /* for bug 20000315-023*/
        /* since seq_hash[] is initialized to -1 for unused sequences*/
        /* assigning n = pseqctl->seq_hash[hash] sets n to 65535, causing*/
        /* pseqtbl to point to garbage. */
	COUNT	  n; 

    hash = seqHashString (pcontext, pname, stlen (pname), SEQHASHP);

    for (n = pseqctl->seq_hash[hash] ; n != SEQINVALID; n = pseqtbl->seq_nxhash)
    {
	/* get entry address */

        pseqtbl = pseqctl->seq_setbl + n;

        if ((pseqtbl->seq_recid) && (pseqtbl->seq_vhash == (COUNT)hash))
	{
            /* hash values match. check name match to be sure */
	
            if (stcomp (pseqtbl->seq_name, pname) == 0)
	    {
#ifdef SETRACE
MSGD(pcontext, "%LseqFind: seq %i recid %D %s",
      (int)pseqtbl->seq_num, pseqtbl->seq_recid, pseqtbl->seq_name);
#endif
		return (pseqtbl);
	    }
	}
    }
    /* entry not found */

    return ((seqTbl_t *)PNULL);

}  /* end seqFind */


/* PROGRAM: seqFindId - given a _Sequence recid, find the corresponding entry
 *		     in the sequence cache
 *
 * RETURNS: NULL if no match
 *	    pointer to setbl entry if found
 */
LOCALF seqTbl_t *
seqFindId (
	dsmContext_t	*pcontext,
	seqCtl_t	*pseqctl,
	DBKEY		 recid)
{
	seqTbl_t *pseqtbl;
	COUNT	  n;

    pseqtbl = pseqctl->seq_setbl;
    for (n = MAXSEQVALS(pcontext->pdbcontext->pdbpub->dbblksize) ; n > (COUNT)0; n--)
    {
	if (pseqtbl->seq_recid == recid)
	{
	    /* found it */

#ifdef SETRACE
MSGD(pcontext, "%LseqFindId: seq %i recid %D name %s",
      (int)pseqtbl->seq_num, pseqtbl->seq_recid, pseqtbl->seq_name);
#endif
	    return (pseqtbl);
	}
	pseqtbl++;
    }
    /* entry not found */

    return ((seqTbl_t *)PNULL);

}  /* end seqFindId */


/* PROGRAM: seqFindNum - given a _Sequence seq_num, find the corresponding entry
 *		     in the sequence cache
 *
 * RETURNS: NULL if no match
 *	    pointer to setbl entry if found
 */
LOCALF seqTbl_t *
seqFindNum (
	dsmContext_t	*pcontext,
	seqCtl_t	*pseqctl,
	COUNT	 	num)
{
	seqTbl_t *pseqtbl;
	COUNT	  n;

    pseqtbl = pseqctl->seq_setbl;
    for (n = MAXSEQVALS(pcontext->pdbcontext->pdbpub->dbblksize) ; n > 0; n--)
    {
	if (pseqtbl->seq_num == num)
	{
	    /* found it */

#ifdef SETRACE
MSGD(pcontext, "%LseqFindNum: seq %i recid %D name %s",
      (int)pseqtbl->seq_num, pseqtbl->seq_recid, pseqtbl->seq_name);
#endif
	    return (pseqtbl);
	}
	pseqtbl++;
    }
    /* entry not found */

    return ((seqTbl_t *)PNULL);

}  /* end seqFindNum */


/* PROGRAM: seqGetCtlSize - Return the size of private structure seqCtl
 *          This is being done to preserve this structure's privacy.
 *
 * RETURNS: size of the seqCtl_t structure (as a LONG)
 */
LONG
seqGetCtlSize (dsmContext_t *pcontext)
{
    return (sizeof(seqCtl_t) +
            (sizeof(seqTbl_t) *
                  (MAXSEQVALS(pcontext->pdbcontext->pdbpub->dbblksize))));

}  /* end seqGetCtlSize */


/* PROGRAM: seqHashString - Given a string & its length, compute a hash function
 *
 * RETURNS: returns a value in the range 0 to modulus -1 inclusive
 */
UCOUNT
seqHashString (
	dsmContext_t	*pcontext _UNUSED_,
	TEXT		*pbuf,		/* pointer to buffer */
	int		 len,		/* length of buffer */
	COUNT		 modulus)	/* hash modulus */
{
	UTEXT	*s;
	UTEXT	 h1;
	UTEXT 	 h2;

    if (len == 0) return (0);
    s = pbuf;

    /* pick two places to start */

    h1 = 0;
    h2 = 31;

    /* walk through the array using the byte values to direct the path */

    while (len--)
    {
	h1 = hashtable[ h1 ^ *s ];
	h2 = hashtable[ h2 ^ *s ];
	s++;
    }
    return ((UCOUNT)(h1 + (h2 << 8)) % (UCOUNT)modulus);

}  /* end seqHashString */


/* PROGRAM: seqInsert - insert an setbl entry into the name lookup table
 *
 * RETURNS: DSMVOID
 */
DSMVOID
seqInsert (
	dsmContext_t	*pcontext,
	seqCtl_t	*pseqctl,
	seqTbl_t	*pseqtbl)
{
	   UCOUNT	 hash;

    hash = seqHashString (pcontext, pseqtbl->seq_name,
                          stlen (pseqtbl->seq_name), SEQHASHP);

    pseqtbl->seq_vhash = hash;
    pseqtbl->seq_nxhash = pseqctl->seq_hash[hash];
    pseqctl->seq_hash[hash] = pseqtbl->seq_num;

#ifdef SETRACE
MSGD(pcontext, "%LseqInsert: seq %i hash %i next %i name %s",
      (int)pseqtbl->seq_num, (int)pseqtbl->seq_vhash, (int)pseqtbl->seq_nxhash,
      pseqtbl->seq_name);
#endif

}  /* end seqInsert */


/* PROGRAM: seqLoad - Load data from the record.
 *		Given a recid of a _Sequence record and an setbl
 *		entry, load the data from the record.
 *
 * RETURNS: 0 if successful
 *          DSM_S_RMNOTFND if dsmRecordGet fails to find record 
 *          1 otherwise
*/
LOCALF dsmStatus_t
seqLoad (
	dsmContext_t	*pcontext,
	seqTbl_t	*pseqtbl,
	DBKEY	  	 recid)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t  ret;
    int          badvals = 0;
    dsmRecord_t  record;
    dsmBuffer_t  *prec;
    DBKEY  	 dbrecid;

    svcByteString_t byteStringField;
    ULONG indicator;

    pseqtbl->seq_nxhash = SEQINVALID;
    pseqtbl->seq_recid = recid;


    /* read the record out of the database */
    record.table = SEQFILE;
    record.recid = recid;
    record.maxLength = 100;
    record.pbuffer = (dsmBuffer_t *)stRent(pcontext,
                                        pdbcontext->privpool, record.maxLength);

    for (;;)
    {
	ret = dsmRecordGet(pcontext,&record,0);

        if (ret == DSM_S_RECORD_BUFFER_TOO_SMALL)
	{
	    /* buffer was too small */
            stVacate(pcontext, pdbcontext->privpool, record.pbuffer);
	    record.maxLength = record.recLength;
            record.pbuffer = (dsmBuffer_t *)stRent(pcontext,
                                                   pdbcontext->privpool,
                                                   record.maxLength);
	    continue;
        }
        else if (ret < 0)
        {
            stVacate(pcontext, pdbcontext->privpool, record.pbuffer);
	    MSGN_CALLBACK(pcontext, seMSG002, ret, recid);
	    return (ret);
        }
	break;
    }

    prec = record.pbuffer;

    /* extract the db-recid from the record */
    if ( recGetLONG(prec, SEQDBKEY,(ULONG)0, (LONG *)&dbrecid, &indicator) )
        goto badret;

    /* only load the records for this database */
    if (dbrecid != pcontext->pdbcontext->pmstrblk->mb_dbrecid)
    {
        stVacate(pcontext, pdbcontext->privpool, record.pbuffer);
        return (DSM_S_RMNOTFND);
    }

    /* sequence number value slot in sequence block */
    if ( recGetSMALL(prec, SEQNUM,(ULONG)0, &pseqtbl->seq_num, &indicator) )
	goto badret;

    /* get the sequence name */
    byteStringField.pbyte = pseqtbl->seq_name;
    byteStringField.size  = sizeof(pseqtbl->seq_name);
    if (recGetBYTES(prec, SEQNAME,(ULONG)0, &byteStringField,
                    &indicator) )
	goto badret;

#ifdef SETRACE
MSGD(pcontext, "%LseqLoad: seq %i recid %D %s",
     pseqtbl->seq_num, recid, pseqtbl->seq_name);
#endif

    /* max value */
    if ( recGetLONG(prec, SEQMAX,(ULONG)0, &pseqtbl->seq_max, &indicator) )
	goto badret;

    if (indicator & SVCUNKNOWN)
        pseqtbl->seq_max = MAXPOS;

    /* min value */
    if ( recGetLONG(prec, SEQMIN,(ULONG)0, &pseqtbl->seq_min, &indicator) )
	goto badret;

    if (indicator & SVCUNKNOWN)
        pseqtbl->seq_min = ~(MAXPOS);
    
    /* increment value */
    if ( recGetLONG(prec, SEQINCR,(ULONG)0, &pseqtbl->seq_increment, &indicator) )
	goto badret;

    if (indicator & SVCUNKNOWN)
        badvals = 1;

    /* initial value */
    if ( recGetLONG(prec, SEQINIT,(ULONG)0, &pseqtbl->seq_initial, &indicator) )
	goto badret;

    if (indicator & UNKNOWN)
        badvals = 1;
    
    /* cycleok value */
    if ( recGetBOOL(prec, SEQCYCOK,(ULONG)0, (svcBool_t *)&pseqtbl->seq_cycle,
                    &indicator) )
	goto badret;

    if (indicator & SVCUNKNOWN)
        badvals = 1;
    
    if (badvals)
    {
	/* some values were unknown, so we cant do the next value
	   operation. So we set some values in memory to prevent
	   that */

	pseqtbl->seq_increment = 0;
	pseqtbl->seq_cycle = 0;
    }
    stVacate(pcontext, pdbcontext->privpool, prec);
    return (0);

badret:
    /* here if recGet fails */
    stVacate(pcontext, pdbcontext->privpool, prec);
    return (1);

}  /* end seqLoad */


/* *********************************************************************
   *********************************************************************

 	initialization code

   *********************************************************************
   ********************************************************************* */

/* PROGRAM: seqAllocate - allocate the sequence generator structures
 *
 * RETURNS: 0 in all cases
 */
int
seqAllocate (dsmContext_t *pcontext)
{
	     seqCtl_t   *pseqctl;
	     STPOOL	*pdbpool;
	     int	 i;

    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    /* allocate the sequence generator control structures */

    pdbpool = XSTPOOL(pdbcontext, pdbcontext->pdbpub->qdbpool);

    pseqctl = (seqCtl_t *)stGet(pcontext, pdbpool, seqGetCtlSize(pcontext) );
    pdbcontext->pseqctl = pseqctl;
    pdbcontext->pdbpub->qseqctl = (QSEQCTL)P_TO_QP(pcontext, pseqctl);
    for (i = 0; i < SEQHASHP; i++)
    {
	pseqctl->seq_hash[i] = SEQINVALID;
    }
    for (i = 0; i < (int)MAXSEQVALS(pdbcontext->pdbpub->dbblksize); i++)
    {
	pseqctl->seq_setbl[i].seq_nxhash = SEQINVALID;
	pseqctl->seq_setbl[i].seq_num = i;
    }

    return (0);

}  /* end seqAllocate */


/* PROGRAM: seqInit - initialize sequence cache
 *		should be called only once, before crash
 *		recovery, but after database is open.
 *
 * RETURNS: 0 if successful
 *          TOOFAR if cxnxt failed
 *          1 if seqLoad failed
 *         -1 if build of key failed
 */
int
seqInit (dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    seqCtl_t	*pseqctl = pdbcontext->pseqctl;
    seqTbl_t	*pseqtbl;
    seqTbl_t	 tmpseqtbl;
    int		 ret;
    int		 numseqs = 0;
    int		 i;
	
    DBKEY            recid;
    CURSID           cursid = NULLCURSID;

    svcDataType_t component[1];
    svcDataType_t *pcomponent[1];
    COUNT         maxKeyLength = 32; /* SHOULD BE A CONSTANT! */

    AUTOKEY(keylow, 32)
    AUTOKEY(keyhi, 32)

    /* set up keys and cursor for the find */
    component[0].type            = SVCLOWRANGE;
    component[0].indicator       = SVCLOWRANGE;
 
    keylow.akey.index    = IDXSEQ;
    keylow.akey.keycomps = 1;
    keylow.akey.ksubstr  = 0;
    keylow.akey.word_index = 0;
 
    pcomponent[0] = &component[0];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &keylow.akey))
        return -1;

    component[0].type            = SVCHIGHRANGE;
    component[0].indicator       = SVCHIGHRANGE;
 
    keyhi.akey.index    = IDXSEQ;
    keyhi.akey.keycomps = 1;
    keyhi.akey.ksubstr  = 0;
    keyhi.akey.word_index = 0;
 
    pcomponent[0] = &component[0];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &keyhi.akey))
        return -1;

    for (i = 0; i < SEQHASHP; i++)
    {
	pseqctl->seq_hash[i] = SEQINVALID;
    }
    for (i = 0; i < (int)MAXSEQVALS(pdbcontext->pdbpub->dbblksize); i++)
    {
	pseqctl->seq_setbl[i].seq_nxhash = SEQINVALID;
	pseqctl->seq_setbl[i].seq_num = i;
	pseqctl->seq_setbl[i].seq_recid = (DBKEY)0;;
    }

    /* If there is no sequence block or partial db, can't init sequences */
    if (pdbcontext->pmstrblk->mb_seqblk == (DBKEY)0)
    {
        /* pretend all is well */
        return (0);
    }

    ret = cxGetIndexInfo(pcontext, SEQFILE, &keylow.akey);
    if( ret )
	return ret;

    /* The following assignment isn't really needed since 
     * cxFind only looks at the lo bracket for this info.
     */
    keyhi.akey.area = keylow.akey.area;
    keyhi.akey.root = keylow.akey.root;
    keyhi.akey.unique_index = keylow.akey.unique_index;
   
    /* find the first one. We do not need to lock the records because
       this is being done during database open */
    ret = cxFind (pcontext, &keylow.akey, &keyhi.akey, &cursid, 
		  DSMAREA_SCHEMA, (dsmRecid_t *)&recid,
		  DSMPARTIAL, DSMFINDFIRST, LKNOLOCK);
    for( ; ; )
    {	
        if (ret) break;

        if (numseqs >= (int)MAXSEQVALS(pdbcontext->pdbpub->dbblksize))
	{
	    /* no more room */

	    MSGN_CALLBACK(pcontext, seMSG003);
	    break;
	}
        /* load the setbl entry with the values from the record */

        stnclr ((TEXT *)&tmpseqtbl, (int)(sizeof (tmpseqtbl)));

	ret = (int)seqLoad (pcontext, &tmpseqtbl, recid);
	if (ret == 0)
	{
            numseqs++;

	    /* take the slot */

	    pseqtbl = &(pseqctl->seq_setbl[tmpseqtbl.seq_num]);

            /* copy in the new values */

            bufcop (pseqtbl, &tmpseqtbl, sizeof (tmpseqtbl));

            /* add name to name lookup table */

            seqInsert (pcontext, pseqctl, pseqtbl);
        }
	else if (ret != DSM_S_RMNOTFND) break;

        /* advance to the next record */

        ret = cxNext (pcontext, &cursid, DSMAREA_SCHEMA, (dsmRecid_t *)&recid,
                      &keylow.akey, &keyhi.akey,
                       DSMPARTIAL, DSMFINDNEXT, LKNOLOCK);
    }
    if (cursid != NULLCURSID)
        cxDeactivateCursor(pcontext, &cursid);

    if (ret == ENDLOOP) ret = 0;
    if (ret) FATAL_MSGN_CALLBACK(pcontext, seFTL001, ret);

    return (ret);

}  /* end seqInit */


/* *********************************************************************
   *********************************************************************

 	The functions below are the ones to manipulate the sequences

   *********************************************************************
   ********************************************************************* */

/* PROGRAM: seqNumAssign - get a PARTICULAR free sequence number slot
 *
 * RETURNS: seq no
 *	    FDSEQFULL
 */
LOCALF int
seqNumAssign (
	dsmContext_t	*pcontext,
	COUNT		 sno)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    seqCtl_t      *pseqctl    = pdbcontext->pseqctl;
    seqTbl_t      *pseqtbl;
    bmBufHandle_t  seqblkHandle;
    seqBlock_t    *pseqblk;
    seqAsnNote_t   anote;

    rlTXElock(pcontext,RL_TXE_SHARE,RL_MISC_OP);

    seqblkHandle = bmLocateBlock(pcontext, BK_AREA1, 
                                 pdbcontext->pmstrblk->mb_seqblk, TOMODIFY);
    pseqblk = (seqBlock_t *)bmGetBlockPointer(pcontext,seqblkHandle);
    MT_LOCK_SEQ ();
    
    /* Check for a free slot */

    if (sno < 0 || sno >= (int)MAXSEQVALS(pdbcontext->pdbpub->dbblksize))
    {
        /* no more room */

        MT_UNLK_SEQ ();

        bmReleaseBuffer(pcontext, seqblkHandle);
        rlTXEunlock(pcontext);
        return (DSM_S_FDSEQFULL);
    }

    pseqtbl = pseqctl->seq_setbl;
    pseqtbl += sno;  

    if (pseqtbl->seq_num != sno || pseqtbl->seq_recid != (DBKEY)0)
    {

        MT_UNLK_SEQ ();

        bmReleaseBuffer(pcontext, seqblkHandle);
        rlTXEunlock(pcontext);
        return (DSM_S_FDSEQFULL);
    }

    /* mark the slot reserved */

    pseqtbl->seq_recid = (DBKEY)1;

    MT_UNLK_SEQ ();

    INITNOTE(anote, RL_SEASN, RL_PHY | RL_LOGBI);
    sct ((TEXT*)&anote.seqno, sno);

    slng ((TEXT*)&anote.oldval, pseqblk->seq_values[sno]);

    /* Now we do the note */

    rlLogAndDo (pcontext, (RLNOTE *)&anote, seqblkHandle, (COUNT)0, PNULL);

    bmReleaseBuffer(pcontext, seqblkHandle);

    rlTXEunlock(pcontext);

    return sno;

}  /* end seqNumAssign */


/* PROGRAM: seqAssign - get a free sequence number slot
 *
 * RETURNS: seq no
 *	    FDSEQFULL
 */
int
seqAssign (dsmContext_t	*pcontext)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    seqCtl_t      *pseqctl    = pdbcontext->pseqctl;
    seqTbl_t      *pseqtbl;
    bmBufHandle_t  seqblkHandle;
    seqBlock_t    *pseqblk;
    seqAsnNote_t   anote;
    COUNT          sno;
    COUNT          n;

    rlTXElock(pcontext,RL_TXE_SHARE,RL_MISC_OP);

    seqblkHandle = bmLocateBlock(pcontext, BK_AREA1, 
			         pdbcontext->pmstrblk->mb_seqblk, TOMODIFY);
    pseqblk = (seqBlock_t *)bmGetBlockPointer(pcontext,seqblkHandle);
    MT_LOCK_SEQ ();
    
    /* find a free slot */

    pseqtbl = pseqctl->seq_setbl;
    for (n = MAXSEQVALS(pdbcontext->pdbpub->dbblksize); n > 0; n--)
    {
        if (pseqtbl->seq_recid == (DBKEY)0) break;
        pseqtbl++;
    }
    if (n == 0)
    {
        /* no more room */

        MT_UNLK_SEQ ();

        bmReleaseBuffer(pcontext, seqblkHandle);
        rlTXEunlock(pcontext);
        return (DSM_S_FDSEQFULL);
    }
    sno = pseqtbl->seq_num;

    /* mark the slot reserved */

    pseqtbl->seq_recid = (DBKEY)1;

    MT_UNLK_SEQ ();

    INITNOTE(anote, RL_SEASN, RL_PHY | RL_LOGBI);
    sct ((TEXT*)&anote.seqno, sno);

    slng ((TEXT*)&anote.oldval, pseqblk->seq_values[sno]);

    /* Now we do the note */

    rlLogAndDo (pcontext, (RLNOTE *)&anote, seqblkHandle, (COUNT)0, PNULL);

    bmReleaseBuffer(pcontext, seqblkHandle);

    rlTXEunlock(pcontext);

    return sno;

}  /* end seqAssign */


/* PROGRAM: seqXCurrentValue - get the current value for a sequence
 *
 * RETURNS: 0		current value set
 *	    DSM_S_FNDFAIL sequence name not found
*/
LOCALF int
seqXCurrentValue (
	dsmContext_t	*pcontext,
	seqTbl_t	*pseqtbl,
	LONG	        *pcurval)
{
    bmBufHandle_t  seqblkHandle;
    seqBlock_t    *pseqblk;
    COUNT          n;

    if (pseqtbl == (seqTbl_t *)PNULL || pseqtbl->seq_recid <= 1)
    {
	MT_UNLK_SEQ ();
	return (DSM_S_FNDFAIL);
    }
    n = pseqtbl->seq_num;

    MT_UNLK_SEQ ();

    /* get exclusive lock on the sequence value block */

    seqblkHandle = bmLocateBlock(pcontext, BK_AREA1,
                       pcontext->pdbcontext->pmstrblk->mb_seqblk, TOREAD);
    pseqblk = (seqBlock_t *)bmGetBlockPointer(pcontext,seqblkHandle);
    /* extract the current value */

    *pcurval = pseqblk->seq_values[n];

    /* unlock the buffer */

    bmReleaseBuffer(pcontext, seqblkHandle);
#ifdef SETRACE
MSGD(pcontext, "%LseqXCurrentValue: seq %i v %i", (int)n, (int)*pcurval);
#endif
    return (0);

}  /* end seqXCurrentValue */


/* PROGRAM: seqCurrentValue - get the current value for a sequence -- by name
 *
 * RETURNS: 0		current value set
 *	    DSM_S_FNDFAIL sequence name not found
*/
int
seqCurrentValue (
	dsmContext_t	*pcontext,
	TEXT	        *pseqname,
	LONG	        *pcurval)
{
    seqCtl_t	*pseqctl;
    seqTbl_t	*pseqtbl;
             
    pseqctl = pcontext->pdbcontext->pseqctl;

    MT_LOCK_SEQ ();

    pseqtbl = seqFind (pcontext, pseqctl, pseqname);

    return seqXCurrentValue (pcontext, pseqtbl, pcurval);        

}  /* end seqCurrentValue */


/* PROGRAM: seqNumCurrentValue - get current value for a sequence -- by number
 *
 * RETURNS: 0		current value set
 *	    DSM_S_FNDFAIL sequence name not found
*/
int
seqNumCurrentValue (
	dsmContext_t	*pcontext,
	COUNT		 seqnum,
	LONG		*pcurval)
{
    seqCtl_t	*pseqctl;
    seqTbl_t	*pseqtbl;
             
    pseqctl = pcontext->pdbcontext->pseqctl;

    MT_LOCK_SEQ ();

    pseqtbl = seqFindNum (pcontext, pseqctl, seqnum);

    return seqXCurrentValue (pcontext, pseqtbl, pcurval);        

}  /* end seqNumCurrentValue */


/* PROGRAM: seqXNextValue - get the next value of a sequence
 *
 * RETURNS: 0		ok
 *	    DSM_S_FNDFAIL sequence name not found
 *	    DSM_S_FDSEQCYC	cycled, but cycle ok false
 */
LOCALF int
seqXNextValue (
	dsmContext_t	*pcontext,
	seqTbl_t	*pseqtbl,
	LONG	        *pnextval,
	bmBufHandle_t    seqblkHandle,
	seqBlock_t	*pseqblk)
{
    COUNT		 sno;
    LONG		 curval;
    LONG		 newval;
    seqIncNote_t	 note;
    int		         ret = 0;
    int                  overFlow = 0;

    if (pseqtbl == (seqTbl_t *)PNULL || pseqtbl->seq_recid <= 1)
    {
	MT_UNLK_SEQ ();

        bmReleaseBuffer(pcontext, seqblkHandle);

	rlTXEunlock(pcontext);

	return (DSM_S_FNDFAIL);
    }
    sno = pseqtbl->seq_num;
    curval = pseqblk->seq_values[sno];
    /* This could be the case if the minimum/initial value was changed 
     * by the client.
     */
    if (curval < pseqtbl->seq_min)
       curval = pseqtbl->seq_min;

    newval = curval + pseqtbl->seq_increment;

    /* check for roll-over */
    if( pseqtbl->seq_increment > 0 )
    {
	/* Incrementing by a positive number should always result
	   in the new value being greater than the current value
	   unless there has been integer overflow  */
	if ( newval < curval )
	    overFlow = 1;

    }
    else if( newval > curval )
	/* Incrementing by a negative number should result
	   in the new value always being less than the current
	   value unless there has been integer overflow.  */
	overFlow = 1;
	
    if ((newval > pseqtbl->seq_max) || 
        (newval < pseqtbl->seq_min) ||
	overFlow )
    {
	/* going to roll over now */

	if (!pseqtbl->seq_cycle)
	{
	    /* cant allow it */

            MT_UNLK_SEQ ();
            bmReleaseBuffer(pcontext, seqblkHandle);

	    rlTXEunlock(pcontext);

	    return (DSM_S_FDSEQCYC);
	}
	/* go to the starting value again to begin the sequence over */

	newval = pseqtbl->seq_initial;
    }
    /* build the note */

    INITNOTE (note, RL_SEINC, RL_PHY);

    sct ((TEXT *)&note.seqno, sno);
    slng ((TEXT *)&note.newval, newval);

    MT_UNLK_SEQ ();

    /* Now do the note. Note that this note can not be undone because once
       a client has used a sequence number, a second client can get the next
       one. If the first client rolls back his transaction, we can not undo
       the increment because then the second clients number will be
       duplicated */

    rlLogAndDo (pcontext, (RLNOTE *)&note, seqblkHandle, (COUNT)0, PNULL);

    /* release the block */

    bmReleaseBuffer(pcontext, seqblkHandle);

    rlTXEunlock(pcontext);

#ifdef SETRACE
MSGD(pcontext, "%LseqXNextValue: seq %i v %i", (int)sno, (int)newval);
#endif
    *pnextval = newval;
    return (ret);

}  /* end seqXNextValue */


/* PROGRAM: seqNextValue - get the next value of a sequence -- by name
 *
 * RETURNS: 0		ok
 *	    DSM_S_FNDFAIL sequence name not found
 *	    DSM_S_FDSEQCYC	cycled, but cycle ok false
 */
int
seqNextValue (
	dsmContext_t	*pcontext,
	TEXT		*pseqname,
	LONG		*pnextval)
{
    seqCtl_t	        *pseqctl;
    seqTbl_t	        *pseqtbl;
    bmBufHandle_t        seqblkHandle;
    seqBlock_t	        *pseqblk;

    pseqctl = pcontext->pdbcontext->pseqctl;

    rlTXElock(pcontext,RL_TXE_SHARE,RL_SEQ_UPDATE_OP);

    /* get exclusive lock on the sequence value block */

    seqblkHandle = bmLocateBlock(pcontext, BK_AREA1,
                     pcontext->pdbcontext->pmstrblk->mb_seqblk, TOMODIFY);
    pseqblk = (seqBlock_t *)bmGetBlockPointer(pcontext,seqblkHandle);

    MT_LOCK_SEQ ();

    pseqtbl = seqFind (pcontext, pseqctl, pseqname);

    return seqXNextValue (pcontext, pseqtbl, pnextval, seqblkHandle, pseqblk);

}  /* end seqNextValue */


/* PROGRAM: seqNumNextValue - get the next value of a sequence -- by number
 *
 * RETURNS: 0		ok
 *	    DSM_S_FNDFAIL sequence name not found
 *	    DSM_S_FDSEQCYC	cycled, but cycle ok false
 */
int
seqNumNextValue (
	dsmContext_t	*pcontext,
	COUNT		 seqnum,
	LONG		*pnextval)
{
    seqCtl_t	        *pseqctl;
    seqTbl_t	        *pseqtbl;
    bmBufHandle_t        seqblkHandle;
    seqBlock_t	        *pseqblk;

    pseqctl = pcontext->pdbcontext->pseqctl;

    rlTXElock(pcontext,RL_TXE_SHARE,RL_SEQ_UPDATE_OP);

    /* get exclusive lock on the sequence value block */

    seqblkHandle = bmLocateBlock(pcontext, BK_AREA1,
                          pcontext->pdbcontext->pmstrblk->mb_seqblk, TOMODIFY);
    pseqblk = (seqBlock_t *)bmGetBlockPointer(pcontext,seqblkHandle);

    MT_LOCK_SEQ ();

    pseqtbl = seqFindNum (pcontext, pseqctl, seqnum);

    return seqXNextValue (pcontext, pseqtbl, pnextval, seqblkHandle, pseqblk);

}  /* end seqNumNextValue */


/* PROGRAM: seqRefresh - update sequence cache
 *	funCode:
 *		SEQADD	_Sequence record created
 *		SEQDEL	_Sequence record deleted
 *		SEQUPD	_Sequence record changed
 *
 * RETURNS: 0		ok
 *	    DSM_S_FNDFAIL recid not found
 */
int
seqRefresh(
	dsmContext_t	*pcontext,
	int		 funCode,
	seqParmBlock_t	*pseq_parm)
{
	DBKEY		 recid = pseq_parm->seq_recid;
	seqCtl_t	*pseqctl = pcontext->pdbcontext->pseqctl;
	seqTbl_t	*pseqtbl;
	bmBufHandle_t    seqblkHandle;
	seqBlock_t	*pseqblk;
	seqAddNote_t	 anote;
	seqDelNote_t	 dnote;
	seqUpdNote_t	 unote;
	RLNOTE		*pnote;
	COUNT		 sno;
	LONG		 oldval;

    pseqctl = pcontext->pdbcontext->pseqctl;

#ifdef SETRACE
MSGD(pcontext, "%LseqRefresh: func %i recid %D", funCode, recid);
#endif

    rlTXElock(pcontext,RL_TXE_SHARE,RL_MISC_OP);

    seqblkHandle = bmLocateBlock(pcontext, BK_AREA1,
                    pcontext->pdbcontext->pmstrblk->mb_seqblk, TOMODIFY);
    pseqblk = (seqBlock_t *)bmGetBlockPointer(pcontext,seqblkHandle);
    if (funCode == SEQADD)
    {
	sno = pseq_parm->seq_num;

	/* lock the value block. We are going to set the current value
	   to the initial value */

        oldval = pseqblk->seq_values[sno];

	pseqtbl = pseqctl->seq_setbl + sno;

        MT_LOCK_SEQ ();

	if (pseqtbl->seq_recid != 1)
	{
	    /* slot was not previously reserved by an assign */

	    MT_UNLK_SEQ ();
            bmReleaseBuffer(pcontext, seqblkHandle);
	    FATAL_MSGN_CALLBACK(pcontext, seFTL002, pseq_parm->seq_num);
	}
	pseqtbl->seq_recid = pseq_parm->seq_recid;
	pseqtbl->seq_initial = pseq_parm->seq_initial;
	pseqtbl->seq_min = pseq_parm->seq_min;
	pseqtbl->seq_max = pseq_parm->seq_max;
	pseqtbl->seq_cycle = (GBOOL)pseq_parm->seq_cycle;
	pseqtbl->seq_increment = pseq_parm->seq_increment;
        bufcop (pseqtbl->seq_name, pseq_parm->seq_name, sizeof (pseqtbl->seq_name));
        seqInsert (pcontext, pseqctl, pseqtbl);

        INITNOTE(anote, RL_SEADD, RL_PHY | RL_LOGBI);
        sct ((TEXT*)&anote.seqno, sno);
        slng ((TEXT *)&anote.newval, pseqtbl->seq_initial);
        slng ((TEXT *)&anote.oldval, oldval);
 
	MT_UNLK_SEQ ();

        pnote = (RLNOTE *)&anote;

        /* Now we do the note */

        rlLogAndDo (pcontext, (RLNOTE *)pnote, seqblkHandle, (COUNT)0, PNULL);

        bmReleaseBuffer(pcontext, seqblkHandle);

	rlTXEunlock(pcontext);

	return (0);
    }
    MT_LOCK_SEQ ();

    /* find existing entry by matching recid */

    pseqtbl = seqFindId (pcontext, pseqctl, recid);
    if (pseqtbl == (seqTbl_t *)PNULL)
    {
	MT_UNLK_SEQ ();

        bmReleaseBuffer(pcontext, seqblkHandle);

        rlTXEunlock(pcontext);

	return (DSM_S_FNDFAIL);
    }
    sno = pseqtbl->seq_num;

    switch (funCode)
    {
	case SEQDEL:	/* _sequence was deleted. delete from cache */

	    INITNOTE(dnote, RL_SEDEL, RL_PHY | RL_LOGBI);
            sct ((TEXT*)&dnote.seqno, sno);
            slng ((TEXT*)&dnote.recid, pseqtbl->seq_recid);
            slng ((TEXT*)&dnote.initial, pseqtbl->seq_initial);
            slng ((TEXT*)&dnote.min, pseqtbl->seq_min);
            slng ((TEXT*)&dnote.max, pseqtbl->seq_max);
            slng ((TEXT*)&dnote.increment, pseqtbl->seq_increment);
            dnote.cycle = (GBOOL)(pseqtbl->seq_cycle);
            bufcop (dnote.name, pseqtbl->seq_name, sizeof (dnote.name));

	    MT_UNLK_SEQ ();

            /* get the current value, put into note */

            slng ((TEXT*)&dnote.oldval, pseqblk->seq_values[sno]);

	    pnote = (RLNOTE *)&dnote;

            break;

	case SEQUPD:	/* _Sequence record changed, reload it */

	    INITNOTE(unote, RL_SEUPD, RL_PHY | RL_LOGBI);

            /*  Preserve the old values in case we back out	*/
            sct ((TEXT*)&unote.seqno, sno);
            slng ((TEXT*)&unote.recid, pseqtbl->seq_recid);
            slng ((TEXT*)&unote.initial, pseqtbl->seq_initial);
            slng ((TEXT*)&unote.min, pseqtbl->seq_min);
            slng ((TEXT*)&unote.max, pseqtbl->seq_max);
            slng ((TEXT*)&unote.increment, pseqtbl->seq_increment);
            unote.cycle = (GBOOL)(pseqtbl->seq_cycle);
            bufcop (unote.name, pseqtbl->seq_name, sizeof (unote.name));
            
            /* Insert the new values to update the cache	*/
            slng ((TEXT*)&unote.new_initial, pseq_parm->seq_initial);
            slng ((TEXT*)&unote.new_min, pseq_parm->seq_min);
            slng ((TEXT*)&unote.new_max, pseq_parm->seq_max);
            slng ((TEXT*)&unote.new_increment, pseq_parm->seq_increment);
            unote.new_cycle = pseq_parm->seq_cycle;
            bufcop (unote.new_name, pseq_parm->seq_name, sizeof (unote.new_name));


	    MT_UNLK_SEQ ();

	    pnote = (RLNOTE *)&unote;
            break;

	default:
	    MT_UNLK_SEQ ();

            bmReleaseBuffer(pcontext, seqblkHandle);
	    
	    rlTXEunlock(pcontext);

	    MSGN_CALLBACK(pcontext, seMSG004, funCode);
	    return (DSM_S_FNDFAIL);
    }
    /* Now we do the note */

    rlLogAndDo (pcontext, (RLNOTE *)pnote, seqblkHandle, (COUNT)0, PNULL);

    bmReleaseBuffer(pcontext, seqblkHandle);

    rlTXEunlock(pcontext);

    return (0);

}  /* end seqRefresh */


/* PROGRAM: seqXSetValue - set the current value of a sequence
 *
 * RETURNS: 0		ok
 *	    DSM_S_FNDFAIL sequence name not found
 *	    FDSEQCYC	value not in range min .. max
 */
LOCALF int
seqXSetValue (
	dsmContext_t	*pcontext,
	seqTbl_t	*pseqtbl,
	LONG		 newval)
{
    bmBufHandle_t        seqblkHandle;
    seqBlock_t	        *pseqblk;
    seqSetNote_t	 note;
    COUNT		 sno;
    LONG		 oldval;

    MT_UNLK_SEQ ();

    if (pseqtbl == (seqTbl_t *)PNULL || pseqtbl->seq_recid <= 1)
    {
	return (DSM_S_FNDFAIL);
    }
    sno = pseqtbl->seq_num;

#ifdef SETRACE
MSGD(pcontext, "%LseqXSetValue: seq %i v %i", (int)sno, (int)newval);
#endif
    
    rlTXElock(pcontext,RL_TXE_SHARE,RL_SEQ_UPDATE_OP);

    /* get exclusive lock on the sequence value block */

    seqblkHandle = bmLocateBlock(pcontext, BK_AREA1,
                       pcontext->pdbcontext->pmstrblk->mb_seqblk, TOMODIFY);
    pseqblk = (seqBlock_t *)bmGetBlockPointer(pcontext,seqblkHandle);
    oldval = pseqblk->seq_values[sno];

    /* check for range problem. There shouldn't be one because the
       dictionary should have verified the value. But we'll just make
       sure */

    MT_LOCK_SEQ ();

    if ((newval > pseqtbl->seq_max) || (pseqtbl->seq_min > newval))
    {
	/* bad value */

	MT_UNLK_SEQ ();

        bmReleaseBuffer(pcontext, seqblkHandle);

	rlTXEunlock(pcontext);

	return (DSM_S_FDSEQCYC);
    }
    MT_UNLK_SEQ ();

    /* build the note */

    INITNOTE (note, RL_SESET, RL_PHY);

    sct ((TEXT *)&note.seqno, sno);
    slng ((TEXT *)&note.newval, newval);
    slng ((TEXT *)&note.oldval, oldval);

    /* Now do the note. */

    rlLogAndDo (pcontext, (RLNOTE *)&note, seqblkHandle, (COUNT)0, PNULL);

    /* release the block */
    
    bmReleaseBuffer(pcontext, seqblkHandle);

    rlTXEunlock(pcontext);

    return (0);

}  /* end seqXSetValue */


/* PROGRAM: seqSetValue - set the current value of a sequence -- by name
 *
 * RETURNS: 0		ok
 *	    DSM_S_FNDFAIL sequence name not found
 *	    DSM_S_FDSEQCYC	value not in range min .. max
 */
int
seqSetValue(
	dsmContext_t	*pcontext,
	TEXT		*pseqname,
	LONG		 newval)
{
    seqCtl_t	*pseqctl;
    seqTbl_t	*pseqtbl;

    pseqctl = pcontext->pdbcontext->pseqctl;

    MT_LOCK_SEQ ();

    pseqtbl = seqFind (pcontext, pseqctl, pseqname);

    return seqXSetValue (pcontext, pseqtbl, newval);

}  /* end seqSetValue */


/* PROGRAM: seqNumSetValue - set the current value of a sequence -- by number
 *
 * RETURNS: 0		ok
 *	    DSM_S_FNDFAIL sequence name not found
 *	    DSM_S_FDSEQCYC	value not in range min .. max
 */
int
seqNumSetValue (
	dsmContext_t	*pcontext,
	COUNT		 seqnum,
	LONG		 newval)
{
    seqCtl_t	*pseqctl;
    seqTbl_t	*pseqtbl;

    pseqctl = pcontext->pdbcontext->pseqctl;

    MT_LOCK_SEQ ();

    pseqtbl = seqFindNum (pcontext, pseqctl, seqnum);

    return seqXSetValue (pcontext, pseqtbl, newval);

}  /* end seqNumSetValue */


/* PROGRAM: seqKeyBuild - Build key value for the IDXSEQ from the values in the
 *                        seqTbl_t table entry.  The xDbkey_t is also built.
 *
 * RETURNS: 0		ok
 *                      Errors from keyBuild.
 */
LOCALF int
seqKeyBuild (dsmContext_t *pcontext,
             dsmKey_t   *pkey,
             seqTbl_t	*pseqtbl,
             lockId_t   *plockId)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int		 ret;
 
    svcDataType_t component[4];
    svcDataType_t *pcomponent[4];
    COUNT         maxKeyLength = KEYSIZE;

    /* Now build the key and add the entry to the index.  
     * The key has two componets:
     * 1. A long, SEQDBKEY
     * 2. Text,   SEQNAME
     *
     * The index is IDXSEQ.  If it fails, zero pseqtbl->seq_recid and call 
     * dsmRecordDelete to undo everything we've done.
     */
    /* The first componet: the db-recid */
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = pdbcontext->pmstrblk->mb_dbrecid;

    /* The second componet: the sequence name */
    component[1].type            = SVCCHARACTER;
    component[1].indicator       = 0;
    component[1].f.svcByteString.pbyte = pseqtbl->seq_name;
    component[1].f.svcByteString.size  = stlen(pseqtbl->seq_name);
    /* TODO: remove the following when keysvcs is working properly! */
    component[1].f.svcByteString.length  = stlen(pseqtbl->seq_name);

    pkey->index    = IDXSEQ;
    pkey->keycomps = 2;
    pkey->ksubstr  = 0;

    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];

    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, pkey))
        FATAL_MSGN_CALLBACK(pcontext, ixFTL003); /*FATAL*/

    ret = cxGetIndexInfo(pcontext, SEQFILE, pkey);
    if (ret)
        return ret;

    plockId->table  = SEQFILE;
    plockId->dbkey = pseqtbl->seq_recid;

    return 0;
}


/* PROGRAM: seqCreate - Create a sequence and its _SEQUENCE record
 *
 * RETURNS: 0		ok
 *	    DSM_S_FNDFAIL sequence name not found
 *	    DSM_S_FDSEQCYC	value not in range min .. max
 */

int
seqCreate (
	dsmContext_t	*pcontext,
	seqParmBlock_t	*pseqparms)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    seqCtl_t	*pseqctl = pdbcontext->pseqctl;
    seqTbl_t	*pseqtbl;
    TEXT        *ptxt;
    int		 ret;
    LONG         newval;
    COUNT        seqnum;
    ULONG        recSize;
    ULONG        maxLen = 100;
    DBKEY        recid = 0;
    TEXT         name = '\0';
    LONG         prev_uc_task;

    dsmRecord_t  record;
    dsmBuffer_t *prec;
    lockId_t     lockId;
    dsmKey_t    *pkey;
    svcByteString_t    recTextValue;

    AUTOKEY(seqKey, KEYSIZE)

    if (pseqparms->seq_increment == 0)
        return DSM_S_FDSEQCYC;

    newval = pseqparms->seq_initial + pseqparms->seq_increment;
    
    if (newval > pseqparms->seq_max || pseqparms->seq_min > newval)
        return DSM_S_FDSEQCYC;

    if (pseqparms->seq_num == -1)
        seqnum = seqAssign (pcontext);
    else if ((pseqparms->seq_num < 0) ||
             (pseqparms->seq_num >= (int)MAXSEQVALS(pdbcontext->pdbpub->dbblksize)) )
        return DSM_S_FNDFAIL;
    else seqnum = seqNumAssign(pcontext, pseqparms->seq_num);

    if (seqnum < 0)
        return (int) seqnum;

    pseqtbl = seqFindNum (pcontext, pseqctl, seqnum);
    if (!pseqtbl)
        return DSM_S_FNDFAIL;

    if (pseqtbl->seq_recid != (DBKEY)1) /* seq[Num]Assign reserved */
        return pseqtbl->seq_recid ? DSM_S_FDDUPKEY : DSM_S_FNDFAIL;

    pseqtbl->seq_nxhash = SEQINVALID;

    if (!*pseqparms->seq_name) {
        newval = (LONG) seqnum;
        ptxt = stmove (pseqparms->seq_name, (TEXT *)"Triton");
        hextoa(ptxt, (TTINY *) &newval, sizeof (LONG));
    }

    stcopy (pseqtbl->seq_name, pseqparms->seq_name);

    pseqtbl->seq_num       = seqnum;
    pseqtbl->seq_max       = pseqparms->seq_max;
    pseqtbl->seq_min       = pseqparms->seq_min;
    pseqtbl->seq_increment = pseqparms->seq_increment;
    pseqtbl->seq_initial   = pseqparms->seq_initial;
    pseqtbl->seq_cycle     = pseqparms->seq_cycle;

    prec = (dsmBuffer_t *)stRent(pcontext, pdbcontext->privpool, maxLen);

    /* Create a _sequence record out of thin bits */

    if (RECSUCCESS != recRecordNew(SEQFILE, prec, maxLen, &recSize,
                                   SEQDBKEY+1, 0/*schemaversion*/))
    {
        stVacate(pcontext, pdbcontext->privpool, prec);
        pseqtbl->seq_recid = 0;
        return -1;
    }

    if (RECSUCCESS != recInitArray(prec, maxLen, &recSize,
                                   SEQMISC, 8/* num field extents */))
    {
        stVacate(pcontext, pdbcontext->privpool, prec);
        pseqtbl->seq_recid = 0;
        return -1;
    }

    /* Put in the proper values */

    recTextValue.pbyte  = pseqtbl->seq_name;
    recTextValue.length = stlen(pseqtbl->seq_name);
    /* TODO: remove the following when rec services is working properly! */
    recTextValue.size   = recTextValue.length;
    if (
        recPutBYTES(prec, maxLen, &recSize, SEQNAME,0,  &recTextValue,   0) ||
        recPutBOOL (prec, maxLen, &recSize, SEQCYCOK,0, 
                                            (GTBOOL)pseqtbl->seq_cycle, 0) ||
        recPutLONG(prec, maxLen, &recSize, SEQNUM,0,   pseqtbl->seq_num, 0) ||
        recPutLONG(prec, maxLen, &recSize, SEQMAX,0,   pseqtbl->seq_max, 0) ||
        recPutLONG(prec, maxLen, &recSize, SEQMIN,0,   pseqtbl->seq_min, 0) ||
        recPutLONG(prec, maxLen, &recSize, SEQINCR,0,  pseqtbl->seq_increment, 0) ||
        recPutLONG(prec, maxLen, &recSize, SEQINIT,0,  pseqtbl->seq_initial, 0) ||
        recPutLONG(prec, maxLen, &recSize, SEQDBKEY,0, pcontext->pdbcontext->pmstrblk->mb_dbrecid, 0))
    {
        stVacate(pcontext, pdbcontext->privpool, prec);
        pseqtbl->seq_recid = 0;
        return -1;
    }

    /* Subtract 1 from recsize because recRecordNew has added in ENDREC */
    record.recLength    = recSize - 1;
    record.table        = SEQFILE;
    record.maxLength    = maxLen;
    record.pbuffer      = prec;

    ret = dsmRecordCreate(pcontext,&record);
    if (ret)
    {
        stVacate(pcontext, pdbcontext->privpool, prec);
        pseqtbl->seq_recid = 0;
        return (dsmStatus_t)ret;
    }

    recid = record.recid;
    pseqtbl->seq_recid = recid;
    pkey = &seqKey.akey;

    ret = seqKeyBuild(pcontext, pkey, pseqtbl, &lockId);

    if (!ret)
    {
        pdbcontext->inservice++; /* keep signal handler out */
        prev_uc_task = pcontext->pusrctl->uc_task;
        pcontext->pusrctl->uc_task = 1;  /* fake out cxAddNL */
        ret = cxAddEntry(pcontext, pkey, &lockId, &name, SEQFILE);
        pcontext->pusrctl->uc_task = prev_uc_task;
        pdbcontext->inservice--;
    }
    else 
    {   /* Had a prolem building the key or inserting it */
        pseqtbl->seq_recid = 0;
        dsmRecordDelete(pcontext, &record, &name);
    }

    stVacate(pcontext, pdbcontext->privpool, prec);

    pseqparms->seq_num = (ret ? 0 : seqnum);

    return ret;

}  /* end seqCreate */


/* PROGRAM: seqRemove - Remove the _sequence record, its index entries and the 
 *                      seqTbl_t entry.
 *
 * RETURNS: 0		ok
 *	    DSM_S_FNDFAIL sequence name not found
 */
int
seqRemove (
	dsmContext_t	*pcontext,
	COUNT		 seqnum)
{
    seqCtl_t	*pseqctl;
    seqTbl_t	*pseqtbl;
    int		 ret;
    TEXT         name = '\0';
    LONG         prev_uc_task;

    dsmRecord_t  record;
    lockId_t     lockId;
    dsmKey_t    *pkey;

    AUTOKEY(seqKey, KEYSIZE)

    pseqctl = pcontext->pdbcontext->pseqctl;

    MT_LOCK_SEQ ();

    pseqtbl = seqFindNum (pcontext, pseqctl, seqnum);
    if (!pseqtbl || pseqtbl->seq_recid <= 1) {
        MT_UNLK_SEQ ();
        return DSM_S_FNDFAIL;
    }

    record.pbuffer = (dsmBuffer_t *)0;
    record.table        = SEQFILE;
    record.recid        = pseqtbl->seq_recid;
    record.recLength    = 0;
    record.maxLength    = 0;

    MT_UNLK_SEQ ();

    pkey = &seqKey.akey;

    ret = seqKeyBuild(pcontext, pkey, pseqtbl, &lockId);

    if (!ret) {
        pcontext->pdbcontext->inservice++; /* keep signal handler out */
        prev_uc_task = pcontext->pusrctl->uc_task;
        pcontext->pusrctl->uc_task = 1;  /* fake out cxAddNL */
        ret = cxDeleteEntry(pcontext, pkey, &lockId, IXLOCK, SEQFILE, &name);
        pcontext->pusrctl->uc_task = prev_uc_task;
        pcontext->pdbcontext->inservice--;

        pseqtbl->seq_recid = 0;
        dsmRecordDelete(pcontext, &record, &name);
    }

    return ret;

}  /* end seqRemove */


/* PROGRAM: seqInfo     - Return sequence schema info
 *
 * RETURNS: 0		ok
 *	    DSM_S_FNDFAIL sequence name not found
 *	    DSM_S_FDSEQCYC	value not in range min .. max
 */

int
seqInfo (
	dsmContext_t	*pcontext,
	seqParmBlock_t	*pseqparms)
{
    seqCtl_t	*pseqctl = pcontext->pdbcontext->pseqctl;
    seqTbl_t	*pseqtbl;

    pseqctl = pcontext->pdbcontext->pseqctl;

    MT_LOCK_SEQ ();

    pseqtbl = seqFindNum (pcontext, pseqctl, pseqparms->seq_num);
    if (!pseqtbl || pseqtbl->seq_recid <= 1) {
        MT_UNLK_SEQ ();
        return DSM_S_FNDFAIL;
    }

    pseqparms->seq_recid     = pseqtbl->seq_recid;
    pseqparms->seq_max       = pseqtbl->seq_max;
    pseqparms->seq_min       = pseqtbl->seq_min;
    pseqparms->seq_increment = pseqtbl->seq_increment;
    pseqparms->seq_initial   = pseqtbl->seq_initial;
    pseqparms->seq_cycle     = pseqtbl->seq_cycle;

    MT_UNLK_SEQ ();

    return 0;

}  /* end seqInfo */











