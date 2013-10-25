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

#include <stdio.h>
#include <stdlib.h>
#include "dbconfig.h"

#include "bkpub.h"
#include "bmpub.h"
#include "dbcontext.h"
#include "dsmpub.h"
#include "drmsg.h"
#include "latpub.h"
#include "mstrblk.h"
#include "latpub.h"
#include "rlmsg.h"
#include "rlprv.h"
#include "rlpub.h"
#include "shmpub.h"
#include "stmpub.h"
#include "stmprv.h"
#include "tmprv.h"
#include "tmtrntab.h"
#include "usrctl.h"  /* temp for DSMSETCONTEXT */
#include "utepub.h"  /* utgetenvwithsize */
#include "utfpub.h"
#include "utspub.h"
#include "uttpub.h"
#include "utmpub.h"

#include <time.h>

LOCALF DSMVOID rlclclean		(dsmContext_t *pcontext, RLBLK *prlblk);

#if 0
LOCALF DSMVOID rlclmov		(dsmContext_t *pcontext, LONG clstaddr,
				 LONG nextaddr);
#endif

LOCALF int rlcltry		(dsmContext_t *pcontext, int syncCP);

LOCALF  LONG rllogxtn		(dsmContext_t *pcontext, LONG xtndamt );

LOCALF int rltest1		(dsmContext_t *pcontext, RLBLK *prlblk);

LOCALF int rltest2		(dsmContext_t *pcontext, RLBLK *prlblk);

LOCALF int rltest3		(dsmContext_t *pcontext, RLBLK  *prlblk);

LOCALF DSMVOID rlSaveRLtime        (dsmContext_t *pcontext, int flush);
LOCALF DSMVOID rlUpdRLtime         (dsmContext_t *pcontext);

/* rlcl.c - recovery log file (.bi file) cluster control programs */

/* PROGRAM: rlAgeLimit -- Returns the number of seconds a cluster      *
 *                        must age before it can be reused.            *
 *                                                                     *
 * RETURNS:
 */
int
rlAgeLimit (dsmContext_t *pcontext)
{
    dbshm_t     *pdbshm = pcontext->pdbcontext->pdbpub;
    int   ageTime;

    /* If the user has set the -G parameter > 60 then use that
       as the amount of time a cluster must age before it can be
       re-used.                                                       */
    if ( pdbshm->rlagetime > RLFIXAGE )
        ageTime = pdbshm->rlagetime;
    else
        ageTime = RLFIXAGE;
    
    return ageTime;
}

/* PROGRAM: rlInitRLtime - initialize the base time in rlctl
 *      This time is the bi file's base time. All cluster timestamps
 *      are measured relative to this time. The base time is recovered
 *      when we do the redo pass of crash recovery.
 *
 * RETURNS: void
 */
DSMVOID
rlInitRLtime (dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLCTL	    *prl = pdbcontext->prlctl;
    LONG            curTime;
 
    /* Get the current time */
 
    time (&curTime);
 
    /* remember it */
 
    prl->rlsystime = curTime;
 
    /* This is now the bi file's base time. All time stamps are measured
       relative to this time */
    prl->rlbasetime = curTime;
 
    /* Cluster time is same as current time */
 
    prl->rlclstime  = 0;
 
    /* Master block's time is same as current time */
 
    pdbcontext->pmstrblk->mb_rltime = 0;
 
    /* No clock errors */
 
    prl->rlClockError = 0;

} /* end rlInitRLtime */

/* PROGRAM: rlLoadRLtime - Load previous RL time stamp from master block 
 *
 *
 * RETURNS: void
 */
DSMVOID
rlLoadRLtime(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLCTL	    *prl = pdbcontext->prlctl;
    LONG            curTime;
    LONG            savedRLtime;
 
    time (&curTime);
 
    savedRLtime = pdbcontext->pmstrblk->mb_rltime;
 
    /* Last time o/s gave us */
 
    prl->rlsystime = curTime;
 
    /* base for cluster time stamps - the oldest cluster will have this
       time stamp or later */
 
    prl->rlbasetime = curTime - savedRLtime;
 
      /* Cluster time stamp, relative to base time */
 
    prl->rlclstime = savedRLtime;
 
    /* No clock errors */
 
    prl->rlClockError = 0;

#ifdef RL_SESSION_TIME
    MSGD_CALLBACK(pcontext, "%LrlLoadRLtime: cTime %l, rlsystime %l",
                  curTime, prl->rlsystime);
    MSGD_CALLBACK(pcontext, "%Lrlbasetime %l, mstblk time %l, rlclstime %l",
                  prl->rlbasetime, savedRLtime, prl->rlclstime);
#endif

} /* end rlLoadRLtime */

/* PROGRAM: rlUpdRLtime - Get a new RL time stamp 
 *
 * RETURNS: void
 */
LOCALF DSMVOID
rlUpdRLtime(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    struct bkctl    *pbkctl = pcontext->pdbcontext->pbkctl;
    RLCTL	    *prl = pdbcontext->prlctl;
    LONG            curTime = 0;
    LONG            adjustedTime;
    LONG            diffTime;
    TEXT            ptestAdjust[9];
 
TRACE_CB(pcontext, "rlUpdRLtime")
 
    /* If we're doing an online backup, do not update the cluster time; it
    ** is kept constant during the backup so that clusters won't be reused.
    */
    if (pbkctl->bk_olbackup)
        return;

    /* Check to see if time adjustment exists in the environment */
    /* Note: this is to test time problems, normally the time
       adjustment would be zero at a customer site.              */

    /* Check for invalid value only when the database first opens */
    if ( prl->rlTestAdjust == RL_INVALID_TIME_VALUE )
    {
        if (!utgetenvwithsize((TEXT*)"UTTIMEADJUST", ptestAdjust, 9) ) 
            prl->rlTestAdjust = atoi((psc_rtlchar_t *)ptestAdjust);
        else
            prl->rlTestAdjust = RL_NO_TIME_VALUE;
    }

    if ( ( prl->rlTestAdjust != RL_INVALID_TIME_VALUE ) &&
       ( prl->rlTestAdjust != RL_NO_TIME_VALUE) )
    {
        /* Check for every third cluster close */
        if ( pdbcontext->pdbpub->biclCt % 3 )  
            utSysTime(prl->rlTestAdjust, &curTime);
        else
            utSysTime((LONG)0, &curTime);
    }
    else
        utSysTime((LONG)0, &curTime);

    /* apply the correction to it */
    adjustedTime = curTime + prl->rlClockError;
   
#ifdef RL_SESSION_TIME
    MSGD_CALLBACK(pcontext,  
    "%LrlUpdRLtime: curTime %l, adjTime %l, clockError %l, TestAdjust %l",
    curTime, adjustedTime, prl->rlClockError, prl->rlTestAdjust);
#endif

    if (adjustedTime < (prl->rlbasetime + prl->rlclstime) )
    {
        /* The adjusted current time is earlier than rlbasetime
           plus the cluster time interval (rlclstime).
           That means the clock has gone backwards (maybe again).
 
           This commonly happens in the fall when daylight savings time
           ends and clocks are set back an hour. But the difference we
           detect may be less than that because we might not have looked
           for awhile.
 
           We can also get this error when time servers are being
           consulted to synchronize clocks across machines in a
           network. Some systems have bogus clients that adjust the
           clock backwards instead of slowing the forward motion.
        */
 
        /* We can only tell how much time we are off since the 
           last cluster time interval */
        diffTime = (prl->rlbasetime + prl->rlclstime) - curTime;
 
        /* Report it */
 
        MSGN_CALLBACK(pcontext, rlMSG141, diffTime);
        
        /* We will apply this much correction to the clock so we
           can keep it going forward. */
 
        prl->rlClockError = diffTime;
 
        /* This TIME will be the same as the previous one, but
           future ones will keep going forward at the correct rate
           (unless the clock jumps back again) */
 
        adjustedTime = (curTime + prl->rlClockError + 1);
    }
    /* RL time stamp, relative to bi file's base time */
 
    prl->rlclstime = adjustedTime - prl->rlbasetime;

#ifdef RL_SESSION_TIME
    MSGD_CALLBACK(pcontext, 
                  "%LrlUpdRLtime: rlbasetime %l, rlclstime %l, adjTime %l",
                  prl->rlbasetime, prl->rlclstime, adjustedTime);
#endif

} /* end rlUpdRLtime */

