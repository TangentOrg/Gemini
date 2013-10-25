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

#include "gem_global.h"

#include "dbconfig.h"

#include "bkpub.h"
#include "bmpub.h"  /* for bmReleaseBuffer call */
#include "cxmsg.h"
#include "cxpub.h"
#include "cxprv.h"
#include "dbcode.h"
#include "dbcontext.h"
#include "dbpub.h"
#include "dsmpub.h"
#include "ixmsg.h"
#include "keypub.h"     /* for Constants definition LOWRANGE... */
#include "kypub.h"
#include "latpub.h"
#include "lkpub.h"
#include "lkschma.h"
#include "ompub.h"
#include "rlpub.h"
#include "tmmgr.h"
#include "usrctl.h"
#include "statpub.h"
#include "umlkwait.h"
#include "geminikey.h"
#include "scasa.h"
/*#if 0*/
#include "utfpub.h"
#include "utspub.h"
/*#endif*/

#undef FILMIN
#undef KEYMIN
#undef FLDMIN
#undef USRMIN
#undef KCMIN

#if 0
LOCALF DSMVOID     cxStopDebug     ();
#endif

/**** END LOCAL FUNCTION PROTOTYPE DEFINITIONS ****/


/*************************************************************************/
/* PROGRAM: cxAddEntry -- add a new entry to an index
 *
 * RETURNS:
 *    0         OK
 *    IXDUPKEY  The entry already exists and the index doesnt allow dups
 *    RQSTQUED  Another user has a delete lock on that key, this user
 *              has been queued.
 */

int
cxAddEntry(
        dsmContext_t    *pcontext,
        dsmKey_t        *pkey,  /* the key to be inserted in the index   */
        lockId_t        *plockId, /* the record's lock identifier        */
        TEXT            *pname, /* refname for Rec in use msg (child stub */
	dsmObject_t      tableNum) /* table number */
{

    int          ret;
    
    TRACE_CB(pcontext, "cxAddEntry")

#if SHMEMORY
    if (pcontext->pdbcontext->resyncing) return 0; /* resyncing in progress */
    if (lkservcon(pcontext)) return DSM_S_CTRLC;
#endif

  retry:

    ret = lklock(pcontext, plockId, LKEXCL, pname);

    if (ret) return (ret);

    /* Do the add here */
    ret = cxAddNL(pcontext, pkey, plockId->dbkey, pname, tableNum);
    if (ret == IXDLOCK) goto retry;

    if (ret == DSM_S_RQSTQUED)
    {
        if (pcontext->pusrctl->uc_wait == TSKWAIT)
        {
            /* chain the user to TSKWAIT chain if task is alive */
            if (lktskchain(pcontext)) goto retry; 
        }

        if  ( pcontext->pusrctl->uc_block )
        {
            if ((ret = lkwait(pcontext, LW_RECLK_MSG, pname)) == 0)
                goto retry;
            return(ret);
        }

    }
    return(ret);

}  /* end cxAddEntry  */


/************************************************************************/
/* PROGRAM: cxAddNL -- add a new entry to an index
 *
 * RETURNS:
 *    0         OK
 *    IXDUPKEY  The entry already exists and the index doesnt allow dups
 *    RQSTQUED  Another user has a delete lock on that key, this user
 *              has been queued.
 */
