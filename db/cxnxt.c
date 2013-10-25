
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
#include "bkpub.h"  /* contains declarations needed by rlpub and cxprv */
#include "bmpub.h"
#include "dsmpub.h"
#include "kypub.h"
#include "dbpub.h"
#include "usrctl.h" /* BUM contains the global definition of the usrctl
                       structure pointed to by pusrctl */
#include "dbcontext.h"  /* BUM defines global pointer dbctl block */
#include "keypub.h"  /* contains constants used in managing indexes */
#include "ixmsg.h"
#include "latpub.h"
#include "latprv.h"
#include "lkpub.h"  /* defines constants specify lock level */
#include "stmpub.h" /* storage manager subsystem */
#include "stmprv.h"
#include "cxpub.h"
#include "cxprv.h"
#include "geminikey.h"

#include "utspub.h"
#include "ompub.h"
#include "scasa.h"  /* for SCTBL_VST_FIRST */


/**** LOCAL FUNCTION PROTOTYPE DEFINITIIONS ****/

LOCALF DSMVOID        cxOutSize   (cxeap_t *peap);

LOCALF cxcursor_t *cxFindCursor(CURSID cursid);


#define CXDEBUGX 0
#if CXDEBUGX
LOCALF DSMVOID     cxkeyprt   (int l, TEXT *p);
#endif

/**** END LOCAL FUNCTION PROTOTYPE DEFINITIONS ****/


/* local variables used only by cxnxt                    */
LOCAL latch_t          cursorsLatch; /* per process cursor latch   */
LOCAL cxCursorList_t  *pcursorList;  /* anchor for all the cursors */
LOCAL int              curscnt;      /* count allocated of cursors */


/**********************************************************************/
/* PROGRAM: cxBackUp -- backup a cursor 1 step, used after lock conflict
 *                      so that CXNEXT will get the current key,
 *                      the CXBACK flag will cause a correction
 *                      if the next operation is CXPREV
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxBackUp (
        dsmContext_t *pcontext _UNUSED_,
        CURSID        cursid)
{
     cxcursor_t   *pcursor;
     TEXT         *pos;
     int           len;
#ifdef VARIABLE_DBKEY
     DBKEY         dbkey;
     int           newlen;
#endif  /* VARIABLE_DBKEY */

#if CXDEBUG
     /* this can't be used until context is pushed through all of
        the cx layer */
    MSGD_CALLBACK(pcontext, (TEXT *)"%LcxBackUp, cursid %i", cursid);
#endif

    /* use the cursor id and the cursor array address to calculate the 
       cursor address                                                  */

    pcursor = cxFindCursor(cursid);
    /* invalidate the cursor so that the search will start at the root */
    pcursor->blknum = 0;

    /* modify the key in the cursor so that the a next would find the same key
        again  */
   
    /* set pos to equal the address of the start of the recid.  This assumes 
        that this is a key in a leaf, and the first three digits of the dbkey
        are in the keystring itself.
    */
#ifdef VARIABLE_DBKEY
     pos = &pcursor->pkey[xct(pcursor->pkey + KS_OFFSET)  + FULLKEYHDRSZ];
     len = *(pos - 1) & 0x0f; /* size of dbkey */
     pos -= len; 
#else
     len = 3;
?????????
      pos = pcursor->pkey 
            + xct(pcursor->pkey + KS_OFFSET) 
            + FULLKEYHDRSZ 
            - len; /* to dbkey */
#endif  /* VARIABLE_DBKEY */

    if (pcursor->unique_index)
    /* if the index is unique, than a 0 in the dbkey will ensure that 
        the key is less then the one last found since 0 is not a valid recid
        and also be greater the the previous key. 
    */
#ifdef VARIABLE_DBKEY
    {
        newlen = gemDBKEYtoIdx( (DBKEY)0, len, pos );
        pos[newlen] = 0;   /* the is byte = 0 */
    }
#else
        slng(pos, (DBKEY)0);
#endif  /* VARIABLE_DBKEY */
    else
    {
        /* since this key is non-unique, subtract 1 from the dbkey to insure 
           that the key is less then or equal the previous key.     */
#ifdef VARIABLE_DBKEY
        gemDBKEYfromIdx( pos, &dbkey); /* get the leading part of the dbkey  */
	/*	dbkey <<= 8; */
	/*dbkey += *(pos + len); */
        dbkey--;
        newlen = gemDBKEYtoIdx( dbkey >> 8, 2, pos );
        pos[newlen] = (TEXT)dbkey;   /* the is byte */
#else
        slng(pos, xlng(pos) -1);
#endif  /* VARIABLE_DBKEY */
        pcursor->flags |= CXBACK; /* to cause +1 if CXPREV is used next
                                     This is needed to ensure a previous does not 
                                     jump over the correct key because it is only 
                                     one less then the last key found. */
    }

#ifdef VARIABLE_DBKEY
    if (newlen != len )
    {
        /* the size of the dbkey has been changed */
/*        pcursor->pkey[1] += newlen - len; */
/*        pcursor->pkey[0] += newlen - len; */
        pos = pcursor->pkey + TS_OFFSET;
        sct (pos, xct(pos) + newlen - len); /* update total size */
        pos = pcursor->pkey + KS_OFFSET;
        sct (pos, xct(pos) + newlen - len); /* update key size */
    }
#endif  /* VARIABLE_DBKEY */
}  /* end cxBackUp */



/**************************************/
/* PROGRAM: cxDeactivateCursor -- deactivate cursor
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxDeactivateCursor(
        dsmContext_t *pcontext,
        CURSID       *pcursid)
{
    cxcursor_t    *pcursor;

#if CXDEBUG
    MSGD_CALLBACK(pcontext, (TEXT *)"%LcxDeactivateCursor: cursid %i",
                   pcursid ? *pcursid : 0);
#endif

    if (pcursid != NULL && *pcursid != NULLCURSID)
    {
        pcursor = cxFindCursor(*pcursid);
        /* already free */
        if ((pcursor->flags & CXINUSE) == 0)
            return;

        if (pcursor->cxusrnum != pcontext->pusrctl->uc_usrnum)
        {
            MSGD_CALLBACK(pcontext, "User %d attempting to free a cursor owned by %d",  pcontext->pusrctl->uc_usrnum, pcursor->cxusrnum);
            return;
        }

#if CXDEBUG
        MSGD_CALLBACK(
            pcontext, 
            (TEXT *)"%LcxDeactivateCursor: cursid        %i%s%s", *pcursid,
            (pcursor->flags & CXNOCURS) ? "   NOCURS" : "         ",
            (pcursor->flags & CXNEWCURS) ? "   NEWCURS": "");
#endif

        /* free the memory used to hold the key */
        if(pcursor->pkey != 0)
            stVacate(pcontext, pdsmStoragePool, pcursor->pkey);

        *pcursid = NULLCURSID;
        pcursor->ixnum = 0;
        pcursor->blknum = 0;
        pcursor->pkey = 0;
        pcursor->kyareasz = 0;
        pcursor->cxusrnum = 0;
        PL_LOCK_CURSORS(&cursorsLatch)
        pcursor->flags = 0;                     /* mark cursor as free */
        PL_UNLK_CURSORS(&cursorsLatch)
    }

}  /* end cxDeactivateCursor */


