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
#include "bkpub.h"
#include "dbcontext.h"
#include "drmsg.h"
#include "dsmpub.h"
#include "usrctl.h"
#include "mstrblk.h"
#include "utfpub.h"  /* for utapath() */

#include <errno.h>

#if OPSYS==UNIX
#include <unistd.h>    /* needed for lseek and read calls */
#endif

#if !NW386
#if BKCHK
/* PROGRAM: bkverify - verify critical bk layer db & bi information
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkverify (
	TEXT	*whocalled,		/* name of routine who called */
	dbcontext_t	*pdbcontext)	/* current db */
{
		mstrblk_t mblock;
		TEXT	  dbname[MAXUTFLEN + 1];
		COUNT	  dbcnt;
		COUNT	  bicnt;
		LONG	  ret;
		bkftbl_t *pbkftbl;
    dsmContext_t context;
    stnclr((TEXT *)&context, sizeof(dsmContext_t) );

    if (!pdbcontext) 
	MSGD ("bkverify: pdbcontext not allocated");
    else
    {
	MSGD ("%s:", whocalled);
	utapath(dbname, sizeof(dbname), pdbcontext->pdbname, (TEXT *) ".db");
	MSGD ("\tdbname: %s", dbname);

	/* display info from fd look-aside table */
	bkdfdtbl (pdbcontext->pbkfdtbl);

	/* open .db & read master block */
        DSMSETCONTEXT(&context, p_dbctl, pusrctl);
	if (bkReadMasterBlock(&context, dbname, &mblock, -1) != BK_RDMB_SUCCESS) 
	    MSGD ("bkverify: bkrbmb on dbname=%s failed", dbname);
	else if (!pdbcontext->pbkctl) 
	    MSGD ("bkverify: pbkctl not allocated");
	else
	{
	    dbcnt = (mblock.mb_dbcnt > 0 ? mblock.mb_dbcnt : 1);
	    bicnt = (mblock.mb_bicnt > 0 ? mblock.mb_bicnt : 1);

	    /* display bk file extent table info */

            ret = bkGetAreaFtbl(BKDATA,&pbkftbl);
	    bkdftbl (pbkftbl, BKDATA, dbcnt, pdbcontext->pmstrblk);

            ret = bkGetAreaFtbl(DSMAREA_BI,&pbkftbl);
	    bkdftbl (pbkftbl, BKBI, bicnt, pdbcontext->pmstrblk);

            ret = bkGetAreaFtbl(BKAI,&pbkftbl);
	    bkdftbl (pbkftbl, BKAI, 1, pdbcontext->pmstrblk);

	    /* run tests on bk file extent table info */

            ret = bkGetAreaFtbl(BKDATA,&pbkftbl);
	    bkvftbl (pbkftbl, BKDATA, dbcnt, pdbcontext->pmstrblk);

            ret = bkGetAreaFtbl(DSMAREA_BI,&pbkftbl);
	    bkvftbl (pbkftbl, BKBI, bicnt, pdbcontext->pmstrblk);

            ret = bkGetAreaFtbl(&context, BKAI,&pbkftbl);
	    bkvftbl (pbkftbl, BKAI, 1, pdbcontext->pmstrblk);
	}
    }
}  /* end bkverify */
#endif /* BKCHK */
#endif /* !NW386 */


#if BKCHK
/* PROGRAM: bkdfdtbl - display fd look-aside table
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkdfdtbl (BKFDTBL *pbkfdtbl)	/* fd look-aside table */
{
    int	 i;

    if (!pbkfdtbl) 
	MSGD ("bkdfdtbl: pbkfdtbl not allocated");
    else
    {
	MSGD ("\tbkfdtbl:");
	for (i = 0; i < pbkfdtbl->numused; i++) 
	    MSGD ("\t\tfd[%d]: %d", i,pbkfdtbl->fd[i]);
    }

}  /* end bkdfdtbl */


