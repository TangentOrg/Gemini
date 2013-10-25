
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
#include "shmpub.h"
#include "bmpub.h"
#include "bkmsg.h"
#include "bkpub.h"
#include "dbcontext.h"
#include "dbmgr.h"
#include "dbpub.h"
#include "dbvers.h"
#include "dsmpub.h"
#include "latpub.h"
#include "latprv.h"
#include "mstrblk.h"
#include "objblk.h"
#include "tmprv.h"
#include "rlpub.h"
#include "rlprv.h"
#include "rlai.h"
#include "rlmsg.h"
#include "rldoundo.h"
#include "stmpub.h"
#include "stmprv.h"
#include "tmmgr.h"
#include "tmpub.h"
#include "usrctl.h"
#include "utbpub.h"
#include "utcpub.h"
#include "utspub.h"
#include "uttpub.h"

#if OPSYS==WIN32API
#include <time.h>
#endif


LOCALF	DSMVOID	rlbidte		(dsmContext_t *pcontext);
LOCALF  DSMVOID    warmstrt	(dsmContext_t *pcontext, int mode);
LOCALF	DSMVOID	rlclfix		(dsmContext_t *pcontext);
LOCALF	DSMVOID	rlinitn		(dsmContext_t *pcontext, RLCURS *prlcurs,
				 LONG nextBlock, TEXT *passybuf);
LOCALF	DSMVOID	rlclropn	(dsmContext_t *pcontext, LONG eofBlock, 
                                 LONG eofOffset);
LOCALF	DSMVOID	rlpbcrsh	(dsmContext_t *pcontext, RLCURS  *prlcurs);
LOCALF	DSMVOID	rlforce		(dsmContext_t *pcontext);
LOCALF	DSMVOID	rlchk		(dsmContext_t *pcontext);
LOCALF  int     rlfixclhdr	(dsmContext_t *pcontext, RLBLK *prlblk);
LOCALF  int     chkdte		(dsmContext_t *pcontext);
LOCALF  int     dorollf		(dsmContext_t *pcontext, LONG clusterBlock,
				 TEXT *assybuf);
LOCALF  int     rlredo		(dsmContext_t *pcontext, RLNOTE *prlnote,
				 RLCURS  *prlcurs,  
       DSMVOID (*pfunc)(dsmContext_t *pcontext, RLNOTE *pNote,
                     BKBUF *pblk,COUNT len,TEXT *p));
LOCALF  int     rdbifwd		(dsmContext_t *pcontext, TEXT *pbuf, int len,
				 RLCURS *prlcurs);
LOCALF RLNOTE * rlrdnxt		(dsmContext_t *pcontext, RLCURS *prlcurs);
LOCALF int rlairedo		(dsmContext_t *pcontext, RLNOTE *pnote,
				 RLCURS *prlcurs, LONG64 rldepend);
LOCALF
DSMVOID
rlEmptyBlockFixup(dsmContext_t *pcontext);

#define REDO_DB 1
#define REDO_AI 2

#ifdef RLTRACE
LOCALF DSMVOID rlnttrace (dsmContext_t *pcontext, TEXT tag, RLNOTE *pnote,
                       LONG noteBlock, LONG noteOffset);
#define MSGD MSGD_CALLBACK
#define RL_NRDTRACE(pn, nb, no) rlnttrace (pcontext, 'R', pn, nb, no)
#define RL_NWRTRACE(pn, nb, no) rlnttrace (pcontext, 'W', pn, nb, no)
#else
#define RL_NRDTRACE(pn, nb, no)
#define RL_NWRTRACE(pn, nb, no)
#endif


/* Import the pointer to the do/undo routine dispatch table  */
IMPORT struct rldisp *prldisp;

/* rlset.c - recovery log file (.bi file) open and warm start programs	*/
/*	     this file is used only during startup and can be overlayed	*/


/* PROGRAM: rlnttrace - trace notes to log file
 *
 * RETURNS: 
 */
#ifdef RLTRACE
LOCALF DSMVOID
rlnttrace (
        dsmContext_t *pcontext,
        TEXT         tag,
        RLNOTE       *pnote,
        LONG         noteBlock,
        LONG         noteOffset)
{
        RLCTL   *prl = pcontext->pdbcontext->prlctl;
        COUNT   i;
        TEXT    f[8];
 
    i = 0;
    if (xct((TEXT *)&pnote->rlflgtrn) & RL_LOGBI) f[i] = (TEXT)'L';
    else f[i] = (TEXT)' ';
    i++;
    if (xct((TEXT *)&pnote->rlflgtrn) & RL_PHY) f[i] = (TEXT)'P';
    else f[i] = (TEXT)' ';
    i++;
    if (xct((TEXT *)&pnote->rlflgtrn) & RL_UNPHY) f[i] = (TEXT)'U';
    else f[i] = (TEXT)' ';
    i++;
    f[i] = (TEXT)'\0';
 
    MSGD (pcontext, 
         (TEXT*)"1c %l %l %s %s dbk %D pbb %l pbo %l nrd %l updctr %l",
         tag, noteBlock, noteOffset, f, prldisp[pnote->rlcode].rlname,
         xdbkey ((TEXT *)&pnote->rldbk), prl->rlpbkbBlock, prl->rlpbkbOffset, 
         prl->rlpbnundo, xlng64 ((TEXT *)&pnote->rlupdctr));
}
#endif

/* PROGRAM: rlInit - Initialize rlctl structure and allocate 
 *                   bi buffers.
 *
 * RETURNS: Pointer to rl control structure
 *          If failure NULL is returned
 */
RLCTL *
rlInit(
	dsmContext_t	*pcontext,
	COUNT		 clusterSize,    /* Size of bi cluster in 16k units */
	COUNT		 biBlockSize,    /* Size of bi blocks in 1k units   */
	ULONG		 biArea)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    RLCTL    *prl;
    QRLBF    q;
    int      i;
    RLBF     *prlbf;
    BKTBL    *pbktbl;

    /* setup rlctl */
    pdbcontext->prlctl =
    prl = (RLCTL *)stGet(pcontext, (STPOOL *)QP_TO_P(pdbcontext, pdbpub->qdbpool), 
			 (unsigned)(sizeof(struct rlctl)));

    if( prl == NULL )
    {
	/* rlInit: Could not get memory for rl control */
	MSGN_CALLBACK(pcontext, rlMSG106); 
	return prl;
    }

    pdbpub->qrlctl = P_TO_QP(pcontext, prl);

    /* put blocksize and log2 blocksize and block mask in convienient place */
    prl->rlbiblksize = biBlockSize;
    prl->rlbiblklog = bkblklog(prl->rlbiblksize);
    prl->rlbiblkmask = (ULONG)0xFFFFFFFF << prl->rlbiblklog;

    /* setup number of bi buffers */
    prl->numBufs = pdbpub->argbibufs;
    if (prl->numBufs < 3) prl->numBufs = 3;

    /* compute cluster size in bytes and total capacity of the .bi file */
    prl->rlclbytes = (LONG)clusterSize * 16384L;
    prl->rlcap = (prl->rlbiblksize - sizeof(RLBLKHDR)) 
	          * (prl->rlclbytes >> prl->rlbiblklog);

    /* Set defaults for txe lock granting limits   */
    prl->txeMaxUpdateLocks = 1;
    prl->txeMaxCommitLocks = 1;

    /* Set test time adjustment to invalid value */
    prl->rlTestAdjust = RL_INVALID_TIME_VALUE;

    /* allocate bi buffers and link to free chain */
    prl->numDirty = 0;
    q = 0;
    for (i = prl->numBufs; i > 0; i--)
    {
        prlbf = (RLBF *)stGet (pcontext, 
                              (STPOOL *)QP_TO_P(pdbcontext, pdbpub->qdbpool),
                              (unsigned)(sizeof(RLBF)));
	QSELF (prlbf) = P_TO_QP(pcontext, prlbf);

        prlbf->bufstate = RLBSFREE;

	pbktbl = &(prlbf->rlbktbl);
        QSELF (pbktbl) = P_TO_QP(pcontext, pbktbl);

        pbktbl->bt_dbkey = -1;	/* buffer is empty */
        pbktbl->bt_raddr = -1;
	pbktbl->bt_area = biArea;
	pbktbl->bt_ftype = BKBI;

        bmGetBufferPool(pcontext, pbktbl);	/* get a buffer	*/

	q = QSELF (prlbf);

	/* add to free chain and chain through all */
        prlbf->qnext = prl->qfree;
        prlbf->qlink = prl->qfree;
	prl->qfree =  q;
    }

    /* save head of chain */
    prl->qpool = q;

    /* set the output buffer pointer in rlctl */
    prl->qoutbf = prl->qfree;
    prl->qfree = XRLBF(pdbcontext, prl->qfree)->qnext;
    prl->qoutblk = (QRLBLK)XRLBF(pdbcontext, prl->qoutbf)->rlbktbl.bt_qbuf;
    XRLBF(pdbcontext, prl->qoutbf)->bufstate = RLBSOUT;

    /* set the input buffer pointer in rlctl */
    prl->qinbf = prl->qfree;
    prl->qfree = XRLBF(pdbcontext, prl->qfree)->qnext;
    prl->qinblk = (QRLBLK)XRLBF(pdbcontext, prl->qinbf)->rlbktbl.bt_qbuf;
    XRLBF(pdbcontext, prl->qinbf)->bufstate = RLBSIN;

    return prl;
}
#define RLMINXTN   ((3 * prl->rlclbytes) >> prl->rlbiblklog)

/* PROGRAM: rlseto - do a warm restart 
 *
 * RETURNS:	0 - A-OK		
 *	    non-0 - Unable	
 */
int
rlseto(
	dsmContext_t	*pcontext,
	int		 mode)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    RLCTL    *prl;
    int	     crashed = 0;
    MSTRBLK  *pmstr = pdbcontext->pmstrblk;
    LONG     clblocks;
    LONG     clusterSize;
    BKFTBL   *pbkftbl = (struct bkftbl *)NULL;
    LONG      ret;
    LONG     newCluster;
    LONG     xvar;
    LONG64   bithreshold = 0;
    int      previousClusterSize;

    TRACE_CB(pcontext, "rlseto")

      previousClusterSize = pmstr->mb_rlclsize;

    MT_LOCK_BIB ();
#ifdef VARIABLE_LOG_CLUSTER_SIZE
    /* set the possibly new cluster size */
    if(pmstr->mb_bistate == BITRUNC)
      pmstr->mb_rlclsize = pdbcontext->rlclsize;