/********************************************************/
/* PROGRAM: cxDeaCursors -- free all the CX cursors for a user
 *
 * NOTE: this has adverse side effects for multi threaded applications
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxDeaCursors(dsmContext_t *pcontext)
{
        cxcursor_t     *pcursor;
        cxCursorList_t *pcursList;
        int             cursorListIndex;
        CURSID          cursnum = 0;
        CURSID          tmp;

    if (pcursorList)
    {
        cursorListIndex = 1;
        for (pcursList = pcursorList;
             pcursList->pnextCursorList;
             pcursList = pcursList->pnextCursorList)
        {
            for (cursorListIndex = 0;
                 cursorListIndex < NUM_CURSORS;
                 cursorListIndex++)
            {
                pcursor = &(pcursList->cursorEntry[cursorListIndex]);

                if ( (pcursor->flags & CXINUSE)            &&
                     (pcursor->pdbcontext == pcontext->pdbcontext) &&
                     (pcursor->cxusrnum == pcontext->pusrctl->uc_usrnum) )
                {
                    tmp = cursnum; /* we need tmp to preserve cursnum */
                    cxDeactivateCursor(pcontext, &tmp);
                }
                cursnum++;
            }
        }
    }

}  /* end cxDeaCursors */


/*********************************************************************/
/* PROGRAM: cxLock -- Lock record or wait on task if index delete lock
 *
 *          Returns:  0 Record locked ok.
 *                    DSM_S_RQSTREJ, DSM_S_RQSTQUED, DSM_S_LKTBFULL, 
 *                    DSM_S_CTRLC,
 *                    DSM_S_TSKGONE - 
 *********************************************************************/
LOCALF
dsmStatus_t
cxLock(dsmContext_t  *pcontext,
       LONG           table,
       DBKEY          recordId,
       int            lockMode)
{
    dsmStatus_t   ret;
    lockId_t      lockId;
    
    if (pcontext->pdbcontext->arg1usr)
    {
        if (recordId < 0)
            return DSM_S_TSKGONE;               /* bug 92-07-17-029 */
        return 0;
    }

    if (recordId < 0)
    {
        /* check if the transaction is live, DONT wait yet */
        ret = lkCheckTrans(pcontext, -recordId, lockMode);

        /* if lkCheckTrans returns 0, then the transaction is either dead       */
        /* or myself.  In either case, the index entry should be        */
        /* considered as non-existent from my point of view             */
        if (ret==0)
            return DSM_S_TSKGONE;

        /* if lkCheckTrans returns NON-0, then it is either 
           DSM_S_LKTBFULL, DSM_S_RQSTQUED, DSM_S_RQSTREJ or DSM_S_CTRLC.  
           The caller will handle it    */
        return ret;
    }

    /* NOTE: when lockmd == LKNOLOCK, lkstat shouldn't be cleared,      */
    /* otherwise the ambiguity check phase of a unique FIND (which uses */
    /* nolock) will erase the lock state of the previous FIND.          */

    if ((lockMode & LKTYPE) == LKNOLOCK)        /* ignore void lock requests */
        return 0;

    /* positive dbkey, lock the recid, DONT wait yet, just return       
       the return code: DSM_S_LKTBFULL, DSM_S_RQSTQUED, DSM_S_RQSTREJ, 
       DSM_S_CTRLC                                                      */
    lockId.table = table;
    lockId.dbkey = recordId;
    ret = lklocky(pcontext, &lockId, lockMode, LK_RECLK, PNULL, NULL);

    return ret;
}

/* PROGRAM: cxFind -- search an index for a key
 *
 * RETURNS:
 *    FNDOK     - OK
 *    DSM_S_FNDFAIL     - The desired entry was not found
 *    DSM_S_FNDAMBIG    - There are more than 1 keys in the index which match
 *    DSM_S_IXINCKEY    - key passes is incomplete. Impossible?
 *    DSM_S_RQSTQUED    - The key is locked, this user has been queued
 *    DSM_S_RQSTREJ     - The key is locked, lockmode has the LKNOWAIT bit on
 *    DSM_S_CTRLC       - self-server - no more available cursors
 *    CURSORERR - server - no more available cursors
 */