/* PROGRAM: bkdftbl - display file extent table
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkdftbl(
	bkftbl_t  *pbkftbl,	/* file extent table */
 	int	   ftype,	/* file type, BKDATA, BKBI or BKAI */
	COUNT	   extents,	/* number of data extents */
	mstrblk_t *pmstr)	/* master block info */
{
TEXT	 *pmsg;
bkftbl_t *ptmp;

    pmsg = "bkftbl";
    bkdftype (ftype, pmsg);
    if (!pbkftbl) 
	MSGD ("bkdftbl: pbkftbl not allocated");
    else
    {
	for (ptmp = pbkftbl; extents > 0; ptmp++, extents--) 
	{
	    MSGD ("\tft_qname: %s", QP_TO_P(p_dbctl, ptmp->ft_qname));
	    MSGD ("\t\tft_curlen: %l", ptmp->ft_curlen);
	    MSGD ("\t\tft_offset: %l", ptmp->ft_offset);
	    MSGD ("\t\tft_xtndoff: %l", ptmp->ft_xtndoff);
	    MSGD ("\t\tft_qrawio: %s", QP_TO_P(p_dbctl, ptmp->ft_qrawio));
	    MSGD ("\t\tft_fdtblIndex: %d", ptmp->ft_fdtblIndex);
	    MSGD ("\t\tft_syncd: %d", ptmp->ft_syncd);
	    MSGD ("\t\tft_xtndamt: %d", ptmp->ft_xtndamt);
	    MSGD ("\t\tft_type: %b", ptmp->ft_type);

	    if (ptmp->ft_type & BKDATA) 
		MSGD ("\t\t\tfile: BKDATA");
	    if (ptmp->ft_type & BKBI) 
		MSGD ("\t\t\tfile: BKBI");
	    if (ptmp->ft_type & BKAI) 
		MSGD ("\t\t\tfile: BKAI");
	    if (ptmp->ft_type & BKFIXED) 
		MSGD ("\t\t\tfile: BKFIXED");
	    if (!(ptmp->ft_type & BKFIXED)) 
		MSGD ("\t\t\tfile: VARIABLE");

	    MSGD ("\t\tft_syncio: %t", ptmp->ft_syncio);
	}
    }

}  /* end bkdftbl */


/* PROGRAM: bkvftbl - run tests on bk file extent table
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkvftbl(
	bkftbl_t  *pbkftbl,	/* file extent table */
	int	   ftype,	/* file type, BKDATA, BKBI or BKAI */
	COUNT	   extents,	/* number of data extents */
	mstrblk_t *pmstr)	/* master block info */
{
TEXT	 *pmsg;
bkftbl_t *curpbkftbl;
int	  curextent;

    MSGD (" ");
    pmsg = "bkftbl";
    bkdftype (ftype, pmsg);
    if (!pbkftbl) 
	MSGD ("bkvftbl: pbkftbl not allocated");
    else
    {
	for (curextent = 1, curpbkftbl = pbkftbl;
		curextent <= extents; curextent++, curpbkftbl++) 
	{
	    MSGD (" ");
	    MSGD ("\tft_qname: %s", QP_TO_P(p_dbctl, curpbkftbl->ft_qname));
	    bkvnormf (curpbkftbl, ftype, extents, curextent, pmstr);
	    bkvsyncf (curpbkftbl, ftype, extents, curextent, pmstr);
	}
    }

}  /* end bkvftbl */


/* PROGRAM: bkvnormf - run tests on normal fd
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkvnormf(
	bkftbl_t  *pbkftbl,	/* file extent table */
	int	   ftype,	/* file type, BKDATA, BKBI or BKAI */
	COUNT	   extents,	/* number of data extents */
	COUNT	   curextent,	/* current data extent */
	mstrblk_t *pmstr)	/* master block info */
{
    /* due to inconsistent methods of handling sync/raw I/O in the
       bk layer, for now this test NEEDS to be a conditional compile
    */
#if OSYNCFLG
    		TEXT	*pmsg;
		int	 idx;		/* fd table index */
		fileHandle_t fd;

    idx = pbkftbl->ft_fdtblIndex;
    MSGD ("\t\tnormal fd table index is: %d", idx);
    if (idx < 0 ) 
    {
	MSGD ("\t\tno index to a normal fd!");
	MSGD ("\t\tOSYNC: this should never happen");
	pmsg = "FAILED";
	MSGD ("\t\t...%s...", pmsg);
    }
    else
    {
	fd = bkioGetFd(pcontext, p_dbctl->pbkfdtbl->pbkiotbl[idx]);
	MSGD ("\t\t\tnormal fd is: %d", fd);

	/* test length of normal fd */
	bkvfdlen (pbkftbl->ft_curlen + pbkftbl->ft_offset, fd);

	/* now check the file descriptors */
	switch (ftype) 
	{
	    case BKDATA:
		MSGD ("\t\t\tfile type is: BKDATA");
		bkvfiof (REGIO, pbkftbl->ft_syncio);
		bkvfdio (REGIO, fd);
		break;
	    case BKBI:
		MSGD ("\t\t\tfile type is: BKBI");
		bkvfiof ((p_dbpub->useraw ? SWRTIO : REGIO),
			pbkftbl->ft_syncio);
		bkvfdio ((p_dbpub->useraw ? SWRTIO : REGIO), fd);
		break;
	    case BKAI:
		MSGD ("\t\t\tfile type is: BKAI");
		if (pmstr->mb_aisync == UNBUFFERED) 
		{
		    MSGD ("\t\t\tai is UNBUFFERED");
		    bkvfiof (SWRTIO, pbkftbl->ft_syncio);
		    bkvfdio (SWRTIO, fd);
		}
		else
		{
		    MSGD ("\t\t\tai is BUFFERED");
		    bkvfiof (REGIO, pbkftbl->ft_syncio);
		    bkvfdio (REGIO, fd);
		}
		break;
	    default:
		bkdftype (ftype, "bkvnormf: FAILED");
		break;
	}
    }
#endif /* OSYNCFLG */
}  /* end bkvnormf */


