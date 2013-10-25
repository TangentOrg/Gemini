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

/*************************************************************/
/* cx - Compressed Index module. Management of a compressed  */
/*      B-tree for the new index and the word index          */
/* NOTICE - most of the procedures have only one argument    */
/* it is a pointer to a parameter block.                     */
/*************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
 ** is not the first include, so we can't put gem_config.h there.
 */
#include "gem_global.h"

#include "dbconfig.h"
#include "cxmsg.h"
#include "bkpub.h"
#include "dbpub.h"
#include "bmpub.h"
#include "rlpub.h"

#include "usrctl.h"
#include "lkpub.h"
#include "dbmgr.h"
#include "latpub.h"
#include "dbcontext.h"
#include "stmpub.h" /* storage manager subsystem */
#include "stmprv.h"
#include "dsmpub.h"
#include "cxpub.h"
#include "cxprv.h"
#include "utspub.h"   /* for sct, slng, etc */
#include "statpub.h"
#include "geminikey.h"


/**** GLOBAL DATA DEFINITIONS              ****/

/**** LOCAL FUNCTION PROTOTYPE DEFINITIONS ****/

LOCALF int cxGetInfo    (dsmContext_t *pcontext, cxeap_t *peap, TEXT *from,
                         int is);
LOCALF int cxGetKey     (dsmContext_t *pcontext, cxeap_t *peap, TEXT *from, 
                         int cs, int ks);
LOCALF int cxSearchTree (dsmContext_t *pcontext, cxeap_t *peap, findp_t *pfndp, 
                         downlist_t *pdownlst);
LOCALF int cxCheckInsert (dsmContext_t *pcontext, findp_t *pfndp, 
			  dsmObject_t tableNum, int tagonly);
LOCALF int cxUpgradeLock (dsmContext_t *pcontext, downlist_t *pdownlst);



LOCALF int
cxEnumEntry(
        cxblock_t   *pixblk,        /* pointer to the searched block */        
        TEXT        *pinpkey,       /* pointer to the key to be searched */
        UCOUNT       inkeysz,       /* number of bytes in the key to be searched */
        TEXT       **pDownEntry);   /* returnedpointer to the entry to traverse down with */

LOCALF int cxCheckDelete ( findp_t *pfndp);
LOCALF TEXT * cxPositionToNext
                        (dsmContext_t *pcontext, cxeap_t *peap, findp_t *pfndp);
LOCALF int cxExpandKey  (dsmContext_t *pcontext, TEXT *pto, TEXT *pstart,
                         TEXT *pentry,
                         int leng);

#if 0
LOCALF int cxIsInList   (dsmContext_t *pcontext, downlist_t *pdownlst, LONG dbk);
LOCALF int cxReplaceInfo  (dsmContext_t *pcontext, findp_t *pfndp);        
#endif




/**** END OF LOCAL FUNCTION PROTOTYPE DEFINITIONS ****/

/*****************************************************************************/
/*                                                                           */
/*       Compressed (B-tree) index maintenance and access procedures         */
/*       -----------------------------------------------------------         */
/*                                                                           */
/* cxPut - Puts an entry or a record TAG (last byte of dbkey) in the B-tree. */
/*                                                                           */
/* cxRemove - Deletes the given entry from the B-tree                        */
/*                                                                           */
/* cxGet - finds the entry of the given key and returns its INFO if required */
/*                                                                           */
/* cxNextPrevious - finds next or prev entry in the B-tree and returns KEY   */
/*                   and INFO                                                */
/*                                                                           */
/*****************************************************************************/
/*          Entry Access Parameters      (by "Entry" we mean a B-tree entry) */
/*          -----------------------                                          */
/*                                                                           */
/* The above listed procedures get only one parameter - peap, which is a     */
/* pointer to a parameter structure called the "Entry Access Parameter struct"*/
/* or EAP.  The EAP contains all the input and output parametes.             */
/*                                                                           */
/* Not all the parameters are used by each procedure.                        */
/*                                                                           */
/* It is the responsibility of the caller to initialize the relevant         */
/* parameters in each case.                                                  */
/*                                                                           */
/* The list of the required input parameters and of the returned output      */
/* parameters is specified in the description of each procedure.             */
/*                                                                           */
/*****************************************************************************/
/*          Compressed Index Entry Structure                                 */
/*          -------------------------------                                  */
/*                                                                           */
/*  An entry in the index consists of a 3 length bytes followed by a variable*/
/*  length KEY followed by a variable length INFO. The 3 length bytes are:   */
/*                                                                           */
/*           cs - Compression Size, number of compressed leading bytes       */
/*           ks - Key Size, number of bytes left in the KEY after compression*/
/*           is - Info Size, number of bytes in the INFO part                */
/*                                                                           */
/*  The internal structure of the KEY part is as follows:                    */
/*                                                                           */
/*   - 2 bytes reserved for future use (may be index type) currently NULL.   */
/*   - 2 bytes Index number stored with sct().                               */
/*   - Variable length key value, NULL terminated (may not include NULL).    */
/*   - All but the least significant byte of the DBKEY(s) represented by     */
/*     the entry, stored in a Variable DBKEY format.                         */
/*                                                                           */
/*  The INFO part contains one or many TAGs. A TAG is the least significant  */
/*  byte of a DBKEY represented by the the index entry. Many dbkeys may be   */
/*  represented in one index entry, as long as all of them share the same    */
/*  key including the 3 most significant bytes of their dbkeys.              */
/*                                                                           */
/*  An entry in the index may represent 1 to 256 dbkeys, therefore the INFO  */
/*  may include up to 256 TAGs.                                              */
/*                                                                           */
/*  There are 2 basic structures of the INFO part, as follows:               */
/*                                                                           */
/*  1) to represent 1 to 32 TAGs: 1 to 32 bytes each is the TAG itself       */
/*     (the least significant byte of a dbkey) ordered by their numerical    */
/*     value to allow binary search. The is (Info Size) byte is the TAG count*/
/*                                                                           */
/*  2) to represent 33 to 256 TAGs: one byte of TAGs count followed by 32    */
/*     bytes "bit map". The bit map includes 256 bits, an ON bit represents  */
/*     a TAG where the bit position is equal to the TAG's value. The is      */
/*     (INFO Size) is always 33.                                             */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/******************************************************************************
                INDEX MANAGER 
Basic index operations:
cxPut - Put an entry or a record TAG in the index.
cxRemove - Delete the given entry from the index.
cxGet - Traverses the index and finds the entry with a given key.
cxNextPrevious - get next or prev entry from the index


******************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/*************************************************************************/
/* PROGRAM: cxPut - Put an entry or a record TAG in the index B-tree.
 *
 * NOTICE: the B-tree manager can be used for other purposes than the index.
 *      It may be used to enter any set of keyed entries, in order to sort
 *      them and search them.  This procedure is ready to accept these non
 *      index entries, even though we are not using it yet.  Until we use it,
 *      you may ignore part of the procedure which deals with cases where
 *      the CXTAG flag is OFF.
 *
 *      Two modes:
 *      1 - Inserts the given entry into the cxtree.
 *
 *      2 - Insert a given "tag" to the list of tags in a given entry.
 *              If an entry with the same key does not exist, a new
 *              entry will be inserted.
 *              If an entry with the same key exists, the tag will be
 *              added to the list of tags.
 *
 *              A "tag" is a 4 bytes number that usually identifies a record.
 *              Currently, in the case of an index entry, the DBKEY is the
 *              TAG (TAG = RECID).
 *              All but the least significant byte of the TAG are stored at the key
 *              end in a Varuable DBKEY format.
 *              The least significant byte of the tag is in the data (pointed
 *              by peap->pdata).
 *              NOTICE: to make the tag machine independent it must be
 *              stored with slng() and retrieved with xlng().
 *
 *
 *      Input Parameters
 *      ----------------
 *      peap->flags - must have CXPUT+CXTAG for non-unique index entry
 *                    must have CXPUT+CXTAG+CXUNIQE for a unique index entry
 *                  (only CXPUT to enter a non index entry with no TAG)
 *
 *      peap->pkykey - must points to a key where:
 *
 *              peap->pkykey->keystr[0] = total length of keystr
 *              peap->pkykey->keystr[1] = length of key including
 *                                          indexnum, key value, NULL, most
 *                                          significant bytes of dbkey.
 *              peap->pkydkey->keystr[2] = length of INFO. Usualy 1
 *                                              which is the length of the TAG.
 *              Followed by the KEY including indexnum, key value, NULL and
 *                      most significant bytes of dbkey.
 *              Followed by the TAG (least significant byte of the dbkey).
 *
 *
 *      peap->pinpkey - points to the beginnig of the key which is
 *              &peap->pkykey->keystr[3]
 *
 *      peap->pdata - points to the TAG in the keystr.
 *
 *      peap->pcursor - must be NULL for regular index entry put.
 *                      It must point to a cxcursor_t if the index is generated
 *                      in collating order like in index rebuild or convert
 *                      V6 to V7. In this case CXREBLD must also be set in
 *                      peap->flags.
 *      peap->inkeysz - length of the key as in peap->pkykey->keystr[1]
 *
 *      peap->datasz - length of INFO as in peap->pkykey->keystr[2]
 *
 *      Output Parameters
 *      -----------------
 * RETURNS:
 *      0   - Successful              
 *      !0  - Failure
 *            IXDUPKEY
 *            RQSTREJ  (from lkCheckTrans())
 *            RQSTQUED (from lkCheckTrans())
 */