int
cxFind(dsmContext_t *pcontext,
       dsmKey_t     *pkey_Base,     /* key to start to search at */
       dsmKey_t     *pkey_Limit,    /* key defines search upper limit  */
       CURSID       *pcursid,       /* if this has a cursid, use that
                                       cursor, else use a dummy cursor */
       LONG          table,         /* the table that the index
                                       is on.  So we can lock the record */
       dsmRecid_t   *pdbkey,      /* return the record's dbkey here */
       dsmMask_t     fndType,     /* Type of search to do:
                                        DSMPARTIAL not all components
                                                   of the key are given
                                        DSMUNIQUE  accept only conplete
                                                   key match
                                        DSMDBKEY   accept only a complete
                                                   match including recid  */

       dsmMask_t   fndmode,     /* The mode of the search:
                                        DSMFINDFIRST
                                        DSMFINDLAST           */

       dsmMask_t   lockmode)    /* bit mask may have 1 or more of:
                                        LKEXCL, LKSHARE, LKNOWAIT       */
{
        dsmKey_t    *pkey;          /* the key to search for */
        dsmKey_t    *plimit;        /* the key to check limit with */
        cxcursor_t  *pcursor;
        COUNT        nonuniq = 0;
        cxeap_t      eap;            /* Entry Access Parameters */
        cxeap_t     *peap = &eap;   /* to Entry Access Parameters */
        TEXT         tag[2];         /* must be at least 2 for ambig chk */
        GBOOL        chkambig = 0;   /* need to check for ambig. */
        COUNT        ixnum;
        int          donext;
        int          ret;
#ifdef VARIABLE_DBKEY
        TEXT        *pos;
#endif  /* VARIABLE_DBKEY */
        int          len;
        
#if CXDEBUG
    MSGD_CALLBACK(pcontext, (TEXT *)"%LcxFind: cursid %i", pcursid ? *pcursid : 0);
#endif

    if (fndmode == DSMFINDFIRST || fndType == DSMDBKEY )
        donext=1;
    else
        donext=0;

    plimit = pkey_Limit;
    pkey = pkey_Base;

    ixnum = pkey->index;
    if (pcursid != NULL)
    {
        pcursor = cxGetCursor(pcontext, pcursid, ixnum);
        pcursor->blknum = 0; /* it is start of cursor, clean old blk num */
        if (pcursor->flags & CXNOCURS)
            pcursor = 0; /* tmp cursor for query suspension, not needed here*/
    }
    else
        pcursor = 0; /* no need for a cursor */

#ifdef VST_ENABLED
    /* siphon off VSI requests */
    /* TODO: this needs macros and constants */
    if (SCIDX_IS_VSI(ixnum))
    {
      return cxFindVirtual(pcontext, pcursor, pdbkey, pkey_Base, pkey_Limit,
                         fndmode, lockmode);
    }
#endif  /* VST ENABLED */
    
    peap->pcursor = pcursor;
        
    /* prepare access parameters */
    peap->pkykey = pkey;
/*    peap->pinpkey = &pkey->keystr[ENTHDRSZ];  to new key */ 
    peap->pinpkey = &pkey->keystr[FULLKEYHDRSZ]; /* to new key */
    peap->inkeysz = pkey->keyLen;

#if CXDEBUGX && 0
    if (pkey) cxkeyprt(pkey->keyLen, &pkey->keystr[0]);
    if (plimit) cxkeyprt(plimit->keyLen, &plimit->keystr[0]);
#endif

    /* check if the caller passed an ok key */
    if ((COUNT)peap->inkeysz == -KEY_ILLEGAL)
        return(DSM_S_IXINCKEY);

    /* is this a nonunique situation? nonuniq is already on if the */
    /* parm key has any missing value components                   */
    if (fndType & DSMPARTIAL || !pkey->unique_index)
    {
        nonuniq=1;
    }


    if (peap->pkykey->unknown_comp)
    {
        /* an UNKNOWN component was found */
        nonuniq=1; /* UNKNOWN is not unique */
    }

    peap->datasz = 0;
    /*
     * for query phase 2 resolution - check whether an entry with a DBKEY
     * equal to '*pdbkey' exists.
     * WARNING: this code is meant only for QUERY BY WORD, and does not handle
     * unique indexes correctly: when a negative DBKEY is encoutered - for a
     * deleted record where the transaction hasn't completed yet - it returns
     * FNDFAIL rather than waiting for the transaction to complete.
     */
    if (fndType == DSMDBKEY)
    {
        /* dbkey in machine independant format */
#ifdef VARIABLE_DBKEY
        int minlen;
        if(peap->pkykey->unique_index)
            minlen = sizeof(dsmTrid_t);
        else
            minlen = 2;

        peap->inkeysz +=  gemDBKEYtoIdx( *pdbkey >> 8,  /* 3 high bytes only */
                                    minlen, &peap->pinpkey[peap->inkeysz] );
        peap->pinpkey[peap->inkeysz] = (TEXT)(*pdbkey); /* last byte to data */
#else
        slng(&peap->pinpkey[peap->inkeysz], *pdbkey);
        /* 3 most significant bytes to end of the key, last byte to data */
        peap->inkeysz += 3;
#endif  /* VARIABLE_DBKEY */

        peap->datasz = 1;
        peap->pdata = &peap->pinpkey[peap->inkeysz];
        peap->flags = CXGET+CXTAG;
        if (cxGet(pcontext, peap))
            return DSM_S_ENDLOOP;
        goto fndok;
    }
/*    pkey->keystr[1] = pkey->keystr[0] = (TEXT)peap->inkeysz; */
/*    pkey->keystr[2] = (TEXT)peap->datasz; */
/*    pkey->keystr[0] += peap->datasz + ENTHDRSZ;  total size in 1st byte */ 
    sct(&pkey->keystr[KS_OFFSET], peap->inkeysz); /* key size */
    sct(&pkey->keystr[IS_OFFSET], peap->datasz);  /* info size */
    sct(&pkey->keystr[TS_OFFSET], peap->inkeysz + peap->datasz + FULLKEYHDRSZ); 
                                                 /* total size */
 
    peap->flags = CXTAG+CXRETDATA;
    peap->pdata = &tag[0];
    peap->datamax = 2;

    /* prepare for output */
    if (!pcursor)
/*        peap->poutkey = &pkey->keystr[ENTHDRSZ]; */
        peap->poutkey = &pkey->keystr[FULLKEYHDRSZ];
    peap->mxoutkey = KEYSIZE;

    /* The or clause can I believe be removed since these are now the only
       possibilities  */
    if ((fndmode == DSMFINDFIRST || fndmode == DSMFINDLAST)
    && (!(fndType & DSMUNIQUE && pkey->ksubstr)))
    {
        if ( fndmode == DSMFINDFIRST )
            peap->flags |= CXFIRST;
        else
            peap->flags |= CXLAST;

        if (cxNextPrevious(pcontext, peap))
        {
            ret = DSM_S_ENDLOOP;
            goto fail;
        }
        if (peap->state == CXSucc) /* next tag has the same key as current */
        {
            peap->poutkey[peap->inkeysz] = tag[0];
        }
        else
        {
            peap->poutkey[peap->outkeysz] = tag[0];
            cxOutSize(peap); /* set sizes of output */
        }
    }
    else
    {
        /* this is find EQUAL with given key */
        peap->flags = CXFIRST+CXSUBSTR+CXTAG;
        if ( ( fndmode ==  DSMFINDLAST ) &&
             ( fndType == DSMUNIQUE ) )
        {
            /* if decending key, then subtract 5. This is because
                use of the HI key which is terminsted with 
                0xffff0ffff0 rather the 0 thus 5 additional bytes 
             */
            peap->substrsz = peap->inkeysz - 5;
            peap->flags &= ~CXFIRST;
            peap->flags |= CXLAST;
        }
        else
        {
            peap->substrsz = peap->inkeysz;
        }

        if (pkey->ksubstr)
            peap->substrsz--;/*do not include the NULL key terminator
                               since this is a partial key and the NULL 
                                would not match existing keys          */

        /* if the last component in the key is a null component, i.e., */
        /* a COMPSEPAR, NULLCOMPONENT sequence, ignore it. */
        if (peap->substrsz >= (TEXT)6 &&
            peap->pinpkey[peap->substrsz - 1] == NULLCOMPONENT &&
            peap->pinpkey[peap->substrsz - 2] == LOWESCAPE &&
                (peap->substrsz == 6 ||
                 peap->pinpkey[peap->substrsz - 3] == COMPSEPAR))
            peap->substrsz -= 2;

        if (cxNextPrevious(pcontext, peap))
        {
            ret = DSM_S_FNDFAIL;
            goto fail;
        }

        /* the key was found */
        peap->poutkey[peap->outkeysz] = tag[0];
        if (pcursor)
        {
            cxOutSize(peap);    /* set output sizes */
        }

        /* Check ambiguity only if
        nonuniq or if uniq but there no exact match (no NULL
        at position past the substr length) */
        if ((nonuniq)
        || (pkey->ksubstr && peap->poutkey[peap->substrsz]))
        {
            peap->inkeysz = peap->outkeysz;
            chkambig =1;
        }

        /* if not unique and substr and exact key found, check ambig
        only on exact key */
        if (nonuniq && pkey->ksubstr && !peap->poutkey[peap->substrsz])
        {
                peap->substrsz++; /* include the NULL key terminator */
        }

        if (fndType == DSMUNIQUE && pcursor != 0)
        {
            chkambig = 0; /* do not check ambiguity here, only in dbnxt */
            if ( fndmode & DSMFINDLAST ) /* DESCENDING INDEX */
            {
                /* decending indexes used in a begin will need to have the
                   last two characters check for exact match since the search
                   was done with an FFFF rather then an FFO1   */
                if ( (peap->poutkey[peap->substrsz + 1 ] == 0x01 )
                    && (peap->poutkey[peap->substrsz + 2 ] == 0 ) )
                {
                    /* found with exact match*/
                    /* BUM - assuming UCOUNT peap->substrsz < 256 */
                    pcursor->substrsz = (TEXT)peap->substrsz;
                }
                else
                    pcursor->substrsz = 0;
            }
            else
            { /* this must be an ASSCENDING INDEX */
                if (peap->poutkey[peap->substrsz])
                    pcursor->substrsz = 0;
                else
                {
                    /* found with exact match*/
                    /* BUM - assuming UCOUNT peap->substrsz < 256 */
                        pcursor->substrsz = (TEXT)peap->substrsz;
                }
            }
        }
    }
#ifdef VARIABLE_DBKEY
    pos = &peap->poutkey[peap->outkeysz];  /* past the key */
    len = *(pos - 1) & 0x0f;  /* length of the variable size DBKEY */
    gemDBKEYfromIdx( pos - len, pdbkey );  /* part of dbkey from the key */
    /*    *pdbkey <<= 8; */
    /*    *pdbkey += *pos;   the last byte of the dbkey */ 
#else
    len = 3;
    /* dbk starts 3 bytes from the end*/
    *pdbkey = xlng(&peap->poutkey[peap->outkeysz - len]);
#endif  /* VARIABLE_DBKEY */
    if (plimit) /* need to check limit? */
    {
        TEXT hiDbkeyByte = peap->poutkey[peap->outkeysz - len ];
        if((DBKEY)*pdbkey < 0)
        {
            /* A negative dbkey means this is a delete lock. Change the
               high byte so that it will compare to be less than
               SVCHIGHRANGE  BUG 99-04-14-012                     */
            peap->poutkey[peap->outkeysz - len ] = 1;
        }
        /* check high limit */
/*        ret = cxNewKeyCompare( peap->poutkey */
/*                             , peap->outkeysz */
/*                             , &plimit->keystr[ENTHDRSZ] */
/*                             , plimit->keyLen */
/*                             ); */
        ret = cxNewKeyCompare( peap->poutkey
                             , peap->outkeysz
                             , &plimit->keystr[FULLKEYHDRSZ]
                             , plimit->keyLen
                             );
        
        peap->poutkey[peap->outkeysz - len ] = hiDbkeyByte;  
        if ((ret > 0 && donext && !(pkey->ksubstr && ret == 2))
        ||  (ret < 0 && !donext))
        {
            *pdbkey = 0;
            ret = DSM_S_ENDLOOP;
            goto relfail;
        }
    }

  fndok:

           
    /* It is very important that the buffer is not released until the 
        record is locked.  This insures the record is as it was when 
        it was found in the index block.
    */
    ret = cxLock(pcontext,table, *pdbkey, lockmode);
        
 
        
    if (peap->bftorel)
        bmReleaseBuffer(pcontext, peap->bftorel);
        
    if ( ret == DSM_S_TSKGONE )
    {
        /* We are assuming that this function is being called from */
        /* dbfnd, and that we do no have a bracket upper limit.    */
        /* Adding a line in to check if the limit is 0, and if so, */
        /* return a FNDFAIL. See either richb, richt, or gss for   */
        /* details. BUG # 97-10-30-009                             */
        if (pkey_Limit == 0)
            return DSM_S_FNDFAIL;

        if (fndmode & DSMFINDLAST) {
            fndmode = DSMFINDPREV;
            pkey_Limit->area         = pkey_Base->area;
            pkey_Limit->root         = pkey_Base->root;
            pkey_Limit->unique_index = pkey_Base->unique_index;
            
            ret = cxNext(pcontext, pcursid, table, pdbkey, pkey_Limit,
                         pkey_Base, fndType, fndmode ,lockmode);
        } else {
            
            fndmode = DSMFINDNEXT;
            ret = cxNext(pcontext, pcursid, table, pdbkey, pkey_Base,
                         pkey_Limit, fndType, fndmode ,lockmode);
        }
    }
        
    return ret;

  relfail:
    if (peap->bftorel)
        bmReleaseBuffer(pcontext, peap->bftorel);
  fail:
    if (pcursor)
    {
        /* set the cursor with the searched key */
        pcursor->blknum = 0;
#ifdef VARIABLE_DBKEY
        cxSetCursor(pcontext, peap, (COUNT)(peap->inkeysz + FULLKEYHDRSZ + sizeof(DBKEY) + 2), 0);
        bufcop(peap->poutkey, peap->pinpkey, peap->inkeysz);
        pos = &peap->poutkey[peap->inkeysz];
        len = gemDBKEYtoIdx( (DBKEY)0, 2, pos );
        peap->outkeysz = peap->inkeysz + len;     /* account for the DBKEY data */
        peap->poutkey[peap->outkeysz] = 0;
#else
        cxSetCursor(pcontext, peap, (COUNT)(peap->inkeysz + ENTHDRSZ + 4), 0);
        bufcop(peap->poutkey, peap->pinpkey, peap->inkeysz);
        slng(&peap->poutkey[peap->inkeysz], (LONG)0);
        peap->outkeysz = peap->inkeysz + 3;     /* account for the DBKEY data */
#endif  /* VARIABLE_DBKEY */
        cxOutSize(peap);        /* set output sizes */
    }


        
    return ret;
}



