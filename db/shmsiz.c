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
#if SHMEMORY

#include "bkpub.h"
#include "bmpub.h"
#include "dbcontext.h"
#include "dsmpub.h"
#include "latpub.h"
#include "lkpub.h"
#include "ompub.h"
#include "rlai.h"
#include "rlprv.h"
#include "seqpub.h"
#include "semprv.h"
#include "shmpub.h"
#include "stmpub.h"
#include "stmprv.h"
#include "tmtrntab.h"
#include "usrctl.h"
#include "utmsg.h"
#include "shmprv.h"
#include "statpub.h"
#include "rltlpub.h"
#include "rltlprv.h"
#include "dbxapub.h"

/*
 * PROGRAM: shmGetShmSize - calculate the total amount of shared memory needed
 *
 * RETURNS: number of bytes of shared memory needed
 */ 
VLMsize_t
shmGetShmSize(
	dsmContext_t	*pcontext,
	LONG		 blockSize )
{
    DOUBLE	sz;
    DOUBLE	maxsz;
    DOUBLE	calcsz;
    DOUBLE	nbkbufs;
    DOUBLE	naibufs;
    DOUBLE	nbibufs;
    LONG        latchstructure;
    dbshm_t     *pdbpub = pcontext->pdbcontext->pdbpub;
    ULONG       dummy = 3407872;

    /* compute needed size of shared memory */
    nbkbufs = pdbpub->argbkbuf + pdbpub->argtpbuf + XTRABUFS;
    nbibufs = pdbpub->argbibufs;
    naibufs = pdbpub->argaibufs;

    /* miscellaneous structures */
    sz = (DOUBLE)sizeof(shmTable_t) +
	(DOUBLE)sizeof(dbcontext_t) +
	(DOUBLE)sizeof(dbshm_t) +
	(latchstructure = latGetSizeofStruct()) +
	(DOUBLE)pdbpub->argnusr * sizeof (usrctl_t);

    /* average users tty name */
    sz += (DOUBLE)pdbpub->argnusr * 20 ;

    /* database buffer pool */
    sz += (DOUBLE)sizeof(BKCTL) +
	(DOUBLE)nbkbufs * sizeof (BKTBL) +
	(DOUBLE)pdbpub->arghash * sizeof (QBKTBL) +
	(DOUBLE)nbkbufs * STROUND(BKBUFSIZE(blockSize) + BK_BFALIGN);

    /* transaction table */
    sz += (DOUBLE)TRNSIZE(pdbpub->argnclts);

    /* Ready-to-commit transaction table */
    sz += (DOUBLE)TRNRCSIZE(pdbpub->argnclts) ;

    /* undo/redo log (before imaging) */
    sz += (DOUBLE)sizeof(RLCTL) +
	(DOUBLE)(sizeof (RLBF) * (DOUBLE)(nbibufs)) +
	/* TODO: this needs to be a variable based on mb_biblksize */
	(DOUBLE)(nbibufs * STROUND(16384 + BK_BFALIGN));

    sz += (DOUBLE)sizeof(rltlCtl_t) + RLTL_BUFSIZE;
    
    /* redo log (after imaging) */
    sz += (DOUBLE)sizeof(AICTL) +
	(DOUBLE)(sizeof (AIBUF) * (DOUBLE)(naibufs)) +
	/* TODO: this needs to be a variable based on mb_aiblksize */
	(DOUBLE)naibufs * STROUND(16384 + BK_BFALIGN);

    /* Progress lock table only - not gateways */
    sz += (DOUBLE)sizeof(LKCTL) +
	(DOUBLE)pdbpub->arglkhash * (LK_NUMTYPES * sizeof (QLCK)) +
	(DOUBLE)pdbpub->arglknum * sizeof (struct lck);
    
    /* Object manager cache - cache of _Object schema records */
    sz += omCacheSize(pcontext);

    /* Sequence number cache and control */
    sz += seqGetCtlSize(pcontext);

    /* size for the bkArea structure */
    sz += BK_AREA_DESC_SIZE(20) + BK_AREA_DESC_PTR_SIZE(1000);

    if (pdbpub->argxshm == -1)
    {
	/* 16k + (300/400 bytes per user) extra */
	sz += 16L * 1024L + pdbpub->argnusr
		* (sizeof(long)==4 ? 300L : 400L); /* extra for misc use */
    }
    else
    {
        /* the command line argument -Mxs <amount> tells Progress to pad
           shared memory by <amount>, instead of the default padding */
	sz += pdbpub->argxshm;
    }

    sz += semGetSemidSize(pcontext);

#if ALPHAOSF
    if (pcontext->pdbcontext->argMpte)
    	/* Round VLM64 segments up to nearest 8Mb for optimal shared PTE's */
    	/* NOTE:  See doc on /etc/sysconfigtab vm: gh-chunks */
	sz = ((VLMsize_t)sz + 0x007fffff) & 0xffffffffff800000;
#endif

        sz +=
            (pcontext->pdbcontext->pdbpub->argtablesize)*
            (sizeof(struct tablestat));
 
        sz +=
            (pcontext->pdbcontext->pdbpub->argindexsize)*
            (sizeof(struct indexstat ));

   /* bug 19990816-030, Need to handle more than a ULONG, changed all VLMSIZE_T
      to DOUBLE in order to calculate properly.  Issue message in MB. Meleedy */
   /* sz can't exceed 3.25GB.  gcc complains if we compare against 3.25GB
      (3489660928) directly, so we use dummy to fool the compiler and
      avoid the warning.
   */
   if (( sizeof(VLMsize_t) == 4 ) && ( sz > (dummy * 1024)))
   {
       maxsz = (dummy * 1024) / 1000000;
       calcsz = sz / 1000000;
       MSGN_CALLBACK(pcontext, utMSG149, (LONG)calcsz); 
       MSGN_CALLBACK(pcontext, utMSG150, (LONG)maxsz); 
       return 0;
    }


    return((VLMsize_t)sz);
}

/*
 * PROGRAM: shmGetMaxUSRCTL calculate the largest usrctl_t for MAXSEGSIZE.
 *
 * RETURNS: number of entries 
 */ 
psc_user_number_t
shmGetMaxUSRCTL(DSMVOID)
{	/* this calculation needs to be refined to subtract overhead data */
	/* while psc_user_number_t is a COUNT, we can't exceed MAXUSRS */
    return( MIN( MAXSEGSIZE / sizeof (usrctl_t) , MAXUSRS) );
}

#endif  /* if SHMEMORY  */
