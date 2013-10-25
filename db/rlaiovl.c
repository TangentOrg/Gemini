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
#include "bkmsg.h"
#include "bkpub.h"
#include "bmpub.h"
#include "dbcontext.h"
#include "dsmpub.h"
#include "mstrblk.h"
#include "rlpub.h"
#include "rlprv.h"
#include "rlai.h"
#include "rlmsg.h"
#include "stmpub.h"
#include "stmprv.h"
#include "usrctl.h"  /* temp for DSMSETCONTEXT */
#include "uttpub.h"

#if OPSYS==WIN32API
#include "time.h"
#endif

/* RLAIOVL.C */

/*
 * PROGRAM: rlaiVersion - check the ai file version
 *
 * RETURNS:  0 if version ok.
 *           1 otherwise.
 */

LOCALF 
int rlaiVersion(
	dsmContext_t	*pcontext,
	AIBLK		 *paiblk) /* Pointer to ai file header        */
{
    LONG     AiVersion;
    LONG     rc = 1;

    /* Make sure that the header length is in the ball park     */
    if( paiblk->aihdr.aihdrlen > (COUNT)(sizeof(AIHDR) + sizeof(AIFIRST)))
    {
	/* This is not a Progress ai file */
	MSGN_CALLBACK(pcontext, rlMSG104);
	return ( rc );
    }

    /* Copy ai version to avoid alignment errors when testing the
       ai version in what is a bogus block.                        */
    bufcop((TEXT *)&AiVersion,
	   ((TEXT *)paiblk + paiblk->aihdr.aihdrlen - sizeof(LONG)),
	   sizeof(LONG));

    if( AiVersion != AIVERSION )
    {
	/* Invalid after image version number found in ai file header */
	MSGN_CALLBACK(pcontext, rlMSG100); 
	if( paiblk->aihdr.aihdrlen != sizeof(AIHDR) + sizeof(AIFIRST))
	{
	    if ( paiblk->aihdr.aihdrlen == 
		sizeof(AIHDRv0) + sizeof(AIFIRSTv0))
	    {
		/* This is a version 0 Progress ai file. */
		MSGN_CALLBACK(pcontext, rlMSG101); 
		/* The ai file must be truncated.    */
		MSGN_CALLBACK(pcontext, rlMSG102);                   
		/* See documentation for aimage truncate */
		MSGN_CALLBACK(pcontext, rlMSG103); 
		return ( rc );
	    }
	    /* This is not a Progress ai file */
	    MSGN_CALLBACK(pcontext, rlMSG104); 
	    return ( rc );
	}
    }
    rc = 0;
    return ( rc );
}



/*
 * PROGRAM: rlaiblksize - validate ai file blocksize with ai subsystem
 *
 * This function compares the blocksize stored in the first ai block with
 * the blocksize stored in the ai control structure.  If they do not match
 * then we cannot continue.  This code does not issue a fatal exception, but
 * it is important that no I/O be done to the ai file or it will be damaged.
 *
 * RETURNS; 0 if blocksize matches, 1 if it does not match
 */
LOCALF int
rlaiblksize(
	dsmContext_t	*pcontext,
	AIBLK		 *paiblk, /* ptr to 1st ai block in ai file or extent */
	AICTL		*paictl) /* pointer to ai control structure */
{
    
    if (paiblk->aifirst.aiblksize == paictl->aiblksize)
    {
	/* blocksize matches should be ok */
	return 0;
    }
    else
    {
	/* Invalid blocksize in ai file found %d, expected %l */
	MSGN_CALLBACK(pcontext, rlMSG060,
                      paiblk->aifirst.aiblksize, paictl->aiblksize);
	return 1;
    }
} /* rlaiblksize */

/*
 * PROGRAM: rlaiCompare - compare the "on disk" size with the "as written"
 *                        size of the ai file 
 *
 * This function compares the size of the ai file on disk with the size as
 * written that is stored in the header. The "on-disk" size is determined 
 * using bkflen (passed in) which returns the number of blocks. The size 
 * as stored in the header is the number of bytes. To do the comparison
 * we'll expand the "as stored" size to blocks and then round up by one 
 * block.
 *
 * RETURNS; 0 if files compare, 1 if they do not
 */