/*****************************************************************************/
/* PROGRAM: cxFindGE -- find an index entry which is >= a given key + dbkey
 *                      value.
 *
 *      Find an entry in the index which is greater or equal
 *      to a given key + dbkey value.  The key is either the one saved in the
 *      cursor (in pcursor->pkey) if a cursor is supplied, or the key in the
 *      low range ditem otherwise.  The dbkey is pointed to by '*pdbkey'.
 *
 *      This procedure is similar to cxNext, except that it may return
 *      equal rather than greater, if such an entry is found.  The current
 *      cursor position is not used; the search always starts at the root.
 *
 *      On return, the cursor is positioned to the returned key and dbkey, and
 *      a following call to cxNext will return the next <key, dbkey> pair.
 *
 *
 * RETURNS: Return codes are the same as for cxNext().
 *          0       - OK
 *          TOOFAR  - gone past end of index or range
 */

int
cxFindGE( dsmContext_t *pcontext,
          CURSID       *pcursid,       /* the cursor to be advanced    */
          LONG          table,         /* table the index is on. */
          dsmRecid_t   *pdbkey,        /* return the dbkey of next entry here */
          dsmKey_t     *pdlo_key,      /* low range ditem  */
          dsmKey_t     *pdhi_key,      /* hi  range ditem  */
          int           lockmode)      /* bit mask may have 1 or more of:
                                   LKEXCL, LKSHARE, LKNOWAIT    */
{
    cxcursor_t *pcursor;
    int         keylen;
#ifdef VARIABLE_DBKEY
    TEXT       *pos;
    int         len;
    int         oldlen;
#endif  /* VARIABLE_DBKEY */

    pcursor = cxGetCursor(pcontext, pcursid, pdlo_key->index);
    if (pcursor->pkey == NULL)          /* this is a new cursor - generate a */
    {                                   /* key and place it in pcursor->pkey */

/*#ifdef NEW_MAX_KEY_SIZES_DISABLED */
        pcursor->pkey = (TEXT *)stRentd(pcontext, pdsmStoragePool, 2*KEYSIZE);
        /* NOTE: pcursor->kyareasz is a TEXT variable, and thus cannot be */
        /* set to more than 255.  However, a key size > 255 is illegal    */
        /* anyway.  The area here is set to more than that to guarantee   */
        /* that the key conversion routine - cxKeyToCx() - won't overflow.   */
        pcursor->kyareasz = KEYSIZE;

	/*#else */
/*        pcursor->kyareasz = 2 * pcontext->pdbcontext->pdbpub->maxkeysize; */
/*        pcursor->pkey = (TEXT *)stRentd( pcontext */
/*                                       , pdsmStoragePool */
/*                             , pcursor->kyareasz); */
        
/*#endif   NEW_MAX_KEY_SIZES_DISABLED */ 

        keylen = pdlo_key->keyLen;
        if (pdlo_key->unknown_comp )
        {
            if (keylen == KEY_ILLEGAL)
                return(DSM_S_IXINCKEY);
        }

        /* copy the new key into 'pcursor->pkey', and add room for the dbkey */
#ifdef VARIABLE_DBKEY
/*        bufcop(pcursor->pkey, pdlo_key->keystr, keylen + ENTHDRSZ); */
        bufcop( pcursor->pkey + FULLKEYHDRSZ , 
                               pdlo_key->keystr + FULLKEYHDRSZ , keylen);
        /* setup the key of the cursor with the given dbkey minus 1*/
/*        pos = &pcursor->pkey[keylen + ENTHDRSZ];  to start of DBKEY */ 
        pos = &pcursor->pkey[keylen + FULLKEYHDRSZ]; /* to start of DBKEY */
        len = gemDBKEYtoIdx( (*pdbkey -1) >> 8, 2, pos );
        pos[len] = (TEXT)(*pdbkey -1);   /* the IS byte */
/*        pcursor->pkey[0] = keylen + ENTHDRSZ + len + 1; */
/*        pcursor->pkey[1] = keylen + len;   to account for the dbkey */ 
/*        pcursor->pkey[2] = 1; */
        sct(&pcursor->pkey[TS_OFFSET], keylen + FULLKEYHDRSZ + len + 1); /* Total Size */
        sct(&pcursor->pkey[KS_OFFSET], keylen + len); /* 3 to account for the dbkey */
        sct(&pcursor->pkey[IS_OFFSET], 1);
    }
    else
    {
        /* replace the dbkey in the key of the cursor with the 
	   given dbkey minus 1*/
        pos = &pcursor->pkey[pcursor->pkey[1]  + ENTHDRSZ];                    
        oldlen = *(pos -1) & 0x0f;
        pos -= oldlen; /* to start of DBKEY */
        len = gemDBKEYtoIdx( (*pdbkey -1) >> 8, 2, pos );
        pos[len] = (TEXT)(*pdbkey -1);   /* the IS byte */
        pcursor->pkey[0] += len - oldlen;
        pcursor->pkey[1] += len - oldlen;  /* to account for the dbkey */
        pcursor->pkey[2] = 1;
    }
#else
/*        pcursor->pkey[0] = keylen + ENTHDRSZ + 4; */
/*        pcursor->pkey[1] = keylen + 3;   to account for the dbkey */ 
/*        pcursor->pkey[2] = 1; */
        sct(&pcursor->pkey[TS_OFFSET], keylen + FULLKEYHDRSZ + 4);
                                                          /* Total Size */
        sct(&pcursor->pkey[KS_OFFSET], keylen + 3);
                                              /* 3 to account for the dbkey */
        sct(&pcursor->pkey[IS_OFFSET], 1;

        bufcop( pcursor->pkey + FULLKEYHDRSZ , 
                               pdlo_key->keystr + FULLKEYHDRSZ , keylen);
    }

    /* replace the dbkey in the key of the cursor with the given 
       dbkey minus 1*/
/*    slng(&pcursor->pkey[pcursor->pkey[1]  + ENTHDRSZ - 3], *pdbkey -1); */
    slng(&pcursor->pkey[xct(&pcursor->pkey[KS_OFFSET])  + FULLKEYHDRSZ - 3]
                         , *pdbkey -1);
#endif  /* VARIABLE_DBKEY */

    /* invalidate the cursor so that the search will start at the root */
    pcursor->blknum = 0;

    return cxNext(pcontext, pcursid, table, pdbkey,
                  pdlo_key, pdhi_key, DSMGETGE, DSMFINDNEXT, lockmode);
}