int
cxPut(
        dsmContext_t    *pcontext,
	cxeap_t         *peap, /* ptr to the input Entry Access Parameter Blk*/
	dsmObject_t      tableNum)
{
        findp_t   *pfndp;        /* pointer to the FIND P. B. */
        findp_t   fndp;         /* parameter block for the FIND */
        downlist_t  dlist;                        /* temporary list down */
        downlist_t  *pdownlst= &dlist;     /* to temporary list down */
        inlevel_t   *plevel;
        TEXT      *pos;
        int       is, ks;
/*        int       is; */

        int        unsafe;               /* assume safe */
        int        diddelete;            /* assume no delete */
        int        notfound;
        int        tagonly;  /* 1 if insert tag only to existing entry */
        DBKEY      root = 0;
        int        ixnum;
        cxcursor_t *pcursor;
        cxbap_t    bap;                  /* block access parameters */
        int        ret;
        int        cxfast;               /* flag: 1 => didn't search from the top */

        int         dbkeySizeInKey=0;
        DBKEY        transNumber;  /* must be defined as DBKEY not as LONG
                                    When DBKEY is 64 bits, transaction may 
                                    be stored as 64 bits DBKEY */

        /* check the input key and data for size limits */
/*#ifdef NEW_MAX_KEY_SIZES_DISABLED */
        if ((peap->inkeysz == 0) || (peap->inkeysz > MAXKEYSZ))
/*#else */
/*        if ((peap->inkeysz == 0) */
/*        || (peap->inkeysz > pcontext->pdbcontext->pdbpub->maxkeysize)) */
/*#endif  NEW_MAX_KEY_SIZES_DISABLED  */
        {
            FATAL_MSGN_CALLBACK(pcontext, cxFTL001,peap->inkeysz);
        }
/*#ifdef NEW_MAX_KEY_SIZES_DISABLED */
        if (peap->datasz > MAXINFSZ)
/*#else */
/*        if (peap->datasz > pcontext->pdbcontext->pdbpub->maxinfosize) */
/*#endif   NEW_MAX_KEY_SIZES_DISABLED  */
        {
            FATAL_MSGN_CALLBACK(pcontext, cxFTL002, peap->datasz);
        }

        /* initialize the parameters of the find */
	INIT_S_FINDP_FILLERS( fndp ); /* bug 20000114-034 by dmeyer in v9.1b */

        pfndp = &fndp;  /* load pointer to the FIND Parameter structure */
        pfndp->pinpdata = peap->pdata;
        pfndp->indatasz = peap->datasz;
        pfndp->pkykey = peap->pkykey;
        pfndp->flags = (UCOUNT)peap->flags; /* BUM - CAST */
        pcursor = peap->pcursor;
        ixnum = peap->pkykey->index;


        /* Check if a FAST insert can be used without traversing the
        B-tree from the top. During rebuild we can add many entries in 
        sequence, filling the block, without the need to search from
        the root of the tree. We use the cursor which points to the
        current block in the bottom of the tree. */
/*        cxfast = ((peap->flags & CXREBLD) && pcursor->blknum && */
/*                                ixnum == xct(&pcursor->pkey[ENTHDRSZ + 2])); */
        cxfast = (  (peap->flags & CXREBLD) && pcursor->blknum
                 && ixnum == xct(&pcursor->pkey[FULLKEYHDRSZ + 2]));

    rlTXElock(pcontext,RL_TXE_SHARE,RL_KEY_ADD_OP);

    again:

        unsafe = 0;
        diddelete = 0;
        tagonly = 0;
        CXINITLV(pdownlst);     /* initialize the list of down levels */
        plevel = &pdownlst->level[0];

        pdownlst->area = peap->pkykey->area;

        /* Do some delete chain maintenance                          */
        cxRemoveFromDeleteChain(pcontext, tableNum, pdownlst->area);
        
        if (cxfast)                     /* don't search from the top */
        {
            plevel->dbk = pcursor->blknum; /* start with cursor block */
            bap.blknum = plevel->dbk;    /* blknum of strating level block */
                                                /* get current block */
            bap.bufHandle = bmLocateBlock(pcontext, pdownlst->area, bap.blknum, 
                                     TOREADINTENT);
            bap.pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,bap.bufHandle);
            pfndp->pblkbuf = bap.pblkbuf;
            pfndp->blknum = bap.blknum;   /* blknum of the block */
            pfndp->bufHandle = bap.bufHandle;
            pfndp->pinpkey = peap->pinpkey;
            pfndp->inkeysz = peap->inkeysz;
/*            pfndp->csofprev = cxKeyCompare(peap->pinpkey, (int)peap->inkeysz, */
/*                                    &pcursor->pkey[3], (int)pcursor->pkey[1]); */
            pfndp->csofprev = cxKeyCompare(peap->pinpkey, (int)peap->inkeysz,
                 &pcursor->pkey[FULLKEYHDRSZ],
                 (int)xct(&pcursor->pkey[KS_OFFSET]));
            /* csofnext is used later to determine if the key is unique and by 
               by current convention, it needs to be equal to the number of 
               leading bytes that the current key has in common with the 
               previous key                                                */           
            pfndp->csofnext =  pfndp->csofprev;

            if (pfndp->csofprev == 0)
            {
                notfound = 0;
            }
            else
            {
                /* notfound so add at end of block   */
                notfound = 1;
                pcursor->offset = pfndp->pblkbuf->IXLENENT;
            }
            pfndp->position = &pfndp->pblkbuf->ixchars[0] + pcursor->offset;
        }
        else /* put the normal way by traversing the tree */
        {
            /* start with root block */
            /* This call will need to be replaced by the root_block information 
               which will have to be passed in */
            root = peap->pkykey->root; /* get the index root block */
            pcursor = 0;             /* do not use cursor */

            plevel->dbk = root;
            if ((peap->flags & (CXUNIQE | CXTAG)) == (CXUNIQE | CXTAG))
            {
                /* unique key will be compared without the dbkey bytes in its end.
                   the key size will be temporarily decreased by the size of the dbkey*/
#ifdef VARIABLE_DBKEY
                dbkeySizeInKey = peap->pinpkey[ peap->inkeysz - 1 ] & 0x0f; /* last nibble of DBKEY */
#else
                dbkeySizeInKey = 3;/* 3 dbk bytes */
#endif  /* VARIABLE_DBKEY */
                peap->inkeysz -= dbkeySizeInKey;/* for uniqe check key without dbk bytes */
            }

            /* This traverses the tree and finds the block and position if possible */
            /* This should have no possibility of failure since both finding the 
               key and not finding it is acceptable.  Anything else would result in a
               fatal error at a lower level
            */
            notfound = cxSearchTree(pcontext, peap, pfndp, pdownlst);/*traverse down the tree */
            if ((peap->flags & (CXUNIQE | CXTAG)) == (CXUNIQE | CXTAG))
                pfndp->inkeysz =
                peap->inkeysz += dbkeySizeInKey; /* restore to the correct key length */
        }

        /* At this point we have the bottom level block were the PUT
        operation should take place. pfndp->pblkbuf points to the buffer
        with the block. */

        if(peap->flags & CXREBLD)
        {
            /*  for index rebuild - save the key in the cursor */
            pcursor = peap->pcursor;
            pcursor->offset = pfndp->position - &pfndp->pblkbuf->ixchars[0];
/*#ifdef NEW_MAX_KEY_SIZES_DISABLED */
/*            if (pcursor->pkey == 0) */
/*                pcursor->pkey = (TEXT *)stRentd(pcontext, pdsmStoragePool,  */
/*                                                2*KEYSIZE); */
/*#else */
/*            if (pcursor->pkey == 0) */
/*                pcursor->pkey = (TEXT *)stRentd(pcontext, pdsmStoragePool, */
/*                                  (2 * pcontext->pdbcontext->pdbpub->maxkeysize)); */
/*#endif  NEW_MAX_KEY_SIZES_DISABLED  */
            /* cursors always assumes large key format */
            if (pcursor->pkey == 0)
                pcursor->pkey = (TEXT *)stRentd(pcontext, pdsmStoragePool, 
                                                2*MAXKEYSZ);

            pcursor->pkey[0] = peap->inkeysz + ENTHDRSZ;
            pcursor->pkey[1] = (TEXT)peap->inkeysz;
            pcursor->pkey[2] = 1;
            bufcop(pcursor->pkey + ENTHDRSZ, peap->pinpkey, peap->inkeysz);
        }

        if(!notfound)                   /* does the same key exists?  */
        {
            /* Same key found */
            pos = pfndp->position;
            if (peap->flags & CXTAG) /* if put tag */
            {
                if ((peap->flags & CXUNIQE)
                || (!cxFindTag(pos, *peap->pdata, CXFIND))) /* already there */
                {
                    /* the entry is already in the index, no need to put */
                    peap->errcode = IXDUPKEY;
                    goto fail1;/* no replace */
                }
                tagonly = 1;  /* insert tag only to existing entry */
            }
            else
            {
                /* This is for non index B-tree entries.  When the same
                key exists, the operation become a replacement of the
                INFO only.  The new entry have the same key and the new
                INFO.  */
                diddelete = 1;  /* need to delete the current entry in order
                to replace it with the new one */
		is = IS_GET(pos);
                if ((pfndp->pblkbuf->IXLENENT + (pfndp->indatasz - is))
                                        > (int)MAXIXCHRS)
                    /* unsafe - no space for the new entry, must split */
                    unsafe = 1;
            }
        }
        else
        {
            /* the same key was NOT found */
            if ((peap->flags & (CXUNIQE | CXTAG)) == (CXUNIQE | CXTAG))
            {
                /* same key was not found, but in the case of unique key
                the input key was compared without the 3 bytes to the key
                in the index that includes the 3 bytes, therefore even when
                the "real" key is the same we fall here were notfound = 1.
                We need to further check if the real key (without the 3 bytes)
                is the same.
                Here we check for duplicate uniqe key */

#ifdef VARIABLE_DBKEY
                dbkeySizeInKey = peap->pinpkey[ peap->inkeysz - 1 ] & 0x0f; /* last nibble of DBKEY */
#else
                dbkeySizeInKey = 3;/* 3 dbk bytes */
#endif  /* VARIABLE_DBKEY */
                if (pfndp->csofnext >= (UCOUNT)(peap->inkeysz - dbkeySizeInKey)) 
                {
                    /* the real key ( without the  dbk bytes) is the same */

                    /* check for index lock - negative dbk */
                    pos = pfndp->position;
/*                    pos++; */
/*                    ks = *pos++;     pos point to IS at this point */
                    ks = KS_GET(pos);   
					pos += HS_GET(pos) + ks;  /* pos point to IS at this point */
#ifdef VARIABLE_DBKEY
/*                    pos += ks;   to the last byte of the dbkey in the key  */
                    pos--;  /* to the last byte of the dbkey in the key */
                    dbkeySizeInKey = *pos & 0x0f; /* last nibble of DBKEY */
                    pos = pos + 1 - dbkeySizeInKey;/* to most significant dbk byte */
                    if (( *pos >> 4 ) < 8)  /* exponent of negative DBKEY < 8 */
#else
/*                    pos = pos + ks + 1 - 3;  to most significant dbk byte  */
                    pos = pos - 3;/* to most significant dbk byte */
                    if (xlng(pos) < 0) /*** Used to be:  if (*pos & 0x80) ****/
#endif  /* VARIABLE_DBKEY */
                    {
                        /* negative dbk */
#ifdef VARIABLE_DBKEY
                        gemDBKEYfromIdx( pos, &transNumber );
/*====Rich's change=============================== */
/* gemDBKEYfromIdx already gets the last byte of the dbkey */
/*			transNumber <<= 8; */
/*			transNumber += pos[dbkeySizeInKey] */
/*------------------------------------------------ */
                        transNumber = -transNumber;
			
#else
                        transNumber = -xlng(pos);
#endif  /* VARIABLE_DBKEY */
                        ret = lkCheckTrans(pcontext, (LONG)transNumber, LKEXCL);
                        if (ret)
                        {
                            peap->errcode = ret;
                            goto fail;/* no replace */
                        }
                        /* found same key but it is mine or dead transaction */
                        diddelete = 1; /* negative but not locked, delete it */
                    }
                    else
                    {
                        peap->errcode = IXDUPKEY;
                        goto fail1;/* no replace */
                    }
                }
                /******no need to look at any other block, even if we fall
                  at the end of the block.  Because it is a unique index and because
                  of the trailing compression and because we search for the key
                  without the DBK, we are guarantied to fall in the block where another
                  entry of same key resides (if exists at all) ***************/
                
            }  /* end of unique */
        } /* end of not found */
        /* now do all the work */

        STATINC (ixgnCt);

        if (unsafe || (!diddelete && 
		       cxCheckInsert(pcontext, pfndp, tableNum, tagonly)))
        {
            /* unsafe - no space */

            if (cxfast)
            {
                /* if the CXREBLD flag is set, free all blocks and start */
                /* again from the root */
                cxfast = 0;
                cxReleaseAbove(pcontext, pdownlst);     /* release all predecessors */
                bmReleaseBuffer(pcontext, pfndp->bufHandle); /* release the blk */
                goto again;
            }
            /* upgrade current & predecessors to EXCLUSIVE */
            cxUpgradeLock(pcontext, pdownlst);
            if (diddelete)
            {
                /* avoid logical note of delete, it is part of the put */
                pfndp->flags |= CXNOLOG;
                cxRemoveEntry(pcontext, tableNum, pfndp);
                pfndp->flags &= ~CXNOLOG;
                cxFindEntry(pcontext, pfndp); /* to establish new cxofnext */
            }

            /***** SPLIT *******/
            ret = cxSplit(pcontext, peap, pfndp, pdownlst, tableNum, tagonly);
            cxReleaseAbove(pcontext, pdownlst); /* release all predecessors */
            /* BUM - Assuming DBKEY root > 0 */
            if(pfndp->blknum != (DBKEY)root)
                bmReleaseBuffer(pcontext, pfndp->bufHandle); /* not done for split
                                      of 1st blk that was both root and leaf */
            if (pcursor)
                pcursor->blknum = 0;

            if (ret)    /* for a large entry the split may fail 1st time */
                goto again;

            rlTXEunlock (pcontext);

            return 0;
        }
        else
        {
            /* safe */
            cxReleaseAbove(pcontext, pdownlst); /* release all predecessors */
            cxUpgradeLock(pcontext, pdownlst); /* upgrade current */
            if (diddelete)
            {
                /* avoid logical note of delete, it is part of the put */
                pfndp->flags |= CXNOLOG;
                cxRemoveEntry(pcontext, tableNum, pfndp);
                pfndp->flags &= ~CXNOLOG;
                cxFindEntry(pcontext, pfndp); /* to establish new cxofnext */
            }
            cxInsertEntry(pcontext, pfndp, tableNum, tagonly);  /* insert the new entry */
            if (pcursor)
                pcursor->blknum = pfndp->blknum;
        }


        ret = 0;
 release:
        bmReleaseBuffer(pcontext, pfndp->bufHandle); /* release the current
                                                     block buffer */
        rlTXEunlock (pcontext);
        return ret;
fail1:
        ret = peap->errcode;
fail:
        cxReleaseAbove(pcontext, pdownlst); /* release all predecessors */
        goto release;

}       /* cxPut */

/*************************************************************************/
/* PROGRAM:  cxRemove - Delete the given entry
 *      Input Parameters
 *      ----------------
 *      peap->flags - must have CXDEL | CXTAG
 *                  (only CXDEL to delete a non index entry with no TAG)
 *
 *      peap->pkykey - must points to a key key where:
 *
 *              peap->pkykey->keystr[0] = total length of keystr
 *              peap->pkykey->keystr[1] = length of key including
 *                                          indexnum, key value, NULL,  most
 *                                          significant bytes of dbkey.
 *              peap->pkykey->keystr[2] = length of INFO. Usualy 1
 *                                              which is the length of the TAG.
 *              Followed by the KEY including indexnum, key value, NULL and
 *                      most significant bytes of dbkey.
 *              Followed by the TAG (least significant byte of the dbkey).
 *
 *
 *      peap->pinpkey - points to the beginnig of the key which is
 *              &peap->pkykey->keystr[3]
 *
 *      peap->pdata - points to the TAG in the key.
 *
 *      peap->inkeysz - length of the key as in peap->pkykey->keystr[1]
 *
 *      peap->datasz - length of INFO as in peap->pkykey->keystr[2]
 *
 * RETURNS:
 *          0       - Success
 *          DSM_S_FNDFAIL - Failure
 */

int
cxRemove(
        dsmContext_t    *pcontext,
        dsmObject_t      tableNum,
        cxeap_t         *peap)
{
        findp_t  *pfndp;        /* pointer to the FIND P. B. */
        TEXT     *pos;
        findp_t   fndp;         /* parameter block for the FIND */
        downlist_t  dlist;                        /* temporary list down */
        downlist_t *pdownlst= &dlist;     /* to temporary list down */
        inlevel_t  *plevel;
        int       ixnum;
        CXNOTE    note;
#ifdef VARIABLE_DBKEY
        TEXT        task[sizeof(DBKEY) + 1];
        int         dbkeySize;
        DBKEY       dbkey;
#else
        TEXT      task[4];
#endif  /* VARIABLE_DBKEY */
        int       ret;
        ULONG     retvalue;     /* return value from latSetCount() */
        int       counttype,    /* 0=waitcnt, 1=lockcnt */
                  codetype;     /* type of wait */

	INIT_S_FINDP_FILLERS( fndp ); /* bug 20000114-034 by dmeyer in v9.1b */

        pfndp = &fndp;  /* load pointer to the FIND P.B. */
        pfndp->pkykey = peap->pkykey;

        CXINITLV(pdownlst);             /* initialize the list down level */
        plevel = &pdownlst->level[0];
        ixnum = peap->pkykey->index;
        
        pdownlst->area = peap->pkykey->area;

        /* Start a micro-transaction  */ 
        if ( !(peap->flags & CXNOLOG) )
        {
            if( peap->flags & CXLOCK )
                /* Get an update txe lock.  We have to do this because
                   of the purely logical RL_IXDEL note whose action
                   actually gets taken by the subsequent RL_BKREPL note.
                   If we don't do this then we may get ixundo ixgen errors
                   during database crash recovery if the IXDEL note
                   gets written
                   but we crash before writing the BKREPL note.          */
                rlTXElock (pcontext, RL_TXE_UPDATE, RL_KEY_DELETE_OP);
            else
                rlTXElock (pcontext, RL_TXE_SHARE, RL_KEY_DELETE_OP);
        }

        /* this needs to be replaced with information passed in */
        plevel->dbk = peap->pkykey->root; /* start with root block */


        if (cxSearchTree(pcontext, peap, pfndp, pdownlst))
        {
            /* the search key does not exist */
            ret = DSM_S_FNDFAIL;
            goto relup; /* release all block locks and return */
        }

        pos = pfndp->position; /* point to the found entry */

        if (peap->flags & CXTAG) /* if fnd tag */
        {
            if (cxFindTag(pos, *peap->pdata, CXFIND))
            {
                ret = DSM_S_FNDFAIL;    /* the tag was not found */
                goto relup;     /* release all block locks and return */
            }
            pfndp->pinpdata = peap->pdata; /** needed for the rlnotes */
        }


        STATINC (ixdlCt);
        if (peap->flags & CXLOCK)
        {
            cxReleaseAbove(pcontext, pdownlst); /* release all predecessors */
            cxUpgradeLock(pcontext, pdownlst); /* upgrade current */

            /* lock the unique index entry until the end of transaction */
            /* the dbkey of the record is replaced by the negative task # */
            INITNOTE( note, RL_IXDEL, RL_LOGBI );
            note.indexRoot = peap->pkykey->root;

            if( peap->pkykey->unique_index)
                note.indexAttributes = DSMINDEX_UNIQUE;
            else if ( peap->pkykey->word_index )
                note.indexAttributes = DSMINDEX_WORD;
            else
                note.indexAttributes = 0;
	    note.tableNum = tableNum;
            slng((TEXT *)&note.rlnote.rlArea,peap->pkykey->area);
#ifdef VARIABLE_DBKEY
            dbkeySize = *(peap->pdata - 1) & 0x0f;
/*====Rich's change=============================== */
/*            gemDBKEYfromIdx( peap->pdata - dbkeySize, &dbkey );   part of dbkey from the key  */
/*            dbkey <<= 8; */
/*            dbkey += *peap->pdata;   the last byte of the dbkey  */
/*            sdbkey( &note.sdbk[0], dbkey );  dbkey to the note (big indian) as in the past  */
 	        bufcop(&note.sdbk[sizeof(note.sdbk) - (dbkeySize + 1)], 
 		        peap->pdata - dbkeySize, dbkeySize + 1);   
/*------------------------------------------------ */
#else
            bufcop(&note.sdbk[0], peap->pdata - 3, 4);
#endif  /* VARIABLE_DBKEY */

            pos = &peap->pkykey->keystr[0];
            rlLogAndDo(pcontext, (RLNOTE *)&note, 0, xct(&pos[TS_OFFSET]), pos );
#if SHMEMORY
            /* collect stats on locks*/
            pcontext->pusrctl->lockcnt[TSKWAIT]++;
            counttype = 1;
            codetype = TSKWAIT;
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
#endif
            pos = pfndp->position; /* point to the found entry */
            pos += ENTRY_SIZE_COMPRESSED(pos); /* to next entry */
#ifdef VARIABLE_DBKEY
            pos -=  2; /* to last byte of the key */
            dbkeySize = *pos & 0x0f; /* size of the dbkey part in the key, without the last byte */
            pos -= (dbkeySize - 1); /* to start of the variable dbkey */
            dbkey = -(pcontext->pusrctl->uc_task);
            dbkeySize = gemDBKEYtoIdx( dbkey >> 8,  /* the 32 or 64 bits dbkey - less last byte*/
                    dbkeySize ,  /* preserve the size of the original dbkey */
		                &task[0] ); /* ptr to area receiving the Variable size DBKEY */
            task[dbkeySize] = (TEXT)dbkey; /* last byte of the transaction number */
            bkReplace(pcontext, pfndp->bufHandle, pos, &task[0], (COUNT)dbkeySize + 1);
#else
/*            pos += ENTHDRSZ + pos[1] + pos[2];  to next entry  */
            slng(&task[0],-(pcontext->pusrctl->uc_task));
            bkReplace(pcontext, pfndp->bufHandle, pos-4, &task[0], (COUNT)4);
#endif  /* VARIABLE_DBKEY */
            /* chain, put it on mine */
            if ( pfndp->pblkbuf->BKFRCHN == NOCHN)
            {
               if ( cxCheckLocks(pcontext, pfndp->bufHandle ) )
               {
                  /* Block only contains delete lock entries so
                     put in on the bottom of the index delete chain */
                    bkrmbot(pcontext, pfndp->bufHandle);
               }
            }
        }
        else
        if (cxCheckDelete(pfndp) )
        {
            /* unsafe */
            cxUpgradeLock(pcontext, pdownlst); /* upgrade predecessors to EXCLUSIVE */
            if (peap->flags & CXNOLOG)
                pfndp->flags |= CXNOLOG; /* for cxdlkcs */
            cxRemoveEntry(pcontext, tableNum, pfndp);  /* delete the last entry, for logical note */
            cxRemoveBlock(pcontext, peap, pfndp, tableNum, pdownlst); /* remove the empty block */
            cxReleaseAbove(pcontext, pdownlst); /* release all predecessors */
        }
        else
        {
            /* safe */
            cxReleaseAbove(pcontext, pdownlst); /* release all predecessors */
            cxUpgradeLock(pcontext, pdownlst); /* upgrade current */
            cxRemoveEntry(pcontext, tableNum, pfndp);  /* delete the entry */
        }

        ret = 0;
 release:
        bmReleaseBuffer(pcontext, pfndp->bufHandle); /* release the block buffer */
        if ( !(peap->flags & CXNOLOG) )
            rlTXEunlock (pcontext);
        return ret;
 relup:
        cxReleaseAbove(pcontext, pdownlst); /* release all predecessors */
        goto release;

}       /* cxRemove */

