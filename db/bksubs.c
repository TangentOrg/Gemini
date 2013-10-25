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

#include <stdlib.h>

#include "dbconfig.h"

#include "area.h"
#include "bkpub.h"
#include "bkprv.h"
#include "bkdoundo.h"
#include "bmpub.h"   /* for bmAlign call */
#include "cxpub.h"
#include "cxprv.h"
#include "dbcontext.h"
#include "dbpub.h"
#include "dsmpub.h"  /* for area types */
#include "extent.h"
#include "keypub.h"
#include "latpub.h"
#include "rmprv.h"
#include "lkpub.h"
#include "objblk.h"
#include "recpub.h"
#include "rlai.h"    /* BUM needed for definition of AIBLK */
#include "rlpub.h"
#include "rlprv.h"
#include "rmpub.h"
#include "scasa.h"
#include "stmpub.h"  /* storage management subsystem */
#include "stmprv.h"
#include "svcspub.h"
#include "usrctl.h"
#include "utcpub.h"
#include "utfpub.h"
#include "utmpub.h"
#include "utmsg.h"
#include "mvmsg.h"
#include "utospub.h"
#include "utspub.h"
#include "uttpub.h"
#include "rltlpub.h"
#include "bmprv.h"
#include "mstrblk.h"


#if OPSYS==WIN32API
#include <io.h>
#endif

#if FCNTL
#include  <fcntl.h>
#endif

#if OPSYS==UNIX
#include <unistd.h>  /* for close call */
#endif

#if OPSYS==UNIX || OPSYS==WIN32API
#include <errno.h>
#endif

/*
 * Number of bytes to write at once when extending files. But not too
 * high because of diminishing returns. And we don't want to use too
 * much memory either. Must be at least as large as largest blocksize
 * supported (16K for bi blocks)
 */
#define BK_XT_CHUNK_BYTES 16384

/* Number of blocks in a new area */
#define INIT_OBJECT_HIWATER 3

LOCAL latch_t extBufferLatch;  /* process private latch for pBkExtBuf */
LOCAL BKBUF *pBkExtBuf = NULL; /* pointer to buffer for extending files */

#define GEMINI_CRASH 1
#ifdef GEMINI_CRASH
#if OPSYS==UNIX
#include <signal.h>
#endif
LOCAL int gemini_crash_count = 0;
LOCAL int gemini_crash_num = -1;
#endif

#if BKTRACE
#define MSGD	MSGD_CALLBACK
#endif
#include "bkcrash.h"



/*** LOCAL FUNCTION PROTOTYPES ***/

LOCALF LONG bkxtnwrite	(dsmContext_t *pcontext,
			 bkftbl_t *pbkftbl, gem_off_t offset, TEXT *pbuf, 
			 LONG bufBlocks, int blockLog);

#if NUXI==0
LOCALF
DSMVOID bkMasterBlockToStore(bkhdr_t *pblk);

LOCALF
DSMVOID bkIndexBlockToStore(bkhdr_t *pblk);
LOCALF
DSMVOID bkRecordBlockToStore(bkhdr_t *pblk);

LOCALF
DSMVOID bkIndexAnchorBlockToStore(bkhdr_t *pblk);

LOCALF
DSMVOID bkObjectBlockToStore(bkhdr_t *pblk);

LOCALF
DSMVOID bkExtentBlockToStore(bkhdr_t *pblk);

LOCALF
DSMVOID bkMasterBlockFromStore(bkhdr_t *pblk);
LOCALF
DSMVOID bkIndexBlockFromStore(bkhdr_t *pblk);

LOCALF
DSMVOID bkRecordBlockFromStore(bkhdr_t *pblk);

LOCALF
DSMVOID bkIndexAnchorBlockFromStore(bkhdr_t *pblk);

LOCALF
DSMVOID bkObjectBlockFromStore(bkhdr_t *pblk);

LOCALF
DSMVOID bkExtentBlockFromStore(bkhdr_t *pblk);
#endif /* #if NUXI==0  */

/*** END LOCAL FUNCTION PROTOTYPES ***/

/*****************************************************************/
/**								**/
/**	bksubs.c - routines for extent management. These	**/
/**	routines provide support for multi-file PROGRESS	**/
/**	databases and bi files. This includes the conversion	**/
/**	of database-relative or bi file-relative block		**/
/**	addresses to a specific file and byte offset, as	**/
/**	well as performing all disk I/O, EXCEPT that required	**/
/**	to achieve reliable I/O against standard UNIX files.	**/
/**	This also includes the extension of databases or bi	**/
/**	files, where possible.					**/
/**								**/
/**	The following external entries are provided:		**/
/**								**/
/**	bkRead - read the indicated physical block into a	**/
/**		disk buffer.					**/
/**	bkCreate - setup a buffer as if it contained the		**/
/**		indicated physical block, yet do not actually	**/
/**		read the block.					**/
/**	bkWrite - write the block contained in a particular	**/
/**		buffer back out to its disk location.		**/
/**	bkxtn - extend the indicated type file, if possible.	**/
/**		by a number of blocks.                          **/
/**     bkxtnfile - extend a specific file, if posible by a     **/
/**             specific number of blocks.                      **/
/**	bkflen - return the length, in BLOCKS, of the		**/
/**		db or bi file(s), not counting any secondary	**/
/**		master blocks used in extent management.	**/
/**	bktrunc - truncate varable length files/extents and	**/
/**		initialize fixed length extents.		**/
/**	bkaddr - convert from a data or bi relative byte	**/
/**		offset to a file extent and offset within	**/
/**		that extent.					**/
/**								**/
/*****************************************************************/

/**************************************************************************/
/* PROGRAM: bkRead - read the indicated block from the database or bi file
 *		into the specified buffer.
 *
 *		bkaddr is used to determine the appropriate file table
 *		entry and to initialize the block address fields in
 *		the buffer control block.
 *
 *		If this system supports the synchronous write (swrite)
 *		call, then all reads are performed using the file
 *		descriptor contained in the file table entry.
 *
 *		Otherwise, if the requested block is in a standard
 *		UNIX file, and reliable I/O has not been disabled
 *		with the -r or -i options, bkrread is used to read
 *		the requested block into the buffer.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkRead (
	dsmContext_t	*pcontext,
	bktbl_t		*pbktbl)	/* identifies buffer */
{
        dbcontext_t	*pdbcontext = pcontext->pdbcontext;
	bkftbl_t    	*pbkftbl = (bkftbl_t *)NULL;
	int	 	addrmode = 0;	/* RAWADDR or NORAWADDR */
        int      	filetype;      /* BKDATA, BKBI, BKAI or BKTL */
        bkbuf_t		*pbkbuf;
	LONG	 	biblk;
	int	 	retryCnt = 0;
	LONG	 	blknum = 0;		/* block number within file */
	int	 	ret = 0;
        ULONG           retvalue;      /* return value from latSetCount() */
        int             counttype,     /* 0=waitcnt, 1=lockcnt */
                        codetype;      /* type of wait */

    if (pbktbl->bt_flags.changed)
        FATAL_MSGN_CALLBACK(pcontext, bkFTL016); /* missing bmFlushx call */

    filetype = pbktbl->bt_ftype;

    /* get the file table block for the first file of requested type */
    /* this is needed to get the offset for bkaddr */
    ret = bkGetAreaFtbl(pcontext, pbktbl->bt_area, &pbkftbl);

    if (ret != 0)
    {
        FATAL_MSGN_CALLBACK(pcontext,bkFTL041,pbktbl->bt_area,ret);
    }

    /* buffer address */
    pbkbuf = XBKBUF(pdbcontext, pbktbl->bt_qbuf);

    switch (filetype)
    {
        case BKDATA:

	   blknum = (pbktbl->bt_dbkey >> BKGET_AREARECBITS(pbktbl->bt_area))
                                                                           - 1;

            /* all blocks of the .db except the master block get NON-RAW i/o */

           if (pbktbl->bt_dbkey > BLK1DBKEY(BKGET_AREARECBITS(pbktbl->bt_area)))
                 addrmode = NORAWADDR;
            else addrmode = RAWADDR;
	    break;

        case BKBI:
            /* "dbkey" is the block number */
            blknum = pbktbl->bt_dbkey;
            break;

        case BKAI:
	    /* "dbkey" is the file offset in bytes */
	    blknum = pbktbl->bt_dbkey  >> pbkftbl->ft_blklog;
            addrmode = RAWADDR;
	    break;
            
        case BKTL:
	    blknum = pbktbl->bt_dbkey;
            addrmode = RAWADDR;
	    break;

	default:
	    FATAL_MSGN_CALLBACK(pcontext, bkFTL065, filetype);
    }

    /* get the block's file extent, relative address and raw address */
    bkaddr (pcontext, pbktbl, addrmode);

    /* which extent table entry block is in */
    pbkftbl = XBKFTBL(pdbcontext, pbktbl->bt_qftbl);

#if BKTRACE
MSGD(pcontext, "%LbkRead  ftype %i, dbkey %D buffer %X",
     filetype, pbktbl->bt_dbkey, pbkbuf);
#endif

retry:

   ret = bkioRead (pcontext,
                   pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                   &pbkftbl->ft_bkiostats,
		   pbktbl->bt_offst >> pbkftbl->ft_blklog, 
		   (TEXT *)pbkbuf, 
		   (int)(pbkftbl->ft_blksize >> pbkftbl->ft_blklog),
		   (int)pbkftbl->ft_blklog,
		   BUFFERED);
    if (ret < 0)
    {
	/* Error reading file %s, ret = %d */
	FATAL_MSGN_CALLBACK(pcontext, bkFTL008,
                            XTEXT(pdbcontext, pbkftbl->ft_qname),ret);
    }

    switch (filetype)
    {
	case BKDATA:
	    /* Convert block to machine dependent format  */
	    BK_FROM_STORE(&pbkbuf->bk_hdr);
            /* check if we got the right block */

            if (pbktbl->bt_dbkey != pbkbuf->BKDBKEY)
	    {
		/* This error is known to have been caused by at least
		   all of the following:

			progress bugs
			broken disks or controllers
			broken memory 
			corrupted filesystems
			filesystem bugs (e.g. not flushing inodes)
			disk driver bugs
			operating system bugs
		*/

		/* note failure in log before retrying */
		if (retryCnt == 0)
		{
		    /* read wrong dbkey at offset <offset> in file <file> 
		       found <dbkey>%D, expected <dbkey>, retrying. */
		    MSGN_CALLBACK(pcontext, bkMSG073,
                         (LONG)pbktbl->bt_offst,
                         XTEXT(pdbcontext, pbkftbl->ft_qname),
			 pbkbuf->BKDBKEY, pbktbl->bt_dbkey);
		}

		retryCnt++;
		utsleep (1);

		if (retryCnt < 10) goto retry;

		/* ten tries is enough. we'll never get it */

		pdbcontext->pdbpub->shutdn = DB_EXBAD;
                /* Corrupt block while reading - #95-02-24-024 */
                MSGN_CALLBACK(pcontext, bkMSG107);
                pdbcontext->pdbpub->shutdn = DB_EXBAD;
	        FATAL_MSGN_CALLBACK(pcontext, bkFTL056,
                              pbkbuf->BKDBKEY, pbktbl->bt_dbkey);
	    }
#if SHMEMORY
            if (pbkbuf->BKTYPE == IDXBLK) STATINC (ixrdCt);
            pcontext->pusrctl->lockcnt[READWAIT]++;
            counttype = 1;
            codetype = READWAIT;
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
#endif
	    break;

	case BKBI:
	    /* check if we got the right block. see note above */

	    biblk = rlGetBlockNum(pcontext, pbkbuf);
	    if (biblk)
	    {
	        if (biblk != blknum)
                    FATAL_MSGN_CALLBACK(pcontext, bkFTL052, biblk, blknum);
	    }
#if SHMEMORY
            pcontext->pusrctl->lockcnt[BIREADWAIT]++;
            counttype = 1;
            codetype = BIREADWAIT;
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
#endif
	    break;

	case BKAI:
        case BKTL:
	    /* ai files dont currently have the disk address in the block
	       header */
#if SHMEMORY
            pcontext->pusrctl->lockcnt[AIREADWAIT]++;
            counttype = 1;
            codetype = AIREADWAIT;
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
#endif
	    break;
    }
    pbkftbl->ft_nreads++;

}  /* end bkRead */


/**********************************************************************/
/* PROGRAM: bkCreate - initialize a buffer control block to contain
 *		the various addresses for an indicated block from the
 *		database or bi file.
 *
 *		Bkaddr is used to determine the appropriate file table
 *		entry and to initialize the block address fields in
 *		the buffer control block.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkCreate(
	dsmContext_t	*pcontext,
	bktbl_t		*pbktbl)	/* identifies buffer */
{
	int	 addrmode = 0;	/* RAWADDR or NORAWADDR */

    switch (pbktbl->bt_ftype)
    {
        case BKDATA:
            /* all blocks of the .db except the master block get NON-RAW i/o */

           if (pbktbl->bt_dbkey > BLK1DBKEY(BKGET_AREARECBITS(pbktbl->bt_area)))
                 addrmode = NORAWADDR;
            else addrmode = RAWADDR;
	    break;

        case BKBI:
        case BKAI:
        case BKTL:
            addrmode = RAWADDR;
	    break;

	default:
	    FATAL_MSGN_CALLBACK(pcontext, bkFTL065, pbktbl->bt_ftype);
    }
#if BKTRACE
MSGD(pcontext, "%LbkCreate  ftype %i, dbkey %D",
     (int)pbktbl->bt_ftype, pbktbl->bt_dbkey);
#endif

    bkaddr (pcontext, pbktbl, addrmode);

}  /* end bkCreate */


/****************************************************************************/
/* PROGRAM: bkWrite - write out the block from an indicated buffer to file
 *
 * This procedure is the write call for database I/O to database, BI, AI or TL
 * files.  Before bkWrite can be called for a particular buffer, bkRead or
 * bkCreate must be called to prepare the various address fields in the buffer
 * control block.
 *
 * bkWrite() understands adjustable block sizes and will use the correct size
 * when doing I/O based on file type.
 *
 * If the block in this buffer belongs in a non-UNIX partition, or if
 * reliable I/O has been disabled with the -r or -i option, the file
 * descriptor from the file table entry is used with a standard write call.
 *
 * Otherwise, the block in this buffer is from a standard UNIX file and
 * reliable I/O is desired.  If this system supports the synchronous write
 * (swrite) call, swrite is used to write out the block using the file
 * descriptor from the file table entry.  Otherwise, bkrwrite is used to
 * reliably write out the block.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkWrite(
	dsmContext_t	*pcontext,
	bktbl_t		*pbktbl,	/* identifies buffer	  */
	int		 buffmode)	/* BUFFERED or UNBUFFERED */
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    dbshm_t	*pdbpub = pdbcontext->pdbpub;
    bkftbl_t	*pbkftbl = (bkftbl_t *)NULL;
    bkbuf_t	*pbkbuf;
    int      	filetype;      /* BKDATA, BKBI, BKAI or BKTL */
    AIBLK	*paiblk;
    LONG	blknum;
    LONG	biblk;
    LONG	buflen;
    int		smashed;
    LONG        rc;           /* OS write return code */
    ULONG       retvalue;     /* return value from latSetCount() */
    int         counttype,    /* 0=waitcnt, 1=lockcnt */
                codetype;     /* type of wait */

    /* Check for active crashtesting */
    if (pdbcontext->p1crashTestAnchor)
        bkCrashTry (pcontext ,(TEXT *)"bkWrite");

    /* get the file type */
    filetype = pbktbl->bt_ftype;

    /* which extent table entry block is in */
    pbkftbl = XBKFTBL(pdbcontext, pbktbl->bt_qftbl);

    /* has this extent been removed?  if so, no need to write */
    if (pbkftbl->ft_delete)
        return;

    /* determine the length of this type of buffer based on blocksize */
    buflen = pbkftbl->ft_blksize;

    /* buffer address */
    pbkbuf = XBKBUF(pdbcontext, pbktbl->bt_qbuf);