LOCALF int
rlaiCompare(
	dsmContext_t *pcontext,
	AIBLK        *paiblk, /* ptr to 1st ai block in ai file or extent */
	AICTL        *paictl) /* pointer to ai control structure */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    LONG onDiskBlocks;
    LONG asWrittenBlocks;

    onDiskBlocks = paictl->aisize / pdbcontext->pmstrblk->mb_aiblksize;
    
    if (paiblk->aifirst.aieof % pdbcontext->pmstrblk->mb_aiblksize)
    {
        asWrittenBlocks = 
            paiblk->aifirst.aieof / pdbcontext->pmstrblk->mb_aiblksize + 1;
    }
    else
    {
        asWrittenBlocks = 
            paiblk->aifirst.aieof / pdbcontext->pmstrblk->mb_aiblksize;
    }

    
    if ( onDiskBlocks < asWrittenBlocks )
    {
        /* Issue a message indicating that we have an incomplete ai file */
	MSGN_CALLBACK(pcontext, rlMSG145, asWrittenBlocks, onDiskBlocks);
        return -1;
    }
    return 0;
}

/* PROGRAM: rlaiseto - initialize aictl and buffers
*/
DSMVOID
rlaiseto (dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    AICTL	*paictl;
    BKTBL	*pbktbl;
    AIBUF	*paibuf = (AIBUF *)0;
    AIBUF	*pn;
    int	i;

    /* *** TODO: there is a memory leak when peforming aimage begin on a
     * database that has 2phase on already rlaiseto is called twice.  Not
     * a big deal, but something to remember.
     */
    if (pdbcontext->paictl != NULL)
    {
	/* memory leak! */
    }

    /* allocate the ai control structure */
    paictl = (AICTL *)stGet(pcontext, (STPOOL *)QP_TO_P(pdbcontext, pdbpub->qdbpool),
                                              sizeof(AICTL));
    pdbpub->qaictl = P_TO_QP(pcontext, paictl);
    pdbcontext->paictl = paictl;

    /* initialize blocksize information based on masterblock info */
    paictl->aiblksize = pdbcontext->pmstrblk->mb_aiblksize;
    paictl->aiFileType = BKAI;

    /* initialize block size log and mask constants for quick calculations */
    paictl->aiblklog = bkblklog(paictl->aiblksize);
    paictl->aiblkmask = (ULONG)0xFFFFFFFF << paictl->aiblklog;

    /* 
     * allocate buffer for ai writer to use for writing. This buffer is
     * used for making a copy of the current buffer prior to writing
     * it so that the ai writer can be writing while others are storing
     * more data in the next output buffer.  Also used during extent switches.
     */

    pbktbl = &paictl->aiwbktbl;
    QSELF (pbktbl) = P_TO_QP(pcontext, pbktbl);

    pbktbl->bt_raddr = -1;
    pbktbl->bt_dbkey = -1;
    /* BUM - Assuming aiFileType < 256 */
    pbktbl->bt_ftype = (TEXT)paictl->aiFileType;
    if ( paictl->aiFileType == BKAI )
    {
       paictl->aiArea = rlaiGetNextArea(pcontext, 1);
       if( pdbcontext->pmstrblk->mb_aibusy_extent )
       {
	  paictl->aiArea = pdbcontext->pmstrblk->mb_aibusy_extent;
       }
    }
    else
    {
    	paictl->aiArea = DSMAREA_TL;
    }
    pbktbl->bt_area = paictl->aiArea;
    
    bmGetBufferPool(pcontext, pbktbl);

    /* must have at least one ai buffer */
    if (pdbpub->argaibufs < 1) pdbpub->argaibufs = 1;

    /* allocate a ring of output ai buffer control blocks */
    pn = NULL;
    for (i = pdbpub->argaibufs; i > 0; i--)
    {
        paibuf = (AIBUF *)stGet (pcontext, (STPOOL *)QP_TO_P(pdbcontext, pdbpub->qdbpool),
			 	 sizeof(AIBUF));
        QSELF (paibuf) = P_TO_QP(pcontext, paibuf);

	if (pn)
	{
	    /* set previous buffer's link to next buffer in chain */
	    pn->aiqnxtb = QSELF (paibuf);
	}
	else
	{
	    /* this is the first, current, and next to write buffer */
	    paictl->aiqbufs = QSELF (paibuf);
            paictl->aiqcurb = QSELF (paibuf);
            paictl->aiqwrtb = QSELF (paibuf);
	}
	pn = paibuf;
    }

    /* link the last one to the first one */
    paibuf->aiqnxtb = paictl->aiqbufs;

    /* now allocate the ai block buffers themselves */
    pn = XAIBUF(pdbcontext, paictl->aiqbufs);
    for (i = pdbpub->argaibufs; i > 0; i--)
    {
	pbktbl = &(pn->aibk);

        QSELF (pbktbl) = P_TO_QP(pcontext, pbktbl);

        pbktbl->bt_raddr = -1;
	pbktbl->bt_dbkey = -1;
        pbktbl->bt_area = paictl->aiArea;
        /* BUM - Assuming UCOUNT aiFileType < 256 */
        pbktbl->bt_ftype = (TEXT)paictl->aiFileType;
        bmGetBufferPool(pcontext, pbktbl);
	pn = XAIBUF(pdbcontext, pn->aiqnxtb);
    }

} /* rlaiseto */


