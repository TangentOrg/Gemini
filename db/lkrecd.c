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

/*****************************************************************/
/**  Server-Less (shared memory) version!!!!!!!!!!!             **/
/**                                                             **/
/**     Record lock manager routines. These are dummy           **/
/**     stubs in the single user version, and are resident      **/
/**     in the server process in the multi user version.        **/
/**                                                             **/
/**     The record lock manager assumes that a certain          **/
/**     amount of bookkeeping is performed by its callers.      **/
/**     Specifically, a user may only hold a given record       **/
/**     in one of three states:                                 **/
/**                                                             **/
/**          1. No lock. However, the lock manager may          **/
/**             invisibly hold the record in limbo for          **/
/**             transaction management purposes.                **/
/**                                                             **/
/**          2. Share lock. The lock manager may actually       **/
/**             hold the record in exclusive status, again      **/
/**             for purposes of transaction management.         **/
/**                                                             **/
/**          3. Exclusive lock.                                 **/
/**                                                             **/
/**     The record locking table is the primary data            **/
/**     structure used by the record locking routines.          **/
/**     Each entry in the record locking table contains all     **/
/**     of the information pertinent to one dbkey/user pair.    **/
/**     A user is identified by the pointer to his or her       **/
/**     usrctl structure. Records are identified by their       **/
/**     dbkey.                                                  **/
/**                                                             **/
/**     The record locking table is organized as a set of       **/
/**     chains. There is one chain for all free entries,        **/
/**     anchored from the external variable lk_pfree. This      **/
/**     free chain is maintained in no particular order         **/
/**     and with all flags, etc intact from the last use.       **/
/**                                                             **/
/**     There are LKACOUNT chains of active entries, each       **/
/**     anchored by an entry in the lk_anchor array.            **/
/**     The last entry in this array anchors a special          **/
/**     purpose chain used to hold all of the locks which       **/
/**     have been purged by lkpurge, but are still waiting      **/
/**     for lkrels or lktend calls. Entries in this chain       **/
/**     are processed by lkrels, lktend, and lkrend, but are    **/
/**     not processed by lklock.                                **/
/**                                                             **/
/**     The correct lk_anchor entry for any given dbkey         **/
/**     is found by anding the dbkey with the mask LKHMASK,     **/
/**     the result being the lka_anchor index. Note that        **/
/**     LKHMASK will not produce the purge chain value,         **/
/**     nor will it produce the index and the rec-get chains    **/
/**                             Within each of the              **/
/**     non-purged active chains, the lck entries are           **/
/**     maintained in dbkey order. The purged chain is          **/
/**     maintained in FIFO order for convenience.               **/
/**                                                             **/
/*****************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"

#include "dbpub.h"  /* for LKTBFULL*/
#include "dbcontext.h"
#include "dbmsg.h"
#include "dsmpub.h"
#include "latpub.h"
#include "lkprv.h"
#include "lkpub.h"
#include "lkschma.h"  /* for EXCLCTR */
#include "mtmsg.h"
#include "mumsg.h"

#include "shmpub.h"
#include "stmpub.h"
#include "stmprv.h"
#include "svmsg.h"
#include "tmmgr.h"
#include "umlkwait.h"
#include "usrctl.h"
#include "utcpub.h"
#include "scasa.h"
#include <time.h>

/* LOCAL Function Prototypes */
LOCALF struct lck * lkgetlocks  (dsmContext_t *pcontext, int amount);
LOCALF DSMVOID lkwkup              (dsmContext_t *pcontext, struct lck *first,
                                 LONG table, DBKEY dbkey, int chain);
LOCALF int lktbresy             (dsmContext_t *pcontext, usrctl_t *pusr);
LOCALF struct lck * lkchecky    (dsmContext_t *pcontext, lockId_t *plockId);
LOCALF DSMVOID lkfree              (dsmContext_t *pcontext, struct lck *prev,
                                 struct lck * cur, int chain);
LOCALF struct lck *
lkTableLockGet(dsmContext_t *pcontext, LONG table, lck_t **pwhere, int latch);
 
LOCALF DSMVOID lkTableLocksFree(dsmContext_t *pcontext);
 

LOCAL  GBOOL lockCompare[5][5] =
{         /* IS ,IX ,S  , SIX ,X */
    /* IS */{0  ,0  ,0  ,0    ,1},
    /* IX */{0  ,0  ,1  ,1    ,1},
    /* S  */{0  ,1  ,0  ,1    ,1},
    /* SIX*/{0  ,1  ,1  ,1    ,1},
    /* X  */{1  ,1  ,1  ,1    ,1}
};
    
/* PROGRAM: lkLockConflict -- 
 *                            
 *                            
 *                               
 *
 * RETURNS: 0 - if locks do NOT conflict
 *          1 - if locks do conflict
 */
LOCALF
int
lkLockConflict(int request, int current )
{
    if (current & LKEXPIRED)
        return 0;

    request &= LKTYPE;
    current &= LKTYPE;

    if( request == 0 || current == 0)
        return 0;
    
    if( request == LKIS )
        request = 0;
    else if (request == LKIX)
        request = 1;
    else if (request == LKSHARE )
        request = 2;
    else if (request == LKSIX)
        request = 3;
    else
        request = 4;

    if( current == LKIS )
        current = 0;
    else if (current == LKIX)
        current = 1;
    else if (current == LKSHARE )
        current = 2;
    else if (current == LKSIX)
        current = 3;
    else
        current = 4;

    return (int)lockCompare[request][current];
}

/* PROGRAM: lkCacheLock - Add pointer to the table lock in the user
   control if there is room for it.
   */
LOCALF
DSMVOID
lkCacheLock(dsmContext_t *pcontext,lck_t *plock)
{
    usrctl_t  *pusr;
    int        i;
    
    /* Cache a pointer to this lock in the user control */
    pusr = pcontext->pusrctl;
    for( i = 0;
         i < UC_TABLE_LOCKS && pusr->uc_qtableLocks[i];
         i++);
    if ( i < UC_TABLE_LOCKS )
        pusr->uc_qtableLocks[i] = QSELF(plock);
                 
}

/* PROGRAM: lkTableLockCheck   -- Checks if table locking should be
 *                                turned off.
 *
 */
DSMVOID
lkTableLockCheck (dsmContext_t *pcontext)
{
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    lkctl_t      *plkctl     = pdbcontext->plkctl;
    LONG          curtime;
    
    if( !plkctl->tableLocksOn )
        return ;

    if( plkctl->numTableLocks )
        return;

    MU_LOCK_ALL_LOCK_CHAINS();
    if ((!plkctl->tableLocksOn) || plkctl->numTableLocks)
    {
        MU_UNLK_ALL_LOCK_CHAINS();
        return;
    }
    time(&curtime);

    if ( curtime - plkctl->timeOfLastTableLock > LK_NO_TABLE_LOCK_TIME )
    {
        /* Free all the intent locks.                    */
        lkTableLocksFree(pcontext);
    }
    MU_UNLK_ALL_LOCK_CHAINS();
    return;
}

/* PROGRAM: lkTableLocksFree   -- Free all table locks.             
 *                                It is assumed that caller has LOCKED
 *                                ALL of the LOCK CHAINS.
 */
LOCALF DSMVOID
lkTableLocksFree(dsmContext_t *pcontext)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    lkctl_t       *plkctl     = pdbcontext->plkctl;
    int            chain,lastChain;
    lck_t          *prev,*pcur;
    
    plkctl->tableLocksOn = 0;
        
    for( chain = plkctl->lk_chain[LK_TABLELK],
             lastChain = chain + plkctl->lk_hash;
         chain < lastChain; chain++)
    {
        prev = (struct lck *)&plkctl->lk_qanchor[chain];
        for ( ; (pcur = XLCK(pdbcontext, prev->lk_qnext)) != 0;)
        {
            lkfree(pcontext,prev, pcur,chain);
            prev = (struct lck *)&plkctl->lk_qanchor[chain];
        }
    }
    return;
}
  
/* PROGRAM: lkTurnOnTableLocks -- Scans the record lock table making sure
 *                               that all record locks have an intent lock
 *                               on the table.  By default when the database
 *                               is started table locking is not enabled.  It
 *                               is automatically enabled when the first
 *                               table lock is requested and then this routine
 *                               is called.
 *
 * RETURNS: 0 - for success
 *          DSM_S_LKTBFULL       If adding intent locks causes lock table
 *                               overflow.
 */
LOCALF    
LONG
lkTurnOnTableLocks(dsmContext_t  *pcontext)
{
    lck_t        *plkEntry,*ptableLock,*pwhereItShouldGo;
    int           chain, lastChain, tableChain;
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    lkctl_t      *plkctl     = pdbcontext->plkctl;
    usrctl_t     *pme;
    
    MU_LOCK_ALL_LOCK_CHAINS();
    if ( plkctl->tableLocksOn )
    {
        /* Someone else must be turning it on already.          */
        latunlatch(pcontext, MTL_LHT);
        return 0;
    }
    /* Set the flag indicating that table locking is on.        */
    plkctl->tableLocksOn = 1;
    pme = pcontext->pusrctl;
    
    /* Scan through all of the chains making sure each row lock has an
       intent lock on the table.                                 */
    chain = plkctl->lk_chain[LK_RECLK];
    for (lastChain = chain + plkctl->lk_hash ; chain < lastChain; chain++)
    {
        for ( plkEntry = XLCK(pdbcontext,plkctl->lk_qanchor[chain]);
              plkEntry;
              plkEntry = XLCK(pdbcontext,plkEntry->lk_qnext))
        {
            pcontext->pusrctl = XUSRCTL(pdbcontext,plkEntry->lk_qusr);
            ptableLock = lkTableLockGet(pcontext,plkEntry->lk_id.table,
                                        &pwhereItShouldGo,0);
            pcontext->pusrctl = pme;
            if( ptableLock )
            {
                ptableLock->lk_refCount.referenceCount++;
            }
            else
            {
                tableChain = MU_HASH_CHN(plkEntry->lk_id.table,LK_TABLELK);
                ptableLock = XLCK(pdbcontext,plkctl->lk_qfree);
                if( !ptableLock )
                {
                    lkTableLocksFree(pcontext);
                    MU_UNLK_ALL_LOCK_CHAINS();
                    return DSM_S_LKTBFULL;
                }
                plkctl->lk_qfree = ptableLock->lk_qnext;
                plkctl->lk_cur++;
                if ( plkctl->lk_cur > plkctl->lk_hwm )
                    plkctl->lk_hwm = plkctl->lk_cur;

                if ((plkEntry->lk_curf & LKTYPE) == LKSHARE)
                    ptableLock->lk_curf = LKIS;
                else
                    ptableLock->lk_curf = LKIX;
                 
                ptableLock->lk_id.table = plkEntry->lk_id.table;
                ptableLock->lk_refCount.referenceCount = 1;
                ptableLock->lk_qusr = plkEntry->lk_qusr;
                ptableLock->lk_qnext = pwhereItShouldGo->lk_qnext;
                pwhereItShouldGo->lk_qnext = QSELF(ptableLock);
                /* Cache it in the user's user control     */
                pcontext->pusrctl = XUSRCTL(pdbcontext,plkEntry->lk_qusr);
                lkCacheLock(pcontext,ptableLock);
                pcontext->pusrctl = pme;
            }
        }
    }
    MU_UNLK_ALL_LOCK_CHAINS();
    return 0;
}

/* PROGRAM: lkTableIntentLock -- Make sure table has a lock on the table
 *                               of the appropriate strength given the
 *                               requested record lock mode.            
 *                               
 *
 * RETURNS: 0 - for success 
 */
LOCALF    
LONG
lkTableIntentLock(dsmContext_t  *pcontext,
                  lockId_t      *plockId,      /* Lock identifier */
                  int            lockmode,     /* LKSHARE or LKEXCL */
                  int           *pmustLockLower, /* 1 - if caller
                                                    must still get record
                                                    locks.         */
                  TEXT           *pname)       /* table name for
                                                  lock wait message */
{
    lockId_t      lockId;
    int           tableLockMode;
    lck_t         *cur;
    usrctl_t      *pusr;
    dbcontext_t   *pdbcontext;
    lkctl_t       *plkctl;
    int            i;
    int            chain;
    LKSTAT         lockState;
    LONG           rc;

    *pmustLockLower = 1;
    
    if( plockId->dbkey == 0 )
    {
        /* Request is for a table lock             */
        return 0;
    }
    
    pdbcontext = pcontext->pdbcontext;
    plkctl = pdbcontext->plkctl;

    if (!plkctl->tableLocksOn)
    {
        /* If there are no table locks or requests for them
           then no need to get intent locks.                */
        return 0;
    }

    pusr = pcontext->pusrctl;
    tableLockMode = lockmode & ~LKTYPE;
    lockmode &= LKTYPE;    
    if( lockmode == LKSHARE )
    {
        tableLockMode |= LKIS;
    }
    else if (lockmode == LKEXCL )
    {
        tableLockMode |= LKIX;
    }
    else
    {
        MSGD_CALLBACK(pcontext,"%G invalid lock mode %l",lockmode);
    }
    lockState.lk_curf = 0;
    /* Check in user control to see if we already have a table lock */
    /* Caller should have locked the lock chain that the record lock
       is on.  This lock also protects our cache entry from being
       modified by lock purgers (see lkpurge) during or after our look
       at it.                                                        */
    for ( i = 0 ; i < UC_TABLE_LOCKS; i++ )
    {
        cur = XLCK(pdbcontext,pusr->uc_qtableLocks[i]);
        if ( cur && cur->lk_id.table == plockId->table )
        {
            /* Turn off no hold -- it is on if the first lock for
               a row of a table resulted in a qued intent lock for
               the table.  Make sure it is off so that lklocky won't
               release our table lock thinking this is a queued lock that
               is no longer wanted.                                */
            cur->lk_curf &= ~(LKNOHOLD | LKLIMBO);
            /* Is lock mode of sufficient strength   */
            if( (int)(cur->lk_curf & LKTYPE) >= (tableLockMode & LKTYPE) )
            {
                if( (cur->lk_curf & LKTYPE) == LKSHARE &&
                    (tableLockMode & LKTYPE) == LKIX)
                {
                    tableLockMode &= ~LKTYPE;
                    tableLockMode |= LKSIX;
                }
                else
                {
                    lockState.lk_curf = cur->lk_curf & LKTYPE;
                }
            }
            break;
        }
    }

    rc = 0;
    if( lockState.lk_curf == 0 )
    {
        lockId.table = plockId->table;
        lockId.dbkey = 0;
        /* Unlock chain record is on.                         */
        chain = MU_HASH_CHN ((plockId->table + plockId->dbkey), LK_RECLK);
        MU_UNLK_LOCK_CHAIN(chain);
        rc = lklocky(pcontext,&lockId,tableLockMode,LK_RECLK,
                     pname,&lockState);
        MU_LOCK_LOCK_CHAIN(chain);
    }
    if(rc == 0)
    {
        /* Determine if the strength of the granted table lock is
           sufficient to eliminate the need for obtaining locks at the
           next lower level of granularity.                          */
        if((lockState.lk_curf & LKTYPE) == LKEXCL)
            *pmustLockLower = 0;
        else if((lockState.lk_curf & LKTYPE) == LKSIX &&
                tableLockMode == LKIS)
            *pmustLockLower = 0;
        else if(((lockState.lk_curf & LKTYPE) == LKSHARE) &&
                tableLockMode == LKIS)
            *pmustLockLower = 0;
    }
    
    return rc;
}