#endif
    
    /* patch up the masterblock value if it contains obsolete zero */
    if (pmstr->mb_rlclsize == 0) pmstr->mb_rlclsize = 1;

    prl = rlInit(pcontext, pmstr->mb_rlclsize, 
                 pmstr->mb_biblksize, DSMAREA_BI );
    if ( prl == NULL )
    {
	return -1;
    }

    /* If the (-bithold) parameter is passed in, fix it up and store it */
    if (pdbpub->bithold != 0) /* pdbpub->bithold is set in drentctl_init() */
    {
        /* bi threshold is provided in MBs, convert to blocks and
           round down by a cluster */
        bithreshold =
            ( (((LONG64)pdbpub->bithold * 1048576) / prl->rlbiblksize)
                  - (prl->rlclbytes / prl->rlbiblksize) );

        /* Now store in a convenient location */
        pdbpub->bithold = bithreshold;
    }


    /* process the -F parameter first before doing any other proessing.
       The only way confirmforce gets set is by specifying -F to the
       proutil truncate bi command.  */
    if (pdbcontext->confirmforce) 
    {
        rlforce(pcontext);
    }

    /* write the -i and -r warning messages to both log file and screen */
    if (!pdbpub->fullinteg)
	MSGN_CALLBACK(pcontext, rlMSG016);
    else if (!pdbpub->useraw)
	MSGN_CALLBACK(pcontext, rlMSG015);


    /* print some messages and set some flags if there was a crash */
    if (pmstr->mb_dbstate == DBOPEN && !(mode & DBAIROLL))
    {	
        crashed = 1;

        if (pdbpub->bithold != 0)
        {
            /* since database has crashed disable the bi threshold size
               until crash recovery is complete */
            bithreshold = pdbpub->bithold;
            pdbpub->bithold = 0;
        }

	rlchk(pcontext);
    }

    MT_UNLK_BIB ();
    /* now, open up the after-image  manager */
    /* is a-image enabled for this database ? */
    if(pdbpub->qainame && (!(mode & DBAIROLL)))
    {
        MSGD_CALLBACK(pcontext,
         "%BThe -a parameter is only valid with rfutil -C roll forward");
        return DB_EXBAD;
    }
    
    if (rlaiqon(pcontext))
    {
	if (rlaiopn(pcontext, AIOUTPUT, XTEXT(pdbcontext, pdbpub->qainame),
                    0, (int)0, (int)0))
	    return -1;
    }
    if ( rltlQon(pcontext))
        if( rltlOpen(pcontext))
           return -1;
    
    /* get the current .bi file size, rounded down to multiple of clstr*/

    clblocks = bkflen(pcontext, DSMAREA_BI);
    MT_LOCK_BIB();

    clusterSize = prl->rlclbytes >> prl->rlbiblklog;
    prl->rlsize = clblocks - (clblocks % clusterSize);

    /*	If the .bi file doesnt need to exist because the last session	*/
    /*	ended during a quiescent point, then there is no warm restart	*/
    /*	to be done.							*/
    /*	We also do a cold start if argforce (the -F parameter)		*/

    if (pmstr->mb_bistate == BITRUNC)
    {
       /* if single-file BI then we must check before we truncate */
        ret = bkGetAreaFtbl(pcontext, DSMAREA_BI,&pbkftbl);

        if (!pbkftbl)
        {
            MT_UNLK_BIB ();
            MSGN_CALLBACK(pcontext, bkMSG139); 
            return -1;
        }
	if (pbkftbl->ft_type & BKSINGLE)
        {
            /* truncate only allowed file 93-04-19-055 */
            if (chkdte(pcontext) == -1)
            {
               /* check failed, cannot continue 94-10-18-022 */
               MT_UNLK_BIB ();
               return -1;
            }
         }


#ifdef VARIABLE_LOG_CLUSTER_SIZE
	if(pmstr->mb_rlclsize != previousClusterSize)
	{
	  /* truncate the .bi file but dont delete it	*/
	  rlbitrnc(pcontext, 0, pmstr->mb_rlclsize, 0);
	}
        else
#endif
	  /* truncate the BI file, but don't change the cluster size */
	  rlbitrnc(pcontext, 0, 0, 0);

	prl->rlsize = 0;
        rlInitRLtime (pcontext); /* init session time in masterblock
                                    and rlctl */

	rlclxtn(pcontext, 0L, 4, &newCluster); /* create 4 clusters */
        /* if rlclxtn failed to make the minimun required clusters, 
           then fail */
        xvar = RLMINXTN;
        if (prl->rlsize < RLMINXTN)
        {
            /* ** Insufficient disk space to extend the .bi */
            MSGN_CALLBACK(pcontext, rlMSG113);
            MT_UNLK_BIB();
            return -1;
        }
	rlbidte(pcontext); /* Put identical time stamp in mstrblk and .bi */
	tmalc(pcontext, pdbpub->argnclts);/* get a trans table of the reqstd size*/

	/* if there was no roll forward to find the ai eof */
	/* then get the ai eof out of the master block	   */
        if (pdbpub->qaictl)
        {
            MT_UNLK_BIB ();
            rlaisete(pcontext);
            MT_LOCK_BIB ();
        }
        
	rlclopn(pcontext, 0L, 0L);	/* open the 1st cluster in the file */
	prl->rlopen = 1;/* allows close to flush buffers set mb_rltime*/
    }
    else
    {

        /* check the dates are OK, and reset them to new values */
	if(chkdte(pcontext) == -1)
        {
            MT_UNLK_BIB ();
            return -1; /* return indicates bad bi file */
        }
	warmstrt(pcontext, mode);/* do a warm start */

	/* shrink the xaction table to the reqstd size  */

	tmrealc(pcontext, pdbpub->argnclts);
    }

    /* in the case of a crashed db, check to ensure the bi size
       hasn't grown past the size of the requested threshold.
       if it has, don't start, otherwise continue with the requested
       bi threshold */
    if (crashed)
    { 
        /* Comparison is done in blocks, don't forget we subtracted a
           cluster from bithreshold */
        if ((bithreshold != 0) &&
               (bithreshold <= pdbcontext->prlctl->rlsize))
        {
            /* BI file size is greater than threshold */
            MSGN_CALLBACK(pcontext, rlMSG123);
            /* You must increase the BI Threshold or truncate the BI file */
            MSGN_CALLBACK(pcontext, rlMSG124);
            MT_UNLK_BIB ();
            dbSetClose(pcontext, DB_NICE_EXIT);
            dbExit(pcontext, DB_NICE_EXIT, DB_EXGOOD);
        }
        else
        {
            /* restore bi threshold value */
            pdbpub->bithold = bithreshold;
        }
    }


    if (pmstr->mb_tainted & (NOINTEG + ARGFORCE))
    {
	/* both to the log file and console */
	MSGN_CALLBACK(pcontext, rlMSG029);
    }
    else
    if (pmstr->mb_tainted & NONRAW)
	MSGN_CALLBACK(pcontext, rlMSG030);   /* goes to log file */

    if (pmstr->mb_tainted & DBRESTORE)
    {
	/* database has not been restored successfully */
	MSGN_CALLBACK(pcontext, rlMSG023);
    }

    /* check and see if the bi file size is larger than or equal to the size
       of the bi threshold.  If it is, don't allow successful start up */
    if (pdbpub->bithold != 0)
    {
        /* make sure the bi file size is not larger than bi threshold
           don't forget we subtracted a cluster from bithreshold */
        if (prl->rlsize >= bithreshold)
        {
            /* BI file size is greater than threshold */
            MSGN_CALLBACK(pcontext, rlMSG123);
            /* You must increase the BI Threshold or truncate the BI file */
            MSGN_CALLBACK(pcontext, rlMSG124);
            MT_UNLK_BIB ();
            return -1;
        }
        else
        {
            pdbpub->bithold = bithreshold;
        }
    }

    MT_UNLK_BIB ();
    return 0;
}


/*
 * PROGRAM: rlfixhdr - fixup problems with bi cluster headers
 *
 * This procedure can be called with a cluster header and it will repair the
 * header before further processing on the header is done.
 *
 * RETURNS: number of changes made
 */
LOCALF  int
rlfixclhdr(
	dsmContext_t	*pcontext _UNUSED_,
	RLBLK		*prlblk)
{
    int changes = 0;

    /* early V7.1C bi files have the default cluster size stored in bytes*/
    if (prlblk->rlclhdr.rlclsize == 16384)
    {
	/* a value of 16384 means 16k bytes - should be 1 16K unit */
	prlblk->rlclhdr.rlclsize = 1;
	changes++;
    }
    
    return changes;
}

/*
 * PROGRAM: chkdte - Check mstrblk and .bi file have the same open date 
 *
 * This procedure reads the 1st block of the .bi file and makes sure it
 * matches the database.  Both the cluster size and open dates are checked.
 *
 * This code has to deal with possible inconsistancies:
 *
 * 1) The bi file doesn't exist, but is expected to exist, the bi file was
 *    somehow lost or deleted and needs to be found to continue - error.
 *
 * 2) The bi file exists, was truncated, but was crashed while attempting to 
 *    create the initial clusters in the bi file - and just contains zeros.
 *
 * 3) The bi file exists, but the last open was interrupted if its date
 *    stamp matches mb_biprev, then the last call to rlbidte was 
 *    interrupted by a crash.  In this case, we will simply finish up what 
 *    was started.
 *
 * RETURNS: 0 on success, -1 on failure,
 *          issues fatal error if there is a mismatch between files.
 */
LOCALF int
chkdte(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLBLK	*prlblk;
    MSTRBLK	*pmstr = pdbcontext->pmstrblk;
    TEXT         timeString[TIMESIZE];

    TRACE_CB(pcontext, "rl: chkdte")

    /* check to see if the size of the file is zero */
    if (pdbcontext->prlctl->rlsize == 0)
    {
	/* size is zero, do we expect it to have something in it */
        if (pmstr->mb_bistate == BIEXIST)
	{
            /* we expected something in the file, it is zero length */
	    MSGN_CALLBACK(pcontext, rlMSG048);
            return -1;
	}

	/* no file to check */
        return 0;
    }

    /* read the first block in the file */
    prlblk = rlbird(pcontext, 0L);

    /* fix up the cluster header if needed before processing info from it */
    rlfixclhdr(pcontext, prlblk);

    /* check to see if this is a partially created bi file (all zeros) */
    if (prlblk->rlclhdr.rlclsize == 0 &&     /* last field in cluster header */
	prlblk->rlblkhdr.rlcounter == 0)     /* first field in block header */
    {
	/* header is zero, do we expect it to have something in it */
        if (pmstr->mb_bistate == BITRUNC)
	{
	    /* zero'd file, nothing expected, all should be well */
	    return 0;
	}

	/* let the following remaining checks catch the problem */
    }

    /* the cluster size should match */
    if (prlblk->rlclhdr.rlclsize != pmstr->mb_rlclsize)
    {
	/* something is wrong, mismatch on cluster size */
        FATAL_MSGN_CALLBACK(pcontext, rlFTL035);
    }

    /* check to see if bi matches previous date and fixup if needed */
    if (   pmstr->mb_biprev != 0
	&& pmstr->mb_biprev == prlblk->rlclhdr.rlbiopen)
    {
	/* bi matches previous open, fixup dates so they match */
	prlblk->rlclhdr.rlbiopen = pmstr->mb_biopen;
	rlbiwrt(pcontext, prlblk);
	pmstr->mb_biprev = 0;	/* If we didnt do this, executed for a	 */
				/* while, then crashed, the old .bi would*/
				/* still match the new .db and that would*/
				/* be bad				 */
        MT_UNLK_BIB ();
	bmFlushMasterblock(pcontext);	/* flush the masterblock with raw i/o */
        MT_LOCK_BIB ();
	return 0;
    }

    /* Now check if the .db and .bi have exactly matching time stamps	*/
    /* If they do, then get a new time stamp and put it in both		*/
    if (prlblk->rlclhdr.rlbiopen == pmstr->mb_biopen)
    {	
	rlbidte(pcontext);
	return 0;
    }

    if (pdbcontext->modrestr & DSM_MODULE_DEMOONLY)
    {
	/*msg usually means user has tampered with time stamps*/
	/*Invalid use of progress demo database*/
        FATAL_MSGN_CALLBACK(pcontext, bkFTL018);
    }

    /* the time stamps dont match */
    MSGN_CALLBACK(pcontext, rlMSG020,
                  uttime((time_t *)&pmstr->mb_biopen,
                         timeString, sizeof(timeString)));
    MSGN_CALLBACK(pcontext, rlMSG021, 
                  uttime((time_t *)&prlblk->rlclhdr.rlbiopen,
                          timeString, sizeof(timeString)));
    MSGN_CALLBACK(pcontext, rlMSG022);

    /* check failed */
    return -1;

} /* chkdte */


/* PROGRAM: rlbidte -- Put identical time stamp in mstrblk and in .bi
 *
 * RETURNS: 
 */