/****************************************************************************/
/* PROGRAM: cxGet - Traverses the cxtree and finds the entry with a given key.
 *          This routine is used for exact matches.
 *          Returns the data part of the entry if required.
 *
 *
 *      Input Parameters
 *      ----------------
 *
 *      peap->flags - must have CXFIND | CXTAG to check if entry exists
 *                  - must have CXGET to find with given key and return INFO
 *
 *      peap->pinpkey - pointer to the searched key.
 *
 *      peap->inkeysz - size in bytes of the searched key.
 *
 *      peap->pdata - pointer to area to receive the INFO.
 *
 *      peap->pcursor - NULL or pointer to a cxcursor to be set by cxGet.
 *
 *      peap->datamax - maximum size of INFO to be returned.
 *
 *      Output Parameters
 *      -----------------
 *      peap->datasz - returns the size of the returned INFO
 *
 * RETURNS: 0  - Success
 *          1  - Failure; entry was not found
 *
 *          ALSO SEE Output Parameters (above).
 */
int
cxGet(
      dsmContext_t    *pcontext,
      cxeap_t         *peap)          /* pointer to the input EAP */
{
        findp_t         fndp;           /* parameter block for the FIND */
        findp_t         *pfndp;         /* pointer to the FIND P. B. */
        cxcursor_t      *pcursor;
        downlist_t      dlist;          /* temporary list down */
        downlist_t      *pdownlst = &dlist;     /* to temporary list down */
        inlevel_t       *plevel;
        int             notfound;
        int             ixnum;
        TEXT            *pos;

    INIT_S_FINDP_FILLERS( fndp ); /* bug 20000114-034 by dmeyer in v9.1b */

    pfndp = &fndp;                      /* load pointer to the FIND P.B. */

    CXINITLV(pdownlst);                 /* initialize the list down level */
    
    plevel = &pdownlst->level[0];

    pcursor = peap->pcursor;            /* set up the cusror key */

    ixnum = xct(&peap->pinpkey[2]);

    pdownlst->area = peap->pkykey->area;
    plevel->dbk = peap->pkykey->root;

    notfound = cxSearchTree(pcontext, peap, pfndp, pdownlst);   /* traverse down the b-tree */
    cxReleaseAbove(pcontext, pdownlst);         /* release all predecessors */

    /* fill in the cursor if there is one. */
    if (pcursor)
    {
        pcursor->flags &= ~(CXBACK | CXFORWD);  /* remove old flags */
        pcursor->updctr = pfndp->pblkbuf->BKUPDCTR;     /* update counter */
        pcursor->blknum = peap->blknum;
        pcursor->ixnum = pfndp->pblkbuf->IXNUM;
        pos = pfndp->position;                  /* point to the found entry */
        pcursor->offset = pos - pfndp->pblkbuf->ixchars;

/*        cxSetCursor(pcontext, peap, */
/*                    (COUNT)(peap->inkeysz + ENTHDRSZ + peap->datasz), 0); */
        cxSetCursor(pcontext, peap, 
                    (COUNT)(peap->inkeysz + FULLKEYHDRSZ + peap->datasz), 0);

        bufcop(peap->poutkey, peap->pinpkey, (int)(peap->inkeysz+peap->datasz));
        peap->outkeysz = peap->inkeysz;

/*        pcursor->pkey[0] = peap->inkeysz + ENTHDRSZ + peap->datasz; */
/*        pcursor->pkey[1] = (TEXT)peap->inkeysz; */
/*        pcursor->pkey[2] = (TEXT)peap->datasz; */
        sct(&pcursor->pkey[TS_OFFSET] , peap->inkeysz + FULLKEYHDRSZ + peap->datasz);
        sct(&pcursor->pkey[KS_OFFSET] , (TEXT)peap->inkeysz);
        sct(&pcursor->pkey[IS_OFFSET] , (TEXT)peap->datasz);
    }

    if (notfound)
        goto failed;

    pos = pfndp->position;              /* point to the found entry */
    if (peap->flags & CXTAG)            /* if find tag */
        if (cxFindTag(pos, *peap->pdata, CXFIND))
            goto failed;                /* the tag was not found */

    peap->bftorel = pfndp->bufHandle;
    return 0;

failed:
    if (pcursor)
        pcursor->flags |= CXFORWD;
    bmReleaseBuffer(pcontext, pfndp->bufHandle); /* release the block buffer */
    peap->bftorel = 0;
    return 1;
}       /* cxGet */

/****************************************************************************/
/* PROGRAM: cxNextPrevious - get next or prev entry from the cxtree
 *    Two modes: 1 - with cursor: the cursor is used to find next/prev.
 *             2 - without a cursor: the next/prev to the given key if found.
 *
 *  The cursor is used only if the block pointed by it was not
 *  modified since the cursor was set. Other wise the search
 *  starts at the root of the tree, using the saved key from the cursor.
 *
 *      Input Parameters
 *      ----------------
 *
 *      peap->flags - must have CXNEXT to get the next KEY
 *                  - must have CXGETNEXT to get the next KEY and INFO
 *                  - must have CXNEXT+CXTAG to get the next KEY and TAG
 *                  - must have CXPREV to get the next KEY
 *                  - must have CXGETPREV to get the next KEY and INFO
 *                  - must have CXPREV+CXTAG to get the next KEY and TAG
 *
 *                  In addition if CXSUBSTR is set, a substring search
 *                  is done.
 *
 *      peap->pcursor - NULL if not used.
 *                    - pointer to cxcursor to be used in the operation.
 *
 *      peap->pinpkey - pointer to the searched key.
 *
 *      peap->inkeysz - size in bytes of the searched key.
 *
 *      peap->substrsz - length of the substring to be searched.
 *
 *      peap->pdata - pointer to area to receive the INFO if needed.
 *
 *      peap->pcursor - NULL or pointer to a cxcursor to be set by cxGet.
 *
 *      peap->datamax - maximum size of INFO to be returned - rest is truncated.
 *
 *      peap->poutkey - pointer to area to receive the next/prev key.
 *
 *      peap->mxoutkey - maximum size of KEY to be returned - rest is truncated.
 *
 *      Output Parameters
 *      -----------------
 *      peap->datasz - returns the size of the returned INFO
 *      peap->outkeysz - returns the size of the returned KEY
 *
 * RETURNS: 0       -  Success
 *          DSM_S_ENDLOOP -  no next/prev exists.
 *
 *          ALSO SEE Output Parameters (above).
 */

int
cxNextPrevious(
        dsmContext_t    *pcontext,
        cxeap_t         *peap)          /* pointer to the input EAP */
{
        dbcontext_t*    pdbcontext = pcontext->pdbcontext;
        findp_t         fndp;           /* parameter block for the FIND */
        findp_t         *pfndp;         /* pointer to the FIND P. B. */
        TEXT            *pos;
/*        int             cs, ks, is; */
        int             cs, ks, is, hs;
        cxcursor_t      *pcursor;
        int             usecurs = 0;


    INIT_S_FINDP_FILLERS( fndp ); /* bug 20000114-034 by dmeyer in v9.1b */

    pfndp = &fndp;      /* load pointer to the FIND P.B. */

    if ( pdbcontext->pidxstat &&
         (pdbcontext->pdbpub->isbaseshifting ==0 ) &&
         xct(&peap->pinpkey[2]) >= pdbcontext->pdbpub->argindexbase &&
         xct(&peap->pinpkey[2]) < pdbcontext->pdbpub->argindexbase +
                                  pdbcontext->pdbpub->argindexsize )
       ((pdbcontext->pidxstat)[ xct(&peap->pinpkey[2])  - 
                              pdbcontext->pdbpub->argindexbase ]).read++;


    pfndp->position = cxPositionToNext(pcontext, peap, pfndp);
    pos = pfndp->position;
    if (pos == NULL)
        goto fail;

    /* get cs, ks, is  from the found key*/
/*    cs = *pos++;   number of compressed bytes  */
/*    ks = *pos++;   number of bytes present     */
/*    is = *pos++;   number of information bytes */
    CS_KS_IS_HS_GET(pos);
    pos += hs;
    pcursor = peap->pcursor;
    if (pcursor)
        pcursor->flags &= ~(CXBACK | CXFORWD);  /* remove old flags */

    if ((peap->flags & CXTAG) &&                /* if fnd tag */
        (peap->state == CXSucc))                /* same key exists */
    {
        if (pcursor)
        {
/*            peap->poutkey = pcursor->pkey + ENTHDRSZ; */
/*            peap->outkeysz = pcursor->pkey[1]; */
            peap->poutkey = pcursor->pkey + FULLKEYHDRSZ;
            peap->outkeysz = xct(&pcursor->pkey[KS_OFFSET]);
        }
        goto success;                   /* no need to return key nor data */
    }

    /* setup the cursor */
    if (pcursor)
    {
        /* the cursor needs to be updated */
        if (pcursor->blknum)            /* if the cursor is valid */
            usecurs = 1;
        pcursor->updctr = pfndp->pblkbuf->BKUPDCTR; /* update counter */
        pcursor->blknum = peap->blknum;
        pcursor->ixnum = pfndp->pblkbuf->IXNUM;
        pcursor->offset = pfndp->position - pfndp->pblkbuf->ixchars;

        /* setup cursor to save the key */
/*        cxSetCursor(pcontext, peap, (COUNT)(cs + ks + ENTHDRSZ + 1), cs); */
        cxSetCursor(pcontext, peap, (COUNT)(cs + ks + FULLKEYHDRSZ + 1), cs);
    }

    if (peap->flags & (CXNEXT | CXFIRST))       /* Get next */
    {

        /* for compare substr only */
        if (peap->flags & CXSUBSTR)
        {
            if (pfndp->csofnext < peap->substrsz)
                goto fail_release;
        }

        if ((peap->poutkey) &&
            (cs != 0))          /* if CS != null, expand the compressed part */
        {                       /* which is taken from the search key */
            peap->outkeysz = ((int)peap->mxoutkey < cs) ? peap->mxoutkey : cs;
            /* peap->outkeysz gets the min of the CS or the max out-key */
            if (!usecurs)       /* if old key  not already there */
                bufcop(peap->poutkey, peap->pinpkey, (int)peap->outkeysz);
        }
    }
    else                /* Get previous */
    {
        if (ks == 0)    /* KS=0, it is the dummy lowest key, no prev. */
            goto fail_release;

        /* for compare substr only */
        if (peap->flags & CXSUBSTR)
            if (pfndp->csofprev < peap->substrsz)
                goto fail_release;

        if (peap->poutkey)
        {
            if (cs != 0)           /* CS not null */
            {
                /*******expand the key   */
                peap->outkeysz = ((int)peap->mxoutkey < cs) ? peap->mxoutkey 
                                                            : cs;
                /* peap->outkeysz gets the min of the CS or the max out-key */

                cxExpandKey(pcontext, peap->poutkey, pfndp->pblkbuf->ixchars,
                            pfndp->pprv, (int)peap->outkeysz);
            }
        }
    }
    /* move the key from the entry to the output */
    if (peap->poutkey)
        cxGetKey(pcontext, peap, pos, cs, ks);

    if (!(peap->flags & CXTAG) /* if NOT fnd tag */
    && (peap->flags & CXRETDATA)) /* return data? */
    {
        pos += ks;         /* point to the data */
        cxGetInfo(pcontext, peap, pos, is);          /* return the data */
    }
    else
        peap->datasz = is;

success:

    peap->bftorel = pfndp->bufHandle;
    return 0;

fail_release:
    bmReleaseBuffer(pcontext, pfndp->bufHandle); /* release the block buffer */
fail:
    peap->bftorel = 0;
    return DSM_S_ENDLOOP;

}       /* cxNextPrevious */

/*************************************************************************/
/* PROGRAM: cxCheckDelete - check if the delete will empty the block
 *
 * RETURNS: 0  - if safe; cannot empty the block 
 *          1  - if not safe
 */
LOCALF int
cxCheckDelete(
        findp_t         *pfndp) /* to parameter block of the find */
{
        TEXT    *pos;   /* address of current pos */
        int     is;

    pos = pfndp->position;

    /* get data size for key */
/*    is = *(pos + 2); */
    is = IS_GET(pos);

    if(pfndp->pblkbuf->IXBOT && pfndp->flags & CXTAG)
    {
        /* in leaf node, is there more then one record for this key */
        if (is > 1)
            return 0; /* safe */
    }

    /* compare the length of the first entry to the length of bytes in 
       the block */
    pos = pfndp->pblkbuf->ixchars;      /* start of 1st entry */
    /* if only one entry */
 /*   if ( *(pos + 1) + *(pos + 2) + ENTHDRSZ == pfndp->pblkbuf->IXLENENT) */
    if ( ENTRY_SIZE_COMPRESSED(pos) == pfndp->pblkbuf->IXLENENT)
        return 1;
    else
        return 0;

}       /* cxCheckDelete */

/*************************************************************************/
/* PROGRAM: cxCheckInsert - check if there is enough space for the insert
 * NOTICE:  both inserted key and the next key may have to be compressed
 *
 * RETURNS: 0 if enough space
 *          1 if not enough
 */
LOCALF int
cxCheckInsert(
        dsmContext_t    *pcontext,
        findp_t         *pfndp,   /* to parameter block of the find */
	dsmObject_t      tableNum, 
        int             tagonly)  /* 1 if insert tag only to existing entry */

{
        TEXT      *pos;   /* address of current pos */
        cxblock_t *pixblk;
        int       cs, is;
        int       inslng, extrcmpr;
        TEXT      *pfreepos;/*1st free position past end of the text in the blk*/
        LONG      deletedLocks = 0;
        int	  dbkeySizeInKey;

    pixblk = pfndp->pblkbuf;

    for (;;)
    {
        pfreepos = pixblk->ixchars + pixblk->IXLENENT;
        pos = pfndp->position;
        /* if this operation is only adding a data byte */
        if(tagonly)
        {       
            /* get the size of the data */
/*            is = *(pos + 2); */
            is = IS_GET(pos);

            if (is == 33)  /* 33 indicates that the data is bit mapped and there 
                              must be room */
                return 0; /* safe */
            else
                inslng = 1;
        }
        else
        {
           /* Compute the length of insertion */
/*           inslng = pfndp->inkeysz + pfndp->indatasz  size of key and data */
/*              + ENTHDRSZ  for cs, ks, is  */
/*              - pfndp->csofprev;  compression */
           inslng = pfndp->inkeysz + pfndp->indatasz  /*size of key and data */
              - pfndp->csofprev;/* compression*/
           if (pfndp->inkeysz > MAXSMALLKEY)
               inslng += LRGENTHDRSZ;
           else
               inslng += ENTHDRSZ;
           /* check if next entry is to be compressed */
           if (pos != pfreepos) /*insert pos. not at end, next exist*/
           {
/*              cs = *pos; */
              cs = CS_GET(pos);
              extrcmpr = pfndp->csofnext - cs;  /* next CS - current CS */
              inslng -= extrcmpr;   /* reduce length by extra comp. */
           }
        }

        if ((int)(MAXIXCHRS - pixblk->IXLENENT) < inslng)
        {
           if( deletedLocks || ((pfndp->flags & CXUNIQE) == 0) || !pixblk->IXBOT )
           {
              return 1;  /* not enough space  */
           }
           else
           {
              /* See if we can remove any index hold entries to avoid
                 the block split.
                 -- these entries are only in unique indexes.              */
              deletedLocks = cxDelLocks(pcontext, pfndp->bufHandle,
                              tableNum, (int)1 /* Upgrade buffer lock */ );
              if ( deletedLocks )
              {
                  /* We deleted something so we have to refind our position
                     in the block less the bytes of dbk.          */
#ifdef VARIABLE_DBKEY
                  dbkeySizeInKey = pfndp->pinpkey[ pfndp->inkeysz - 1 ] & 0x0f; /* last nibble of DBKEY */
#else
                  dbkeySizeInKey = 3;/* 3 dbk bytes */
#endif  /* VARIABLE_DBKEY */
                  pfndp->inkeysz -= dbkeySizeInKey;/* for uniqe check key without dbk bytes */
                  cxFindEntry(pcontext, pfndp);
                  pfndp->inkeysz += dbkeySizeInKey;
              }
              else
              {
                  return 1;  /* not enough space */
              }
           }
        }
        else
        {
           return 0;
        }
    }

}       /* cxCheckInsert */

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/******************************************************************************
                B-TREE MANAGER 
B-tree operations:
cxSearchTree - traverse down the index B-tree
cxSplit - split a block
cxFindEntry - find an entry in a given block
cxInsertEntry - insert a given entry at the current pos in a block 
cxReplaceInfo - replace the info of the current position 
cxRemoveEntry - delete a given entry from the current pos in the block 


******************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/*********************************************************/
/* PROGRAM: cxSearchTree - traverse down the index B-tree
 *           starting at a given node block, traverse down
 *
 * RETURNS:     0 the search key was found
 *              1 the search key was not found
 *
 */