/* PROGRAM: rlSaveRLtime  - Record the current run time permanently 
 *
 * RETURNS: void
 */
LOCALF DSMVOID
rlSaveRLtime (dsmContext_t *pcontext,
              int flush)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLCTL	    *prl = pdbcontext->prlctl;
    COUNT            depthSave;
 
    /* get time relative to base time */
 
    rlUpdRLtime (pcontext);
 
    /* save it in the master block */
 
    pdbcontext->pmstrblk->mb_rltime = prl->rlclstime;
    if (flush)
    {
        /* Write to master block on disk */
 
        MT_FREE_BIB(&depthSave);
        bmFlushMasterblock(pcontext);
        MT_RELOCK_BIB(depthSave);
    }

} /* end rlSaveRLtime */

/* PROGRAM: rltest1 -- Carry out cluster reuse test 1 (see cluster.txt)	* 
 *	    this test checks to see if the cluster has aged at least    * 
 *          1 minute	                                                * 
 * RETURNS:	0 - If the cluster passes test 1			* 
 *		1 - If the cluster fails test 1				* 
 */
LOCALF int
rltest1(
	dsmContext_t	*pcontext,
	RLBLK		*prlblk)  /*  points to the header block of the cluster
				we are trying to reuse			*/
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    int   ageTime;
TRACE_CB(pcontext, "rltest1")

    if ( !pdbshm->fullinteg) return 0;
    
    ageTime = rlAgeLimit(pcontext);

    if (   prlblk->rlclhdr.rlclose != 0
	&& pdbcontext->prlctl->rlclstime - prlblk->rlclhdr.rlclose
          >= ageTime)
	 return 0;
    else return 1;
}

/* PROGRAM: rltest2 -- Carry out cluster reuse test 2 
 *  this tests to see if the cluster to the right of this one has
 *  been aged enough. This is necessary because we do "fuzzy" checkpoints.
 *
 * RETURNS:	0 - If the cluster passes test 2			*
 *		1 - If the cluster fails test 2				*
 */
LOCALF int
rltest2(
	dsmContext_t	*pcontext,
	RLBLK		*prlblk)  /* points to the header block of the cluster
			     we are trying to reuse			*/
{
    dbshm_t     *pdbshm = pcontext->pdbcontext->pdbpub;
TRACE_CB(pcontext, "rltest2")

	/* Assume we have four clusters as show below:

		1 -- 2
		|    |
		4 -- 3

	Assume we just closed cluster 3. The next cluster is 4.
	If we crash now, we will begin the redo phase with cluster 2
	if it has aged. If it has not, we begin with the latest one
	that has aged enough (1 or 4).

	Since we just closed cluster 3, blocks modified during
	cluster 3 have just been scheduled for checkpoint. Any
	blocks modified during cluster 2 that were still in memory
	were written out during the checkpoint for cluster 3.

	Any blocks modified during cluster 1 and 4 have already been
	written out. The blocks for cluster 4 were written when cluster
	1 was checkpointed, or earlier, and will be safely on disk by
	the time cluster 1 has been aged.

	So, we can reuse a cluster if the one to the right of it (i.e.
	the one that was used after it) has aged.
    */

    if (!pdbshm->fullinteg) return 0;

    /* read the next header for the next cluster in the ring */

    prlblk = rlbird(pcontext, prlblk->rlclhdr.rlnextBlock);

    /* now apply the aging test to that block */

    return (rltest1(pcontext, prlblk));
}

/* PROGRAM: rltest3 -- Carry out cluster reuse test 3 (see cluster.txt)	*
 *   this test checks there are no live transactions in this cluster	*
 *
 * RETURNS:	0 - If the cluster passes test 3			*
 *		1 - If the cluster fails test 3				*
 */
LOCALF int
rltest3(
	dsmContext_t	*pcontext,
	RLBLK		*prlblk)  /*  points to the header block of the cluster
			     we are trying to reuse			*/
{
TRACE_CB(pcontext, "rltest3")
    return tmrlctr(pcontext, prlblk->rlblkhdr.rlcounter);
}

/* PROGRAM: rlclcls - Close an rl file cluster
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlclcls(
	dsmContext_t	*pcontext,
	int		 reclose)/* 1=reclose the cluster:  If it is open */
				/* then dont write a close date into it  */
{
  dbcontext_t   *pdbcontext=pcontext->pdbcontext;
  RLBLK         *prlblk;
TRACE_CB(pcontext, "rlclcls")
  
#if SHMEMORY
    /**********TEMPORARY**
    sltrace("rlclcls",0 );
    ***/
#endif
    /* flush all blocks and write out mstr blk with curr time */
    rlflsh(pcontext, 0); 

    /* put the current "run-time" in the clustr hdr */
    /* read the cluster header block*/
    prlblk = rlbird(pcontext, pdbcontext->prlctl->rlcurr); 

    if ( prlblk->rlclhdr.rlclose != 0 || reclose == 0 )
    {	/* give the cluster a close time stamp */
	prlblk->rlclhdr.rlclose = pdbcontext->prlctl->rlclstime;
	rlbiwrt(pcontext, prlblk);	/* write it back out	 */
    }
    
    STATINC (biclCt);

} /* end rlclcls */

/* PROGRAM: rlclcurr - return the current rl counter
 *
 * RETURNS: prlctl->rlcounter
 */
LONG
rlclcurr(dsmContext_t *pcontext)
{
    /* return the current rlcounter */
    return(pcontext->pdbcontext->prlctl->rlcounter);
}

/* PROGRAM: rlclopn -- open a cluster for output
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlclopn(
	dsmContext_t	*pcontext,
	LONG		 clusterHeader, /* cluster header blk number */
        LONG             syncCP) 
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLCTL	    *prl = pdbcontext->prlctl;
    RLBLK	    *prlblk;	/* points to the cluster header block	*/
    struct bktbl     theBktbl;

    TRACE1_CB(pcontext, "rlclopn %l", clusterHeader)

    prl->rlcounter++;

    prlblk = rlbird(pcontext, clusterHeader);
    rlbiout(pcontext, prlblk);	/* designate this the official output block */
    prlblk->rlblkhdr.rlcounter = RL_CHECKPOINT;
    prlblk->rlblkhdr.rlused =	RLCLHDRLEN;
    prlblk->rlblkhdr.rlhdrlen = RLCLHDRLEN;

    prlblk->rlclhdr.rlclose   = 0;	/* cluster is now open */
    prlblk->rlclhdr.rlrightBlock= 0;
    prlblk->rlclhdr.rlmaxtrn  = pdbcontext->ptrantab->maxtrn;