LOCALF DSMVOID
rlbidte(dsmContext_t *pcontext)
{
	RLBLK	*prlblk;
	MSTRBLK	*pmstr = pcontext->pdbcontext->pmstrblk;
		LONG	 newdate;

TRACE_CB(pcontext, "rlbidte")

    time(&newdate);	/* get the current date/time		*/
    /* get the first block of the .bi file	*/
    prlblk = rlbird(pcontext, 0L); 

    /* save the old time stamp in case of a system crash */
    /* and insert the new date in the master block	 */
    pmstr->mb_biprev = pmstr->mb_biopen;
    pmstr->mb_biopen = newdate;

    MT_UNLK_BIB ();
    bmFlushMasterblock(pcontext); /* write out master block with raw i/o */
    MT_LOCK_BIB ();

    /* insert the new date in the .bi file */
    prlblk->rlclhdr.rlbiopen = newdate;
    rlbiwrt(pcontext, prlblk);

    /* and clean up the master block */
    pmstr->mb_bistate = BIEXIST;  /* important */
    pmstr->mb_biprev = 0;   /* If we didnt do this, executed for a    */
			    /* while, then crashed, the old .bi would */
			    /* still match the new .db and that would */
			    /* be bad				      */
    MT_UNLK_BIB ();
    bmFlushMasterblock(pcontext);	/* flush the masterblock with raw i/o */
    MT_LOCK_BIB ();

    if (!pcontext->pdbcontext->pdbpub->fullinteg)
    {
        bkioSync(pcontext);
    }

}
/* PROGRAM: warmstrt -- do a warm start, read the .bi file and
 *			bring the .db and .ai file up to date	
 *
 * RETURNS: 
 */
LOCALF DSMVOID
warmstrt(
	dsmContext_t	*pcontext,
	int		 mode)
{
	TEXT	*passybuf;
	RLCURS	rlcurs;
	int     numLiveTxs;
	int     redoUpdate;
	RLCTL	*prl = pcontext->pdbcontext->prlctl;

TRACE_CB(pcontext, "rl: warmstrt")

    /* bug 93-01-21-035: allocate assembly buffer with stRent */

    passybuf = stRent (pcontext, pdsmStoragePool, (unsigned)MAXRLNOTE);

    prl->rlwarmstrt = 1;

    /* phase 1, repair damaged .rl file	*/
    rlclfix(pcontext);

    /* allocate a xaction and ready-to-commit tables big
     * enough for roll forward.
     */

    /* The size of the transaction table and the maxtrn value in the
     * table must all match the trmask value when performing a warmstrt
     * The transaction id obtained from the inmem note
     * may mask to a slot larger than prl->rlmaxtrn (the max active
     * transaction in a given cluster) and smaller than the mask value
     * calculated. If such a case occurs, the transaction table will be
     * populated with an entry larger than maxtrn by rlmemrd.  This will
     * cause problems when determining if a transaction is alive.
     */

    tmalc(pcontext, tmInitMask((int)prl->rlmaxtrn) + 1);
 
    /* If rolling forward we go through enough warm start to get 
       the bi logical eof after the last bi note.   */
#if 0
    if( mode & DBAIROLL )
    {
	rlclropn(pcontext, (LONG)0, (LONG)0);
	return;
    }
#endif	
    /* phase 2, roll forward .db and .ai */
    redoUpdate = rlrollf(pcontext, passybuf);

    numLiveTxs = tmntask(pcontext);

    prl->rlwarmstrt = 2;   /* Physical undo            */
	
    /* now re-open the cluster which was open before */
    rlclropn(pcontext, prl->rlRedoBlock, prl->rlRedoOffset);

    prl->rlopen = 1;/* allows close to flush buffers and set mb_rltime*/

    if ( mode & DBAIROLL )
    {
        prl->rlwarmstrt = 0;
	return;
    }

    /* Dialogue with the user to decide whether to commit ready-to-commit
       transactions.
    */
    MT_UNLK_BIB ();
    tmdlg(pcontext, mode);
    MT_LOCK_BIB ();

    /* if a physical backout is in process, then	*/
    /* rlpbkbBlock and rlpbkbOffset is the address of the 
       physical backout begin note */
    rlinitp(pcontext, &rlcurs, prl->rlpbkbBlock, prl->rlpbkbOffset, passybuf);
    if (prl->rlpbkbBlock & prl->rlpbkbOffset)
    {	
        /* phase 3, resume a crashed physical backout */
	rlpbcrsh(pcontext, &rlcurs);
    }
    else
	/* phase 4, do a physical backout */
	rlphase4(pcontext, &rlcurs);

    /* phase 5, do logical backout */
    prl->rlwarmstrt = 3;
    rllbk(pcontext, &rlcurs);
    if(numLiveTxs)
      rlEmptyBlockFixup(pcontext);
    prl->rlwarmstrt = 0;

    /* If any transactions were backed out make sure database
       gets marked as changed and time stamp the change
       or if any ai notes had to be redone then need to set
       db changed flag because the ai file will need to be
       backed up before an AIMAGE BEGIN or AIMAGE NEW command
       can be permitted. */
    if ( numLiveTxs || redoUpdate == REDO_AI )
    {
	MT_UNLK_BIB();
	rlsetfl(pcontext);  /* set DBOPEN flag & set warning bits */
	tmlstmod(pcontext, time((LONG)0));
	MT_LOCK_BIB();
    }
    if( redoUpdate || numLiveTxs )
    {
	/* redo phase changed some blocks or txs were backed out
	   so flush the resulting modified blocks              */
	rlflsh(pcontext, 0);
    }
    stVacate (pcontext, pdsmStoragePool, passybuf);
}
/* PROGRAM: rlclfix - phase 1 of warm start:			
 *			repair the cluster ring and find starting point	
 * RETURNS: 
 */
LOCALF DSMVOID
rlclfix(dsmContext_t *pcontext)
{
	RLCTL	*prl = pcontext->pdbcontext->prlctl;
	LONG	 numclstrs;
	RLBLK	*prlblk;
	LONG	 numseen;       /* number of clusters encountered */
	LONG	 firstBlock;    /* first block in cluster ring */
	LONG	 currentBlock;  /* current block in cluster ring */
	LONG	 previousBlock; /* previous block in cluster ring */
	LONG	 prevctr;
	LONG	 hiwater;	/* MAX block number of valid clusters */

TRACE_CB(pcontext, "rl phase 1: rlclfix")

    prl->rltype1 =
    prl->rltype2 =
    prl->rltype3 = -1;
    /* size of the live transaction table */
    prl->rlmaxtrn = pcontext->pdbcontext->pdbpub->argnclts;

#ifdef TRANSTABLETRACE
    MSGD_CALLBACK(pcontext, (TEXT *)"%Lrlclfix: RLCTL->rlmaxtrn[%l] = pcontext->pdbcontext->pdbpub->argnclts[%l]",
                  prl->rlmaxtrn, pcontext->pdbcontext->pdbpub->argnclts);
#endif /* TRANSTABLETRACE */



    /* read any cluster (the first one) and follow its next pointer	*/
    /* to get to a cluster which is guaranteed to be in the ring	*/
    /*	  (the first cluster might only be half way in the ring)	*/
    prlblk = rlbird(pcontext, 0L);

    firstBlock = prlblk->rlclhdr.rlnextBlock; /* addr of 1st valid cluster*/
    hiwater = firstBlock;  
    numclstrs = (firstBlock / (prl->rlclbytes >> prl->rlbiblklog)) + 1;
    prlblk = rlbird(pcontext, firstBlock);

    numseen = 1;			/* number of valid clusters	*/
    prl->rlcounter = prlblk->rlblkhdr.rlcounter;  /* save max rlcounter	*/

    /* now, scan the whole ring of clusters, doing:			*/
    /*		repair any incorrect prev pointers			*/
    /*		find the leftmost and rightmost clusters		*/
    for(currentBlock=firstBlock;;numseen++)
    {	
        previousBlock = currentBlock;
	currentBlock = prlblk->rlclhdr.rlnextBlock;
	prevctr  = prlblk->rlblkhdr.rlcounter;
	prlblk = rlbird(pcontext, currentBlock);

	if (currentBlock > hiwater)	/* the hiwater mark tells us how */
	{   
            hiwater = currentBlock;	/* many valid clusters to expect */
            numclstrs = (hiwater /(prl->rlclbytes >> prl->rlbiblklog)) + 1;
	}

	/* prev pointer damaged in crash, repair it */
	if (prlblk->rlclhdr.rlprevBlock != previousBlock)
	{
	    prlblk->rlclhdr.rlprevBlock = previousBlock;
	    rlbiwrt(pcontext, prlblk);
	}
	/* keep track of the largest rlcounter and largest maxtrn seen */
	if (prlblk->rlblkhdr.rlcounter > prl->rlcounter)
	    prl->rlcounter = prlblk->rlblkhdr.rlcounter;
	if (prlblk->rlclhdr.rlmaxtrn > prl->rlmaxtrn)
    {
	    prl->rlmaxtrn = prlblk->rlclhdr.rlmaxtrn;

#ifdef TRANSTABLETRACE
    MSGD_CALLBACK(pcontext, (TEXT *)"%Lrlclfix: RLCTL->rlmaxtrn[%l] = RLBLK->rlclhdr.rlmaxtrn[%l]",
                  prl->rlmaxtrn, prlblk->rlclhdr.rlmaxtrn);
#endif /* TRANSTABLETRACE */
    }

	/* is this the rightmost cluster ? */
	if (prlblk->rlblkhdr.rlcounter < prevctr)
        {
            if (prl->rltype1 != -1) 
            {
                FATAL_MSGN_CALLBACK(pcontext, rlFTL036);
            }

            if (prlblk->rlblkhdr.rlcounter == RL_CHECKPOINT)
            {
                /* found that we are in the middle of a checkpoint,
                   hence this will be the rightmost cluster when
                   we fix this up in rltest1()  
                */

                /* leftmost cluster      */
	        prl->rltype1 = prlblk->rlclhdr.rlnextBlock;
                /* rightmost cluster     */
	        prl->rltype2 = currentBlock;	        
            }
            else
            {
                /* leftmost cluster      */
	        prl->rltype1 = currentBlock;	
                /* rightmost cluster     */
	        prl->rltype2 = previousBlock;
            }
	}

	if (   prlblk->rlblkhdr.rlcounter == prevctr
	    && prevctr != 0)
            FATAL_MSGN_CALLBACK(pcontext, rlFTL037);

	/* seen the whole ring? */
	if (currentBlock == firstBlock) break;	/* yes */
	if (numseen >= numclstrs)
            FATAL_MSGN_CALLBACK(pcontext, rlFTL038);
    }

    for(currentBlock = 0L;
	numseen < numclstrs && currentBlock <= hiwater;
	currentBlock += (prl->rlclbytes >> prl->rlbiblklog))
    {
	prlblk = rlbird(pcontext, currentBlock);
	previousBlock = prlblk->rlclhdr.rlprevBlock;

	/* if a cluster points back at itself, or its rlprev points	*/
	/* to a cluster which doesnt point back at it, then it is an	*/
	/* orphan which must be linked back into the ring of clusters	*/
	if (   prlblk->rlclhdr.rlnextBlock == currentBlock
	    || (prlblk=rlbird(pcontext, previousBlock))
		   ->rlclhdr.rlnextBlock != currentBlock)
	{   
            rlclins(pcontext, currentBlock, prl->rltype1);
	    prl->rltype1 = currentBlock;
	    numseen++;
	}
    }

    if (numseen < numclstrs)
        FATAL_MSGN_CALLBACK(pcontext, rlFTL039);

    /* now adjust rlsize to properly indicate the current FORMATTED */
    /* length of the .bi file, ignoring all unformatted blocks	    */
    prl->rlsize = 
        hiwater + (prl->rlclbytes >> prl->rlbiblklog);

    /* if the .bi file is a virgin, then all clusters have rlcounter==0	*/
    /* so the above code will fail to choose a starting point		*/
    if (prl->rltype1 == -1)
    {	prl->rltype1 = 0L;	/* pick one arbitrarily */
	prlblk = rlbird(pcontext, 0L);
	prl->rltype2 = prlblk->rlclhdr.rlprevBlock;
    }

}