LOCALF int
cxSearchTree(
        dsmContext_t    *pcontext,
        cxeap_t         *peap,          /* to Entry Access Parameters */
        findp_t         *pfndp, /* to parameters for the FIND */
        downlist_t        *pdownlst)      /* to list down, with starting level */
                                        /* and starting dbk already set by   */
                                        /* caller                            */
{
        TEXT      *pos;          /* address of the position in the block */
        TEXT      *pnxtprv;      /* the position of nxt or prev entry */
        TEXT      *pfreepos;/*1st free position past end of the text in the blk*/
        cxbap_t   bap;          /* block access parameters */
        DBKEY     pushval;      /* value to be pushed on stack */
        UCOUNT    lockmode;
        inlevel_t   *plevel;
        int       safe;
        int       ret;
        cxblock_t *pixblk;
        int       ixnum;
        dbcontext_t*  pdbcontext = pcontext->pdbcontext;


    /* initialize parameters */
    plevel = &pdownlst->level[pdownlst->currlev];       /* pointer to the level info */
    bap.blknum = plevel->dbk;           /* blknum of starting level block */
    pfndp->pinpkey = peap->pinpkey;
    pfndp->inkeysz = peap->inkeysz;
    pfndp->flags = (UCOUNT)peap->flags; /* BUM - CAST */
    if (peap->flags & (CXPUT | CXDEL | CXKILL))
        lockmode = TOREADINTENT;
    else
        lockmode = TOREAD;


    ixnum = xct(&peap->pinpkey[2]);
    switch(peap->flags & CXOPMASK)
    {
      case CXPUT:
        if ( pdbcontext->pidxstat &&
         (pdbcontext->pdbpub->isbaseshifting ==0 ) &&
         ixnum >= pdbcontext->pdbpub->argindexbase &&
         ixnum < pdbcontext->pdbpub->argindexbase +
                 pdbcontext->pdbpub->argindexsize )
         ((pdbcontext->pidxstat)[ ixnum -
                                 pdbcontext->pdbpub->argindexbase ]).create++;
        break;
      case CXDEL:
        if ( pdbcontext->pidxstat &&
         (pdbcontext->pdbpub->isbaseshifting ==0 ) &&
         ixnum >= pdbcontext->pdbpub->argindexbase &&
         ixnum < pdbcontext->pdbpub->argindexbase +
                 pdbcontext->pdbpub->argindexsize )
         ((pdbcontext->pidxstat)[ ixnum -
                                 pdbcontext->pdbpub->argindexbase ]).delete++;
        break;
      case CXGET:
        if ( pdbcontext->pidxstat &&
         (pdbcontext->pdbpub->isbaseshifting ==0 ) &&
         ixnum >= pdbcontext->pdbpub->argindexbase &&
         ixnum < pdbcontext->pdbpub->argindexbase +
                 pdbcontext->pdbpub->argindexsize )
         ((pdbcontext->pidxstat)[ ixnum -
                                 pdbcontext->pdbpub->argindexbase ]).read++;
        break;
    }

    /* Each pass through this loop will move one level down the tree.
       This terminates when a leaf block is reached                 */
    for(;;)
    {
        /* read the next block to be examined */
        bap.bufHandle = bmLocateBlock(pcontext, pdownlst->area,bap.blknum,
                                      lockmode);
 

        bap.pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,bap.bufHandle);
        pixblk = bap.pblkbuf;
     
        pfndp->bufHandle = bap.bufHandle; /* Handle to buffer in buffer pool */
        pfndp->pblkbuf = pixblk;        /* point to the block */
        pfndp->blknum = bap.blknum;     /* blknum of the block */

        ret = cxFindEntry(pcontext, pfndp);     /* search for the key in the block */

        if (pixblk->IXBOT)              /* if leaf block */
        {
            peap->blknum = bap.blknum;
            return ret;
        }
        pfreepos = pixblk->ixchars + pixblk->IXLENENT;
        FASSERT_CB(pcontext, *(pixblk->ixchars+1) <= (TEXT)1,
                   "Bad index structure, rebuild index");
        /* it is a non-leaf block */
        if (ret && pfndp->pprv != 0)
        {
            /* if exact key not found and there is a previous entry*/
            /* no previous entry is possible if key was deleted without*/
            /* update of upper level                      */
            pos = pfndp->pprv;/* point to the previous entry to go down*/
            pnxtprv = pfndp->pprvprv;/* point to prev. of prev. as prev. */
        }
        else
        {
            pos = pfndp->position; /* point to the current entry to go down */
            pnxtprv = pfndp->pprv; /* point to prev. entry */
        }
        cxGetDBK(pcontext, pos, &bap.blknum); /* get blknum of block to go down with */


        /* check if the block is "SAFE" */
        safe = 1; /* assume safe */
        pushval = -1;   /* assume no prev nor next */

        switch (peap->flags & CXOPMASK)
        {
        case CXPUT:             /* check for safe insert */
#ifdef VARIABLE_DBKEY
/*            if ((MAXIXCHRS - pixblk->IXLENENT) < (MAXKEYSZ +12)) */
      /**** THIS SHOULD BE IMPROVED  checking for max key is over kill????? - Amnon 12/26/97 */
            if ((MAXIXCHRS - pixblk->IXLENENT) < (MAXKEYSZ + LRGENTHDRSZ + 9))
#else
            if ((MAXIXCHRS - pixblk->IXLENENT) < (MAXKEYSZ +7))
#endif  /* VARIABLE_DBKEY */
            {
                safe = 0; /* not safe - not enough space for max size entry */
            }
            break;

        case CXDEL:             /* check for safe delete */
            pos = pixblk->ixchars;      /* start of 1st entry */
/*            if (*(pos + 1) + *(pos + 2) + ENTHDRSZ == pixblk->IXLENENT) */
            if (ENTRY_SIZE_COMPRESSED(pos) == pixblk->IXLENENT)
            {
                safe = 0; /* not safe - last entry in blk may be removed */
            }
            break;

        case CXNEXT:            /* check for safe next */
        case CXFIRST:           /* check for safe next */
            /* skip to next entry */
/*            pnxtprv = pos + *(pos+1) + *(pos +2) + ENTHDRSZ;  to next entry  */
            pnxtprv = pos + ENTRY_SIZE_COMPRESSED(pos);     /*to next entry */
            if (pnxtprv == pfreepos)      /* there is no next entry */
                pnxtprv = 0;

        case CXPREV:            /* check for safe previous */
        case CXLAST:
            if (pnxtprv)        /* there is a previous entry */
                cxGetDBK(pcontext, pnxtprv, &pushval); /* pushval = previous block */
            else
                safe = 0;       /* not safe - no previous entry in the block */
            break;

        case CXKILL:
            safe = 0;           /* not safe */
            break;
        }

        if (safe)
            cxReleaseAbove(pcontext, pdownlst); /* release all predecessors */

        plevel++;       /* to next level of the B-tree */
        pdownlst->currlev++;    /* to next level of the B-tree */
        pdownlst->locklev++; /* count locked predecessor levels */

        plevel->nxtprvbk = pushval; /* next or previous dbk to list down */
        plevel->dbk = bap.blknum; /* current block to list down */
    }

}       /* cxSearchTree */


/*************************************************************************/
/* PROGRAM:  cxSplit - split a block
 *  Recursively called function to split a block and update
 *  the level above, possibly to split it, and so on and so on.
 *  This procedure expects all the "unsafe" blocks in the path
 *  to be already locked EXCLUSIVE and in the memory buffers,
 *  done by the caller that traversed down the B-tree and checked
 *  for "UNSAFE" blocks. This procedure does not release these
 *  blocks either, it expect the caller to do so after the split.
 *  Only new blocks that were generated by the split are locked
 *  and released by this procedure.
 *
 *  The split is done for two reasons:
 *  1 - a new entry is to be inserted in the b-tree block, but
 *      there is not enough space for it.
 *  2 - a new tag is to be inserted to the INFO part of an existing
 *      entry, but there is not even one byte available for the tag.
 *
 * RETURNS: return 0 which is the return from cxInsertEntry
 */