#if BKTRACE
MSGD(pcontext, "%LbkWrite ftype %i, dbkey %D offset %i fd %i buffer %X",
     filetype, pbktbl->bt_dbkey, pbktbl->bt_offst, fd, pbkbuf);
#endif

    switch (filetype)
    {
	case BKDATA: /* database */

#if NUXI==0
            /* if we are going to convert to platform independent 
                we can't do it in place because other threads may 
                be reading the buffer, so make a copy and write out 
                the copy */
            pbkbuf = (struct bkbuf *)stRent(pcontext, pdbcontext->privpool, 
                                                 buflen);
            bufcop (pbkbuf, XBKBUF(pdbcontext, pbktbl->bt_qbuf), buflen);
#endif

	    blknum = (pbktbl->bt_dbkey >> BKGET_AREARECBITS(pbktbl->bt_area))
                                                                           - 1;

	    /* check for valid address, etc */
	    if ((pbktbl->bt_dbkey) &&
	        (pbkbuf->BKDBKEY != pbktbl->bt_dbkey))
	    {
                /* Corrupt block while writing - #95-02-24-024 */
                MSGN_CALLBACK(pcontext, bkMSG108);
		/* we can not write a block with a bad dbkey in it */
                
	        FATAL_MSGN_CALLBACK(pcontext,  bkFTL022,
                              pbktbl->bt_dbkey, pbkbuf->BKDBKEY );
	    }

            /* update stats */
#if SHMEMORY
	    pcontext->pusrctl->lockcnt[WRITEWAIT]++;
            counttype = 1;
            codetype = WRITEWAIT;
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
            if (pbkbuf->BKTYPE == IDXBLK) STATINC (ixwrCt);
#endif
	    BK_TO_STORE(&pbkbuf->bk_hdr);
	    break;

	case BKBI: /* before image variable length buffer */
#ifdef GEMINI_CRASH
            if (gemini_crash_num < 0)
            {
                /* haven't checked if we need to crash yet */
                TEXT       *pcrashval;
                pcrashval = (TEXT *)getenv("GEMINI_CRASH_NUM");
                if (pcrashval == NULL)
                {
                    /* turn off crash test for this sessions */
                    gemini_crash_num = 0;
                }
                else
                {
                    gemini_crash_num = atoi((char *)pcrashval);
                    MSGD_CALLBACK(pcontext, "setting gemini_crash_num to %d (%s)\n",
                       gemini_crash_num, pcrashval);
                }
            }
            else if ((gemini_crash_num > 0) && 
                     (gemini_crash_count > gemini_crash_num))
            {
                /* exceeded number of writes, tine to crash */
                MSGD_CALLBACK(pcontext, "Exceeded RL writes, aborting\n");
                MSGD_CALLBACK(pcontext, "killing process %d\n", pdbcontext->p1usrctl->uc_pid);
#if OPSYS==WIN32API
                TerminateProcess(GetCurrentProcess(), 0);
#else /* #if OPSYS==WIN32API */
                kill(pdbcontext->p1usrctl->uc_pid,9);
#endif /* #if OPSYS==WIN32API */
            }
            else if (gemini_crash_num > 0)
            {
                MSGD_CALLBACK(pcontext, "RL write %d of %d\n",
                                    gemini_crash_count, gemini_crash_num);
                gemini_crash_count++;
            }
#endif

	    /* check for valid address, etc */
	    if (pbktbl->bt_dbkey < 0)
                FATAL_MSGN_CALLBACK(pcontext, bkFTL029, pbktbl->bt_dbkey);

	    /* get block number from buffer */
            blknum = pbktbl->bt_dbkey;

	    /* validate the block number against the number in the buffer */
	    biblk = rlGetBlockNum(pcontext, pbkbuf);
	    if (biblk)
	    {
	        if (biblk != blknum)
		{
                    /* Corrupt block while writing - #95-02-24-024 */
                    MSGN_CALLBACK(pcontext, bkMSG108);
		    /* wrong bi block write <dbkey> into <dbkey> */
		    FATAL_MSGN_CALLBACK(pcontext, bkFTL053, biblk, blknum);
		}
	    }
	    else
	    {
	        /* store the block number in the header */
		rlPutBlockNum(pcontext, pbkbuf,blknum);
	    }

            /* update stats */
#if SHMEMORY
            pcontext->pusrctl->lockcnt[BIWRITEWAIT]++;
            counttype = 1;
            codetype = BIWRITEWAIT;
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
#endif
	    break;

	case BKAI: /* after image variable length buffer */
	    
	    /* get the block number from the buffer */
	    blknum = pbktbl->bt_dbkey >> pbkftbl->ft_blklog;

	    /* set up the validate the header info */
	    smashed = 0;
	    paiblk = (AIBLK *)pbkbuf;

	    /* check to see if this is the first block */
	    if (blknum == 0L)
	    {
		/* validate first block header size */
	        if (paiblk->aihdr.aihdrlen != (sizeof(AIHDR)+sizeof(AIFIRST)))
		{
		    /* header is invalid */
		    smashed += 1;
		}
	    }
	    else
	    {
		/* validate regular block header size */
		if ((paiblk->aihdr.aihdrlen != 0) &&
	            (paiblk->aihdr.aihdrlen != sizeof (AIHDR)))
		{
		    /* header is invalid */
		    smashed += 2;
		}
	    }

	    /* validate used value against header length and buffer length */
	    if ((paiblk->aihdr.aiused > buflen) ||
                (paiblk->aihdr.aiused < paiblk->aihdr.aihdrlen))
	    {
		/* header is invalid */
		smashed += 4;
	    }

	    /* if block is invalid then shutdown */
	    if (smashed)
	    {
		pdbcontext->pdbpub->shutdn = DB_EXBAD;
		FATAL_MSGN_CALLBACK(pcontext, bkFTL066,
                              blknum, smashed, &paiblk->aihdr);
	    }

	    /* store the block number in the header */
	    paiblk->aihdr.aiblknum = blknum;

            /* update stats */
            pcontext->pusrctl->lockcnt[AIWRITEWAIT]++;
            counttype = 1;
            codetype = AIWRITEWAIT;
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
	    break;
            
        case BKTL:
	    
	    /* validate the block                   */
	    rltlCheckBlock(pcontext,pbktbl);

            pcontext->pusrctl->lockcnt[AIWRITEWAIT]++;
            counttype = 1;
            codetype = AIWRITEWAIT;
            retvalue = latSetCount(pcontext, counttype, codetype, LATINCR);
	    break;

	default:
	    FATAL_MSGN_CALLBACK(pcontext, bkFTL065, filetype);
    }

    /* just to be safe, we don't write anymore if this happens */
    if (pdbcontext->pdbpub->shutdn == DB_EXBAD)
        dbExit(pcontext, 0, DB_EXBAD);
    rc = buflen;

    if (buffmode == UNBUFFERED && pdbpub->useraw)
    {

	rc = bkioWrite (pcontext,
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                        &pbkftbl->ft_bkiostats,
			(pbktbl->bt_offst >> pbkftbl->ft_blklog),
			(TEXT *)pbkbuf, 
			(int)(buflen >> pbkftbl->ft_blklog), 
			(int)pbkftbl->ft_blklog,
			UNBUFFERED);
	if (rc < 0)
	{
	    /* Error writing file %s, ret = %d */
	    FATAL_MSGN_CALLBACK(pcontext, bkFTL019,
                          XTEXT(pdbcontext, pbkftbl->ft_qname), rc);
	}
	rc = rc << pbkftbl->ft_blklog;
#if PS2AIX
        /* cause syncd flush of extents to disk */

        fcommit(bkioGetFd(pcontext,
                          pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex]));
#endif  /* PS2AIX */
    }
    else
    {
	/* regular write on regular file descriptor */
	rc = bkioWrite (pcontext,
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                        &pbkftbl->ft_bkiostats,
			pbktbl->bt_offst >> pbkftbl->ft_blklog,
			(TEXT *)pbkbuf, 
			(int)(buflen >> pbkftbl->ft_blklog), 
			(int)pbkftbl->ft_blklog,
			BUFFERED);
	if (rc < 0)
	{
	    /* Error writing file %s, ret = %d */
	    FATAL_MSGN_CALLBACK(pcontext, bkFTL019,
                          XTEXT(pdbcontext, pbkftbl->ft_qname), rc);
	}
	rc = rc << pbkftbl->ft_blklog;
    }
    if( rc != buflen )
    {
	pdbcontext->pdbpub->shutdn = DB_EXBAD;
	FATAL_MSGN_CALLBACK(pcontext, bkFTL067,errno);
    }


    /* validate that the block didn't change after we wrote it out */
    switch (filetype)
    {
	case BKDATA: /* database */
#if NUXI==0
            /* free the buffer for platform independent, and reset the 
               pointer to the actial block */
            stVacate(pcontext, pdbcontext->privpool, (TEXT *)pbkbuf);
            pbkbuf = XBKBUF(pdbcontext, pbktbl->bt_qbuf);
#endif
	    if ((pbktbl->bt_dbkey) &&
		(pbkbuf->BKDBKEY != pbktbl->bt_dbkey))
	    {
		/* Corrupt block while writing - #95-02-24-024 */
		MSGN_CALLBACK(pcontext, bkMSG108);
		/* we can not write a block with a bad dbkey in it */
		FATAL_MSGN_CALLBACK(pcontext,  bkFTL022,
                              pbktbl->bt_dbkey, pbkbuf->BKDBKEY );
	    }
            break;
	case BKBI: /* before image variable length buffer */
            blknum = pbktbl->bt_dbkey;

	    biblk = rlGetBlockNum(pcontext, pbkbuf);
	    if (biblk)
	    {
		if (biblk != blknum)
		{
		    /* Corrupt block while writing - #95-02-24-024 */
		    MSGN_CALLBACK(pcontext, bkMSG108);
		    /* wrong bi block write <dbkey> into <dbkey> */
		    FATAL_MSGN_CALLBACK(pcontext, bkFTL053, biblk, blknum);
                }
            }
            break;
    }

    pbkftbl->ft_nwrites++;

}  /* end bkWrite */


/*****************************************************************************/
/* PROGRAM: bkaddr - determine the extent which contains the given rel blk addr
 *
 *		Given a database-relative or bi file-
 *		relative block address and the pointer to a buffer
 *		control block, determine the particular extent which
 *		contains the block, the byte offset of the block within
 *		the extent, and, if appropriate, its raw address.
 *
 *	Note:  this code assumes that all files of the same type have the
 *	       same blocksize.
 *
 * RETURNS:
 *		a non-zero value if the indicated block is in a standard
 *		UNIX file and reliable I/O is active.
 *
 *		Otherwise, a value of zero.
 */
DSMVOID
bkaddr(
        dsmContext_t *pcontext,
        bktbl_t      *pbktbl,	/* identifies buffer */
        int           addrmode _UNUSED_) /* RAWADDR or NORAWADDR */

{
	bkftbl_t	*pbkftbl;	/* file extent table to use */
	ULONG     blockOffset;		/* block offset in db */ 
	gem_off_t byteOffset;		/* byte offset in db */ 
	ULONG	numblocks;	/* # of blocks in extent */
	LONG	blknum;		/* block # within file */
	LONG    ret;
	
#if BKTRACE
MSGD(pcontext, "%Lbkaddr ftype %i, dbkey %D", filetype, pbktbl->bt_dbkey);
#endif

    /* get the file table block for the first file of requested type */
    pbkftbl = (bkftbl_t *)NULL;
    ret = bkGetAreaFtbl(pcontext, pbktbl->bt_area, &pbkftbl);

    if (ret != 0)
    {
        FATAL_MSGN_CALLBACK(pcontext,bkFTL041,pbktbl->bt_area, ret);
    }

    /* determine the block number of the provided dbkey */
    switch (pbktbl->bt_ftype)
    {
	case BKDATA:
	    /* "dbkey" is record within block which is the file offset */
	    blknum = (pbktbl->bt_dbkey >> BKGET_AREARECBITS(pbktbl->bt_area))
                                                                            - 1;
	    break;

	case BKBI:
            /* "dbkey" is the block number */
            blknum = pbktbl->bt_dbkey;
            break;

	case BKTL:
	    /* "dbkey" is the block number */
	    blknum = pbktbl->bt_dbkey;
	    break;
	    
	case BKAI:
	    /* "dbkey" is the file offset in bytes */
	    blknum = pbktbl->bt_dbkey >> pbkftbl->ft_blklog;
	    break;
	default:
	    FATAL_MSGN_CALLBACK(pcontext, bkFTL065, pbktbl->bt_ftype);
            return;
    }
    if (blknum < 0)
        FATAL_MSGN_CALLBACK(pcontext, bkFTL028, blknum);

    /* determine correct file table entry and offset within its file */

    numblocks = (ULONG)(pbkftbl->ft_curlen >> pbkftbl->ft_blklog);
    
    for (blockOffset = blknum; blockOffset >= numblocks;
     blockOffset -= numblocks, pbkftbl++,
	numblocks = (ULONG)(pbkftbl->ft_curlen >> pbkftbl->ft_blklog))
    {
       /* if we have fallen off end of extent table, blknum is too
	  big. FATAL out */
       
       if (pbkftbl->ft_type == 0)
       {
	  pbkftbl--;
	  /* bkMSG074 */
	  /* Invalid block %l for file %s, max is %l   */
	  MSGN_CALLBACK(pcontext, bkMSG074,
	       blknum, XTEXT(pdbcontext, pbkftbl->ft_qname),
	       (LONG)(pbkftbl->ft_curlen >> pbkftbl->ft_blklog));
	  FATAL_MSGN_CALLBACK(pcontext, bkFTL017, blknum );
       }
    }
	
    byteOffset = (gem_off_t)blockOffset << pbkftbl->ft_blklog;

    /* put offset and extent table pointer into buffer control block */

    pbktbl->bt_qftbl = QSELF(pbkftbl);
    pbktbl->bt_offst = (byteOffset + pbkftbl->ft_offset);

    pbktbl->bt_raddr = -1;

    return;

}  /* end bkaddr */


/***************************************************************************/
/* PROGRAM: bkxtn - extend a file by type a requested number of blocks
 *
 * Any single-file PROGRESS database, and its bi file, may be extended. 
 * A multi-file database or its bi file can only be extended if the last
 * extent file is not fixed in length.  The current ai file or ai extent
 * may be extended.
 *
 * This routine chooses the last file extent table entry for the type of file
 * being requested.  It chooses the current active ai extent.
 *
 * If zero blocks are requested, this routine picks an automatic amount and
 * maintains increments that amount by 16 until it reaches 128.
 *
 * RETURNS: returns the number of blocks actually addded
 *          otherwise 0 if the indicated file cannot be extended
 */ 