/* PROGRAM: rlrollf -- Phase 2 of warm start, roll forward .bi file	 
 *
 * WARNING: the return value gives no information about whether the	
 * memnotes made changes to the master block.  That must be determined
 * some other way	
 *
 * RETURNS: 1 - the roll forward caused a database write
 *	    0 - It didnt cause a database write	
 */
int
rlrollf(
	dsmContext_t	*pcontext,
	TEXT		*assybuf) /* assembly buffer for scanning .bi file */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    LONG	previousCluster;    /* block num of previous cluster */
    LONG	currentCluster;     /* block num of current cluster */
    LONG	firstCluster = -1;  /* block num of first used cluster */
    LONG	syncCluster = -1;   /* temp var for reverse scan */
    RLCTL	*prl = pdbcontext->prlctl;
    RLBLK	*prlblk;
    int         ageTime;

TRACE_CB(pcontext, "rl phase 2: rlrollf")

    prl->rlRedoBlock = 0;
    prl->rlRedoOffset = 0;

    /* initialize the accumulated run time as of last session end */
    rlLoadRLtime (pcontext);

    ageTime = rlAgeLimit(pcontext);

    /* scan the .bi file starting at the leftmost cluster    */
    /* looking for the leftmost one which needs roll forward */
    for(previousCluster = -1, 
        currentCluster = prl->rltype1;
	previousCluster != prl->rltype2;
	previousCluster=currentCluster, 
        currentCluster=prlblk->rlclhdr.rlnextBlock)
    {

	prlblk = rlbird(pcontext, currentCluster);

	/* if this cluster was never opened, skip it */
	if (prlblk->rlblkhdr.rlcounter == 0)
	    continue;
	else
	    if (firstCluster == -1) firstCluster = currentCluster;

	/* We can skip this cluster if it passes all 4 tests:	*/
	/*	It is not the rightmost cluster			*/
	/*	It was closed					*/
	/*	It has aged properly				*/

	if ( currentCluster != prl->rltype2
	    && prlblk->rlclhdr.rlclose > 0
	    && prlblk->rlclhdr.rlclose <= prl->rlclstime - ageTime)
		    continue;

        /* now check to see if there are any syncronous checkpoints
           to the right of this cluster. Travel backwards from the
           right and check until I hit the current cluster */
        for ( syncCluster = prl->rltype2;
              syncCluster != currentCluster;
              syncCluster = prlblk->rlclhdr.rlprevBlock)
        {

	    prlblk = rlbird(pcontext, syncCluster);
            if (prlblk->rlclhdr.rlSyncCP)
            {
                currentCluster = syncCluster;
                break;
            }
        }

        /* reread the current cluster block incase it moved in
           the reverse scan */
	prlblk = rlbird(pcontext, currentCluster);

        if ((previousCluster != -1) && (prlblk->rlclhdr.rlSyncCP == 0))
        {
            /* not the leftmost cluster, so move 1 to the left
	       to allow for delayed checkpoint */
            prlblk = rlbird(pcontext, previousCluster);
	    if( prlblk->rlblkhdr.rlcounter > 0 )
	    {
		currentCluster = previousCluster;
	    }
        }
	/* this cluster might start in the middle of a physical backout,
	   so we have to rember how far it got in case it did not
	   finish */

        prl->rlpbkbBlock = prlblk->rlclhdr.rlpbkbBlock; 
        prl->rlpbkbOffset = prlblk->rlclhdr.rlpbkbOffset;
        prl->rlpbnundo = prlblk->rlclhdr.rlpbkcnt;

	/* start the roll forward */

	return dorollf(pcontext, currentCluster, assybuf);
    }
    /* there were no clusters to roll forward */

    /* if there was no roll forward to find the ai eof	*/
    /* then get the ai eof out of the master block	*/
    if (pdbpub->qaictl)
    {
        MT_UNLK_BIB ();
        rlaisete(pcontext);
        MT_LOCK_BIB ();
    }
    return 0;
}

/* PROGRAM: dorollf - Do a roll forward, starting from the specified block
 *
 * WARNING: the return value gives no information about whether the	 
 * memnotes made changes to the master block.  That must be determined	
 * some other way
 *
 * RETURNS: 1 - the roll forward caused a database write	
 *	    0 - It didnt cause a database write		
 *
 */
LOCALF int
dorollf(
	dsmContext_t	*pcontext,
	LONG		 clusterBlock, /* cluster block to start with     */
	TEXT		*assybuf)  /* an assembly buffer available for use */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLBLK       *prlblk;
    RLCTL	*prl = pdbcontext->prlctl;
    LONG	 prevcls = 0;
    RLCURS	 rlcurs;
    RLNOTE	*prlnote;
    int	         fixstamp=0;
    int          i, numMemnotes;
    LONG         blockInNote;
    LONG         offsetInNote;
    LONG	 updateCount = 0;
    MEMNOTE      *pMemnote;

TRACE1_CB(pcontext, "dorollf clusterBlock=%l", clusterBlock)

    /* Begin Physical Redo Phase */
    MSGN_CALLBACK(pcontext, rlMSG115, clusterBlock);
    blockInNote = clusterBlock;
    offsetInNote = 0;
    /* get a forward cursor*/
    rlinitn(pcontext, &rlcurs, blockInNote, assybuf); 
    prl->rlcurr = clusterBlock;

    /* read the first note in the cluster, to initialize the		*/
    /* memory tables (the live transaction table and free chains	*/

    pMemnote = (MEMNOTE *)rlrdnxt(pcontext, &rlcurs);
    if (pMemnote == NULL)
        FATAL_MSGN_CALLBACK(pcontext, rlFTL040);

    RL_NRDTRACE ((RLNOTE*)pMemnote, rlcurs.noteBlock, rlcurs.noteOffset);

    /* 95-09-05-041 There is no guarantee that the INMEM note will */
    /* be the first note in the cluster after the header.          */
    /* The reason this is true, happens when we are trying to      */
    /* put a note in the cluster, which in turns causes a cluster  */
    /* close (note to be generated) -- which coincides with a need */
    /* for an ai extent switch (another note to be generated.      */
    if ( pMemnote->rlnote.rlcode == RL_AIEXT )
    {
        if (rlainote(pcontext, (RLNOTE *)pMemnote))
        {
            /* make sure the note made it to the .ai file */

            MT_UNLK_BIB ();
            MT_LOCK_AIB ();

            if (rlairedo(pcontext, (RLNOTE *)pMemnote,
                &rlcurs,
                prl->rldepend))
                fixstamp=REDO_AI;
            MT_UNLK_AIB ();
            MT_LOCK_BIB ();
        }
        /* read the next note to continue processing with the INMEM note */
        pMemnote = (MEMNOTE *)rlrdnxt(pcontext, &rlcurs);
        if (pMemnote == NULL)
            FATAL_MSGN_CALLBACK(pcontext, rlFTL040);
    }

    numMemnotes = pMemnote->numMemnotes;
    for ( i = 0; i < numMemnotes; )
    {
	if (pMemnote == NULL)
            FATAL_MSGN_CALLBACK(pcontext, rlFTL040);

	if ( pMemnote->rlnote.rlcode != RL_INMEM )
	{
	    if ( pMemnote->rlnote.rlcode == RL_AIEXT )
            {
                if (((RLNOTE *)pMemnote))
                {
                    /* make sure the note made it to the .ai file */
                    MT_UNLK_BIB ();
                    MT_LOCK_AIB ();
	            if (rlairedo(pcontext, (RLNOTE *)pMemnote,
			         &rlcurs,
			         prl->rldepend))
		                 fixstamp=REDO_AI;
                    MT_UNLK_AIB ();
                    MT_LOCK_BIB ();
                }
                /* read the next note to find the next INMEM note in sequence */
	           pMemnote = (MEMNOTE *)rlrdnxt(pcontext, &rlcurs);
                   continue;
            }
            else
            {
	        FATAL_MSGN_CALLBACK(pcontext, rlFTL078);
            }
	}
	if( pMemnote->noteSequence != ( i + 1 ))
	{
	    FATAL_MSGN_CALLBACK(pcontext, rlFTL079,
		 (LONG)pMemnote->noteSequence, (LONG)i);
	}
	    
	/* pass the note on to rlmemrd */
	rlredo(pcontext, (RLNOTE *)pMemnote, &rlcurs, rlmemrd);

	if (rlainote(pcontext, (RLNOTE *)pMemnote))
	{   
	    /* make sure the note made it to the .ai file */

	    MT_UNLK_BIB ();
	    MT_LOCK_AIB ();

	    if (rlairedo(pcontext, (RLNOTE *)pMemnote,
			 &rlcurs,
			 prl->rldepend))
		         fixstamp=REDO_AI;
	    MT_UNLK_AIB ();
	    MT_LOCK_BIB ();
	}
	i++;
	if ( i < numMemnotes )
	{
	   pMemnote = (MEMNOTE *)rlrdnxt(pcontext, &rlcurs);
	}
    }

    /* now, scan the rest of the notes */

    for (;;)
    {
        /* Save address of last byte of note we process. After the redo is,
           finished, we want to start writing new notes at the next byte */
 
        pdbcontext->prlctl->rlRedoBlock = rlcurs.noteBlock;
        pdbcontext->prlctl->rlRedoOffset = rlcurs.noteOffset;
        blockInNote = rlcurs.noteBlock;
        offsetInNote = rlcurs.noteOffset;
 
        prlnote = rlrdnxt(pcontext, &rlcurs);
        if (prlnote == NULL) break;

        RL_NRDTRACE (prlnote, blockInNote, offsetInNote);

	/* have we moved into a new cluster? */

        if (prlnote->rlcode == RL_INMEM &&
            ((MEMNOTE *)prlnote)->noteSequence == 1) /* 98-11-17-029 */
	{
	    /* bug 92-02-07-005 & 92-02-07-007 and 92-03-05-003         */
	    /* This code was moved from */
	    /* rlmemchk because it works better if the check is done    */
	    /* before writing the RL_INMEM note to the .ai file         */
	    /* because the paictl values match what they were when the  */
	    /* RL_INMEM note was created				*/

            if (pdbcontext->paictl)
	    {
		if( (ULONG)prlnote & ALMASK )
		{
		    /* Align the note before checking it           */
		    bufcop(rlcurs.passybuf, (TEXT *)prlnote, 
			   (int)rlcurs.notelen);
		    prlnote = (RLNOTE *)rlcurs.passybuf;
		}
		pMemnote = (MEMNOTE *)prlnote;
		
		/* But only check it if memnote is in the busy extent       */
		if ( pMemnote->aiupdctr >= pdbcontext->paictl->aiupdctr)
		{
		    if (pdbcontext->pmstrblk->mb_aictr != pMemnote->aiupdctr)
		    {
			/* note counter mismatch */

			FATAL_MSGN_CALLBACK(pcontext, rlFTL024, 
                                            pMemnote->aiupdctr,
			      pdbcontext->pmstrblk->mb_aictr);
		    }
		    if (pdbcontext->paictl->ailoc +
                        pdbcontext->paictl->aiofst
                        != pMemnote->aiwrtloc)
		    {
			/* file write location mismatch */

			FATAL_MSGN_CALLBACK(pcontext, rlFTL025, 
                                            pMemnote->aiwrtloc,
			      pdbcontext->paictl->ailoc + 
			      pdbcontext->paictl->aiofst);
		    }
		}
	    }
	    /* cleanup the previous cluster */
	    /* if any .db or .ai blocks had to be changed, then rewrite  */
	    /* the cluster's time stamp, and all subsequent clusters too */
	    /* WARNING: if you change a cluster's time stamp, you must	 */
	    /* change the time stamps of all subsequent clusters 'cause  */
	    /* cluster re-use logic requires all time stamps to increase */
	    /* from left to right					 */
	    if (fixstamp) 
               rlclcls(pcontext, 1);  /* if closed, then chg close date */

	    /* get the new cluster address */
            /* This calculation is extremely important when dealing    */
            /* with segmented checkpoint notes                         */
            prl->rlcurr = rlcurs.nextBlock -
                 (rlcurs.nextBlock % (prl->rlclbytes >> prl->rlbiblklog));

	    /* if any clusters have close dates out of order, then	*/
	    /* a roll forward must have crashed, all succeeding clstr's	*/
	    /* close dates must be fixed to put them back in order, or	*/
	    /* the cluster reuse logic wont work			*/
	    prlblk = rlbird(pcontext, prl->rlcurr );
	    if (   prlblk->rlclhdr.rlclose != 0
		&& prlblk->rlclhdr.rlclose < prevcls) fixstamp=REDO_DB;
	    prevcls = prlblk->rlclhdr.rlclose;
	}

        /* make sure the note made it to the .ai (.tl) file              */
        /* WARNING: The AIMAGE BEGIN (2PHASE BEGIN) utility              */
        /* must always truncate the .bi file first, cause otherwise,     */
        /*  the next roll forward might   try to put some notes in       */
        /* the .ai (.tl) file which dont belong there.                   */

        if (rlainote(pcontext, prlnote))
        {   
            MT_UNLK_BIB ();
            MT_LOCK_AIB ();

            if (rlairedo(pcontext, prlnote,
                         &rlcurs,
                         prl->rldepend))
                fixstamp=REDO_AI;

            MT_UNLK_AIB ();
            MT_LOCK_BIB ();
        }
        if((prlnote->rlcode == RL_CTEND &&
            ((TMNOTE *)prlnote)->accrej == TMACC)) /* TL note. */
        {
            dsmTrid_t  trid;
            bufcop((TEXT *)&trid,(TEXT*)&(((TMNOTE *)prlnote)->xaction),
                                          sizeof(dsmTrid_t));
            MT_UNLK_BIB();
            rltlSpool(pcontext,trid);
            MT_LOCK_BIB();
        }
        
	/* re-do the physical memory or database effect of the note */

	if (xct((TEXT *)&prlnote->rlflgtrn) & RL_UNPHY)
	{
	    /* this is a redo of an undo that happened during physical
	       backout */

	    if (prl->rlpbkbBlock & prl->rlpbkbOffset) 
                prl->rlpbnundo++;

	    if (rlredo(pcontext, prlnote, &rlcurs,
                       prldisp[prlnote->rlcode].undo_it))
	    {
		fixstamp = REDO_DB;
		updateCount++;
	    }
	}
	else if (xct((TEXT *)&prlnote->rlflgtrn) & RL_PHY)
	switch(prlnote->rlcode) {
	case RL_PBKOUT:

            prl->rlpbkbOffset = rlcurs.noteOffset;
            prl->rlpbkbBlock = rlcurs.noteBlock;
	    prl->rlpbnundo = 0;
	    break;
	case RL_PBKEND:
            prl->rlpbkbOffset = 0;
            prl->rlpbkbBlock = 0;
	    prl->rlpbnundo = 0;
	    break;
	default:
	    if (rlredo(pcontext, prlnote, &rlcurs,
                       prldisp[prlnote->rlcode].do_it))
	    {
		fixstamp = REDO_DB;
		updateCount++;
	    }
	    break;
	}
    }

    /* Physical Redo Phase Completed */
    MSGN_CALLBACK(pcontext, rlMSG135, 
    prl->rlRedoBlock, prl->rlRedoOffset, updateCount);

    return fixstamp;	/* returns 1 if the database or .ai was modified */
}
/* PROGRAM: rlinitn -- initialize an rlcurs before using rlrdnxt 
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlinitn(
	dsmContext_t	*pcontext,
	RLCURS		*prlcurs,    /* pointer to the rl cursor to be init*/
	LONG		 nextBlock,  /* block number of cluster header block*/
	TEXT		*passybuf)   /* points to the assembly buffer	*/
{
	RLBLK	*prlblk;

TRACE1_CB(pcontext, "rlinitn block=%l", nextBlock)

    /* read the cluster header block and initialize the cursor */
    prlblk = rlbird(pcontext, nextBlock);

    prlcurs->rlcounter = prlblk->rlblkhdr.rlcounter;
    prlcurs->rlnextCluster = prlblk->rlclhdr.rlnextBlock;
    prlcurs->noteBlock = 0;
    prlcurs->noteOffset = 0;
    prlcurs->nextOffset = prlblk->rlblkhdr.rlhdrlen;
    prlcurs->nextBlock = nextBlock; 
    prlcurs->status    = 0;
    prlcurs->passybuf = passybuf;
}
/* PROGRAM: rlredo -- redo a physical .bi note during physical roll fwd 
 *
 * RETURNS: 1 - If the database was modified
 *	    0 - If the database was not	
 */