int
cxSplit(
        dsmContext_t    *pcontext,
        cxeap_t         *peap,     /* pointer to the Entry Access Parameters */
        findp_t         *pfndp,    /* parameter block for the FIND */
        downlist_t        *pdownlst,
        dsmObject_t      tableNum,
        int             tagonly)  /* if 1 - the split is caused by adding
                                       a tag to an existing index entry.
                                       if 0 - the split was caused by the
                                       need to insert a new index entry */
{
        dbcontext_t*  pdbcontext = pcontext->pdbcontext;
        cxbap_t BAP;              /* block access parameters */
        cxbap_t *pbap;            /* fast pointer to the BAP */
        TEXT    *pos;             /* current position in the block */
        TEXT    *nextpos;         /* next position in the block */
        TEXT    *psplit;          /* initial choice of the split point */
        TEXT    *plast;           /* last used position in the block */
        TEXT    *prevpos;         /* previous position */
        bmBufHandle_t leftHandle; /* buffer pool hand to left block */
        cxblock_t     *pleft;     /* to left block */
        cxblock_t     *pright;    /* to right block */
        int     keylng;           /* length of promoted key */
        DBKEY   newroot;          /* blknum of the new root block */
        findp_t fndp1;
        findp_t *pfndp1 = &fndp1;
        int     rightins = 0;
        int     newkeysize;
        int     rspace;
        int     i;
        int     leftents;       /* no. of entries in left block after split */
        int     ret;
/*        int     is; */
        int       is, ks, cs, hs;


        DBKEY    rightdbk;       /* blknum of the right block */
        DBKEY    leftdbk;        /* blknum of the left block */
                /* a temp key for new root */
        LONG    tmpkey[(sizeof(dsmKey_t) + 20)/sizeof(LONG)];

                /* key to be promoted  */
/*        LONG    promoted[(sizeof(dsmKey_t) + 256)/sizeof(LONG)]; */
        LONG    promoted[(sizeof(dsmKey_t) + MAXKEYSZ)/sizeof(LONG)];

        dsmKey_t        *ptmpKey = (dsmKey_t*)&tmpkey[0];
        dsmKey_t        *promotedKey = (dsmKey_t*)&promoted[0];

/*        TEXT    *poskey = &promotedKey->keystr[ENTHDRSZ]; */
        TEXT    *poskey = &promotedKey->keystr[FULLKEYHDRSZ];

        TEXT     tmp;
        TEXT     tmpdbk[sizeof(DBKEY) + 2];
        GBOOL    suffix_truncate;
#ifdef VARIABLE_DBKEY
        DBKEY   dbkey;
#endif  /* VARIABLE_DBKEY */

    INIT_S_FINDP_FILLERS( fndp1 ); /* bug 20000114-034 by dmeyer in v9.1b */

    pbap = &BAP;                        /* load address of the BAP */

    if ( pdbcontext->pidxstat &&
         (pdbcontext->pdbpub->isbaseshifting ==0 ) &&
         xct(&peap->pinpkey[2]) >= pdbcontext->pdbpub->argindexbase &&
         xct(&peap->pinpkey[2]) < pdbcontext->pdbpub->argindexbase +
                                  pdbcontext->pdbpub->argindexsize )
        ((pdbcontext->pidxstat)[ xct(&peap->pinpkey[2])  - 
                               pdbcontext->pdbpub->argindexbase ]).split++;

    /* the caller read the block to be split (the left block) */
    /* and marked it as modified */

    /* initialize some of the new block parameters */
    pleft = pfndp->pblkbuf;             /* to the left block, to be split */
    leftdbk = pfndp->blknum;
    leftHandle = pfndp->bufHandle;

#if CXDEBUG
    MSGD("%Lsplit - left dbk = %D",leftdbk);
#endif
    bufcop((TEXT *)promoted, peap->pkykey, sizeof(dsmKey_t));

    /* decide the split position */
    plast = &pleft->ixchars[pleft->IXLENENT - 1];
    if (pfndp->position > plast)                /* if insert at end */
    {
        /* if the insert position (pfndp->position) is after the last */
        /* entry in the block, split at the end to improve sequential */
        /* insertion of keys, such as in index rebuild with sort. */

        cxDoSplit(pcontext, tableNum, 
		  pfndp, pbap, pleft->IXNUMENT); /* do the split */

        /* determine which the key to promote: if splitting a leaf block, */
        /* perform trailing compression on the promoted key, otherwise    */
        /* promote the whole inserted key */
        if (pleft->IXBOT)
            keylng = pfndp->csofprev + 1;       /* length of promoted key */
        else
            keylng = pfndp->inkeysz;

        /* copy the key to promote into promotedKey */
        bufcop(poskey, pfndp->pinpkey, keylng);
        rightins = 1;                           /* insert in the right */
    }
    else                /* inserting BEFORE the last element in the block */
    {
        pos = pfndp->position;  /* the insertion position */
/*        if (tagonly && pos + pos[1] + pos[2] + ENTHDRSZ > plast) */
        if (tagonly && (pos + ENTRY_SIZE_COMPRESSED(pos) > plast))
            psplit = pos;       /* inserting a tag into the last element */
        else                    /* split about the middle of the block */
            psplit = &pleft->ixchars[0] + pleft->IXLENENT/2;

        /****************************************************************/
        /* the following loop does the following:                       */
        /*      locates the chosen split position                       */
        /*      counts how many elements to the left of the split point */
        /*      builds the promoted key.                                */
        /****************************************************************/

        leftents = 0;
        pos = &pleft->ixchars[0];
/*        nextpos = pos + ENTHDRSZ + pos[1] + pos[2]; */
        nextpos = pos + ENTRY_SIZE_COMPRESSED(pos);
        do
        {
            /* skip the current entry.  Count its tags and get its */
            /* compressed portion into promotedKey. */
            leftents += cxSkipEntry(pcontext, pfndp, pos, nextpos, poskey);

            prevpos = pos;
            pos = nextpos;
 /*           nextpos = pos + ENTHDRSZ + pos[1] + pos[2]; */
            nextpos = pos + ENTRY_SIZE_COMPRESSED(pos);
        } while (nextpos <= psplit);

        /****************************************************************/
        /* We are now at or before the desired split point.  If we are  */
        /* inserting into the left block, inserting at the split point  */
        /* or inserting only a 1 byte tag we are safe.                  */
        /* Otherwise, we are inserting into the right block, and we     */
        /* need to make sure it has enough space.  If not, we must move */
        /* the split point to the right until we either hit the         */
        /* insertion point, or there is enough room in the right block. */
        /****************************************************************/
        if (pfndp->position > pos && !tagonly)
        {
/*            newkeysize = pfndp->inkeysz + pfndp->indatasz + ENTHDRSZ; */
            newkeysize = pfndp->inkeysz + pfndp->indatasz;
            if (pfndp->inkeysz > MAXSMALLKEY)
                newkeysize += LRGENTHDRSZ;
            else
                newkeysize += ENTHDRSZ;

            /* calculate how much space will be left in the right block */
            rspace = MAXIXCHRS - pleft->IXLENENT;

            /* advance the split point to the right until there is enough   */
            /* space in the right block, or we reached the insertion point. */
            /* If the insertion is anywhere other than the split position,  */
            /* the max amount of space needed in the right block is the     */
            /* size of the new key + whatever we loose when we decompress   */
            /* the key at the split point.  */
            while ((pfndp->position > pos) &&
 /*               (int)(rspace + (pos - &pleft->ixchars[0]) - *pos) < newkeysize) */
                (int)(rspace + (pos - &pleft->ixchars[0]) - CS_GET(pos)) < newkeysize)
            {
/*                nextpos = pos + ENTHDRSZ + pos[1] + pos[2]; */
                nextpos = pos + ENTRY_SIZE_COMPRESSED(pos);
                leftents += cxSkipEntry(pcontext, pfndp, pos, nextpos, poskey);
                prevpos = pos;
                pos = nextpos;
            }

        }
        FASSERT_CB(pcontext, pos < plast, 
                   "cxSplit: shouldn't be splitting at the end");

        /********************************************************************/
        /* build the key to promote and determine the block to insert into. */
        /********************************************************************/

        /* truncate the suffix of the promoted key only if splitting */
        /* a leaf block, and the key on the left is not a null key   */
        /* and not a single zero byte. */
        suffix_truncate = 0;
        if (pleft->IXBOT)
        {
/*            if (prevpos[0] + prevpos[1] > 1 ||      prev key size > 1 */ 
            if ( (*prevpos == LRGKEYFORMAT)           /* Large key */
              || (prevpos[0] + prevpos[1] > 1)     /* prev key size > 1 */
              || (prevpos[1] == 1 && prevpos[ENTHDRSZ] != 0))
                suffix_truncate = 1;
        }

        /* if not inserting at the split point, or inserting only */
        /* a tag, the promoted key must differentiate the keys on */
        /* either side of the insertion position. */
        if (pfndp->position != pos || tagonly)
        {
/*            if (pfndp->position >= pos) */
/*                rightins = 1; */

/*            if (suffix_truncate) */
/*                i = 1; */
/*            else */
/*                i = *(pos + 1);                  the whole right key */ 
/*            bufcop(&poskey[*pos], &pos[ENTHDRSZ], i); */
/*            keylng = *pos + i; */
            CS_KS_IS_HS_GET(pos);
            if (pfndp->position >= pos)
                rightins = 1;

            if (suffix_truncate)
                i = 1;
            else
                i = ks;                 /* the whole right key */
            bufcop(&poskey[cs], &pos[hs], i);
            keylng = cs + i;
        }
        else            /* inserting a key (not a tag) at the split position */
        {
            /* insert the key into the smaller of the two blocks */

            if (pos <= &pleft->ixchars[0] + pleft->IXLENENT/2)
            {
                /* insert into the left block.  The promoted key must  */
                /* differentiate the new key from the key to the right */
                /* of the insertion position. */

/*                FASSERT_CB( pcontext */
/*                          , pfndp->csofnext >= *pos */
/*                          , "cxSplit: csofnext < pos" */
/*                         ); */

/*                if (suffix_truncate) */
/*                    keylng = pfndp->csofnext + 1; */
/*                else */
/*                    keylng = pos[0] + pos[1]; */
/*                bufcop(poskey + *pos, &pos[ENTHDRSZ], keylng - *pos); */
                CS_KS_IS_HS_GET(pos);

                FASSERT_CB(pcontext, pfndp->csofnext >= cs,
                           "cxSplit: csofnext < cs");

                if (suffix_truncate)
                    keylng = pfndp->csofnext + 1;
                else
                    keylng = cs + ks;
                bufcop(poskey + cs, &pos[hs], keylng - cs);
            }
            else
            {
                /* insert into the right block.  The promoted key must */
                /* differentiate the key to the left of the insertion  */
                /* position from the new key. */

                rightins = 1;

                if (suffix_truncate)
                    keylng = pfndp->csofprev + 1;
                else
                    keylng = pfndp->inkeysz;
                bufcop(poskey, pfndp->pinpkey, keylng);
            }
        }

        bufcop(pfndp1, pfndp, sizeof(findp_t));
        pfndp1->pinpkey = poskey;
        pfndp1->inkeysz = keylng;
        cxFindEntry(pcontext, pfndp1);
        FASSERT_CB(pcontext, pfndp1->position == pos,
                   "cxSplit: found wrong pos to split");
        cxDoSplit(pcontext, tableNum, 
		  pfndp1, pbap, leftents);    /* do the split */
    }

#if CXDEBUG
    MSGD("%Lsplit - right dbk = %D", pbap->blknum);
#endif

    /* generate the entry to be promoted: the promoted key + the 
    dbkey of the right block. */
/*    pos = &promotedKey->keystr[ENTHDRSZ + keylng]; */
    pos = &promotedKey->keystr[FULLKEYHDRSZ + keylng];
    rightdbk = pbap->blknum;    /* the blknum of the right block */

#ifdef VARIABLE_DBKEY
/*====Rich's change=============================== */
/*    is = gemDBKEYtoIdx( rightdbk,  2, pos ); */
    is = gemDBKEYtoIdx( rightdbk >> 8,  2, pos );
    *(pos + is) = (TEXT)rightdbk;
    is++;
/*------------------------------------------------ */
#else
    is = 4;    /* IS of non-leaf block is always 4 */
    slng(pos, rightdbk);        /* dbk of the right is promoted */
#endif  /* VARIABLE_DBKEY */

/*    promotedKey->keystr[0] = keylng + ENTHDRSZ + is; */
/*    promotedKey->keystr[1] = keylng; */
/*    promotedKey->keystr[2] = is;     IS of non-leaf block is always 4 */ 
    sct(&promotedKey->keystr[TS_OFFSET] , keylng + FULLKEYHDRSZ + is);
    sct(&promotedKey->keystr[KS_OFFSET] , keylng);
    sct(&promotedKey->keystr[IS_OFFSET] , is);    /* IS of non-leaf block is always 4 */

    pright = pbap->pblkbuf;     /* the right (new) block */
    bmReleaseBuffer(pcontext, pbap->bufHandle); /* release only the right (new) blk */

    if (pdownlst->currlev == 0) /* was the split block the root block? */
    {

#if CXDEBUG
    MSGD("%Lsplit - of root block");
#endif

        /* the root block must forever remain the root, to eliminate
        the need to lock and change the anchor of the root **************/

        /* save the blknum of the "left" as the new root block */
        newroot = pfndp->blknum;

        bufcop(pfndp1, pfndp, sizeof(findp_t));
        pfndp1->position = pleft->ixchars;/* a trick to split at the beginning*/
        pfndp1->csofprev = 0;
        pfndp1->csofnext = 0;
        /* copy left to a new block by simulating a split at the beginning */
        cxDoSplit(pcontext, tableNum, 
		  pfndp1, pbap, 0); /* get new block, to become new left */
        /***cxbkmod(pbap->pblkbuf); *** done by rl.. */

        /* shift the list down to allow a new root level */
        (&pdownlst->level[0])->dbk = pbap->blknum;/*new left to top of list */

#if CXDEBUG
    MSGD("%Lsplit - new left dbk = %D",pbap->blknum);
#endif

        for (i = MAXLEVELS - 1; i; i--)
        {
            (&pdownlst->level[i])->dbk = (&pdownlst->level[i - 1])->dbk;
        }
        (&pdownlst->level[0])->dbk = newroot;
        pdownlst->locklev++;    /* increase number of locked predecessors */

        /* create the 1st entry of the new root block */
        bufcop((TEXT *)tmpkey, peap->pkykey, sizeof(dsmKey_t));
        pos = &ptmpKey->keystr[0];

#ifdef VARIABLE_DBKEY
/*====Rich's change=============================== */
/*        is = gemDBKEYtoIdx( pbap->blknum,  2, pos + ENTHDRSZ); */
        is = gemDBKEYtoIdx( pbap->blknum >> 8,  2, pos + FULLKEYHDRSZ);
	*(pos + FULLKEYHDRSZ + is) = (TEXT)pbap->blknum;
	is++;
/*------------------------------------------------ */
#else
        is = 4;    /* IS of non-leaf block is always 4 */
        slng(pos + ENTHDRSZ, pbap->blknum);
#endif  /* VARIABLE_DBKEY */

        sct(&pos[TS_OFFSET],is + FULLKEYHDRSZ);  /* total length of the entry */
        sct(&pos[KS_OFFSET],0);             /* ks=0 */
	sct(&pos[IS_OFFSET],is);
        pfndp1->pkykey = ptmpKey;
        pfndp1->csofnext = 0;
        pfndp1->csofprev = 0;
        tmp = 0;
        bkReplace(pcontext, pfndp1->bufHandle, (TEXT *)&pfndp1->pblkbuf->IXBOT,
                                 &tmp, (COUNT)1);          /*IXBOT = 0 */
        cxInsertEntry(pcontext, pfndp1, tableNum, 0);/* must be after IXBOT=0, insert new entry */
        tmp = 1;
        bkReplace(pcontext, pfndp1->bufHandle, (TEXT *)&pfndp1->pblkbuf->IXTOP,
                                 &tmp, (COUNT)1);         /*IXTOP = 1 */



        /* needed for the release of the block by the caller */
        pfndp->blknum = leftdbk = pbap->blknum;
        pfndp->pblkbuf = pbap->pblkbuf;
        pfndp->bufHandle = pbap->bufHandle;

        /* swap left with new root */
        pbap->pblkbuf = pleft;
        pbap->blknum = newroot;
        pbap->bufHandle = leftHandle;
    }
    else                        /* there is a predecessor */
    {

        /* get the blknum of the predecessor block */
        pdownlst->currlev--;    /* up a level */
        pbap->blknum = (&pdownlst->level[pdownlst->currlev])->dbk;

        /* locate the buffer of the predecessor, it must have been
        already in a buffer and locked by the caller */
        /*get buff of the predeces. blk*/
        pbap->bufHandle = bmFindBuffer(pcontext, pdownlst->area,pbap->blknum);
        pbap->pblkbuf = (cxblock_t*)bmGetBlockPointer(pcontext,pbap->bufHandle);
    }

    /* set parameters of new pfndp1 for the insertion in the predecessor*/
    pfndp1->flags = pfndp->flags;
    pfndp1->blknum = pbap->blknum;
    pfndp1->bufHandle = pbap->bufHandle;
    pfndp1->pblkbuf = pbap->pblkbuf;
    pfndp1->inkeysz = keylng;
/*    pfndp1->pinpkey = &promotedKey->keystr[ENTHDRSZ]; */
    pfndp1->pinpkey = &promotedKey->keystr[FULLKEYHDRSZ];

/*#ifdef VARIABLE_DBKEY */
/*    pfndp1->indatasz = promotedKey->keystr[2];    promoted IS */ 
/*#else */
/*    pfndp1->indatasz = 4;                only a dbk */ 
/*#endif   VARIABLE_DBKEY */ 
    pfndp1->indatasz = xct(&promotedKey->keystr[IS_OFFSET]);   /* promoted IS */

    pfndp1->pinpdata = pfndp1->pinpkey + keylng;
    pfndp1->pkykey = promotedKey;
        /***cxbkmod(pbap->pblkbuf); *** done by rl.. */

    cxFindEntry(pcontext, pfndp1);              /* search for the insert position */
    if (cxCheckInsert(pcontext, pfndp1, tableNum, 0)) /* check if need to split */
        cxSplit(pcontext, peap, pfndp1, pdownlst, tableNum, 0); /* split the predecessor blk */
    else
        cxInsertEntry(pcontext, pfndp1, tableNum, 0);   /* insert the entry  */


    /*-------------------now the split is done --------------
     --------------------insert the new key which caused the split */

    pdownlst->currlev++;        /* down a level */
    if (!rightins)              /* insert in the left block */
    {

        /* get the left block back to memory */
        pbap->blknum = leftdbk; /* blknum of the left block */
        /*get the left block */
        pbap->bufHandle = bmFindBuffer(pcontext, pdownlst->area,pbap->blknum);
        pbap->pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,pbap->bufHandle);
    }
    else                        /* insert in the right block */
    {
        /* get the right block back to memory */
        pbap->blknum = rightdbk;        /* blknum of the right block */
                                        /* get the right block  */
        pbap->bufHandle = bmLocateBlock(pcontext, pdownlst->area,
                                        pbap->blknum, TOMODIFY);
        pbap->pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,pbap->bufHandle);
        /***cxbkmod(pbap->pblkbuf); *** done by rl.. */
    }
    pfndp1->bufHandle = pbap->bufHandle;
    pfndp1->pblkbuf = pbap->pblkbuf;
    pfndp1->blknum = pbap->blknum;
    pfndp1->inkeysz = pfndp->inkeysz;
    pfndp1->pinpkey = pfndp->pinpkey;
    pfndp1->indatasz = pfndp->indatasz;
    pfndp1->pinpdata = pfndp->pinpdata;
    pfndp1->pkykey = pfndp->pkykey;
    cxFindEntry(pcontext, pfndp1);              /* find insert position */
    ret = cxInsertEntry(pcontext, pfndp1, tableNum, tagonly); /* insert the entry */

    if (!(pfndp1->pblkbuf->IXBOT))
    {
        /* non-leaf level*/

        if (!rightins)
        {
            /* need to get the right block */
            pbap->blknum = rightdbk;    /* blknum of the right block */
                                        /* get the right block  */
            pbap->bufHandle = bmLocateBlock(pcontext, pdownlst->area,
                                            pbap->blknum, TOMODIFY);
            pbap->pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,
                                                       pbap->bufHandle);
            rightins = 1;               /* to cause release of right block */
        }
        /* at this point pbap has the right block */
        FASSERT_CB(pcontext, *(pbap->pblkbuf->ixchars+1) > (TEXT)1,
                   "Wrong key length at split");

        /* replace the 1st key with zero key, by deleting the 1st entry
        and inserting the zero entry, this may also expand the 2nd key */

        pos = &pbap->pblkbuf->ixchars[0];
#ifdef VARIABLE_DBKEY
        cxGetDBK(pcontext, pos, &dbkey); /* get the dbkey of 1st entry */
        /* The dbkey of the 1st entry must be of the max size for
        possible replacement in cxRemoveBlock */
/*====Rich's change=============================== */
 /*       is = gemDBKEYtoIdx( dbkey,  sizeof(DBKEY)+1, &tmpdbk[0] ); */
	/* Max size is two less than sizeof dbkey because we don't encode the low
	   order byte and no database will ever be so big as to need the high order byte
	   of a 64 bit dbkey.     */
        is = gemDBKEYtoIdx( dbkey >> 8, CX_MAX_X , &tmpdbk[0] );
 	    tmpdbk[is] = (TEXT)dbkey;
 	    is++;
/*------------------------------------------------ */
#else
        is = 4;
/*        bufcop(&tmpdbk[0], pos + ENTHDRSZ + pos[1], is);  save the dbk */ 
        bufcop(&tmpdbk[0], pos + HS_GET(pos) + KS_GET(pos), is); /* save the dbk */
#endif  /* VARIABLE_DBKEY */

        /* delete the 1st entry */
        pfndp1->bufHandle = pbap->bufHandle;
        pfndp1->pblkbuf = pbap->pblkbuf;
        pfndp1->blknum = pbap->blknum;
        pfndp1->position = pos;
        cxRemoveEntry(pcontext, tableNum, pfndp1); /* delete the entry from the block */

        /* create the 1st entry - the zero key entry */
        bufcop((TEXT *)tmpkey, peap->pkykey, sizeof(dsmKey_t));
        pos = &ptmpKey->keystr[0];
	sct(&pos[TS_OFFSET],FULLKEYHDRSZ + is + 1); /* total length of the string */
	sct(&pos[KS_OFFSET],1);     /* ks=1 */
	sct(&pos[IS_OFFSET],is);
	pos += FULLKEYHDRSZ;
        *pos++ = 0;             /* the ZERO KEY */
        bufcop(pos, &tmpdbk[0], is);             /* the saved dbkey */
        pfndp1->pkykey = ptmpKey;

        hs = HS_GET(pbap->pblkbuf->ixchars);
        if (pbap->pblkbuf->ixchars[hs + 1] == 0 && pbap->pblkbuf->IXLENENT)
/*        if (pbap->pblkbuf->ixchars[4] == 0 && pbap->pblkbuf->IXLENENT) */
            pfndp1->csofnext = 1;
        else
            pfndp1->csofnext = 0;
        pfndp1->csofprev = 0;
        cxInsertEntry(pcontext, pfndp1, tableNum, 0);  /* must be after IXBOT=0, insert the new entry */
    }

    if (rightins)
        bmReleaseBuffer(pcontext, pbap->bufHandle); /* only rgt blk is released */
    return ret;   
                      /* end of the split operation */
}       /* cxSplit */

/*************************************************************************/
/* PROGRAM: cxSkipEntry - returns the number of tags represented by the
 *  current entry, and copy the compressed part of the next entry into
 *  *poskey.
 *
 * RETURNS: returns the number of tags represented by the current entry
 */
int
cxSkipEntry(
        dsmContext_t    *pcontext _UNUSED_,
        findp_t         *pfndp,
        TEXT            *pos,
        TEXT            *nextpos,
        TEXT            *poskey)
{
/*    int         ntags; */
    int         ntags, cs, ks, is, hs;
    COUNT       nextCS;


    ntags = 1;          /* normally, a B-tree element represents one entry */
    CS_KS_IS_HS_GET(pos);
    if(pfndp->pblkbuf->IXBOT && (pfndp->flags & CXTAG))
    {
        /* a leaf element may represent multiple entries */
/*        if (pos[2] == 33)       a bit map: count is the 1'st info byte */ 
        if (is == 33)       /* a bit map: count is the 1'st info byte */
        {
/*            ntags = pos[ENTHDRSZ + pos[1]]; */
            ntags = pos[hs + ks];
            if (ntags == 0)     /* zero bit count represents 256 */
                ntags = 256;
        }
        else                    /* the info size is the count of entries */
        {
/*            ntags = pos[2]; */
            ntags = is;
            if (ntags == 0)     /* the null entry contains no information */
                ntags = 1;      /* but counts as an entry */
        }
    }

/*    if (*nextpos > *pos)         get next key's cmpressed part */ 
/*        bufcop(poskey + *pos, &pos[ENTHDRSZ], (int)(*nextpos - *pos)); */
    if ((nextCS = CS_GET(nextpos)) > cs) /* get next key's cmpressed part */
        bufcop(poskey + cs, &pos[hs], (int)(nextCS - cs));

    return ntags;

}       /* cxSkipEntry */