LONG
bkxtn (
	dsmContext_t	*pcontext,
	ULONG		 area,           /* Database area to be extended      */
	LONG		 blocksWanted)	/* requested extend amount in db blocks
				   to extend or zero for default extend */
{
    bkftbl_t *pbkftbl = (bkftbl_t *)NULL;
    int	    ret;
    
    /* get the table for the requested type of file */
    ret = bkGetAreaFtbl(pcontext, area, &pbkftbl);

    if (ret != 0)
    {
        FATAL_MSGN_CALLBACK(pcontext,bkFTL041, area, ret);
    }


    /* get last entry in extent table */ 
    for ( ; (pbkftbl+1)->ft_type != 0; pbkftbl++ ) ;

    /* determine if selected file can be extended */
    if (pbkftbl->ft_type & BKFIXED)
    {
	/* fixed extents cannot expand */
	return 0;
    }

    if (blocksWanted == 0L)
    {
	/* if the extend amount is not specified, then we do 16 blocks more
	   than last time. When we get to 128, we stop increasing the size.
	   perhaps we should consider letting it get larger. */

	blocksWanted = (LONG)pbkftbl->ft_xtndamt;
	if (blocksWanted < 128) pbkftbl->ft_xtndamt += 16;
    }

    /* extend the file */
     return bkxtnfile(pcontext, pbkftbl, (LONG) blocksWanted);

}  /* end bkxtn */


/*****************************************************************************/
/* PROGRAM: bkxtnfile - extend the physical file by requested number of blocks
 *
 * This routine uses the standard file descriptor in the given extent table 
 * entry for the requested file to write the blocks being added to the file.
 * If the synchronous write or direct write is supported, it is used to write
 * the blocks. Otherwise, the standard write call is used.
 *
 * If the file contains partial blocks, then the file is extended to complete
 * any partial blocks even if no full blocks are requested.  This function is
 * used to readjust files when blocksizes change.
 * 
 * If the blocks are being added to the database, they are initialized as
 * empty blocks.  If the blocks are being added to any other file, they
 * are initialized with all zeros.  If only 1 block is requested and the file
 * is currently of zero length, then the file is extended to 1 block, in all
 * other cases the total size of the file is rounded to a multiple of 16
 * blocks so blocks are generally added in increments of 16.
 *
 * RETURNS: returns the number of blocks actually added
 *          otherwise 0 - check ft_curlen to test for change in size.
 *          if only a partial block was added, 0 will be returned.
 */
LONG
bkxtnfile(
	dsmContext_t	*pcontext,
	bkftbl_t	*pbkftbl,		/* file to extend */
	LONG		 blocksWanted)	/* extend amount in database blocks */
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    dbshm_t	*pdbpub = pdbcontext->pdbpub;
    TEXT	*pname;
    bkbuf_t	*pblk;
    LONG	i;
    LONG	blocksToGo;
    gem_off_t	oldBytePos;
    LONG	newExtentSize; /* new extent size in blocks */
    LONG	oldExtentSize = 0; /* old extent size in blocks */
    bkbuf_t	*pbuf;
    LONG	bufBlocks;
    LONG	partialWanted;  /* bytes needed to complete partial blocks */
    LONG        partialWritten; /* bytes written to complete partial blocks */
    int		blocksWritten = 0;

    /* number of blocks in the extend buffer */
    bufBlocks = BK_XT_CHUNK_BYTES >> pbkftbl->ft_blklog;

    /* make sure buffer is at least minimum size */
    if (bufBlocks < 1 )
    {
	/* SYSTEM_ERROR: extend buffer %l smaller than blocksize %l */
        FATAL_MSGN_CALLBACK(pcontext, bkFTL073,
                      BK_XT_CHUNK_BYTES, pbkftbl->ft_blksize);
    }

    PL_LOCK_EXTBUFFER(&extBufferLatch)
    if (pBkExtBuf == (bkbuf_t *)NULL)
    {
	/* allocate a 16K buffer for extending */
	pBkExtBuf = (bkbuf_t *)stRentd(pcontext, pdsmStoragePool, 
                                       BK_XT_CHUNK_BYTES + BK_BFALIGN);
    }

    pbuf = pBkExtBuf;

    /* align the buffer according to the kind of I/O we are doing.
       The constant BK_BFALIGN controls this. */

    bmAlign(pcontext, (TEXT **)&pbuf);

    /* initialize the buffer */
    stnclr ((TEXT *)pbuf, BK_XT_CHUNK_BYTES);

    /* get the name of the extent */
    pname = XTEXT (pdbcontext, pbkftbl->ft_qname);

    switch (pbkftbl->ft_type & BKETYPE)
    {
      case BKDATA:
	/* new database blocks are of type "empty" */
	pblk = pbuf;
	i = bufBlocks;
	while (i > 0)
	{
	    /* format header block type and chain type */
	    pblk->BKTYPE  = EMPTYBLK;
	    pblk->BKFRCHN = NOCHN;

	    /* move to the next block */
	    pblk = (bkbuf_t *)((unsigned_long)pblk + pbkftbl->ft_blksize);
	    i--;
	}
	break;

      case BKBI:
      case BKTL:
      case BKAI:
	/* nothing special to do here */
	break;
      default:
	FATAL_MSGN_CALLBACK(pcontext, bkFTL065, pbkftbl->ft_type);
    }

    /* calculate the byte offset of existing data in the file */
    pbkftbl->ft_xtndoff = pbkftbl->ft_curlen + pbkftbl->ft_offset;

#if 0
    if ( pbkftbl->ft_xtndoff + ((gem_off_t)blocksWanted << pbkftbl->ft_blklog)
         <= 0 )
    {
        /* display error message that file has attempted to exceed the
           2GB limit */
        MSGN_CALLBACK(pcontext, bkMSG147, pname);

        /* This extend would exceed the 2gig file limit  */
        return 0;
    }
#endif
    
    /* determine if we have partial blocks in the file to be completed */
    partialWanted = 0;
    if (pbkftbl->ft_xtndoff % pbkftbl->ft_blksize)
    {

	/* there is a partial block in the file - get size to adjust with */
	if (pbkftbl->ft_xtndoff < (gem_off_t)pbkftbl->ft_blksize)
	{
	    /* file is smaller than a block */
	    partialWanted = pbkftbl->ft_blksize - pbkftbl->ft_xtndoff;
	}
	else
	{
	    /* determine existing size of extent rounded into blocks */
	    oldExtentSize = pbkftbl->ft_xtndoff >> pbkftbl->ft_blklog;
	    
	    /* file is already larger than a block round it up by one block */
	    partialWanted = (LONG)(((gem_off_t)(oldExtentSize + 1) << 
                            pbkftbl->ft_blklog) - pbkftbl->ft_xtndoff);
	}
    }

    /* see if we need to calculate for block extend */
    if (blocksWanted)
    {
	/* start with the old size of the file in bytes account for partial */
	oldBytePos = pbkftbl->ft_xtndoff + partialWanted;
	
	/* determine existing size of extent rounded into blocks */
	oldExtentSize = (LONG)(oldBytePos >> pbkftbl->ft_blklog);

	if (oldExtentSize == 0 && blocksWanted == 1)
	{
	    /* new size in blocks - when empty we allow just one block */
	    newExtentSize = blocksWanted;
	}
	else
	{
	    /* new size in blocks  - round it up to a multiple of 16 blocks */
	    newExtentSize = (oldExtentSize + blocksWanted + 15L) & (~15L);
	    blocksWanted = newExtentSize - oldExtentSize;
	}
    }

    PL_UNLK_EXTBUFFER(&extBufferLatch)

    /* before we go and open files, etc, make sure there is work to do */
    if (blocksWanted < 1 && partialWanted < 1)
    {
	/* nothing to do */
	return (0);
    }

    /* position file so to allow writes at current end */

    if (partialWanted)
    {
	TRACE1_CB(pcontext, "Found partial block, adding %i bytes to file",
                  partialWanted);

	/* - this is trick used to write bytes */
	partialWritten = bkxtnwrite(pcontext, pbkftbl, pbkftbl->ft_xtndoff,
			    (TEXT *)pbuf, partialWanted, 0 /* bytes! */);
	
	if (partialWanted != partialWritten)
	{
	    /* there was a problem completing the block - short circuit */
	    blocksWanted = 0;
	    
	    /* force error to be detected after cleanup below */
	    bufBlocks = partialWanted;
	    blocksWritten = partialWritten;
	}
	else
	{
	    /* file should now be block aligned - fix up file sizes */
	    pbkftbl->ft_curlen += partialWanted;
	    pbkftbl->ft_xtndoff = pbkftbl->ft_curlen + pbkftbl->ft_offset;
	    /* So final check will pass if all that was wanted
	       was a partial write to extend the extent header.   */
	    blocksWritten = bufBlocks;
	}
    }

    /*
     * at this point the file is block aligned, extend based on blocks now.
     */

    /* now write complete blocks if there are any to write */
    blocksToGo = blocksWanted;
    if (blocksToGo)
    {

	/* loop doing large writes using buffer */
	while (blocksToGo > 0)
	{

	    if (blocksToGo < bufBlocks)
	    {
		/* shrink the buffer to get a smaller piece */		
		bufBlocks = blocksToGo;
	    }

	    /* write out blocks in current buffer */
	    blocksWritten = bkxtnwrite(pcontext, pbkftbl, oldExtentSize,
			(TEXT *)pbuf, bufBlocks, (int)pbkftbl->ft_blklog);

	    /* verify that all were written */
	    if (blocksWritten != bufBlocks) break;

	    /* adjust blocks remaining, and file sizes */
	    blocksToGo = blocksToGo - bufBlocks;
	    oldExtentSize += bufBlocks;
	    pbkftbl->ft_curlen += (gem_off_t)bufBlocks << pbkftbl->ft_blklog;

	} /* loop to write */

	/* writes are complete */

    } /* blocksWanted */

    /* check the results of last write */
    if (blocksWritten != bufBlocks)
    {
	if ((pbkftbl->ft_type & BKETYPE) == BKAI) return 0;

	pdbpub->shutdn = DB_EXBAD;
	MSGN_CALLBACK(pcontext, bkMSG039, pname, errno);
        return 0;
    }
    pbkftbl->ft_nxtnds++;
    return (blocksWanted - blocksToGo);

}  /* end bkxtnfile */


/* PROGRAM bkxtnwrite - write a buffer to extend a file
 *
 * RETURNS: number of blocks written
 */
LOCALF LONG
bkxtnwrite(
	dsmContext_t	*pcontext,
	bkftbl_t	*pbkftbl,
        gem_off_t	 offset, /* Usually a block offset - it is a byte
                                  * offset in the case where the extend abount
                                  * is not a multiple of block size.  In that
                                  * case, blocklog is exected to be zero so
                                  * the block to byte offset transformation
                                  * does not occur.
                                  */
	TEXT		*pbuf,
	LONG		 bufBlocks,
	int		 blockLog)
{
    int		blocksWritten;	/* resulting number of bytes written */
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dbshm_t   *pdbpub = pdbcontext->pdbpub;

     /* Check for active crashtesting */
    if (pdbcontext->p1crashTestAnchor)
        bkCrashTry (pcontext ,(TEXT *)"bkxtnwrite");

     /* 94-04-28-045 - set errno to ENOSPC.  If a write fails because of
        an out-of-space condition, it won't set errno unless it was unable
        to write any of the buffer. */

#ifdef ENOSPC
    errno = ENOSPC;
#endif

     if (pdbpub->useraw)
     {
	 blocksWritten = bkioWrite (pcontext,
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex],
                        &pbkftbl->ft_bkiostats,
                        offset, pbuf, (int)bufBlocks, blockLog, 
                        UNBUFFERED);
     }
     else
     {
	 /* not using reliable I/O -regular write on regular file descriptor */
         blocksWritten = bkioWrite (pcontext,
                         pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                         &pbkftbl->ft_bkiostats,
                         offset, pbuf, (int)bufBlocks, blockLog, 
                         BUFFERED);
     }

    return blocksWritten;

}  /* end bkxtnwrite */


/* PROGRAM: bkGetAreaFtbl - gete the bkftbk for the passed area
 *
 *
 * RETURNS: 0 - SUCCESS
 *         DSM_S_AREA_NOT_INIT  - FAILURE Area Descriptors not yet initiated
 *         DSM_S_AREA_NULL      - FAILURE No Such Area
 *         DSM_S_AREA_NO_EXTENT - FAILURE bkftbl == NULL
 */
LONG
bkGetAreaFtbl(
	dsmContext_t	*pcontext,
	ULONG		 Area,
	bkftbl_t	**pbkftbl)
{
    dbcontext_t        *pdbcontext = pcontext->pdbcontext;
    bkAreaDescPtr_t    *pbkAreaDescPtr;
    bkAreaDesc_t       *pbkAreaDesc;

    /* initialize the passed bkftbl */
    *pbkftbl = (bkftbl_t *)NULL;

    /* get the area descriptor pointers */
    pbkAreaDescPtr = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);

    if (pbkAreaDescPtr == (bkAreaDescPtr_t *)NULL)
    {
        /* Database has not AREAS INITIATED */
        return DSM_S_AREA_NOT_INIT;
    }
    if( Area > pbkAreaDescPtr->bkNumAreaSlots )
    {
       pbkAreaDesc = NULL;
    }
    else
    {
        /* get the area descriptor for the passed area */
        pbkAreaDesc = XBKAREAD(pdbcontext, pbkAreaDescPtr->bk_qAreaDesc[Area]);
    }

    if (pbkAreaDesc == (bkAreaDesc_t *)NULL)
    {
        /* No Such Area */
        return DSM_S_AREA_NULL;
    }

    /* finally get the bkftbl */
    if( pbkAreaDesc->bkNumFtbl > 0 )
    {
        *pbkftbl = pbkAreaDesc->bkftbl;
    }

    if (*pbkftbl == (bkftbl_t *)NULL)
    {
        /* no extents in this area */
        return DSM_S_AREA_NO_EXTENT;
    }

    /* we found the first extent of this area, return success */
    return 0;

}  /* end bkGetAreaFtbl */


/**************************************************************************/
/* PROGRAM: bkflen - find the data portion length, in BLOCKS,
 *		 of the db or bi file(s).
 *
 * RETURNS:
 *	The length of the data portion, in BLOCKS, of the db or bi file(s)
 */
LONG
bkflen (
	dsmContext_t	*pcontext,
	ULONG		 area)	
{

    bkftbl_t	*pbkftbl = (bkftbl_t *)NULL;
    LONG	 totlen;
    int		 ret;

    /* get pointer to appropriate file extent table */

    ret = bkGetAreaFtbl(pcontext, area, &pbkftbl);

    if (ret != 0)
    {
        return (ret);
    }

    /* add up the lengths for all extents */
    for (totlen = 0; pbkftbl && pbkftbl->ft_type != 0; pbkftbl++ )
    {
	totlen += (LONG)(pbkftbl->ft_curlen >> pbkftbl->ft_blklog);
    }

    return totlen;

}  /* end bkflen */