#if 0 /* Not needed for MySQL  */

/* PROGRAM: lkTableLockUpdate - updates reference count in the table
   lock entry for this table and this user.
   */
int
lkTableLockUpdate(dsmContext_t    *pcontext,
                  lck_t           *precordLock, /* the record lock entry */
                  int              chain,       /* lock chain the record
                                                   lock is on            */
                  int              updateValue) /* 1 or -1               */
{
    int      tableChain;      /* lock chain the table lock is on   */
    int      weLockedTableChain = 0;
    lck_t    *prev, *cur, *first;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    lkctl_t  *plkctl = pcontext->pdbcontext->plkctl;
    usrctl_t *pusr;
    int       i;
    
    if (!plkctl->tableLocksOn)
        return 0;
    
    if( precordLock->lk_id.dbkey == 0 )
    {
        return 0;
    }
    
    if( chain >= plkctl->lk_chain[LK_RGLK])
        /* record get locks don't count */
        return 0;

    
    /* We may be purging another user's lock that was qued on a record
       we deleted -- so use the user control from the lock entry and
       not our context.                                                  */
    pusr = XUSRCTL(pdbcontext,precordLock->lk_qusr);

    /* It's assumed that lock chain for the record IS LOCKED!!! */

    /* Check if table lock is pointed to from the user control */

    for ( i = 0 ; i < UC_TABLE_LOCKS; i++ )
    {
        cur = XLCK(pdbcontext,pusr->uc_qtableLocks[i]);
        if ( cur && cur->lk_id.table == precordLock->lk_id.table )
        {
            /* Modify the reference count for this table lock */
            cur->lk_refCount.referenceCount += updateValue;

            /* is this a table lock with current locks */
            if (cur->lk_refCount.referenceCount > 0)
            {
                return 0;
            }
            else
            {
                /* ref count is zero must find the entry in
                   the lock chain to remove it.             */
                /* increment the reference count for this table lock */
                cur->lk_refCount.referenceCount++;
                break;
            }
        }
    }

    tableChain = MU_HASH_CHN(precordLock->lk_id.table,LK_TABLELK);

    MU_LOCK_LOCK_CHAIN(tableChain);
    
    /* Now find table lock entry in its chain    */
   
    prev = (struct lck *)&plkctl->lk_qanchor[tableChain];
    for (first = NULL; (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0; prev = cur)
    {
        if (precordLock->lk_id.table < cur->lk_id.table)
        {
            /* entry not found */
            cur = NULL;
            break;
        }

        if (precordLock->lk_id.table == cur->lk_id.table)
        {
            if (first == 0 )
                first = cur;
            if (precordLock->lk_qusr == cur->lk_qusr)
            {
                /* found the entry */
                break;
            }
        }
    }
    
    if( cur == NULL )
    {
        /* entry not found */
        if( weLockedTableChain )
	{
	  MU_UNLK_LOCK_CHAIN(tableChain);
	}
#ifdef MYDEBUG
        MSGD_CALLBACK(pcontext,
         "%BSYSTEM ERROR: Can't find table lock for table %l recid %l",
                      precordLock->lk_id.table,precordLock->lk_id.dbkey);
        utsleep(10000);
#else
        MSGD_CALLBACK(pcontext,
        "%GSYSTEM ERROR: Can't find table lock for table number %l",
                      precordLock->lk_id.table);
#endif       
    }

    /* for this table lock, increment(or decrement) the number of references */
    cur->lk_refCount.referenceCount += updateValue;

    /* for this table lock, see if there are no more references and 
       the lock strength is less than share */
    if((cur->lk_refCount.referenceCount == 0) && 
       ((int)(cur->lk_curf & LKTYPE) < LKSHARE))
    {
        if ( first == cur )
            first = XLCK(pdbcontext,cur->lk_qnext);

        if((cur->lk_curf & LKTYPE) == LKIS)
        {
            lkfree(pcontext,prev,cur,tableChain);
            /* Wake up any waiters.               */
            lkwkup(pcontext,first,precordLock->lk_id.table,0,tableChain);
        }
        else
        {
            /* This makes sure we keep our intent lock on the table
               when deleting rows from a table.                    */
            cur->lk_curf |= LKLIMBO;
        }
    }
    
    MU_UNLK_LOCK_CHAIN(tableChain);

    return 0;
}
#endif /* Not needed for MySQL  */

/* PROGRAM: lkTableLockGet - Seach for the specified table lock entry.
 *
 * RETUNRS: pointer to the lock entry if found; NULL otherwise.
 */
LOCALF struct lck *
lkTableLockGet(dsmContext_t *pcontext, LONG table, lck_t **pwhere, int latch)
{
    
    int      tableChain;      /* lock chain the table lock is on   */
    lck_t    *prev, *cur;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    lkctl_t  *plkctl = pcontext->pdbcontext->plkctl;
    usrctl_t *pusr = pcontext->pusrctl;
    int       i;
    
    /* Check if table lock is pointed to from the user control */

    for ( i = 0 ; i < UC_TABLE_LOCKS; i++ )
    {
        cur = XLCK(pdbcontext,pusr->uc_qtableLocks[i]);
        if ( cur && cur->lk_id.table == table )
        {
            return cur;
        }
    }

    tableChain = MU_HASH_CHN(table,LK_TABLELK);

    if ( latch )
    {
        MU_LOCK_LOCK_CHAIN(tableChain);
    }
    /* Now find table lock entry in its chain    */
    /* Note that prev gets assigned in a such a way that it
       is never null even the lock chain is empty.  Note also
       this works because lk_qnext is and must be the first member
       of the lock structure.                                      */
    prev = (struct lck *)&plkctl->lk_qanchor[tableChain];
    for ( ; (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0; prev = cur)
    {
        if (table < cur->lk_id.table)
        {
            /* entry not found */
            cur = NULL;
            break;
        }
        if (table == cur->lk_id.table)
        {
            if (QSELF(pusr) == cur->lk_qusr)
            {
                /* found the entry */
                break;
            }
        }
    }

    if (latch)
    {
        MU_UNLK_LOCK_CHAIN(tableChain);
    }

    if ( pwhere != NULL )
        *pwhere = prev;
    
    return cur;
}
   
/* PROGRAM: lkgetlocks -- allocates and initializes  the requested amount
 *                        of entries for the record lock table.
 *
 * RETURNS: pfirst - struct lck * - pointer to lock structure
 */
LOCALF struct lck *
lkgetlocks (
        dsmContext_t    *pcontext,
        int              amount)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
        struct  lck     *plck;
        struct  lck     *plast;
        struct  lck     *pfirst;
        struct  lck     *pnxtlck;

    pfirst = plck = (struct lck *)stGet(pcontext, XSTPOOL(pdbcontext, pdbpub->qdbpool),
                                        (unsigned)(amount*sizeof(struct lck)));
    if (plck == (struct lck *)NULL)
    {
        /* Insufficient storage to allocate lock table */
        FATAL_MSGN_CALLBACK(pcontext, svFTL008);
    }

    QSELF(pfirst) = P_TO_QP(pcontext, pfirst);

    /* chain all entries together */

    for (plast = plck + amount -1; plck < plast; plck++)
    {
        pnxtlck = plck + 1;
        plck->lk_qnext = QSELF(pnxtlck) = P_TO_QP(pcontext, pnxtlck);
    }
    /* initialize the next pointer to NULL for the last lock */

    plast->lk_qnext = 0;
    pdbpub->arglknum += amount;         /* update arglknum */

    return pfirst;
}

/* PROGRAM: lkinit -- allocates and initializes the record lock table,
 *                    with the indicated number of entries. All entries are
 *                    initially placed on the free chain. 
 *
 * RETURNS: None
 */
DSMVOID
lkinit (dsmContext_t *pcontext)
{

    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
        struct  lck     *plck;
        struct  lck     *plast = (struct lck *)NULL;
        struct  lkctl   *plkctl;
        LONG            amount;
        int             chain;
        int             hashprime;

    if (pdbcontext->arg1usr)
        return; /* a single user, no locking is needed #*/

    hashprime = pdbpub->arglkhash;

    if (hashprime > 9973)
        hashprime = 9973;

    MU_LOCK_LOCK_TABLE ();

    /* allocate lkctl */
    plkctl = (LKCTL *)stGet(pcontext, XSTPOOL(pdbcontext, pdbpub->qdbpool),
                       (unsigned)(sizeof(struct lkctl)) +
                       ((hashprime * LK_NUMTYPES) * sizeof (QLCK)));

    pdbpub->qlkctl = (QLKCTL)P_TO_QP(pcontext, plkctl);
    pdbcontext->plkctl = plkctl;
    plkctl->lk_hash = hashprime;

    /* fill in starting chain numbers for each lock class */

    for (chain = 0; chain < LK_NUMTYPES; chain++)
    {
        plkctl->lk_chain[chain] = chain * hashprime;
    }
    /* allocate lock table entries in groups of 32
       The actual number of entries is rounded to the next 32 multiple*/

    plkctl->lk_qfree = 0;

    amount = pdbpub->arglknum; /* amount requested */
    for (pdbpub->arglknum = 0; pdbpub->arglknum < amount;)
    {
#if NW386
        ThreadSwitchWithDelay();
#endif
        plck = lkgetlocks(pcontext, 32); /* get 32 entries for the lock table */

        if (!plkctl->lk_qfree)
        {
            /* first group goes on front of free list */

            plkctl->lk_qfree = QSELF(plck);
        }
        else
        {
            /* add to the end of the previous group */

            plast->lk_qnext = QSELF(plck);
        }
        plast = plck + 31;
    }

    MU_UNLK_LOCK_TABLE ();
    return;
}

/* PROGRAM: lklocky -- called to lock a record with the given mode.
 * (see also lklock which puts the user to sleep
 * if DSM_S_RQSTQUED)
 * 
 * Lockmode indicates the strength of lock requested and
 * whether or not to wait for the lock. It is composed
 * from LKSHARE, LKEXCL, and  LKNOWAIT in the obvious
 * additive combinations.
 * 
 * RETURNS:
 * If the lock is available and granted, the returned value
 * is zero.
 * 
 * If the lock is not available and lockmode includes the
 * LKNOWAIT bit, the value returned is DSM_S_RQSTREJ.
 * 
 * If LKNOWAIT is not specified and the lock request cannot
 * be immediately granted, the request is queued and the
 * value DSM_S_RQSTQUED is returned. The queued request can be
 * cancelled with a call to lkcncl. 
 *
 */

/*****************************************************************/
/**                                                             **/
/**     Synopsis of processing:                                 **/
/**                                                             **/
/**     The usrctl field uc_qwait2 is examined to see if        **/
/**     the record being requested is the same as the last      **/
/**     record requested. If not, and if the previous request   **/
/**     was queued, then the place holder is released. If so,   **/
/**     and if the previous request was for the same or greater **/
/**     strength, update the entry and return a zero.           **/
/**                                                             **/
/**     If this simple optimization fails, then the correct     **/
/**     chain anchor is selected by and'ing the dbkey with      **/
/**     LKHMASK. This has the effect distributing all of        **/
/**     the possible dbkeys to shorten the length of chains     **/
/**     which must be scanned.                                  **/
/**                                                             **/
/**     After the appropriate lck chain has been selected,      **/
/**     it is scanned for the indicated dbkey/user pair. This   **/
/**     scan manipulates two pointers into the lck chain:       **/
/**                                                             **/
/**     prev - points to the previous lck entry examined, or    **/
/**     to the chain anchor. Used for chaining new entries,     **/
/**     if necessary.                                           **/
/**                                                             **/
/**     cur - points to the current entry being examined.       **/
/**     NULL if there are no entries for the indicated dbkey.   **/
/**                                                             **/
/**     As this scan is being performed, each lock is examined  **/
/**     for a conflict.  A conflict is recognized if an         **/
/**     existing lock is found and EITHER it is exclusive, or   **/
/**     the request is exclusive.  If a conflict is detected    **/
/**     the usrctl variable uc_wusrnum is set to remember       **/
/**     the usrctl of the first user who holds a conflicting    **/
/**     lock.  Nevertheless, the lock scan continues, to find   **/
/**     the correct point to insert a Queued lock request       **/
/**                                                             **/
/**     The scan stops when either a dbkey with a larger        **/
/**     value is next, or when an entry for the user/dbkey      **/
/**     pair is found. This yields three possible states        **/
/**     at the end of the scan:                                 **/
/**                                                             **/
/**     1. No conflicting entry for the dbkey was found.        **/
/**        Indicated by cur = NULL, conflict = 0. In this case  **/
/**        prev points to the entry after which the current     **/
/**        dbkey should be added (or the anchor), the request   **/
/**        may be granted immediately.                          **/
/**                                                             **/
/**     2. No entry for this dbkey/user was found, but          **/
/**        conflicting entries for other users were found.      **/
/**        Indicated by cur = NULL, conflict != 0. In this case **/
/**        prev points to the entry after which the current     **/
/**        dbkey should be added (or the anchor), the request   **/
/**        cannot be granted immediately.                       **/
/**                                                             **/
/**     3. An entry for this dbkey/user was found. cur points   **/
/**        to the entry.  conflict is set if any previous entry **/
/**        conflicts with the request.  If the request is for   **/
/**        exclusive control and the entry is not already an    **/
/**        active or limbo exclusive lock, and there is an      **/
/**        entry for the indicated dbkey immediately following  **/
/**        cur, then conflict is set to indicate the lock can't **/
/**        be granted immediately (uc)wusrnum is also set to    **/
/**        indicate the conflicting usrctl).                    **/
/**        If conflict is not set, then the lock request will   **/
/**        will be granted immediately.                         **/
/**                                                             **/
/**     After determining the insertion point and state,        **/
/**     a check is made to see if the request conflicts with    **/
/**     existing entries. If so, and if LKNOWAIT was            **/
/**     specified, DSM_S_RQSTREJ is returned with no further    **/
/**     processing.                                             **/
/**                                                             **/
/**     If cur is NULL, allocate an entry and fill it in.       **/
/**     It is a fatal error if no free entry is available.      **/
/**     High water mark is maintained to someday assist in      **/
/**     estimating the correct value for the -L parameter.      **/
/**                                                             **/
/**     For upgrade requests, save the current entry status     **/
/**     for lkcncl and to preserve limbo status. If this        **/
/**     is a share request and the entry has limbo exclusive    **/
/**     status, grant the lock as exclusive.                    **/
/**                                                             **/
/**     The uc_qwait2 field in usrctl is set to point at the    **/
/**     current entry, both to support the optimization         **/
/**     previously mentioned, and for lkcncl if the request     **/
/**     cannot be granted.                                      **/
/**                                                             **/
/**     If the request cannot be granted, the LKQUEUED bit      **/
/**     in the entry's current flags is set, uc_wait1 is set    **/
/**     to the dbkey of the requested record, uc_wait is set    **/
/**     to RECWAIT to indicate the cause of wait, uc_wusrctl    **/
/**     is set indicating to the first usrctl who holds a       **/
/**     conflicting lock, and DSM_S_RQSTQUED is returned to the **/
/**     caller (the server loop).                               **/
/**     The child process is then signalled to retry its most   **/
/**     recent operation. The retry may or may not produce      **/
/**     a lock request for the same dbkey. If a different       **/
/**     dbkey is requested on retry, the original lock would    **/
/**     be "hanging". Solving this by releasing the acquired    **/
/**     lock before signalling the child would be somewhat      **/
/**     unfair if the same dbkey were re-requested. The         **/
/**     compromise solution used is as follows:                 **/
/**                                                             **/
/**     If the caller did not already have some form of         **/
/**     lock for the dbkey, then the waiting lock request       **/
/**     is flagged with LKNOHOLD. When the request is finally   **/
/**     granted, the user will be sent a RECGRANT response.     **/
/**     If the next lklock request is for a different dbkey,    **/
/**     then the LKNOHOLD entry will be released. Otherwise     **/
/**     the entry is immediately available. Of course, lkpurge  **/
/**     or lkrels will discard the lock if called.              **/
/**                                                             **/
/**     If the caller already holds the lock in real share,     **/
/**     it will be granted in real exclusive, since the caller  **/
/**     already knows to free the lock, even if retry does      **/
/**     not re-request it.                                      **/
/**                                                             **/
/**     If the caller holds the lock in limbo, it will be       **/
/**     granted in exclusive limbo. Then either the retry       **/
/**     will re-request it and get a "free" upgrade, which      **/
/**     is later released by lkrels, or lktend will free it.    **/
/**                                                             **/
/**     Either a lkrels will wake up the caller, or a lkcncl    **/
/**     call will be made if the user's process detects a       **/
/**     control-c.                                              **/
/**                                                             **/
/*****************************************************************/


/* PROGRAM: lklocky -- does the work of lockx for a specific lock chain
 *                     the anchor of which is given by "prev"
 *
 * RETURNS: (0) - success
 *          DSM_S_RQSTREJ - request rejected
 *          DSM_S_RQSTQUED - request queued 
 */
int
lklocky (
        dsmContext_t    *pcontext,
        lockId_t        *plockId,        /* identifier of thing to be locked */
        int              lockmode,       /* requested lock strength:
                                          * LKSHARE or LKEXCL */
                                         /* optionally ORed with LKNOWAIT */
        int              lockgroup,      /* the lock chain set to use */
        TEXT             *pname,         /* refname for Rec in use message - 
                                          * not used in this (the server)
                                          * version.Used in child stub only*/
        LKSTAT           *plkstat)       /* returns lock status if not null */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    LCK             *prev;
    LCK             *cur;
    usrctl_t        *pusr = pcontext->pusrctl;
    struct  lkctl   *plkctl = pdbcontext->plkctl;
    int              conflict;
    LCK             *plastGrantedLock; /* Ptr to last granted lock in the lock
                                          chain for the given lock identifier */
    int     chain;
    TEXT    waitCode = RECWAIT;
    ULONG   lkmask;
    int     retvalue;
    int     counttype, /* 0 = waitcnt, 1 = lockcnt */
            codetype;
    int     hashGroup;
    int     lockRecords;
    int     nextLockState;
    LONG    rc;
    
    if (!plkctl)        /* if lkinit not yet called, ignore this call */
        return 0;

    if(pdbcontext->pdbpub->slocks[EXCLCTR - 1] > 0 &&
       pdbcontext->accessEnv == DSM_SQL_ENGINE)
        /* No need to get record locks when holding the schema lock. */
        return 0;

    if (plockId->dbkey < 0) return (0); /* bug 93-06-24-066 */

    if(SCTBL_IS_TEMP_OBJECT(plockId->table))
      return 0;

    /* let the system know that we've actually gotten a lock */
    pusr->uc_lockinit = 1;

    if (lockgroup >= LK_RGLK)
    {
        /* not a record lock request */

        if (lockmode & LKNOWAIT)
        {
            /* Caller does not want to wait. But this is not allowed */

            /* lklocky: LKNOWAIT for non-record lock */
            FATAL_MSGN_CALLBACK(pcontext, svFTL031);
        }
        if (lockgroup == LK_RGLK) waitCode = RGWAIT;
    }

    TRACE3_CB(pcontext, "LKLOCKY dbkey %D lockmode %i usrctl %p",
              plockId->dbkey, lockmode, pusr)

    if ((lockmode & LKTYPE) == LKNOLOCK)        /* ignore void lock requests */
        return 0;

    /* We can only wait for one lock request at a time.  If the previous */
    /* one was not resolved, barf */
    if (pcontext->pusrctl->uc_wait)
        FATAL_MSGN_CALLBACK(pcontext, muFTL001, pcontext->pusrctl->uc_wait1);

    /* determine correct chain for this dbkey */
    if( plockId->dbkey == 0 && lockgroup <= LK_TABLELK )
    {
        hashGroup = LK_TABLELK;
        /* Is table locking on ?                   */
        if(!plkctl->tableLocksOn && ((lockmode & LKTYPE) >= LKSHARE))
        {
            /* No - we must turn it on              */
            rc = lkTurnOnTableLocks(pcontext);
        }        
    }
    else
    {
        hashGroup = lockgroup;
    }
    
    chain = MU_HASH_CHN ((plockId->table + plockId->dbkey), hashGroup);
    prev = (LCK *)&plkctl->lk_qanchor[chain];

    conflict = 0;
    plastGrantedLock = NULL;
    lkmask = 0;
    
    MU_LOCK_LOCK_CHAIN (chain);    

    

    /* if there is a previous lock request, see if it was for the
       same dbkey with sufficient strength. If so, grant the request
       immediately. If it was for a different dbkey, see if the previous
       request was queued and then later granted and if so, delete the
       place holder */

    if (pusr->uc_qwait2 && (lockgroup == LK_RECLK))
    {
        /* there was a previous request. see if for same dbkey and
           if so, save some time */

        cur = XLCK(pdbcontext, pusr->uc_qwait2);
        if (plockId->dbkey == cur->lk_id.dbkey &&
             plockId->table == cur->lk_id.table)
        {
            /* same key re-requested, see if previous request sufficient */
            if(((lockmode  & LKIX) &&
               (cur->lk_curf & LKSHARE)) ||
               ((lockmode & LKSHARE) &&
                (cur->lk_curf & LKIX)))
            {
                lockmode &= ~LKTYPE;
                lockmode |= LKSIX;
            }
            if ((!(cur->lk_curf & LKQUEUED)) &&
                ((int)(cur->lk_curf & LKTYPE) >= (int)(lockmode & LKTYPE)))
            {
                /* lock not queued and of sufficient strenght: grant it */

                /* if the current lock is NOHOLD or UPGRD, then we didn't */
                /* actually have it, so don't change cur->lk_prevf. */
                if ((cur->lk_curf & (LKNOHOLD | LKUPGRD)) == 0)
                {
                    cur->lk_prevf = cur->lk_curf;
                    /* increment lock strength counters if this is not
                       a table lock */
                    if (cur->lk_id.dbkey != 0)
                    {
                        if (lockmode & LKSHARE) 
                        {
                            cur->lk_refCount.lockCounts.shareLocks++;
                        }
                        else if(lockmode & LKEXCL)
                        {
                            cur->lk_refCount.lockCounts.exclusiveLocks++;
                        }
                    }
                }
                cur->lk_curf &= LKTYPE;
                goto ok;
            }
        }
        else
        {
            /* Different dbkey. */

            if (cur->lk_curf & LKNOHOLD)
            {
                /* Previous request was queued and then granted. But we
                   don't want it anymore, so delete it */
                MU_UNLK_LOCK_CHAIN( chain );
                lkrels (pcontext, &cur->lk_id);
                MU_LOCK_LOCK_CHAIN( chain );
            }
        }
    }

    if (hashGroup == LK_RECLK)
    {
        /* bit to set for record lock chains we use. lktend only
           looks at these chains when releasing all the locks */
        lkmask = ((ULONG)1 << (chain & 0x1F));
        
        /* Make sure table has an appropriate strength lock  */
        retvalue = lkTableIntentLock(pcontext,plockId,lockmode,
                                     &lockRecords,pname);
        if( retvalue || !lockRecords)
        {
            MU_UNLK_LOCK_CHAIN(chain);
            return retvalue;
        }
    }

    if (lockgroup == LK_RECLK)
    {
        /* The code above handles requests for the same lock as we
           requested previously. Since it wasn't handled above, we
           start over. But we must be sure to set it below */

        pusr->uc_qwait2 = (QLCK)NULL;
    }
    
    /* determine insertion point or locate entry, set cum flags */
    /* scan the chain for conflicting locks */

    for (; (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0; prev = cur)
    {
        if (plockId->table < cur->lk_id.table ||
            ( plockId->table == cur->lk_id.table &&
             plockId->dbkey < cur->lk_id.dbkey))
        {
            /* No matches. New entry goes right after prev because we
               keep them ordered by dbkey */

            cur = (LCK *)NULL;
            break;
        }
        if (plockId->table == cur->lk_id.table && 
            plockId->dbkey == cur->lk_id.dbkey)
        {
            /* Matches the one we want to lock */

            if (QSELF(pusr) == cur->lk_qusr)
            {
                /* This is a lock we own */
                
                /* If the request and the current lock state are IX or S
                   respectively or vice-versa then the upgrade request should
                   become SIX.                                             */
                if((((lockmode & LKTYPE) == LKIX) &&
                   ((cur->lk_curf & LKTYPE) == LKSHARE)) ||
                   (((lockmode & LKTYPE) == LKSHARE &&
		     (cur->lk_curf & LKTYPE) == LKIX)))
                {
                    lockmode &= ~LKTYPE;
                    lockmode |= LKSIX;
                }
   
                if ((!(cur->lk_curf & LKQUEUED)) &&
                    ((int)(cur->lk_curf & LKTYPE) >= (int)(lockmode & LKTYPE)))
                {
                    /* active and same or greater strength than new request, */
                    /* but not pointed to by uc_qwait2, so it can't be */
                    /* LKNOHOLD or LKUPGRD */

                    pusr->uc_qwait2 = (QLCK)QSELF(cur);
                    cur->lk_prevf = cur->lk_curf;
                    cur->lk_curf &= LKTYPE;
                    /* increment lock strength counters if this is not
                       a table lock */
                    if (cur->lk_id.dbkey != 0)
                    {
                        if (lockmode & LKSHARE)
                        {
                            cur->lk_refCount.lockCounts.shareLocks++;
                        }
                        else if(lockmode & LKEXCL)
                        {
                            cur->lk_refCount.lockCounts.exclusiveLocks++;
                        }
                    }
                    goto ok;
                }
                else
                    break;      /* cur is entry, with flags set so far */
            }
            if( (cur->lk_curf & LKQUEUED) && plastGrantedLock == NULL &&
                (!(cur->lk_curf & LKUPGRD)))
                /* We'll insert our lock after this one later on if there
                 are no conflicting locks active or queued */
                plastGrantedLock = prev;
            
            /* different owner -- there is a lock conflict.   */
            if (!conflict && 
                (conflict = lkLockConflict(lockmode,cur->lk_curf)) )
            {
                /*  there is a conflict */
                pusr->uc_wusrnum = (XUSRCTL(pdbcontext, cur->lk_qusr))->uc_usrnum;
                conflict = 1;
            }
        }
    } /* end for loop on lock chain  */
#if 0    
    if (!conflict
        && lockmode & LKEXCL
        && cur != NULL
        && cur->lk_qnext != NULL
        && XLCK(pdbcontext, cur->lk_qnext)->lk_id.table == plockId->table
        && XLCK(pdbcontext, cur->lk_qnext)->lk_id.dbkey == plockId->dbkey
        && XLCK(pdbcontext, cur->lk_qnext)->lk_curf != (LKQUEUED | LKEXCL | LKNOHOLD))
            {
        /* This is an upgrade, but there are more locks, queue it */

        /* NOTE: the last test in this if statement says that it */
        /* is OK to upgrade a share lock if the next lock is a   */
        /* queued excl request (WHICH IS NOT AN UPGRADE REQUEST) */
        /* This is a policy decision made 3/10/86.  This code    */
        /* depends on the fact that if the next lock is QUE+EXCL */
        /* then there can't be any conflicting locks beyond it   */
        pusr->uc_wusrnum = 
           (XUSRCTL(pdbcontext, XLCK(pdbcontext, cur->lk_qnext)->lk_qusr))->uc_usrnum;
        conflict = 1;
    }
#else
    if (!conflict
        && cur != NULL)
    {
        lck_t   *pnext;

        /* This a lock upgrade request see if it conflicts with any
           granted entry.  */
        for( pnext = XLCK(pdbcontext,cur->lk_qnext);
             pnext;
             pnext = XLCK(pdbcontext,pnext->lk_qnext))
        {
            if( pnext->lk_id.table == plockId->table &&
                pnext->lk_id.dbkey == plockId->dbkey)
            {
                if(pnext->lk_curf & (LKUPGRD | LKQUEUED))
                    /* Test for conflict with granted lock strength
                       not the queued lock strength                 */
                    nextLockState = pnext->lk_prevf;
                else
                    nextLockState = pnext->lk_curf;
                
                if( nextLockState & LKQUEUED )
                    /* Into the queued locks -- have looked far
                       enough for a conflict.  We'll grant the lock */
                    break;
                else if ((conflict =  lkLockConflict(lockmode,nextLockState)))
                {
                    /* This is a conflicting granted lock
                         so queue the upgrade request.   */
                    pusr->uc_wusrnum = 
                        (XUSRCTL(pdbcontext,pnext->lk_qusr))->uc_usrnum;
                    break;
                }
            }
            else
                /* Different table or dbkey looked far enough  */
                break;
        }
    }
#endif    
    if (conflict && (lockmode & LKNOWAIT))
    {
        /* conflict, but caller said he does not want to wait */

        MU_UNLK_LOCK_CHAIN (chain);
        return DSM_S_RQSTREJ;
    }
    if (cur == (LCK *)NULL)
    {
        /* not an upgrade request, allocate and initialize entry */

        /* check if user caused overflow earlier and must resync */

        if ((lockgroup < LK_RGLK)       /* it is a record lock */
            && (pusr->lktbovfw  & LKOVFLOW) /* user caused overflow before */
            && !(pusr->lktbovfw  & INTMREJ)) /* not in the middle of tmrej */
        {
            MU_UNLK_LOCK_CHAIN (chain);

            return lktbresy(pcontext, pusr); /* resync user to free his locks */
        }

again:
        MU_LOCK_LOCK_TABLE ();

        cur = XLCK(pdbcontext, plkctl->lk_qfree);
        if (cur == (LCK *)NULL)
        {
            /* no more free lock nodes - allocate a few more */
            /* get new entry */
            plkctl->lk_qfree = P_TO_QP(pcontext, lkgetlocks (pcontext, 1));
            pusr->lktbovfw  |= LKOVFLOW; /* mark user caused overflow */

            MU_UNLK_LOCK_TABLE ();

            /* if the user already started the tmrej, use the new entries */
            if (pusr->lktbovfw & INTMREJ)
                goto again;
            if (lockgroup >= LK_RGLK)   /* may not resync if RG lock*/
                goto again;
            MU_UNLK_LOCK_CHAIN (chain);

            return lktbresy(pcontext, pusr); /* resync user to free his locks */
        }

        plkctl->lk_qfree = cur->lk_qnext; /* take one off the free list */

        if (++plkctl->lk_cur >= plkctl->lk_hwm) /* update the highwater mark */
            plkctl->lk_hwm = plkctl->lk_cur;

        /*initialize the new lock entry */
        cur->lk_id.table = plockId->table;
        cur->lk_id.dbkey = plockId->dbkey;
        cur->lk_qusr = QSELF(pusr);
        cur->lk_prevf = 0;
        cur->lk_curf = lockmode;
        /* increment lock strength counters if this is not a table lock */
        if (cur->lk_id.dbkey != 0)
        {
            if (lockmode & LKSHARE)
            {
                cur->lk_refCount.lockCounts.shareLocks = 1;
                cur->lk_refCount.lockCounts.exclusiveLocks = 0;
            }
            else if(lockmode & LKEXCL)
            {
                cur->lk_refCount.lockCounts.exclusiveLocks = 1;
                cur->lk_refCount.lockCounts.shareLocks = 0;
            }
        }

        /* remember which record chains we used for lktend to look at */
        if( plockId->dbkey > 0 )
            pusr->uc_lkmask |= lkmask;
        else
        {
            /* Its a table lock add it to the cache in the
               user's usrctl.                              */
            lkCacheLock(pcontext,cur);
        }
        if (conflict)
        {
            /* request is to be queued, wake up in auto-purge */
            cur->lk_curf |= (LKNOHOLD | LKQUEUED);
        }
        else
        {
            /* no conflict but there may be queud locks so the insert
               point in the chain isafter the last granted lock.       */
            if ( plastGrantedLock )
                prev = plastGrantedLock;
        }
        
        cur->lk_qnext = prev->lk_qnext; /* insert after prev */
        prev->lk_qnext = QSELF(cur);
        
        if ( (int)(cur->lk_id.dbkey == 0) && 
             (int)(cur->lk_curf & LKTYPE) >= LKSHARE)
            plkctl->numTableLocks++;
        
        MU_UNLK_LOCK_TABLE ();
#if 0
        retvalue = lkTableLockUpdate(pcontext,cur,chain,1);
#endif
    }
    else                                /* upgrade request */
    {
        /* if the previous lock was NOHOLD, we really didn't have a lock   */
        /* so the request really wasn't an upgrade but a new lock request. */
        if ((cur->lk_curf & LKNOHOLD) == 0)
        {
            cur->lk_prevf = cur->lk_curf; /* save pre upgrade entry status */
            /* increment lock strength counters if this is not a table lock */
            if (cur->lk_id.dbkey != 0)
            {
                if (lockmode & LKSHARE)
                {
                    cur->lk_refCount.lockCounts.shareLocks++;
                }
                else if(lockmode & LKEXCL)
                {
                    cur->lk_refCount.lockCounts.exclusiveLocks++;
                }
            }
        }

        cur->lk_curf = lockmode & LKTYPE;
#if 0
        if ((cur->lk_prevf & (LKEXCL | LKLIMBO)) == (LKEXCL | LKLIMBO))
            /* was limbo exclusive: upgrade to non-limbo exclusive */
            cur->lk_curf = LKEXCL;
#endif        
        if( cur->lk_prevf & LKLIMBO )
        {
            if ( (int)(cur->lk_prevf & LKTYPE) > (int)(cur->lk_curf & LKTYPE) )
            {
                /* This is really a downgrade request -- but within
                   a transaction we don't downgrade -- within a tx
                   we always keep current the highest strength lock
                   thus far obtained by the user on this object.   */
                cur->lk_curf = cur->lk_prevf & LKTYPE;
            }
        }
        
        if (conflict)           /* conflict: queue the upgrade request */
        {
            cur->lk_curf |= LKQUEUED;
            if (cur->lk_prevf & LKTYPE)
            {
                cur->lk_curf |= LKUPGRD;
            }
            else
                cur->lk_curf |= LKNOHOLD;       /* didn't have a lock */

            if (cur->lk_prevf & LKLIMBO)        /* used to be limbo */
                cur->lk_curf |= LKLIMBO;        /* so it remains limbo */
        }
    }

    if (lockgroup < LK_RGLK)
        pusr->uc_qwait2 = (QLCK)QSELF(cur);

    if (conflict)
    {
        pusr->uc_wait = waitCode;
        pusr->waitArea = plockId->table;
        pusr->uc_wait1 = plockId->dbkey;
        if (lockgroup == LK_RGLK)
        {
            /* record-get locks are granted to us when the holder
               releass the lock, so when we wake up, we have the lock */

            cur->lk_curf = cur->lk_curf & ~LKNOHOLD;

            MU_UNLK_LOCK_CHAIN (chain);

            latUsrWait (pcontext);      /* wait for the lock to be granted */

            pcontext->pusrctl->lockcnt[waitCode]++;     /* collect stats on locks */
            counttype = 1;            
            codetype = waitCode;
            /* update the lockcnt in the latch structure */
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);

            return 0;
        }
        /* lock request was queued, tell caller */

        MU_UNLK_LOCK_CHAIN (chain);

        if (!pcontext->pusrctl->uc_block)
        {
            /* Bug 94-03-17-020: add this code
               Collect stats on locks for the monitor. We count for
               server's clients here because the server does not call
               latUsrWait(pcontext), it just goes on to do something else
               while the client is waiting. */

            pcontext->pusrctl->waitcnt[waitCode]++;
            counttype = 0;            
            codetype = waitCode;
            /* update the waitcnt in the latch structure */
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
        }
        return DSM_S_RQSTQUED;
    }

ok:                                             /* the lock was granted */
    if (plkstat != (LKSTAT *)NULL)             /* return lock status */
    {
        plkstat->lk_prevf = cur->lk_prevf;
        plkstat->lk_curf = cur->lk_curf;
    }

    /* collect stats on locks*/

    pcontext->pusrctl->lockcnt[waitCode]++;
    counttype = 1;
    codetype = waitCode;
    /* update the lockcnt in the latch structure */
    retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);

    MU_UNLK_LOCK_CHAIN (chain);

    return 0;

}  /* end lklocky */