LOCALF int
rlredo(
	dsmContext_t	*pcontext,
	RLNOTE		*prlnote,
	RLCURS		*prlcurs,	/* this cursor "contains" the note */
        DSMVOID (*pfunc)(dsmContext_t *pcontext, RLNOTE *pNote,BKBUF *pblk,COUNT len,TEXT *p))
{
	int	 retcode = 0;
	LONG64	 updctr;
	COUNT	 dlen;
	BKBUF	*pblk;
	ULONG    area;
	TEXT	*pdata;
	DBKEY	 dbkey;
	bmBufHandle_t bufHandle;

TRACE1_CB(pcontext, "rlredo code=%t", prlnote->rlcode)

    /* get a pointer to the data and the data length */

    dlen = prlcurs->notelen - prlnote->rllen;
    if (dlen) pdata = (TEXT *)prlnote + prlnote->rllen;
    else pdata = NULL;

    /* if this is a memory note, it has no data block pointer		*/
    /* if it is a data block note, check if the change has already	*/
    /* been made to the data block					*/

    area = (ULONG)xlng((TEXT *)&prlnote->rlArea);
    if(area == DSMAREA_TEMP)
      return 0;
    dbkey = xdbkey((TEXT *)&prlnote->rldbk);

    if (dbkey == 0) pblk = NULL;
    else
    {
        MT_UNLK_BIB ();

	bufHandle = bmLocateBlock(pcontext, area, dbkey, TOMODIFY);
	pblk = bmGetBlockPointer(pcontext,bufHandle);
	updctr = xlng64((TEXT *)&prlnote->rlupdctr);

        if (pblk->BKUPDCTR >= updctr)
	{
            /* Release the buffer for others to use */
	    bmReleaseBuffer(pcontext, bufHandle); 

            MT_LOCK_BIB ();
	    return 0; /* already done */
	}
	if (pblk->BKUPDCTR != updctr-1)
            FATAL_MSGN_CALLBACK(pcontext, rlFTL041, pblk->BKUPDCTR, updctr);

	pblk->BKUPDCTR++;
	bmMarkModified(pcontext, bufHandle, 0);
	bmReleaseBuffer(pcontext, bufHandle);

        MT_LOCK_BIB ();
	retcode = 1;
    }
    /* if the note header is not aligned, copy it to align it */

    if ((LONG)prlnote & ALMASK)
    {
	bufcop(prlcurs->passybuf, (TEXT *)prlnote, (int)prlcurs->notelen);
	prlnote = (RLNOTE *)prlcurs->passybuf;
        if (dlen) pdata = (TEXT *)prlnote + prlnote->rllen;
    }
    MT_UNLK_BIB ();

    /* call the function */

    (*pfunc)(pcontext, prlnote, pblk, dlen, pdata);

    MT_LOCK_BIB ();
    return retcode;
}
/* PROGRAM: rlclropn -- re-open a cluster which is already open		 
 *			used during database open to reinitialize the	
 *			rlctl fields which describe the open cluster
 * If the .rl file contains no open clusters this program opens a new one
 * expects rlctl.rltype2 to be the lseek address of the rightmost cluster
 * in the .rl file.  This is the only cluster which might be open	
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlclropn(
	dsmContext_t	*pcontext,
        LONG            eofBlock,
        LONG            eofOffset)
{
	LONG	 addrBlock;  /* block number of open cluster */
	LONG	 ctr;
	RLBLK	*prlblk;
	RLCTL	*prl = pcontext->pdbcontext->prlctl;
	LONG	blkused = 0;

TRACE_CB(pcontext, "rlclropn")
#ifdef BITRACE
    MSGD_CALLBACK(pcontext,
               (TEXT *)"%Lrlclropn: cluster at block=%l, eblock=%l, eoffset=%l",
               prl->rltype2, eofBlock, eofOffset);
#endif

    /* prlctl->rltype2 is the rightmost cluster	*/
    /* and is the only possible open cluster	*/
    addrBlock = prl->rltype2;
    prlblk = rlbird(pcontext, addrBlock);

    /* check if it is closed */
    if (   prlblk->rlblkhdr.rlcounter == 0
	|| prlblk->rlclhdr.rlclose != 0)
    {	
        /* it is not open */
	rlclnxt(pcontext,0);	/* open a new one */
	return;
    }

#ifdef RLTRACE
MSGD (pcontext, (TEXT *)"%Lcluster in %l reopen eof in %l at %l", 
      addrBlock, eofBlock, eofOffset);
#endif
    /* it is open, if the transaction table is bigger than last session */
    /* it is necessary to fix the cluster header			*/
    if (prl->rlmaxtrn > prlblk->rlclhdr.rlmaxtrn)
    {	
        prlblk->rlclhdr.rlmaxtrn = prl->rlmaxtrn;

#ifdef TRANSTABLETRACE
    MSGD_CALLBACK(pcontext, (TEXT *)"%Lrlclopn: RLBLK->rlclhdr.rlmaxtrn[%l] = RLCTL->rlmaxtrn[%l]",
                  prlblk->rlclhdr.rlmaxtrn, prl->rlmaxtrn);
#endif /* TRANSTABLETRACE */

        rlbiwrt(pcontext, prlblk);
    }

    /* scan the cluster forwards to find the last valid block		*/
    /* and find out how much usable space has been used (where rldorollf
       ended its scan */
    ctr = prlblk->rlblkhdr.rlcounter;
    prl->rlclused = 0;

    for(;;)
    {
#ifdef RLTRACE
MSGD (pcontext, (TEXT *)"%Lropn: block at %l hdr %i ctr %i used %i",
     addrBlock, prlblk->rlblkhdr.rlhdrlen, 
     prlblk->rlblkhdr.rlcounter, prlblk->rlblkhdr.rlused);
#endif
	/* is this block valid? */
	if (prlblk->rlblkhdr.rlcounter != ctr) break;		/* no  */
	blkused = prlblk->rlblkhdr.rlused;
	prl->rlclused += blkused-sizeof(RLBLKHDR);/*yes*/
	/* read the next block in the cluster */
	addrBlock += 1;

        /* Check that we don't scan past last byte of data */
        if ((eofBlock) && (addrBlock > eofBlock)) 
                break;

        /* Check for end of Cluster by block number */
        if (addrBlock - prl->rltype2 == (prl->rlclbytes >> prl->rlbiblklog))
            break; 
                
	prlblk = rlbird(pcontext, addrBlock);
    }

    /* re read the last block of the cluster into the output buffer */
    addrBlock -= 1;
    prlblk = rlbird(pcontext, addrBlock);
    rlbiout(pcontext, prlblk);	/* designate this the official output buffer */

    if ( (eofBlock == 0) && (eofOffset == 0) )
    {
        /* since we did not do bi roll forward, eof is last byte of block
	   used. This is important for ai roll forward. We must have a
	   properly closed bi file before doing roll forward */
 
        eofBlock = addrBlock;
        eofOffset = prlblk->rlblkhdr.rlused - 1;
    }

    /* setup the rlctl variables */

    /* addr of open cluster */
    prl->rlcurr    = prl->rltype2;      
 
    /* set the correct spot in block, adjust space used in cluster */
 
    prl->rlblOffset = eofOffset + 1;
    prl->rlblNumber = addrBlock;
    prl->rlnxtBlock = addrBlock;
    prl->rlnxtOffset = prl->rlblOffset; 

    prlblk->rlblkhdr.rlused = prl->rlblOffset;
 
    /* adjust for the fact that eof may be less than space used if the last
       bi note was incomplete */
 
    prl->rlclused = prl->rlclused - (blkused - prl->rlblOffset);
 