/* PROGRAM: bkvsyncf - run tests on synced fd
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkvsyncf(
	bkftbl_t  *pbkftbl,	/* file extent table */
	int	   ftype,	/* file type, BKDATA, BKBI or BKAI */
	COUNT	   extents,	/* number of data extents */
	COUNT	   curextent,	/* current data extent */
	mstrblk_t *pmstr)	/* master block info */
{
    /* due to inconsistent methods of handling sync/raw I/O in the
       bk layer, for now this test NEEDS to be a conditional compile
    */
#if OSYNCFLG
		TEXT	*pmsg;
		int	 idx;		/* fd table index */
		fileHandle_t fd;

    idx = pbkftbl->ft_syncd;
    MSGD ("\t\tsync fd table index is: %d", idx);
    if (idx < 0 ) 
    {
	MSGD ("\t\twhen is this ok?");
	switch (ftype) 
	{
	    case BKDATA:
		MSGD ("\t\t\tfile type is: BKDATA");
		if (p_dbpub->useraw
			&& (curextent == 1 /* first extent (master blk) */
			    || curextent == extents)) /* last extent */
		{
		    MSGD ("\t\t\twe are UNBUFFERED");
		    MSGD ("\t\t\tAND this is first or last extent");
		    pmsg = "FAILED";
		}
		else
		{
		    MSGD ("\t\t\twe are BUFFERED");
		    MSGD ("\t\t\tOR this is NOT first or last extent");
		    pmsg = "SUCCEEDED";
		}
		MSGD ("\t\t...%s...", pmsg);
		break;
	    case BKBI:
		MSGD ("\t\t\tfile type is: BKBI");
		MSGD ("\t\t\tcurrently all bi extents have ft_syncd = -1");
		MSGD ("\t\t\tft_fdtblIndex has real synced fd index");
		pmsg = "SUCCEEDED";
		MSGD ("\t\t...%s...", pmsg);
		break;
	    case BKAI:
		MSGD ("\t\t\tfile type is: BKAI");
		MSGD ("\t\t\tcurrently ai has ft_syncd = -1");
		MSGD ("\t\t\tft_fdtblIndex has real synced fd index");
		pmsg = "SUCCEEDED";
		MSGD ("\t\t...%s...", pmsg);
		break;
	    default:
		bkdftype (ftype, "bkvsyncf: FAILED");
		break;
	}
    }
    else
    {
	fd = bkioGetFd(pcontext, p_dbctl->pbkfdtbl->pbkiotbl[idx]);
	MSGD ("\t\t\tsync fd is: %d", fd);

	/* test length of sync fd */
	bkvfdlen (pbkftbl->ft_curlen + pbkftbl->ft_offset, fd);

	/* now check the file descriptors */
	switch (ftype) 
	{
	    case BKDATA:
		MSGD ("\t\t\tfile type is: BKDATA");
		if (curextent == 1 /* first extent (master blk) */
			|| curextent == extents) /* last extent */
		{
		    MSGD ("\t\t\tthis is first or last extent");
		    bkvfdio ((p_dbpub->useraw ? SWRTIO : REGIO), fd);
		}
		else
		{
		    MSGD ("\t\t\tthis is NOT first or last extent");
		    pmsg = "FAILED";
		    MSGD ("\t\t...%s...", pmsg);
		}
		break;
	    case BKBI:
	    case BKAI:
	    default:
		bkdftype (ftype, "bkvsyncf: FAILED");
		break;
	}
    }
#endif /* OSYNCFLG */
}  /* end bkvsyncf */