/*ARGSUSED*/
int
cxAddNL(
        dsmContext_t    *pcontext,
        dsmKey_t        *pkey,     /* the key to be inserted in the index */
        DBKEY            dbkey,    /* the record's dbkey */
        TEXT            *pname _UNUSED_, /* refname for Rec in use msg (child stub*/
	dsmObject_t      tableNum) /* table number */
{

    cxeap_t     eap;            /* Entry Access Parameters */
    cxeap_t    *peap = &eap;    /* to Entry Access Parameters */
    int         ixnum;
    int         ret;

   TRACE_CB(pcontext, "cxAddNL")

    /* ALL indexes need the dbkey on the initial find call */
    if (dbkey == 0)
        FATAL_MSGN_CALLBACK(pcontext, ixFTL001);      /*FATAL*/
    /* the index repair utility doesnt start a transaction */
    /* but everyone else is required to                    */
        if ( pcontext->pusrctl->uc_task==0
        && !cxIxrprGet()
        && pcontext->pdbcontext->dbcode != PROCODET)
        FATAL_MSGN_CALLBACK(pcontext, ixFTL008);

    /* prepare access parameters */
    peap->flags = CXPUT+CXTAG;
    peap->pcursor = 0;
    ret = cxKeyPrepare(pcontext, pkey, peap, dbkey);

    ixnum = pkey->index;
    /* BUM - Assuming ixnum < 2^15 */

    if ((!pkey->unknown_comp) /* no UNKNOWN components */
    &&  (pkey->unique_index & DSMINDEX_UNIQUE)) /* if nuniq and no UNKNOWN components */
    {
        /* unique key with no unknown components */
        peap->flags |= CXUNIQE;/* reject if another exists*/
    }
    /* else
         either non-unique situation, or UNKNOWN value */

    ret = cxPut(pcontext, peap, tableNum);

/************TEMPORARY FOR DEBUG ***
MSGD("%LcxAdd ret %i len %i key %t %t %t %t %t %t %t %t %t %t %t %t"
                ,ret, peap->inkeysz,
                peap->pinpkey[2], peap->pinpkey[3], peap->pinpkey[4],
                peap->pinpkey[5], peap->pinpkey[6], peap->pinpkey[7],
                peap->pinpkey[8], peap->pinpkey[9], peap->pinpkey[10],
                peap->pinpkey[11], peap->pinpkey[12], peap->pinpkey[13]);
*********/
    if (ret == 1)
    {
        return peap->errcode;
    }

    return ret;

}  /* cxAddNL */

#if 0
/**********************************************************/
/* PROGRAM: cxCheckEntries - test for existing entries in an index
 *
 * RETURNS: 1 if index has any entrys, otherwise 0
 */
int
cxCheckEntries(
        dsmContext_t    *pcontext,
        int              ixnum)
{
    bmBufHandle_t   ixblkHandle;
    cxblock_t      *pixblk;
    int             ret;
    omDescriptor_t  indexDesc;

    ret = omFetch(pcontext, DSMOBJECT_MIXINDEX, (UCOUNT)ixnum,&indexDesc);
    if(ret)
    {
        return 0;
    }
    ixblkHandle = bmLocateBlock(pcontext, indexDesc.area, indexDesc.objectRoot,
                                TOMODIFY);
    pixblk = (cxblock_t *)bmGetBlockPointer(pcontext,ixblkHandle);
    if( pixblk->IXBOT )
        ret = cxDelLocks(pcontext,ixblkHandle,0);
    
    if(pixblk->IXNUMENT > 1)
        ret = 1;
    else
        ret = 0;
    
    bmReleaseBuffer(pcontext, ixblkHandle); /* Release the buffer for
                                  others to use */
    return ret;

}  /* cxCheckEntries */
#endif

/**************************************************/
/* PROGRAM: cxDeleteEntry -- delete an entry from an index
 *
 * RETURNS: 0   OK
*/

int
cxDeleteEntry(
        dsmContext_t    *pcontext,
        dsmKey_t        *pkey,          /* the key to be deleted */
        lockId_t        *plockId,       /* lock identifier of indexed record */
        int              lockmode,       /* unused - should be zero        */
        dsmObject_t     tableNum,
        TEXT            *pname)         /* refname for Rec in use msg
                                           (child stub */
{

        int      ret;

    TRACE_CB(pcontext, "cxDelete")


    if (pcontext->pdbcontext->resyncing) return 0;  /* resyncing in progress */
    if (lkservcon(pcontext)) return DSM_S_CTRLC;
    lockmode = pcontext->pdbcontext->pdbpub->slocks[EXCLCTR-1] ? IXNOLOCK : IXLOCK;

retry:


    ret = lklock(pcontext, plockId, LKEXCL, pname);

    if (ret)
        return ret;

    /* delete the key */
    ret = cxDeleteNL(pcontext, pkey, plockId->dbkey, lockmode, tableNum, pname);


    if (ret == IXDLOCK) goto retry;

    return(ret);

}  /* cxDeleteEntry */