/***************************************************************/
/* PROGRAM: bktrunc - truncate the BI, AI or TL file or extents.
 *
 * The header of all fixed extents is rewritten to clear any alignment
 * space following in the extent header before the next block. 
 * This cleans up any old bits in the alignment space of the file after
 * blocksize changes.
 *
 * The variable-length extents are truncated to contain only the extent
 * header in a complete bi block to account for alignment.
 *
 * The first bi block of the initial extent is zeroed as a (a logical EOF).
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bktrunc(
        dsmContext_t    *pcontext,
        ULONG            area)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    bkftbl_t      *pbkftbl = (bkftbl_t *)NULL;
    ULONG         *pbuffer;
    fileHandle_t  fd;
    TEXT *        pname;
    int           count;
    ULONG         blockSize = 0;
    int           newBlklog;
    int           ret;
    int           filetype;
    int           errorStatus;
    
    ret = bkGetAreaFtbl(pcontext, area, &pbkftbl);
    
    switch (ret)
    {
    case DSM_S_SUCCESS:
        break;
    case DSM_S_AREA_NO_EXTENT:
    /* bug 19981112-026 - if no bi extent exist, procopy cores.
        Check if  runmode = procopy, msgn_callback and return   */
        if(strcmp ((char *)pcontext->pdbcontext->runmode,"procopy") == 0) 
        {
            MSGN_CALLBACK(pcontext,bkMSG139);
            return;
        }
        /* otherwise fall thru to default and fatal */
    default:
        FATAL_MSGN_CALLBACK(pcontext,bkFTL041, area, ret);
    }
    
    filetype = pbkftbl->ft_type & BKETYPE;
    
    /* retrieve new blocksize for file */
    switch (filetype)
    {
    case BKBI:
        /* use current bi blocksize */
        blockSize = pdbcontext->pmstrblk->mb_biblksize;
        break;
    case BKAI:
        /* use current ai blocksize */
        blockSize = pdbcontext->pmstrblk->mb_aiblksize;
        break;  
    default:
        FATAL_MSGN_CALLBACK(pcontext, bkFTL065, filetype);
    }
    
    /* make sure there is work to do before continuing */
    if (pbkftbl == NULL)
    {
        /* nothing to do since there are no files in list */
        return;
    }
    
    /* allocate a single block to be used as a buffer here */
    pbuffer = (ULONG *)stRentd(pcontext, pdsmStoragePool, 
        pbkftbl->ft_blksize > blockSize ? (unsigned)pbkftbl->ft_blksize
        : (unsigned) blockSize);
    
    newBlklog = bkblklog(blockSize);
    
    /* look through all the extents updating extent header blocks */
    for (count = 0;
    pbkftbl->ft_type > 0;
    pbkftbl++, count++)
    {
        /* get the name of the file */
        pname = XTEXT(pdbcontext, pbkftbl->ft_qname);
        
        /* get file descriptor for the extent */
        fd = bkioGetFd(pcontext,
            pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], BUFIO);
        
        /* clear the working buffer for a complete block */
        stnclr(pbuffer, (unsigned) blockSize);
        
        /* check for fixed size files that need headers updated */
        if ( pbkftbl->ft_type & BKFIXED )
        {
            /* *** TODO: this code could be changed to use bkRead/bkWrite */
            
            /* read the extent header block */
            ret = bkioRead(pcontext,
                pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                &pbkftbl->ft_bkiostats, (LONG)0,
                (TEXT *)pbuffer, 1,
                pbkftbl->ft_blklog, BUFFERED);
            if (ret < 0)
            {
                /* Error reading extent header in %s, ret = %d */
                FATAL_MSGN_CALLBACK(pcontext, bkFTL023, pname, ret);
            }
            
            /* (re)set new blocksize and blklog constants */
            pbkftbl->ft_blksize = blockSize;
            pbkftbl->ft_blklog = newBlklog;
            
            /* new length is total size minus new blocksize header */
            pbkftbl->ft_curlen = 
                (pbkftbl->ft_curlen + pbkftbl->ft_offset) - blockSize;
            
            /* new offset is new blocksize */
            pbkftbl->ft_offset = (gem_off_t)blockSize;
            
            /* Check for active crashtesting */
            if (pdbcontext->p1crashTestAnchor)
                bkCrashTry (pcontext ,(TEXT *)"bktrunc");
            
            /* write extent header block and zeros over rest of bi block */
            ret=bkioWrite(pcontext,
                pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                &pbkftbl->ft_bkiostats, (LONG)0,
                (TEXT *)pbuffer, 1,
                (int)pbkftbl->ft_blklog, UNBUFFERED);
            if (ret < 0)
            {
                /* Write failed on extent header in %s, ret = %d */
                FATAL_MSGN_CALLBACK(pcontext, bkFTL020, pname, ret);
            }
            
            /* first BI extent gets all zeros in first bi block */
            if ( (filetype == BKBI) && (count == 0) )
            {
                /* clear the working buffer for a complete block */
                stnclr(pbuffer, (unsigned) blockSize);
                
                /* write all zeros to the first bi data block in the file */
                ret=bkioWrite(pcontext,
                    pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                    &pbkftbl->ft_bkiostats,
                    pbkftbl->ft_offset >> pbkftbl->ft_blklog,
                    (TEXT *)pbuffer, 1,
                    (int)pbkftbl->ft_blklog, UNBUFFERED);
                if (ret < 0)
                {
                    /* Write failed on %s, ret = %d */
                    FATAL_MSGN_CALLBACK(pcontext, bkFTL019, pname, ret);
                }
            } 
        } /* fixed extents */
        else
        {   /* if the extent is not fixed-length, truncate or delete it
            * (the existing header will be saved and restored later)
            */
            
            /* read and save the extent header */
            ret = bkioRead(pcontext,
                pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                &pbkftbl->ft_bkiostats, (LONG) 0,
                (TEXT *)pbuffer, 1,
                pbkftbl->ft_blklog, BUFFERED);
            if (ret < 0)
            {
                /* Error reading extent header in %s, ret = %d */
                FATAL_MSGN_CALLBACK(pcontext, bkFTL023, pname, ret);
            }

            /* Truncate the file */
            ret = utTruncateFile(fd, (gem_off_t)0, &errorStatus);
            if (ret != DBUT_S_SUCCESS)
            {
                /* Truncation failed on extent header %s, ret = %d */
                FATAL_MSGN_CALLBACK(pcontext, bkFTL085, pname, ret);
            }
            
            /* reflect the true state of affairs of the file */
            pbkftbl->ft_offset = 0L;
            pbkftbl->ft_curlen = 0L;
            
            /* (re)set new blocksize and blklog constants */
            pbkftbl->ft_blksize = blockSize;
            pbkftbl->ft_blklog = newBlklog;
            
            /* write extent header back */
            /* first, extend the newly created file by at least one block */
            bkxtnfile(pcontext, pbkftbl, (LONG) 1);
            
            /* write extent block to the beginning of the file */
            ret=bkioWrite(pcontext,
                pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                &pbkftbl->ft_bkiostats, (LONG) 0,
                (TEXT *)pbuffer, 1,
                (int)pbkftbl->ft_blklog, UNBUFFERED);
            if (ret < 0)
            {
                /* Write failed on extent header %s, ret = %d */
                FATAL_MSGN_CALLBACK(pcontext, bkFTL020, pname, ret);
            }
            
            /* length of file is now physical - change to logical */
            pbkftbl->ft_offset = (gem_off_t)blockSize;
            pbkftbl->ft_curlen -= pbkftbl->ft_offset;       
            
        } /* variable length file */
        
    } /* file loop */
    
    /* free the buffer */
    stVacate(pcontext, pdsmStoragePool, (TEXT *)pbuffer);
    
}  /* end bktrunc */

/**************************************************************/
/* PROGRAM: bkAreaTruncate - truncate an area by updating the high water mark
 *
 *
 * RETURNS: 0 success
 *         -1 failure
 */
int
bkAreaTruncate(
    dsmContext_t *pcontext)
    
{
    dsmStatus_t      returnCode;
    ULONG            indicator;
 
    dsmArea_t        areaNumber;
    dsmRecord_t      objectRecord;
    TEXT             objectBuffer[MAXRECSZ];
    svcBool_t        areaTable[DSMAREA_MAXIMUM+1] = {0};
 
    bkAreaDesc_t    *pbkAreaDesc;
 
    dsmCursid_t      cursorId = DSMCURSOR_INVALID;
 
    svcDataType_t    component[2];
    svcDataType_t   *pcomponent[2];
    COUNT            maxKeyLength = 64; /* SHOULD BE A CONSTANT! */
 
    objectBlock_t   *pobjectBlock;
    bmBufHandle_t    bufHandle, controlBufHandle;

    mstrblk_t       *pcontrolBlock;
    TEXT            *pareaName;
    DBKEY            areaRecid;
    UCOUNT           controlRecbits;
    ULONG            recordSize = 0;

 
    AUTOKEY(objKeyLow, 64)
    AUTOKEY(objKeyHi, 64)
 
    /* 1. look at all _storage objects and enable the flag in the areaTable if
     *    one exists.
     * 2. traverse the area descriptors.
     * 3. For each valid area Descriptor, if
     *    then areaTable[area] is not enabled then it is OK to update
     *    its high water mark.
     */
 
    /* set up keys to define bracket for scan of the area table */
    objKeyLow.akey.area         = DSMAREA_SCHEMA;
    objKeyHi.akey.area          = DSMAREA_SCHEMA;
    objKeyLow.akey.root       = pcontext->pdbcontext->pmstrblk->mb_objectRoot;
    objKeyHi.akey.root        = pcontext->pdbcontext->pmstrblk->mb_objectRoot;
    objKeyLow.akey.unique_index = 1;
    objKeyHi.akey.unique_index  = 1;
    objKeyLow.akey.word_index   = 0;
    objKeyHi.akey.word_index    = 0;
 
    /* create bracket */
    component[0].indicator = SVCLOWRANGE;
    pcomponent[0]          = &component[0];
 
    objKeyLow.akey.index    = SCIDX_OBJID;
    objKeyLow.akey.keycomps = 1;
    objKeyLow.akey.ksubstr  = 0;
 
    returnCode = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                          &objKeyLow.akey);
    if (returnCode)
        goto errdone;
 
    /* high range consists of table num followed by ANY filed position value */
    component[0].indicator = SVCHIGHRANGE;
    pcomponent[0]          = &component[0];
 
    objKeyHi.akey.index    = SCIDX_OBJID;
    objKeyHi.akey.keycomps = 1;
    objKeyHi.akey.ksubstr  = 0;
 
    returnCode = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                          &objKeyHi.akey);
    if (returnCode)
        goto errdone;
 
    /* find area number of each _storageObject record. */
    returnCode = (dsmStatus_t)cxFind(pcontext,
                               &objKeyLow.akey, &objKeyHi.akey,
                               &cursorId, SCTBL_OBJECT,
                               &objectRecord.recid,
                               DSMPARTIAL, DSMFINDFIRST,
                               (dsmMask_t)DSM_LK_SHARE);
    if (returnCode)
        goto errdone;
 
    while (!returnCode)
    {
        /* Get the _StorageObject record */
        objectRecord.table        = SCTBL_OBJECT;
        objectRecord.maxLength    = MAXRECSZ;
        objectRecord.pbuffer      = objectBuffer;
        objectRecord.recLength = rmFetchRecord(pcontext, DSMAREA_SCHEMA,
                                    objectRecord.recid, objectRecord.pbuffer,
                                    MAXRECSZ, 0);
 
        if ( objectRecord.recLength > MAXRECSZ)
        {
            returnCode = -1;
            goto errdone;
        }
 
        /* get the area for the _storageObject record */
        returnCode = recGetLONG(objectRecord.pbuffer, SCFLD_OBJAREA, 0,
                                (svcLong_t *)&areaNumber, &indicator);
        if (returnCode)
            goto errdone;

        if (indicator)
            goto errdone;
       
        /* mark the area as in use */
        if (areaNumber > DSMAREA_MAXIMUM)
             goto errdone;
        
        areaTable[areaNumber] = 1;
 
        /* find next entry */
         returnCode = cxNext(pcontext,
                             &cursorId, SCTBL_OBJECT,
                             &objectRecord.recid,
                             &objKeyLow.akey, &objKeyHi.akey,
                             DSMPARTIAL, DSMFINDNEXT,
                             (dsmMask_t)DSM_LK_SHARE);
    }
 
    if (returnCode == DSM_S_ENDLOOP)
        returnCode = 0;

    cxDeactivateCursor(pcontext, &cursorId);
 
    /* now traverse each valid area and determine which ones could
     * be "truncated".
     */
    for (areaNumber = 1; areaNumber <= DSMAREA_MAXIMUM; areaNumber++)
    {
        /* skip areas that have objects */
        if (areaTable[areaNumber])
            continue;

        pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, areaNumber);
 
        if (pbkAreaDesc &&
            (pbkAreaDesc->bkAreaType == DSMAREA_TYPE_DATA))
        {
 
            bufHandle = bmLocateBlock(pcontext, pbkAreaDesc->bkAreaNumber,
                              BLK2DBKEY(pbkAreaDesc->bkAreaRecbits), TOMODIFY);
            pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,
                                                              bufHandle);
 
            if (pobjectBlock->hiWaterBlock > 2)
            {
	        controlRecbits = BKGET_AREARECBITS(DSMAREA_CONTROL);
                controlBufHandle = bmLocateBlock(pcontext, DSMAREA_CONTROL, BLK1DBKEY(controlRecbits), TOREAD);
                pcontrolBlock = (mstrblk_t *)bmGetBlockPointer(pcontext,controlBufHandle);

                pareaName = (TEXT *)utmalloc(MAXPATHN);

                /* given the area number, get the name */
                returnCode = bkFindAreaName (pcontext, pcontrolBlock, pareaName,
                                              &pbkAreaDesc->bkAreaNumber,
                                              &areaRecid, &recordSize);
                bmReleaseBuffer(pcontext, controlBufHandle);

                /* Truncating area areaNumber. */
                MSGN_CALLBACK(pcontext, bkMSG146, pareaName); 

                pobjectBlock->hiWaterBlock = 2;
                pobjectBlock->chainFirst[0] = 0;
                pobjectBlock->chainFirst[1] = 0;
                pobjectBlock->chainFirst[2] = 0;
                pobjectBlock->numBlocksOnChain[0] = 0;
                pobjectBlock->numBlocksOnChain[1] = 0;
                pobjectBlock->numBlocksOnChain[2] = 0;
                pobjectBlock->chainLast[0] = 0;
                pobjectBlock->chainLast[1] = 0;
                pobjectBlock->chainLast[2] = 0;

                pbkAreaDesc->bkHiWaterBlock = 2;
                pbkAreaDesc->bkChainFirst[0] = 0;
                pbkAreaDesc->bkChainFirst[1] = 0;
                pbkAreaDesc->bkChainFirst[2] = 0;
                pbkAreaDesc->bkNumBlocksOnChain[0] = 0;
                pbkAreaDesc->bkNumBlocksOnChain[1] = 0;
                pbkAreaDesc->bkNumBlocksOnChain[2] = 0;
 
                bmMarkModified(pcontext, bufHandle, 0);
            }
 
            bmReleaseBuffer(pcontext, bufHandle);
        }
    }

    return 0;

errdone:
    /* An error occurred during area truncation. rtc %l. */
   MSGN_CALLBACK(pcontext, bkMSG144, returnCode);

    return -1;

}  /* end bkAreaTruncate */