#ifdef BITRACE
     MSGD_CALLBACK(pcontext,
                  (TEXT *)"%Lrlclropn: nxtBlk=%l, nxtOff=%l",
                  prl->rlnxtBlock, prl->rlnxtOffset);
     MSGD_CALLBACK(pcontext,
                  (TEXT *)"%Lrlclropn: blNum=%l blOff=%l",
                  prl->rlblNumber, prl->rlblOffset);
#endif
#ifdef RLTRACE
MSGD (pcontext, (TEXT *)"%Lcluster reopened in %l at %l", 
      prl->rlnxtBlock, prl->rlnxtOffset);
#endif
}
/* PROGRAM: rlphase4 -- warm start phase 4, do physical backout		
 *
 * RETURNS: 1 - the physical backout caused a database write	
 *	    0 - It didnt cause a database write		
 */
int
rlphase4(
	dsmContext_t	*pcontext,
	RLCURS		*prlcurs)/* cursor to ready to read
                                  * notes to be undone */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLNOTE	    rlnote;   /* rl note for the RL_PBKOUT note */
    LONG	    returnCode;
    RLCTL	    *prl = pdbcontext->prlctl;
    LONG            lastBlock; /* block number of last rlwrite */
    LONG            lastOffset; /* offset within block of last rlwrite */

TRACE_CB(pcontext, "rlphase4")

    /* if there are no live transactions, do nothing */
    if	(tmntask(pcontext) == 0) return 0;

    /* write an RL_PBKOUT note to indicate */
    /* that physical backout is starting   */
    rlnote.rllen   = sizeof(RLNOTE);
    rlnote.rlcode  = RL_PBKOUT;
    sct((TEXT *)&rlnote.rlflgtrn, (COUNT)RL_PHY);
    /* put 0 in the trn id part of note */
    rlnote.rlTrid  = 0;
    rlnote.rldbk   = 0;
    rlnote.rlupdctr= 0;
    returnCode = rlwrite(pcontext, &rlnote, (COUNT)0, PNULL);
    /* Save last address written in case other rl calls */
    lastOffset = prl->rllastOffset;
    lastBlock =  prl->rllastBlock;
    prl->rlpbkbOffset = prl->rllastOffset;
    prl->rlpbkbBlock =  prl->rllastBlock;
    prl->rlpbnundo  = 0;
 
    RL_NWRTRACE (&rlnote, prl->rlpbkbBlock, prl->rlpbkbOffset);

    if (rlainote(pcontext, &rlnote))
    {   
	/* write an ai note for the change */

	MT_UNLK_BIB ();
	MT_LOCK_AIB ();

	rlaiwrt (pcontext, &rlnote, (COUNT)0, PNULL,
		 ++pdbcontext->pmstrblk->mb_aictr, prl->rldepend);
	MT_UNLK_AIB ();
	MT_LOCK_BIB ();
    }

    /* setup a cursor to start reading at the end of the .bi file */
    rlinitp(pcontext, prlcurs, lastBlock, lastOffset, PNULL);

    /* now, do the physical backout */
    rlpbcrsh(pcontext, prlcurs);
    return 1;
}

/* Process private latch to single thread rm undo */
LOCAL latch_t rmUndoLatch;

/* PROGRAM: rllbk - Do logical backout of all live x-actions 
 *
 * RETURNS: 1 - the logical backout caused a database write
 *	    0 - It didnt cause a database write		
 */
int
rllbk(
	dsmContext_t	*pcontext,
	RLCURS		*prlcurs)  /* cursor to read notes to be undone */
{
        dbcontext_t     *pdbcontext = pcontext->pdbcontext;
	int	 numlive;
        UCOUNT	 flags;
	RLMISC	 rlmisc;
	RLNOTE	*prlnote;
	COUNT    dlen;
	TEXT    *pdata;
	LONG	 transnum;

    numlive = tmntask(pcontext);
TRACE3_CB(pcontext, "rlphase 5: rllbk nextblk=%l at nextoff=%l, numlive=%i",
	    prlcurs->nextBlock, prlcurs->nextOffset, numlive)
#ifdef BITRACE
    MSGD_CALLBACK(pcontext, 
    (TEXT *)"%Lrllbk: rlphase 5 Block=%l, Offset=%l",
	    prlcurs->nextBlock, prlcurs->nextOffset);
#endif

    /* if there are no live transactions, do nothing */
    if	(numlive <= 0) return 0;

    /* Begin Logical Undo Phase, %i incomplete transactions */
    /* MSGN_CALLBACK(pcontext, rlMSG117, numlive); */
    MSGN_CALLBACK(pcontext, rlMSG136, numlive);

   
    /* Tell rm layer that logical backout is beginning */
    PL_LOCK_RMUNDO(&rmUndoLatch);
    rmBeginLogicalUndo(pcontext);

    while (numlive > 0)
    {
	prlnote = rlrdprv(pcontext, prlcurs);

        RL_NRDTRACE (prlnote, prlcurs->noteBlock, prlcurs->noteOffset);

	/* save everything we will need from the note	  */
	/* cause the undo routine may cause it to go away */

	flags = xct((TEXT *)&prlnote->rlflgtrn);

	/* skip notes which dont influence logical backout */

	if (flags & RL_LOGBI)
	{
            /* using the trid from the note,
             * look in trantab to see if transaction completed
             */
            transnum = xlng((TEXT *)&prlnote->rlTrid);
            if (tmdtask(pcontext, transnum))
                continue;   /* this xaction completed */

            /* make sure it is in the assembly buffer, cause logical undo
               might clobber the .bi file buffers */

            if (prlcurs->buftype != RLASSYBUF)
            {
	        bufcop (prlcurs->passybuf, (TEXT *)prlnote, prlcurs->notelen);
		prlnote = (RLNOTE *)(prlcurs->passybuf);
            }
	    dlen = prlcurs->notelen - prlnote->rllen;
	    if (dlen > 0) pdata = (TEXT *)(prlnote) + prlnote->rllen;
	    else pdata = (TEXT *)NULL;

	    pcontext->pusrctl->uc_task = transnum;
	    pcontext->pusrctl->uc_lastBlock = 0;
	    pcontext->pusrctl->uc_lastOffset = 0;

	    switch(prlnote->rlcode)
	    {
            case RL_TABRT:
            case RL_RCOMM:
	    case RL_JMP:
	    case RL_TMSAVE: /* Ignore Savepoint notes */
		    break;

	    case RL_TBGN:
                    MT_UNLK_BIB ();

		    tmend (pcontext, TMREJ, (DSMVOID *)0, (int)0);
                    MT_LOCK_BIB ();
		    numlive--;
		    break;
	    default:
                MT_UNLK_BIB ();

                /* call undo dispatcher */

		rlundo(pcontext, prlnote, dlen, pdata);

                MT_LOCK_BIB ();
		break;
	    }
	}
        if (prlnote->rlcode == RL_TEND)
        {
            if (rlainote(pcontext, &rlmisc.rlnote))
            {
                /* write an ai note for the change */
 
                MT_UNLK_BIB ();
                MT_LOCK_AIB ();
 
                rlaiwrt (pcontext, &rlmisc.rlnote, (COUNT)0, PNULL,
                         ++pdbcontext->pmstrblk->mb_aictr,
                         pdbcontext->prlctl->rldepend);
                MT_UNLK_AIB ();
                MT_LOCK_BIB ();
            }
            /* remember where we did this. It gets put in the cluster
               header */
        }
    }

    /* Free storage used by rm for performing logical undos   */
    rmEndLogicalUndo(pcontext);
    PL_UNLK_RMUNDO(&rmUndoLatch);

    /* Logical Undo Phase Completed */
    MSGN_CALLBACK(pcontext, rlMSG118);

    return 1;
}
/* PROGRAM: rlpbcrsh -- Do physical backout after a crash 
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlpbcrsh(
	dsmContext_t	*pcontext,
	RLCURS		*prlcurs)  /* a cursor already setup for scanning */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    RLMISC	 rlmisc2;  /* used for writing PUNDO notes		*/
    RLCTL	*prl = pdbcontext->prlctl;
    RLNOTE	*prlnote;
    LONG	skipCount;
    int	         phyundo;
    BKBUF	*pdblk;
    COUNT	 dlen;
    TEXT	*pdata;
    DBKEY	 dbkey;
    LONG	returnCode;
    int	        undoit;
    bmBufHandle_t bufHandle;
    dsmArea_t   area;
    bkAreaDesc_t *pbkAreaDesc;
    ULONG       numAreaSlots;
    LONG        pbkendblock;
    LONG        pbkendoffset;

    int         numberlive;
    UCOUNT	areaRecbits;
    numberlive = tmntask(pcontext);

TRACE2_CB(pcontext, "rlpbcrsh nextblk=%l at nextoff=%l", prlcurs->nextBlock,
                     prlcurs->nextOffset)

    stnclr(&bufHandle, sizeof(bmBufHandle_t));
#ifdef BITRACE
    MSGD_CALLBACK(pcontext, 
                 (TEXT *)"%Lrlpbcrsh: Block=%l, offset=%l", 
                 prlcurs->nextBlock, prlcurs->nextOffset);