/***************************************************/
/* PROGRAM: cxDeleteNL -- delete an entry from an index
 *
 * RETURNS: 0   OK
*/

/*ARGSUSED*/
int
cxDeleteNL(
        dsmContext_t    *pcontext,
        dsmKey_t        *pkey,          /* the key to be deleted */
        DBKEY            dbkey,         /* dbkey of record to be deleted */
        int              lockmode,      /* Bit mask may have some of:
                                           IXLOCK - put on a delete lock   */
        dsmObject_t   tableNum,
        TEXT            *pname _UNUSED_) /* refname for Rec in use msg (child stub*/
{
     cxeap_t            eap;           /* Entry Access Parameters */
     cxeap_t           *peap= &eap;    /* to Entry Access Parameters */
     int                ret;

    TRACE_CB(pcontext, "cxixdelf")

    if (pcontext->pusrctl->uc_task==0
        && pcontext->pdbcontext->dbcode != PROCODET )
        FATAL_MSGN_CALLBACK(pcontext, ixFTL009);

    /* prepare access parameters */
    peap->flags = CXDEL+CXTAG;
    peap->pcursor = 0;
    ret = cxKeyPrepare(pcontext, pkey, peap, dbkey);

    /* either put a delete lock on the key, or delete it from the block */
    /* cxCheckUnique needs to be replaced by information in key structure */
    if (
        (pkey->unique_index             /* unique index         */
          && lockmode & IXLOCK          /* not recovery or single user  */
	 && !(SCTBL_IS_TEMP_OBJECT(pkey->index)) /* nor temp table   */
          && !(pcontext->pusrctl->uc_schlk & EXCLCTR) /* We have the schema lock
                                               so one else can be accessing
                                               indices                  */
          && !pkey->unknown_comp))     /* and all comps not missing    */
    {
        /* We'll need index hold entries for deletes on unique
           index entries.  Index hold entries prevent the unique
           index value from being used by another user - have
           to do this in case transaction rolls back      */
        peap->flags |= CXLOCK;
    }


    /* count index entry delete statistics */

    /* delete it */
    ret = cxRemove(pcontext, tableNum, peap);

/************TEMPORARY FOR DEBUG ***
MSGD("%LcxDeleteNL ret %i len %i key %t %t %t %t %t %t %t %t %t %t %t %t"
                ,ret, peap->inkeysz,
                peap->pinpkey[2], peap->pinpkey[3], peap->pinpkey[4],
                peap->pinpkey[5], peap->pinpkey[6], peap->pinpkey[7],
                peap->pinpkey[8], peap->pinpkey[9], peap->pinpkey[10],
                peap->pinpkey[11], peap->pinpkey[12], peap->pinpkey[13]);
*********/

    return ret;

}  /* cxDeleteNL */

/***********************************************************************/
/* PROGRAM: cxDelLocks -- purge all dead delete locks created by a xaction
 *
 * RETURNS:
 *          if upgrade is zero returns
 *              0:      block is empty - ok to use.
 *              1:      block should be put at end of ix delete chain
 *              2:      block contains at least one non index entry.
 *
 *          if upgrade is 1 returns
 *              0:      no dead delete locks were found
 *              1:      at least one dead delete lock was found
 */