/* PROGRAM: lklock -- calls lklockx to lock a record with the given mode.
 * it calls lklockx to do the work, and put the user to sleep
 * if the return code is DSM_S_RQSTQUED.
 * 
 * Lockmode indicates the strength of lock requested and
 * whether or not to wait for the lock. It is composed
 * from LKSHARE, LKEXCL, and  LKNOWAIT in the obvious
 * additive combinations.
 * 
 * RETURNS: 
 * If the lock is available and granted, the returned value
 * is zero.
 * 
 * If the lock is not available and lockmode includes the
 * LKNOWAIT bit, the value returned is DSM_S_RQSTREJ.
 * 
 * If LKNOWAIT is not specified and the lock request cannot
 * be immediately granted, the request is queued and the
 * user put to sleep. The queued request can be
 * cancelled with a call to lkcncl.
 *
 */
int
lklock (
        dsmContext_t    *pcontext,
        lockId_t        *plockId,   /* identifier of thing to be locked */
        int              lockmode,  /* strength of lock requested,
                                           either LKSHARE or LKEXCL, with
                                           LKNOWAIT optionally added in */
        TEXT            *pname)         /* refname for Rec in use message -
                                        not used in this (the server) version.
                                   Used in child stub only*/
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    int    ret;
    
    /* if lkinit not yet called, ignore lklock calls */
    if (!pdbcontext->plkctl)
        return 0;

    for (;;)
    {
        if (pdbcontext->resyncing || lkservcon(pcontext))
            return DSM_S_CTRLC;

        ret = lklocky (pcontext, plockId, lockmode, LK_RECLK,
                       pname, (LKSTAT *)NULL);
        if (ret != DSM_S_RQSTQUED)
            return (ret);

        /* some servers don't wait for their users */
        if (!pcontext->pusrctl->uc_block)
            return (ret);

        /* wait for the lock and retry */

        if ((ret = lkwait(pcontext, LW_RECLK_MSG, pname)) != 0)
            return ret;
    }

}  /* end lklock */