/***********************************************************************/
/* PROGRAM: cxForward -- backup a cursor 1 step, used after lock conflict
 *                      so that CXPREV will get the current key,
 *                      the CXFORWD flag will cause a correction
 *                      if the next operation is CXNEXT
 *
 * RETURNS: DSMVOID
 */

DSMVOID
cxForward(
        dsmContext_t *pcontext _UNUSED_,
        CURSID        cursid)
{
        cxcursor_t   *pcursor;
        TEXT         *pos;
        int           len;
#ifdef VARIABLE_DBKEY
        DBKEY         dbkey;
        int           newlen;
#endif  /* VARIABLE_DBKEY */

#if CXDEBUG
    /* this can't be used until context is pushed through all of
       the cx layer */
    MSGD_CALLBACK(pcontext, (TEXT *)"%LcxForward, cursid %i", cursid);
#endif

    pcursor = cxFindCursor(cursid);

    /* invalidate the cursor so that the search will start at the root */
    pcursor->blknum = 0;

    /* add 1 to the dbkey in the key of the cursor */
#ifdef VARIABLE_DBKEY
     len = pcursor->pkey[xct(&pcursor->pkey[KS_OFFSET]) + FULLKEYHDRSZ-1] 
       & 0xf;
#else
     len = 3;
#endif  /* VARIABLE_DBKEY */
/*     pos = &pcursor->pkey[pcursor->pkey[1]  + ENTHDRSZ - len];  to start of dbkey */ 
     pos = &pcursor->pkey[xct(&pcursor->pkey[KS_OFFSET]) +FULLKEYHDRSZ - len]; /* to dbkey */

    if (pcursor->unique_index)
        /* by replacing the dbkey by the largest possible dbkey, this insures 
           that the key will find the same key if a prev is done.  Since this 
           is a unique index, there can not be two identicals keys with different 
           recids 
        */
#ifdef VARIABLE_DBKEY
    {
        newlen = gemDBKEYtoIdx( (DBKEY)-1, len, pos );
        pos[newlen] = 0xff;   /* the is byte = 0xff */
    }
#else
        slng(pos, (DBKEY)-1); /* highest value 0xfffffffffffff */  
#endif  /* VARIABLE_DBKEY */

    else
    {
#ifdef VARIABLE_DBKEY
        gemDBKEYfromIdx( pos, &dbkey); /* get the leading part of the dbkey  */
        dbkey++;
        newlen = gemDBKEYtoIdx( dbkey >> 8, 2, pos );
        pos[newlen] = (TEXT)dbkey;   /* the is byte */
#else
        slng(pos, xlng(pos) +1);
#endif  /* VARIABLE_DBKEY */
        pcursor->flags |= CXFORWD; /* to cause -1 if CXNEXT is used next */
    }
#ifdef VARIABLE_DBKEY
    if (newlen != len )
    {
        /* the size of the dbkey has been changed */
/*        pcursor->pkey[1] += newlen - len; */
/*        pcursor->pkey[0] += newlen - len; */
        pos = pcursor->pkey + TS_OFFSET;
        sct (pos, xct(pos) + newlen - len); /* update total size */
        pos = pcursor->pkey + KS_OFFSET;
        sct (pos, xct(pos) + newlen - len); /* update key size */
    }
#endif  /* VARIABLE_DBKEY */
}

/****************************************************************************/
/* PROGRAM: cxFreeCursors -- Free's the anchor (pcursorList) for all the cursors
 *                           of this process!
 *
 * NOTE: doing this in a multi-threaded environment while other threads 
 *       are still active for this process WILL have adverse effects.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxFreeCursors(dsmContext_t *pcontext)
{
    cxCursorList_t *pcursList;
    cxCursorList_t *pcursListHold;

    PL_LOCK_CURSORS(&cursorsLatch)

    for (pcursList = pcursorList;
         pcursList;
         pcursList = pcursListHold)
    {
        pcursListHold = pcursList->pnextCursorList;
        stVacate(pcontext, pdsmStoragePool, (TEXT *)pcursList);
    }
    pcursorList = (cxCursorList_t *)NULL;

    PL_UNLK_CURSORS(&cursorsLatch)

} /* end cxFreeCursors */