/* PROGRAM: rlaiopn - open the	after-image (transaction-log) file
 *
 *
   Updates:
	    allocates an aictl
	    aictl.aibktbl will be setup.  If the ai is opened for output,
			  then that buffer will contain the current output
			  block of the ai file (the last block containing
			  valid data).	If the ai is opened for input,
			  then that buffer will contain the first blk of
			  the ai file.
	    aictl.aimode  will be set appropriately
	    aictl.aisize - physical EOF
	    aictl.ailoc  - input: start of file, output: logical EOF
	    aictl.aiofst - offset of ailoc within current ai block

   Some of the updates is diffrent for an off-line Transaction-log file
   which doe not use the noral AI mechanism, but is very similar to it.
									
 * NOTE: rlaiopn can be called by normal progress to open the .ai file	*
 *	 for input or output.						*
 *									*
 *	When being called for output, The caller should check		*
 *	pmstrblk->mb_ai.ainew and should not call rlaiopn if it is 0.	*
 *
 * RETURNS: 0 - on success
 *         -1 - on failure
 *          1 - only a partial ai file was detected
 */
int
rlaiopn(
	dsmContext_t	*pcontext,
	int  aimode,        /* AIINPUT or AIOUTPUT	*/
	TEXT *pname,        /* name of the .ai file	*/
	int  execmode,      /* non-zero => open for input with intent to rf */
	int  tmplog,        /* 1 for off-line TL file.   */
        int  retry)         /* 1 - retry active, 0 - retry inactive */  
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    MSTRBLK *pmstr = pdbcontext->pmstrblk;
    AICTL	*paictl;
    AIBLK	*paiblk;
    BKTBL	*pbktbl;