/* PROGRAM: bkvfiof - verify file io flag
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkvfiof (
	int	 wantio,	/* REGIO, RAWIO or SWRTIO */ 
	TEXT	 haveio)
{
    /* due to inconsistent methods of handling sync/raw I/O in the
       bk layer, for now this test NEEDS to be a conditional compile
    */
#if OSYNCFLG
	TEXT	*pmsg;

    switch (wantio) 
    {
	case REGIO:
	    MSGD ("\t\t\twe want REGIO");
	    break;
	case SWRTIO:
	    MSGD ("\t\t\twe want SWRTIO");
	    break;
	case RAWIO:
	    MSGD ("\t\t\twe want RAWIO");
	    MSGD ("\t\t\tOSYNC: this should never happen");
	    MSGD ("bkvfiof: FAILED");
	    break;
	default:
	    MSGD ("bkvfiof: io unknown: FAILED");
	    break;
    }

    switch (haveio) 
    {
	case REGIO:
	    MSGD ("\t\t\twe have (ft_syncio) REGIO");
	    break;
	case SWRTIO:
	    MSGD ("\t\t\twe have (ft_syncio) SWRTIO");
	    break;
	case RAWIO:
	    MSGD ("\t\t\twe have (ft_syncio) RAWIO");
	    MSGD ("\t\t\tOSYNC: this should never happen");
	    MSGD ("bkvfiof: FAILED");
	    break;
	default:
	    MSGD ("bkvfiof: io unknown: FAILED");
	    break;
    }

    if (wantio == (int) haveio) 
	pmsg = "SUCCEEDED";
    else
	pmsg = "FAILED";
    MSGD ("\t\t...%s...", pmsg);
#endif /* OSYNCFLG */
}  /* end bkvfiof */


/* PROGRAM: bkvfdlen - run file length test
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkvfdlen (gem_off_t len,	/* in memory file length */
	  int       fd)		/* stat this file */
{
    TEXT *pmsg;
#if OPSYS==WIN32API && defined(DBUT_LARGEFILE)
    struct _stati64 statbuf;
#else
    struct stat statbuf;
#endif

    MSGD ("\t\t\twe want length: %l", len);
#if OPSYS==WIN32API && defined(DBUT_LARGEFILE)
    if (_fstati64(fd, &statbuf)) 
#else
    if (fstat (fd, &statbuf)) 
#endif
    {
        MSGD ("bkvfdlen: fstat on fd=%d failed with errno=%d", fd, errno);
    }
    else
    {
        MSGD ("\t\t\tactual length is: %l", statbuf.st_size);
        if (len == statbuf.st_size) 
            pmsg = "SUCCEEDED";
        else 
            pmsg = "FAILED";
        MSGD ("\t\t...%s...", pmsg);
    }
}  /* end bkvfdlen */


/* PROGRAM: bkvfdio - run file I/O test(s)
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkvfdio(
	int	 wantio,	/* REGIO, RAWIO or SWRTIO */ 
	int	 fd)		/* fcntl this file */
{
    /* due to inconsistent methods of handling sync/raw I/O in the
       bk layer, for now this test NEEDS to be a conditional compile
    */
#if OSYNCFLG
	TEXT	*pmsg;
	int	 arg;

    if ((arg = fcntl (fd, F_GETFL, 0)) == -1) 
	MSGD ("bkvfdio: fcntl on fd=%d failed with errno=%d", fd, errno);
    else 
    {
	switch (wantio) 
	{
	    case REGIO:
		MSGD ("\t\t\twe want normal/BUFFERED (REGIO)");
		if (arg & O_SYNC) 
		{
		    MSGD ("\t\t\tfile opened: O_SYNC");
		    pmsg = "FAILED";
		} 
		else 
		{
		    MSGD ("\t\t\tfile is not O_SYNC");
		    pmsg = "SUCCEEDED";
		}
		break;
	    case SWRTIO:
		MSGD ("\t\t\twe want sync/UNBUFFERED (SWRTIO)");
		if (arg & O_SYNC) 
		{
		    MSGD ("\t\t\tfile opened: O_SYNC");
		    pmsg = "SUCCEEDED";
		} 
		else 
		{
		    MSGD ("\t\t\tfile is not O_SYNC");
		    pmsg = "FAILED";
		}
		break;
	    case RAWIO:
		MSGD ("\t\t\twe want raw (RAWIO)");
		MSGD ("\t\t\tOSYNC: this should never happen");
		pmsg = "bkvfdio: FAILED";
		break;
	    default:
		pmsg = "bkvfdio: io unknown: FAILED";
		break;
	}
	MSGD ("\t\t...%s...", pmsg);
    }
#endif /* OSYNCFLG */
}  /* end bkvfdio */


/* PROGRAM: bkdftype - display file type (db bi or ai)
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkdftype(
	int	 ftype,		/* file type, BKDATA, BKBI or BKAI */
	TEXT	*pmisc)		/* misc message */
{
	TEXT	*pmsg;

    switch (ftype) 
    {
	case BKDATA:
	    pmsg = "db (BKDATA)";
	    break;
	case BKBI:
	    pmsg = "bi (BKBI)";
	    break;
	case BKAI:
	    pmsg = "ai (BKAI)";
	    break;
	default:
	    pmsg = "bkdftype: file type unknown: FAILED";
	    break;
    }
    MSGD ("\t%s: %s", pmsg, pmisc);

}  /* end bkdftype */

#endif /* BKCHK */