#ifdef TRANSTABLETRACE
    MSGD_CALLBACK(pcontext, (TEXT *)"%Lrlclopn: RLBLK->rlclhdr.rlmaxtrn[%l] = pdbcontext->ptrantab->maxtrn[%l]",
                  prlblk->rlclhdr.rlmaxtrn, pdbcontext->ptrantab->maxtrn);
#endif /* TRANSTABLETRACE */

    prlblk->rlclhdr.rlpbkbOffset = prl->rlpbkbOffset;
    prlblk->rlclhdr.rlpbkbBlock = prl->rlpbkbBlock; 
    prlblk->rlclhdr.rlpbkcnt = prl->rlpbnundo;
    prlblk->rlclhdr.rlSyncCP = syncCP;  /* a normal checkpoint */

    /* current open cluster address */
    prl->rlcurr = clusterHeader;
    prl->rlclused = RLCLHDRLEN - sizeof(RLBLKHDR);
    prl->rlblOffset = RLCLHDRLEN;
    prl->rlblNumber = clusterHeader;
    prl->rlnxtBlock = clusterHeader;
    prl->rlnxtOffset = RLCLHDRLEN;
    prl->rltype1  =	prlblk->rlclhdr.rlnextBlock;

    rlmemwt(pcontext);	/* write the memory table note */
    /* In case checkpoint note required a multi-block write */
    rlbiflsh(pcontext, RL_FLUSH_ALL_BI_BUFFERS);
    /* re read the cluster header to put the new cluster counter in it */
    prlblk = rlbird(pcontext, clusterHeader);
    prlblk->rlblkhdr.rlcounter = prl->rlcounter;

    /* set self-referential shared ptrs */
    theBktbl.qself = P_TO_QP(pcontext,&theBktbl);
    /* set the control block pointer to buffer */
    theBktbl.bt_qbuf = (QBKBUF)P_TO_QP(pcontext,prlblk);
    
    theBktbl.bt_area = DSMAREA_BI;
    theBktbl.bt_ftype = BKBI;
    theBktbl.bt_dbkey = clusterHeader;
    
    bkaddr (pcontext,&theBktbl, RAWADDR);
    bkWrite(pcontext,&theBktbl, BUFFERED); /* Write it back out   */

}

/* PROGRAM rlsetc - close up shop
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlsetc (dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
FAST	RLCTL	*prl = pdbcontext->prlctl;
FAST    AICTL   *pai = pdbcontext->paictl;

TRACE_CB(pcontext, "rlsetc")

    if (!prl) return;

    MT_LOCK_BIB ();
    if (prl->rlopen)
    {
        prl->rlopen = 0;	/* prevent infinite loops */
        rlflsh(pcontext, 1);	/* flush out all blocks   */
        pdbcontext->prlctl = NULL;
    }
    MT_UNLK_BIB ();
    if( pai )
    {
	rlaicls(pcontext);
    }
    if( pdbcontext->ptlctl )
        rltlClose(pcontext);
    
}

/* PROGRAM: rlflsh - flush out all modified blocks for .db, .bi, .ai
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlflsh(
	dsmContext_t	*pcontext,
	int		 closedb)    /* if ON, then close the database */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
    COUNT        depthSave;

    if (!pdbshm->fullinteg && closedb)
	rlbitrnc(pcontext, 0, 0, 0); /* truncate the .bi if -i session */
    else
	/* flush unwritten .bi buffer	*/
	rlbiflsh(pcontext,RL_FLUSH_ALL_BI_BUFFERS );

    if (pdbshm->fullinteg || closedb)
    {
	MT_FREE_BIB(&depthSave);
	bmFlushx(pcontext, FLUSH_ALL_AREAS, closedb); /* checkpoint the buffer pool */

	rlaifls(pcontext);		/* flush unwritten .ai block*/
	MT_RELOCK_BIB(depthSave);
    }

	/* must be before the rlSaveRLtime so the mb
	will be writen after all the other blocks #*/
	if ((closedb) || (!pdbshm->directio && pdbshm->fullinteg))
	    bkioSync(pcontext);

    if (closedb)
    {	
        pdbcontext->pmstrblk->mb_dbstate = DBCLOSED;
	pdbcontext->pmstrblk->mb_flags   = 0;
        /* The invalid semid/shmid is a (UNLONG)-1 */
        pdbcontext->pmstrblk->mb_shmid = (ULONG)-1;
        pdbcontext->pmstrblk->mb_semid = (ULONG)-1;
    }
    /* label the master block with current time	*/
    rlSaveRLtime (pcontext, (int)1); /* 1 to force the mb flush */

}

/* PROGRAM: rlclnxt - Open the next available rl file cluster
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlclnxt(dsmContext_t *pcontext, int syncCP)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    int	ret;
    int	retrycnt = 0;
    RLCTL	*prl = pdbcontext->prlctl;
    DOUBLE          bitholdBytes = 0;
    DOUBLE          bitholdSize = 0;
    TEXT            sizeId;
    TEXT            printBuffer[13];

TRACE_CB(pcontext, "rlclnxt")

    /* this loop calls rlcltry() to open the next available cluster */
    /* During crash recovery (warmstrt), if there is no disk space  */
    /* then the loop will retry 6 times, hoping a cluster will age  */
    while(   (ret=rlcltry(pcontext, syncCP)) == RLRETRY
	  && prl->rlwarmstrt
	  && ++retrycnt < 6)
    {	
        utsleep(15);
        rlSaveRLtime (pcontext, (int)1); /* 1 to force the mb flush */
    }

    if (ret == RLTHRESHOLD)
    {
        /* bi threshold reached, starting emergency shutdown */
        /* Convert and display bithreshold size */
        bitholdBytes = (DOUBLE)pdbcontext->pdbpub->bithold * prl->rlbiblksize;
        utConvertBytes(bitholdBytes, &bitholdSize, &sizeId);
        sprintf((char *)printBuffer, "%-5.1f %cBytes", bitholdSize, sizeId);
        MSGN_CALLBACK(pcontext, drMSG683, printBuffer);
        MSGD_CALLBACK(pcontext, "%rEmergency shutdown initiated...");
        dbExit(pcontext, 0, DB_EXBAD);
    }

    if (ret != 0)
        FATAL_MSGN_CALLBACK(pcontext, rlFTL017);

    /* during warm restart, if there is no type 3 */
    /* reuse candidate, now there is one	  */
    if (prl->rltype3 == -1) 
        prl->rltype3 = prl->rlcurr;

    return;
}

/* PROGRAM: rlcltry - Make 1 try to open the next avail rl cluster
 *
 * RETURNS:	0   - OK, all done					* 
 *	    RLDIE   - Unable, the caller should terminate		* 
 *	    RLRETRY - Unable, but the caller should wait a little	* 
 *			and retry because a cluster will become avail	*
 */
LOCALF int
rlcltry(dsmContext_t *pcontext, int syncCP)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    int	            r1ret=0,r2ret=0,r3ret=0;
    RLBLK	    *prlblk;
    LONG	    clusterBlock;
    RLCTL	    *prl = pdbcontext->prlctl;
    LONG            returnStatus;
    LONG            newCluster;