/*************************************************************************/
/* PROGRAM: cxGetAuxCursor - get an auxiliary cursor to check ambiguity for the
 *          cursor in *pcursid.
 *
 *  Allocates a new cursor and copies the given cursor to it.
 *  Allocates a new key save area for the new cursor, and copies the passed
 *      cursor's key into it, so as not to change of the key in the original
 *      cursor.
 *
 * RETURNS:
 *    The new cursorid in *pcursid.  The caller must save the old *pcursid
 *    before this call.
 */
DSMVOID
cxGetAuxCursor(
        dsmContext_t    *pcontext,
        CURSID          *pcursid)
{
     cxcursor_t   *pcursor;
     cxcursor_t   *pauxcur;
     CURSID        cursid;

    cursid = *pcursid;                                  /* old cursor */
    *pcursid = NULLCURSID;
    pauxcur = cxGetCursor(pcontext, pcursid,0);         /* new cursor */

    pcursor = cxFindCursor(cursid);                     /* old cursor */
    bufcop(pauxcur, pcursor, sizeof(cxcursor_t));         /* copy old to new */
    pauxcur->pkey = stRent(pcontext, pdsmStoragePool, pcursor->kyareasz);
    bufcop(pauxcur->pkey, pcursor->pkey, pcursor->kyareasz);
    pauxcur->flags |= CXAUXCURS;        /* mark it as an auxiliary cursor */

}  /* end cxGetAuxCursor */


/***************************************************************************/
/* PROGRAM: cxGetCursor - get a new index cursor, or find an existing one.
 *  If the caller already has a cursor, reuses it.  Otherwise, iscans the
 *  cursor table to find an unused one and returns its CURSID to the caller.
 *
 *  RETURNS:    pointer to the cursor
 */
cxcursor_t *
cxGetCursor (
        dsmContext_t    *pcontext,
        CURSID          *pcursid,
        COUNT            ixnum)
{
     cxcursor_t     *pcursor=0;
     cxCursorList_t *pcursList;
     CURSID          cursnum;
     int             notFound;
     int             cursorListIndex;

#if CXDEBUG
    MSGD_CALLBACK(pcontext, 
           (TEXT *)"%LcxGetcursor: input cursid %i", pcursid ? *pcursid : 0);
#endif

    /* can we reuse the callers cursor ? */
    if (pcursid != NULL && *pcursid != NULLCURSID)
    {
        pcursor = cxFindCursor(*pcursid);

        FASSERT_CB(pcontext, pcursor->flags & CXINUSE,
                        "cxGetCursor: trying to use a free cursor");
        FASSERT_CB(pcontext, pcursor->cxusrnum == pcontext->pusrctl->uc_usrnum,
                        "cxGetCursor: trying to use someone else's cursor");
        FASSERT_CB(pcontext, pcursor->pdbcontext == pcontext->pdbcontext,
                        "cxGetCursor: trying to a cursor with a wrong pdbcontext");
    }
    else
    {   /* no, we must scan to find an available cursor */
        PL_LOCK_CURSORS(&cursorsLatch)
        if (!pcursorList)
        {
            /* start with NUM_CURSORS cursors, add more later if needed */
            curscnt = NUM_CURSORS -1; /* only NUM_CURSORS-1 available,
                                       * cursor # 0 is never used */
            pcursorList = (cxCursorList_t *)stRent(pcontext, pdsmStoragePool,
                        sizeof(cxCursorList_t));
        }

        notFound = 1;
        cursnum  = 0;
        /* search for an available cursor */
        for (pcursList = pcursorList;
             pcursList && notFound;
             pcursList = pcursList->pnextCursorList)
        {
            /* traverse the current list of cursors
             * (1st cursor slot of 1st cursor list is skipped)
             */
            for (cursorListIndex = ((pcursList == pcursorList) ? 1 : 0);
                 (cursorListIndex < NUM_CURSORS) && notFound;
                 cursorListIndex++)
            {
                 pcursor = &(pcursList->cursorEntry[cursorListIndex]);
                 if (!pcursor->flags) /* EMPTY */
                     notFound = 0;
            }
            cursnum += cursorListIndex - ((pcursList == pcursorList) ? 1 : 0);
        }

        if (notFound)  /* no cursors available - allocate NUM_CURSORS more */
        {
            /* Find/update next ptr in LAST cursor list to new cursor list */
 
            /* find last list of cursors */
            for (pcursList = pcursorList;
                 pcursList->pnextCursorList;
                 pcursList = pcursList->pnextCursorList);
 
            /* allocate and chain in new list of cursors */
            pcursList->pnextCursorList = (cxCursorList_t*)stRent(pcontext,
                            pdsmStoragePool, sizeof(cxCursorList_t)); 
            curscnt += NUM_CURSORS;
 
            /* Get the requested cursor out of the newly allocated list */
            pcursor = &(pcursList->pnextCursorList->cursorEntry[0]);
            cursnum++;
        }
 
#if CXDEBUG
        MSGD_CALLBACK(pcontext, (TEXT *)"%LcxGetCursor: new cursid %i", cursnum);
#endif
        *pcursid = cursnum;     /* assign the new cursor to the caller */
        pcursor->flags |= CXINUSE;      /* mark the cursor as used */
        pcursor->ixnum = ixnum;
        pcursor->blknum = 0;            /* mark the cursor as new */
        pcursor->cxusrnum = pcontext->pusrctl->uc_usrnum;
        pcursor->pdbcontext = pcontext->pdbcontext;
        PL_UNLK_CURSORS(&cursorsLatch)
    }

    return(pcursor);

}  /* end cxGetCursor */


/* PROGRAM: cxFindCursor - given a cursid, return the pointer to the cursor
 *
 * RETURNS: pointer to the cursor or PNULL on error
 */
LOCALF cxcursor_t *
cxFindCursor(
        CURSID cursid)
{
    int             cursorListIndex;
    int             cursorList;
    int             i;
    cxCursorList_t *pcursList = 0;
    cxcursor_t     *pcursor;
 
#if 1  /* do we really need cursid validation here? */
    if ( (cursid == NULLCURSID) || (cursid > curscnt) )
        return (cxcursor_t *)NULL;
#endif
 
    /* find list of cursors for this cursid */
    cursorList = cursid / NUM_CURSORS;
    pcursList = pcursorList;
    for (i=0; i < cursorList; i++)
         pcursList = pcursList->pnextCursorList;
 
    /* Now determine the entry in this list for this cursid */
    cursorListIndex = cursid % NUM_CURSORS;
    pcursor = &(pcursList->cursorEntry[cursorListIndex]);
 
    return pcursor;
 
}  /* end cxFindCursor */


/***************************************************************/
/* PROGRAM: cxNext -- advance to the next entry in an index
 *
 * This procedure does range checks and also checks flags which
 * indicate the advance has already been done.
 *
 * RETURNS:
 *      0       - OK
 *      TOOFAR  - gone past end of index or range
*/