/*************************************************************************/
/* PROGRAM: cxFindEntry - find an entry in a given block
 *
 * RETURNS:     0 the search key was found
 *              1 the search key was not found
 *              pfndp->position: points to the found entry
 *                      or the place it is to be inserted
 *              pfndp->prv: points to the previous entry if exists, else = NULL
 *              pfndp->prvprv: points to the previous of the
 *                              previous entry if exists, else = NULL
 */
int
cxFindEntry(
        dsmContext_t    *pcontext _UNUSED_,
        findp_t         *pfndp)         /* pointer to the parameter block */
{
        TEXT    *pos;           /* addr of the position in the searched blk */
        TEXT    *keypos;        /* addr of the position in the search key*/
        TEXT    *bkkeyend;      /* end of current key in the block */
        TEXT    *keyend;        /* end of the search key */
        TEXT    *pfreepos;      /* 1'st free pos past end of text in the blk */
        int     cs;             /* size of the compressed part */
        int     ks;             /* size of the key */
        int     is;             /* size of data (info size) */
        int     keycs;          /* CS of search key relative to current key*/
        int     hs;             /* size of entry-header */

#if 0  /* UCOUNT is never < 0 */
    if(pfndp->inkeysz <= 0)
        FATAL_MSGN_CALLBACK(pcontext, cxFTL001,pfndp->inkeysz);
#endif

    /* initialization of the search */
    pos = pfndp->pblkbuf->ixchars;      /* start of 1st entry */
    pfndp->position = pos;
    pfreepos = pos + pfndp->pblkbuf->IXLENENT;
    keypos = pfndp->pinpkey;            /* point to start of search key */
    keyend = keypos + pfndp->inkeysz;
    keycs = 0;
    pfndp->csofnext = 0; pfndp->csofprev = 0;
    pfndp->pprv = pfndp->pprvprv = 0;   /* init pointers to prev. entrys */

    /* pos points to the CS of the current entry */
    while ((pos < pfreepos))            /* while there is an entry in pos */
    {

        /* get cs, ks, is */
/*        cs = *pos++; */
/*        ks = *pos++; */
/*        is = *pos++;                     point to the key */ 
        CS_KS_IS_HS_GET(pos);
        pos += hs;                /* point to the key */

        if (keycs == cs)
        {
            if (ks == 0)                /* only for dummy entry in 1'st blk */
                goto next;

            bkkeyend = pos + ks;
            while (*keypos == *pos)     /* the two bytes are equal */
            {
                keycs++;        /* increment the CS of the search key */
                keypos++;       /* step to next search key position */
                pos++;          /* step to next position of the key in blk */

                if (keypos == keyend)           /* end of search key */
                {
                    if (pos != bkkeyend)        /* search key is shorter */
                        goto not_found;
                    else                        /* keys are equal */
                    {
                        pos += is;              /* point to next entry */
                        if (pos >= pfreepos)
                            pfndp->csofnext = 0;
                        else
/*                            pfndp->csofnext = *pos; */
                            pfndp->csofnext = CS_GET(pos);
                        return 0;               /* keys are equal */
                    }   /* end of keys are equal */
                }       /* end of keypos == keyend */
                else if (pos == bkkeyend)       /* blk key ended, srch key */
                    goto next;                  /* didn't: blk key is longer */
            }   /* end while */

            if (*keypos < *pos)         /* search key is lower */
                goto not_found;

            pos = bkkeyend;
            goto next;                          /* search key is higher */
        }       /* end cs == keycs */

        if (keycs > cs)
        {
            keycs = cs;         /* set CS of search key to that of the cur */
            goto not_found;                     /* search key is lower */
        }

        /* search key is higher, step to next entry */
        pos = pos + ks;                 /* point to data */

    next:                               /* pos is on data, skip to next entry */
        pos = pos + is;                 /* point to CS of next entry if exist */
        pfndp->pprvprv = pfndp->pprv;   /* pointer of prev. of prev. entry */
        pfndp->pprv = pfndp->position;  /* pointer to prev. entry */
        pfndp->csofprev = keycs;        /* CS of cur key relative to prev */
        pfndp->position = pos;          /* address of the current entry */
    }                                   /* while there are more */
    /* end of entries, no entries in the block */

not_found:
    /* no more entrys, or the search key is smaller, end of search */
    pfndp->csofnext = keycs;    /* CS of the cur key relative to next key */
    return 1;                                   /* failed */

}       /* cxFindEntry */

/*************************************************************************/
/* PROGRAM: cxInsertEntry - insert a given entry at the current pos in a block
 * both the inserted key and the next key may have to be compressed
 * 
 * RETURNS: 0
 */
int
cxInsertEntry(
        dsmContext_t    *pcontext,
        findp_t         *pfndp,   /* to parameter block of the find */
	dsmObject_t      tableNum,
        int             tagonly)  /* 1 if insert tag only to existing entry */

{
        cxblock_t  *pixblk = pfndp->pblkbuf;
        TEXT       *pos;           /* address of current pos */
        TEXT       *pentry = 0;    /* address inserted entry key */
        CXNOTE     note;
        TEXT       *pfreepos;/*1st free position past end of the text in the blk*/

#ifdef VARIABLE_DBKEY
        int         dbkeySize;
        TEXT       *pend;
#endif  /* VARIABLE_DBKEY */

    pos = pfndp->position;
    pfreepos = pixblk->ixchars + pixblk->IXLENENT;
    pentry = &pfndp->pkykey->keystr[0];

    /* initialize the note */
    note.rlnote.rllen = sizeof(note);
    note.rlnote.rlcode = RL_CXINS;
    note.tableNum = tableNum;
    note.offset = pos - (TEXT*)&pixblk->ixchars[0];
    if(pixblk->IXBOT && tagonly)
/*        note.cs = 0xff;  mark a TAG operation */ 
        note.tagOperation = INDEX_TAG_OPERATION; /* mark a TAG operation */
    else
    {
/*        note.cs = (TEXT)pfndp->csofprev;         CS of inserted key */ 
        note.cs = pfndp->csofprev;        /* CS of inserted key */
        note.tagOperation = 0; 
    }
#ifdef VARIABLE_DBKEY
/*====Rich's change=============================== */
/*    pend = pentry + ENTHDRSZ + pentry[1]; */
    pend = pentry  + xct(&pentry[TS_OFFSET]) - 1;
    dbkeySize = *( pend - 1 ) & 0x0f;
/*    gemDBKEYfromIdx( pend - dbkeySize, &dbkey );  part of dbkey from the key */ 
/*    dbkey <<= 8; */
/*    dbkey += *pend;   the last byte of the dbkey */ 
/*    sdbkey( &note.sdbk[0], dbkey );  dbkey to the note (big indian) as in the past */ 
    bufcop(&note.sdbk[sizeof(note.sdbk) - (dbkeySize + 1)], 
 	    pend - dbkeySize, dbkeySize + 1);    
/*------------------------------------------------ */
#else
/*    bufcop(&note.sdbk[0], pentry + *pentry - 4, 4); */
    bufcop(&note.sdbk[0], pentry + xct(&pentry[TS_OFFSET]) - 4, 4);
#endif  /* VARIABLE_DBKEY */

    sct((TEXT *)&note.rlnote.rlflgtrn,
         (COUNT)((pixblk->IXBOT && !(pfndp->flags & CXNOLOG)) ?
                        RL_PHY|RL_LOGBI: RL_PHY));

    note.indexRoot = pfndp->pkykey->root;
    if( pfndp->pkykey->unique_index )
        note.indexAttributes = DSMINDEX_UNIQUE;
    else if (pfndp->pkykey->word_index )
        note.indexAttributes = DSMINDEX_WORD;
    else
        note.indexAttributes = 0;

    if (pos != pfreepos) /*insert pos. not at end, next exist*/
    {
        /* calculate the extra compression of the next entry if needed */
/*        note.extracs = pfndp->csofnext - *pos;  Nxt CS - Curr CS */ 
        note.extracs = pfndp->csofnext - CS_GET(pos); /* Nxt CS - Curr CS */
    }
    else
        note.extracs = 0;

/*    rlLogAndDo (pcontext, (RLNOTE *)&note, pfndp->bufHandle, (COUNT)*pentry, pentry); */
    rlLogAndDo ( pcontext, (RLNOTE *)&note, pfndp->bufHandle, xct(&pentry[TS_OFFSET]),
                 pentry);
    return 0;

}       /* cxInsertEntry */

#if 0
/***************************************************************/
/* PROGRAM: cxReplaceInfo - replace the info of the current position
 *
 * RETURNS: 0
 */
LOCALF int
cxReplaceInfo(
        dsmContext_t    *pcontext,
        findp_t         *pfndp)    /* pointer to the FIND P. B. */
{
        TEXT    *pos;  /* current position */
        TEXT    *pdata;  /* address of the info */
        int      diff;
 /*       int      is, ks; */
        int      is, ks, cs, hs;

        pos = pfndp->position;

        /* calculate ks, is */
/*        ks = *(pos + 1); */
/*        is = *(pos + 2); */
/*        pdata =  pos + ENTHDRSZ + ks;  to data */ 
        CS_KS_IS_HS_GET(pos);
        pdata =  pos + hs + ks; /* to data */

        pos =  pdata + is; /* to next entry */
        diff = pfndp->indatasz - is; /* diff between new and old info size */

        /* adjust to the new size of data */
        bufcop(pos + diff, pos, pfndp->pfreepos - pos);

        /* replace old with new info */
        bufcop(pdata, pfndp->pinpdata, (int)pfndp->indatasz);

        /* update the used space */
        pfndp->pblkbuf->IXLENENT += diff;
        pfndp->pfreepos += diff;

}       /* cxReplaceInfo */
#endif

/*************************************************************************/
/* PROGRAM: cxRemoveEntry - delete a given entry from the current pos in the block
 *
 * RETURNS: 0
 */
int
cxRemoveEntry(
        dsmContext_t    *pcontext,
        dsmObject_t      tableNum,
       findp_t         *pfndp)    /* pointer to the FIND P. B. */
{
        cxblock_t   *pixblk = pfndp->pblkbuf;
        TEXT    *pos;  /* address of the position in the searched blk */
        int      is, ks, cs, nxtcs;
        TEXT    *pfreepos;/*1st free position past end of the text in the blk*/
        CXNOTE   note;
/*        TEXT     oldentry[ENTHDRSZ + 255]; */
        TEXT     oldentry[LRGENTHDRSZ + MAXKEYSZ + 9];
        TEXT    *poldentry = &oldentry[0];
        int      hs;
#ifdef VARIABLE_DBKEY
        int         dbkeySize;
/*        DBKEY       dbkey; */
#endif  /* VARIABLE_DBKEY */

    pos = pfndp->position;
    pfreepos = pixblk->ixchars + pixblk->IXLENENT;

    /* initialize the note */
    note.rlnote.rllen = sizeof(note);
    note.rlnote.rlcode = RL_CXREM;
    note.offset = pos - &pixblk->ixchars[0];
    note.tableNum = tableNum;
    note.indexRoot = pfndp->pkykey->root;
    if( pfndp->pkykey->unique_index)
        note.indexAttributes = DSMINDEX_UNIQUE;
    else if ( pfndp->pkykey->word_index )
        note.indexAttributes = DSMINDEX_WORD;
    else
        note.indexAttributes = 0;

/*    ks = *(pos + 1);  size of the compressed key to be deleted */ 
/*    is = *(pos + 2); */
/*    cs = *pos;   CS of deleted key */ 
    CS_KS_IS_HS_GET(pos);

    if(pixblk->IXBOT && (pfndp->flags & CXTAG) && is > 1)
/*        note.cs = 0xff;  mark a TAG operation */ 
        note.tagOperation = INDEX_TAG_OPERATION; /* mark a TAG operation */
    else
    {
        note.cs = cs;   /* CS of deleted key */
       /****TEMPORARY, NEED to CHANGE: if IDX LOCK only RL_PHY see ixremnt ***/
        note.tagOperation = 0; 
    }


    /* check if next entry is to be decompressed */
/*    pos += ENTHDRSZ + ks + is; */
    pos += hs + ks + is;

    note.extracs = 0;

    if (pos != pfreepos)        /* pos. not at end, next exist*/
    {
        /* get cs of the next entry */
/*        nxtcs = *pos; */
        nxtcs = CS_GET(pos);

        if (nxtcs > cs)     /* if next key to be expanded */
            note.extracs = nxtcs - cs;
    }
    sct((TEXT *)&note.rlnote.rlflgtrn,
        (COUNT)((pfndp->pblkbuf->IXBOT && !(pfndp->flags & CXNOLOG)) ?
                            RL_PHY|RL_LOGBI: RL_PHY));

    /* build the complete deleted entry for the logical note */
/*    oldentry[0] = is + ks + cs + ENTHDRSZ; */
/*    oldentry[1] = cs + ks; */
/*    oldentry[2] = is; */
/*    bufcop(&oldentry[ENTHDRSZ], pfndp->pinpkey, cs); */
/*    bufcop(&oldentry[ENTHDRSZ] + cs, &pfndp->position[ENTHDRSZ], ks + is); */
    sct(&oldentry[TS_OFFSET] , is + ks + cs + FULLKEYHDRSZ);
    sct(&oldentry[KS_OFFSET] , cs + ks);
    sct(&oldentry[IS_OFFSET] , is);
    bufcop(&poldentry[FULLKEYHDRSZ], pfndp->pinpkey, cs);
    bufcop(&poldentry[FULLKEYHDRSZ] + cs, &pfndp->position[hs], ks + is);
    pos = &oldentry[FULLKEYHDRSZ + cs + ks]; /* past the last byte of the key */
    if (pixblk->IXBOT)          /* bug no. 93-08-19-035 */
    {
/*        pos = &oldentry[ENTHDRSZ + cs + ks];past the last byte of the key */ 
#ifdef VARIABLE_DBKEY
        /* reconstruct the dbkey from its variable part and the last byte */
        dbkeySize = *(pos - 1) & 0x0f;
	/*        gemDBKEYfromIdx( pos - dbkeySize, &dbkey );*/  /* part of dbkey from the key */
/*====Rich's change=============================== */
        if(pfndp->flags & CXTAG && is > 1)
           note.sdbk[sizeof(note.sdbk) -1 ] = 
	     *pfndp->pinpdata;  /* the last byte of the dbkey */
        else
           note.sdbk[sizeof(note.sdbk) -1 ]  = 
	     *pos; /* the last byte of the dbkey */

    }
    else

/*        gemDBKEYfromIdx( pos, &dbkey );    dbkey from the is */ 

/*    sdbkey( &note.sdbk[0], dbkey );  dbkey to the note (big indian) as in the past */ 
       bufcop(&note.sdbk[sizeof(note.sdbk) - is],
 	     &oldentry[FULLKEYHDRSZ]+cs+ks,is);
/*------------------------------------------------ */
#else
/*        bufcop(&note.sdbk[0], pos - 3, 4);  old dbkey from the IS part */ 
        bufcop(&note.sdbk[0], &poldentry[FULLKEYHDRSZ + cs + ks - 3], 4);/*old dbk */
        if(pfndp->flags & CXTAG && is > 1)
            note.sdbk[3] = *pfndp->pinpdata;
    }
    else
/*        bufcop(&note.sdbk[0], &oldentry[ENTHDRSZ + cs + ks], 4);  old dbkey */ 
        bufcop(&note.sdbk[0], &poldentry[FULLKEYHDRSZ + cs + ks], 4); /* old dbkey */
#endif  /* VARIABLE_DBKEY */

    rlLogAndDo (pcontext, (RLNOTE *)&note, pfndp->bufHandle,
/*             (COUNT)oldentry[0], &oldentry[0]); */
             xct(&poldentry[TS_OFFSET]), &poldentry[0]);

    return(0);

}       /* cxRemoveEntry */

/******************************************************************/
/* PROGRAM: cxUpgradeLock - upgrade to EXCLUSIVE predecessors and current
 *
 * RETURNS: 0
 */
LOCALF int
cxUpgradeLock(
        dsmContext_t    *pcontext,
        downlist_t        *pdownlst)      /*to list down */
{
    inlevel_t  *plevel;
    inlevel_t  *pcurrl;

        
    plevel = &pdownlst->level[pdownlst->currlev]; /* to current level */
    pcurrl = &pdownlst->level[pdownlst->currlev]; /* to current level */
    if(pdownlst->locklev > 0)
    {
        /* Upgrade our TXE lock                 */
        /* We may already have it in update mode because of removing
           blocks from indexes as part of servicing the index delete
           chain.  See cxRemoveFromDeleteChain in cxput.     */
        if(pcontext->pusrctl->uc_txelk != RL_TXE_UPDATE)
            rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);
    }
    
    for (plevel -= pdownlst->locklev; pcurrl != plevel; plevel++)
    {
        /* upgrade predecessor levels */
        bmUpgrade(pcontext, bmFindBuffer(pcontext, pdownlst->area,plevel->dbk));
    }
    /* upgrade current level */
    bmUpgrade(pcontext, bmFindBuffer(pcontext, pdownlst->area,plevel->dbk));
    return(0);

}       /* cxUpgradeLock */