struct  bkftbl  *pbkftbl = (struct bkftbl *)NULL;
    COUNT 	 buffmode;
    LONG	 ret;
    TEXT         timeString[TIMESIZE];
    UCOUNT       returnCode = 0;

    if (aimode == AIOUTPUT && pmstr->mb_aisync==UNBUFFERED && !tmplog)
	buffmode = UNBUFFERED;
    else buffmode = BUFFERED;

    /* allocate all buffers and control structures */
    rlaiseto (pcontext);

    /* get local pointer to global structure */
    paictl = pdbcontext->paictl;

    /* get the ai file size in bytes */
    if( pmstr->mb_aibusy_extent > (TEXT)0 )
    {
	/* using an extent, get the length of busy extent */
        paictl->aisize = rlaielen(pcontext, paictl->aiArea);
    }
    else
    {
	/* *** TODO: bkflen returns size in terms of database blocks */
	/* using a named file get the length directly */
        paictl->aisize = bkflen(pcontext, paictl->aiArea) *
            pdbcontext->pmstrblk->mb_aiblksize;
    }

    /* read the first ai block into current buffer */
    pbktbl = &(XAIBUF(pdbcontext, paictl->aiqcurb)->aibk);
    pbktbl->bt_dbkey = 0L;
    bkRead(pcontext, pbktbl);
    paiblk = XAIBLK(pdbcontext, pbktbl->bt_qbuf);

    /* Make sure we have a compatible version                */
    if (rlaiVersion(pcontext, paiblk))
    {
	/* AI file cannot be used with this database */
	MSGN_CALLBACK(pcontext, rlMSG094);
	return -1;
    }

    /* make sure blocksize matches database */
    if (rlaiblksize(pcontext, paiblk, paictl))
    {
	/* AI file cannot be used with this database */
	MSGN_CALLBACK(pcontext, rlMSG094);
	return -1;
    }

    /* Make sure the file is complete, BUG# 20000510-002 */
    if ( rlaiCompare(pcontext, paiblk, paictl) )
    {
        /* File lengths don't compare for whatever reason.
           Prohibit the ai seq num from being incremented so if the user 
           happens to find the correct, full ai file they can retry. */

        returnCode = 1;
    }

    /* setup for reading (rollforward) or writing (logging) to file */
    if (aimode == AIINPUT)
    {
	if ( execmode )
	{
         if ( retry )
            {
                /* Retry is activate - check extent status */
                if( paiblk->aifirst.aiextstate == AIEXTEMPTY )
                {
                    /* That's because this is an empty ai extent */
                    MSGN_CALLBACK(pcontext, rlMSG058);
                    return -1;
                }
                /* Ensure ai in proper sequence. */
                pmstr->mb_aiseq = pmstr->mb_aiseq ? pmstr->mb_aiseq : 1;
                if (paiblk->aifirst.aidates.aigennbr != pmstr->mb_aiseq)
                {
                    /* This ai file is not in the proper sequence for rretry. */
     /* Expected ai file number %l but file sepecified is %l in sequence. */
                    MSGN_CALLBACK(pcontext, rlMSG140, pmstr->mb_aiseq,
                                  paiblk->aifirst.aidates.aigennbr);
                    return -1;
                }
            }
            else if  (paiblk->aifirst.lstmod != pmstr->mb_lstmod )
	    {
		/* the time stamps dont match */
		if( paiblk->aifirst.aiextstate == AIEXTEMPTY )
		{
		    /* That's because this is an empty ai extent */
		    MSGN_CALLBACK(pcontext, rlMSG058);
		}
		else
		{
                    /* Ensure ai in proper sequence. */
                    pmstr->mb_aiseq = pmstr->mb_aiseq ? pmstr->mb_aiseq : 1;
                    if (paiblk->aifirst.aidates.aigennbr != pmstr->mb_aiseq)
                    {
                        /* ai file is not in the proper sequence for rollf. */
    /* "Expected ai file number %l but file sepecified is %l in sequence." */
                        MSGN_CALLBACK(pcontext, rlMSG140, pmstr->mb_aiseq,
                                      paiblk->aifirst.aidates.aigennbr);
                    }
		    MSGN_CALLBACK(pcontext, rlMSG001,
                                uttime((time_t *)&pmstr->mb_lstmod, timeString,
                                       sizeof(timeString)));
		    MSGN_CALLBACK(pcontext, rlMSG002,
		                  uttime((time_t *)&paiblk->aifirst.lstmod,
                                         timeString, sizeof(timeString)));
		    MSGN_CALLBACK(pcontext, rlMSG003);
		}
		return -1;
	    }
	}

	/* Init for reading from beginning of file */
	paictl->aiofst = paiblk->aihdr.aihdrlen;
	paictl->ailoc = pbktbl->bt_dbkey;
	paictl->aimode = AIINPUT;
    }
    else
    {
	/* file is being opened for output (logging) check the dates */
	if (paiblk->aifirst.aidates.aiopen != pmstr->mb_ai.aiopen)
        {
	    /* time stamps dont match */
            if( (pname == NULL) && 
                (pdbcontext->pmstrblk->mb_aibusy_extent > (TEXT)0) )
            {
		/* Means its an ai extent file */
		ret = bkGetAreaFtbl(pcontext, paictl->aiArea,&pbkftbl);
		pbkftbl += pdbcontext->pmstrblk->mb_aibusy_extent - 1;
                pname = XTEXT(pdbcontext, pbkftbl->ft_qname);
	    }
	    MSGN_CALLBACK(pcontext, rlMSG045, pname);
	    return -1;
	}

	/* get the current date/time and put into ai block */
	time(&paiblk->aifirst.aidates.aiopen);

	/* put time into masterblock */
	pmstr->mb_ai.aiopen = paiblk->aifirst.aidates.aiopen;

	/* write out both blocks */
	bkWrite(pcontext, pbktbl, UNBUFFERED);
	bmFlushMasterblock(pcontext);

	/* initialize ai control structure for logging */
        paictl->aiupdctr = paiblk->aifirst.aifirstctr;
        paictl->ailoc = pbktbl->bt_dbkey;
        paictl->aiofst = paiblk->aihdr.aihdrlen;
	paictl->aimode = AIOUTPUT;
    }
    /* done. ready to go */

    if ( returnCode )
    {
        /* detected partial ai file */
        return (1);
    }
    else
    {
        /* success */
        return (returnCode);
    }
}