/* PROGRAM: lkrels -- called to release the indicated record previously
 * locked by the caller. Complains if the record is not
 * currently locked by the calling user. 
 *
 * RETURNS:  None
 */

/*****************************************************************/
/**                                                             **/
/**     Synopsis of processing:                                 **/
/**                                                             **/
/**     First the appropriate active chain is scanned. If       **/
/**     the indicated dbkey is not found, then the purged       **/
/**     lock chain is scanned. If the lock is on the purged     **/
/**     chain, it is deleted without any wakeup or limbo check. **/
/**                                                             **/
/**     If the caller owns the exclusive schema lock, then      **/
/**     the released lock is always purged. Otherwise,          **/
/**     if the caller is in a transaction (uc_task != 0),       **/
/**     the released lock is not purged, instead it is          **/
/**     placed in a "limbo" state with the same strength.       **/
/**     This is used to avoid cascading backout if a            **/
/**     transaction is rejected.                                **/
/**                                                             **/
/**     A limbo lock will never be granted in a weaker state.   **/
/**     Thus, if an exclusive lock is released with a non-      **/
/**     zero purge parameter, and the same user re-requests     **/
/**     the lock as share, it will be granted as exclusive.     **/
/**                                                             **/
/**     Callers of the record lock manager are generally        **/
/**     unaware of any limbo locks held in their behalf.        **/
/**     Once a database task has been accepted or rejected      **/
/**     (backed out), lktend is called. lktend releases all     **/
/**     limbo locks held by its caller.                         **/
/**                                                             **/
/*****************************************************************/
DSMVOID
lkrels(
        dsmContext_t    *pcontext,
        lockId_t        *plockId) /* extended dbkey of RECORD to release */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    /* if lkinit not yet called, ignore lkrels calls */
    if (!pdbcontext->plkctl)
        return;

    if (pdbcontext->resyncing || lkservcon(pcontext))
        return;/*SELF-SERVICE only */

    TRACE2_CB(pcontext, "LKRELS dbkey %D usrctl %p",
              plockId->dbkey, pcontext->pusrctl)

    lkrelsy(pcontext, plockId, LK_RECLK);
    return;
}


/* PROGRAM: lkrelsy -- does the work of lkrels for a specific lock chain 
 *
 * RETURNS:  None
 */