TRACE_CB(pcontext, "rlcltry")

    /* This program is called right after calling rlclcls or rlrollf.	*/
    /* It expects:							*/
    /*	- prlctl->systime, basetime, clstime to be up to date.		*/
    /*	- prlctl->rltype1 to be the lseek addr of the leftmost cluster	*/
    /*	    in the ring.						*/
    /*	(this is the type 1 cluster for the re-use logic)		*/

    /* check if the leftmost cluster in the file (rltype1) can be reused*/
    clusterBlock = prl->rltype1;
    prlblk = rlbird(pcontext, clusterBlock);
    if ((prlblk->rlblkhdr.rlcounter == 0) || 
        (prlblk->rlblkhdr.rlcounter == RL_CHECKPOINT))
    {	
        /* this cluster has never been used, it is A-OK */
	rlclopn(pcontext, clusterBlock, (LONG)syncCP);
	return 0;
    }

    rlbiout(pcontext, prlblk); /* prevent rltest2 from flushing this block */
    if (   (r1ret=rltest1(pcontext, prlblk))==0
	&& (r2ret=rltest2(pcontext, prlblk))==0
	&& (r3ret=rltest3(pcontext, prlblk))==0)
    {
        STATINC (biruCt);
	rlclopn(pcontext, clusterBlock, (LONG)syncCP);
	return 0;
    }

    /* try to create a new cluster */
    /* extend 1 cluster */
    returnStatus = rlclxtn(pcontext, prl->rltype1, 1, &newCluster);  
    if (returnStatus == 0)
    {	
        /* addr of 2nd new cluster */
        prl->rltype1 = newCluster + 
                       (prl->rlclbytes >> prl->rlbiblklog); 
	rlclopn(pcontext, newCluster, (LONG)syncCP);
	return 0;
    }
    else if(returnStatus == -1)
    {
        return RLTHRESHOLD;
    }

    /* if rlclxtn failed and there is a cluster which will become */
    /* reusable if we wait patiently, then wait for it		  */
    if (r1ret + r2ret == 0) return RLRETRY;
    else		    return RLDIE;
}

/* PROGRAM: rlclxtn - Create new clusters in the .rl file		*
 * 
 * RETURN:	1 - could not create a cluster, disk is full		*
 *	    non-0 - success - first new cluster added	*
 */
LONG
rlclxtn(
	dsmContext_t	*pcontext,
	LONG	         clusterBlock, /* lseek blk of a cluster header */
                          	       /* link the new cluster in the ring */
				       /* preceeding the addressed cluster */
	int		 numToExtend,  /* 4 on startup otherwise 1 */
        LONG            *pnewCluster)  /* block number of new cluster */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t         *pdbshm = pdbcontext->pdbpub;
    LONG	    addr;
    LONG	    oldsize;
    LONG	    i;
    LONG	    amtclrd;	/* nbr of existing blocks beyond "EOF" */
    LONG	    amttoxtn;	/* nbr of new blocks to be added       */
    RLCTL	    *prl = pdbcontext->prlctl;
    LONG	    amtdesired;     /* amount we want to extend */
    LONG            clusterSize;  /* cluster size in blocks */
    LONG            stalled = 0;  /* are we bi stalled? */ 
    DOUBLE          bitholdBytes = 0;
    DOUBLE          bitholdSize = 0;
    TEXT            sizeId;
    TEXT            printBuffer[13];

    TRACE1_CB(pcontext, "rlclxtn blknumber=%l", clusterBlock)

#ifdef BITRACE
    MSGD_CALLBACK(pcontext,
                 (TEXT*)"%L rlclxtn: blknumber=%l", clusterBlock);
#endif

    oldsize = prl->rlsize;	/* first byte of new cluster */

    amtdesired = (numToExtend * prl->rlclbytes) >> prl->rlbiblklog;

    if ((pdbshm->bithold != 0) &&
        (( ((float)(pdbshm->bithold)) * .8) <= (float)oldsize))
    {
        /* bi size has reached % of theshold */
        /* Convert and display bithreshold size */
        bitholdBytes = (DOUBLE)pdbshm->bithold * prl->rlbiblksize;
        utConvertBytes(bitholdBytes, &bitholdSize, &sizeId);
        sprintf((char *)printBuffer, "%-5.1f %cBytes", bitholdSize, sizeId);
        MSGN_CALLBACK(pcontext, drMSG684, printBuffer);
    }

    /* Is a threshold size set for the recovery log file? */
    while ((pdbshm->bithold != 0) && (pdbshm->bithold <= (ULONG)oldsize))
    {
        if (pdbshm->bistall)
        {
            if (!stalled)
            {
                /* bi size has reached threshold.  forward processing
                   stalled until dba intervention */
                /* Convert and display bithreshold size */
                bitholdBytes = (DOUBLE)pdbshm->bithold * prl->rlbiblksize;
                utConvertBytes(bitholdBytes, &bitholdSize, &sizeId);
                sprintf((char *)printBuffer, "%-5.1f %cBytes", bitholdSize, sizeId);
                MSGN_CALLBACK(pcontext, drMSG683, printBuffer);
                MSGN_CALLBACK(pcontext, drMSG457);
                stalled = 1;

                /* flag quietpoint as enabled */
                pdbshm->quietpoint = QUIET_POINT_ENABLED;
            }
            /* nap before checking again */

            /*------------------------------------------------------------*/
            /* 19991108-036 - Need to see if shutdown flag is set so that */
            /*  a stalled database can be manually shut down.             */
            /*------------------------------------------------------------*/
            if (pdbcontext->pdbpub->shutdn)
            {
                pdbcontext->pdbpub->shutdn = DB_EXBAD;
                return RLTHRESHOLD;
            }
            else
                utnap(10);
        }
        else
        {
            pdbcontext->pdbpub->shutdn = DB_EXBAD;
            return RLTHRESHOLD;
        }
    }
 
    if (stalled)
    {
        stalled = 0;
        /* forward processing continuing */
        MSGN_CALLBACK(pcontext, drMSG458);
    }

    /*try to format existing blocks first*/
    amtclrd = rllogxtn(pcontext, amtdesired);
    amttoxtn = amtdesired - amtclrd;

    /* now, do a physical extend if needed */
    if (amttoxtn > 0)	
    {
        amttoxtn = bkxtn(pcontext, DSMAREA_BI, amttoxtn);
    }

    if (amtclrd + amttoxtn < (prl->rlclbytes >> prl->rlbiblklog)) 
    {	
	return 1;  /* unable, not enough disk */
    }

    /* use no more than a multiple of cluster size of the new space */
	 
    /* convert to a block based calculation */
    clusterSize = prl->rlclbytes >> prl->rlbiblklog;
    i = prl->rlsize + (amtclrd + amttoxtn);
    prl->rlsize = i - (i % clusterSize);

    /* format the new clusters and link them into the ring */
    for(addr = oldsize; addr < prl->rlsize; addr += clusterSize)
	rlclins(pcontext, addr, clusterBlock);

    *pnewCluster = oldsize;
    return 0;  /* return successful creation of 1st new cluster */
}