/**/
/* PROGRAM: rlaisete - positions .ai (.tl) file and control structures to
 *		       logical eof
 *
 * RETURNS: DSMVOID
 */
DSMVOID
rlaisete(dsmContext_t *pcontext)
{
	LONG	 wrtloc;

    wrtloc = pcontext->pdbcontext->pmstrblk->mb_aiwrtloc;

    rlaiseek (pcontext, wrtloc);
}

/* PROGRAM: rlaiseek - read in the correct .ai file block into the
 *	current buffer
 * 
 * RETURNS: DSMVOID
 */
DSMVOID
rlaiseek(
	dsmContext_t	*pcontext,
	LONG		 aipos)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
	AICTL	*paictl = pdbcontext->paictl;
	BKTBL	*pbktbl;
	AIBLK	*paiblk;

    if (aipos < 0L )
        FATAL_MSGN_CALLBACK(pcontext, rlFTL002);

    pbktbl = &(XAIBUF(pdbcontext, pdbcontext->paictl->aiqcurb)->aibk);

    paictl->ailoc  = aipos & paictl->aiblkmask;
    /* BUM - Assuming LONG(aipos & ~paictl->aiblkmask) < 2^15 */
    paictl->aiofst = (COUNT)(aipos & ~paictl->aiblkmask);
    pbktbl->bt_dbkey = (DBKEY)(aipos & paictl->aiblkmask);

    bkRead(pcontext, pbktbl);

    paiblk = XAIBLK(pdbcontext, pbktbl->bt_qbuf);
    if (paiblk->aihdr.aihdrlen == 0)
    {
        /* this is a virgin block, format it */

        if (paictl->ailoc == 0L)
	{
	    /* first block */

	    paiblk->aihdr.aihdrlen = sizeof(AIHDR) + sizeof(AIFIRST);
	    paiblk->aihdr.aiused = sizeof(AIHDR) + sizeof(AIFIRST);
	}
	else
	{
	    paiblk->aihdr.aihdrlen = sizeof(AIHDR);
	    paiblk->aihdr.aiused = sizeof(AIHDR);
	}
        /* #96-02-15-040 */
        paictl->aiofst = paiblk->aihdr.aiused;
    }
}

/**/
/* PROGRAM: rlaielen - Return length in bytes of an ai area.
 *
 * RETURNS:
 */
gem_off_t
rlaielen(
	dsmContext_t 	*pcontext,
	ULONG		 area)
{
struct bkftbl    *pbkftbl = (struct bkftbl *)NULL;
       LONG       ret;
    
    /* get the extent list for the AI file */
    ret = bkGetAreaFtbl(pcontext, area,&pbkftbl);

    return (pbkftbl->ft_curlen);
}

    