/**************************************************************/
/* PROGRAM: bkxtnfree - Release the buffer for extending files
 *
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkxtnfree(
	dsmContext_t *pcontext)
{
    PL_LOCK_EXTBUFFER(&extBufferLatch)

    if(pBkExtBuf != (bkbuf_t *)NULL)
    {
        stVacate(pcontext, pdsmStoragePool, (TEXT *)pBkExtBuf);
        pBkExtBuf = (bkbuf_t *)NULL;
    }

    PL_UNLK_EXTBUFFER(&extBufferLatch)

}  /* end bkxtnfree */


/**************************************************************
 * PROGRAM: bkAreaAddDo - Add an area to the cache
 *
 *
 * RETURNS: 
 */
DSMVOID
bkAreaAddDo(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,
	bkbuf_t		*pbkbuf _UNUSED_,
	COUNT		 dlen _UNUSED_,
	TEXT		*pdata _UNUSED_)
{
    dbcontext_t    *pdbcontext = pcontext->pdbcontext;
    dbshm_t        *pdbpub = pdbcontext->pdbpub;
    bkAreaNote_t   *pAreaNote;
    bkAreaDesc_t   *pbkArea;
    bkAreaDescPtr_t      *pbkAreas;

    pAreaNote = (bkAreaNote_t *)pnote;

    pbkArea = BK_GET_AREA_DESC(pdbcontext, pAreaNote->area);

    if( pbkArea == NULL )
    {
	/* We need to allocate some storage for the area descriptor */
	pbkArea = (bkAreaDesc_t *)stRentIt(pcontext,
                                          XSTPOOL(pdbcontext, pdbpub->qdbpool),
					(unsigned)BK_AREA_DESC_SIZE(2), 0);
	if ( pbkArea == NULL )
	{
	    FATAL_MSGN_CALLBACK(pcontext,bkFTL042);
            /* Can't allocate storage for area descriptor */
	}
    }

    pbkArea->bkAreaNumber   = pAreaNote->area;
    pbkArea->bkAreaType     = pAreaNote->areaType;
    pbkArea->bkAreaRecbits  = pAreaNote->areaRecbits;
    pbkArea->bkBlockSize    = pAreaNote->blockSize;
    pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
    pbkAreas->bk_qAreaDesc[pAreaNote->area] = (QBKAREAD)P_TO_QP(pcontext, pbkArea);
    
    return;

}  /* end bkAreaAddDo */


 /**************************************************************
 * PROGRAM: bkAreaRemoveDo - Remove an area from the cache
 *
 *
 * RETURNS: 
 */
DSMVOID
bkAreaRemoveDo(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,
	bkbuf_t		*pbkbuf _UNUSED_,
	COUNT	 	 dlen _UNUSED_,
	TEXT		*pdata _UNUSED_)
{
   
    dbcontext_t    *pdbcontext = pcontext->pdbcontext;
    bkAreaNote_t   *pAreaNote;
    bkAreaDesc_t   *pbkArea;

    pAreaNote = (bkAreaNote_t *)pnote;

    pbkArea = BK_GET_AREA_DESC(pdbcontext, pAreaNote->area);

    if (pbkArea)
    { 
       pbkArea->bkDelete = 1;
       bmAreaFreeBuffers(pcontext, pAreaNote->area);
    }

    return;

}  /* end bkAreaRemoveDo */


/**************************************************************
 * PROGRAM: bkExtentCreate - Adds an extent to the db         *
 *
 *
 * RETURNS: 
 */
dsmStatus_t
bkExtentCreate(
	dsmContext_t	*pcontext,
	bkAreaDesc_t	*pbkArea,
	dsmSize_t       *pinitialSize,  /* initial size IN BLOCKS */
        GTBOOL		 extentType,
	dsmText_t 	*pname)
{
    dbcontext_t             *pdbcontext = pcontext->pdbcontext;
    bkExtentAddNote_t        extentNote;
    bkWriteObjectBlockNote_t objectBlockNote;
    objectBlock_t           *pobjectBlock;
    bmBufHandle_t            objHandle;

    /* create extent with extent header and possibly area & object blocks */
    INITNOTE( extentNote, RL_BKEXTENT_CREATE, RL_LOGBI | RL_PHY );
    INIT_S_XTDADDNOTE_FILLERS( extentNote );	/* bug 20000114-034 by dmeyer in v9.1b */

    extentNote.area = pbkArea->bkAreaNumber;
    extentNote.initialSize = *pinitialSize;
    extentNote.extentNumber = pbkArea->bkNumFtbl + 1;
    extentNote.extentType = extentType;
    if (pbkArea->bkNumFtbl)
        extentNote.areaType = pbkArea->bkAreaType;
    else
    {  /* cannot rely on pbkArea->bkAreaType if no extents exist in the area */
        switch (extentNote.extentType & BKETYPE)
        {
          case BKBI:
             extentNote.areaType = DSMAREA_TYPE_BI;
             break;
          case BKAI:
             extentNote.areaType = DSMAREA_TYPE_AI;
          case BKTL:
             extentNote.areaType = DSMAREA_TYPE_TL;
             break;
          default:
             extentNote.areaType = DSMAREA_TYPE_DATA;
       }
    }
    
    rlLogAndDo(pcontext, (RLNOTE *)&extentNote, 0, (COUNT)stlen(pname)+1,pname);

    *pinitialSize = extentNote.initialSize;

    if (extentNote.areaType == DSMAREA_TYPE_DATA)
    {
      /* HUGE NOTE:
       * pbkArea is probably no longer valid since the adding of an extent
       * will reallocate the area this points to.  Reference the saved
       * values instead.
       */
      pbkArea = BK_GET_AREA_DESC(pdbcontext, extentNote.area);

      if (pbkArea->bkMaxDbkey == 0)
      {
          /* error encountered during add */
          return -1; 
      }

      objHandle = bmLocateBlock(pcontext, extentNote.area,
                            (2L << pbkArea->bkAreaRecbits), TOMODIFY);

      /* Get pointer to object block   */
      pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext,objHandle);

      /* Create object note and write out the object block */
      INITNOTE(objectBlockNote, RL_BKWRITE_OBJECT_BLOCK, RL_PHY);
      objectBlockNote.areaNumber   = extentNote.area;
      if ( extentNote.extentNumber == 1)
      {
          objectBlockNote.totalBlocks  = *pinitialSize - 1;
          objectBlockNote.hiWaterBlock = 3;
          objectBlockNote.initBlock = 1;
      }
      else
      {
          objectBlockNote.totalBlocks =
                                pobjectBlock->totalBlocks + *pinitialSize - 1;
          objectBlockNote.hiWaterBlock = pobjectBlock->hiWaterBlock;
          objectBlockNote.initBlock = 0;
      }

      rlLogAndDo(pcontext, (RLNOTE *)&objectBlockNote, objHandle,
                 0, NULL);

      /* release the buffer block */
      bmReleaseBuffer(pcontext, objHandle);
    }

    return 0;

}  /* end bkExtentCreate */


/******************************************************************
 * PROGRAM: bkExtentCreateFile - Adds an extent to the db         *
 *
 *
 * RETURNS: 0 on success
 *          1 on success, but no file created
 *         -1 on failure
 */
dsmStatus_t
bkExtentCreateFile(
	dsmContext_t	*pcontext,
	bkftbl_t	*pExtent,
	ULONG	 	*pinitialSize, /* initial size IN BLOCKS */
        dsmExtent_t	 extentNumber,
        dsmRecbits_t	 areaRecbits,
        ULONG            areaType)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dbshm_t  *pdbpub = pdbcontext->pdbpub;
    dsmStatus_t    rc;
    fileHandle_t   fd;
    extentBlock_t *pextentBlock;
    COUNT          thisDate;
    gem_off_t      fileSize;
    bkftbl_t      *pFtbl;
    ULONG          creationDate;
    ULONG          openDate;
    bkbuf_t       *pdbBlockBuffer;
    areaBlock_t   *pareaBlock;
    objectBlock_t *pobjectBlock;
    ixAnchorBlock_t *pixAnchorBlock;
    AIBLK         *pAiHeaderBlock;
    int            errorStatus;
    TEXT           fileName[MAXPATHN];
    TEXT           *pfileName;


    /* If datadir set prepend file name with that directory  */
    if(pdbcontext->pdatadir)
    {
        pfileName = fileName;
        utmypath(pfileName,pdbcontext->pdatadir,
                 XTEXT(pdbcontext,pExtent->ft_qname),NULL);   
    }
    else
    {
        pfileName = XTEXT(pdbcontext,pExtent->ft_qname);
    }
    
    
    /* Determine if the file exists                         */
    
    rc = utfchk(pfileName,UTFEXISTS);
    if( rc == utMSG001 )
    {
	/* File does not exist -- so create it              */
	fd = utOsCreat(pfileName , CREATE_DATA_FILE,
                             0, &errorStatus);
	if( fd == INVALID_FILE_HANDLE)
	{
	    MSGN_CALLBACK(pcontext, utFTL007,
		 pfileName, errorStatus);
	}
#if OPSYS == UNIX
	/* If file did not exist, then it was created with the effective user
	   and group id, which should be root. We must fix it to belong to the
	   real user. */
	chown ((char *)pfileName, getuid (), getgid () );
#endif
	utOsClose( fd,CREATE_DATA_FILE );
    }
    else if (pdbcontext->prlctl->rlwarmstrt)
    {
        /* if the file already existed and we are in the redo phase
           of crash recovery, there is no need to open the file since
           it was opened in bkopen */
        return 1;
    }

    switch (areaType)
    {
        case DSMAREA_TYPE_BI:
            pExtent->ft_openmode = UNBUFIO;
            break;

        case DSMAREA_TYPE_TL:
            pExtent->ft_openmode = UNBUFIO;
            break;

        case DSMAREA_TYPE_DATA:
            pExtent->ft_openmode = BOTHIO;
            break;

        case DSMAREA_TYPE_AI:
            pExtent->ft_openmode = UNBUFIO;
            break;

    }

    /* override the above is directio is turned on */
    if (pdbpub->directio)
        pExtent->ft_openmode = UNBUFIO;

    rc = bkOpenOneFile(pcontext, pExtent,OPEN_RW);
    if ( rc )
	return rc;

    /* Get the size of the file                             */
    if (utflen(pfileName,&fileSize))
    {
	FATAL_MSGN_CALLBACK(pcontext, bkFTL045,
	     pfileName);
    }
    
    /* Read in the extent header of the control area. */
    rc = bkGetAreaFtbl(pcontext, DSMAREA_CONTROL, &pFtbl);
    if( rc )
    {
	FATAL_MSGN_CALLBACK(pcontext,bkFTL057);
    }

    pdbBlockBuffer = (bkbuf_t *)utmalloc(pExtent->ft_blksize);
    if (!pdbBlockBuffer)
	FATAL_MSGN_CALLBACK(pcontext,bkFTL058); 
    pextentBlock = (extentBlock_t *)pdbBlockBuffer;

    rc = bkioRead(pcontext,
                   pdbcontext->pbkfdtbl->pbkiotbl[pFtbl->ft_fdtblIndex], 
		   &pFtbl->ft_bkiostats, (LONG)0, 
		   (TEXT *)pextentBlock, 1,
                   pExtent->ft_blklog, BUFFERED);
    if( rc != 1 )
    {
	FATAL_MSGN_CALLBACK(pcontext,bkFTL060,XTEXT(pdbcontext, pFtbl->ft_qname));
    }
    BK_FROM_STORE(&pextentBlock->bk_hdr);

    thisDate     = pextentBlock->dateVerification;
    creationDate = pextentBlock->creationDate[thisDate];
    openDate     = pextentBlock->lastOpenDate[thisDate];

    if( (fileSize > 0) && 
        (fileSize < (ULONG)pExtent->ft_blksize) )
    {
	FATAL_MSGN_CALLBACK(pcontext,bkFTL063,
	    fileSize, pfileName);
    }
    else if ( fileSize == 0 )
    {
	/* Write out an extent header                         */
        switch (pExtent->ft_type & BKETYPE)
        {
          case BKAI:
	    pextentBlock->extentType = DBMAI;
            break;
          case BKBI:
	    pextentBlock->extentType = DBMBI;
            break;
          case BKTL:
            pextentBlock->extentType = DBMTL;
            break;
	  default:
	    pextentBlock->extentType = DBMDB;
        }
	BK_TO_STORE(&pextentBlock->bk_hdr);
        /* write the extent block */
	rc = bkioWrite(pcontext,
                       pdbcontext->pbkfdtbl->pbkiotbl[pExtent->ft_fdtblIndex],
		       &pExtent->ft_bkiostats,(LONG)0,
                       (TEXT *)pextentBlock, 1,
                       pExtent->ft_blklog, UNBUFFERED);
        if( rc != 1 )
        {
	    FATAL_MSGN_CALLBACK(pcontext,bkFTL076,
	         XTEXT(pdbcontext, pFtbl->ft_qname));
        }
	BK_FROM_STORE(&pextentBlock->bk_hdr);

	fileSize += pExtent->ft_blksize;

        /* NOTE: this is NOT intended for schema area - there would
         *       be a mstrblk as block 1 rather than an area block
         */
        if ( (pextentBlock->extentType == DBMDB) &&
             (extentNumber == 1) )
        {

            /* build and write the area block */
            pareaBlock = (areaBlock_t *)pdbBlockBuffer;
            stnclr( (TEXT *)pareaBlock, pExtent->ft_blksize);
            pareaBlock->BKDBKEY  = (DBKEY)(1L << areaRecbits);
            pareaBlock->BKTYPE   = AREABLK;
            pareaBlock->BKFRCHN  = NOCHN;
            pareaBlock->BKINCR   = 1;
	    BK_TO_STORE(&pareaBlock->bk_hdr);
	    rc = bkioWrite(pcontext,
                         pdbcontext->pbkfdtbl->pbkiotbl[pExtent->ft_fdtblIndex],
                         &pExtent->ft_bkiostats, (LONG)1,
                         (TEXT *)pdbBlockBuffer, 1,
                         pExtent->ft_blklog, UNBUFFERED);
            if( rc != 1 )
            {
	        FATAL_MSGN_CALLBACK(pcontext,bkFTL077,
                     XTEXT(pdbcontext, pFtbl->ft_qname));
            }
            fileSize += pExtent->ft_blksize;

            /* build and write the object block */
            pobjectBlock = (objectBlock_t *)pdbBlockBuffer;
            stnclr( (TEXT *)pobjectBlock, pExtent->ft_blksize);
            pobjectBlock->BKDBKEY      = (DBKEY)(2L<< areaRecbits);
            pobjectBlock->BKTYPE       = OBJECTBLK;
            pobjectBlock->BKFRCHN      = NOCHN;
            pobjectBlock->BKINCR       = 1;
            pobjectBlock->totalBlocks  = INIT_OBJECT_HIWATER; 
            pobjectBlock->hiWaterBlock = INIT_OBJECT_HIWATER;
	    BK_TO_STORE(&pobjectBlock->bk_hdr);
            rc = bkioWrite(pcontext,
                         pdbcontext->pbkfdtbl->pbkiotbl[pExtent->ft_fdtblIndex],
 	                 &pExtent->ft_bkiostats, (LONG)2,
                         (TEXT *)pdbBlockBuffer, 1,
                         pExtent->ft_blklog, UNBUFFERED);
            if( rc != 1 )
            {
                FATAL_MSGN_CALLBACK(pcontext,bkFTL078,
                    XTEXT(pdbcontext, pFtbl->ft_qname));
            }
            fileSize += pExtent->ft_blksize;

            /* build and write the Index Anchor Block */
            pixAnchorBlock = (ixAnchorBlock_t *)pdbBlockBuffer;
            stnclr( (TEXT *)pobjectBlock, pExtent->ft_blksize);
            pixAnchorBlock->BKDBKEY      = (DBKEY)(3L<< areaRecbits);
            pixAnchorBlock->BKTYPE       = IXANCHORBLK;
            pixAnchorBlock->BKFRCHN      = NOCHN;
            pixAnchorBlock->BKINCR       = 1;
	    BK_TO_STORE(&pixAnchorBlock->bk_hdr);
            rc = bkioWrite(pcontext,
                         pdbcontext->pbkfdtbl->pbkiotbl[pExtent->ft_fdtblIndex],
 	                 &pExtent->ft_bkiostats, (LONG)3,
                         (TEXT *)pdbBlockBuffer, 1,
                         pExtent->ft_blklog, UNBUFFERED);
            if( rc != 1 )
            {
                FATAL_MSGN_CALLBACK(pcontext,bkFTL078,
                    XTEXT(pdbcontext, pFtbl->ft_qname));
            }
            fileSize += pExtent->ft_blksize;
        }
        else if (pextentBlock->extentType == DBMAI)
        {
            pAiHeaderBlock = (AIBLK *)pdbBlockBuffer;

            stnclr(pAiHeaderBlock, pExtent->ft_blksize);
            pAiHeaderBlock->aihdr.aihdrlen = sizeof(AIFIRST) + sizeof(AIHDR);
            pAiHeaderBlock->aihdr.aiused = pAiHeaderBlock->aihdr.aihdrlen;
            pAiHeaderBlock->aifirst.aiextstate = AIEXTEMPTY;
                 
            rc = bkioWrite(pcontext,
                        pdbcontext->pbkfdtbl->pbkiotbl[pExtent->ft_fdtblIndex],
 	                &pExtent->ft_bkiostats, (LONG)1,
                        (TEXT *)pdbBlockBuffer, 1,
                        pExtent->ft_blklog, UNBUFFERED);
            if( rc != 1 )
            {
                FATAL_MSGN_CALLBACK(pcontext,bkFTL079,
                    XTEXT(pdbcontext, pFtbl->ft_qname));
            }
            fileSize += pExtent->ft_blksize;
        }
#if OPSYS==WIN32API
       /* On windows, the file size will be 0 until the file
          has been closed, so close the file and reopen */
       bkioClose(pcontext, 
                 pdbcontext->pbkfdtbl->pbkiotbl[pExtent->ft_fdtblIndex], 0);
       rc = bkOpenOneFile(pcontext, pExtent,OPEN_RW);
       if ( rc )
	   return rc;
#endif
    }
    else
    {
	/* If more than a zero length file read in the first
	 * block and test if its one of our files
         */
	rc = bkioRead(pcontext,
                      pdbcontext->pbkfdtbl->pbkiotbl[pExtent->ft_fdtblIndex], 
		      &pExtent->ft_bkiostats, (LONG) 0, 
		      (TEXT *)pextentBlock, 1,
                      pExtent->ft_blklog, BUFFERED);
	if( rc != 1 )
	{
	    FATAL_MSGN_CALLBACK(pcontext,bkFTL080,
		 pfileName);
	}
	BK_FROM_STORE(&pextentBlock->bk_hdr);
	if( pextentBlock->creationDate[thisDate] != creationDate )
	{
	    FATAL_MSGN_CALLBACK(pcontext,bkFTL081,pfileName);
	}
        /* Update the last opened date and write the extent header
           block back out.                                        */
        pextentBlock->lastOpenDate[thisDate] = openDate;
	BK_TO_STORE(&pextentBlock->bk_hdr);
        rc = bkioWrite(pcontext,
                       pdbcontext->pbkfdtbl->pbkiotbl[pExtent->ft_fdtblIndex],
		       &pExtent->ft_bkiostats,(LONG)0,
                       (TEXT *)pextentBlock, 1,
                       pExtent->ft_blklog, UNBUFFERED);
        if( rc != 1 )
        {
	    FATAL_MSGN_CALLBACK(pcontext,bkFTL076,
	         XTEXT(pdbcontext, pFtbl->ft_qname));
        }
    }

    utfree((TEXT *)pdbBlockBuffer);
    
    /* Get the file extended to the requested size                  */
    pExtent->ft_offset = (gem_off_t)pExtent->ft_blksize;
    pExtent->ft_curlen = fileSize - pExtent->ft_blksize;
    pExtent->ft_xtndamt = 64;

    /* initial size is a request of blocks
     * pExtent->ft_curlen and fileSize are the file size in bytes
     * fileSize includes the extent header block.
     * pExtent->ft_curlen DOES NOT include the extent header block.
     * bkxtnfile extends the file by blocks.
     * If the current number of bytes in the file is less than the 
     * requested number of bytes, extend the database by enough
     * blocks to reach the requested number of bytes.
     */
    if ( *pinitialSize > (ULONG)(fileSize / pExtent->ft_blksize) )
    {
        ULONG blocksExtended;
        ULONG blocksWanted;
        blocksWanted = (*pinitialSize - (ULONG)(fileSize / pExtent->ft_blksize));
	blocksExtended = bkxtnfile(pcontext, pExtent,  
                (*pinitialSize - (ULONG)(fileSize / pExtent->ft_blksize)) );
        if (blocksWanted > blocksExtended)
        {
           if (UT_DELETE((char *)pfileName) == -1)
           {
               MSGN_CALLBACK(pcontext, 
                   mvMSG288, pfileName);
           }
           else
           {
               MSGN_CALLBACK(pcontext, bkMSG157, pfileName); 
           }
        }
        *pinitialSize = blocksExtended + (ULONG)(fileSize / pExtent->ft_blksize);
    }
    return 0;

}  /* end bkExtentCreateFile */