/* PROGRAM: rlclins -- Insert a cluster in the ring of clusters
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlclins(
	dsmContext_t	*pcontext,
	LONG		 clusterBlock,   /* the Block of the clstr to insert */
	LONG		 nextCluster)    /* insert the cluster into the ring
				         preceeding this one. */
{
        LONG	 prevBlock;
        RLBLK	 *prlblk;
TRACE2_CB(pcontext, "rlclins clusterBlock=%l nextCluster=%l", clusterBlock, 
                     nextCluster)
#ifdef BITRACE
MSGD_CALLBACK(pcontext,
             (TEXT *)"%Lrlclins clusterBlock=%l nextCluster=%l", 
                      clusterBlock, nextCluster);
#endif

    /* If nextCluster == clusterBlock, then this is the first cluster 
       and it should be linked to itself.  This only happens to cluster 0.  
       It is already all zeros, so we dont have to do anything at all	
       except initialize its header fields				*/
    if (nextCluster == clusterBlock)
    {	
        if (clusterBlock != 0)
            FATAL_MSGN_CALLBACK(pcontext, rlFTL019, clusterBlock);
        
	prlblk = rlbird(pcontext, clusterBlock);
	rlclclean(pcontext, prlblk);
	rlbiwrt(pcontext, prlblk);
	return;
    }

    /* the order of the following operations is critical */
    /* in case there is a system crash in the middle	 */
    /* read the next neighbor */
    prlblk = rlbird(pcontext, nextCluster);	
    prevBlock = prlblk->rlclhdr.rlprevBlock;
    prlblk->rlclhdr.rlprevBlock = clusterBlock;
    rlbiwrt(pcontext, prlblk);		/* next neighbor is now linked */

    /* read the new clstr header */
    prlblk = rlbird(pcontext, clusterBlock);	
    rlclclean(pcontext, prlblk);	/* clear its header out		*/
    prlblk->rlclhdr.rlnextBlock = nextCluster;
    prlblk->rlclhdr.rlprevBlock = prevBlock;
    rlbiwrt(pcontext, prlblk);		/* the new block is now linked */

    /* read the prev neighbor */
    prlblk = rlbird(pcontext, prevBlock);	
    prlblk->rlclhdr.rlnextBlock = clusterBlock;
    rlbiwrt(pcontext, prlblk);		/* the prev block is now linked */
}

/* PROGRAM: rlclclean - clean up the cluster header
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlclclean(
	dsmContext_t	*pcontext,
	RLBLK		*prlblk)
{
    TRACE_CB(pcontext, "rlclclean")

    prlblk->rlclhdr.rlnextBlock = 0;
    prlblk->rlclhdr.rlprevBlock = 0;
    prlblk->rlclhdr.rlclose    = 0;
    prlblk->rlclhdr.rlrightBlock = 0;
    prlblk->rlclhdr.rlclsize   = pcontext->pdbcontext->pmstrblk->mb_rlclsize;
    prlblk->rlblkhdr.rlused    = 0;
    prlblk->rlblkhdr.rlcounter = 0;
    prlblk->rlclhdr.rlpbkbBlock = 0;
    prlblk->rlclhdr.rlpbkbOffset = 0;
}

#if 0
/* PROGRAM: rlclmov -- Move a clstr from 1 position in the ring to another
 *
 *  During normal execution, the last few clusters in the .rl file
 *  will typically contain live transactions.  If there is a crash, and
 *  the database is subsequently re-opened, the information in these
 *  clusters will be used to backout those live transactions.  As each
 *  of these clusters is completely backed out, it becomes eligible for
 *  re-use.  If such a cluster is reused, it must be removed from the ring
 *  and replaced in a different spot of the ring.  rlclmov does that job.
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlclmov(
	dsmContext_t	*pcontext,
	LONG clstaddr,      /* the lseek addr of the clstr to be moved */
	LONG nextaddr)      /* move the cluster into the ring preceeding
				   this one                            */
{
FAST	LONG	 next;      /* block number of next cluster */
FAST	LONG	 prev;      /* block number of prev cluster */
FAST	RLBLK	*prlblk;
        RLCTL    *prl = pcontext->pdbcontext->prlctl;
        LONG     clusterBlock = 0;
TRACE2_CB(pcontext, "rlclmov clstaddr=%l nextaddr=%l", clstaddr, nextaddr)

    /* the order of the following operations is critical */
    /* in case there is a system crash in the middle	 */

    /* set the cluster's rlcounter to 0 and get its next and prev ptrs */
   clusterBlock = clstaddr >> prl->rlbiblklog;
    prlblk = rlbird(pcontext, clusterBlock);


    prev = prlblk->rlclhdr.rlprevBlock;
    next = prlblk->rlclhdr.rlnextBlock;
    rlclclean(pcontext, prlblk);	/* clean out its header */
    rlbiwrt(pcontext, prlblk);

    /* unlink the next cluster */
    /* next is already a block */
    clusterBlock = next;
    prlblk = rlbird(pcontext, clusterBlock);
    prlblk->rlclhdr.rlprevBlock = prev;
    rlbiwrt(pcontext, prlblk);

    /* unlink the prev cluster */
   /* prev is already a block */
   clusterBlock = prev;
    prlblk = rlbird(pcontext, clusterBlock);
    prlblk->rlclhdr.rlnextBlock = next;
    rlbiwrt(pcontext, prlblk);

    /* now link the old cluster into the new location */
    rlclins(pcontext, clstaddr, nextaddr);
}

#endif

/* 
 * PROGRAM: rlbitrnc - truncate .bi file, change cluster and/or blocksize
 *
 * This procedure truncates the .bi file leaving it as a zero length file.
 * A waiting time in seconds can be requested to allow I/O to settle and
 * new cluster size and/or block size can be provided.
 *
 *   NOTE: If the cluster size or block size is changed then no further
 *         processing using the bi file should be done - in memory
 *         structures do not match the new values.  A waiting time is
 *         important to allow database i/o that was performed to
 *         complete.
 *
 *   BUGS: If RAW I/O is not in use, it is possible that the writes
 *	   in this program will occur in the wrong order.  If they
 *	   do, and a crash occurs before they all complete, then
 *	   the user may not be able to get back into his database
 *	   without using the -F parameter.  In fact, the database
 *	   will almost certainly be in good condition and the -F will
 *	   cause no harm.
 */