DSMVOID
lkrelsy (
        dsmContext_t    *pcontext,
        lockId_t        *plockId,   /* dbkey of RECORD to release */
        int              lockgroup)       /* the type of the lock set to use*/
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
        struct  lck     *cur;
        struct  lck     *prev;
        usrctl_t        *pusr;
        struct  lck     *first;
        struct  lkctl   *plkctl = pdbcontext->plkctl;
        int      chain;
        int      purged = 0;
        int      hashGroup;
        
    /***the DB semaphore must already be locked before calling this ***/

    if (!plkctl)        /* if lkinit not yet called, ignore this call */
        return;

    if (plockId->dbkey < 0)
        return; /* bug 93-06-24-066 */

    if(SCTBL_IS_TEMP_OBJECT(plockId->table))
      return;

    pusr = pcontext->pusrctl;

    TRACE2_CB(pcontext, "LKRELS dbkey %D usrctl %p", plockId->dbkey, pusr)

    /* determine correct chain for this dbkey */
    if( plockId->dbkey == 0 && lockgroup <= LK_TABLELK )
    {
        /* Its a table lock                      */
        hashGroup = LK_TABLELK;
    }
    else
    {
        hashGroup = lockgroup;
    }
    
    chain = MU_HASH_CHN ((plockId->table + plockId->dbkey), hashGroup);
    prev = (struct lck *)&plkctl->lk_qanchor[chain];

    /* locate the entry to release, set pointers */

    MU_LOCK_LOCK_CHAIN (chain);

    for (first = (LCK *)NULL; (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0; prev = cur)
    {
        if (plockId->table < cur->lk_id.table)
        {
            cur = (struct lck *)NULL;
            break;
        }
        else if (plockId->table == cur->lk_id.table &&
                 plockId->dbkey < cur->lk_id.dbkey)
        {
            cur = (struct lck *)NULL;
            break;                              /* entry not found */
        }
        else if (plockId->table == cur->lk_id.table &&
                 plockId->dbkey == cur->lk_id.dbkey)
        {
            if (first == (struct lck *)NULL)
                first = cur;

            if (QSELF(pusr) == cur->lk_qusr)    /* found the entry */
                break;
        }
    }
    /* if lock was not in its appropriate active chain, try the purged
       chain. note the purged chain is not sorted */
    if ((cur == (struct lck *)NULL) && (lockgroup < LK_RGLK))
    {
        MU_UNLK_LOCK_CHAIN (chain);

        chain = MU_HASH_PRG (pusr->uc_usrnum);

        purged = 1;
        MU_LOCK_PURG_CHAIN (chain);     /* lock the purged record chain */

        /* check the purged chain */
        for (prev = (LCK *)&plkctl->lk_qanchor[chain];
              (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0;
              prev = cur)
        {
            if ((plockId->table == cur->lk_id.table) &&
                (plockId->dbkey == cur->lk_id.dbkey) &&
                (QSELF(pusr) == cur->lk_qusr))
                break;
        }
    }
    if ((cur == (struct lck *)NULL) || (cur->lk_curf & LKQUEUED))
    {
        /* could not find the lock entry, or we found one that was never
           granted */

        if (purged)
            MU_UNLK_PURG_CHAIN (chain);
        else
            MU_UNLK_LOCK_CHAIN (chain);

        if( lockgroup == LK_RECLK )
        {
            /* Check if there is a table lock with strength
               >= LKSHARE -- if there is assume there is no record lock
               because the table lock was of sufficient strength to
               eliminate the need for creating a record lock entry
               in the lock table.                                  */
            cur = lkTableLockGet(pcontext,plockId->table,NULL,1);
            if( cur && (int)(cur->lk_curf & LKTYPE) >= LKSHARE )
            {
                return;
            }
        }
        
        /* %Blkrels record %D not locked */        
        MSGN_CALLBACK(pcontext, svMSG013, plockId->dbkey);
        return;
    }
    if ((tmNeedRollback(pcontext,pusr->uc_task) && (purged == 0))
        && (lockgroup < LK_RGLK))
    {
        /* user is in a transaction,
	   that has changed something so just clear the keep flag */
        cur->lk_curf &= ~DSM_LK_KEEP_LOCK;

        MU_UNLK_LOCK_CHAIN (chain);

        return;
    }
    /* free our lock entry, fix first if necessary, clear uc_qwait2 */

    if (cur == first)
        first = XLCK(pdbcontext, cur->lk_qnext);
    lkfree (pcontext, prev, cur,chain);

    if (lockgroup < LK_RGLK)            /* only for record locks */
    {
        pusr->uc_qwait2 = (QLCK)0;
        if (purged)     /* this was a purged entry, so don't wake anyone */
        {
            MU_UNLK_PURG_CHAIN (chain);
            return;
        }
    }

    /* wake up the waiter(s) */

    lkwkup (pcontext, first, plockId->table, plockId->dbkey, chain);
    MU_UNLK_LOCK_CHAIN (chain);
    return;
}

/* PROGRAM: lkpurge -- called to discard all entries for the indicated
 * dbkey. It is assumed that the caller has/had  an active
 * lock for the dbkey, this lock will be purged as well.
 * The effect of this assumption is that we do not need to
 * check to see if another user has an active lock for
 * the record. This assumption is satisfied by acquiring
 * an exclusive lock for the record before calling lkpurge.
 * 
 * For now there is no direct protection against purging a
 * lock which is owned by another user. However, that user
 * would get an error message at lkrels time.
 *
 * RETURNS:  None
 */
DSMVOID
lkpurge(
        dsmContext_t    *pcontext,
        lockId_t        *plockId)     /* dbkey for RECORD being dropped
                                         from lock table */
{

    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
struct  lck     *cur;
struct  lck     *prev;
usrctl_t        *pusr;
struct  lkctl   *plkctl = pdbcontext->plkctl;
GBOOL           found_one = 0;
                int      chain;
                int      pchain;
                LONG     rc;

/*****************************************************************/
/**                                                             **/
/**     Synopsis of processing:                                 **/
/**                                                             **/
/**     Starts by finding correct chain, then scans that        **/
/**     chain looking for all entries for the dbkey. If the     **/
/**     entry is queued, utusrwake is used to wake up the       **/
/**     user.                   It's possible that the entry    **/
/**     is marked as not queued but the event has not yet       **/
/**     been reflected to the caller.                           **/
/**                                                             **/
/**     If the purged entry was to wake up in nohold or         **/
/**     limbo status, the lock is simply discarded.             **/
/**     Otherwise, it is moved to the purged chain to await     **/
/**     its lkrels call. This allows us to enforce that all     **/
/**     acquired locks are correctly released.                  **/
/**                                                             **/
/*****************************************************************/

    if (!plkctl)        /* if lkinit not yet called, ignore this call */
        return;

    if (plockId->dbkey < 0) 
        return; /* bug 93-06-24-066 */
    
    if(pdbcontext->pdbpub->slocks[EXCLCTR - 1] > 0 &&
       pdbcontext->accessEnv == DSM_SQL_ENGINE)
        /* No need to get record locks when holding the schema lock. */
        return;

    if (pdbcontext->resyncing || lkservcon(pcontext))
        return;                 /*SELF-SERVICE only */

    pusr = pcontext->pusrctl;

    TRACE2_CB(pcontext, "LKPURGE dbkey %D usrctl %p", plockId->dbkey, pusr)
    /* determine correct chain for this dbkey */
    chain = MU_HASH_CHN ((plockId->table + plockId->dbkey), LK_RECLK);
#ifdef MYDEBUG
    MSGD_CALLBACK(pcontext,
                  "%LPurging lock for table %l recid %l",
                  (LONG)plockId->table,plockId->dbkey);
#endif
    
    if (!plkctl->tableLocksOn)
    {
        lockId_t  tableLock;
        int       i;
        
        /* Have to make sure that there is an ix lock on the table
           just in case table locking gets turned on.            */
        for ( i = 0 ; i < UC_TABLE_LOCKS; i++ )
        {
            cur = XLCK(pdbcontext,pusr->uc_qtableLocks[i]);
            if ( cur && cur->lk_id.table == plockId->table )
                break;
        }
        
        if ( i == UC_TABLE_LOCKS )
        {
            tableLock.table = plockId->table;
            tableLock.dbkey = 0;

            lklocky(pcontext, &tableLock,LKIX,LK_TABLELK,NULL,NULL);
            /* Make it a limbo lock right away so that it will
               get released when the tx commits.                */
            lkrelsy(pcontext, &tableLock,LK_TABLELK);
        }
    }
    
    /* scan the chain for all entries for the dbkey */
    MU_LOCK_LOCK_CHAIN (chain);

    prev = (struct lck *)&plkctl->lk_qanchor[chain];

    for (; (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0; prev = cur)
    {
        if (plockId->table < cur->lk_id.table )
            break;       /* no entries found   */
        if (plockId->table == cur->lk_id.table && 
            plockId->dbkey < cur->lk_id.dbkey)
            break;      /* no entries found */
        if (plockId->table == cur->lk_id.table &&
            plockId->dbkey == cur->lk_id.dbkey)
        {
            found_one = 1;
            XUSRCTL(pdbcontext, cur->lk_qusr)->uc_qwait2 = (QLCK)0;
            if (cur->lk_curf & LKQUEUED)
            {
                /* wake up waiter */
                lkwakeusr (pcontext, XUSRCTL(pdbcontext, cur->lk_qusr), RECWAIT);
            }
            if (cur->lk_curf & (LKLIMBO | LKNOHOLD))
            {
                /* the entry was to be granted in limbo or nohold
                   status so simply free it. */
                lkfree (pcontext, prev, cur, chain);
            }
            else
            {
                /* put it on the purged chain */

                pchain = MU_HASH_PRG (pusr->uc_usrnum);
                MU_LOCK_PURG_CHAIN (pchain);

                prev->lk_qnext = cur->lk_qnext;
                cur->lk_qnext = plkctl->lk_qanchor[pchain];
                plkctl->lk_qanchor[pchain] = QSELF(cur);
                cur->lk_curf = LKPURGED;

                MU_UNLK_PURG_CHAIN (pchain);
#if 0
                rc = lkTableLockUpdate(pcontext, cur, chain,-1);
#endif
            }
            cur = prev;
        }
    }
    MU_UNLK_LOCK_CHAIN (chain);

    /* if no entries found and caller does not hold the exclusive
       schema lock, error */

    if (!(pusr->lktbovfw & LKOVFLOW))   /* user didn't cause overflow */
        if ((!found_one )&& (pdbpub->slocks[EXCLCTR-1] == 0))
        {
            /* But if we have an exclusive table lock it's ok
               not to find a record lock to purge.             */
            cur = lkTableLockGet(pcontext,plockId->table,NULL,1);
            if ( cur && !(cur->lk_curf & LKEXCL))
                /* lkpurge: record not locked */
                FATAL_MSGN_CALLBACK(pcontext, svFTL011);
        }
    
    return;
}

/* PROGRAM: lkTxEndForAChain -- called to process locks for a given user
   on a given chain at transaction end.

   RETURNS: None
   */
LOCALF
DSMVOID
lkTxEndForAChain (dsmContext_t *pcontext,
                  int           chain,      /* lock chain number */
                  int           *plockLeftOnChain)
{
    LONG   table;
    DBKEY  dbkey;
    struct lck *first;
    struct lck *cur;
    struct lck *prev;
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t *pusr = pcontext->pusrctl;
    struct lkctl *plkctl = pdbcontext->plkctl;

    *plockLeftOnChain = 0;
    
    for (prev = (struct lck *)&plkctl->lk_qanchor[chain],
             table = 0,
             dbkey = 0,
             first = (struct lck *)NULL;
         (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0;
         prev = cur)
    {
        if (dbkey != cur->lk_id.dbkey || table != cur->lk_id.table)
        {
            /* remember the first one in a group of identical dbkeys */

            table = cur->lk_id.table;
            dbkey = cur->lk_id.dbkey;
            first = cur;
        }
        /* skip it if it is not ours */

        if (QSELF(pusr) != cur->lk_qusr)
            continue;

        if (!(cur->lk_curf & DSM_LK_KEEP_LOCK))
        {
            /* free the current entry, fixing first if necessary */
            if (first == cur)
                first = XLCK(pdbcontext, cur->lk_qnext);
            pusr->uc_qwait2 = (QLCK)0;

            lkfree (pcontext, prev, cur, chain);
            cur = prev;
        }
        else
        {
            *plockLeftOnChain = 1;    /* still holding a lock in this chain */
	    continue;
        }
        /* scan for waiters to wake up */

        if (first)
            lkwkup (pcontext, first, table, dbkey, chain);
    }       

    return;
}

    
/* PROGRAM: lktend -- called when a transaction ends. Scans the entire
 * lock table for locks owned by the current user, deleting
 * limbo locks and purged locks, and converting exclusive
 * locks to share locks.
 *
 * RETURNS:  None
 */
DSMVOID
lktend (dsmContext_t *pcontext,ULONG64 lockChainMask _UNUSED_)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    usrctl_t *pusr;
    struct lkctl *plkctl = pdbcontext->plkctl;
    int  n;
    int firstchn;
    int lastchn;
    int lockLeftOnChain;
    int cachedTableLocks;
    GBOOL onelock;
    ULONG newbits;
    ULONG oldbits;
    ULONG chainbit;
    lck_t *plock;
    
    if (!plkctl)        /* if lkinit not yet called, ignore this call */
        return;

    pusr = pcontext->pusrctl;
    oldbits = (ULONG)pusr->uc_lkmask;

    if (pdbcontext->resyncing || lkservcon(pcontext))
        return;/*SELF-SERVICE only */
      
    newbits = 0;
    
    for( cachedTableLocks = 0; cachedTableLocks < UC_TABLE_LOCKS;
         cachedTableLocks++)
        if( pusr->uc_qtableLocks[cachedTableLocks] == 0 )
            break;
            
    if(!oldbits && !cachedTableLocks)
     /* User has no locks     */
      return;
      
    TRACE1_CB(pcontext, "LKTEND usrctl %p", pusr)

    /* loop over all the record lock and table chains but not purged */

    firstchn = plkctl->lk_chain[LK_RECLK];
    lastchn = firstchn + plkctl->lk_hash - 1;
    
    if (pdbpub->argspin)
    {
        /* Lock chains have individual locks */
        onelock = 0;
    }
    else
    {
        /* All the lock chains use the same lock */
        onelock = 1;
        MU_LOCK_LOCK_CHAIN (firstchn);
    }
    for (n = firstchn; n <= lastchn; n++)
    {
        /* skip empty chains to avoid chain locks */

        if (!(plkctl->lk_qanchor[n]))
            continue;

        /* only need to look at chains we used */

        chainbit = (ULONG)1 << (n & 0x1F);

        if ((oldbits & chainbit) == 0)
            continue;

        if (!onelock)
	{
            MU_LOCK_LOCK_CHAIN (n);
	}
        /* Process the chain                             */
        lkTxEndForAChain(pcontext,n,&lockLeftOnChain);

        if (!onelock)
        {
            MU_UNLK_LOCK_CHAIN (n);
        }

        if( lockLeftOnChain )
            newbits |= chainbit;
        
    }

    /* remember what chains we still hold locks in */

    pusr->uc_lkmask = newbits;
    
    /* Now handle table locks                  */
    if( cachedTableLocks < (UC_TABLE_LOCKS-1)  )
    {
        for( n = 0; n < UC_TABLE_LOCKS; n++ )
        {
            plock = XLCK(pdbcontext,pusr->uc_qtableLocks[n]);
            if( plock )
            {
                firstchn = MU_HASH_CHN(plock->lk_id.table,LK_TABLELK);
                if (!onelock)
		{
                    MU_LOCK_LOCK_CHAIN(firstchn);
		}
                lkTxEndForAChain(pcontext,firstchn,&lockLeftOnChain);
                if (!onelock)
		{
                    MU_UNLK_LOCK_CHAIN(firstchn);
		}
            }
        }
    }
    else
    {
        firstchn = plkctl->lk_chain[LK_TABLELK];
        lastchn =  plkctl->lk_chain[LK_TABLELK+1];
        for (n = firstchn; n < lastchn; n++)
        {
            /* skip empty chains to avoid chain locks */

            if (!(plkctl->lk_qanchor[n]))
                continue;

            if (!onelock)
            {
                MU_LOCK_LOCK_CHAIN (n);
            }

            /* Process the chain                             */
            lkTxEndForAChain(pcontext,n,&lockLeftOnChain);

            if (!onelock)
            {
                MU_UNLK_LOCK_CHAIN (n);
            }
        }
    }
    
    if (onelock)
    {
        MU_UNLK_LOCK_CHAIN (firstchn);        
    }

    return;
}

/* PROGRAM: lkrend -- called as a protection against "forgotten" locks.
 * Scans the entire lock table for locks held by the
 * current user.
 * 
 * If flag is 0, lkrend is being called by child death
 * processing. Any found locks are deleted silently.
 * Otherwise, the call is a result of the schema share lock
 * being released for the user. Although this check is fairly
 * expensive, it does protect against introduction of bugs.
 *
 * RETURNS:  None
 */
DSMVOID
lkrend(
        dsmContext_t    *pcontext,
        int              flag)
                        /* if zero, silently release any locks
                        held by caller. This is used by child/
                        client death recovery. */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    usrctl_t        *pusr;
    struct  lck     *first;
    struct  lck     *cur;
    struct  lck     *prev;
    struct  lkctl   *plkctl = pdbcontext->plkctl;
    LONG             table = 0;
    DBKEY            dbkey;
    int              n;
    int             lastchn;
    int             lastrecchn;

    /* if lkinit not yet called, ignore lkpurge calls */
    if (!plkctl)
        return;

    /* if we are in abnormal shutdown, then return */
    if (pdbcontext->pdbpub->shutdn )
        return;

    if (pdbcontext->resyncing || lkservcon(pcontext) || !pcontext->pusrctl)
        return;/*SELF-SERVICE only */

    pusr = pcontext->pusrctl;

    /* if we have never gotten a lock, no need to call lkread */
    if (pusr->uc_lockinit == 0)
        return;

    TRACE2_CB(pcontext, "LKREND flag %i usrctl %p", flag, pusr)

    if (pusr->uc_task)
    {
        /* 95-08-16-005 - transaction active */

        /* lkrend called with active transaction */
        MSGN_CALLBACK(pcontext, svMSG003,((int)pusr->uc_task));
    }


    /* loop over all chains */

    lastchn = plkctl->lk_chain[LK_MAXTYPE] + plkctl->lk_hash - 1;
    lastrecchn = plkctl->lk_chain[LK_PURLK] + plkctl->lk_hash - 1;

    for (n = 0; n <= lastchn; n++)
    {
        MU_LOCK_LOCK_CHAIN (n);

        for (prev = (struct lck *)&plkctl->lk_qanchor[n],
                dbkey = 0,
                first = (struct lck *)NULL;
            (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0;
            prev = cur)
        {
            if (dbkey != cur->lk_id.dbkey)
            {
                table = cur->lk_id.table;
                dbkey = cur->lk_id.dbkey;
                first = cur;
            }
            if (QSELF(pusr) != cur->lk_qusr)
                continue;       /* skip to entry owned by user */

            /* found entry for user. fatal error if flag != 0 */

            /* another check added. if a lock has the LKNOHOLD flag     */
            /* set, it is a tentative lock, which is ok to delete,      */
            /* ie don't complain - gpf 12/2/86                          */

            /* 95-08-16-005 */
            if (flag && !(cur->lk_curf & LKNOHOLD))
            {
                /* lkrend: forgotten record lock, recid . */
                MSGN_CALLBACK(pcontext, svMSG001, dbkey);
                /* Lock type %d recid */
                MSGN_CALLBACK(pcontext, svMSG004, ((int)cur->lk_curf), dbkey);
                if (first->lk_curf & LKEXCL)
                {
                    /* Can't release exclusive locks in trans, skip */

                    if (pusr->uc_task) continue;
                }
            }


            /* silently free lock */

            if (cur->lk_curf & LKPURGED)
                first = (struct lck *)NULL;
            else if (first == cur)
                first = XLCK(pdbcontext, cur->lk_qnext);

            lkfree (pcontext, prev, cur, n);

            cur = prev;

            /* scan for waiters to wake up */
            if (n <= lastrecchn) /* only for record locks, not for other locks*/
                pusr->uc_qwait2 = (QLCK)0;

            lkwkup (pcontext, first, table, dbkey, n);
        }

        MU_UNLK_LOCK_CHAIN (n);
    }

    /* no need to call lkrend again until we actually have gotton a lock */
    pusr->uc_lockinit = 0;

    return;
}

/* PROGRAM: lkcncl -- called to cancel the queued lock request for the
 * current user. If the queued request is share or exclusive,
 * simply purge it. Else, if the queued request is an
 * upgrade, restore the previously existing status (share
 * plus possibly limbo). Dies a horrible death if no
 * request is queued.
 *
 * RETURNS: None
 */

/*** this is only for RECORD locks */
DSMVOID
lkcncl (dsmContext_t *pcontext)
{
        dbcontext_t     *pdbcontext = pcontext->pdbcontext;
        usrctl_t        *pusr = pcontext->pusrctl;
        struct  lck     *firstMatch;
        struct  lck     *cur;
        struct  lck     *prev;
        struct  lkctl   *plkctl = pdbcontext->plkctl;
        LONG             table;
        DBKEY            recid;
        int              chain;
        int              hashGroup;
        
    if (!plkctl)
    {
        /* There is no lock table. Either we are in single-user mode or
           still initializing */

        return;
    }
    if (pdbcontext->resyncing) return; /* resyncing in progress */

    TRACE1_CB(pcontext, "LKCNCL usrctl %p", pusr)

    firstMatch = (LCK *)NULL;
    prev = (LCK *)NULL;
    cur = (LCK *)NULL;

    /* Get recid of the lock we are waiting for */

    table = pusr->waitArea;
    recid = pusr->uc_wait1;
    if( recid == 0 )
        hashGroup = LK_TABLELK;
    else
        hashGroup = LK_RECLK;
    
    chain = MU_HASH_CHN((table + recid), hashGroup);

    MU_LOCK_LOCK_CHAIN (chain);

    /* Cant touch uc_qwait2 until *after* we lock the lock chain. If we
       try, someone else can delete the lock request and change uc_qwait2
       before we lock it */

    if (pusr->uc_qwait2)
    {
        /* We are still queued for a lock request */

        cur = XLCK(pdbcontext, pusr->uc_qwait2);
        prev = (struct lck *)&plkctl->lk_qanchor[chain];

        for (;;)
        {
            cur = XLCK(pdbcontext, prev->lk_qnext);
            if (cur == (struct lck *)NULL)
            {
                /* No more entries on chain and we did not find the one we
                   were looking for */

                MU_UNLK_LOCK_CHAIN (chain);
                /* lkcncl: invalid call */
                FATAL_MSGN_CALLBACK(pcontext, svFTL014);
            }
            if (recid == cur->lk_id.dbkey && table == cur->lk_id.table)
            {
                /* matching recid */
             
                if (firstMatch == (struct lck *)NULL)
                {
                    /* remember where the first match is */

                    firstMatch = cur;
                }
                if (QSELF(pusr) == cur->lk_qusr)
                {
                    /* This is ours */

                    if (cur != XLCK(pdbcontext, pusr->uc_qwait2))
                    {
                        /* Oops! Not the one we were waiting for */

                        MU_UNLK_LOCK_CHAIN (chain);
                        /* lkcncl: invalid call */
                        FATAL_MSGN_CALLBACK(pcontext, svFTL014);
                    }
                    /* The right one. cancel the wait now */

                    if ((cur->lk_curf & LKUPGRD))
                    {
                        /* We were waiting for an upgrade.
                           Change the lock state back, downgrading it. */

                        if (cur->lk_prevf == 0)
                        {
                            /* Waiting for upgrade, but we did not have
                               a lock to upgrade. This is never supposed
                               to happen */

                            MU_UNLK_LOCK_CHAIN (chain);
                            /* lkcncl: invalid downgrade */
                            FATAL_MSGN_CALLBACK(pcontext, svFTL015);
                        }
                        cur->lk_curf = cur->lk_prevf;
                        cur->lk_prevf = 0;
                    }
                    else
                    {
                        /* We were waiting for a regular request,
                           not an upgrade. Delete the request.
                           Update pointer to first match if needed */

                        if (firstMatch == cur)
                            firstMatch = XLCK(pdbcontext, cur->lk_qnext);
                        pusr->uc_qwait2 = (QLCK)NULL;
                        lkfree (pcontext, prev, cur, chain);
                    }
                    break;
                }
            }
            /* Advance to the next entry */

            prev = cur;
        }
    }
    /* Clear the wait information in user table */

    pusr->uc_wait = 0;
    pusr->waitArea = 0;
    pusr->uc_wait1 = 0;
    pusr->uc_qwait2 = (QLCK)NULL;
    pusr->usrwake = 0;

    /* clear the semaphore in case someone has incremented it already */

    latSemClear (pcontext);

    if (firstMatch)
    {
        /* See if we can wake anyone else who is waiting for the same lock */

        lkwkup (pcontext, firstMatch, table, recid, chain);
    }
    MU_UNLK_LOCK_CHAIN (chain);

    return;
}

/* PROGRAM: lkWakeUp -- local routine called to see if any queued lock
 * requests for a given table can be granted.
 *
 */
 
LOCALF DSMVOID
lkWakeUp (
        dsmContext_t    *pcontext,
        struct lck      *first,    /* first entry for dbkey still
                             in lock chain, else NULL */
        LONG             table,          /* area containing released record */
        DBKEY            dbkey,         /* dbkey of released record */
        int              chain _UNUSED_)
{
        struct  lck     *plck;
        int     maxLockStrength;
        
/*****************************************************************/
/**                                                             **/
/**     Synopsis of processing:                                 **/
/**                                                             **/
/**                                                             **/
/*****************************************************************/


    /* Find the highest strength granted lock.                  */
    maxLockStrength = 0;
    for ( plck = first ;
          (plck != (struct lck *)NULL) && (dbkey == plck->lk_id.dbkey) &&
         (table == plck->lk_id.table) ;
          plck = XLCK(pdbcontext, plck->lk_qnext))
    {
        /* skip expired locks. */
        if (plck->lk_curf & LKEXPIRED)
            continue;

        if(!(plck->lk_curf & LKQUEUED))
            if ( (int)(plck->lk_curf & LKTYPE) > maxLockStrength )
                maxLockStrength = plck->lk_curf & LKTYPE;
    }
    
    /* Wake up waiters until lock conflict or end of chain for this
       lock id. */
    for ( plck = first ;
          (plck != (struct lck *)NULL) && (dbkey == plck->lk_id.dbkey) &&
         (table == plck->lk_id.table) ;
          plck = XLCK(pdbcontext, plck->lk_qnext))
    {
        /* skip expired locks. */
        if (plck->lk_curf & LKEXPIRED)
            continue;

        if((plck->lk_curf & LKQUEUED))
        {
            if( !lkLockConflict(plck->lk_curf,maxLockStrength))
            {
                plck->lk_curf &= ~(LKQUEUED | LKUPGRD);
                /* wake up the user */
                lkwakeusr (pcontext, XUSRCTL(pdbcontext, plck->lk_qusr), RECWAIT);
            }
            else
            {
                /* Waking next waiter would result in lock conflict
                   so we're done   */
                break;
            }
            if ( (int)(plck->lk_curf & LKTYPE) > maxLockStrength )
                maxLockStrength = plck->lk_curf & LKTYPE;
        }
    }
    
    return;
}

/* PROGRAM: lkwkup -- local routine called to see if any queued lock
 * requests for a given dbkey can be granted.
 * 
 * Dbkey identifies the record which has been released by
 * the current user, first is either a pointer into the lck
 * chain which contained the freed entry, or NULL.
 * 
 * If first is null, the freed record was the only instance
 * of dbkey in the chain. This test is performed here to save
 * code. Otherwise, first points to the first lck entry for
 * the dbkey.
 * 
 * 
 * If it is not a record lock it does not change the flags,
 * this will be done by the awaken user himself in the right
 * sequence to prevent deadlocks.
 *
 * RETURNS:  None
 */
 
LOCALF DSMVOID
lkwkup (
        dsmContext_t    *pcontext,
        struct lck      *first,    /* first entry for dbkey still
                             in lock chain, else NULL */
        LONG             table,          /* area containing released record */
        DBKEY            dbkey,         /* dbkey of released record */
        int              chain)
{
        dbcontext_t     *pdbcontext = pcontext->pdbcontext;
        struct  lck     *plck;
        int     type;
        struct  lkctl   *plkctl = pdbcontext->plkctl;

/*****************************************************************/
/**                                                             **/
/**     Synopsis of processing:                                 **/
/**                                                             **/
/**     If the first entry in the chain for dbkey is an         **/
/**     exclusive waiter, scan the chain looking for any        **/
/**     other entries which own the record, i.e. are not        **/
/**     queued. If another active entry is found, return        **/
/**     without waking up the exclusive waiter. Otherwise       **/
/**     wake up the exclusive waiter only.                      **/
/**                                                             **/
/**     If the first entry is an active exclusive entry,        **/
/**     return, no one can be awakened.                         **/
/**                                                             **/
/**     Else the first entry is either an active or queued      **/
/**     share entry. If it is queued, activate it. Then         **/
/**     scan the chain activating any share waiters and         **/
/**     stop when either the chain end or an exclusive          **/
/**     waiter is encountered.                                  **/
/**                                                             **/
/*****************************************************************/

    if (first == (struct lck *)NULL || first->lk_id.dbkey != dbkey ||
        first->lk_id.table != table)
        return;

    while ( (first->lk_curf & LKEXPIRED) )
    {
        /* The current lock is marked as expired (it has timed out
         * and is in the process of cleaning up).
         * We will therefore ignore it.  If there is another
         * lock in the que continue with it.
         */
        first = XLCK(pdbcontext, first->lk_qnext);
        if ((first == (struct lck *)NULL) || 
            (first->lk_id.dbkey != dbkey) ||
            (first->lk_id.table != table))
            return;
    }

    if( dbkey == 0 )
    {
        
        /* A table lock so use wakeup protocol that can handle
           Intent Locks as well as share and exclusive.         */
        lkWakeUp(pcontext,first,table,dbkey,chain);
        return;
    }
    
    type = RECWAIT;
    if (chain >= plkctl->lk_chain[LK_RGLK])
        type = RGWAIT;

    if ((first->lk_curf & (LKQUEUED | LKEXCL)) == (LKQUEUED | LKEXCL))
    {
        /* first entry is queued exclusive request */
        /* cant grant it if the next lock is share */

        plck = XLCK(pdbcontext, first->lk_qnext);
        if (plck && plck->lk_id.dbkey == dbkey && plck->lk_id.table == table)
        {
            /* if someone had active share and upgraded, don't wakeup */
            if (plck->lk_curf & LKUPGRD)
                return;

            /* if someone has active (share), don't wakeup */
            if (!(plck->lk_curf & LKQUEUED))
                return;
        }
        /* grant it - wake up exclusive waiter */

        if (type == RECWAIT)            /* for records */
        {
            first->lk_curf &= ~(LKQUEUED | LKUPGRD);
             /* wake up the user */
            lkwakeusr (pcontext, XUSRCTL(pdbcontext, first->lk_qusr), type);
            return;
        }
        /* record-get lock. We want to grant the
           lock and then wake up the requestor, who will then be
           exclusive holder of the lock */

        first->lk_curf &= LKTYPE;
        lkwakeusr (pcontext, XUSRCTL(pdbcontext, first->lk_qusr), type);
        return;

    }
    if (first->lk_curf & LKEXCL)
        /* first entry active exclusive so we cant grant any */
        return;

    /* first entry active share, wake up other shares until excl */
    for (; (first != (struct lck *)NULL) && (dbkey == first->lk_id.dbkey) &&
         (table == first->lk_id.table) ;
          first = XLCK(pdbcontext, first->lk_qnext))
    {
        if (first->lk_curf & LKEXCL)
            return;

        if ((first->lk_curf & LKQUEUED) &&
            !(first->lk_curf & LKEXPIRED) )
        {
            /* wake up the user */
            first->lk_curf &= ~(LKQUEUED | LKUPGRD);
            lkwakeusr (pcontext, XUSRCTL(pdbcontext, first->lk_qusr), type);
        }
    }
    return;
}



/* PROGRAM: lktbresy -- resyn the user as a result of lock
 * table overflow.
 *
 * RETURNS:  int - LKTBFULL - lock table full
 *                 DSM_S_CTRLC
 */
LOCALF int
lktbresy(
        dsmContext_t    *pcontext _UNUSED_,
        usrctl_t        *pusr _UNUSED_)
{
    return DSM_S_LKTBFULL; 
}

/* PROGRAM: lkchecky -- does the work of lkcheck for a specific lock chain 
 *
 * RETURNS: struct lck * - pointer to lock structure
 *          (NULL) - premature call or entry not found
 *                   nullify pointer
 *          (cur)  - pointer to lock entry
 */
LOCALF struct lck *
lkchecky(
        dsmContext_t    *pcontext,
        lockId_t        *plockId) /* Extended dbkey of RECORD to release */
{
        dbcontext_t     *pdbcontext = pcontext->pdbcontext;
        struct  lck     *cur;
        struct  lck     *prev;
        usrctl_t        *pusr;
        struct  lkctl   *plkctl = pdbcontext->plkctl;
                int      pchain;
                int      chain; /* the number of the lock chain to use*/

    if (!plkctl)        /* if lkinit not yet called, ignore this call */
        return (NULL);

    if (plockId->dbkey < 0) return (NULL); /* bug 93-06-24-066 */

    pusr = pcontext->pusrctl;

    /* determine correct chain for this dbkey */
    chain = MU_HASH_CHN ((plockId->table + plockId->dbkey), LK_RECLK);

    /* locate the entry to release, set pointers */
    MU_LOCK_LOCK_CHAIN (chain);

    prev = (struct lck *)&plkctl->lk_qanchor[chain];
    for (; (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0; prev = cur)
    {
        if (plockId->table < cur->lk_id.table ||
            ( plockId->table == cur->lk_id.table &&
             plockId->dbkey < cur->lk_id.dbkey))
        {
            /* entry not found */
            break;
        }

        if (plockId->table == cur->lk_id.table &&
            plockId->dbkey == cur->lk_id.dbkey)
        {
            if (QSELF(pusr) == cur->lk_qusr)    /* found the entry */
            {
                MU_UNLK_LOCK_CHAIN (chain);
                return (cur);
            }
        }
    }

    MU_UNLK_LOCK_CHAIN (chain);

    /* lock was not in its appropriate active chain, try the purged
       chain. note the purged chain is not sorted */

    pchain = MU_HASH_PRG (pusr->uc_usrnum);

    MU_LOCK_PURG_CHAIN (pchain);

    prev = (struct lck *)&plkctl->lk_qanchor[pchain];
    for (; (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0; prev = cur)
    {
        if ((plockId->table == cur->lk_id.table) &&
            (plockId->dbkey == cur->lk_id.dbkey) && 
            (QSELF(pusr) == cur->lk_qusr))
        {
            /* remove the PURGED entry!!!! */
            lkfree (pcontext, prev, cur, pchain);
            break;
        }
    }
    MU_UNLK_PURG_CHAIN (pchain);

    return (NULL);              /* not found */
}

/* PROGRAM: lkcheck --  checks if a lock exists in the lock table
 *                      for the given dbkey for the checking user.
 * If active lock exist - return a pointer to it.
 * If PURGED lock exist - remove it and return NULL.
 * Return in *pplck
 *        NULL - if no lock exists in the lock table
 *        pointer to the lock entry - if found in the lock table
 *
 * RETURNS: None
 */
DSMVOID
lkcheck(
        dsmContext_t    *pcontext,
        lockId_t        *plockId,    /* extended dbkey of RECORD to release */
        struct lck      **pplck)        /* return here pointer to lock */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
struct  lck     *cur;

    *pplck = NULL;

    if (!pdbcontext->plkctl)        /* if lkinit not yet called, ignore this call */
        return;

    if (plockId->dbkey < 0) return; /* bug 93-06-24-066 */

    if (pdbcontext->resyncing || lkservcon(pcontext))
        return;                 /*SELF-SERVICE only */

    cur = lkchecky(pcontext, plockId);
    *pplck = cur;
    return;
}

/* PROGRAM: lkfree -- free a lock entry, remove it from its chain and put
 * it on the free chain
 *
 * RETURNS: None
 */
LOCALF DSMVOID
lkfree (
        dsmContext_t    *pcontext,
        struct lck      *prev,
        struct lck      *cur,
        int              chain)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    struct  lkctl   *plkctl = pdbcontext->plkctl;
    usrctl_t        *pusr;
    int              i;
#ifdef MYDEBUG
    MSGD_CALLBACK(pcontext,
                  "%Lfreeing lock for %l %l",
                  cur->lk_id.table,cur->lk_id.dbkey);
#endif    
    /* the chain the lock entry is on must be locked by caller !!!! */

#if 0
    if( chain < plkctl->lk_chain[LK_RECLK + 1])
    {
        /* Update the table lock for record locks only at this point */
        /* Purge locks updated the table lock when the lock was
           put on the purge chain.                                   */
        i = lkTableLockUpdate(pcontext,cur,chain, -1);
    }
#endif
    if( cur->lk_id.dbkey == 0 )
    {
        if ((int)(cur->lk_curf & LKTYPE) >= LKSHARE)
        {
            plkctl->numTableLocks--;
            if (plkctl->numTableLocks == 0)
                time(&plkctl->timeOfLastTableLock);
        }
        pusr = XUSRCTL(pdbcontext,cur->lk_qusr);
        /* Clear pointer to this table lock in the user control
         of the user that owned the lock    */
        for( i = 0; i < UC_TABLE_LOCKS; i++)
        {
            if( pusr->uc_qtableLocks[i] == QSELF(cur))
            {
                pusr->uc_qtableLocks[i] = 0;
                break;
            }
        }
    }

    /* clear the reference count.  note that since lk_refCount is a union, this 
       clears the lock strength counts too */
    cur->lk_id.dbkey = 0;
    cur->lk_id.table = 0;
    cur->lk_qusr = 0;
    cur->lk_prevf = 0;
    cur->lk_curf = 0;
    cur->lk_refCount.referenceCount = 0;


    /* unlink from lock chain */

    prev->lk_qnext = cur->lk_qnext;
    
    MU_LOCK_LOCK_TABLE ();

    /* put on the front of the free list */

    cur->lk_qnext = plkctl->lk_qfree;

    plkctl->lk_qfree = QSELF(cur);

    /* update count of in-use lock entries */

    plkctl->lk_cur--;

    MU_UNLK_LOCK_TABLE ();

}

/* PROGRAM: lkrestore -- restore a record lock obtained by the server to its
 * previous state after the server decided that it doesn't satisfy the
 * query or release the lock all together if it it was not previously
 * locked.
 * 
 * Used after selection by the server that failed, and by the new query
 * engine.
 * 
 *
 * RETURNS:  None
 */
DSMVOID
lkrestore(
        dsmContext_t    *pcontext,
        lockId_t        *plockId,       /* restore the lock for this record */
        dsmMask_t        lockMode)      /* mode the restore is from */
{
        dbcontext_t     *pdbcontext = pcontext->pdbcontext;
        LCK             *cur;
        LCK             *prev = NULL;
        LCK             *first = NULL;
        LKCTL           *plkctl = pdbcontext->plkctl;
        int             chain = 0;
        usrctl_t        *pusr;
        int              hashGroup;
        int              lockedChain = 0;
        
    if (pdbcontext->resyncing)
        return;         /* resyncing in progress */

    if (!plkctl)        /* if lkinit not yet called, ignore this call */
        return;

    if(pdbcontext->pdbpub->slocks[EXCLCTR - 1] > 0 &&
       pdbcontext->accessEnv == DSM_SQL_ENGINE )
        /* No need to get record locks when holding the schema lock. */
        return;

    if (plockId->dbkey < 0) return; /* bug 95-10-18-016 */

    if(SCTBL_IS_TEMP_OBJECT(plockId->table))
       return;

    TRACE1_CB(pcontext, "LKRESTORE dbkey %D", plockId->dbkey);

    pusr = pcontext->pusrctl;

    if( plockId->dbkey == 0 )
        hashGroup = LK_TABLELK;
    else
        hashGroup = LK_RECLK;

 
    cur = XLCK(pdbcontext, pusr->uc_qwait2);
    if (cur && cur->lk_qusr != QSELF(pusr))    /* wrong lock entry */
        FATAL_MSGN_CALLBACK(pcontext, muFTL002);

    /* We have to find the lock entry in the lock table
       if we are going to free the lock unless it is the last
       lock we got (uc_qwait2) and it has a previous state which
       we'll be restoring rather than freeing the lock.  */
    if(!(cur && cur->lk_id.table == plockId->table &&
       cur->lk_id.dbkey == plockId->dbkey &&
       cur->lk_prevf))
    {
        /* find the lock entry and the previous entry */
        chain = MU_HASH_CHN((plockId->table + plockId->dbkey), hashGroup);

        MU_LOCK_LOCK_CHAIN(chain);
        lockedChain = 1;
        prev = (struct lck *)&plkctl->lk_qanchor[chain];

        for(; (cur = XLCK(pdbcontext, prev->lk_qnext)) != 0; prev = cur)
        {
            if (cur->lk_id.table != plockId->table || 
                cur->lk_id.dbkey != plockId->dbkey)
                continue;
            if (first == (LCK *)NULL)
                first = cur;
            if (QSELF(pusr) == cur->lk_qusr)
                break;
        }       

        if (cur == (struct lck *)NULL)
        {       
            if ( lockedChain )
            {
                MU_UNLK_LOCK_CHAIN (chain);
            }
            if( hashGroup == LK_RECLK )
            {
                /* Check if there is a table lock with strength
                   >= LKSHARE -- if there is assume there is no record lock
                   because the table lock was of sufficient strength to
                   eliminate the need for creating a record lock entry
                   in the lock table.                                  */
                cur = lkTableLockGet(pcontext,plockId->table,NULL,1);
                if( cur && (int)(cur->lk_curf & LKTYPE) >= LKSHARE )
                {
                    return;
                }
            }
            FATAL_MSGN_CALLBACK(pcontext, muFTL005, plockId->dbkey, plockId->table);
        }
    }

    /* defensive code; this should never happen */
    if ((int)(cur->lk_curf & LKTYPE) < (int)(cur->lk_prevf & LKTYPE))
    {
        if ( lockedChain )
        {
            MU_UNLK_LOCK_CHAIN(chain);
        }
        return;
    }

    FASSERT_CB(pcontext, (cur->lk_curf & LKNOHOLD) == 0, 
               "lkrestore: not holding the lock");

    if ( cur->lk_curf & LKSHARE )
    {
        pcontext->pdbcontext->pdbpub->shrfndlkCt++;
        pusr->uc_shrfindlkCt++;
 
    }
    else if ( cur->lk_curf & LKEXCL )
    {
       pcontext->pdbcontext->pdbpub->exclfndlkCt++;
       pusr->uc_exclfindlkCt++;
    }
    
    if (cur->lk_prevf)                  /* got a previous lock */
    {
        if ((cur->lk_prevf & LKTYPE) == (cur->lk_curf & LKTYPE))
        {
            /* previous lock is same strength as the current lock - we don't */
            /* need a shared memory lock to restore the previous lock. */
            cur->lk_curf = cur->lk_prevf;       /* restore previous lock */
            if( pdbcontext->accessEnv == DSM_4GL_ENGINE)
                cur->lk_prevf = 0; /* don't keep previous lock state */
            else if (pdbcontext->accessEnv == DSM_SQL_ENGINE)
            {
                if ((cur->lk_id.dbkey != 0) &&
                    (cur->lk_refCount.lockCounts.exclusiveLocks + 
                    cur->lk_refCount.lockCounts.shareLocks == 2))
                {
                    /* only one lock left , don't previous lock state */
                    cur->lk_prevf = 0;
                }
                
            }
            /* decrement lock strength count for record locks
               (not table locks) */
            if (cur->lk_id.dbkey != 0)
            {
                if (lockMode & LKEXCL )
                {
                    cur->lk_refCount.lockCounts.exclusiveLocks--;
                }
                else if (lockMode & LKSHARE)
                {
                    cur->lk_refCount.lockCounts.shareLocks--;
                }
            }
            
            if( lockedChain )
            {
                MU_UNLK_LOCK_CHAIN(chain);
            }
            return;
        }
        
        /* decrement lock strenght counts for records */
        if (cur->lk_id.dbkey != 0)
        {
            if (cur->lk_refCount.lockCounts.exclusiveLocks == 1)
            {
                FASSERT_CB(pcontext, (cur->lk_refCount.lockCounts.shareLocks != 0), 
                    "lkrestore: invalid downgrade");
                if (cur->lk_refCount.lockCounts.exclusiveLocks + 
                    cur->lk_refCount.lockCounts.shareLocks == 2)
                {
                    /* only one lock left, clear the previous lock state */
                    cur->lk_prevf = 0;
                }
            }
            
            /* decrement lock strength count */
            if (lockMode & LKEXCL )
            {
                cur->lk_refCount.lockCounts.exclusiveLocks--;
            }
            else if (lockMode & LKSHARE)
            {
                cur->lk_refCount.lockCounts.shareLocks--;
            }
        }

        /* are we down grading to a share */
        if ((cur->lk_refCount.lockCounts.exclusiveLocks == 0) &&
            (cur->lk_refCount.lockCounts.shareLocks > 0))
        {
            cur->lk_curf &= ~LKEXCL;
            cur->lk_curf |= LKSHARE;
        }
        
        /* this is a downgrade - wake up waiters for the lock */
        chain = MU_HASH_CHN((plockId->table + plockId->dbkey), hashGroup);

        if ( !lockedChain )
        {    
            MU_LOCK_LOCK_CHAIN(chain);
            lockedChain = 1;
        }
        

        first = (LCK *)&plkctl->lk_qanchor[chain];
        while ((first = XLCK(pdbcontext, first->lk_qnext)) != (LCK *)0)
            if (first->lk_id.dbkey == plockId->dbkey && 
                first->lk_id.table == plockId->table)
                break;
        if (first == 0)
        {
            if ( lockedChain )
            {
                MU_UNLK_LOCK_CHAIN (chain);
            }
            FATAL_MSGN_CALLBACK(pcontext, muFTL004);
        }
        if( pdbcontext->accessEnv == DSM_4GL_ENGINE)
        {
            cur->lk_curf = cur->lk_prevf;   /* restore previous lock */
            cur->lk_prevf = 0;
        }
    }
    else    /* there is no previous lock.  Free the lock. */
    {
        if (first == cur)
            first = XLCK(pdbcontext, cur->lk_qnext);
        if (prev)
            lkfree(pcontext, prev, cur, chain);
    }

    pusr->uc_wait = 0;
    pusr->uc_qwait2 = (QLCK)NULL;
    pusr->usrwake = 0;
    /* scan for waiters to wake up */
    lkwkup (pcontext, first, plockId->table, plockId->dbkey, chain);

    if ( lockedChain )
    {
        MU_UNLK_LOCK_CHAIN (chain);
    }
}  /* lkrestore */


/* PROGRAM: lkclrels -- release a record locked by the server if the client
 * would have released it if it handled the record, i.e., if the client
 * does not have another lock on it.
 * 
 * Called after a CAN-FIND with a shared lock is executed by the client,
 * or for auto-release locks for a presort/preselect pass.
 * 
 * As in lkrestore, the lock being released must be pointed to by
 * pusr->uc_qwait2 (see comment there).
 * 
 * RETURNS:  None
 */
DSMVOID
lkclrels(
        dsmContext_t    *pcontext,
        lockId_t        *plockId)    /* release the lock for this record */
{

    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    LCK         *cur;
    usrctl_t    *pusr;

    if (pdbcontext->resyncing)
        return;         /* resyncing in progress */

    if (!pdbcontext->plkctl)        /* if lkinit not yet called, ignore this call */
        return;

    if (plockId->dbkey < 0)
        return; /* bug 93-06-24-066 */

    TRACE1_CB(pcontext, "LKCLRELS dbkey %D", plockId->dbkey);

    pusr = pcontext->pusrctl;

    FASSERT_CB(pcontext, pusr->uc_qwait2, "lkclrels: uc_qwait2 is NULL");
    if (pusr->uc_qwait2 == (QLCK)NULL)
    {
        MSGN_CALLBACK(pcontext, muMSG005);
        return;
    }

    cur = XLCK(pdbcontext, pusr->uc_qwait2);
    if (cur->lk_qusr != QSELF(pusr))    /* wrong lock entry */
        FATAL_MSGN_CALLBACK(pcontext, muFTL006);

    /* if we have no previous lock, or the previous lock is in limbo or */
    /* nohold state, the client has no other locks on this record and */
    /* would have released it if it did this */
    if (!(cur->lk_prevf & LKTYPE) || (cur->lk_prevf & (LKLIMBO | LKNOHOLD)))
        lkrels(pcontext, plockId);
}

/* PROGRAM: lkwait -- used when a requested lock is not available.
 * it notifies the user about the holder of the record
 * lock if the wait is a long wait (on record, schema or task),
 * it unlocks the database service and waits for either
 * the lock to be free or for a DSM_S_CTRLC.
 *
 * RETURNS:
 *     DSM_S_CTRLC - the user was tired of waiting and typed DSM_S_CTRLC
 *     0 - the record was released, try to lock it again
 */
int
lkwait (
        dsmContext_t    *pcontext,
        int              msgNum, /* message number to use for lock msg */
        TEXT            *pname) /* file name */
{

        dbcontext_t     *pdbcontext = pcontext->pdbcontext;
        usrctl_t        *pholder;
        int     waitCode;
        int     ret;
        TEXT    *pusr;
        TEXT    *ptty;

    if (msgNum == 0) msgNum = LW_RECLK_MSG;

    waitCode = (int)pcontext->pusrctl->uc_wait;
    if ((waitCode == 0) || (waitCode > TSKWAIT))
    {
        /* invalid wait code                              */
        FATAL_MSGN_CALLBACK(pcontext, dbFTL011, (int)waitCode);
    }
#if MTSANITYCHECK
    if (pcontext->pusrctl->uc_latches)
    {
        /* can not be holding any latches or deadlock will occur */

        pdbcontext->pdbpub->shutdn = DB_EXBAD;
        FATAL_MSGN_CALLBACK(pcontext, mtFTL016, pcontext->pusrctl->uc_latches);
    }
#endif
    pholder = pdbcontext->p1usrctl + pcontext->pusrctl->uc_wusrnum;
    pusr = XTEXT(pdbcontext, pholder->uc_qusrnam);
    ptty = XTEXT(pdbcontext, pholder->uc_qttydev);

    ret = latLockWait (pcontext, waitCode, msgNum, pname, pusr, ptty);

    return (ret);

}  /* end lkwait */


/* PROGRAM: lkrclk - lock record, the record is lock EXCL
 * if any is unavailable free all the others and wait.
 * If fail (return not 0) all the locks are released
 * before the return.
 * RETURNS:
 * 0           OK
 * not 0       the fail code that is returned by the lklockx
 *             (the record lock fail code) 
 */
int
lkrclk (
        dsmContext_t    *pcontext,
        lockId_t        *plockId, /* dbkey of record */
        TEXT             *pname) /*refname for rec in use message (child stub)*/
{
        int     ret;

    for (;;)
    {
        /* try to lock the record */

        ret = lklocky (pcontext, plockId, LKEXCL, LK_RECLK,
                       pname, (LKSTAT *)NULL);
        if (ret != DSM_S_RQSTQUED) return (ret);

        /* some servers cant wait for record locks */
        if (!pcontext->pusrctl->uc_block)
            return ret;

        /* wait for the record lock to be released */
        ret = lkwait (pcontext, LW_RECLK_MSG, pname);
        if (ret != 0) return (ret);
    }

}  /* end lkrclk */


/* PROGRAM: lkrglock -- called to lock a record during "record get".
 * It calls lklocky to do the work. The record is locked
 * similar to a record lock but in a separate lock chain set.
 * 
 * Unlike the regular record lock, that is a long term
 * lock, until the end of the transaction, this is a short
 * term lock, only during the process of "geting the record".
 * It is needed to protect uses with NOLOCK against disasters,
 * created by somebody modifying the record at the same time
 * a NOLOCK gets it.
 * 
 * Lockmode indicates the strength of lock requested and
 * whether or not to wait for the lock. It is composed
 * from LKSHARE, LKEXCL.
 * 
 * If the lock is available and granted, the returned value
 * is zero.
 * 
 * If the lock request cannot
 * be immediately granted, the request is queued and the
 * user put to sleep.
 *
 * RETURNS: (0) - success
 *              If the lock is available and granted, the returned
 *              value is zero.
 *          DSM_S_RQSTREJ - request rejected
 *          DSM_S_RQSTQUED - request queued
 */
int
lkrglock (
        dsmContext_t    *pcontext,
        lockId_t        *plockId,    /* Extended dbkey of record */     
        int              lockmode)    /* SHARE or EXCLUSIVE       */
{
    int ret;

    for (;;)
    {
        ret = lklocky (pcontext, plockId, lockmode, LK_RGLK, 
                       (TEXT *)NULL, (LKSTAT *)NULL);
        if (ret != DSM_S_RQSTQUED)
            break;

        /* wait for the record to become free */

        latUsrWait (pcontext);
    }
    return ret;
}

/* PROGRAM: lkrgunlk -- called to unlock the record from the "record get" 
 * locked by the caller. Complains if the record is not
 * currently locked by the calling user. 
 *
 * RETURNS:  None
 *
 */
DSMVOID
lkrgunlk (
        dsmContext_t    *pcontext,
        lockId_t        *plockId)     /* Extended dbkey */
{

    lkrelsy (pcontext, plockId, LK_RGLK);
}


/* PROGRAM: lkCheckTrans -- called by index manager to see if the active user
 *              must wait for a deleted record mark set by the indicated
 *              database task number.
 *
 * RETURNS: zero if the user need not wait. Returns DSM_S_RQSTREJ
 *              if the user should wait, but the LKNOWAIT option was
 *              specified. Returns DSM_S_RQSTQUED if the user must wait until
 *              the indicated database task ends.
 *
 * [Function previously existed as svwaitt in drdb.c]
 *
 */
int
lkCheckTrans (
        dsmContext_t    *pcontext,
        LONG            taskn,         /* database task number which set a
                                           deleted record mark in a record
                                           we want */
        int             lockmode)      /* strength of lock requested,
                                           either LKSHARE or LKEXCL, with
                                           LKNOWAIT optionally added in */
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    usrctl_t  *pusr   = pcontext->pusrctl;
    dbshm_t   *pdbpub = pdbcontext->pdbpub;

    int     i;


    if (pdbcontext->arg1usr)
        return 0;

    if ((lockmode & LKTYPE) == LKNOLOCK)
        return 0;

    if (taskn == pusr->uc_task)
        return 0;

    /* not my own task */

    MT_LOCK_TXT ();
    if (tmdtask(pcontext, taskn))                       /* dead task */
    {
        MT_UNLK_TXT ();
        return (0);
    }
    if (lockmode & LKNOWAIT)
    {
        /* live task but caller does not want to wait for it */

        MT_UNLK_TXT ();
        return (DSM_S_RQSTREJ);
    }
    if (pusr->uc_wait)
    {
        /* was svwaitt called twice? or FATAL? */

        if ((pusr->uc_wait != TSKWAIT) || (pusr->uc_wait1 != taskn))
        {
            MT_UNLK_TXT ();
            /* svwaitt - user already in a wait */
            FATAL_MSGN_CALLBACK(pcontext, dbFTL010);
        }
    }
    else
    {
        pusr->uc_wait = TSKWAIT;
        pusr->uc_wait1 = taskn;
    }
    /* find out which user owns the lock */

    pusr = NULL;
    for (pusr = pdbcontext->p1usrctl, i=0; i < pdbpub->argnusr;  i++, pusr++)
    {
        if ((pusr->usrinuse) && (pusr->uc_task == taskn))
            break;
    }
    /* chain the usr in the task waiting chain */

    /* FIXFIX -- Kluge to get holder name in lkwait message */
    pcontext->pusrctl->uc_wusrnum = pusr->uc_usrnum;


    MT_UNLK_TXT ();

    return (DSM_S_RQSTQUED);

}  /* end lkCheckTrans */



/* PROGRAM: lkAdminLock - use the record get lock to protect objects 
 *                        during utility administration activities.
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_FAILURE
 */
dsmStatus_t
lkAdminLock(dsmContext_t    *pcontext,     /* IN database context */
            dsmObject_t     objectNumber,  /* IN object to be locked */ 
            dsmArea_t       objectArea,    /* IN area of object to be locked */
            dsmMask_t       lockMode)      /* IN bit mask for lock type */
        
{
    dsmStatus_t  returnCode;
    lockId_t xDbkey;

    xDbkey.table = objectArea;
    xDbkey.dbkey = objectNumber;

    /* Prevent calls with object number zero - doesn't make sense */
    /* 98-09-16-051 - removing this check since _user._user-id index */
    /*                has a valid object number of 0                 */
#if 0
    if (objectNumber == 0)
        return(DSM_S_FAILURE);
#endif

    returnCode = (dsmStatus_t)lkrglock(pcontext, &xDbkey, lockMode);
    return returnCode;

}

/* PROGRAM: lkAdminUnlock - release the record get lock on an object
 *                          during utility administration activities.
 *
 * RETURNS: DSM_S_SUCCESS
 *          DSM_S_FAILURE - called function with area 0
 *         
 */
dsmStatus_t
lkAdminUnlock(dsmContext_t    *pcontext,     /* IN database context */
              dsmObject_t     objectNumber,  /* IN object to be unlocked */ 
              dsmArea_t       objectArea)    /* IN area of object */
        
{
    lockId_t xDbkey;

    xDbkey.table = objectArea;
    xDbkey.dbkey = objectNumber;

    /* Prevent calls with object number zero - table 0 dbkey 0 causes core */
    /* 98-09-16-051 - removing this check since _user._user-id index */
    /*                has a valid object number of 0.                */
#if 0
    if (objectNumber == 0)
        return(DSM_S_FAILURE);
#endif

    lkrgunlk(pcontext, &xDbkey);
    return(DSM_S_SUCCESS);

}