/******************************************************************
 * PROGRAM: bkExtentRemoveDo - Removes in memory extent
 *                             descriptors and then
 *                             attempts to close and delete the file.       
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkExtentRemoveDo(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,
	bkbuf_t		*pblk _UNUSED_,
	COUNT		 dlen _UNUSED_,
	TEXT		*pExtentPath _UNUSED_)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    bkAreaDesc_t      *pbkArea;
    bkExtentAddNote_t *pExtentNote;


    pExtentNote = (bkExtentAddNote_t *)pnote;

    pbkArea = BK_GET_AREA_DESC(pdbcontext, pExtentNote->area);

    if (pbkArea)
    {
        pbkArea->bkftbl[pExtentNote->extentNumber - 1].ft_delete = 1;
    }

}  /* end bkExtentRemoveDo */


/******************************************************************
 * PROGRAM: bkExtentCreateDo - Allocates in memory extent
 *                             descriptors and then
 *                             creates the file.       
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkExtentCreateDo(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,
	bkbuf_t		*pblk _UNUSED_,
	COUNT		 dlen _UNUSED_,
	TEXT		*pExtentPath _UNUSED_)
{
    LONG               rc;
    ULONG              i;
    bkAreaDesc_t      *pNewArea;
    bkAreaDesc_t      *pbkArea;
    bkAreaDescPtr_t   *pbkAreas;
    TEXT              *pName;
    bkftbl_t          *pExtent;
#ifdef BK_ONLINE_EXTENT_ADD
    bktbl_t           *pbktbl;
#endif
    bkExtentAddNote_t *pExtentNote;
    dbcontext_t	      *pdbcontext = pcontext->pdbcontext;
    dbshm_t           *pdbpub = pdbcontext->pdbpub;
    LONG               areaRecbits;
    
    pExtentNote = (bkExtentAddNote_t *)pnote;

    pbkArea = BK_GET_AREA_DESC(pdbcontext, pExtentNote->area);

    areaRecbits = pbkArea->bkAreaRecbits;
    
    pNewArea = NULL;
    
    if( pExtentNote->extentNumber > pbkArea->bkNumFtbl )
    {
	pNewArea = (bkAreaDesc_t *)stRentIt(pcontext,
                                           XSTPOOL(pdbcontext, pdbpub->qdbpool),
				BK_AREA_DESC_SIZE(pbkArea->bkNumFtbl + 2), 0);
	if( pNewArea == NULL )
	{
	    FATAL_MSGN_CALLBACK(pcontext,bkFTL082);
	}
    
	pName = (TEXT *)stRentIt(
            pcontext,
            XSTPOOL(pdbcontext, pdbpub->qdbpool),MAXPATHN + 1, 0);
    
	if ( pName == NULL )
	{
	    stVacate(pcontext, XSTPOOL(pdbcontext, pdbpub->qdbpool),(TEXT *)pNewArea);
	    FATAL_MSGN_CALLBACK(pcontext,bkFTL083);
	}
	/* Copy the existing area descriptor to the newly allocated storage */
	bufcop((TEXT *)pNewArea, (TEXT *)pbkArea, 
	       BK_AREA_DESC_SIZE(pbkArea->bkNumFtbl + 1));

	/* Copy the extent names                                            */
	for ( i = 0; i < pbkArea->bkNumFtbl; i++ )
	{
	    /* Fix up the q pointers to the new names area                  */
	    pNewArea->bkftbl[i].ft_qname = pbkArea->bkftbl[i].ft_qname;
	    pNewArea->bkftbl[i].qself = P_TO_QP(pcontext, &pNewArea->bkftbl[i]);
	}
	stVacate(pcontext, XSTPOOL(pdbcontext, pdbpub->qdbpool),(TEXT *)pbkArea);
	/* Initialize attributes of new extent                              */
	pExtent = &pNewArea->bkftbl[pNewArea->bkNumFtbl];
	pExtent->qself = P_TO_QP(pcontext, pExtent);
	pExtent->ft_type = pExtentNote->extentType;
        utmypath(pName,pdbcontext->pdatadir,pExtentPath,NULL);
	pExtent->ft_qname = P_TO_QP(pcontext, pName);
	pExtent->ft_blksize = pNewArea->bkBlockSize;
	pExtent->ft_blklog = bkblklog(pExtent->ft_blksize);
        pExtent->ft_fdtblIndex = bkInitIoTable (pcontext, 1);
        pExtent->ft_curlen = 0;

	pNewArea->bkNumFtbl++;
    
	/* Hook new area into shared memory data structures         */
	pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
	
	pbkAreas->bk_qAreaDesc[pExtentNote->area] = (QBKAREAD)P_TO_QP(pcontext, pNewArea);

#ifdef BK_ONLINE_EXTENT_ADD
        /* Update ALL in use bktbl buffer pool references associated
         * with this area
         */
        for ( pbktbl = XBKTBL(pdbcontext, pdbcontext->pbkctl->bk_qbktbl);
              pbktbl != (bktbl_t *)0;
              pbktbl = XBKTBL(pdbcontext, pbktbl->bt_qnextb) )
        {
            if (pbktbl->bt_area == pExtentNote->area )
                bkaddr(pcontext, pbktbl, 0);
        }
#endif
    }
    else
    {
	pExtent = &pbkArea->bkftbl[pExtentNote->extentNumber - 1];
    }
    /* Create the file for the extent and initialize bkio sub-system */
    rc = bkExtentCreateFile(pcontext, pExtent, &pExtentNote->initialSize,
                            pExtentNote->extentNumber,
                            areaRecbits, pExtentNote->areaType);

    /* initialize the bkMaxDbkey if this is a new extent in a new area */
    if (rc == 0)
    {
        if (pNewArea)
        {
            pNewArea->bkMaxDbkey += (DBKEY)(pExtentNote->initialSize - 1) <<
                                pNewArea->bkAreaRecbits;
        }
        else
        {
            pbkArea->bkMaxDbkey += (DBKEY)(pExtentNote->initialSize - 1) <<
                                pbkArea->bkAreaRecbits;
            pbkArea->bkHiWaterBlock = INIT_OBJECT_HIWATER;
            pbkArea->bkDelete = 0;
        }
        
    }
    
  

}  /* end bkExtentCreateDo */


/******************************************************************
 * PROGRAM: bkWriteAreaBlockDo - creates and writes out an area block
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkWriteAreaBlockDo(
	dsmContext_t	*pcontext,
        RLNOTE		*pnote _UNUSED_,
        bkbuf_t		*pblk _UNUSED_,
        COUNT		 dlen _UNUSED_,
        TEXT		*pdata _UNUSED_)
{
#if 0
    bkWriteAreaBlockNote_t *pbkWriteAreaBlockNote =
                                 (bkWriteAreaBlockNote_t *)pnote;
#endif

    MSGD_CALLBACK(pcontext,
                  (TEXT *)"%GbkWriteAreaBlockDo: Note not implemented.");

}  /* end bkWriteAreaBlockDo */

/******************************************************************
 * PROGRAM: bkWriteObjectBlockDo - creates and writes out an object block
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkWriteObjectBlockDo(
	dsmContext_t	*pcontext,
	RLNOTE		*pnote,
	bkbuf_t		*pblk,
	COUNT		 dlen _UNUSED_,
	TEXT		*pdata _UNUSED_)
{
    bkAreaDesc_t       *pbkAreaDesc;
    objectBlock_t      *pObjectBlock;

    pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext,
                                   xlng((TEXT *)&pnote->rlArea));

    pObjectBlock = (objectBlock_t *)pblk;
    if ( ((bkWriteObjectBlockNote_t *)pnote)->initBlock)
    {
        stnclr((TEXT *)&pObjectBlock->totalBlocks, 
                      (sizeof(objectBlock_t) - sizeof(struct bkhdr)));
    }
    pObjectBlock->totalBlocks  =
                            ((bkWriteObjectBlockNote_t *)pnote)->totalBlocks;
    pbkAreaDesc->bkMaxDbkey = 
              ((DBKEY)((bkWriteObjectBlockNote_t *)pnote)->totalBlocks + 1) <<
                     pbkAreaDesc->bkAreaRecbits;
 
    pObjectBlock->hiWaterBlock =
                            ((bkWriteObjectBlockNote_t *)pnote)->hiWaterBlock;
    pbkAreaDesc->bkHiWaterBlock           = 
                           ((bkWriteObjectBlockNote_t *)pnote)->hiWaterBlock;
 
}  /* end bkWriteObjectBlockDo */


/* PROGRAM: bk - return the areaname of the specified area
 *
 * RETURNS: 0 success
 *         -1 failure
 */