#endif

 
    /* the rlcurs is setup pointing to the note PAST the physical undo
       begin note. Therefore the first step is to backup 1 note to the
       left. Then we read backwards and backout until we reach a point
       where we can switch to logical backout. If prl->rlpbnundo is
       nonzero, then we are in the middle of a crashed physical backout
       and have already backed out that many notes, so we don't want to
       back them out again. We just skip them until we get to the spot
       where we left off */
 
    /* Begin Physical Undo %i transactions at %l */
    MSGN_CALLBACK(pcontext, rlMSG137,
    numberlive, prlcurs->nextBlock, prlcurs->nextOffset);
 
    skipCount = prl->rlpbnundo;

    prlnote = rlrdprv(pcontext, prlcurs);

    /* now, scan backwards undoing every physical note */
    for(;;)
    {

	prlnote = rlrdprv(pcontext, prlcurs); /* read the note		*/

        RL_NRDTRACE (prlnote, prlcurs->noteBlock, prlcurs->noteOffset);
 
        if (prlnote->rlcode == RL_PBKEND)
        {
            /* RL_PBKEND note causes a jump over its physical backout block.
               We can encounter one of these if we crash just after we finish
               physical backout.  
               This leaves the cursor pointing at note to the left of the last
               note we backed out. So the next one we see should cause us
               to switch to logical backout */
 
            pbkendblock = xlng((TEXT *)&((RLMISC *)prlnote)->rlvalueBlock);
            pbkendoffset = xlng((TEXT *)&((RLMISC *)prlnote)->rlvalueOffset);
            rlinitp(pcontext, prlcurs, pbkendblock, pbkendoffset, PNULL);
#ifdef  RLTRACE
MSGD (pcontext, (TEXT *)"%LSkip over old physical backout in %l at %l", 
                        prlcurs->noteBlock, prlcurs->noteOffset);
#endif
            continue;
        }
        /* RCOMM or TEND or LRA cause physical backout */
        /* to stop and switch over to logical backout  */
        /* RL_RTEND is written during 2phase recovery  */
        /* and therefore we cannot be sure that the    */
        /* preciding notes can be done logically.      */
        if (   prlnote->rlcode == RL_TEND  ||
               prlnote->rlcode == RL_CTEND ||
               prlnote->rlcode == RL_RCOMM) 
               break;
        if ( prlnote->rlcode == RL_RTEND ) continue; 
        if ( prlnote->rlcode == RL_TABRT ) continue; 
        if ( prlnote->rlcode == RL_INMEM ) continue; /* bug 92-02-06-012 */
	if ( prlnote->rlcode == RL_AIEXT ) continue;
        if ( prlnote->rlcode == RL_BKMAKE ) continue; /* bug 95-08-01-080 */
	
	if (prlnote->rlcode == RL_TBGN)
	{
	    /* xaction begin, that transaction is completely backed out */

	    if (tmntask(pcontext) == 1) break;	/* last xaction, all done */
	}
	/* now, the only interesting notes are those with PHYS */

	phyundo = xct((TEXT *)&prlnote->rlflgtrn) & RL_PHY;
	if (!phyundo) continue;

        if (skipCount)
        {
            /* We are still skipping over notes that we already backed
               out. Once the counter reaches zero, we will start backing
               out again */
 
            skipCount--;
            continue;
        }
        /* backing out the operation will require writing a new note,   */
	/* so copy the note to safe storage out of the way		*/

	if (prlcurs->buftype != RLASSYBUF)
	{
	    bufcop(prlcurs->passybuf, (TEXT *)prlnote, (int)prlcurs->notelen);
	    prlnote = (RLNOTE *)prlcurs->passybuf;
	}
        /* get a pointer to the data and the data length */

        dlen = prlcurs->notelen - prlnote->rllen;
        if (dlen > 0) pdata = (TEXT *)prlnote + prlnote->rllen;
        else pdata = (TEXT *)NULL;

        dbkey = xdbkey((TEXT *)&prlnote->rldbk);
	area = xlng((TEXT *)&prlnote->rlArea);
	if(area == DSMAREA_TEMP)
	  continue;
	if (dbkey != 0)
	{ 
            MT_UNLK_BIB ();
	    bufHandle = bmLocateBlock(pcontext, area, dbkey, TOMODIFY);
	    pdblk = bmGetBlockPointer(pcontext,bufHandle);
	    /* Block update counter should be at least as large as
	       what is in the note -- otherwise we lost some changes
	       to the block which the redo phase of warm start did not 
	       recover                                                 */
	    if( pdblk->BKUPDCTR < xlng64((TEXT *)&prlnote->rlupdctr) )
	    {
		MSGN_CALLBACK(pcontext,rlMSG138); 
	        MSGN_CALLBACK(pcontext,rlMSG139,
			pdblk->BKDBKEY, pdblk->BKUPDCTR,
			xlng64((TEXT *)&prlnote->rlupdctr));
	        MSGN_CALLBACK(pcontext, rlFTL080);
	    }

	    /* set the new value for the block update counter */

	    slng64 ((TEXT *)&prlnote->rlupdctr, pdblk->BKUPDCTR + 1);
            MT_LOCK_BIB ();
	}
	else
	{
	    pdblk = NULL;
	}
	if (xct((TEXT *)&prlnote->rlflgtrn) & RL_UNPHY)
	{
	    /* This note is an undo of a previous do. To undo the undo,
	       we must do it this time */

	    undoit = 0;
	}
	else
	{
	    /* The note is a do, so we undo it */

	    undoit = 1;
	}
	/* invert the undo flag in the note to write it to the bi file.
	   This makes an undo into a do and a do into an undo in case
	   we crash and have to do the roll forward phase again */

	sct((TEXT *)&prlnote->rlflgtrn,
            (COUNT)(xct((TEXT *)&prlnote->rlflgtrn) ^ RL_UNPHY));

        /* write the note to the bi file */

        returnCode = rlwrite(pcontext, prlnote, dlen, pdata);
        RL_NWRTRACE (prlnote, prl->rllastBlock, prl->rllastOffset);
 
        /* We have written the note so count it */
 
        prl->rlpbnundo++;

        MT_UNLK_BIB ();

        if (rlainote(pcontext, prlnote))
        {   
            /* write to ai file */

            MT_LOCK_AIB ();

            rlaiwrt (pcontext, prlnote, dlen, pdata,
                     ++pdbcontext->pmstrblk->mb_aictr,
		     prl->rldepend);
            MT_UNLK_AIB ();
        }
	/* undo the note */

	if (undoit) RLUNDO(pcontext, prlnote, pdblk, dlen, pdata);
	else RLDO (pcontext, prlnote, pdblk, dlen, pdata);

        if (pdblk)
        {
            pdblk->BKUPDCTR++;
	    bmMarkModified(pcontext, bufHandle, prl->rldepend);
            /* Release the buffer for others to use */
            bmReleaseBuffer(pcontext, bufHandle);
        }
        MT_LOCK_BIB ();
    }

    /* Look if physical backout has left us with any empty blocks. */
    /* Empty blocks below the hiwater mark occur if we had to undo
       advancing the hiwater mark.                                 */
    /* Scan from the hiwater mark down to the first non-empty block
       and add any empty blocks to the free chain */
    numAreaSlots = XBKAREADP
      (pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr)->bkNumAreaSlots;
    for ( area = 1; area < numAreaSlots; area++ )
    {
        pbkAreaDesc =  BK_GET_AREA_DESC(pdbcontext, area);
        if( pbkAreaDesc == NULL )
            continue;

        if ( pbkAreaDesc->bkAreaType != DSMAREA_TYPE_DATA )
        {
            continue;
        }
	if( pbkAreaDesc->bkDelete )
	{
	  /* Delete area don't need to fix up blocks below
	     the high water mark         */
	  continue;
	}
	if(area == DSMAREA_TEMP)
	  continue;

        areaRecbits = pbkAreaDesc->bkAreaRecbits;
        MT_UNLK_BIB();
        bufHandle = bmLocateBlock(pcontext, area, 
                                  BLK2DBKEY(areaRecbits), TOREAD);
        pdblk = bmGetBlockPointer(pcontext,bufHandle);
        pbkAreaDesc->bkHiAfterUndo = ((objectBlock_t *)pdblk)->hiWaterBlock;
        bmReleaseBuffer(pcontext, bufHandle);
        MT_LOCK_BIB();
    } /* End of loop on areas */
    
    /* physical backout completed, write out the PBKEND note */    
    rlmisc2.rlnote.rlcode = RL_PBKEND;
    rlmisc2.rlnote.rllen  = sizeof(rlmisc2);
    sct((TEXT *)&rlmisc2.rlnote.rlflgtrn, (COUNT)RL_PHY);
 
    /* put 0 in the trn Id part of note */
    rlmisc2.rlnote.rlTrid = 0;
    rlmisc2.rlnote.rldbk  = 0;
    rlmisc2.rlnote.rlupdctr=0;
    slng((TEXT *)&rlmisc2.rlvalueBlock, prlcurs->noteBlock);
    slng((TEXT *)&rlmisc2.rlvalueOffset, prlcurs->noteOffset);
 
    returnCode = rlwrite(pcontext, &rlmisc2.rlnote, (COUNT)0, PNULL);
    RL_NWRTRACE (&rlmisc2.rlnote, prl->rllastBlock, prl->rllastOffset);
 
    if (rlainote(pcontext, &rlmisc2.rlnote))
    {
        /* write an ai note for the change */
 
        MT_UNLK_BIB ();
        MT_LOCK_AIB ();
 
        rlaiwrt (pcontext, &rlmisc2.rlnote, (COUNT)0, PNULL,
                 ++pdbcontext->pmstrblk->mb_aictr, prl->rldepend);
        MT_UNLK_AIB ();
        MT_LOCK_BIB ();
    }
    prl->rlpbkbBlock = 0;
    prl->rlpbkbOffset = 0;
    prl->rlpbnundo = 0;

    /* we didnt process the last note we read, so		*/
    /* reinitialize the cursor to re-read the same note again	*/

    rlinitp(pcontext, prlcurs, prlcurs->noteBlock, prlcurs->noteOffset, PNULL);

    /* Physical Undo Completed at %i */
    MSGN_CALLBACK(pcontext, rlMSG120, prlcurs->noteBlock);
}

/* PROGRAM: rlEmptyBlockFixup -
 */
LOCALF
DSMVOID
rlEmptyBlockFixup(dsmContext_t *pcontext)
{
  dbcontext_t    *pdbcontext = pcontext->pdbcontext;
  dsmArea_t      numAreaSlots;
  bkAreaDesc_t  *pbkAreaDesc;
  dsmArea_t      area;
  int            areaRecbits;
  ULONG          hiWaterBlock;
  bmBufHandle_t  bufHandle;
  BKBUF	         *pdblk;
  DBKEY          dbkey;
  
  /* Look if physical backout has left us with any empty blocks. */
  /* Empty blocks below the hiwater mark occur if we had to undo
     advancing the hiwater mark.                                 */
  /* Scan from the hiwater mark down to the first non-empty block
     and add any empty blocks to the free chain */
  numAreaSlots = XBKAREADP
    (pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr)->bkNumAreaSlots;
  for ( area = 1; area < numAreaSlots; area++ )
  {
    pbkAreaDesc =  BK_GET_AREA_DESC(pdbcontext, area);
    if( pbkAreaDesc == NULL )
      continue;

    if ( pbkAreaDesc->bkAreaType != DSMAREA_TYPE_DATA )
    {
      continue;
    }
    if( pbkAreaDesc->bkDelete )
    {
      /* Delete area don't need to fix up blocks below
	 the high water mark         */
      continue;
    }
    if(area == DSMAREA_TEMP)
      continue;

    areaRecbits = pbkAreaDesc->bkAreaRecbits;
    MT_UNLK_BIB();
    hiWaterBlock = pbkAreaDesc->bkHiAfterUndo;
    MT_LOCK_BIB();
    for( dbkey = (DBKEY)hiWaterBlock << areaRecbits;
	 ; dbkey -= (BLK1DBKEY(areaRecbits) ))
    {
      MT_UNLK_BIB();
      bufHandle = bmLocateBlock(pcontext, area,dbkey,TOMODIFY);
      pdblk = bmGetBlockPointer(pcontext,bufHandle);
      if( pdblk->BKTYPE == EMPTYBLK )
      {
	bkfradd(pcontext, bufHandle, FREECHN);
      }
      else
      {
	bmReleaseBuffer(pcontext, bufHandle);
	MT_LOCK_BIB();
	break;
      }
      bmReleaseBuffer(pcontext, bufHandle);
      MT_LOCK_BIB();
    } 
  } /* End of loop on areas */ 
  return;
}

/* PROGRAM: rlrdnxt -- read the next note out of the .bi file		
 * RETURNS:							
 *  NULL     - There are no more notes to be read.	
 *  non-NULL - a pointer to the desired rl note.  The pointer returned	
 *	       may point into the IN, OUT or the ASSY buffer.	
 *	       The pointer will point at the first byte of the rlnote	
 *	       structure, NOT the 2 byte length prefix.		
 *							
 *  In addition to returning a pointer, rlcurs will contain:		
 *     buftype - which buffer the pointer is in.		
 *     notelen - the length of the note (not including the 2
 *		    byte length prefix and suffix).	
 *     status  - either 0 or EOF		
 *					
 * Notes: In certain cases, if the returned note is not in the ASSY	
 *	buffer, then the caller should copy the note to the ASSY buffer.
 *	These cases include:						
 *	- the caller will do any logical operation (cxixgen, rmdel, etc)
 *	  which might clobber the current IN or OUT buffer.		
 *	- the caller will do any operation which requires the note to be
 *	  in correctly aligned storage.					
 */