LONG
cxDelLocks(
        dsmContext_t    *pcontext,
        bmBufHandle_t    ixblkHandle, 
	dsmObject_t      tableNum,
        int              upGrade       /* Upgrade buffer lock */)
{
     TEXT               *pos;
     TEXT               *pend;
     cxblock_t          *pixblk;
     int                 hs;
     int                 ks;
     int                 cs;
     int                 is;
     DBKEY               trId;   /* must be size of DBKEY for possible 64 bits */
     cxeap_t             eap;            /* Entry Access Parameters */
     cxeap_t            *peap = &eap;    /* to Entry Access Parameters */
     LONG                returnCode = 0;
     findp_t             fndp;           /* FIND P. B. */
     findp_t            *pfndp = &fndp;  /* pointer to the FIND P. B. */
     LONG                upGraded = 0;   /* Has buffer lock been upgraded */
     omDescriptor_t      objectDesc;
#ifdef VARIABLE_DBKEY
    TEXT                *pos1;
    int                  dbkeySize;
#endif  /* VARIABLE_DBKEY */

/*     AUTOKEY(k, KEYSIZE + ENTHDRSZ + sizeof(LONG)) */
     AUTOKEY(k, LRGKEYSIZE + FULLKEYHDRSZ + sizeof(LONG))
    

    /* Get a pointer to the block in the database buffer */
    pixblk = (cxblock_t *)bmGetBlockPointer(pcontext,ixblkHandle);
 
    /* get the key into the ditem */

    kyinitKeydb(&k.akey, pixblk->IXNUM, 1, 0);

    /* prepare access parameters */

    peap->pcursor = 0;
    peap->flags = CXDEL+CXTAG+CXNOLOG;
    peap->pkykey = &k.akey;
/*    peap->pinpkey = &k.akey.keystr[ENTHDRSZ]; */
    peap->pinpkey = &k.akey.keystr[FULLKEYHDRSZ];
    pfndp->pkykey = peap->pkykey;
    /* scan all but the first entry and remove dead ones */

    pos = pixblk->ixchars;      /* start at beginning */
 
/*    for (pend = pos + pixblk->IXLENENT; pos != pend; pos += ENTHDRSZ+ks+is) */
/*    { */
/*        cs = pos[0]; */
/*        is = pos[2]; */
/*        ks = pos[1]; */
    for (pend = pos + pixblk->IXLENENT; pos != pend; pos += hs+ks+is)
    {
        CS_KS_IS_HS_GET(pos);
       if (!ks) 
        {
           /* The dummy entry won't be removed so the caller shouldn't
              re-use this block.                                       */
           returnCode = 2;
           continue;    /* it is the left most (dummy) entry */
        }



#ifdef VARIABLE_DBKEY
/*        pos1 =  &pos[ks + ENTHDRSZ];  to data */
        pos1 =  &pos[ks + hs]; /* to data */
        dbkeySize = *(pos1 - 1) & 0x0f; /* size of the dbkey part in the key, without the last byte */
        pos1 -= dbkeySize; /* to start of the variable dbkey */
        gemDBKEYfromIdx( pos1, &trId );  /* part of dbkey from the key */
/*      trId <<= 8; */
/*   	trId += *pos1;   the last byte of the dbkey */ 
#else
/*        trId = xlng (&pos[ks + ENTHDRSZ - 3]); */
        trId = xlng (pos[ks + hs - 3]);
#endif  /* VARIABLE_DBKEY */

        if (trId < 0)
        {
/*            bufcop (&k.akey.keystr[ENTHDRSZ+cs], pos + ENTHDRSZ, ks+is); */
            bufcop (&k.akey.keystr[FULLKEYHDRSZ+cs], pos + hs, ks+is);
            /* negative last key component. It is a delete lock */
            trId = - trId;
            if (tmremdlcok (pcontext, trId))
            {
                /* transaction manager says we can delete this entry */
                STATINC(dlckCt);
/*                k.akey.keystr[1] = (TEXT)(peap->inkeysz = cs + ks); */
/*                k.akey.keystr[2] = (TEXT)(peap->datasz = is); */
/*                k.akey.keystr[0] = peap->inkeysz + is; total */ 
                peap->inkeysz = cs + ks;
                peap->datasz = is;
                sct(&k.akey.keystr[TS_OFFSET], cs + ks + is); /*total */
                sct(&k.akey.keystr[KS_OFFSET], cs + ks);
                sct(&k.akey.keystr[IS_OFFSET], is);

                peap->pdata = peap->pinpkey + peap->inkeysz;
                /* If upGrade flag is set we'll be adding a new entry to
                   this block so don't allow block to be removed
                   from the index.       */
/*                if ((pixblk->ixchars + ENTHDRSZ + ks + is == pend) && !upGrade) */
                if ((pixblk->ixchars + hs + ks + is == pend) && !upGrade)
                {
                    if( rlGetWarmStartState(pcontext) )
                    {
                        /* Can't do the index number to area mapping during
                           warm start.                                       */
                        return (2);
                    }
                    if( omFetch(pcontext,DSMOBJECT_MIXINDEX, k.akey.index,
                                tableNum, &objectDesc))
                        return (2);
                    k.akey.area = objectDesc.area;
                    k.akey.root = objectDesc.objectRoot;
                    k.akey.unique_index =
                                 (dsmBoolean_t)objectDesc.objectAttributes &
                                               DSMINDEX_UNIQUE;
                    k.akey.word_index   = 
                                 (dsmBoolean_t)objectDesc.objectAttributes &
                                               DSMINDEX_WORD;

                    /* deleting the only entry, do it slowly */
                    /* because higher level ix blks are affected */
                    bmReleaseBuffer(pcontext, ixblkHandle); /* must lock from root */
                    cxRemove (pcontext, tableNum, peap);   /* will lock unsafe blocks from top */
                    return (returnCode);
                }
                else
                {
                    /* not the only entry, its easy to purge it */
                    pfndp->pblkbuf = pixblk;
                    pfndp->bufHandle = ixblkHandle;
                    pfndp->position = pos;
                    pfndp->flags = CXNOLOG;
                    pfndp->inkeysz = peap->inkeysz;
                    pfndp->pinpkey = peap->pinpkey;

                    /* if we are here, this is a unique index */
                    pfndp->pkykey->unique_index = 1;
                    
                    if ( upGrade && !upGraded )
                    {
                       /* If upGrade flag is set we're being called from
                          cxCheckInsert to see if a block split will
                          be necessary.  At this point the buffer has
                          an intent lock and we'll need to upgrade to
                          exclusive before we can change the block.   */
                       bmUpgrade(pcontext, ixblkHandle); 
                       upGraded = 1;
                    }
                    cxRemoveEntry(pcontext, tableNum, pfndp);

                    /* pos should not advance, pend must be recalculated */

                    pend = pixblk->ixchars + pixblk->IXLENENT;
/*                    ks = -ENTHDRSZ;      a trick to avoid advance of pos */
                    ks = -hs;     /* a trick to avoid advance of pos */

                    is = 0;     /* a trick to avoid advance of pos */
                }
            }
            else
            {
                /* Entry is a delete lock for an active tx.   */
                returnCode = 1;
            }
        }
        else
        {
            /* Block contains a real index entry     */
            returnCode = 2;
        }
    }
    if ( upGraded )
    {
       /* We must down grade to an intent lock; otherwise deadlock could
          occur.   */
       bmDownGrade(pcontext, ixblkHandle);
    }
    if( upGrade )
        return (upGraded);
    else
        return (returnCode);

}  /* cxDelLocks */