#if 0
/********************************************************************/
/* PROGRAM: cxIsInList - check if given dbk is locked in the down list
 *
 * RETURNS: 1 if found
 *          else 0
 */
LOCALF int
cxIsInList(
        dsmContext_t    *pcontext, 
        downlist_t        *pdownlst,      /*to list down */
        DBKEY             dbk)
{
        inlevel_t  *plevel;
        inlevel_t  *pcurrl; /* current level */

    plevel =
    pcurrl = &pdownlst->level[pdownlst->currlev]; /* to current level */
    for (plevel -= pdownlst->locklev; pcurrl != plevel; plevel++)
    {
        if (plevel->dbk == dbk)
            return 1;
    }
    if (plevel->dbk == dbk)
        return 1;
    return 0;

}       /* cxIsInList */
#endif

/*************************************************************************/
/* PROGRAM: cxReleaseAbove - release levels up, above the current (predecessors)
 *
 * RETURNS: 0
 */
int
cxReleaseAbove(
        dsmContext_t    *pcontext,
        downlist_t        *pdownlst)      /* to list down */
{
        inlevel_t  *plevel;
        inlevel_t  *pcurrl; /* current level */

    plevel = &pdownlst->level[pdownlst->currlev]; /* to current level */
    pcurrl = &pdownlst->level[pdownlst->currlev]; /* to current level */
    for (plevel -= pdownlst->locklev; pcurrl != plevel; plevel++)
    {
        bmReleaseBuffer(pcontext,
                        bmFindBuffer(pcontext, pdownlst->area, plevel->dbk));
        pdownlst->locklev--;
    }
    return(0);

}       /* cxReleaseAbove */

/*******************************************************************/
/* PROGRAM: cxReleaseBelow - release levels down, below the current (sons)
 *
 * RETURNS: 0
 */
int
cxReleaseBelow(
        dsmContext_t    *pcontext,
        downlist_t        *pdownlst)      /* to list down */
{
        inlevel_t  *plevel;
        inlevel_t  *plast;

    plevel = &pdownlst->level[pdownlst->currlev];/*to current level */
    plast = plevel + pdownlst->locklev;/*to last level */
    for (++plevel; plast != plevel; plevel++)
    {
        bmReleaseBuffer(pcontext,
                        bmFindBuffer(pcontext, pdownlst->area, plevel->dbk));
        pdownlst->locklev--;
    }
    return(0);

}       /* cxReleaseBelow */

/********************************************************************/
/* PROGRAM:  cxPositionToNext - find next  or prev entry in the cxtree
 *
 * RETURNS: its position, or returns 0 when fails.
 *
 *      if successful, returns the position in the block
 *      and leaves the block busy, the caller must release the block
 *      when done with it
 */
LOCALF TEXT *
cxPositionToNext(
        dsmContext_t    *pcontext,
        cxeap_t         *peap,  /* pointer to the input EAP */
        findp_t         *pfndp) /* pointer to the FIND P. B. */
{
        TEXT          *pos;
        bmBufHandle_t pixBufHandle;
        cxblock_t     *pixblk;
        cxcursor_t    *pcursor;
        DBKEY         root = 0;    /* blknum of block to start with going down */
        downlist_t      dlist;                        /* temporary list down */
        downlist_t      *pdownlst= &dlist;     /* to temporary list down */
        inlevel_t       *plevel;
        TEXT          *pfreepos;/*1st free position past end of the text in the blk*/
        cxbap_t        bap;          /* block access parameters */
/*        int      ks, is, tag; */
		int		 tag;
        int      notfound = 0;
        int      mode;
        int      ixnum;

    pcursor = peap->pcursor;
top:
    CXINITLV(pdownlst);                 /* initialize the list down level */
    plevel = &pdownlst->level[0];

    if (pcursor && pcursor->blknum)
    {
        plevel->dbk = pcursor->blknum; /* start with cursor block */
    }
    else
    {
        ixnum = xct(&peap->pinpkey[2]);
        pcursor = 0; /* do not use cursor */
        root = plevel->dbk = peap->pkykey->root; /* start with root block */
    }
    pdownlst->area = peap->pkykey->area;

traverse:
    peap->state = CXSucc;       /* assume success - next tag has same key */
    peap->outkeysz = 0;         /* initialize the returned key length */

    if (pcursor)
    {
        bap.blknum = plevel->dbk;       /* blknum of starting level block */
        /* get current blk */
        bap.bufHandle = bmLocateBlock(pcontext, pdownlst->area,
                                      bap.blknum, TOREAD);
        bap.pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,bap.bufHandle);
        pfndp->bufHandle = bap.bufHandle;
        pixblk = pfndp->pblkbuf = bap.pblkbuf;
        pixBufHandle = bap.bufHandle;
        if (peap->flags & (CXNEXT | CXFIRST))   /* get next */
        {
            /* Move to the next entry in the block */
            pfndp->position = pixblk->ixchars + pcursor->offset;
            notfound = 0;
            if (peap->flags & CXSUBSTR)
            {
                pos = pfndp->position;
/*                pos = pos + *(pos +1) + *(pos +2) + ENTHDRSZ; */
/*                pfndp->csofnext = *pos; */
                pos += ENTRY_SIZE_COMPRESSED(pos);
                pfndp->csofnext = CS_GET(pos);
            }
        }
        else                                    /* get prev */
        {
            /* BUM - Assuming LONG bkhdr->bk_updctr > 0 */
            if ((ULONG)pixblk->BKUPDCTR == pcursor->updctr)
            {
                pfndp->pinpkey = peap->pinpkey;
                pfndp->inkeysz = peap->inkeysz;
                pfndp->flags = (UCOUNT)peap->flags; /* BUM - CAST */
                pfndp->blknum = bap.blknum;     /* blknum of the block */
                notfound = cxFindEntry(pcontext, pfndp);        /* srch for the key in block */
                peap->blknum = bap.blknum;
            }
        }

        /* BUM - Assuming LONG bkhdr->bk_updctr > 0 */
        if ((ULONG)pixblk->BKUPDCTR != pcursor->updctr /* same updat counter? */
        || pixblk->IXNUM != pcursor->ixnum      /* same ixnum? */
        || (int)(pixblk->BKTYPE) != IDXBLK)     /* is it an index block? */
        {
            /* the block pointed by the cursor was changed, search from root*/
            pcursor = 0;
            bmReleaseBuffer(pcontext, pixBufHandle); /* release the block buffer */
            goto top;
        }
    }
    else
    {
        notfound = cxSearchTree(pcontext, peap, pfndp, pdownlst); /* traverse down the b-tree */
        pixblk = pfndp->pblkbuf;
        pixBufHandle = pfndp->bufHandle;
        root = peap->pkykey->root;
    }

    pos = pfndp->position; /* point to the found entry */


    if (peap->flags & (CXNEXT | CXFIRST))       /* Get next */
    {
        if (!notfound)                  /* exact key was found */
        {
            if (peap->flags & CXTAG) /* if next tag */
            {
                /* find next tag in same entry*/

               tag = cxFindTag(pos, *peap->pdata, (int)peap->flags);

                if (tag != -1)
                {
                    *peap->pdata = tag; /* return the found tag */
                    cxReleaseAbove(pcontext, pdownlst);/* it is safe, release all predecessors */
                    return pos;
                }
                peap->state = CXFail;   /* tag key was not found */
            }

            /* skip to next entry */

            /* get ks, is */
/*            pos++; */
/*            ks = *pos++; */
/*            is = *pos++; */
/*            pos += ks + is;              point to next entry */ 
            pos += ENTRY_SIZE_COMPRESSED(pos); /* to next entry */
        }
        else
            peap->state = CXFail;       /* key was not found */
    }
    else
    {
        if (!notfound)                  /* exact key was found */
        {
            if (peap->flags & CXTAG) /* if prev tag */
            {
                /* get the next tag in the entry */
                tag = cxFindTag(pos, *peap->pdata, (int)peap->flags);
                if (tag != -1)
                {
                    *peap->pdata = tag; /* return the found tag */
                    cxReleaseAbove(pcontext, pdownlst);/*release all predecessors */
                    return pos;
                }
                peap->state = CXFail;   /* tag was was not found */
            }
        }
        else
            peap->state = CXFail;       /* key was not found */

        pos = pfndp->pprv; /* to previous entry */
    }

    pfreepos = pixblk->ixchars + pixblk->IXLENENT;
    if (((peap->flags & (CXNEXT | CXFIRST))     /* Get next */
          && (pos == pfreepos)) /* no next entry in the block  */
    ||   ((peap->flags & (CXPREV | CXLAST))  /* Get previous */
          && (pos == 0)))    /* no previous entry in the block */
    {
        /* next or prev not in this block */
        bmReleaseBuffer(pcontext, pixBufHandle); /* release the block buffer */

        if (pcursor)
        {
            /* the block pointed by the cursor does not have next or prev
               we must start from the root */
            pcursor = 0;
            goto top;
        }
        if ((&pdownlst->level[pdownlst->currlev])->dbk != root)
        {
            /* must start again from some predecessor */

            /* initialize the list down level */
            pdownlst->currlev -= pdownlst->locklev;/*top most locked lev*/
            plevel = &pdownlst->level[pdownlst->currlev];
            cxReleaseBelow(pcontext, pdownlst); /* released levels below top most */

            /* start with new top block, a level below the
            top most locked */
            plevel++; /* next level down */
            pdownlst->currlev++; /* next level down */
            plevel->dbk = plevel->nxtprvbk; /* the next bk is the new top */
            if (plevel->dbk != -1)
                goto traverse;
            cxReleaseAbove(pcontext, pdownlst);/*release all predecessors */
            return 0;
        }
        else
        {
            /* the root was reached and no next/prev exist,
            there is no next/prev entry in the entire B-tree */
            return 0;
        }
    }
    else
        cxReleaseAbove(pcontext, pdownlst);/* it is safe, release all predecessors */

    if (peap->flags & CXTAG) /* if next/prev tag */
    {
        if (peap->flags & (CXNEXT | CXFIRST))  /* Get next */
            mode = CXFIRST;
        else
            mode = CXLAST;
        tag = cxFindTag(pos, '\0', mode);/* next tag in same entry*/
        *peap->pdata = tag; /* return the found tag */
    }
    return pos;

}       /* cxPositionToNext */

/*****************************************************************/
/* PROGRAM: cxGetInfo - returns the data part of the index entry
 *
 * RETURNS: 0
 */
LOCALF int
cxGetInfo(
        dsmContext_t    *pcontext _UNUSED_,
        cxeap_t         *peap,
        TEXT            *from,
        int             is)
{
        /* do not exceed the PB.datamax  */
        if (is > (int)peap->datamax)
            is = peap->datamax;
        peap->datasz = is;             /* return the data length */

        bufcop(peap->pdata, from, is);  /* move the data */

        return (0);


}       /* cxGetInfo */

/****************************************************************/
/* PROGRAM:  cxGetKey - returns the key part of the index entry
 *
 * RETURNS:
 */
LOCALF int
cxGetKey(
        dsmContext_t    *pcontext _UNUSED_,
        cxeap_t         *peap,
        TEXT            *from,
        int              cs,
        int              ks)
{
    /* the expanded compressed part may already be in OutKey */
    /* do not exceed the Outdatamax */

    if ((ks + cs) > (int)peap->mxoutkey)
        ks = peap->mxoutkey - cs;

    /* verify that there is more to be returned */
    if (ks > 0)
    {
        bufcop(peap->poutkey + peap->outkeysz, from, ks); /* move key */
        peap->outkeysz += ks;     /* add the non compressed length */
    }
    return (0);

}       /* cxGetKey */

/**********************************************************************/
/* PROGRAM:  cxRemoveBlock - Remove empty blocks from the B-tree.
 *       Recursively called function to split a block and update
 *       the level above, possibly to remove it as well if it is
 *       empty, and so on and so on.
 *       This procedure expects all the "unsafe" blocks in the path
 *       to be already locked EXCLUSIVE and in the memory buffers,
 *       done by the caller that traversed down the B-tree and checked
 *       for "UNSAFE" blocks. This procedure does not release these
 *       blocks either, it expect the caller to do so after the return
 *       from this procedure.
 *
 * RETURNS: 0
 */
int
cxRemoveBlock(
        dsmContext_t    *pcontext,
        cxeap_t         *peap,      /* ptr to input Entry Access Param Blk */
        findp_t         *pfndp,     /* parameter block for the CFIND */
	dsmObject_t      tableNum,
        downlist_t      *pdownlst) /* to list down */
{
        cxbap_t    BAP;   /* block access parameters */
        findp_t    FPB1;  /* parameter block for CFIND */

        cxbap_t        *pbap;   /* fast pointer to the BAP */
        findp_t        *pfndp1; /* pointer to the FPB1 */
        bmBufHandle_t  father;/* to buffer of the father */
        cxblock_t      *pfather;
        bmBufHandle_t  son = 0; /* to buffer of the son */
        cxblock_t      *pson = NULL;
        int            count = 0;
        int            i;
        DBKEY          dbk;
        int            ret;
        TEXT           *pos;
	int            is;

#ifdef VARIABLE_DBKEY
	int	 ks;
        DBKEY    dbkey;
        TEXT     tmpdbk[sizeof(DBKEY) + 2];
	/* key may be only 1 byte + one extra */
#endif  /* VARIABLE_DBKEY */

	INIT_S_FINDP_FILLERS( FPB1 ); /* bug 20000114-034 by dmeyer in v9.1b */

        pfndp1 = &FPB1;  /* load address of the FPB1 */
        pbap = &BAP;     /* load address of the BAP */

    /* sanity check */
    if (pdownlst->currlev == 0)
        FATAL_MSGN_CALLBACK(pcontext, cxFTL003);

    /* free the empty block */
    dbk=
    pbap->blknum = (&pdownlst->level[pdownlst->currlev])->dbk;
#if CXDEBUG
    MSGD("%Lempty - downlist dbk = %D fndp dbk %D",dbk, pfndp->blknum);
#endif
    /*get the buff with predeces. blk */
    pbap->bufHandle = bmFindBuffer(pcontext, pdownlst->area, pbap->blknum);
    pbap->pblkbuf = (cxblock_t*)bmGetBlockPointer(pcontext,pbap->bufHandle);
    cxFreeBlock(pcontext, pbap);        /* free the empty block  */

    /* get the predecessor */
    pdownlst->currlev--;        /* up a level */
    pbap->blknum = (&pdownlst->level[pdownlst->currlev])->dbk;
    father =
    /*get the buff with predeces. blk */
    pbap->bufHandle = bmFindBuffer(pcontext, pdownlst->area, pbap->blknum);
    pbap->pblkbuf = (cxblock_t*)bmGetBlockPointer(pcontext,pbap->bufHandle);
    /* set parameters of the new FPB1 for the deletion in the predecessor blk*/
    pfndp1->blknum = pbap->blknum;
    pfndp1->bufHandle = father;
    pfndp1->pblkbuf = pbap->pblkbuf;
    pfndp1->inkeysz = pfndp->inkeysz;
    pfndp1->pinpkey = pfndp->pinpkey;
    pfndp1->pkykey  = pfndp->pkykey;
    pfndp1->flags = 0;

    ret = cxFindEntry(pcontext, pfndp1); /* search for the entry in the block */
    if ((ret) && (pfndp1->pprv != 0))
    {
            /* if exact key not found and there is a previous entry*/
            /* no previous entry is possible if key was deleted without*/
            /* update of upper level                      */
            pfndp1->position = pfndp1->pprv;/*to the previous entry to go down*/
    }
    /* Sanity check */
#ifdef VARIABLE_DBKEY
        cxGetDBK(pcontext, pfndp1->position, &dbkey);
    if (dbk != dbkey)
        FATAL_MSGN_CALLBACK(pcontext, cxFTL004);
#else
/*    if (dbk != xlng(pfndp1->position + ENTHDRSZ + pfndp1->position[1])) */
    if (dbk != xlng( pfndp1->position 
                   + HS_GET(pfndp1->position) 
                   + KS_GET(pfndp1->position)
                   ))
        FATAL_MSGN_CALLBACK(pcontext, cxFTL004);
#endif  /* VARIABLE_DBKEY */

    /**** if zero key in 1st entry of non-leaf */
    pfather = (cxblock_t *)bmGetBlockPointer(pcontext,father);
    if (pfndp1->position == &pfather->ixchars[0] && pfather->IXNUMENT > 1)
    {
        /* the entry to remove is the left most in the block,
        IT MAY NOT BE REMOVED. its key which is the same as the level above
        must remain as the left most key in the block. In this case
        the second entry will be removed, after copying the dbkey to
        the first entry.  This way the left most key is preserved but pointing
        to the the block that used to be pointed by the second entry. */

        pos = pfndp1->position; /* point to the found entry, 1st entry in the block */
/*        FASSERT_CB(pcontext, pos[1] <= (TEXT)1, */
		ks = KS_GET(pos);
        FASSERT_CB(pcontext, ks <= 1,
                   "invalid key length in a non-leaf index block");
/*        pos += ENTHDRSZ + ks + IS_GET(pos);  to next entry */ 
        pos += ENTRY_SIZE_COMPRESSED(pos); /* to next entry */
#ifdef VARIABLE_DBKEY
        cxGetDBK(pcontext, pos, &dbkey); /* get the dbkey of 2nd entry */
        /* The dbkey of the 2nd entry must be of the max size as
        is the is of the 1st entry */
/*====Rich's change=============================== */
/*        is = gemDBKEYtoIdx( dbkey,  sizeof(DBKEY)+1, &tmpdbk[0] ); */
	
        is = gemDBKEYtoIdx( dbkey>>8, CX_MAX_X , &tmpdbk[0] );
 	    tmpdbk[is] = (TEXT)dbkey;
 	    is++;
/*------------------------------------------------ */

        bkReplace(pcontext, father, pos - is, &tmpdbk[0],
                    (COUNT)is);/*replace 1st dbk with 2nd */
#else
        is = 4;
 /*       bkReplace(pcontext, father, pos - is, pos + ENTHDRSZ + pos[1], */
        bkReplace(pcontext, father, pos - is, 
                  pos + HS_GET(pos) + KS_GET(pos), /* dbk in second entry */
                    (COUNT)is);/*replace 1st dbk with 2nd */
#endif  /* VARIABLE_DBKEY */
        pfndp1->position = pos; /* point to 2nd entry to delete it */
    }

    cxRemoveEntry(pcontext, tableNum, pfndp1);    /* delete the entry from the block */

    if (pfather->IXLENENT == 0)
        cxRemoveBlock(pcontext, peap, pfndp1, tableNum, pdownlst); /*remove the predecessor blk */
    else
    while (pdownlst->currlev == 0 && pfather->IXLENENT == 7)
    {
        /* only one entry, the dummuy one, is left in the root,
        eliminate the level below the root, keep the old root */

        count++;

        /* get dbkey of the only son */
        cxGetDBK(pcontext, pfather->ixchars, &pbap->blknum);
                                /* get block with the correct lock mode */
        pbap->bufHandle = bmLocateBlock(pcontext, pdownlst->area,
                                        pbap->blknum, TOMODIFY);
        pbap->pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,pbap->bufHandle);
        son = pbap->bufHandle;
        pson = pbap->pblkbuf;

        /* if the son is on a lock chain don't copy it to the root.  When */
        /* we remove the locks from it it needs to be at the same DBKEY  */
        if (pson->BKFRCHN == LOCKCHN)
        {
            break;
        }

        /* move all info from the son to the root, then eliminate the son */
        /* bkReplace can only handle data length of 255 - sizeof(BKRPNOTE)
        we must loop */
        for (i = 0; (pson->IXLENENT - i) > 0; i += (255 - sizeof(BKRPNOTE)))
            bkReplace(pcontext, father, &pfather->ixchars[0] + i,
                &pson->ixchars[0] + i,
                (COUNT)((pson->IXLENENT - i) >= 
			(COUNT)(255 - sizeof(BKRPNOTE)) ?
                (255 - sizeof(BKRPNOTE)) : (pson->IXLENENT - i)));

        if (pfather->IXTOP == 1)
            bkReplace(pcontext, son, (TEXT *)&pson->IXTOP,
                    (TEXT *)&pfather->IXTOP, (COUNT)1);
        bkReplace(pcontext, father, (TEXT *)&pfather->ix_hdr,
                (TEXT *)&pson->ix_hdr, (COUNT)sizeof(pson->ix_hdr));
        cxFreeBlock(pcontext, pbap);    /* son to free chain */
        if (count > 1)
            bmReleaseBuffer(pcontext, father); /* 1st time the father is i
                                  not released it will be by the caller */
        father = son;
        pfather = pson;
    }
    if (son)
            bmReleaseBuffer(pcontext, son);

    pdownlst->currlev++;/*down a level, must - to ensure unlock by caller */
    return(0);

}       /* cxRemoveBlock */