LOCALF RLNOTE *
rlrdnxt(
	dsmContext_t	*pcontext,
	RLCURS		*prlcurs)  /* pointer to a valid rl cursor */
{
        RLCTL	*prl = pcontext->pdbcontext->prlctl;
	TEXT	*plen;
	RLNOTE	*prlnote;
	RLBLK	*prlblk;
	COUNT	 totlen;
	LONG     offset;
        LONG     blockNumber;

TRACE2_CB(pcontext, "rlrdnxt nxtblk=%l nextoff=%l", prlcurs->nextBlock,
                     prlcurs->nextOffset)
#ifdef BITRACERW
    MSGD_CALLBACK(pcontext, 
    (TEXT *)"%Lrlrdnxt: nextBlock=%l, nextOffset=%l", 
             prlcurs->nextBlock, prlcurs->nextOffset);
#endif

    /* check if the previous call returned EOF */
    if (prlcurs->status != 0) return NULL;

    /* is the entire note in this block? */
    /* If it is we can be efficient	 */
    blockNumber = prlcurs->nextBlock;
    offset = prlcurs->nextOffset;
    if (offset == 0)
    {	
        blockNumber -= (LONG)1;
	offset = prl->rlbiblksize;
    }
    prlblk = rlbird(pcontext, blockNumber);

#if !DGMV     /* DGMV cannot build structure on odd boundaries - 
		 the following code cannot guarantee that structure
		 lies on a word boundary */

    /* if there aren't at least 4 bytes for the length header and   */
    /* trailer, then there is no possibility the whole note is here */
    if (prlblk->rlblkhdr.rlused >= (COUNT)(offset + 2*sizeof(COUNT)))
    {	
        /* get the length prefix and find out if the */
	/* whole note really is in this block	     */
	totlen = xct((TEXT *)prlblk + offset);
        if( totlen < (COUNT)sizeof (RLNOTE) || totlen > MAXRLNOTE )
        {
	   /*MSGD_CALLBACK(pcontext, "%GInvalid note length %i",totlen);*/
           FATAL_MSGN_CALLBACK(pcontext, rlFTL012,totlen);
        }
 	if (prlblk->rlblkhdr.rlused >= offset + totlen)
	{   
            /* yes, the whole note is in this block */
            prlcurs->nextOffset += totlen;

            prlcurs->noteOffset = prlcurs->nextOffset - 1;
	    prlcurs->buftype = RLBLKBUF;
	    prlcurs->notelen = totlen - 2*sizeof(COUNT);

	    /* Check that the note's prefix and suffix lengths match 
	       Hopefully this will detect if block writes to the bi file
	       aren't atomic                                */
	    if( xct((TEXT *)prlblk + offset + totlen - sizeof(COUNT)) !=
	       totlen )
	    {
		MSGD_CALLBACK(pcontext,
                 (TEXT *)"%G rlrdnxt: note prefix and suffix lengths don't match %i %i",
		     totlen,
		     xct((TEXT *)prlblk + offset + totlen - sizeof(COUNT))
		     );
	    }
	    return (RLNOTE *)((TEXT *)prlblk + offset + sizeof(COUNT));
	}
    }
#endif

    /* the note is not (entirely) in this block, use subroutine */

    /* we will assemble the note in the assembly buffer the entire */
    /* note, including length prefix and suffix will be assembled  */
    /* (to assist ai redo).  But to help all the callers, the rlnote */
    /* will be put at an alignment boundary			     */
    plen = prlcurs->passybuf + sizeof(COUNT);
    prlnote = (RLNOTE *)(plen + sizeof(COUNT));

    /* get the length prefix*/
    rdbifwd(pcontext, plen, sizeof(COUNT), prlcurs);
    if (prlcurs->status) return NULL;
    totlen = xct(plen);
    if( totlen < (COUNT)sizeof (RLNOTE) || totlen > MAXRLNOTE )
    {
        MSGD_CALLBACK(pcontext, (TEXT *)"%GInvalid note length %i",totlen);
    }

    rdbifwd(pcontext, (TEXT *)prlnote, (int)(totlen-sizeof(COUNT)), prlcurs);
    if (prlcurs->status) 
        return NULL;
    prlcurs->buftype = RLASSYBUF;
    prlcurs->notelen = totlen - 2*sizeof(COUNT);
    prlcurs->noteOffset = prlcurs->nextOffset - 1;
    prlcurs->noteBlock = prlcurs->nextBlock;

    /* Check that the note's prefix and suffix lengths match 
       Hopefully this will detect if block writes to the bi file
       aren't atomic                                */
    if( xct((TEXT *)prlnote + prlcurs->notelen) != totlen )
    {
	MSGN_CALLBACK(pcontext, rlFTL081,
	     totlen,
	     xct((TEXT *)prlnote + prlcurs->notelen )
	     );
    }
    return prlnote;
}
/* PROGRAM: rdbifwd - read data forwards out of the .bi file
 *
 * RETURNS: 0 - success 
 *      and/or - prlcurs->status set to EOF 
 */
LOCALF int
rdbifwd(
	dsmContext_t	*pcontext,
	TEXT		*pbuf,	/* read the data into this buffer	*/
	int		 len,	/* amount of data to read		*/
	RLCURS		*prlcurs) /* this cursor controls the read	*/
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    LONG        offset;
    LONG        blockNumber;
    RLBLK	*prlblk;
    COUNT	movelen;
    RLCTL	*prl = pdbcontext->prlctl;

    blockNumber = prlcurs->nextBlock;
    offset = prlcurs->nextOffset;
    if (offset == 0)
    {	
        blockNumber -= (LONG)1;
	offset = prl->rlbiblksize;
    }

    prlblk = rlbird(pcontext, blockNumber);

    /* keep moving chunks of data until we have moved it all */
    while(len > 0)
    {
	/* If there is nothing left in the current block, get the next */
	/* while(...) is used in case the next block has no data in it */
	while(offset >= prlblk->rlblkhdr.rlused)
	{   
            /* advance to next block */
            blockNumber += (LONG)1;
	    /* is this the end of this cluster? */
        if ( ((blockNumber % (prl->rlclbytes >> prl->rlbiblklog)) == 0)
              || ((prlblk=rlbird(pcontext, blockNumber))->rlblkhdr.rlcounter !=
                    prlcurs->rlcounter) )
	    {	
               /* yes, end of cluster, get the next one */
                blockNumber = prlcurs->rlnextCluster;
		if (blockNumber == pdbcontext->prlctl->rltype1)
		    return (int)(prlcurs->status=EOF);
		prlblk = rlbird(pcontext, blockNumber);

                if (prlblk->rlblkhdr.rlcounter == RL_CHECKPOINT)
                {
                   /* if this cluster was in the middle of a checkpoint,
                      we need to make this cluster the leftmost, and
                      the previous one the rightmost
                    */
                    pdbcontext->prlctl->rltype1 = blockNumber;
                    pdbcontext->prlctl->rltype2 = prlblk->rlclhdr.rlprevBlock;
		    return (int)(prlcurs->status=EOF);
                }

		if (prlblk->rlblkhdr.rlcounter <= prlcurs->rlcounter)
                {
		    FATAL_MSGN_CALLBACK(pcontext, rlFTL044);
                }

		prlcurs->rlcounter = prlblk->rlblkhdr.rlcounter;
                prlcurs->rlnextCluster = prlblk->rlclhdr.rlnextBlock;
	    }
	    offset = prlblk->rlblkhdr.rlhdrlen;
	}

	/* now, move all we can */
	movelen = prlblk->rlblkhdr.rlused - offset;
	if (movelen > len) movelen = len;
	bufcop(pbuf, (TEXT *)prlblk+offset, (int)movelen);
	pbuf += movelen;
	len -= movelen;
	offset += movelen;
    }

    /* addr of next byte not yet read */
    prlcurs->nextOffset = offset;
    prlcurs->nextBlock = blockNumber;
    return 0;
}

/* PROGRAM: rlforce - Process the -F force parameter 
 *
 * RETURNS: 
 */
LOCALF DSMVOID
rlforce(dsmContext_t *pcontext)
{
	MSTRBLK	*pmstr = pcontext->pdbcontext->pmstrblk;

    /* tell user about forced entry */
    MSGN_CALLBACK(pcontext, rlMSG024);

    /* mark database as permanently suspect */
    pmstr->mb_tainted |= ARGFORCE + pmstr->mb_flags;

    /* turn off after-imageing */
    if (rlaoff(pmstr)==0)
        MSGN_CALLBACK(pcontext, rlMSG005);

    /* print a message about how -F turns ai off */
    MSGN_CALLBACK(pcontext, rlMSG092); /* After-imaging disabled by the FORCE option */
    pcontext->pdbcontext->pdbpub->qainame = P_TO_QP(pcontext, NULL);

    /* close the database and truncate the bi file */
    pmstr->mb_dbstate = DBCLOSED;
    pmstr->mb_flags = 0;
    rlbitrnc(pcontext, 0, 0, 0);
}

/* PROGRAM: rlchk - print crash recovery and other related messages 
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
rlchk (dsmContext_t	*pcontext)
{
	MSTRBLK	*pmstr = pcontext->pdbcontext->pmstrblk;

    /* dont even think about trying to recover a no-integrity crash */
    if ( pmstr->mb_flags & NOINTEG )
    {	MSGN_CALLBACK(pcontext, rlMSG025);
	FATAL_MSGN_CALLBACK(pcontext, rlFTL046);
    }

    /* special message if recovering after a crash with -r */
    if ( pmstr->mb_flags & NONRAW )
    {	MSGN_CALLBACK(pcontext, rlMSG026);
	MSGN_CALLBACK(pcontext, rlMSG027);
	pmstr->mb_tainted |= NONRAW;
	pmstr->mb_flags &= ~NONRAW;
    }

    rlsetfl(pcontext); /* tag master block with appropriate flags */
    MT_UNLK_BIB ();
    bmFlushMasterblock(pcontext);
    MT_LOCK_BIB ();
}
/* PROGRAM: rlairedo - Put an ai note in the ai file if necessary during
 *                     the warm start redo phase. 
 * RETURNS: 
 */
LOCALF int
rlairedo(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,
	RLCURS		*prlcurs,
	LONG64		 rldepend)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    COUNT    notelen;
    int      rc = 0;
    AICTL    *paictl = pdbcontext->paictl;
    struct   mstrblk *pmstrblk = pdbcontext->pmstrblk;
    AIEXTNOTE  *paiext;

    pmstrblk->mb_aictr++;

    if( pnote->rlcode == RL_AIEXT )
    {
	paiext = (AIEXTNOTE *)pnote;
	/* if the note header is not aligned, copy it to align it */
	if((LONG)pnote & ALMASK)
	{
	    bufcop(prlcurs->passybuf, (TEXT *)pnote,
		   (int)prlcurs->notelen);
	    paiext = (AIEXTNOTE *)prlcurs->passybuf;
	}
	if ((ULONG)paiext->ainew < (ULONG)pmstrblk->mb_ai.ainew)
	    goto done;

	if( paiext->extent == pmstrblk->mb_aibusy_extent )
	{
	    paictl->aiupdctr = paiext->aiupdctr;
	    pmstrblk->mb_aictr = paiext->aiupdctr + 1;
	}
	else if ( pmstrblk->mb_aibusy_extent > (TEXT)0 )
	{
	    if( pmstrblk->mb_aibusy_extent + 1 == paiext->extent ||
	        paiext->extent == 1 )
	    {
		rlaiswitch(pcontext, RLAI_SWITCH, 1);
		paictl->aiupdctr = paiext->aiupdctr;
	    }
	    else
	    {
		/* Unexpected extent switch note */
		FATAL_MSGN_CALLBACK(pcontext, rlFTL011); 
	    }
	}
    }
    /* The update counter in the ai control structure is set to the
       counter of first note logged in the extent.  We don't begin
       processing notes until we get into the current extent which
       is what the condition below insures.  */
    if( paictl->aiupdctr > pmstrblk->mb_aictr )
	goto done;

    notelen = prlcurs->notelen + 2*sizeof(COUNT);

    rc = rlaimv(pcontext, (TEXT *)pnote-sizeof(COUNT),
                (COUNT)notelen,
                pmstrblk->mb_aictr, rldepend);
    

done:
    return rc;
}