int
bkFindAreaName(
        dsmContext_t *pcontext,
        mstrblk_t	*pcontrolBlock,
	TEXT		*areaName,
	dsmArea_t	*pareaNumber,
	DBKEY  		*pdbkey,
        ULONG           *poutSize)
{
    CURSID         cursorId = NULLCURSID;

    svcDataType_t  component[2];
    svcDataType_t *pcomponent[2];
    COUNT          maxKeyLength = KEYSIZE;

    dsmStatus_t    returnCode = -1;
    ULONG          recordSize;
    ULONG          currentSize = *poutSize;
    TEXT          *prec = PNULL;

    svcByteString_t byteStringField;
    ULONG          indicator;

    AUTOKEY(areaKeyLow, KEYSIZE)
    AUTOKEY(areaKeyHi, KEYSIZE)

    if (currentSize == 0)
    {
        currentSize = (8 * sizeof(LONG)) + MAXPATHN +1;
    }
    prec = utmalloc(currentSize);

    if (*pareaNumber)
    {
        /* search by area number */
        areaKeyLow.akey.area         = DSMAREA_CONTROL;
        areaKeyHi.akey.area          = DSMAREA_CONTROL;
        areaKeyLow.akey.root         = pcontrolBlock->areaRoot;
        areaKeyHi.akey.root          = pcontrolBlock->areaRoot;
        areaKeyLow.akey.unique_index = 1;
        areaKeyHi.akey.unique_index  = 1;
        areaKeyLow.akey.word_index   = 0;
        areaKeyHi.akey.word_index    = 0;

        areaKeyLow.akey.index    = SCIDX_AREA;
        areaKeyLow.akey.keycomps = 1;
        areaKeyLow.akey.ksubstr  = 0;

        areaKeyHi.akey.index     = SCIDX_AREA;
        areaKeyHi.akey.keycomps  = 2;
        areaKeyHi.akey.ksubstr   = 0;

        pcomponent[0] = &component[0];
        pcomponent[1] = &component[1];

        component[0].type            = SVCLONG;
        component[0].indicator       = 0;
        component[0].f.svcLong       = (LONG)*pareaNumber;

        if ( keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                      &areaKeyLow.akey) )
            goto done;

        component[1].indicator       = SVCHIGHRANGE;

        if ( keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                      &areaKeyHi.akey) )
            goto done;

        /* attempt to get first index entry within specified bracket */
        returnCode = cxFind (pcontext,
                      &areaKeyLow.akey, &areaKeyHi.akey,
                      &cursorId, DSMAREA_CONTROL, (dsmRecid_t *)pdbkey,
                      DSMUNIQUE, DSMFINDFIRST, 0);
        if (returnCode == 0)
        {
          /* extract the area name from the area */
          recordSize = rmFetchRecord(pcontext, DSMAREA_CONTROL,
                                  *pdbkey, prec,
                                  (COUNT) currentSize,0);
          if ( recordSize > currentSize)
          {
                  *poutSize = recordSize;
                  returnCode = -1;
                  goto done;
          }
          else if (recordSize == (ULONG)DSM_S_RMNOTFND)
          {
              /* record not found */
              MSGN_CALLBACK(pcontext, bkMSG145, *pdbkey, DSMAREA_CONTROL);
              returnCode = -1;
              goto done;
          }

          if (areaName)
          {
            /* copy over the areaName */
            /* Extract the area Name */
            byteStringField.pbyte = areaName;
            byteStringField.size  = MAXPATHN +1;
            /* Extract the extent file name */
            if (recGetBYTES(prec, SCFLD_AREANAME, (ULONG)0,
                             &byteStringField, &indicator) )
            {
                returnCode = -1;
                goto done;
            }

          }
          returnCode = 0;
        }
    }

done:
    cxDeactivateCursor(pcontext, &cursorId);
    if (prec)
    {
        utfree(prec);
    }

    return returnCode;

}  /* end bkFindAreaName */

/* PROGRAM: bkExtentUnlink -  delete physical files that have previously
 *                            been marked for deletion
 *
 *
 * RETURNS: 0 on success
 *          -1 on any failure
 */
dsmStatus_t
bkExtentUnlink(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    ULONG            area;
    int		     rc;
    bkAreaDesc_t    *pbkArea;
    bkAreaDescPtr_t *pbkAreas;
    bkftbl_t        *ptbl = (bkftbl_t *)NULL;
    dbshm_t         *pdbpub = pdbcontext->pdbpub;

    pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
    for ( area = 0 ; area < pbkAreas->bkNumAreaSlots; area++ )
    {
        pbkArea = BK_GET_AREA_DESC(pdbcontext, area);
        if (pbkArea)
        {
            rc = bkGetAreaFtbl(pcontext, area, &ptbl);
       
            if (ptbl)
            {
	        for (; ptbl->ft_qname; ptbl++)
	        {  
                    if (ptbl->ft_delete)
                    {
                        /* close file and delete it */
                        bkioClose(pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[ptbl->ft_fdtblIndex], 0);
                        unlink((char *)XTEXT(pdbcontext, ptbl->ft_qname));
                    }
	        }
            }
            if (pbkArea->bkDelete)
            {
                stVacate(pcontext, XSTPOOL(pdbcontext, pdbpub->qdbpool),
                          (TEXT *)pbkArea);

                pbkAreas->bk_qAreaDesc[area] = 0;

                bmAreaFreeBuffers(pcontext, area);
            }
        }
    }
    return 0;
}

/* PROGRAM: bkSyncFiles - flush out all blocks associated with
 *                        the files of the database
 *
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkSyncFiles(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    bkAreaDesc_t    *pbkArea;
    bkAreaDescPtr_t *pbkAreas;
    bkftbl_t        *ptbl = (bkftbl_t *)NULL;
    ULONG            area;
    int		     rc;

    pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
    for ( area = 0 ; area < pbkAreas->bkNumAreaSlots; area++ )
    {
        pbkArea = BK_GET_AREA_DESC(pdbcontext, area);
        if (pbkArea)
        {
            rc = bkGetAreaFtbl(pcontext, area, &ptbl);
       
            if (ptbl)
            {
	        for (; ptbl->ft_qname; ptbl++)
	        {  
                    /* flush out the OS buffers for this file */
                    bkioFlush(pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[ptbl->ft_fdtblIndex]);
                }
            }
        }
    }
    return;
}


/* PROGRAM: bkExtentRename - rename an extent
 *
 * RETURNS: 0 if successful
 *          DB_EXBAD if error ecountered
 */
int
bkExtentRename(
	dsmContext_t	*pcontext,
	dsmArea_t	 areaNumber,
        dsmText_t	*pNewName) 
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    DBKEY areaExtentRoot = pdbcontext->pmstrblk->areaExtentRoot;
    CURSID   cursorId = NULLCURSID;
    TEXT *pName;

    dsmRecid_t extentRecordId;
    dsmStatus_t ret = -1;  /* initial to failure */
    dsmStatus_t returnCode = DB_EXBAD;  /* initial to failure */

    svcDataType_t component[ 3 /* BKCOMPMAX */ ];
    svcDataType_t *pcomponent[3 /* BKCOMPMAX */ ];
    COUNT         maxKeyLength = KEYSIZE;
    TEXT          storedFileName[MAXUTFLEN+1];

    TEXT  recordBuffer[(8 * sizeof(LONG)) + MAXUTFLEN +1];
    ULONG recordSize;
    svcByteString_t byteStringField;
    ULONG indicator;

    AUTOKEY(keyLow, KEYSIZE)
    AUTOKEY(keyHi,  KEYSIZE)

    /* set up keys to define bracket for scan of the extent table */
    keyLow.akey.area         = DSMAREA_CONTROL;
    keyHi.akey.area          = DSMAREA_CONTROL;
    keyLow.akey.root         = areaExtentRoot;
    keyHi.akey.root          = areaExtentRoot;
    keyLow.akey.unique_index = 1;
    keyHi.akey.unique_index  = 1;
    keyLow.akey.word_index   = 0;
    keyHi.akey.word_index    = 0;

    /* assign component values to high an low brackets */
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = (LONG)areaNumber;
 
    component[1].type            = SVCLONG;
    component[1].indicator       = 0;
    component[1].f.svcLong       = DSMEXTENT_MINIMUM;
 
    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];
 
    keyLow.akey.index    = SCIDX_EXTAREA;
    keyLow.akey.keycomps = 2;
    keyLow.akey.ksubstr  = 0;

   if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &keyLow.akey))
       goto done;

    component[0].f.svcLong       = (LONG)areaNumber;
    component[1].f.svcLong       = DSMEXTENT_MAXIMUM;
 
    keyHi.akey.index    = SCIDX_EXTAREA;
    keyHi.akey.keycomps = 2;
    keyHi.akey.ksubstr  = 0;

    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &keyHi.akey))
        goto done;

    ret = (dsmStatus_t)cxFind (pcontext,
                              &keyLow.akey, &keyHi.akey,
                              &cursorId, SCTBL_EXTENT, &extentRecordId,
                              DSMUNIQUE, DSMFINDFIRST, LKNOLOCK);

    while (ret == 0)
    {
        /* extract the area Extent reocrd information */
        recordSize = rmFetchRecord(pcontext, DSMAREA_CONTROL,
                                extentRecordId, recordBuffer,
                                sizeof(recordBuffer),0);
        if( recordSize > sizeof(recordBuffer))
        {
            MSGN_CALLBACK(pcontext,bkMSG142,recordSize);
            goto done;
        }

        byteStringField.pbyte = storedFileName;
        byteStringField.size  = sizeof(storedFileName);
        /* Extract the extent file name */
        if (recGetBYTES(recordBuffer, SCFLD_EXTPATH,(ULONG)0,
               &byteStringField, &indicator) || (indicator == SVCUNKNOWN) )
            goto done;

        byteStringField.pbyte = pNewName;
        byteStringField.length = strlen((const char *)pNewName);
        byteStringField.size = byteStringField.length;

        /* Store the new name field */
        if (recPutBYTES(recordBuffer, MAXRECSZ, &recordSize, SCFLD_EXTPATH,0,
                        &byteStringField, 0))
            goto done;

        /* Write the record. */
        if (rmUpdateRecord(pcontext, SCTBL_EXTENT,
                           DSMAREA_CONTROL, extentRecordId,
                           recordBuffer, recordSize))
            goto done;

        {
          bkftbl_t *ptbl;

          bkGetAreaFtbl(pcontext, areaNumber, &ptbl);
          if (ptbl->ft_qname)
          {
              stVacate(pcontext, XSTPOOL(pdbcontext, pdbpub->qdbpool),
                       XTEXT(pdbcontext, ptbl->ft_qname));
          }
          pName = (TEXT *)stRentIt(pcontext,
                       XSTPOOL(pdbcontext, pdbpub->qdbpool),
                       stlen(pNewName) + 1, 0);
          ptbl->ft_qname = P_TO_QP(pcontext, pName);
          stcopy(pName, pNewName);
        }

        ret = (dsmStatus_t)cxNext(pcontext,
                                 &cursorId, SCTBL_EXTENT, &extentRecordId,
                                 &keyLow.akey, &keyHi.akey,
                                 DSMGETTAG, DSMFINDNEXT, LKNOLOCK);
    }

    if(ret == DSM_S_ENDLOOP)
        returnCode = 0;

done:
    cxDeactivateCursor(pcontext, &cursorId);
    return returnCode;

}  /* end bkExtentRename */

/* PROGRAM: bkModifyArea - Mark the area as open
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkModifyArea (dsmContext_t *pcontext, ULONG area)
{
    dbcontext_t        *pdbcontext = pcontext->pdbcontext;
    bkAreaDescPtr_t    *pbkAreaDescPtr;
    bkAreaDesc_t       *pbkAreaDesc;

    objectBlock_t   *pobjectBlock;
    bmBufHandle_t    bufHandle;
    bktbl_t         *pbktbl;

    if (area < DSMAREA_DEFAULT)
    {
        /* only update user areas */
        return;
    }
    /* get the area descriptor pointers */
    pbkAreaDescPtr = XBKAREADP(pdbcontext, 
                            pdbcontext->pbkctl->bk_qbkAreaDescPtr);

    /* get the area descriptor for the passed area */
    pbkAreaDesc = XBKAREAD(pdbcontext, pbkAreaDescPtr->bk_qAreaDesc[area]);
    if (pbkAreaDesc == (bkAreaDesc_t *)NULL)
    {
        /* no such area */
        return;
    }

    /* check to see if the area has already been opened */
    if (pbkAreaDesc->bkStatus == DSM_OBJECT_OPEN)
    {
        /* already open */
        return;
    }

    /* mark the area as open */
    pbkAreaDesc->bkStatus = (COUNT)DSM_OBJECT_OPEN;

    /* Update the object block for the area to reflect it's status */
    bufHandle = bmLocateBlock(pcontext, area,
                              BLK2DBKEY(pbkAreaDesc->bkAreaRecbits), TOMODIFY);
    pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext, bufHandle);

    pobjectBlock->objStatus = DSM_OBJECT_OPEN;

    pbktbl = XBKTBL(pdbcontext, bufHandle);
    bkWrite(pcontext, pbktbl, BUFFERED);
    
    bmReleaseBuffer(pcontext, bufHandle);

    return;
}

/* PROGRAM: bkCLoseAreas - Mark all the areas as closed
 *
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkCloseAreas (dsmContext_t *pcontext)
{
    dbcontext_t        *pdbcontext = pcontext->pdbcontext;
    bkAreaDescPtr_t    *pbkAreaDescPtr;
    bkAreaDesc_t       *pbkAreaDesc;

    objectBlock_t   *pobjectBlock;
    bmBufHandle_t    bufHandle;
    ULONG             area;

    /* get the area descriptor pointers */
    pbkAreaDescPtr = XBKAREADP(pdbcontext, 
                            pdbcontext->pbkctl->bk_qbkAreaDescPtr);

    for (area = DSMAREA_DEFAULT; area < pbkAreaDescPtr->bkNumAreaSlots; area++)
    {
        /* get the area descriptor for the passed area */
        pbkAreaDesc = XBKAREAD(pdbcontext, pbkAreaDescPtr->bk_qAreaDesc[area]);
        if (pbkAreaDesc == (bkAreaDesc_t *)NULL)
            continue;

        if (pbkAreaDesc->bkHiWaterBlock < 3)
            continue;

        /* Update the object block for the area to reflect it's status */
        bufHandle = bmLocateBlock(pcontext, area,
                              BLK2DBKEY(pbkAreaDesc->bkAreaRecbits), TOMODIFY);
        pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext, bufHandle);

        pobjectBlock->objStatus = DSM_OBJECT_CLOSED;
        bmMarkModified(pcontext, bufHandle, 0);

        bmReleaseBuffer(pcontext, bufHandle);
        
    }
}
#if NUXI==0  /* Big endian platforms   */
/* PROGRAM: bkToStore - Convert block headers from hardware independent
   form to hardware dependent form.
*/
DSMVOID bkToStore(bkhdr_t *pblk)
{
  LONG64 temp;
  
  temp = (LONG64)pblk->bk_dbkey;
  DBKEY_TO_STORE((TEXT *)&pblk->bk_dbkey,temp);
  temp = (LONG64)pblk->bk_incr;
  COUNT_TO_STORE((TEXT *)&pblk->bk_incr,temp);
  temp = (LONG64)pblk->bk_nextf;
  DBKEY_TO_STORE((TEXT *)&pblk->bk_nextf,temp);
  temp = (LONG64)pblk->bk_checkSum;
  LONG_TO_STORE((TEXT *)&pblk->bk_checkSum,temp);
  temp = pblk->bk_updctr;
  LONG64_TO_STORE((TEXT *)&pblk->bk_updctr,temp);

  if(pblk->bk_type == MASTERBLK)
    bkMasterBlockToStore(pblk);
  else if (pblk->bk_type == IDXBLK)
    bkIndexBlockToStore(pblk);
  else if (pblk->bk_type == RMBLK)
    bkRecordBlockToStore(pblk);
  else if (pblk->bk_type == IXANCHORBLK)
    bkIndexAnchorBlockToStore(pblk);
  else if (pblk->bk_type == OBJECTBLK)
    bkObjectBlockToStore(pblk);
  else if (pblk->bk_type == EXTHDRBLK)
    bkExtentBlockToStore(pblk);

  return;
}