/**********************************************************************/
/* PROGRAM: cxCheckLocks -- Check if block contains all delete locks
 *
 * RETURNS:
 *              0       block has entries that aren't delete locks.
 *              1       block contains only delete locks.
 */
LONG
cxCheckLocks (
        dsmContext_t    *pcontext,
        bmBufHandle_t    ixblkHandle)
{
     TEXT               *pos;
     TEXT               *pend;
     cxblock_t          *pixblk;
     int                 ks;
     int                 hs;
     LONG                returnCode = 1;

#ifdef VARIABLE_DBKEY
    TEXT                *pos1;
#endif  /* VARIABLE_DBKEY */


    /* Get a pointer to the block in the database buffer */
    pixblk = (cxblock_t *)bmGetBlockPointer(pcontext,ixblkHandle);
 
    pos = pixblk->ixchars;      /* start at beginning */

    for (pend = pos + pixblk->IXLENENT; pos != pend; pos += 1+ks+hs)
    {
/*        ks = pos[1]; */
        hs = HS_GET(pos);
        ks = KS_GET(pos);
	/* is is always one for unique indexes.    */
        if (!ks) 
        {
           returnCode = 0;
           break;       /* it is the left most (dummy) entry */
        }
#ifdef VARIABLE_DBKEY
        pos1 = &pos[hs + ks];
        if( (*(pos1 - 
	       ( *(pos1 - 1) & 0x0f )   /*  size of the variable DBKEY */
	       ) >> 8) /* exponent */
	    > 7 ) /* positive dbkey if exponent > 7 */
#else
        if ((pos[hs + ks - 3] & 0x80) == 0x00)
#endif  /* VARIABLE_DBKEY */
        {
            returnCode = 0;
            break;
        }      
    }
    return (returnCode);
}  /* cxCheckLocks */