int
cxNext( dsmContext_t *pcontext,
      CURSID    *pcursid,       /* the cursor to be advanced */
      LONG       table,         /*  table the index is on. */
      dsmRecid_t *pdbkey,       /* return the dbkey of next entry here */
      dsmKey_t  *pdlo_key,      /* low range ditem  */
      dsmKey_t  *pdhi_key,      /* hi  range ditem  */
      dsmMask_t  fndType,       /* DSMGETTAG */
      dsmMask_t  op,            /* DSMFINDNEXT or DSMFINDPREV */
      int        lockmode)      /* bit mask may have 1 or more of:
                                        LKEXCL, LKSHARE, LKNOWAIT */

{
    cxeap_t     eap;         /* Entry Access Parameters */
    cxeap_t    *peap= &eap;  /* to Entry Access Parameters */
    cxcursor_t *pcursor;
    dsmRecid_t  inpdbkey;
    TEXT        tag;
    COUNT       ixnum;
    int		returnOnTSKGONE = 0;
    int         ret;
    TEXT       hiDbkeyByte;
    lockId_t    lockId;
#ifdef VARIABLE_DBKEY
        TEXT        *pos;
#endif  /* VARIABLE_DBKEY */
        int          len;
   
    
    TRACE1_CB(pcontext, "cxNext *pcursid=%d", *pcursid)
#if CXDEBUG
    MSGD_CALLBACK(pcontext, 
                  (TEXT *)"%LcxNext: cursid %i", pcursid ? *pcursid : 0);
#endif

    if ((!pcursid) || (*pcursid == NULLCURSID))
    {
        *pdbkey = 0;
        return DSM_S_ENDLOOP;
    }

    if (fndType == DSMGETGE)
        inpdbkey = *pdbkey; /* save dbkey for the case of CXGREQ */
    else
        inpdbkey =0;

    if ( op == DSMFINDNEXT )
    {
        peap->flags = CXNEXT | CXTAG;
        op = CXNEXT;    
    }
    else if ( op == DSMFINDPREV )
    {
        peap->flags = CXPREV | CXTAG;
        op = CXPREV;    
    }
    else if ( op == DSMFINDNEXTANDTSKGONE )
    {
	/* This is to return if the next entry is a dead delete place holder */
	returnOnTSKGONE = 1;
        peap->flags = CXNEXT | CXTAG;
        op = CXNEXT;    
    }
    else MSGD_CALLBACK(pcontext, (TEXT *)"%Gop == %d", op);
    
  next:
    
    ixnum = pdlo_key->index;
    pcursor = cxGetCursor(pcontext, pcursid, ixnum);
    
#ifdef VST_ENABLED
    /* siphon off VSI requests */
    /* TODO: this needs macros and constants */
    if (SCIDX_IS_VSI(ixnum))
    {
       return cxNextVirtual(pcontext, pcursor, pdbkey, pdlo_key,
                            pdhi_key, op, lockmode);
    }
#endif  /* VST ENABLED */

    if ((pcursor->flags & (CXBACK | CXFORWD)))
    {
        if ((pcursor->flags & CXBACK) && (op == CXPREV))
            cxForward(pcontext, *pcursid); /* restore to current key */
        if  ((pcursor->flags & CXFORWD) && (op == CXNEXT))
            cxBackUp(pcontext, *pcursid); /* restore to current key */
    }

    /* get the block pointed by the cursor */
    peap->blknum = pcursor->blknum;    /* blknum of strating level block */

    /* pkykey used to get area/root info              */
    peap->pkykey = pdlo_key;
    /* set the search key to be the saved cursor key */
/*    peap->pinpkey = pcursor->pkey + ENTHDRSZ; */
/*    peap->inkeysz = pcursor->pkey[1]; */
    peap->pinpkey = pcursor->pkey + FULLKEYHDRSZ;
    peap->inkeysz = xct(&pcursor->pkey[KS_OFFSET]);

    tag = peap->pinpkey[peap->inkeysz];
    peap->pdata = &tag;
    peap->datamax = 1;

    peap->mxoutkey = KEYSIZE;

    peap->pcursor = pcursor;
            
    peap->flags = op | CXTAG;
    if (cxNextPrevious(pcontext, peap))
    {
        *pdbkey = 0;    /* for benefit of caller */
        /* cursor failed to advance, it points to previous key */
        if (op == CXNEXT)  /* Get next */
            cxForward(pcontext, *pcursid); /* in case dbprv is used later */
        else
            cxBackUp(pcontext, *pcursid);   /* in case dbnxt is used later */
        return DSM_S_ENDLOOP;
    }
    peap->poutkey[peap->outkeysz] = tag;

#ifdef VARIABLE_DBKEY
    pos = &peap->poutkey[peap->outkeysz];  /* past the key */
    len = *(pos - 1) & 0x0f;  /* length of the variable size DBKEY */
    gemDBKEYfromIdx( pos - len, pdbkey );  /* part of dbkey from the key */
    /*    *pdbkey <<= 8; */
    /*    *pdbkey += *pos;   the last byte of the dbkey */ 
#else
    len = 3;/* dbk starts 3 bytes  from end of key */
    *pdbkey = xlng(&peap->poutkey[peap->outkeysz - len]); 
#endif  /* VARIABLE_DBKEY */

    cxOutSize(peap);    /* set output sizes */

    /* check high limit */

    hiDbkeyByte = peap->poutkey[peap->outkeysz - len ];
    if((DBKEY)*pdbkey < 0)
    {
        /* A negative dbkey means this is a delete lock. Change the
           high byte so that it will compare to be less than
           SVCHIGHRANGE  BUG 99-04-14-012                     */
        peap->poutkey[peap->outkeysz - len ] = 1;
    }
    ret = cxNewKeyCompare(peap->poutkey, peap->outkeysz,
/*                          &pdhi_key->keystr[3], pdhi_key->keyLen); */
                          &pdhi_key->keystr[FULLKEYHDRSZ], pdhi_key->keyLen);

    
#if CXDEBUGX
    peap->poutkey[peap->outkeysz - len ] = hiDbkeyByte;
/*    cxkeyprt(peap->outkeysz, &peap->poutkey[0]); */
/*    if (pdhi_key) cxkeyprt(pdhi_key->keyLen, &pdhi_key->keystr[3]); */
    cxkeyprt(peap->outkeysz, peap->poutkey);
    if (pdhi_key) 
        cxkeyprt(pdhi_key->keyLen, &pdhi_key->keystr[FULLKEYHDRSZ]);

    if (ret < 0)
        MSGD(pcontext, "%Lh-1");
    else
    if (ret > 0)
        MSGD(pcontext, "%Lh+1");
    else
        MSGD(pcontext, "%Lh0");
    if ((LONG)*pdbkey < 0 )  peap->poutkey[peap->outkeysz - 3 ] = 1;
#endif

    if (ret > 0 && !(pdlo_key->ksubstr && ret == 2))
    {
        /* key higher than high limit */
        *pdbkey = 0;
        if (op == CXNEXT)
        {
            ret = DSM_S_ENDLOOP;
            goto rslixbuf;
        }
        if (op == CXPREV)
        {
            ret = AHEADORNG;
            goto rslixbuf;
        }
    }

    /* check low limit */
/*    ret = cxNewKeyCompare(peap->poutkey, peap->outkeysz, */
/*        &pdlo_key->keystr[3], pdlo_key->keyLen); */
    ret = cxNewKeyCompare( peap->poutkey, peap->outkeysz,
                         &pdlo_key->keystr[FULLKEYHDRSZ], pdlo_key->keyLen);

#if CXDEBUGX
    peap->poutkey[peap->outkeysz - len ] = hiDbkeyByte;
/*    cxkeyprt(peap->outkeysz, &peap->poutkey[0]); */
/*    if (pdlo_key) cxkeyprt(pdlo_key->keyLen, &pdlo_key->keystr[3]); */
    cxkeyprt(peap->outkeysz, peap->poutkey);
    if (pdlo_key) 
        cxkeyprt(pdlo_key->keyLen, &pdlo_key->keystr[FULLKEYHDRSZ]);

    if (ret < 0)
        MSGD(pcontext, "%Ll-1");
    else
    if (ret > 0)
        MSGD(pcontext, "%Ll+1");
    else
        MSGD(pcontext, "%Ll0");
     if( (DBKEY)*pdbkey < 0 ) peap->poutkey[peap->outkeysz - len ] = 1;
#endif
    if (ret < 0)
    {
        /* key lower than low limit */
        *pdbkey = 0;
        if (op == CXNEXT)
        {
            ret = AHEADORNG;
            goto rslixbuf;
        }
        if (op == CXPREV)
        {
            ret = DSM_S_ENDLOOP;
            goto rslixbuf;
        }
    }

    /* check whether this is a find greater/equal dbkey request and whether */
    /* there was a locked input dbkey (inpdbkey != 0) */
    if (fndType == DSMGETGE && inpdbkey != 0)
    {
        if (*pdbkey == inpdbkey)        /* found the same dbkey, which is */
        {                               /* already locked; no need to lock */
            ret = 0;                    /* it again */
            goto rslixbuf;
        }
        lockId.table = (LONG)table;
        lockId.dbkey = inpdbkey; /* inpdbkey before locking *pdbkey */
        if ((lockmode & DSM_LK_TYPE) != DSM_LK_NOLOCK)
        {
            lkrestore(pcontext, &lockId, (dsmMask_t)lockmode);
        }
    }

    ret = cxLock(pcontext,table,*pdbkey, lockmode);
   
  rslixbuf:
    peap->poutkey[peap->outkeysz - len ] = hiDbkeyByte;
    
    if (peap->bftorel)
        bmReleaseBuffer(pcontext, peap->bftorel);  /* release the B-tree
                                                      buffer lock */
    if (ret == DSM_S_ENDLOOP)
    {
        /* cursor already passed the limit, should be returned to the gap */
        if (op == CXNEXT )  /* Get next */
            /* in case dbnxt is used later */
            cxBackUp(pcontext, *pcursid);
        else
            /* in case dbprv is used later */
            cxForward(pcontext, *pcursid);
    }
    else if( ret == DSM_S_TSKGONE && !returnOnTSKGONE)
    {
        goto next;
    }
   
    return ret;
}