/* PROGRAM: bkMasterBlockToStore - Convert master block to storage
   format
*/
LOCALF
DSMVOID bkMasterBlockToStore(bkhdr_t *pblk)
{
  mstrblk_t   *pmstr = (mstrblk_t *)pblk;
  LONG64       temp;

  temp = pmstr->mb_dbvers;
  LONG_TO_STORE((TEXT *)&pmstr->mb_dbvers, temp);
  temp = pmstr->mb_blockSize;
  LONG_TO_STORE((TEXT *)&pmstr->mb_blockSize, temp);
  temp = pmstr->mb_dbstate;
  COUNT_TO_STORE((TEXT *)&pmstr->mb_dbstate, temp);
  temp = pmstr->mb_lasttask;
  LONG_TO_STORE((TEXT *)&pmstr->mb_lasttask, temp);
  temp = pmstr->mb_lstmod;
  LONG_TO_STORE((TEXT *)&pmstr->mb_lstmod, temp);
  temp = pmstr->mb_biopen;
  LONG_TO_STORE((TEXT *)&pmstr->mb_biopen, temp);
  temp = pmstr->mb_biprev;
  LONG_TO_STORE((TEXT *)&pmstr->mb_biprev, temp);
  temp = pmstr->mb_rltime;
  LONG_TO_STORE((TEXT *)&pmstr->mb_rltime,temp);
  temp = pmstr->mb_rlclsize;
  COUNT_TO_STORE((TEXT *)&pmstr->mb_rlclsize, temp);
  temp = pmstr->mb_biblksize;
  COUNT_TO_STORE((TEXT *)&pmstr->mb_biblksize,temp);
  temp = pmstr->mb_shutdown;
  LONG_TO_STORE((TEXT *)&pmstr->mb_shutdown, temp);
  temp = pmstr->mb_shmid;
  LONG_TO_STORE((TEXT *)&pmstr->mb_shmid,temp);
  temp = pmstr->mb_semid;
  LONG_TO_STORE((TEXT *)&pmstr->mb_semid,temp);
  temp = pmstr->mb_objectRoot;
  DBKEY_TO_STORE((TEXT *)&pmstr->mb_objectRoot,temp);
  temp = pmstr->areaRoot;
  DBKEY_TO_STORE((TEXT *)&pmstr->areaRoot,temp);
  temp = pmstr->areaExtentRoot;
  DBKEY_TO_STORE((TEXT *)&pmstr->areaExtentRoot,temp);

  return;
}
  
/* PROGRAM: bkIndexBlockToStore - Convert index block to storage
   format
*/
LOCALF
DSMVOID bkIndexBlockToStore(bkhdr_t *pblk)
{
  cxblock_t   *p = (cxblock_t *)pblk;
  LONG64       temp;
  
  temp = p->ix_hdr.ih_ixnum;
  COUNT_TO_STORE((TEXT *)&p->ix_hdr.ih_ixnum,temp);
  temp = p->ix_hdr.ih_nment;
  COUNT_TO_STORE((TEXT *)&p->ix_hdr.ih_nment,temp);
  temp = p->ix_hdr.ih_lnent;
  COUNT_TO_STORE((TEXT *)&p->ix_hdr.ih_lnent,temp);

  return;
}

/* PROGRAM: bkRecordBlockToStore - Record index block to storage
   format
*/
LOCALF
DSMVOID bkRecordBlockToStore(bkhdr_t *pblk)
{
  rmBlock_t   *p = (rmBlock_t *)pblk;
  LONG64       temp;
  int          i;


  temp = p->freedir;
  COUNT_TO_STORE((TEXT *)&p->freedir,temp);
  temp = p->free;
  LONG_TO_STORE((TEXT *)&p->free,temp);
  for( i = 0; i < p->numdir; i++)
  {
    temp = p->dir[i];
    LONG_TO_STORE((TEXT *)&p->dir[i],temp);
  }
  temp = p->numdir;
  COUNT_TO_STORE((TEXT *)&p->numdir,temp);

  return;
}

/* PROGRAM: bkIndexAnchorBlockToStore - 
   format
*/
LOCALF
DSMVOID bkIndexAnchorBlockToStore(bkhdr_t *pblk)
{
  ixAnchorBlock_t   *p = (ixAnchorBlock_t *)pblk;
  LONG64       temp;
  int          i;

  for(i = 0; i < IX_INDEX_MAX; i++)
  {
    temp = p->ixRoots[i].ixRoot;
    DBKEY_TO_STORE((TEXT *)&p->ixRoots[i].ixRoot,temp);
  }
  
  return;
}


/* PROGRAM: bkObjectBlockToStore - 
   format
*/
LOCALF
DSMVOID bkObjectBlockToStore(bkhdr_t *pblk)
{
  objectBlock_t   *p = (objectBlock_t *)pblk;
  LONG64       temp;
  int          i;

  temp = p->objVersion;
  LONG_TO_STORE((TEXT *)&p->objVersion, temp);
  temp = p->objStatus;
  LONG_TO_STORE((TEXT *)&p->objStatus, temp);
  temp = p->totalBlocks;
  LONG_TO_STORE((TEXT *)&p->totalBlocks,temp);
  temp = p->hiWaterBlock;
  LONG_TO_STORE((TEXT *)&p->hiWaterBlock,temp);
  for(i = 0; i < 3; i++)
  {
    temp = p->chainFirst[i];
    DBKEY_TO_STORE((TEXT *)&p->chainFirst[i],temp);
    temp = p->numBlocksOnChain[i];
    LONG_TO_STORE((TEXT *)&p->numBlocksOnChain[i],temp);
    temp = p->chainLast[i];
    DBKEY_TO_STORE((TEXT *)&p->chainLast[i],temp);
  }
  temp = p->totalRows;
  LONG64_TO_STORE((TEXT *)&p->totalRows,temp);
  temp = p->autoIncrement;
  LONG64_TO_STORE((TEXT *)&p->autoIncrement,temp);
  temp = p->statisticsRoot;
  DBKEY_TO_STORE((TEXT *)&p->statisticsRoot,temp);
		  
  return;
}

/* PROGRAM: bkExtentBlockToStore - 
   format
*/
LOCALF
DSMVOID bkExtentBlockToStore(bkhdr_t *pblk)
{
  LONG64 temp;
  int    i;
  extentBlock_t  *p = (extentBlock_t *)pblk;

  temp = p->dbVersion;
  LONG_TO_STORE((TEXT *)&p->dbVersion,temp);
  temp = p->blockSize;
  LONG_TO_STORE((TEXT *)&p->blockSize,temp);
  temp = p->extentType;
  COUNT_TO_STORE((TEXT *)&p->extentType,temp);
  temp = p->dateVerification;
  COUNT_TO_STORE((TEXT *)&p->dateVerification,temp);
  for(i = 0; i < 2; i++)
  {
    temp = p->creationDate[i];
    LONG_TO_STORE((TEXT *)&p->creationDate[i],temp);
    temp = p->lastOpenDate[i];
    LONG_TO_STORE((TEXT *)&p->lastOpenDate[i],temp);
  }
  
  return;
}

/* PROGRAM: bkFromStore - Convert block headers from hardware independent
   form to hardware dependent form.
*/
DSMVOID bkFromStore(bkhdr_t *pblk)
{
  LONG64  temp;

  temp = STORE_TO_DBKEY((TEXT *)&pblk->bk_dbkey);
  pblk->bk_dbkey = temp;
  temp = STORE_TO_COUNT((TEXT *)&pblk->bk_incr);
  pblk->bk_incr = temp;
  temp = STORE_TO_DBKEY((TEXT *)&pblk->bk_nextf);
  pblk->bk_nextf = temp;
  temp = STORE_TO_LONG((TEXT *)&pblk->bk_checkSum);
  pblk->bk_checkSum = temp;
  temp = STORE_TO_LONG64((TEXT *)&pblk->bk_updctr);
  pblk->bk_updctr = temp;

  if(pblk->bk_type == MASTERBLK)
    bkMasterBlockFromStore(pblk);
  else if (pblk->bk_type == IDXBLK)
    bkIndexBlockFromStore(pblk);
  else if (pblk->bk_type == RMBLK)
    bkRecordBlockFromStore(pblk);
  else if (pblk->bk_type == IXANCHORBLK)
    bkIndexAnchorBlockFromStore(pblk);
  else if (pblk->bk_type == OBJECTBLK)
    bkObjectBlockFromStore(pblk);
  else if( pblk->bk_type == EXTHDRBLK)
    bkExtentBlockFromStore(pblk);
	   

  return;
}

/* PROGRAM: bkMasterBlockFromStore - 
*/
LOCALF
DSMVOID bkMasterBlockFromStore(bkhdr_t *pblk)
{
  mstrblk_t   *pmstr = (mstrblk_t *)pblk;
  LONG64       temp;


  temp = STORE_TO_LONG((TEXT *)&pmstr->mb_dbvers);
  pmstr->mb_dbvers = temp;
  temp = STORE_TO_LONG((TEXT *)&pmstr->mb_blockSize);
  pmstr->mb_blockSize = temp;
  temp = STORE_TO_COUNT((TEXT *)&pmstr->mb_dbstate);
  pmstr->mb_dbstate = temp;
  temp = STORE_TO_LONG((TEXT *)&pmstr->mb_lasttask);
  pmstr->mb_lasttask = temp;
  temp = STORE_TO_LONG((TEXT *)&pmstr->mb_lstmod);
  pmstr->mb_lstmod = temp;
  temp = STORE_TO_LONG((TEXT *)&pmstr->mb_biopen);
  pmstr->mb_biopen = temp;
  temp = STORE_TO_LONG((TEXT *)&pmstr->mb_biprev);
  pmstr->mb_biprev = temp;
  temp = STORE_TO_LONG((TEXT *)&pmstr->mb_rltime);
  pmstr->mb_rltime = temp;
  temp = STORE_TO_COUNT((TEXT *)&pmstr->mb_rlclsize);
  pmstr->mb_rlclsize = temp;
  temp = STORE_TO_COUNT((TEXT *)&pmstr->mb_biblksize);
  pmstr->mb_biblksize = temp;
  temp =STORE_TO_LONG((TEXT *)&pmstr->mb_shutdown);
  pmstr->mb_shutdown = temp;
  temp = STORE_TO_LONG((TEXT *)&pmstr->mb_shmid);
  pmstr->mb_shmid = temp;
  temp = STORE_TO_LONG((TEXT *)&pmstr->mb_semid);
  pmstr->mb_semid = temp;
  temp = STORE_TO_DBKEY((TEXT *)&pmstr->mb_objectRoot);
  pmstr->mb_objectRoot = temp;
  temp = STORE_TO_DBKEY((TEXT *)&pmstr->areaRoot);
  pmstr->areaRoot = temp;
  temp = STORE_TO_DBKEY((TEXT *)&pmstr->areaExtentRoot);
  pmstr->areaExtentRoot = temp;

  return;
}
  
/* PROGRAM: bkIndexBlockFromStore - 
*/
LOCALF
DSMVOID bkIndexBlockFromStore(bkhdr_t *pblk)
{
  cxblock_t   *p = (cxblock_t *)pblk;
  LONG64       temp;
  
  temp = STORE_TO_COUNT((TEXT *)&p->ix_hdr.ih_ixnum);
  p->ix_hdr.ih_ixnum = temp;
  temp = STORE_TO_COUNT((TEXT *)&p->ix_hdr.ih_nment);
  p->ix_hdr.ih_nment =  temp;
  temp = STORE_TO_COUNT((TEXT *)&p->ix_hdr.ih_lnent);
  p->ix_hdr.ih_lnent = temp;

  return;
}

/* PROGRAM: bkRecordBlockFromStore - 
*/
LOCALF
DSMVOID bkRecordBlockFromStore(bkhdr_t *pblk)
{
  rmBlock_t   *p = (rmBlock_t *)pblk;
  LONG64       temp;
  int          i;

  temp = STORE_TO_COUNT((TEXT *)&p->numdir);
  p->numdir = temp;
  temp = STORE_TO_COUNT((TEXT *)&p->freedir);
  p->freedir = temp;
  temp = STORE_TO_LONG((TEXT *)&p->free);
  p->free = temp;
  for( i = 0; i < p->numdir; i++)
  {
    temp = STORE_TO_LONG((TEXT *)&p->dir[i]);
    p->dir[i] = temp;
  }
  return;
}

/* PROGRAM: bkIndexAnchorBlockFromStore - 
   format
*/
LOCALF
DSMVOID bkIndexAnchorBlockFromStore(bkhdr_t *pblk)
{
  ixAnchorBlock_t   *p = (ixAnchorBlock_t *)pblk;
  LONG64       temp;
  int          i;

  for(i = 0; i < IX_INDEX_MAX; i++)
  {
    temp = STORE_TO_DBKEY((TEXT *)&p->ixRoots[i].ixRoot);
    p->ixRoots[i].ixRoot = temp;
  }
  
  return;
}


/* PROGRAM: bkObjectBlockFromStore - 
   format
*/
LOCALF
DSMVOID bkObjectBlockFromStore(bkhdr_t *pblk)
{
  objectBlock_t   *p = (objectBlock_t *)pblk;
  LONG64       temp;
  int          i;

  temp = STORE_TO_LONG((TEXT *)&p->objVersion);
  p->objVersion = temp;
  temp = STORE_TO_LONG((TEXT *)&p->objStatus);
  p->objStatus = temp;
  temp = STORE_TO_LONG((TEXT *)&p->totalBlocks);
  p->totalBlocks = temp;
  temp = STORE_TO_LONG((TEXT *)&p->hiWaterBlock);
  p->hiWaterBlock = temp;
  for(i = 0; i < 3; i++)
  {
    temp = STORE_TO_DBKEY((TEXT *)&p->chainFirst[i]);
    p->chainFirst[i] = temp;
    temp = STORE_TO_LONG((TEXT *)&p->numBlocksOnChain[i]);
    p->numBlocksOnChain[i] = temp;
    temp = STORE_TO_DBKEY((TEXT *)&p->chainLast[i]);
    p->chainLast[i] = temp;
  }
  temp = STORE_TO_LONG64((TEXT *)&p->totalRows);
  p->totalRows = temp;
  temp = STORE_TO_LONG64((TEXT *)&p->autoIncrement);
  p->autoIncrement = temp;
  temp = STORE_TO_DBKEY((TEXT *)&p->statisticsRoot);
  p->statisticsRoot = temp;
		  
  return;
}

/* PROGRAM: bkExtentBlockFromStore - 
   format
*/
LOCALF
DSMVOID bkExtentBlockFromStore(bkhdr_t *pblk)
{
  int       i;
  ULONG64   temp;
  extentBlock_t *p = (extentBlock_t *)pblk;

  temp = STORE_TO_LONG((TEXT *)&p->dbVersion);
  p->dbVersion = temp;
  temp = STORE_TO_LONG((TEXT *)&p->blockSize);
  p->blockSize = temp;
  temp = STORE_TO_COUNT((TEXT *)&p->extentType);
  p->extentType = temp;
  temp = STORE_TO_COUNT((TEXT *)&p->dateVerification);
  p->dateVerification = temp;
  for ( i = 0; i < 2; i++)
  {
    temp = STORE_TO_LONG((TEXT *)&p->creationDate[i]);
    p->creationDate[i] = temp;
    temp = STORE_TO_LONG((TEXT *)&p->lastOpenDate[i]);
    p->lastOpenDate[i] = temp;
  }
      
  return;
}
#endif /* #if NUXI==0 */