DSMVOID
rlbitrnc(
	dsmContext_t	*pcontext,
	int		 waittime,       /* waiting time in seconds */
	int		 newclustersize, /* if > 0, set new cluster size
                                          * in 16k units */
	int		 *pnewblocksize) /* if > 0, set new block size in bytes 
                                          * We will also return the newly 
                                          * calculated value of pnewblocksize
                                          * if the given value is in conflict
                                          * with the stated rules.
                                          */
{
    dbcontext_t         *pdbcontext = pcontext->pdbcontext;
    dbshm_t             *pdbshm = pdbcontext->pdbpub;
    RLCTL		*prl = pdbcontext->prlctl;
    struct mstrblk	*pmstr = pdbcontext->pmstrblk;
    int                 newblocksize;
    RLBF		*prlbf;
    QRLBF		q;

    COUNT                depthSave;

    TRACE_CB(pcontext, "rlbitrnc")

    /* wait for the dust to settle - not necessary on BTOS */
    utsleep ( waittime );

    /* copy the current ai file eof into the master block */
    if (pdbshm->qaictl) rlaieof(pcontext);

    /* since the system may crash while this is going on, we will   */
    /* label the master block that it is ok if the .bi file is gone */
    pmstr->mb_bistate = BITRUNC;

    /* determine if cluster size will change */
    if (newclustersize > 0)
    {
	/* new cluster size set to <size> kb */
	MSGN_CALLBACK(pcontext, rlMSG014, (LONG) newclustersize * 16); 
	pmstr->mb_rlclsize = newclustersize;
    }

    /* determine if the blocksize will change */
    newblocksize = pnewblocksize ? *pnewblocksize : 0;

    if (newblocksize > 0)
    {
        rlDetermineBiBlockSize(pcontext, &newblocksize);

	/* change the value in the masterblock */
        /* Before-image block size set to %l kb (%l bytes) */
        MSGN_CALLBACK(pcontext, rlMSG059,
                      (LONG) newblocksize / 1024, (LONG) newblocksize);
	pmstr->mb_biblksize = newblocksize;

        *pnewblocksize = newblocksize;
    }

    /* initialize the time */
    pmstr->mb_rltime  = 0;

    MT_FREE_BIB(&depthSave);
    bmFlushMasterblock(pcontext);	/* write it out with raw i/o */

    /* now truncate the .bi file */
    bktrunc(pcontext, DSMAREA_BI);

    /* clean up the bi buffers currently in memory */
    MT_RELOCK_BIB(depthSave);
    for (prlbf = XRLBF(pdbcontext, prl->qmodhead); prlbf; )
    {
	/* free all buffers on mod. list */

        q = prlbf->qnext;
        prlbf->qnext = prl->qfree;
	prlbf->bufstate = RLBSFREE;
        prl->qfree = QSELF (prlbf); 
        prlbf = XRLBF(pdbcontext, q);
    }
    prl->qmodhead = 0;
    for (prlbf = XRLBF(pdbcontext, prl->qpool); prlbf; )
    {
        /* mark all the buffers empty */

	prlbf->dpend = 0;
	prlbf->sdpend = 0;
        prlbf->rlbktbl.bt_dbkey = -1;	/* buffer is empty */
        prlbf->rlbktbl.bt_raddr = -1;
        prlbf->rlbktbl.bt_flags.changed = 0;
	prlbf = XRLBF(pdbcontext, prlbf->qlink);
    }
    prl->numDirty = 0;
    prl->rlsize = 0;
    rlInitRLtime (pcontext);

    bkioSync(pcontext);

}  /* end rlbitrnc */


/* PROGRAM: rlDetermineBiBlockSize
 *
 * RETURNS: 0 success
 *         -1 failure
 */
int
rlDetermineBiBlockSize(
        dsmContext_t *pcontext,
        int          *pnewblocksize)
{
    dbcontext_t         *pdbcontext = pcontext->pdbcontext;
    struct mstrblk      *pmstr = pdbcontext->pmstrblk;
 
    LONG boundarySize;  /* maximum kbytes for allocated buffer */
    LONG clusterSize;   /* kbytes in a bi cluster */
    LONG biSize;        /* kbytes in a bi block */
    bkAreaDesc_t        *pbkAreaDesc;

    if (*pnewblocksize <= 0)
        return -1;

    /* 
     * new value must conform to strict set of limitations:
     *   o min size = block size
     *   o max size = boundary size
     *   o number must divide evenly into cluster size.
     *   o number must divide evenly into all .bi extent sizes.
     *
     * the boundary could be max contigious buffer that can be allocated, 
     * but to make sure the value divides evenly into extent sizes, instead
     * the boundary is set at 16k to allow cluster and extent sizes to be
     * change without forcing the .bi block size to change.
     */

    if (*pnewblocksize > (int)BKGET_BLKSIZE(pmstr))
    {
        /* make sure the value fits the rules using kbyte units */
        boundarySize = 16;
        clusterSize = pmstr->mb_rlclsize * 16;
        biSize = *pnewblocksize / 1024;

        /* boundary must be no larger than a cluster */
        if (boundarySize > clusterSize)
        {
            boundarySize = clusterSize;
        }

        /* maximum size is the boundary */
        if (biSize > boundarySize)
        {
            biSize = boundarySize;
        }

        /* bi buffer must divide evenly into the cluster size */
        while (clusterSize % biSize) biSize--;

        /* change the kbytes back into bytes */
        *pnewblocksize = biSize * 1024;
    }
    else
    {
        /* same as database blocksize is always ok */
        *pnewblocksize = BKGET_BLKSIZE(pmstr);
    }

    pbkAreaDesc = BK_GET_AREA_DESC(pdbcontext, DSMAREA_BI);

    /* double check make sure blocksize is compatible with extents */
    if ( bkCheckBlocksize (pcontext, *pnewblocksize, BK_PRE_CHECK,
                           pbkAreaDesc->bkftbl, pdbcontext->pdbname))
    {
        /* Invalid blocksize requested, use default block size. */
        MSGN_CALLBACK(pcontext, rlMSG112, pdbcontext->pdbname);

        /* no choice, but to use the default */
        *pnewblocksize = BKGET_BLKSIZE(pmstr);
    }

    return 0;


}  /* end rlDetermineBiBlockSize */

/* 
 * PROGRAM: rllogxtn - format the existing portion of the .bi file
 *
 * If the .bi file contains some blocks which havent been formatted, this
 * routine formats up to <xtndamt> of them.  It uses the bi blocksize for
 * units.
 *
 * RETURNS: number of bi blocks formatted
 */
LOCALF LONG
rllogxtn(
	dsmContext_t	*pcontext,
	LONG		 xtndamt)      	/* number of bi blocks to extend */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLCTL        *prl = pdbcontext->prlctl; /* ptr to rl control block */
    LONG	 amttoclear;	/* # of blocks logically xtended */
    LONG	 spcavail;	/* amount actually extended in blocks */
    TEXT         *pfullbuf;     /* unaligned buffer space */
    TEXT         *pblockbuf;	/* aligned write buffer */
    struct  bktbl thebktbl;	/* needed for bkWrite */
    LONG         biBlocknumber; /* block number within bi file */
    LONG	 i;		/* for clear looping */
    COUNT        depthSave;

    /* determine number of blocks that are available for formatting */
    spcavail = (bkflen(pcontext, DSMAREA_BI) - prl->rlsize);

    if (spcavail <= 0)
    {
	/* cannot format something that is not there */
	return 0;
    }

    /* allocate and clear a single bi block to be used as an aligned buffer */
    pfullbuf = (TEXT *)stRentd(pcontext, pdbcontext->privpool, 
			       (unsigned)(prl->rlbiblksize + BK_BFALIGN));

    /* use new pointer to align - original unaligned pointer needed to free */
    pblockbuf = pfullbuf;

    /* align the buffer according to kind of I/O being done */
    bmAlign(pcontext, &pblockbuf);

    /* initialsize the buffer */
    stnclr((TEXT *)pblockbuf, (int) prl->rlbiblksize);

    /* set self-referential shared ptrs */
    thebktbl.qself = P_TO_QP(pcontext, &thebktbl);

    /* set the control block pointer to buffer */
    thebktbl.bt_qbuf = (QBKBUF)P_TO_QP(pcontext, pblockbuf);

    /* determine number of bi blocks that can be cleared */
    amttoclear = spcavail;
    if (amttoclear > xtndamt) amttoclear = xtndamt;

    /* start clear from used bi space up to what was end of bi file */
    for( biBlocknumber = prl->rlsize,
         i = amttoclear;
         i > 0; i--, biBlocknumber += 1)
    {
        /* setup dbkey and address info in control block */
        thebktbl.bt_dbkey = biBlocknumber;

	thebktbl.bt_ftype = BKBI;
	thebktbl.bt_area = DSMAREA_BI;
	bkaddr (pcontext, &thebktbl, 1);

	/* reset the block number so bkWrite will be happy and set it */
	((RLBLK *)pblockbuf)->rlblkhdr.rlblknum = 0;

	/* the buffer is unaligned, so must use BUFFERED because */
	/* raw-i/o has stringent, sys dependent alignment reqts */

        MT_FREE_BIB(&depthSave);
	bkWrite(pcontext, &thebktbl, BUFFERED);
        MT_RELOCK_BIB(depthSave);
    }
    /* free the buffer */
    stVacate(pcontext, pdbcontext->privpool, pfullbuf);

    return amttoclear;
}