/***********************************************************************/
/* PROGRAM:  cxGetDBK - gets the INFO dbk from a B-tree entry pointed by pos
 *              THIS is only good for non-leaf levels.
 * RETURNS: 0
 */
int
cxGetDBK(
        dsmContext_t    *pcontext _UNUSED_,
        TEXT            *pos,   /* to 1st byte of the entry ( the CS byte) */
        DBKEY           *pdbkey)
{
 
    /* advance to Data from position */
#ifdef VARIABLE_DBKEY
/*    gemDBKEYfromIdx( pos + *(pos+1) + ENTHDRSZ, pdbkey ); */
    gemDBKEYfromIdx( pos + KS_GET(pos) + HS_GET(pos), pdbkey );
#else
/*    *pdbkey = xlng(pos + *(pos+1) + ENTHDRSZ);  from string dbkey to LONG */ 
    *pdbkey = xlng(pos + KS_GET(pos) + HS_GET(pos)); 
#endif  /* VARIABLE_DBKEY */
 
    return(0);

}       /* cxGetDBK */

/*****************************************************************/
/* PROGRAM:  cxExpandKey -  reconstruct the compressed part of a key
 *
 * RETURNS: 0
 */
LOCALF int
cxExpandKey(
        dsmContext_t    *pcontext _UNUSED_,
        TEXT            *pto,    /* target area */
        TEXT            *pstart, /* first entry in the block */
        TEXT            *pentry, /* position of entry to be expanded */
        int              leng)   /* length of required expansion */
{
        TEXT    *pos;
        int     cs;
        int     ks;
        int     is;
        int     hs;

    pos = pstart;
    while (pos < pentry)
    {
        /* get cs, ks, is */
/*        cs = *pos++; */
/*        ks = *pos++; */
/*        is = *pos++; */
        CS_KS_IS_HS_GET(pos);
        pos += hs;

        if (leng > cs)
            bufcop(pto + cs, pos, leng - cs);
        /* step to next entry */
        pos += ks + is; /* point to the next entry. */
    }
    return(0);

}       /* cxExpandKey */







/*********************************************************/
/* PROGRAM: cxEstimateTree - Estimate the size of the a given bracket relative
 *                            to the size of the table. The bracket is represented
 *                            by a BASE and a LIMIT keys.
 *
 *               The procedure returns a fraction which is the estimation of:
 *
 *               number of rows in the bracket / number of rows in the table
 *
 *               Given bracket BASE and LIMIT keys,
 *               starting at the root of the index B-tree, if both keys belong
 *               to the same branch of the tree it continues to traverse down the
 *               index B-tree until at some level each key belongs to a different
 *               branch. At this point the range estimate can be done and there
 *               is no need to continue travers the tree any more.
 *
 *      The estimated fracion is calculated as follows:
 *
 *         A fraction is calculated at each level of the B-tree that is traversed.
 *         If both BASE and LIMIt belong to the same tree brach the fraction of
 *         this tree level =  1 / number of entries (branches)in the block.
 *
 *         If the BASE and LIMIT keys belong to different brances
 *         the fraction = number of entries in the bracket / number of entries
 *                           (branches)in the block
 *         At this point the traverse stops and the returned fraction is
 *         the multiplication of the fractions of all the traversed levels.
 *         
 *
 * RETURNS:     A fraction representing the estimated
 *               number of rows in the bracket / number of the rows in the table
 */
float
cxEstimateTree(
        dsmContext_t    *pcontext,
        TEXT             *pBaseKey,
        int              baseKeySize,
        TEXT             *pLimitKey,
        int              limitKeySize,
        ULONG            area,             /* the db area where the index is located */
        DBKEY            rootDBKEY         /* pointer to the root block */
        )
{
        TEXT      *pos;          /* address of the position in the block */
        cxbap_t   bap;          /* block access parameters */
        UCOUNT    lockmode;
        inlevel_t   *plevel;
        int         baseEnum, limitEnum;
        float       fraction = 1;
        int         ixnum;
        dbcontext_t*  pdbcontext = pcontext->pdbcontext;
        cxblock_t     *pixblk;

        downlist_t        downlst;   /* to list down, with starting level */

    /* initialize parameters */
    downlst.currlev = downlst.locklev = 0;
    downlst.area = area;
    plevel = downlst.level;       /* pointer to the level info */
    plevel->dbk = bap.blknum = rootDBKEY;
    lockmode = TOREAD;

    ixnum = xct(pBaseKey+2);

    /* Collect stats of index access */
    if ( pdbcontext->pidxstat &&
         (pdbcontext->pdbpub->isbaseshifting ==0 ) &&
         ixnum >= pdbcontext->pdbpub->argindexbase &&
         ixnum < pdbcontext->pdbpub->argindexbase +
                 pdbcontext->pdbpub->argindexsize )
         ((pdbcontext->pidxstat)[ ixnum -
                                 pdbcontext->pdbpub->argindexbase ]).read++;
 

    /* Each pass through this loop will move one level down the tree.
       This terminates when a leaf block is reached                 */
    for(;;)
    {
        /* read the next block to be examined */

        bap.bufHandle = bmLocateBlock(pcontext, area, bap.blknum,
                                      lockmode);


        bap.pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,bap.bufHandle);
        pixblk = bap.pblkbuf;
     
        /* enumerate the base key */
        baseEnum = cxEnumEntry(pixblk, pBaseKey, baseKeySize, &pos);

        /* enumerate the limit key */
        limitEnum = cxEnumEntry(pixblk, pLimitKey, limitKeySize, &pos);

        if (pixblk->IXBOT)              /* if leaf block */
        { 
            /* At the leaf level the actual number of records in
               the range is returned, not the fraction of records in
               the range.   */ 
            fraction = (float)(limitEnum - baseEnum);
            break;
        }
        if ( baseEnum == limitEnum )
            fraction /= pixblk->IXNUMENT;
        else
        {
            FASSERT_CB(pcontext,baseEnum <= limitEnum,"limit smaller than base"); 
            if (baseEnum < limitEnum )
            {
                fraction *= (float)(limitEnum - baseEnum) / 
                            (float)pixblk->IXNUMENT;
                break;
            }
        }


        /* it is a non-leaf block, traverse down the tree */
        cxGetDBK(pcontext, pos, &bap.blknum); /* get blknum of block to go down with */


        cxReleaseAbove(pcontext, &downlst); /* release all predecessors */

        plevel++;       /* to next level of the B-tree */
        downlst.currlev++;    /* to next level of the B-tree */
        downlst.locklev++; /* count locked predecessor levels */

        plevel->dbk = bap.blknum; /* current block to list down */
    }

    cxReleaseAbove(pcontext, &downlst); /* release all predecessors */
    bmReleaseBuffer(pcontext,bap.bufHandle);
    return fraction;
}       /* cxEstimateTree */




/*************************************************************************/
/* PROGRAM: cxEnumEntry - find an entry in a given block and return its
 *                        ENUMERATION.  The enumeration is the count of
 *                        index entries up to the position of the search key.
 *                        In the leaf level the enumeration is the number
 *                        of records represented up to the position of the
 *                        search key.
 *  If the exact key was not found the ENUMERATION is the number of entries 
 *  preceeding its inserted position in the block. If the key was found 
 *  in a non-leaf level we add one to the number of predeeding entries.                                    
 *
 * RETURNS:     the ENUMERATION in the block of the entry relevant to the searched key
 *              
 *              pDownEntry: points to the index entry to traverse down the tree with.
 *                          It is the entry of the relevant bracket branch.
 */
LOCALF int
cxEnumEntry(
        cxblock_t   *pixblk,        /* pointer to the searched block */        
        TEXT        *pinpkey,       /* pointer to the key to be searched */
        UCOUNT       inkeysz,       /* number of bytes in the key to be searched */
        TEXT        **pDownEntry)   /* returned pointer to the entry to traverse down with */
{
        TEXT    *pos, *position;/* addr of the position in the searched blk */
        TEXT    *keypos;        /* addr of the position in the search key*/
        TEXT    *bkkeyend;      /* end of current key in the block */
        TEXT    *keyend;        /* end of the search key */
        TEXT    *pfreepos;      /* 1'st free pos past end of text in the blk */
        int     cs;             /* size of the compressed part */
        int     ks;             /* size of the key */
        int     is;             /* size of data (info size) */
	int     hs;             /* size of key header  */
        int     keycs;          /* CS of search key relative to current key*/
        int     count = 0;

    /* initialization of the search */
    pos = pixblk->ixchars;      /* start of 1st entry */
    position = pos;
    pfreepos = pos + pixblk->IXLENENT;
    keypos = pinpkey;            /* point to start of search key */
    keyend = keypos + inkeysz;
    keycs = 0;
    *pDownEntry = 0;   /* init pointers to prev. entrys */

    /* pos points to the CS of the current entry */
    while ((pos < pfreepos))            /* while there is an entry in pos */
    {

        /* get cs, ks, is */
/*        cs = *pos++; */
/*        ks = *pos++; */
/*        is = *pos++;                     point to the key */ 
        CS_KS_IS_HS_GET(pos);
        pos += hs;

        if (keycs == cs)
        {
            if (ks == 0)                /* only for dummy entry in 1'st blk */
                goto next;

            bkkeyend = pos + ks;
            while (*keypos == *pos)     /* the two bytes are equal */
            {
                keycs++;        /* increment the CS of the search key */
                keypos++;       /* step to next search key position */
                pos++;          /* step to next position of the key in blk */

                if (keypos == keyend)           /* end of search key */
                {
                    if (pos != bkkeyend)        /* search key is shorter */
                        goto not_found;
                    else                        /* keys are equal */
                    {
                        if( ! pixblk->IXBOT)
                        {
                            *pDownEntry = position; /* pointer to prev. entry */
                            count++; /* to force down traverse if only entry in range */
                        }
                        return count;               /* keys are equal */
                    }   /* end of keys are equal */

                }       /* end of keypos == keyend */
                else if (pos == bkkeyend)       /* blk key ended, srch key */
                    goto next;                  /* didn't: blk key is longer */
            }   /* end while */

            if (*keypos < *pos)         /* search key is lower */
                goto not_found;

            pos = bkkeyend;
            goto next;                          /* search key is higher */
        }       /* end cs == keycs */

        if (keycs > cs)
        {
            keycs = cs;         /* set CS of search key to that of the cur */
            goto not_found;                     /* search key is lower */
        }

        /* search key is higher, step to next entry */
        pos = pos + ks;                 /* point to data */

    next:      /* Go to next index entry,  pos is on data, skip to next entry */
        if(pixblk->IXBOT)
        {
            /* it is a leaf, count entries inbitmap or tag list */
    
            if (is == 33)       /* a bit map: count is the 1'st info byte */
            {
                /* zero bit count represents 256 */
                if ( *pos == 0 )
                    count += 256;
                else
                    count += *pos;
            }
            else                    /* the info size is the count of entries */
            {
                count += is;   /* tag count = IS */
                /* notice that DUMMY entry has IS=0 and therefore not counted */
            }
        }
        else
        {
            count++;                /* count previous entries in the block */
            *pDownEntry = position;         /* pointer to prev. entry */
        }

        pos = pos + is;                 /* point to CS of next entry if exist */
        position = pos;                 /* address of the current entry */
    }                                   /* while there are more */
    /* end of entries, no entries in the block */

not_found:
    /* no more entries, or the search key is smaller, end of search */
    return count;                         /* return ENUMERATION */

}       /* cxEnumEntry */



#if 0
/* obsolete 10-19-98 */
/* PROGRAM: cxSetixparams - Sets index related params in public structure
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxSetixparams(
        dsmContext_t    *pcontext,
        UCOUNT           block_size,
        COUNT           *pixtbllen,
        COUNT           *pitblshft,
        COUNT           *pitblmask,
        UCOUNT          *pmaxidxs)
{
        LONG ixtbllen, itblshft, maxidxs;

        /* number of anchors that can fit in a block */
        ixtbllen = BLKSPACE(block_size) / IXANCHRLEN;

        /* make it a power of 2 */
        for (itblshft = 0; (ixtbllen = ixtbllen >> 1) > 0; itblshft++);
        *pitblshft = (COUNT) itblshft;

        for (ixtbllen = 1; --itblshft >= 0; ixtbllen = ixtbllen << 1 );
        *pixtbllen = (COUNT) ixtbllen;
        *pitblmask = (COUNT) (ixtbllen-1);

        /* This can be removed when 'Index Anchor Tables' are gone */
        maxidxs = ixtbllen * MAXIXTB;
        /* ??? Is it Right - Max is 8192 */
        *pmaxidxs = (maxidxs > 8192) ? 8192 : (COUNT)maxidxs;

}       /* cxSetixparams */

#endif