/******************************************************************/
/* PROGRAM: cxKeyPrepare -- prepare the input key and input parameters
 * 
 * NOTICE!!!! - DBKEY is now 4 bytes int, in the future it will be 8 bytes int
 *
 * RETURNS:  -1 if the key included UNKNOWN 
 *            0 otherwise
 */

int
cxKeyPrepare(
        dsmContext_t    *pcontext _UNUSED_,
        dsmKey_t        *pkey,  /* the key to be inserted in the index  */
        cxeap_t         *peap,  /* to Entry Access Parameters */
        DBKEY            dbkey) /* the record's dbkey                   */
{
        int     ret=0;
        int     len;

    /* make sure substring is always cleared */
    pkey->ksubstr = (GTBOOL) 0;  

    /* prepare access parameters */
    peap->pkykey = pkey;
/*    peap->pinpkey = &pkey->keystr[ENTHDRSZ];  to new key */ 
    peap->pinpkey = &pkey->keystr[FULLKEYHDRSZ]; /* to new key */
    peap->inkeysz = pkey->keyLen;

#ifdef VARIABLE_DBKEY
    /* the DBKEY without the last byte is converted to a variable DBKEY
        in case of a unique key,  the variable DBKEY is at least 4 bytes
        for transaction # in case of delete
    */
    /* the 32 or 64 bits dbkey - less last byte*/
    len = gemDBKEYtoIdx( dbkey >> 8,
        (pkey->unique_index ) ?   /* if unique index minimum size is 5 */
        sizeof(dsmTrid_t) : 2, 
			 /* ptr to area receiving the Variable size DBKEY */
		        &peap->pinpkey[peap->inkeysz] );
           
    peap->inkeysz += len;


    peap->pdata = &peap->pinpkey[peap->inkeysz]; /* last byte of the dbkey */
    peap->pdata[0] = (TEXT)dbkey;   /* last byte of the dbkey to IS */
   
#else
    /* 3 most significant bytes to end of the key, last byte to data */
    slng(&peap->pinpkey[peap->inkeysz], dbkey);
    peap->inkeysz += 3;
    peap->pdata = &peap->pinpkey[peap->inkeysz]; /* last byte of the dbkey*/
#endif  /* VARIABLE_DBKEY */

    peap->datasz = 1;

/*    pkey->keystr[0] = (TEXT)peap->inkeysz + (TEXT)peap->datasz */
/*                                            + ENTHDRSZ;  total size in 1st byte */ 
/*    pkey->keystr[1] = (TEXT)peap->inkeysz; */
/*    pkey->keystr[2] = (TEXT)peap->datasz; */
    sct(&pkey->keystr[TS_OFFSET], peap->inkeysz + peap->datasz + FULLKEYHDRSZ);
    sct(&pkey->keystr[KS_OFFSET], peap->inkeysz);
    sct(&pkey->keystr[IS_OFFSET], peap->datasz);
    return ret;
}  /* cxKeyPrepare */

static GBOOL cxixrpr = 0; /* static variable keeps track of whether 
                            index utilities are runing            */

/******************************************************************/
/* PROGRAM: cxIxrprSet - set the static variable ixrpr to on         
 *
 * RETURNS:   DSMVOID 
 */

DSMVOID
cxIxrprSet()
{
        cxixrpr = 1;
}

/******************************************************************/
/* PROGRAM: cxIxrprGet - get the static variable ixrpr to on         
 *
 * RETURNS:   GBOOL value of ixrpr 
 */

GBOOL
cxIxrprGet()
{
        return( cxixrpr );
}

#if 0
LOCALF
DSMVOID
cxStopDebug()
{}
#endif