/* PROGRAM: rlsetfl - mark the master block with any integrity warnings,
 *		such as running with non-raw or no integrity.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlsetfl(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm = pdbcontext->pdbpub;
FAST	struct mstrblk	*pmstr = pdbcontext->pmstrblk;

    MT_LOCK_BIB ();

    pmstr->mb_shutdown = 0;
    pmstr->mb_dbstate = DBOPEN;
    if (!pdbshm->fullinteg)
	pmstr->mb_flags |= NOINTEG;
    else
    if ( !pdbshm->useraw )
	pmstr->mb_flags |= NONRAW;

    MT_UNLK_BIB ();
}

/* PROGRAM: rlGetWarmStartState - Returns value of warm start flag
 *
 * RETURNS:
 */
int
rlGetWarmStartState(dsmContext_t *pcontext)
{

    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    if( pdbcontext->prlctl )
	return (int)pdbcontext->prlctl->rlwarmstrt;
    else
	return 0;
}

/* PROGRAM: rlSynchronousCP - Perform a synchronous checkpoint
 *
 * lock the BI file
 * Flush all the modified datababase blocks to disk then
 * force the blocks out of the OS buffer pool (unless directio)
 * Open a new BI cluster and mark the rlSyncCP in the new
 * cluster.  This cluster will be the start of the redo phase
 * of crash recovery.
 * unlock the BI file
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlSynchronousCP(dsmContext_t *pcontext)
{
    /* Flush all the blocks from the buffer pool */
    bmFlushx(pcontext, FLUSH_ALL_AREAS, 1);

    /* force a flush of all the OS buffers for the open files */
    bkSyncFiles(pcontext);

    /* lock down the BI file so no new blocks can get modified */
    MT_LOCK_BIB ();

    /* close the current cluster and open the next with, marking the
       next one a the start of redo. */
    rlclcls(pcontext,0);
    rlclnxt(pcontext, 1);

    /* all done, unlock the BI file */
    MT_UNLK_BIB ();

    return;
}

/* PROGRAM: fixupArchiveBlock
 */
LOCALF DSMVOID
fixupArchiveBlock(RLCTL *prl, TEXT *pbuf, DBKEY startingBlock)
{
  TEXT   *pend;

  ((RLBLK *)pbuf)->rlclhdr.rlnextBlock = startingBlock +
    (prl->rlclbytes >> prl->rlbiblklog);
  ((RLBLK *)pbuf)->rlclhdr.rlprevBlock = startingBlock -
        (prl->rlclbytes >> prl->rlbiblklog);

  for(pend = pbuf + prl->rlclbytes;pbuf < pend;pbuf+=prl->rlbiblksize) 
  {
    ((RLBLK *)pbuf)->rlblkhdr.rlblknum = startingBlock;
    startingBlock++;
  }
  return;
}


/* PROGRAM: backupThese
 */
LOCALF dsmStatus_t
backupThese(dsmContext_t *pcontext,DBKEY blockNumber,DBKEY toHere, DBKEY *pblocksWritten,
		TEXT *pbuf, fileHandle_t archiveFD,
		TEXT *parchive,int doCurrent)
{
  dbcontext_t   *pdbcontext = pcontext->pdbcontext;
  RLCTL         *prlctl = pdbcontext->prlctl;
  LONG          blocksRead;
  BKTBL         *pbktbl;
  BKFTBL        *pbkftbl;
  int           errorStatus;
  dsmStatus_t   rc = 0;

  pbktbl = &(XRLBF(pdbcontext,prlctl->qinbf)->rlbktbl);
  pbkftbl = XBKFTBL(pdbcontext,pbktbl->bt_qftbl);

  blocksRead = prlctl->rlclbytes >> prlctl->rlbiblklog;
  
  while(((blockNumber != toHere) || doCurrent) && 
	blocksRead  == (prlctl->rlclbytes >> prlctl->rlbiblklog))
  {
    int   bytesWritten;

    blocksRead = bkioRead(pcontext,
		      pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex],
			  &pbkftbl->ft_bkiostats,
			  /* +1 for the extent header  */
			  (gem_off_t)blockNumber+1,
			  pbuf, 
			  prlctl->rlclbytes >> prlctl->rlbiblklog, 
			  prlctl->rlbiblklog, UNBUFFERED);
    blockNumber = (DBKEY)((RLBLK *)pbuf)->rlclhdr.rlnextBlock;
    ((RLBLK *)pbuf)->rlclhdr.rlbiopen = pdbcontext->pmstrblk->mb_biopen;
    fixupArchiveBlock(prlctl,pbuf,*pblocksWritten);
    if(doCurrent)
    {
      ((RLBLK *)pbuf)->rlclhdr.rlnextBlock = 0;
    }
    rc = utWriteToFile(archiveFD,(*pblocksWritten+1) << prlctl->rlbiblklog,
		       pbuf,prlctl->rlclbytes, &bytesWritten, &errorStatus);
    if(rc != 0)
    {
      MSGD_CALLBACK(pcontext,"Write failure to %s errno = %l",
		    parchive,errorStatus);
      goto badret;
    }
    if(doCurrent)
      break;
    *pblocksWritten += (bytesWritten >> prlctl->rlbiblklog);

  }
badret:
  return rc;
}

/* PROGRAM: rlBackup -- Backup the recovery log
 *          Leave the BI subsystem locked!  You must call
 *          bmBackupClear() to unlock the BI subsystem
 *
 * RETURNS: 0  - For success
 */