/********************************************************************/
/* PROGRAM: cxOutSize -- set sizes of output element (cursor or ditem)
 *
 * RETURNS: DSMVOID
 */
LOCALF
DSMVOID
cxOutSize(cxeap_t *peap)  /* to Entry Access Parameters */
{
/*    *(peap->poutkey -1) = 1; */
/*    *(peap->poutkey -2) = (TEXT)peap->outkeysz; */
/*    *(peap->poutkey -3) = peap->outkeysz + 1 + ENTHDRSZ; */
    sct(peap->poutkey - FULLKEYHDRSZ + IS_OFFSET, 1);
    sct(peap->poutkey - FULLKEYHDRSZ + KS_OFFSET, peap->outkeysz);
    sct(peap->poutkey - FULLKEYHDRSZ + TS_OFFSET, peap->outkeysz + 1 +FULLKEYHDRSZ);
}


/**********************************************************************/
/* PROGRAM: cxSetCursor  -  set up the cursor and prepare to save the key
 *      allocates an area to save the key in
 *      only if such area does not exist, or
 *      the area is too small for the key
 *
 * RETURNS: 0
 */
int
cxSetCursor(
        dsmContext_t *pcontext,
        cxeap_t      *peap,       /* pointer to the input EAP */
        COUNT         entlen,     /* length of the new index entry */
        int           cs)         /* compressed size needed from old key */
{
    cxcursor_t  *pcursor;       /* cursor pointer */
    TEXT        *poldkey;       /* to old key */

#if CXDEBUG
    MSGD_CALLBACK(pcontext, (TEXT *)"%LcxSetCursor");
#endif
/*    FASSERT_CB(pcontext, entlen <= 255, "entlen > 255 in cxSetCursor"); */
    FASSERT_CB(pcontext, entlen <= MAXKEYSZ + 25, "entlen > too large in cxSetCursor");

    pcursor = peap->pcursor;

    if(pcursor->kyareasz < entlen)        /* the key needs more space */
    {
        poldkey = pcursor->pkey;                /* allocate a new area 16 */
        pcursor->kyareasz = entlen + 16;        /* bytes longer than needed */
        pcursor->pkey = (TEXT *)stRentd( pcontext
                                       , pdsmStoragePool
                                       , pcursor->kyareasz
                                       );

        /* if an old key exists, copy its compressed part and free its area */
        if(poldkey)
        {
/*            bufcop(pcursor->pkey, poldkey, cs + ENTHDRSZ); */
            bufcop(pcursor->pkey, poldkey, cs + FULLKEYHDRSZ);
            stVacate(pcontext, pdsmStoragePool, poldkey);
            /* if the input key pointed into pcursor->pkey, reset it */
/*            if ( (peap->pinpkey == poldkey + ENTHDRSZ)) */
/*                peap->pinpkey = pcursor->pkey + ENTHDRSZ; */
            if ( (peap->pinpkey == poldkey + FULLKEYHDRSZ))
                peap->pinpkey = pcursor->pkey + FULLKEYHDRSZ;
        }
    }

/*    peap->poutkey = pcursor->pkey + ENTHDRSZ; */
    peap->poutkey = pcursor->pkey + FULLKEYHDRSZ;

    return(0);
}

#if 0
/* PROGRAM: cxCursorDump - dump all the active cursors to the .lg file
 *
 * RETURNS: 0
 */
int
cxCursorDump(dsmContext_t *pcontext)
{
    int inuse = 0;
    int total = 0;
    int i;
    cxCursorList_t *pcursList = 0;

    PL_LOCK_CURSORS(&cursorsLatch)
    for (pcursList = pcursorList; pcursList;
         pcursList = pcursList->pnextCursorList)
    {
        for (i = 0; i < NUM_CURSORS; i++)
        {
            if (pcursList->cursorEntry[i].flags)
            {
                inuse++;
                MSGD_CALLBACK(pcontext, 
       "Cursor in use in list %d, entry %d, owner %d, flags %d",total, i, pcursList->cursorEntry[i].cxusrnum);
            }
        }
        total++;
    }
    MSGD_CALLBACK(pcontext, "Total cursors = %d, inuse = %d", total * NUM_CURSORS, inuse);
    PL_UNLK_CURSORS(&cursorsLatch)
    return 0;
}
#endif

#if CXDEBUGX
/****************************************************************************/
/* PROGRAM: cxkeyprt  -- Prints out a key for debugging purposes         
 *
 * RETURNS: DSMVOID
*/
LOCALF
DSMVOID
cxkeyprt(int l, TEXT    *p)
{
    MSGD_CALLBACK(pcontext, (TEXT *)"%Llen=%d %x %x %x %x %x %x %x %x %x %x %x %x",
         l, p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11]);
    
}
#endif