dsmStatus_t
rlBackup(dsmContext_t *pcontext, dsmText_t *parchive)
{
  dsmStatus_t	rc;
  fileHandle_t  archiveFD;
  RLCTL         *prlctl = pcontext->pdbcontext->prlctl;
  dbcontext_t   *pdbcontext=pcontext->pdbcontext;
  RLBLK         *prlblk;
  int           bytesWritten;
  DBKEY		blockNumber,current, blocksWritten;
  LONG          blocksRead;
  int           errorStatus;
  bkftbl_t        *pbkftbl;
  BKTBL         *pbktbl;
  TEXT          *pbuf;
  TEXT          pBackupCtl[128];
  
  pbuf = utmalloc(prlctl->rlclbytes);
  if(!pbuf)
  {
    MSGD_CALLBACK(pcontext,"Could not allocate %l bytes for rl backup buffer",
		  prlctl->rlclbytes);
    return -1;
  }

  /* Open the archive file    */
  archiveFD = utOsCreat(parchive, CREATE_DATA_FILE,0,&errorStatus);
  
  utOsClose(archiveFD,0);
    

#if OPSYS == WIN32API
  archiveFD = utOsOpen((TEXT *)parchive,
		      OPEN_RW,
		      0, 0, &errorStatus);

#else
  archiveFD = utOsOpen((TEXT *)parchive,
		      OPEN_RW,
		      0, 0, &errorStatus);
#endif
  rc = errorStatus;
  if(rc)
  {
    MSGD_CALLBACK(pcontext,"Unable to open recovery log archive %s %l",
		  parchive,rc);
    return rc;
  }
  /* Lock the recovery log and find the left most cluster that
     would be needed to recover the database if it crashed now.
     This is where we will start backing up the bi file.   */
  MT_LOCK_MTX();
  MT_LOCK_BIB();
  blockNumber = prlctl->rltype1; /* Start with left most cluster */
  do
  {
    prlblk = rlbird(pcontext,blockNumber);
    blockNumber = prlblk->rlclhdr.rlnextBlock;
  }while(rltest3(pcontext,prlblk) == 0 &&
	  rltest1(pcontext,prlblk) == 0 &&
	  rltest2(pcontext,prlblk) == 0 );
  /* Unlock the recovery log                                */
  blockNumber = (DBKEY)prlblk->rlblkhdr.rlblknum;

  /* Backup bi clusters until we get to the currently open
     cluster.                                                */
  /* Since we'll be reading bi blocks from disk flush the bi
     buffers before we start reading.                        */
  rlbiflsh(pcontext,RL_FLUSH_ALL_BI_BUFFERS );
  current = prlctl->rlcurr;
  MT_UNLK_BIB();
  MT_UNLK_MTX();
  pbktbl = &(XRLBF(pdbcontext,prlctl->qinbf)->rlbktbl);
  pbkftbl = XBKFTBL(pdbcontext,pbktbl->bt_qftbl);
  /* Read and write extent header     */
  blocksRead = bkioRead(pcontext,
			pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex],
			&pbkftbl->ft_bkiostats,
			0, pbuf,1,prlctl->rlbiblklog, UNBUFFERED);
  rc = utWriteToFile(archiveFD,0,pbuf, prlctl->rlbiblksize,
		     &bytesWritten,&errorStatus);

  blocksWritten = 0;

  rc = backupThese(pcontext,blockNumber,current, &blocksWritten,
		   pbuf,archiveFD,parchive,0);
  if(rc)
    goto badret;
  
  /* If the online backup test is running, stop here and wait until it
  ** signals that it is done updating the database.
  */
  if (!utgetenvwithsize((TEXT *)"ONLINEBAK_STATUS", pBackupCtl,
                        sizeof(pBackupCtl)))
  {
    if (sticmp(pBackupCtl, (TEXT *)"READY") == 0)
    {
      int pollAttempts = 0;
      fileHandle_t pollFD;

      pollFD = utOsCreat((TEXT *)"olbakstop.tmp", CREATE_DATA_FILE, 0,
                         &errorStatus);
      utOsClose(pollFD, 0);
      do
      {
        utsleep(1);
      } while (utfchk((TEXT *)"olbakstop.tmp", UTFEXISTS) == 0 &&
               pollAttempts++ < 60);
    }
  }

  /* Lock the recovery log                                   */
  MT_LOCK_MTX();
  MT_LOCK_BIB();
  rlbiflsh(pcontext,RL_FLUSH_ALL_BI_BUFFERS );
  blockNumber = current;

  /* Backup bi clusters until we backup the currently open
     cluster.  */
  current = prlctl->rlcurr;
  rc = backupThese(pcontext,blockNumber,current, &blocksWritten,
		   pbuf,archiveFD,parchive,0);  
  if(rc)
    goto badret;
  rc = backupThese(pcontext,current,current, &blocksWritten,
		   pbuf,archiveFD,parchive,1);  
  if(rc)
    goto badret;
#if 0
  MT_UNLK_BIB();
  MT_UNLK_MTX();
#endif
  rc = utReadFromFile(archiveFD,prlctl->rlbiblksize,pbuf,prlctl->rlbiblksize,
		      &bytesWritten, &errorStatus);
  /* Fixup the previous pointer in the first cluster header of the archive */
  ((RLBLK *)pbuf)->rlclhdr.rlprevBlock = current;

  rc = utWriteToFile(archiveFD,prlctl->rlbiblksize,pbuf, prlctl->rlbiblksize,
		     &bytesWritten,&errorStatus);  

  goto goodret;

badret:
  if ( MTHOLDING(MTL_MTX) )
  {
    MT_UNLK_BIB();
    MT_UNLK_MTX();
  }

goodret:
  /* close the archive file                                   */
  utOsClose(archiveFD,0);

  utfree(pbuf);
  /* return success                                           */

  return rc;
}

/* PROGRAM: rlBackupSetFlag - Set the backup status bit in the db file
 *
 * RETURNS: 0  - For success
 */
dsmStatus_t rlBackupSetFlag(dsmContext_t *pcontext,
                            dsmText_t *ptarget,
                            dsmBoolean_t backupStatus)
{
  dsmStatus_t	rc;
  fileHandle_t  targetFD = INVALID_FILE_HANDLE;
  mstrblk_t     mblock;
  int           errorStatus;

  targetFD = utOsOpen((TEXT *)ptarget, OPEN_RW, 0, 0, &errorStatus);
  if (targetFD == INVALID_FILE_HANDLE)
  {
    rc = errorStatus;
    MSGD_CALLBACK(pcontext,"Error %d opening file in rlBackupSetFlag", rc);
    goto err;
  }

  rc = bkReadMasterBlock(pcontext, NULL, &mblock, targetFD);

  if (rc)
  {
    MSGD_CALLBACK(pcontext,"Error %d reading masterblk in rlBackupSetFlag", rc);
    goto err;
  }
  else
  {
    if (backupStatus)
    {
      mblock.mb_flags |= DBBACKUP;
    }
    else
    {
      mblock.mb_flags &= ~DBBACKUP;
    }
  }

  rc = bkWriteMasterBlock(pcontext, NULL, &mblock, targetFD);
  if (rc)
  {
    MSGD_CALLBACK(pcontext,"Error %d writing masterblk in rlBackupSetFlag", rc);
    goto err;
  }

err:
  if (targetFD != INVALID_FILE_HANDLE)
  {
    rc = utOsClose(targetFD, 0);
  }

  return rc;
}

/* PROGRAM: rlBackupGetFlag - Get the backup status bit in the db file
 *
 * RETURNS: 0  - For success
 */
dsmStatus_t rlBackupGetFlag(dsmContext_t *pcontext,
                            dsmText_t *ptarget,
                            dsmBoolean_t *pbackupStatus)
{
  dsmStatus_t	rc;
  fileHandle_t  targetFD = INVALID_FILE_HANDLE;
  mstrblk_t     mblock;
  int           errorStatus;

  targetFD = utOsOpen((TEXT *)ptarget, OPEN_RW, 0, 0, &errorStatus);
  if (targetFD == INVALID_FILE_HANDLE)
  {
    rc = errorStatus;
    MSGD_CALLBACK(pcontext,"Error %d opening file in rlBackupGetFlag", rc);
    goto err;
  }

  rc = bkReadMasterBlock(pcontext, NULL, &mblock, targetFD);

  if (rc)
  {
    MSGD_CALLBACK(pcontext,"Error %d reading masterblk in rlBackupGetFlag", rc);
    goto err;
  }
  else
  {
    if ((mblock.mb_flags & DBBACKUP))
    {
      *pbackupStatus = TRUE;
    }
    else
    {
      *pbackupStatus = FALSE;
    }
  }

err:
  if (targetFD != INVALID_FILE_HANDLE)
  {
    rc = utOsClose(targetFD, 0);
  }

  return rc;
}

