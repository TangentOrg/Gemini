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

#include "bkioprv.h"
#include "bkmsg.h"
#include "bkpub.h"
#include "bkprv.h"
#include "bmpub.h"
#include "mstrblk.h"
#include "cxpub.h"
#include "cxprv.h"
#include "dbcode.h"
#include "dbcontext.h"
#include "dblk.h"
#include "dbpub.h"
#include "dbvers.h"
#include "drmsg.h"
#include "dsmpub.h"
#include "extent.h"
#include "kypub.h"
#include "keypub.h"
#include "lkpub.h"
#include "objblk.h"
#include "recpub.h"
#include "rlmsg.h"
#include "rlpub.h"
#include "rmpub.h"
#include "rmprv.h"
#include "scasa.h"  /* for SCTBL_AREA,... */
#include "stmsg.h"
#include "stmpub.h" /* storage subsystem */
#include "stmprv.h"
#include "usrctl.h"
#include "utcpub.h"
#include "utfpub.h"
#include "utmpub.h"
#include "utmsg.h"
#include "utospub.h"
#include "utspub.h"
#include "uttpub.h"
#include "rltlpub.h"

#include <errno.h>
#include <time.h>

#if FSBSD42 | BULLDPX2 | SGI | OPSYS==UNIX
#include <sys/types.h> /* needed for lseek and read calls */
#endif

#if OPSYS==UNIX
#include <sys/types.h> /* needed for lseek and read calls */
#include <unistd.h>    /* needed for lseek and read calls */
#endif

#include <sys/stat.h>

#if OSYNCFLG
#include <fcntl.h>
#endif

#if OPSYS==WIN32API
#include <io.h>
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#ifndef TEMPBLKS
#define TEMPBLKS 4  /* this needs to go in a header shared by drdbset.c */
#endif

#define BKCOMPMAX 3 /* maximum number of components */

/**** LOCAL FUNCTION PROTOTYPES ****/

LOCALF int   bkDbVersionCheck	(dsmContext_t *pcontext, COUNT dbVersion);

LOCALF int   bkDbDemoVersion	(dsmContext_t *pcontext, mstrblk_t *pmstr);

LOCALF int   bkLoadAreas	(dsmContext_t *pcontext, LONG mode,
				 struct bkctl *pbkctl);
LOCALF bkAreaDesc_t *
             bkLoadFileTable	(dsmContext_t *pcontext, int numFiles,
				 bkflst_t *pFileList);
LOCALF int   bkOpenAllFiles	(dsmContext_t *pcontext, ULONG numAreas);
LOCALF int   bkSetFiles		(dsmContext_t *pcontext,
				 bkAreaDesc_t *pbkAreaDesc, 
                                 mstrblk_t *pcontrolBlock);

LOCALF int   bkTrimExtents	(dsmContext_t *pcontext, ULONG areaNumber,
				 LONG blockSize, TEXT *dbname);
#if 0
LOCALF DSMVOID  bkUpdateDbCounters	(dsmContext_t *pcontext, bkftbl_t *ptbkftbl,
				 bkiotbl_t *pbkiotbl);
#endif
LOCALF int   bkUpdateExtentHeaders(dsmContext_t *pcontext, LONG openTime,
				 COUNT newDate,
				 bkAreaDescPtr_t *pbkAreaDescPtr);

LOCALF int   bkVerifyExtentHeader(dsmContext_t *pcontext,
				  extentBlock_t *pExtentBlock,
				  bkAreaDesc_t *pbkAreaDesc);

LOCALF int   bkOpenControlArea  (dsmContext_t *pcontext, LONG mode,
				 bkAreaDescPtr_t *pbkAreaDescPtr,
                                 mstrblk_t *pcontrolBlock,
                                 extentBlock_t *pextentBlock);

LOCALF int    bkForEachArea	(dsmContext_t *pcontext, LONG mode,
                                 bkAreaDescPtr_t *pbkAreaDescPtr,
                                 mstrblk_t *pcontrolBlock,
                                 extentBlock_t *pextentBlock,
				 dsmArea_t schemaAreaFlag);

LOCALF int    bkForEachExtent	(dsmContext_t *pcontext, DBKEY areaExtentRoot,
				 dsmArea_t area_Number,
				 ULONG *pnumExtents, bkflst_t *pFileList);

LOCALF int    bkReadAmount	(dsmContext_t *pcontext, TEXT *fname,
				 fileHandle_t fd, gem_off_t offset,
				 LONG size, TEXT *pbuffer);

LOCALF DSMVOID  bkMarkDbSynced	(dsmContext_t *pcontext);

DSMVOID bkAreaSetDesc(objectBlock_t *pobjectBlock, bkAreaDesc_t *pbkAreaDesc);

/**** END LOCAL FUNCTION PROTOTYPES ****/

#define BKFD_CHUNKS    50	/* the amount to grow the bkfdtbl array */

/* PROGRAM: bkOpen - initialize the physical block manager and
 *          open both the database and bi file(s).
 *
 *          Accepts as arguments the name of the ".db" file and
 *          an indication of single-user versus multi-user mode.
 *
 *          The ".db" file identifies either a single-file
 *          PROGRESS database, or it is an ASCII structure file
 *          describing a multi-file PROGRESS database. In either
 *          case bkOpen builds file extent tables for the bi
 *          file as well as the database and opens all files.
 *
 * NOTE:    This function expects pdbcontext->pdbpub->dbblksize
 *          to be initialized with actual 'database blocksize'.
 *          bmbufo() requires it.
 *
 * RETURNS:
 *          0 if the block manager opens successfully,
 *          1 if an error is encountered.
 */
int
bkOpen(
	dsmContext_t	*pcontext,	/* open this database */
	LONG		 mode)		/* may have the DBRDONLY bit */
{
	dbcontext_t	*pdbcontext	 = pcontext->pdbcontext;
	dbshm_t	*pdbpub	 = pdbcontext->pdbpub;
        mstrblk_t	*pmstr;
    	struct bkctl	*pbkctl;
	int	 	 ret;		/* temp to pass result of bkOpen */

    TRACE_CB(pcontext, "bkOpen")
    /* verify critical db information */
    BKVERIFY("bkOpen", pdbcontext);


    /* allocate bkctl and set private dbctl pointer */
    pdbcontext->pbkctl =
       pbkctl = (struct bkctl *)stGet (pcontext, XSTPOOL(pdbcontext,
                        pdbpub->qdbpool), sizeof(struct bkctl) +
                        (sizeof (QBKTBL) * (unsigned) pdbpub->arghash));

    pdbpub->qbkctl = P_TO_QP(pcontext, pbkctl); /* set shared ptr */
#if 0
    pdbcontext->pbkfdtbl = (bkfdtbl_t *)NULL;
#endif

    /* save the session start time */
    pbkctl->bk_otime = time((time_t *)NULL);

    pbkctl->bk_dbfile = pdbcontext->bklkfd;

    /* initialize the buffer pool manager */
    bmbufo (pcontext);

    /* verify critical db information */
    BKVERIFY("bkOpen", pdbcontext);

    /* build the two file extent tables, one for the database
       and one for the bi file(s) */

    ret = bkLoadAreas(pcontext, mode, pbkctl);

    if (ret) return ret;

    if ( !((mode & DBCONTROL) && (mode & DBRDONLY)) )
    {

        /* make misc info available globally */
        pmstr = pdbcontext->pmstrblk;
        /* check if this is a DEMO version */
        if (pdbcontext->dbcode == PROCODE)
        {
            ret = bkDbDemoVersion(pcontext, pmstr);
            if (ret)
                return ret;
        }

        /* bug 20000211-012, needed to update oprdate */
        pmstr->mb_oppdate  = pbkctl->bk_otime; 
        pmstr->mb_oprdate  = pbkctl->bk_otime; 
    }

    /* verify critical db information */
    BKVERIFY("bkOpen", pdbcontext);

    return 0;

} /* end bkOpen */


/* PROGRAM: bkReadAmount -  reads sizeof bytes instead of BlockSize bytes.
 *
 * RETURNS: 0 success
 *         -1 error
 */
LOCALF int
bkReadAmount(
	dsmContext_t	*pcontext,
	TEXT		*fname,
	fileHandle_t	 fd,
	gem_off_t        offset,
        LONG		 size,
	TEXT		*pbuffer)
{
    LONG ret;
    int  bytesRead;
    int  readErrNo;
 
    ret = utReadFromFile(fd, offset, pbuffer, size, &bytesRead, &readErrNo);
    switch(ret)
    {
        case 0:
             break;
        case DBUT_S_SEEK_TO_FAILED:
        case DBUT_S_SEEK_FROM_FAILED:
        case DBUT_S_GET_OFFSET_FAILED:
             /* Invalid FD on seek, ret = %d file = %s */
             MSGN_CALLBACK(pcontext, bkMSG095, -1, fname);
             ret = -1;
             break;
        case DBUT_S_READ_FROM_FAILED:
        default:
             MSGN_CALLBACK(pcontext, bkMSG096, fname, readErrNo);
             ret = -1;
             break;
    }
 
    return ret;

}  /* end bkReadAmount */


/* PROGRAM: bkLoadAreas - Allocates and initializes all run-time structures
 *                        pertaining to areas.  Calls bkBuildExtentTable
 *                        to allocate and initialize extent structures
 *                        for the areas and opens the extent files.
 *
 * NOTE:    This function expects pdbcontext->pdbpub->dbblksize
 *          to be initialized with actual 'database blocksize'.
 *
 * RETURNS: 0 if everything goes ok,
 *	    1, EXINUSE or DB_EXBAD otherwise.
 */
LOCALF int
bkLoadAreas(
	dsmContext_t	*pcontext,	/* for this database */
	LONG		 mode,  	/* may have DBRDONLY bit on */
	struct bkctl	*pbkctl)	/* bkctl structure, file table
	 				   entries are chained from it */
{
    dbcontext_t	*pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbpub = pdbcontext->pdbpub;
    int           ret = 0;
    dsmStatus_t   returnCode = DB_EXBAD;
    TEXT	  dbname[MAXUTFLEN +1];
    bkAreaDescPtr_t  *pbkAreaDescPtr;
    bkAreaDesc_t  *pbkAreaDesc;
    mstrblk_t     *pmstr;
    bkflst_t      *pFileList = NULL;
    ULONG          numAreas;
    ULONG          numExtents;
    UCOUNT         blockSize = pdbcontext->pdbpub->dbblksize;
#if 0
    fileHandle_t   fd;
#endif
    bkftbl_t	 *ptmpbkftbl;

    extentBlock_t  *pextentBlock  = 0;
    mstrblk_t *pcontrolBlock = 0;

    /* convert the possibly fully qualified database name to a fully
     * qualified path name
     */

    if(pdbcontext->pdatadir)
    {
        utmypath (dbname, pdbcontext->pdatadir,
                  pdbcontext->pdbname,
                  (TEXT *)".db");            
    }
    else
    {
        utapath(dbname, sizeof(dbname), pdbcontext->pdbname, (TEXT *)".db");
    }


    ptmpbkftbl = (bkftbl_t *) utmalloc(sizeof(bkftbl_t));
    if ( ptmpbkftbl == NULL )
    {
        /* %B utmalloc allocation failure */
        MSGN_CALLBACK(pcontext, bkMSG069);
        goto bktblerror;
    }
    stnclr(ptmpbkftbl, sizeof(bkftbl_t));
    ptmpbkftbl->qself = P_TO_QP(pcontext, ptmpbkftbl);
    ptmpbkftbl->ft_fdtblIndex = pbkctl->bk_dbfile;
    ptmpbkftbl->ft_qname  = P_TO_QP(pcontext, dbname);
    ptmpbkftbl->ft_blklog = bkblklog(blockSize);

    pdbcontext->pdbpub->recbits = DEFAULT_RECBITS;

    pextentBlock = (extentBlock_t *)utmalloc(blockSize);
    if ( pextentBlock == NULL )
    {
        /* %B utmalloc allocation failure */
        MSGN_CALLBACK(pcontext, bkMSG069);
        goto bktblerror;
    }
    
    /* read the extent block to verify the dbVersion */
    ret = bkioRead (pcontext,
                   pdbcontext->pbkfdtbl->pbkiotbl[ptmpbkftbl->ft_fdtblIndex],
                   &ptmpbkftbl->ft_bkiostats, (LONG)0,
                   (TEXT *)pextentBlock, 1,
                   ptmpbkftbl->ft_blklog,
                   BUFFERED);
    if (ret < 0)
    {
        MSGN_CALLBACK(pcontext, bkMSG096, dbname, ret);        
        goto bktblerror;
    }
    BK_FROM_STORE(&pextentBlock->bk_hdr);
    /* check the database version number */
    if (bkDbVersionCheck (pcontext, pextentBlock->dbVersion) == -1)
        goto bktblerror;

    pcontrolBlock = (mstrblk_t *)utmalloc(blockSize);
    if ( pcontrolBlock == NULL )
    {
        /* %B utmalloc allocation failure */
        MSGN_CALLBACK(pcontext, bkMSG069);
        goto bktblerror;
    }
    
    /* read in the control block to get the _Area index root block ptr */
    ret = bkioRead (pcontext, 
                   pdbcontext->pbkfdtbl->pbkiotbl[ptmpbkftbl->ft_fdtblIndex],
                   &ptmpbkftbl->ft_bkiostats, (LONG)1,
                   (TEXT *)pcontrolBlock, 1,
                   ptmpbkftbl->ft_blklog,
                   BUFFERED);
    if (ret < 0)
        goto bktblerror;
    
    /* Get the block in machine dependent format  */
    BK_FROM_STORE(&pcontrolBlock->bk_hdr);
    if ( (mode & DBCONTROL) && (pdbcontext->dbcode != PROCODET) )
    {
      /* Open the .db */

      numAreas =  DSMAREA_MAXIMUM + 1;

      pbkAreaDescPtr = (bkAreaDescPtr_t *)stGet(pcontext,
                        XSTPOOL(pdbcontext, pdbpub->qdbpool),
                                  BK_AREA_DESC_PTR_SIZE(numAreas));

      if (pbkAreaDescPtr == NULL)
      {
          MSGN_CALLBACK(pcontext, stMSG006);
          goto bktblerror;
      }

      pbkAreaDescPtr->bkNumAreaSlots = numAreas;
      pbkctl->bk_qbkAreaDescPtr = (QBKAREADP)P_TO_QP(pcontext, pbkAreaDescPtr);

      /** Call special bkOpenControlArea **/
      ret = bkOpenControlArea(pcontext, mode, pbkAreaDescPtr,
                              pcontrolBlock, pextentBlock);
      if (ret == DB_EXBAD)
      {
         MSGN_CALLBACK(pcontext, bkMSG137); /* bkOpenControlArea error */
         goto bktblerror;
      }

      /* DBRDONLY and DBCONTROL then no master block! */
      if ( !((mode & DBCONTROL) && (mode & DBRDONLY)) )
      {
          bmLockDownMasterBlock(pcontext);
      }

    }  /****** end of Control area description boot strap ******/
    else
    {
    
/* TODO: The number of areas needs to be correctly calculated 
 *       The problem is that we don't know the number of areas 
 *       until we read them but we need the pbkAreaDescPtr set up
 *       to read them.  Maybe set up a dummy one so we can initially
 *       read the records out of the control area?
 */
      /* We allocate 1000 area q pointers to allow for online additions */
      numAreas = DSMAREA_MAXIMUM + 1;
    
      pbkAreaDescPtr = (bkAreaDescPtr_t *)stGet(pcontext,
                                  XSTPOOL(pdbcontext, pdbpub->qdbpool),
                                  BK_AREA_DESC_PTR_SIZE(numAreas));

      if (pbkAreaDescPtr == NULL)
      {
          MSGN_CALLBACK(pcontext, stMSG006);
          goto bktblerror;
      }
      pbkAreaDescPtr->bkNumAreaSlots = numAreas;
      pbkctl->bk_qbkAreaDescPtr = (QBKAREADP)P_TO_QP(pcontext, pbkAreaDescPtr);

      /* allocate a file list with only one entry */
      pFileList = (bkflst_t *)utmalloc(sizeof(bkflst_t));
      if( pFileList == NULL )
      {
          /* %B utmalloc allocation failure */
          MSGN_CALLBACK(pcontext, bkMSG069);
          goto bktblerror;
      }

      /* BUILD Area Descriptor for CONTROL AREA */


      /* build the file list for the .db file */
      pFileList->fl_maxlen = 0;  /* no maximum length */
      pFileList->fl_offset = 0;
      pFileList->fl_type   = BKDATA;
      /* (The control area uses default recbits) */
      pFileList->fl_areaRecbits = pdbcontext->pdbpub->recbits;
      stncop(pFileList->fl_name,dbname,MAXUTFLEN);
      /* put the pFileList extents in the bkftbl for this area */

      ret = bkBuildExtentTable(pcontext, mode,
                               DSMAREA_TYPE_DATA, DSMAREA_CONTROL,
                               &pbkAreaDesc,
                               1, pFileList,
                               pextentBlock, pcontrolBlock);
      if( ret )
          goto bktblerror;
  
      /* anchor this newly created area in the Area Descriptor Table */
      pbkAreaDescPtr->bk_qAreaDesc[DSMAREA_CONTROL] =
          (QBKAREAD)P_TO_QP(pcontext, pbkAreaDesc);
  
      /* Build the file list from the extent record in the CONTROL area.
       * (This cannot be done until after the control area
       *   pbkAreaDesc exists!)
       */
      if ( bkForEachExtent(pcontext, pcontrolBlock->areaExtentRoot,
                           DSMAREA_CONTROL, &numExtents, pFileList) )
          goto bktblerror;

#if 0
      /* The full pathname in the extent record of the .db MUST match
       * the pathname of the .db file opened.
       */
      if( utfcmp( dbname, pFileList->fl_name) != 0 )
      {
          MSGN_CALLBACK(pcontext, bkMSG014, dbname, pFileList->fl_name);
          goto bktblerror;
      }
#endif


      /* Now fix the db master block in memory. */
      bmLockDownMasterBlock(pcontext);

      if( pdbcontext->dbcode == PROCODE ) 
      {
          /* open all remaining areas */
          ret = bkForEachArea(pcontext, mode, pbkAreaDescPtr,
                              pcontrolBlock, pextentBlock, 0);
          if( ret )
             goto bktblerror;

          /* If no primary recovery exists fail! */
          pbkAreaDesc = XBKAREAD(pdbcontext, pbkAreaDescPtr->bk_qAreaDesc[DSMAREA_BI]);
          if (!pbkAreaDesc)
          {
              MSGN_CALLBACK(pcontext,bkMSG138); /* no primary recovery */
              goto bktblerror;
          }
	  if ((pbkAreaDesc->bkNumFtbl == 0)  && !(mode & DBOPENFILE))
          {
              MSGN_CALLBACK(pcontext, bkMSG139); 
              goto bktblerror;
          }
      }

    }  /****** end of non-CONTORL area description set up ******/


    /* DBRDONLY and DBCONTROL then no master block! */
    if ( !((mode & DBCONTROL) && (mode & DBRDONLY)) )
    {
        pmstr = pdbcontext->pmstrblk;
  
        /*----------------------------------------------------------------*/
        /* if the old open time hasn't happenned yet, then make it the    */
        /* current time.                                                  */
        /*                                                                */
        /* (GSS:12/9/99 - The previous open time must never go backwards. */
        /* Incremental backups assumes that time will always move forward.*/
        /* If time is set back, and changes made to the database, those   */
        /* changes would *not* get incrementally backed up.               */
        /*                                                                */
        /*----------------------------------------------------------------*/

        if ( (ULONG)pmstr->mb_oppdate >= (ULONG)pbkctl->bk_otime )
            pbkctl->bk_otime = pmstr->mb_oppdate +1;

        if ((pdbcontext->dbcode != PROCODET) &&
            /* if NOT read only database */
            !( (mode & DBRDONLY) || pdbcontext->argronly ) &&
            !(mode & DBCONTROL) )
        { 

	    if (bkUpdateExtentHeaders(pcontext, pbkctl->bk_otime,
                                      (COUNT)(!pextentBlock->dateVerification),
                                      pbkAreaDescPtr) )
                goto bktblerror;
        }
    }

    returnCode = 0;
   
bktblerror:
   
    if (pFileList)
        utfree((TEXT *)pFileList);
    if (pextentBlock)
        utfree((TEXT *)pextentBlock);
    if (pcontrolBlock)
        utfree((TEXT *)pcontrolBlock);
    utfree((TEXT *)ptmpbkftbl);
    return( returnCode );

}  /* end bkLoadAreas */


/* PROGRAM: bkForEachArea - Load area and extent descriptions
 *
 * RETURNS: 0 if successful
 *         -1 if error ecountered
 */
LOCALF int
bkForEachArea(
    dsmContext_t	*pcontext,
    LONG		 mode,
    bkAreaDescPtr_t	*pbkAreaDescPtr,
    mstrblk_t   	*pcontrolBlock,
    extentBlock_t	*pextentBlock,
    dsmArea_t		 schemaAreaFlag /* open shema area ONLY */)
{
    CURSID   cursorId = NULLCURSID;

    dsmRecid_t areaRecordId;

    ULONG        numExtents;
    svcDataType_t component[4];
    svcDataType_t *pcomponent[4];
    COUNT         maxKeyLength = 50; /* SHOULD BE A CONSTANT! */
    dsmArea_t    areaNumber;
    dsmAreaType_t areaType;
    dsmRecbits_t  areaRecbits;
    bkAreaDesc_t *pbkAreaDesc;
    bkflst_t     *pFileList = 0;
    unsigned int  recordSize;
    dsmStatus_t  returnCode = DB_EXBAD;
    dsmStatus_t  ret;
    TEXT  recordBuffer[(8 * sizeof(LONG)) + MAXUTFLEN +1];
    ULONG    indicator;


    AUTOKEY(areaKeyLow, KEYSIZE)
    AUTOKEY(areaKeyHi, KEYSIZE)

    pFileList = (bkflst_t *)utmalloc(DSMEXTENT_MAXIMUM * sizeof(bkflst_t) );
    if( pFileList == NULL )
    {
        /* %BbkForEachArea: utmalloc allocation failure */
        MSGN_CALLBACK(pcontext, bkMSG069);
        goto done;
    }

    /* set up keys to define bracket for scan of the area table */
    areaKeyLow.akey.area         = DSMAREA_CONTROL;
    areaKeyHi.akey.area          = DSMAREA_CONTROL;
    areaKeyLow.akey.root         = pcontrolBlock->areaRoot;
    areaKeyHi.akey.root          = pcontrolBlock->areaRoot;
    areaKeyLow.akey.unique_index = 1;
    areaKeyHi.akey.unique_index  = 1;
    areaKeyLow.akey.word_index   = 0;
    areaKeyHi.akey.word_index    = 0;

    /* assign high and low component values */
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    if (schemaAreaFlag)
        component[0].f.svcLong   = DSMAREA_SCHEMA;
    else
        component[0].f.svcLong   = DSMAREA_CONTROL+1;

    areaKeyLow.akey.index    = SCIDX_AREA;
    areaKeyLow.akey.keycomps = 1;
    areaKeyLow.akey.ksubstr  = 0;

    pcomponent[0] = &component[0];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                 &areaKeyLow.akey) )
        goto done;

    areaKeyHi.akey.index    = SCIDX_AREA;
    areaKeyHi.akey.ksubstr  = 0;
    if (schemaAreaFlag)
        component[0].f.svcLong = DSMAREA_SCHEMA;
    else
        component[0].f.svcLong = DSMAREA_MAXIMUM;

    component[1].indicator  = SVCHIGHRANGE;
    pcomponent[1]           = &component[1];
    areaKeyHi.akey.keycomps = 2;
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                 &areaKeyHi.akey) )
        goto done;

    /* attempt to get first index entry within specified bracket */
    ret = (dsmStatus_t)cxFind (pcontext,
                               &areaKeyLow.akey, &areaKeyHi.akey,
                               &cursorId, SCTBL_AREA, &areaRecordId,
                               DSMUNIQUE, DSMFINDFIRST, LKNOLOCK);

    while (ret == 0)
    {
        /* extract the area # from the area reocrd information */
        recordSize = rmFetchRecord(pcontext, DSMAREA_CONTROL,
                                areaRecordId, recordBuffer,
                                sizeof(recordBuffer),0);
        if ( recordSize > sizeof(recordBuffer))
        {
            MSGN_CALLBACK(pcontext,bkMSG141,(LONG)recordSize);
            returnCode = DSM_S_RMNOTFND;
            goto done;
        }

        /* Extract the area number    */
         if ( recGetLONG(recordBuffer, SCFLD_AREANUM,(ULONG)0,
               (LONG *)&areaNumber, &indicator) || (indicator == SVCUNKNOWN) )
            goto done;

        /* Extract the area Type */
         if ( recGetLONG(recordBuffer, SCFLD_AREATYPE,(ULONG)0,
                 (LONG *)&areaType, &indicator) || (indicator == SVCUNKNOWN) )
            goto done;

        /* Extract the area recBits */
         if ( recGetSMALL(recordBuffer, SCFLD_AREABITS,(ULONG)0,
              (COUNT *)&areaRecbits, &indicator) || (indicator == SVCUNKNOWN) )
            goto done;

        /* The control area is already opened, so don't open it again.
         * If the shcemaAreaFlag is set, then only open the schema area.
         * If the shcemaAreaFlag isn't set, then open all other areas.
         * (The previously set up bracket in conjunction with the 
         * schemaAreaFlag restricts which areas will be traversed.)
         */
        if ((schemaAreaFlag) || (areaNumber != DSMAREA_SCHEMA))
        {
            /* Build a file list of all extents in this area. */
            if ( bkForEachExtent(pcontext, pcontrolBlock->areaExtentRoot,
                                 areaNumber, &numExtents, pFileList) )
                goto done;

            pFileList->fl_areaRecbits = areaRecbits;
            /* Load the bkftbl for this area */
            if ( bkBuildExtentTable(pcontext, mode,
                                   areaType, areaNumber,
                                   &pbkAreaDesc,
                                   numExtents, pFileList,
                                   pextentBlock, pcontrolBlock) )
              goto done;

            /* anchor this newly created area in the Area Descriptor Table */
            pbkAreaDescPtr->bk_qAreaDesc[areaNumber] =
                               (QBKAREAD)P_TO_QP(pcontext, pbkAreaDesc);

        }

        /* attempt to get next index entry within specified bracket */
        ret = (dsmStatus_t)cxNext(pcontext,
                                 &cursorId, SCTBL_AREA, &areaRecordId,
                                 &areaKeyLow.akey, &areaKeyHi.akey,
                                 DSMGETTAG, DSMFINDNEXT, LKNOLOCK);
    }

    if( ret == DSM_S_ENDLOOP )
    {
          returnCode = 0;
    }

done:
    cxDeactivateCursor(pcontext, &cursorId);

    if (pFileList)
        utfree((TEXT *)pFileList);

    return returnCode;

}  /* end bkForEachArea */


/* PROGRAM: bkForEachExtent - build the file list for the given area
 *
 * RETURNS: 0 if successful
 *          DB_EXBAD if error ecountered
 */
LOCALF int
bkForEachExtent(
	dsmContext_t	*pcontext,
	DBKEY		 areaExtentRoot,
	dsmArea_t	 areaNumber,
	ULONG		*pnumExtents,
	bkflst_t	*pFileList  /* Location for list of files in area */)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    CURSID   cursorId = NULLCURSID;

    dsmRecid_t extentRecordId;
    dsmStatus_t ret = -1;  /* initial to failure */
    dsmStatus_t returnCode = DB_EXBAD;  /* initial to failure */

    svcDataType_t component[BKCOMPMAX];
    svcDataType_t *pcomponent[BKCOMPMAX];
    COUNT         maxKeyLength = KEYSIZE;
    TEXT          storedFileName[MAXUTFLEN+1];

    TEXT  recordBuffer[(8 * sizeof(LONG)) + MAXUTFLEN +1];
    unsigned int   recordSize;
    svcByteString_t byteStringField;
    ULONG indicator;

    AUTOKEY(keyLow, KEYSIZE)
    AUTOKEY(keyHi,  KEYSIZE)

    *pnumExtents = 0;

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

   if ( keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                           &keyLow.akey) )
       goto done;

    component[0].f.svcLong       = (LONG)areaNumber;
    component[1].f.svcLong       = DSMEXTENT_MAXIMUM;
 
    keyHi.akey.index    = SCIDX_EXTAREA;
    keyHi.akey.keycomps = 2;
    keyHi.akey.ksubstr  = 0;

    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                           &keyHi.akey))
        goto done;

    ret = (dsmStatus_t)cxFind (pcontext,
                              &keyLow.akey, &keyHi.akey,
                              &cursorId, SCTBL_EXTENT, &extentRecordId,
                              DSMUNIQUE, DSMFINDFIRST, LKNOLOCK);

    while (ret == 0)
    {
        (*pnumExtents)++;
        /* extract the area Extent reocrd information */
        recordSize = rmFetchRecord(pcontext, DSMAREA_CONTROL,
                                extentRecordId, recordBuffer,
                                sizeof(recordBuffer),0);
        if( recordSize > sizeof(recordBuffer))
        {
            MSGN_CALLBACK(pcontext,bkMSG142,recordSize);
            goto done;
        }

        /* extract the extent file type */
        if ( recGetTINY(recordBuffer, SCFLD_EXTTYPE,(ULONG)0,
                &pFileList->fl_type, &indicator) || (indicator == SVCUNKNOWN) )
            goto done;
#if 0
        /* convert extent type to file type */
        switch(pFileList->fl_type)
        {
          case DBMBI:
            pFileList->fl_type = BKBI;
            break;
          case DBMAI:
            pFileList->fl_type = BKAI;
            break;
          default:
            pFileList->fl_type = BKDATA;
        }
#endif

        /* extract the extent size (0 for variable length files) */
        if ( recGetLONG(recordBuffer, SCFLD_EXTSIZE,(ULONG)0,
               &pFileList->fl_maxlen, &indicator) || (indicator == SVCUNKNOWN) )
            goto done;

        byteStringField.pbyte = storedFileName;
        byteStringField.size  = sizeof(storedFileName);
        /* Extract the extent file name */
        if (recGetBYTES(recordBuffer, SCFLD_EXTPATH,(ULONG)0,
               &byteStringField, &indicator) || (indicator == SVCUNKNOWN) )
            goto done;

        if (storedFileName[0] == '.')
        {
            /* process relative path name */
            /* get path of dbname */
            utapath(pFileList->fl_name, sizeof(pFileList->fl_name),
                    pdbcontext->pdbname, (TEXT *)"");
            utfpath(pFileList->fl_name); /* strip db name */
            /* append file name */
            stcopy (pFileList->fl_name + stlen(pFileList->fl_name),
                    storedFileName+1 );
        }
        else if (pdbcontext->pdatadir)
        {
            utmypath(pFileList->fl_name,pdbcontext->pdatadir,
                     storedFileName, NULL);
        }
        else
            stcopy(pFileList->fl_name, storedFileName);

/* TODO: How should this be set? */
        pFileList->fl_offset = 0; 

        pFileList++;

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

}  /* end bkForEachExtent */


/* PROGRAM: bkOpenControlArea - Open the database control and schema area only
 *
 * RETURNS: 0     - success
 *          DB_EXBAD - Failure
 */
LOCALF int
bkOpenControlArea(
         dsmContext_t	*pcontext,
         LONG             mode,
         bkAreaDescPtr_t *pbkAreaDescPtr,
         mstrblk_t       *pcontrolBlock,
         extentBlock_t   *pextentBlock)
{

/* build a file list for each area we need, then
 * call bkBuildExtentTable to add the entries in
 * bkAreaDesc->bkftbl list of extents.
 * DSMAREA_CONTROL
 * DSMAREA_SCHEMA
 */
    dbcontext_t    *pdbcontext = pcontext->pdbcontext;
    bkAreaDesc_t   *pbkControlAreaDesc; /* allocated by bkBuildExtentTable */
    bkflst_t       *pFileList = 0;   /* Pointer to list of files in the area */
    int             returnCode = DB_EXBAD;

    /* allocate a file list with only one entry */
    pFileList = (bkflst_t *)utmalloc(sizeof(bkflst_t));
    if (pFileList == ((bkflst_t *)0))
    {
        /* %Bbkrdmb: utmalloc allocation failure */
        MSGN_CALLBACK(pcontext, bkMSG069);
        goto done1;
    }

    /* build the file list for the .db file */
    pFileList->fl_maxlen = 0;  /* no maximum length */
    pFileList->fl_offset = 0;
    pFileList->fl_type   = BKDATA;
    /* (The control area uses default recbits) */
    pFileList->fl_areaRecbits = pdbcontext->pdbpub->recbits;
    utmypath(pFileList->fl_name,pdbcontext->pdatadir,pdbcontext->pdbname,
             (TEXT *)".db");

    /* put the pFileList extents in the bkftbl for this area */
    if ( bkBuildExtentTable(pcontext, mode,
               DSMAREA_TYPE_DATA, DSMAREA_CONTROL,
               &pbkControlAreaDesc,
               1, pFileList,
               pextentBlock, pcontrolBlock) )
       goto done1;

    /* anchor this newly created area in the Area Descriptor Table */
    pbkAreaDescPtr->bk_qAreaDesc[DSMAREA_CONTROL] =
                           (QBKAREAD)P_TO_QP(pcontext, pbkControlAreaDesc);

    returnCode = 0;

done1:
    if (pFileList)
        utfree((TEXT *)pFileList);

    return returnCode;

}  /* end bkOpenControlArea */


/* PROGRAM: bkBuildExtentTable - build extent tables and open
 *          database and bi files.
 *
 * This procedure takes as arguments the name of the database to be
 * opened, the desired lock mode, a pointer to the bkctl block to be filled
 * in (this allows multiple databases to be opened by utilities) and some
 * information about the area the extents belong to.
 *
 * The block size of the particular area type is set.
 * A file table for the area is allocated & initialized (via bkLoadFileTable())
 * The area descriptor is filled in.
 * All files are opened for BUFFERED I/O (via bkOpenFilesBuffered())
 * Extent headers are verified
 * All files are re-opened with the proper I/O
 * The file table values are filled in (via bkSetFiles())
 * Some area type specific stuff is performed.
 *
 * The database is now opened and locked, if possible.
 *
 * RETURNS: 0 if everything goes ok,
 *          DB_EXBAD otherwise.
 */
int
bkBuildExtentTable(
	dsmContext_t  *pcontext,     /* for this database */
        LONG           mode,         /* may have DBRDONLY bit on */
        int            areaType,     /* areaType (BKAI,BKDATA,BKBI or BKTL */
        ULONG          area,         /* area number                        */
        bkAreaDesc_t **ppbkAreaDesc, /* Anchor for run-time description
                                       of this area.                    */
        int            numFiles,     /* Number files in the area.            */
        bkflst_t      *pFileList,    /* Pointer to list of files in the area */
        extentBlock_t *pextentBlock, /* Extent Block Pointer */
        mstrblk_t     *pcontrolBlock)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    LONG           blockSize;
    dsmStatus_t    returnCode = DB_EXBAD;
    dsmStatus_t    ret;
    GTBOOL          readOnly = 0; /* Open for read only      */
    bkAreaDesc_t  *pbkAreaDesc;
    objectBlock_t *pobjectBlock = 0;
    ULONG	   fdTableIndex;

    if ( mode & DBRDONLY || pdbcontext->argronly )
        readOnly = 1;

    switch (areaType)
    {
        case DSMAREA_TYPE_DATA:
            /* NOTE: Don't get dbblocksize out of mstrblk because
             * bmLockDownMasterBlock may not have been called yet.
             * The assumption here is that the .db and .dn extents
             * have the same block size.
             */
            blockSize = pdbcontext->pdbpub->dbblksize;
            break;
        case DSMAREA_TYPE_BI:
            blockSize = pdbcontext->pmstrblk->mb_biblksize;
            break;
        case DSMAREA_TYPE_AI:
            blockSize = pdbcontext->pmstrblk->mb_aiblksize;
            break;
        case DSMAREA_TYPE_TL:
            blockSize = RLTL_BUFSIZE;
            break;
            
        default:
            blockSize = pdbcontext->pdbpub->dbblksize;
     }

    pbkAreaDesc = bkLoadFileTable(pcontext, numFiles, pFileList);
    if (pbkAreaDesc == NULL )
       goto bktblerror;

    *ppbkAreaDesc = pbkAreaDesc;

    /* Fill in area description */
    pbkAreaDesc->bkAreaNumber = area;
    pbkAreaDesc->bkAreaType   = areaType;
    pbkAreaDesc->bkBlockSize  = blockSize;
    pbkAreaDesc->bkAreaRecbits  = pFileList->fl_areaRecbits;

    if (pdbcontext->dbcode == PROCODET )
    {
        pbkAreaDesc->bkftbl->ft_fdtblIndex = pdbcontext->pbkctl->bk_dbfile;
        pbkAreaDesc->bkftbl->ft_qname = P_TO_QP(pcontext, pdbcontext->pdbname);
        pbkAreaDesc->bkftbl->ft_curlen = (gem_off_t)TEMPBLKS << bkblklog(blockSize);
    }

    /* opens all files for BUFFERED I/O and initializes structures */
    ret = bkOpenFilesBuffered(pcontext, readOnly, pbkAreaDesc, pcontrolBlock);
    if(ret)
    {
       /* they are forcing in, so let them ignore missing files
          but take the files out of the storage object, area, and
          extent tables */
       if (((pbkAreaDesc->bkAreaNumber > DSMAREA_DEFAULT) && 
           (pdbcontext->confirmforce)) ||
          ((pbkAreaDesc->bkAreaNumber > DSMAREA_DEFAULT) && 
          (ret == BKIO_ENOENT)))
       {
           /* mark the area for deletion once the database is open */
           pbkAreaDesc->bkDelete = 1;
       }
       else
       {
           goto bktblerror;
       }
    }

    /* bkSetFiles fills in the file table values */
    if( bkSetFiles( pcontext, pbkAreaDesc, pcontrolBlock ))
        goto bktblerror;

    if (!readOnly && pextentBlock) 
    {
        if (!(pcontrolBlock->mb_dbstate & DBCLOSED))
        {
            if (!pdbcontext->confirmforce)
                if( bkVerifyExtentHeader(pcontext, pextentBlock, pbkAreaDesc))
                    goto bktblerror;
        }

        /* opens all files for the proper I/O. */
        if( bkOpenFilesUnBuffered( pcontext, pbkAreaDesc ))
            goto bktblerror;
    }

    /* The following is only valid for data areas */
    if ((pbkAreaDesc->bkAreaType == DSMAREA_TYPE_DATA) && (!pbkAreaDesc->bkDelete))
    {
        pobjectBlock = (objectBlock_t *)utmalloc(blockSize);
        if ( pobjectBlock == NULL )
        {
            /* %B utmalloc allocation failure */
            MSGN_CALLBACK(pcontext, bkMSG069);
            goto bktblerror;
        }

        fdTableIndex = pbkAreaDesc->bkftbl->ft_fdtblIndex;
        if (bkioRead (pcontext,
                       pdbcontext->pbkfdtbl->pbkiotbl[fdTableIndex],
                       &pbkAreaDesc->bkftbl->ft_bkiostats, (LONG)2,
                       (TEXT *)pobjectBlock, 1,
                       pbkAreaDesc->bkftbl->ft_blklog,
                       BUFFERED) < 0)
            goto bktblerror;
	BK_FROM_STORE(&pobjectBlock->bk_hdr);
#if 1  /* perform bug #99-02-24-039 fixup */
        if ((pdbcontext->dbcode != PROCODET) &&
            /* if NOT read only database */
            !( (mode & DBRDONLY) || pdbcontext->argronly ) &&
            !(mode & DBCONTROL) )
        {
            ULONG     i;
            ULONG     totalBlocks = 0;
            gem_off_t fileSize;
            bkftbl_t *pbkftbl;

            /* Due to bug #99-02-24-039, we must fix existing databases by
             * ensuring that the totalBlocks matches the actual database sice.
             * So, we count the total blocks in the data area.  If
             * the sum of the extent sizes doesn't match the totalBlocks for
             * this area, update the total blocks value.
             */    
            pbkftbl = pbkAreaDesc->bkftbl;
            for( i = 0; i < pbkAreaDesc->bkNumFtbl; i++, pbkftbl++)
            {
                if (pbkftbl->ft_curlen)
                    totalBlocks += (ULONG)(pbkftbl->ft_curlen / blockSize);
                else
                {
                    /* variable length file so must utflen. */
                    utflen(XTEXT(pdbcontext, pbkftbl->ft_qname), &fileSize);
                    totalBlocks += (ULONG)(fileSize / blockSize) - 1;
                }
            }
            if (totalBlocks != pobjectBlock->totalBlocks)
            {
                /* This database contains the bug.  Update the object block. */
                pobjectBlock->totalBlocks = totalBlocks;
		BK_TO_STORE(&pobjectBlock->bk_hdr);
                returnCode = bkioWrite(pcontext,
                      pdbcontext->pbkfdtbl->pbkiotbl[fdTableIndex],
                      &pbkAreaDesc->bkftbl->ft_bkiostats, (LONG) 2,
                      (TEXT *)pobjectBlock, 1,
                      pbkAreaDesc->bkftbl->ft_blklog, BUFFERED);
                if( returnCode != 1 )
                {
                   /* %BUnable to write object block */
                   MSGN_CALLBACK(pcontext, bkMSG154,
                           XTEXT(pdbcontext, pbkAreaDesc->bkftbl->ft_qname),
                           returnCode );
                   goto bktblerror;
                }
            }
        }
#endif

        bkAreaSetDesc(pobjectBlock, pbkAreaDesc);
    }

    if ( (pbkAreaDesc->bkAreaType == DSMAREA_TYPE_AI) &&
         (pbkAreaDesc->bkNumFtbl == 0) )
    {
        /* AI area but no associated extents!
         * NOTE: This should not be possible in asa database
         */
        if ( (mode & DBAIBEGIN) ||
             pcontext->pdbcontext->pdbpub->qainame || 
             ((mode & DBOPENDB) && (rlaiqon(pcontext) == 1)) )
        {
           goto bktblerror;
        }

        /* Ai is not enabled so we don't have to get this file open */
        return 0;
    }
    else if((pbkAreaDesc->bkAreaType == DSMAREA_TYPE_TL) &&
       (pbkAreaDesc->bkNumFtbl == 0 ))
    {
        if((mode & DBOPENDB) && rltlQon(pcontext))
            goto bktblerror;

        /* 2phase is not enabled so don't need to get these extents open. */
        return 0;
    }
    
    /* make sure files are sized appropriately based on block size */
    if( (pbkAreaDesc->bkAreaType == DSMAREA_TYPE_BI) &&
        (bkTrimExtents(pcontext, DSMAREA_BI, blockSize, pdbcontext->pdbname)) )
        goto bktblerror;

    returnCode = 0;

bktblerror:
    if (pobjectBlock)
        utfree((TEXT *)pobjectBlock);

    return returnCode;

}  /* end bkBuildExtentTable */


/* PROGRAM: bkTrimExtents - trims logical size of extents for blocksize
 *
 * This function takes an areaNumber and trims the length all files of that
 * area by removing any partial blocks.  This is necessary to make sure that
 * variable size blocks fit cleanly within fixed size extents.  This
 * adjustment occurs each time the database is opened.
 *
 * This adjustment is independent of any offset adjustment to align blocks on
 * block boundaries.
 *
 * RETURNS: 0 if ok, 1 if something is wrong.
 */
LOCALF int 
bkTrimExtents(
	dsmContext_t	*pcontext,
	ULONG		 areaNumber,         /* DSMAREA_BI */
	LONG		 blockSize,          /* blocksize in bytes */
	TEXT		*dbname)             /* database name */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
     bkftbl_t *pbkftbl;
     bkftbl_t *pfirst = (bkftbl_t *)NULL;
     int     ret;

    ret = bkGetAreaFtbl(pcontext, areaNumber, &pfirst);

    /* check to see that we have a valid blocksize */
    if (blockSize > 0)
    {
	/* loop through extents for filetype checking each one */
	for (pbkftbl = pfirst; pbkftbl && pbkftbl->ft_type != 0; pbkftbl++)
	{
	    if ( pbkftbl->ft_curlen % blockSize )
	    {
                /* Trimed file %s for blocksize %l bytes old:%l new:%l */
                MSGN_CALLBACK(pcontext, bkMSG040,
                     XTEXT(pdbcontext, pbkftbl->ft_qname), blockSize, 
                     pbkftbl->ft_curlen,
		     (pbkftbl->ft_curlen / blockSize) * blockSize);

		/* force the current length to be a multiple of blocksize */
		pbkftbl->ft_curlen = 
		    (pbkftbl->ft_curlen / blockSize) * blockSize;
	    }
	}
    }

    /* return result of double checking the adjusted lengths */

    return bkCheckBlocksize(pcontext, blockSize, BK_END_CHECK, pfirst, dbname);

}  /* end bkTrimExtents */


/* PROGRAM: bkCheckBlocksize - checks that files are blocksize compatible
 *
 * This function takes a file type and determines if all files of that type
 * in the database are compatible with the provided blocksize.  This is used
 * to determine if adjustable blocksize files are compatible with the given
 * blocksize.
 *
 * The pre check looks to see of a blocksize COULD be compatible with a fixed
 * length extent (variable length extents always could be compatible) by making
 * sure that the total size of the extent is divisible into blocksize units.
 *
 * The end check looks to see of a blocksize IS compatible with all existing
 * fixed and variable length extents by making sure that any extent header
 * is the allocated space
 * is divisible into blocksize units and if a
 * 
 *
 * RETURNS: 0 if ok, 1 if blocksize doesn't fit.
 */
int 
bkCheckBlocksize(
	dsmContext_t	*pcontext,
	int         blockSize, 		/* blocksize in bytes */
	int         checkType, 		/* PRE, END or ALL check */
	bkftbl_t   *pbkftbl,            /* file table structure */
     	TEXT       *dbname)    		/* database name */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int         status = 0;     /* assume the best */

    /* check to see that we have a valid blocksize */
    if (blockSize <= 0)
    {
        /* Invalid blocksize %d<blocksize> in database */
        MSGN_CALLBACK(pcontext, bkMSG041, blockSize, dbname);
	status = 1;
    }
    else
    {
	/* loop through extents for filetype checking each one */
	for (; pbkftbl && pbkftbl->ft_type != 0;
	     pbkftbl++)
	{
	    /* make sure this extent could be compatible with blocksize */
	    if ((checkType & BK_PRE_CHECK) &&
                (pbkftbl->ft_type & BKFIXED) &&
                (pbkftbl->ft_curlen > 0) &&
		((pbkftbl->ft_curlen + pbkftbl->ft_offset) % blockSize))
	    {
		/* ** file %s is not compatible with blocksize %d bytes */
                MSGN_CALLBACK(pcontext, bkMSG042,
                              XTEXT(pdbcontext, pbkftbl->ft_qname), blockSize);
		status = 1;

		/* cut short further checking on this file */
		continue;
	    }

	    /* check to make sure this extent is compatible with blocksize */
	    if ((checkType & BK_END_CHECK))      /* post check */
            {
                /* check allocated space is compatible */
                if (((pbkftbl->ft_offset > 0) &&          /* non-zero header */
                     (pbkftbl->ft_offset != blockSize)) || /* header space */
                    ((pbkftbl->ft_curlen > 0) &&          /* non-zero length */
                     (pbkftbl->ft_curlen % blockSize)))   /* allocated space */
                {
                    /* ** file %s is not compatible with blocksize %d bytes */
                    MSGN_CALLBACK(pcontext, bkMSG042,
                                  XTEXT(pdbcontext, pbkftbl->ft_qname), blockSize);
                    status = 1;
                }
            }
        }
    }
    
    /* return resulting status */
    return status;
    
}  /* end bkCheckBlocksize */


/* PROGRAM: bkblklog - return the blklog constant for a given blocksize
 *
 * This function takes a blocksize specified in bytes and returns the 
 * blklog constant to be used for efficent calculation between blocks 
 * and bytes:
 *
 *      bytes >> blklog  converts from bytes to blocks
 *      blocks << blklog converts from blocks to bytes
 *
 * RETURNS: number of bits to use as blklog shift constant.
 */
LONG
bkblklog(LONG blksize)     /* blocksize specified in bytes */
{
    LONG blklog;

    /* shift right to determine number of bits needed for blocksize */
    for (blklog = 0; (blksize = blksize >> 1) > 0; blklog++);

    return blklog;

}  /* end bkblklog */


/* PROGRAM: bkOpen2ndUser - Do file opening for 2nd user of shared memory
 *          and initialize private buffers
 *
 * RETURNS: 0 if successful, 1 if error
 */
int
bkOpen2ndUser (dsmContext_t *pcontext)	/* open for this database */
{
	dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    	int	 numfiles;
	ULONG    i;
        bkAreaDescPtr_t *pbkAreaDescPtr;
	bkAreaDesc_t    *pbkAreaDesc;

    pdbcontext->pbkfdtbl = (bkfdtbl_t *)NULL;

    /* need to get a bkiotbl structure for the .db whether we are
       opening it or not.  This will line up the ft_fdtblIndex for
       each database file */

    numfiles = 1;
    
    pbkAreaDescPtr = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
    for ( i = 0 ; i < pbkAreaDescPtr->bkNumAreaSlots; i++ )
    {
       pbkAreaDesc = XBKAREAD(pdbcontext, pbkAreaDescPtr->bk_qAreaDesc[i]);
       if(pbkAreaDesc)
       {
           numfiles += pbkAreaDesc->bkNumFtbl;
       }
    }

    bkInitIoTable(pcontext, numfiles);

    if (bkOpenAllFiles (pcontext, pbkAreaDescPtr->bkNumAreaSlots) != 0 )
    {
	return 1;
    }

    /* allocate the private buffer pool and private-buffer-table,
       if single user or not a SHMEMORY machine no private buffers
    */
    usrbufo(pcontext);

    /* *** TODO: demo checking needs to be consolidated */
    /* check if the test-drive stuff is OK */
    if (pdbcontext->usertype & DBSHUT)
    {
	;    /* dont care for proshut */
    }
    else
    {
	if (pdbcontext->modrestr & DSM_MODULE_DEMOONLY)
	{
	    if (!(pdbcontext->pmstrblk->mb_flag & DEMOVERS))
	    {
		/* your database version doesn't match the PROGRESS version */
		MSGN_CALLBACK(pcontext, bkMSG058);
		return 1;
	    }
	}
	else
	{
	    if (pdbcontext->pmstrblk->mb_flag & DEMOVERS)
	    {
		/* your database version doesn't match the PROGRESS version */
		MSGN_CALLBACK(pcontext, bkMSG058);
		return 1;
	    }
	}
    }

    return 0;

}  /* end bkOpen2ndUser */


/* PROGRAM: bkOpenAllFiles - For non 1st users - open all the files in the
 *          file table, include all extents for a multi-volume version.
 *
 *          Not much decision making work should be done here as it 
 *          has already been done as part of bkBuildExtentTable() for the 1st
 *          user.  All the open code is dependent on ft_openmode.
 *         
 * RETURNS: 0 if successful, -1 if error
 */
LOCALF int
bkOpenAllFiles (
	dsmContext_t	*pcontext,
	ULONG		 numAreas)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
	bkftbl_t *ptbl = (bkftbl_t *)NULL;
	LONG	  extent;
	int	  fd;
	ULONG     index;
	
    for( index = 0; index < numAreas; index++ )
    {
       extent = bkGetAreaFtbl(pcontext, index,&ptbl);
       
       if (ptbl)
       {
	  for (extent = 1; ptbl->ft_qname; ptbl++, extent++)
	  {  
              /* open file, error if not possible */
             if ((fd = bkOpenOneFile(pcontext, ptbl, OPEN_RW)) < 0)
	     {
	        MSGN_CALLBACK(pcontext, bkMSG002,
                              XTEXT(pdbcontext, ptbl->ft_qname), errno);
	        return -1;
	     }
	  }
       }
    }
    return 0;

}  /* end bkOpenAllFiles */


/* PROGRAM: bkOpenOneFile - opens 1 file in all ALL I/O modes 
 *
 * RETURNS: 0 on successs
 *         -1 on failure
 */
int
bkOpenOneFile(
	dsmContext_t	*pcontext,
	bkftbl_t	*pbkftbl,
	int		 openFlags)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    LONG     mode;   /* open mode for this file */
    fileHandle_t  fd;     /* temporary storage for the fd utOsOpen() returns */
    TEXT     namebuf[MAXUTFLEN];
    LONG     openReturn;                          /* return for bkioOpen() */

    utapath(namebuf, sizeof(namebuf),
            XTEXT(pdbcontext, pbkftbl->ft_qname), (TEXT *)"");

    if (pdbcontext->argronly) 
    {
	/* if we are opening the database read-only, then everything
	   can be opened buffered, since we won't be doing any writes */

        mode = BUFIO;
    }
    else
    {
        /* if we are not readonly, then open the file the same way 
           the broker opened it */

        mode = pbkftbl->ft_openmode;
    }

    switch (mode)
    {
	case BUFIO:  /* file needs only a buffered fd */

            if ((openReturn = bkioOpen( 
                            pcontext,
                            pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex],
                            NULL, OPEN_SEQL,
                            namebuf, openFlags, DEFLT_PERM)) < 0)
            {
                MSGN_CALLBACK(pcontext, bkMSG002, namebuf, errno);
                return -1;
            }

            fd = bkioGetFd(pcontext, 
                           pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex],
                           mode);
            if (fd == INVALID_FILE_HANDLE)
            {
                MSGN_CALLBACK(pcontext, bkMSG115,namebuf);
                return -1;
            }

	    bkioSaveFd(pcontext,
                       pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex],
                       XTEXT(pdbcontext, pbkftbl->ft_qname), fd, mode);
    
	    break;
    
        case UNBUFIO:  /* file needs only an unbuffered fd */
    
             if ((openReturn = bkioOpen(
                           pcontext, 
                           pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                           NULL,  OPEN_RELIABLE, 
                           namebuf, openFlags, DEFLT_PERM)) < 0)
            {
                MSGN_CALLBACK(pcontext, bkMSG002, namebuf, errno);
                return -1;
            }

            fd = bkioGetFd(pcontext, 
                           pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex],
                           mode);
            if (fd == INVALID_FILE_HANDLE)
            {
                MSGN_CALLBACK(pcontext, bkMSG115,namebuf);
                return -1;
            }
 
	    bkioSaveFd(pcontext,
                       pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                       XTEXT(pdbcontext, pbkftbl->ft_qname), fd, mode);
    
	    break;
    
        case BOTHIO: /* file needs both a buffered and unbuffered fd */
    
            /* open the file buffered first  - OPEN_SEQL */
            if ((openReturn = bkioOpen(
                           pcontext, 
                          pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                          NULL, OPEN_SEQL,
                          namebuf,  openFlags, DEFLT_PERM)) < 0)
            {
                MSGN_CALLBACK(pcontext, bkMSG002, namebuf, errno);
                return -1;
            }

            fd = bkioGetFd(pcontext, 
                           pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex],
                           BUFIO);
            if (fd == INVALID_FILE_HANDLE)
            {
                MSGN_CALLBACK(pcontext, bkMSG115,namebuf);
                return -1;
            }
 
	    bkioSaveFd(pcontext, 
                       pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                       XTEXT(pdbcontext, pbkftbl->ft_qname), fd, BUFONLY);
    
            /* now open the file unbuffered - OPEN_RELIABLE */
            if ((openReturn = bkioOpen(
                           pcontext, 
                           pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                           NULL, OPEN_RELIABLE, 
                           namebuf,  openFlags, DEFLT_PERM)) < 0)
            {
                MSGN_CALLBACK(pcontext, bkMSG002, namebuf, errno);
                return -1;
            }

            fd = bkioGetFd(pcontext, 
                           pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex],
                           UNBUFIO);
            if (fd == INVALID_FILE_HANDLE)
            {
                MSGN_CALLBACK(pcontext, bkMSG115,namebuf);
                return -1;
            }
    
	    bkioSaveFd(pcontext, 
                       pdbcontext->pbkfdtbl->pbkiotbl[pbkftbl->ft_fdtblIndex], 
                       XTEXT(pdbcontext, pbkftbl->ft_qname), fd, UNBUFONLY);
    
	    break;
    
        default:
	    /* invalid mode specified */
	    MSGN_CALLBACK(pcontext, bkMSG065, mode);
	    return -1;
    }

    return 0;

}  /* end bkOpenOneFile */


/* PROGRAM: bkClose - close all the files in the look aside table
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkClose (dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    ULONG	 i;
    int		 failedSync = 0;
    int		 needSync = 1;

    if(!pcontext)
      return;
    if(!pdbcontext)
      return;
    if(!pdbcontext->pdbpub)
      return;
    if(!pdbcontext->pmstrblk)
      return;

    if (pdbcontext->pbkfdtbl == (bkfdtbl_t *)NULL)
    {
	/* Never got database open enough to open files --
	   for example a shared memory version mismatch error */
	return;
    }
    
    if ((pdbcontext->pdbpub) && (pdbcontext->pdbpub->shutdn == DB_EXBAD))
    {
        failedSync = 1;
        needSync = 0;
    }
    if(pdbcontext->pmstrblk->mb_dbstate != DBCLOSED)
    {
      /* At this point if we're going through a normal
	 shutdown the database closed flag should have been set.
	 If it isn't it means there is something wrong so we
	 want to stop the bi file from being truncated. */
      failedSync = 1;
      needSync = 0;
    }

    for (i = 0; i < pdbcontext->pbkfdtbl->maxEntries; i++)
    {
        if (bkioClose(pcontext, pdbcontext->pbkfdtbl->pbkiotbl[i],needSync))
        {
            /* syncing of the file descriptors failed, remeber this
               fact */
            failedSync = 1;
        }
    }
    stVacate(pcontext, pdbcontext->privpool,(TEXT *)pdbcontext->pbkfdtbl);
    pdbcontext->pbkfdtbl = (bkfdtbl_t *)NULL;

    if (!failedSync)
    {
        bkMarkDbSynced(pcontext);
    }

}  /* end bkClose */


/* PROGRAM: bkDbDemoVersion - check if this is the DEMO version
 *
 *  RETURNS: 0 if ok 1 if failure
 */
LOCALF int
bkDbDemoVersion(
	dsmContext_t	*pcontext,
	mstrblk_t	*pmstr)
{
    /* check if this is demo version of software */
    if (pcontext->pdbcontext->modrestr & DSM_MODULE_DEMOONLY)
    {
	/* demo software - make sure this is a demo database */
	if ((!(pmstr->mb_flag & UNDECIDED))  && (!(pmstr->mb_flag & DEMOVERS)))
	{
	    /* your database version doesn't match the PROGRESS version */
	    MSGN_CALLBACK(pcontext, bkMSG058);
	    return 1;
	}

	/* check the demo count */
	if (pmstr->mb_democt <= 0)
	{
	    /* You have exceeded the maximum allowed demo database runs. */
	    MSGN_CALLBACK(pcontext, bkMSG059);
	    return 1;
	}

	/* check if this the last time database can be used */
	if ((!pcontext->pdbcontext->argronly) && (pmstr->mb_democt == 1))
	{
	    /* This is the last time you can use this database. */
	    MSGN_CALLBACK(pcontext, bkMSG029);
	    /* Use the dictionary to dump any data you want to save */
	    MSGN_CALLBACK(pcontext, bkMSG030);
	}
	else
	{
	    /* You may use PROGRESS on this database %d more times */
	    MSGN_CALLBACK(pcontext, bkMSG031, pmstr->mb_democt - 1);
	}
    }
    else
    {
	/* production software - make sure this is not a demo database */
	if (pmstr->mb_flag & DEMOVERS)
	{
	    /* your database version doesn't match the PROGRESS version */
	    MSGN_CALLBACK(pcontext, bkMSG058);
	    return 1;
	}
    }

    return 0;

}  /* end bkDbDemoVersion */


/* PROGRAM: bkDbVersionCheck - check the db version number to ensure that the
 *          database is compatable with this version of PROGRESS
 *          or with a conversion process.
 *
 * RETURNS: 0 if ok, else -1 on error
 */
LOCALF int
bkDbVersionCheck (
	dsmContext_t	*pcontext,
	COUNT		 dbVersion)
{

    /* is this the current version ? */
    if (OKDBVERSN(dbVersion))
        return 0;

    /* invalid version database cannot be opened */
    MSGN_CALLBACK(pcontext, bkMSG003,
                  dbVersion, DBVERSN);

    return -1;

}  /* end bkDbVersionCheck */


/* *** BUM - this should be an fd entry point since it is called from sc layer
 *
 * PROGRAM: bkInvalidateIndexes - make indexes invalid on next startup
 *
 * This procedure marks the database as having invalid indexes (generally
 * caused by changing collation tables).  The invalidation forces a full
 * index rebuild of the database once the session ends, no other operation
 * is supported.
 *
 * RETURNS: 0 if OK
 *          1 if database is not in single user mode
 *          2 if database is already marked with invalid indexes
 */
int
bkInvalidateIndexes(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    /* operation is only supported in single user mode */
    if (!pdbcontext->arg1usr)
    {
        /* cannot invalidate indexes in multiuser mode */
        return 1;
    }

    /* check if bit is already set from some previous change */
    if (pdbcontext->pmstrblk->mb_tainted & DBIXBUILD)
    {
        return 2;
    }

    /* mark masterblock as tainted requiring full index rebuild */
    pdbcontext->pmstrblk->mb_tainted |= DBIXBUILD;

    /* flush the masterblock */
    bmFlushMasterblock(pcontext);

    /* all is well */
    return 0;

}  /* end bkInvalidateIndexes */


/* PROGRAM: bkGetDbBlockSize - retrieves the block size specified in the ext 
 *
 * RETURNS: database block size
 *          0 on error
 */
int
bkGetDbBlockSize(
	dsmContext_t	*pcontext,
	fileHandle_t    fd,         /* database fd */
	TEXT  *dbname)     /* full path name of .db file */
{
    extentBlock_t extentBlock;

    if (bkReadAmount(pcontext, dbname, fd, (gem_off_t)0,
                   sizeof(extentBlock_t), (TEXT *)&extentBlock) )
        return 0;  /* return error */
    BK_FROM_STORE(&extentBlock.bk_hdr);

    /* Check the db version here since it's early on in the startup sequence */
    if (bkDbVersionCheck(pcontext, extentBlock.dbVersion) == -1)
    {
        return 0;
    }

    return(extentBlock.blockSize);

}  /* end bkGetDbBlockSize */


/* PROGRAM: bkReadMasterBlock - open .db & read master block 
 *
 * It's up to the caller to:
 *     a. create the storage for the mstrblk.
 *     b. call dblk and dbunlk to manipulate the .lk file
 *
 * RETURNS: BK_RDMB_SUCCESS if ok,
 *          BK_RDMB_OPEN_FAILED if .db or .d1 can not be opened 
 *          BK_RDMB_READ_FAILED if read of .db or .d1 failed
 *          BK_RDMB_VERSION     if dbversion mismatch
 */
int
bkReadMasterBlock(
	dsmContext_t  *pcontext, 
        TEXT          *dbname,      /* full path name of .db file */
        mstrblk_t     *pmstr,       /* where to put real master block info */
        fileHandle_t    fd)          /* .db fd if available. */
{
    fileHandle_t   dbfd;

    TEXT        namebuf[MAXUTFLEN];
    extentBlock_t extentBlock;
    LONG        blockSize;
    TEXT       *ptheBlock;
    int         errorStatus;

/* 1. open the .db file.
 * 2. read the extent header to get the block size
 * 9. seek to the master block
 * 10. read the master block into the location passed in
 * 12. perform master block fix up
 */

/* The .d1 file location is stored in the control block (block 1)
 * of the control area.
 */
    if (fd == INVALID_FILE_HANDLE)
    {
        utfstrip(dbname, (TEXT *) ".db");
        utmypath(namebuf,pcontext->pdbcontext->pdatadir,
                 dbname,(TEXT *) ".db");
        /* open the .db file */
        dbfd = utOsOpen (namebuf, OPEN_R, DEFLT_PERM,
                         OPEN_SEQL, &errorStatus);
        if (dbfd == INVALID_FILE_HANDLE)
        {
            MSGN_CALLBACK(pcontext, bkMSG002, namebuf, errorStatus);
            return BK_RDMB_OPEN_FAILED;
        }
    }
    else
    {
        dbfd = fd;
    }

    if (bkReadAmount(pcontext, namebuf, dbfd, (gem_off_t)0,
                   sizeof(extentBlock_t), (TEXT *)&extentBlock) )
        return BK_RDMB_READ_FAILED;  /* return error */

    BK_FROM_STORE(&extentBlock.bk_hdr);

    if (bkDbVersionCheck (pcontext, extentBlock.dbVersion) == -1)
        return BK_RDMB_BAD_VERSION;  /* return error */

    blockSize = extentBlock.blockSize;

    ptheBlock = utmalloc(blockSize);
    if (!ptheBlock)
    {
        MSGN_CALLBACK(pcontext, bkMSG069);
        return -1;
    }

    /* NOTE: Must read in block size units to support raw devices. */
    /* now seek on the real multi-volume master block (block 1 of .d1) */
    if (bkReadAmount(pcontext, namebuf, dbfd, (gem_off_t)blockSize,
                   blockSize, ptheBlock) )
        return BK_RDMB_READ_FAILED;

    bufcop(pmstr, ptheBlock, sizeof(mstrblk_t));
    utfree(ptheBlock);

    BK_TO_STORE(&pmstr->bk_hdr);

        /* Only close the dbfd if it was opened here */
    if (fd == INVALID_FILE_HANDLE)
        utOsClose (dbfd, OPEN_SEQL);


    return BK_RDMB_SUCCESS;

}  /* end bkReadMasterBlock */

/* PROGRAM: bkWriteMasterBlock - open .db & write master block 
 *
 * It's up to the caller to:
 *     a. create the storage for the mstrblk.
 *     b. call dblk and dbunlk to manipulate the .lk file
 *
 * RETURNS: BK_WRMB_SUCCESS if ok,
 *          BK_WRMB_OPEN_FAILED if .db or .d1 can not be opened 
 *          BK_WRMB_WRITE_FAILED if read of .db or .d1 failed
 *          BK_WRMB_VERSION     if dbversion mismatch
 */
int
bkWriteMasterBlock(
	dsmContext_t  *pcontext, 
        TEXT          *dbname,      /* full path name of .db file */
        mstrblk_t     *pmstr,       /* where to put real master block info */
        fileHandle_t   fd)          /* .db fd if available. */
{
    fileHandle_t   dbfd;

    TEXT           namebuf[MAXUTFLEN];
    extentBlock_t  extentBlock;
    gem_off_t      blockSize;
    TEXT          *ptheBlock = 0;
    int            errorStatus;
    int            bytesWritten;            /* IN/OUT for utWriteToFile() */
    LONG           retStatus;               /* return code utWriteToFile() */
    int            returnCode;

    if (fd == INVALID_FILE_HANDLE)
    {
      stnclr(namebuf, MAXUTFLEN);
      if(pcontext->pdbcontext->pdatadir)
	  utmypath(namebuf,pcontext->pdbcontext->pdatadir,
                   pcontext->pdbcontext->pdbname,(TEXT *)".db");
      else
      {
        utfstrip(dbname, (TEXT *) ".db");
        utapath(namebuf, sizeof(namebuf), dbname, (TEXT *) ".db");
      }
      /* open the .db file */
      dbfd = utOsOpen (namebuf, OPEN_RW, DEFLT_PERM,
                       OPEN_SEQL, &errorStatus);
    }
    else
    {
      dbfd = fd;
    }

    if (dbfd == INVALID_FILE_HANDLE)
    {
        MSGN_CALLBACK(pcontext, bkMSG002, namebuf, errorStatus);
        return BK_WRMB_OPEN_FAILED;
    }

    if (bkReadAmount(pcontext, namebuf, dbfd, (gem_off_t)0,
                   sizeof(extentBlock_t), (TEXT *)&extentBlock) )
        return BK_WRMB_WRITE_FAILED;  /* return error */

    
    blockSize = STORE_TO_LONG((TEXT *)&extentBlock.blockSize);

    ptheBlock = utmalloc(blockSize);
    if (!ptheBlock)
    {
        MSGN_CALLBACK(pcontext, bkMSG069);
        returnCode = -1;
        goto done;
    }

    stnclr(ptheBlock,blockSize);
    bufcop(ptheBlock, pmstr, sizeof(mstrblk_t));

    BK_TO_STORE((bkhdr_t *)ptheBlock);

    /* NOTE: Must write in block size units to support raw devices. */

    retStatus = utWriteToFile(dbfd, blockSize, (TEXT *)ptheBlock, blockSize,
                                  &bytesWritten, &errorStatus);
    if (bytesWritten != blockSize)
    {
        /* Failed writing to mstrblk */
        MSGN_CALLBACK(pcontext, bkMSG116);
        returnCode = BK_WRMB_WRITE_FAILED;
        goto done;
    }

    returnCode = BK_WRMB_SUCCESS;

done:
    if (ptheBlock)
        utfree(ptheBlock);

    /* Only close the dbfd if it was opened here */
    if (fd == INVALID_FILE_HANDLE)
    {
        utOsClose (dbfd, OPEN_SEQL);
    }

    return returnCode;

}  /* end bkWriteMasterBlock */


/* PROGRAM: bkLoadFileTable - Allocates and initializes the file table.
 *                         Uses the file list if the file type
 *      		   is a set of extents.
 *
 * GLOBAL dependencies:   pdbpub->qdbpool 
 *                        pdbpub->qainame
 *		          pdbpub->qbiname
 *		          pdbpub->qdbname      
 *
 * RETURNS: bkftbl struture on success.
 *          NULL on failure.
 */
LOCALF bkAreaDesc_t *
bkLoadFileTable(
	dsmContext_t	*pcontext,
	int             nExtents,
	bkflst_t       *pFileList)     
{
    dbcontext_t      *pdbcontext = pcontext->pdbcontext;
    dbshm_t      *pdbpub = pdbcontext->pdbpub;
    bkAreaDesc_t *pbkAreaDesc;
    bkftbl_t     *pFile;
    int           i;
    TEXT         *pName;

    pbkAreaDesc = (bkAreaDesc_t *)stRentIt(pcontext,
                                          XSTPOOL(pdbcontext, pdbpub->qdbpool),
			BK_AREA_DESC_SIZE(nExtents + 1), 0);
    
    if ( pbkAreaDesc == NULL )
    {
	/* %BbkLoadFileTable: stGet allocation failure */
	MSGN_CALLBACK(pcontext, bkMSG068);
	return( pbkAreaDesc );
    }
    
    pbkAreaDesc->bkNumFtbl = nExtents;
    pFile = pbkAreaDesc->bkftbl;

    for ( i = 0; i < nExtents; i++ )
    {
        pName = (TEXT *)stRentIt(pcontext,
                                 XSTPOOL(pdbcontext, pdbpub->qdbpool),
                                 stlen((pFileList + i)->fl_name) + 1, 0);
 
        if ( pName == NULL )
        {
	    /* %BbkLoadFileTable: stGet allocation failure */
	    MSGN_CALLBACK(pcontext, bkMSG068);
	    return( NULL );
        }

	pFile->ft_type = (pFileList + i)->fl_type;
/* Needs to be 0 for temp tables? */
	pFile->ft_qname = P_TO_QP(pcontext, pName);
	stcopy(pName, (pFileList + i)->fl_name);
	if( pFile->ft_type & BKFIXED )
	    pFile->ft_curlen = (gem_off_t)((pFileList + i)->fl_maxlen) * FLUNITS;
	pFile->qself = P_TO_QP(pcontext, pFile);
	pFile->ft_fdtblIndex = -1;
	pFile++;
    }

    return( pbkAreaDesc );
}  /* end bkLoadFileTable */

    
/* PROGRAM: bkOpenFilesBuffered - Open for reading each
 *                             file in the file table
 *      		      as controlled by the numFiles
 *      		      structure.
 *      		      If not read only then the file
 *      		      is also opened for "unreliable" write.
 *
 * RETURNS: 0 on success.
 *          < 0 on failure.
 */
int
bkOpenFilesBuffered(
	dsmContext_t	*pcontext,
	GTBOOL		 readOnly,
	bkAreaDesc_t	*pbkAreaDesc,
        mstrblk_t       *pmstr)
{
    dbcontext_t	 *pdbcontext = pcontext->pdbcontext;
    dbshm_t      *pdbpub = pdbcontext->pdbpub;
    bkftbl_t	 *pbkFile;
    ULONG         j;
    int           fileOpenMode;
    fileHandle_t   fd;
    LONG	  fioflags = 0;
    LONG          openReturn;

    if (readOnly)
    {
	fileOpenMode = OPEN_R;
    }
    else
    {
	fileOpenMode = OPEN_RW;
    }


    pbkFile = pbkAreaDesc->bkftbl;
    for( j = 0; j < pbkAreaDesc->bkNumFtbl; j++)
    {
       if( pbkFile == NULL )
       {
	  /* %GbkOpenFilesBuffered: Invalid file table */
	  FATAL_MSGN_CALLBACK(pcontext, bkFTL032);
       }
       
       /* if the files has not been opened yet (unlike the .db in
	  single volume database), don't open it again */
       if( pbkFile->ft_fdtblIndex == -1 )
       {
	  /* get an index into the iotbl for I/O operations */
          pbkFile->ft_fdtblIndex = bkInitIoTable(pcontext, 1);
          /* No matter what open the tl for buffered io.   */
          if (pdbpub->directio)
          {
              fioflags |= OPEN_RELIABLE;
          }
          else
          {
              fioflags |= OPEN_SEQL;
          }

          openReturn = bkioOpen( pcontext, 
                    pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                    NULL, (gem_off_t)fioflags,
                    XTEXT(pdbcontext, pbkFile->ft_qname), fileOpenMode, 0);

	  if( openReturn < 0 )
	  {
	     MSGN_CALLBACK(pcontext, bkMSG002,
                           XTEXT(pdbcontext, pbkFile->ft_qname),errno);
	     if ((pbkAreaDesc->bkAreaType == DSMAREA_TYPE_BI)
                 && ((pdbcontext->confirmforce || pmstr->mb_dbstate == DBCLOSED)))
             {
                 /* if there is no BI file, then create one and retry the 
                    open */
                 ULONG    initSize = 15;
                 pbkFile->ft_blksize = pbkAreaDesc->bkBlockSize;
                 pbkFile->ft_blklog = bkblklog(pbkFile->ft_blksize);
                 openReturn = bkExtentCreateFile(pcontext, pbkFile, &initSize,
                                 1, pbkAreaDesc->bkAreaRecbits, 
                                 pbkAreaDesc->bkAreaType);

                 openReturn = bkioOpen( pcontext, 
                      pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                      NULL, (gem_off_t)fioflags,
                      XTEXT(pdbcontext, pbkFile->ft_qname), fileOpenMode, 0);
                 if (openReturn < 0)
                 {
                     /* not much else we can do here */
                     return openReturn;   
                 }
                 else
                 break;
             }
	     else if( pbkAreaDesc->bkAreaType == DSMAREA_TYPE_AI )
	     {
	        /* Don't stop for bad ai file opens    */
		pbkAreaDesc->bkNumFtbl = 0;
                /* Give back the io table slot.   */
                pdbcontext->pbkfdtbl->numused--;
		break;
	     }
             else if (pbkAreaDesc->bkAreaType == DSMAREA_TYPE_TL )
             {
                 pbkAreaDesc->bkNumFtbl = 0;
                 pdbcontext->pbkfdtbl->numused--;
                 break;
             }
	     else
		return openReturn;
	  }
	  
          fd = bkioGetFd(pcontext, 
                         pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                         BUFIO);
          if (fd == INVALID_FILE_HANDLE)
          {
              MSGN_CALLBACK(pcontext, bkMSG115,
                            XTEXT(pdbcontext, pbkFile->ft_qname));
              return -1;
          }

          bkioSaveFd(pcontext, 
                     pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                     XTEXT(pdbcontext, pbkFile->ft_qname), fd, BUFIO);

       }
       else
       {
	  /* file is already opened, make sure pointers are linked
	     up properly */
	  fd = bkioGetFd(pcontext, 
                         pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                         BUFIO);
	  if (fd == INVALID_FILE_HANDLE)
          {
              MSGN_CALLBACK(pcontext, bkMSG115,
                            XTEXT(pdbcontext, pbkFile->ft_qname));
              return -1;
          }


          /* put the BUFFERED file descriptor into the table */
          bkioSaveFd(pcontext,
                     pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
		  XTEXT(pdbcontext, pbkFile->ft_qname), fd, BUFIO);
       }
       
       /* set the ft_openmode flag to buffered so the 2nd user
	  will know how to open this file  */
       pbkFile->ft_openmode = BUFIO;
       
       pbkFile++;
    }
    
    
    return 0;

}  /* end bkOpenFilesBuffered */


/* PROGRAM: bkVerifyExtentHeader - Checks time stamps in the file
 *                               extent headers against those
 *                               in the master block.
 *                               This catches errors where an extent
 *      			 has been copied to the wrong place.
 * RETURNS: 0 on success.
 *           DB_EXBAD on failure.
 */
LOCALF int
bkVerifyExtentHeader(
	dsmContext_t	*pcontext, 
	extentBlock_t	*pextentBlock,
	bkAreaDesc_t	*pbkAreaDesc)     /* The datbase file table  */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    ULONG   j;
    int     rc = 0;
    int     ret = 0;
    TEXT    timeString[TIMESIZE];

    /* Assign temporary value from the control area's dates */
    ULONG         creationDate =
                  pextentBlock->creationDate[pextentBlock->dateVerification];
    ULONG         lastOpenDate =
                  pextentBlock->lastOpenDate[pextentBlock->dateVerification];
    COUNT         dateVerification =
                  pextentBlock->dateVerification;

    TEXT      *ptheBlock;  /* block read from disk */
    extentBlock_t  *pextentRead;
    bkiotbl_t *pbkiotbl;
    bkftbl_t  *pbkFile;
    int        blockSize = pextentBlock->blockSize;

    if (pbkAreaDesc->bkDelete)
        return 0;

    pbkFile = pbkAreaDesc->bkftbl;
	
    if( pbkFile == NULL )
        return ret;
    
    if ( pbkFile->ft_type & BKSINGLE)
       /* Not an extent */
       return ret;

    ptheBlock = utmalloc(blockSize);
    if (!ptheBlock)
    {
        /* We failed to get storage for pmstr */
        MSGN_CALLBACK(pcontext, stMSG006);
        return DB_EXBAD;
    }

    /* We will allocate the extent block to read in the size of the .db.
     * It won't matter if the if the actual extent blocksize is > this
     * because the extent_t is always smaller than the smallest block size.
     * We must read in blocks to suppport raw devices.
     */
    for( j = 0;
         j < pbkAreaDesc->bkNumFtbl && rc == 0;
         j++, pbkFile++)
    {
       if( pbkAreaDesc->bkAreaType == DSMAREA_TYPE_AI && rlaiqon(pcontext) != 1)
	  /* Don't check ai extents if ai is not enabled */
	  break;
       
       if( pbkFile == NULL || pbkFile->ft_type == 0 )
       {
	  /* %GbkVerifyExtentHeader: Invalid file table */
	  FATAL_MSGN_CALLBACK(pcontext,  bkFTL035 );
       }
       
       pbkiotbl = pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex];
       ret = bkReadAmount(pcontext, XTEXT(pdbcontext, pbkFile->ft_qname),
			pbkiotbl->buffered_filePointer, 
			(gem_off_t)0,
                        blockSize, ptheBlock);
       if( ret < 0 )
       {
	  /* Error reading extent header ret = %d, file = %s */
	  MSGN_CALLBACK(pcontext, bkMSG056,
                        ret, XTEXT(pdbcontext, pbkFile->ft_qname));
	  rc = DB_EXBAD;
	  continue;
       }

       pextentRead = (extentBlock_t *)ptheBlock;
       BK_FROM_STORE(&pextentRead->bk_hdr);
       /* ensure this extent's creationDate matches control area's */
       if ( creationDate != pextentRead->creationDate[dateVerification] )
       { 
	  MSGN_CALLBACK(pcontext, bkMSG150);
	  MSGN_CALLBACK(pcontext, bkMSG148,
	       XTEXT(pdbcontext, pbkFile->ft_qname), 
               uttime((time_t *)&pextentRead->creationDate[dateVerification], 
               timeString, sizeof(timeString))); /* creation date mismatch */
          MSGN_CALLBACK(pcontext, bkMSG152,  
               uttime((time_t *)&creationDate, timeString, sizeof(timeString)));
	  MSGN_CALLBACK(pcontext, bkMSG017); /* probable backup/restore error */
	  rc = DB_EXBAD;
       }
       /* ensure this extent's lastOpenDate matches control area's */
       else if ( lastOpenDate != pextentRead->lastOpenDate[dateVerification] )
       {
	  MSGN_CALLBACK(pcontext, bkMSG151);
	  MSGN_CALLBACK(pcontext, bkMSG149,
	       XTEXT(pdbcontext, pbkFile->ft_qname), 
               uttime((time_t *)&pextentRead->lastOpenDate[dateVerification], 
               timeString, sizeof(timeString))); /* open date mismatch */
          MSGN_CALLBACK(pcontext, bkMSG153,  
               uttime((time_t *)&lastOpenDate, timeString, sizeof(timeString)));
	  MSGN_CALLBACK(pcontext, bkMSG017); /* probable backup/restore error */
	  MSGN_CALLBACK(pcontext, bkMSG013); /* use prostrct unlock */
	  rc = DB_EXBAD;
       }
       
       if( rc == DB_EXBAD && pbkAreaDesc->bkAreaType == DSMAREA_TYPE_AI )
       {
	  /* Set number of ai files to zero but don't set
	     error return.  It will be decided by the caller
	     if they can continue without ai file(s)        */
	  rc = 0;
	  pbkAreaDesc->bkNumFtbl = 0;
       }

    }

    utfree(ptheBlock);
    
    return rc;

}  /* end bkVerifyExtentHeader */


/* PROGRAM: bkSetFiles - Initilializes the file table for
 *                       file size, block size, logical
 *      		 beginning of file and physical
 *      		 end of file.
 *
 * RETURNS: 0 on success.
 *          DB_EXBAD on failure.
 */
LOCALF int
bkSetFiles(
	dsmContext_t   *pcontext,
	bkAreaDesc_t   *pbkAreaDesc,
        mstrblk_t      *pmstr)
{
    dbcontext_t           *pdbcontext = pcontext->pdbcontext;
    int                rc;
    ULONG              j;
    ULONG              blockSize = pbkAreaDesc->bkBlockSize;
    bkftbl_t          *pbkFile;
    ULONG              bkfilesize;

    pbkFile = pbkAreaDesc->bkftbl;
	
    for( j = 0; j < pbkAreaDesc->bkNumFtbl; j++, pbkFile++)
    {
       /* set offset as zero to set physical sizes */
       pbkFile->ft_offset = 0L;
       
       /* set the physical block size */
       pbkFile->ft_blksize = blockSize;
       
       /* set constant to perform block/byte conversions */
       pbkFile->ft_blklog = bkblklog(pbkFile->ft_blksize);

       rc = bkioFileInBlocks(XTEXT(pdbcontext, pbkFile->ft_qname),
                             &bkfilesize, pbkFile->ft_blklog);

       /* set physical length of the file */
       if( pbkFile->ft_type & BKPART )
       {
	  /* *** TODO: read last block to verify size */
          ;
       }
       else if( pbkFile->ft_type & BKFIXED )
       {
	  /* fixed size file check value already set */
	  if( pbkFile->ft_curlen > ((gem_off_t)bkfilesize << pbkFile->ft_blklog )) 
	  {
	     /* actual extent size is below expected size */
	     FATAL_MSGN_CALLBACK(pcontext, bkFTL024,
                   XTEXT(pdbcontext, pbkFile->ft_qname), pbkFile->ft_curlen);
	  }
       }
       else
       {
	  pbkFile->ft_curlen = (gem_off_t)bkfilesize << pbkFile->ft_blklog; 
	  
	  /* set extend amount for variable length files */
	  if( pbkFile->ft_type & BKDATA )
	  {
	     pbkFile->ft_xtndamt = 16;
	  }
	  else
	  {
	     pbkFile->ft_xtndamt = 64;
	  }
       }
       
       /* make sure file includes at least full block for header */
       if ((!pbkAreaDesc->bkDelete) && (pbkFile->ft_curlen < pbkFile->ft_blksize))
       {
           if ((pmstr->mb_dbstate == DBCLOSED) || (pdbcontext->confirmforce))
           {
               ULONG    initSize = 15;
	     
               /* current file is smaller than 1 block.  This means there
                  is no extent header.  Remove the old file and create a
                  new one */
                if (pmstr->mb_dbstate != DBCLOSED)
                {
                    MSGN_CALLBACK(pcontext, bkMSG155, 
                        XTEXT(pdbcontext, pbkFile->ft_qname));
                }

               /* first close the file */
               rc = bkioClose(pcontext, 
                       pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                       0);
           
               /* unlink the file */
               unlink((char *)XTEXT(pdbcontext, pbkFile->ft_qname));

               /* create a new file */
               rc = bkExtentCreateFile(pcontext, pbkFile, &initSize, 1,
                        pbkAreaDesc->bkAreaRecbits, pbkAreaDesc->bkAreaType);

               /* reopen the file */
               rc = bkOpenOneFile(pcontext, pbkFile, OPEN_RW);

               /* get the new size of the file */
               rc = bkioFileInBlocks(XTEXT(pdbcontext, pbkFile->ft_qname),
                                     &bkfilesize, pbkFile->ft_blklog);

	       pbkFile->ft_curlen = (gem_off_t)bkfilesize << pbkFile->ft_blklog; 
           }

	   /* if its not ok now, then we cannot continue */
	   if ((!pbkAreaDesc->bkDelete) && (pbkFile->ft_curlen < pbkFile->ft_blksize))
	   {
		/* File too small to continue with blocksize */
                MSGN_CALLBACK(pcontext, bkMSG156,
                    XTEXT(pdbcontext, pbkFile->ft_qname), pbkFile->ft_blksize);
                return -1;
	   }
	     
        } /* partial block */
	  
	/* reset offset to account for extent header */
	pbkFile->ft_offset = pbkFile->ft_blksize;
	  
	/* reset physical size to logical size of file */
	pbkFile->ft_curlen -= pbkFile->ft_offset;
	  
    } /* loop each file */

    return 0;

}  /* end bkSetFiles */


/* PROGRAM: bkOpenFilesUnBuffered - Open database files for reliable write
 *
 * RETURNS: 0 on success or failure.  
 * BUM This function fatals if it errors!!!
 */
int
bkOpenFilesUnBuffered(
	dsmContext_t	*pcontext,
	bkAreaDesc_t	*pbkAreaDesc)
{
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    dbshm_t  *pdbpub = pdbcontext->pdbpub;
    ULONG  i;
    bkftbl_t *pbkFile;
    fileHandle_t       syncfd;
    LONG   openReturn;

    if (pbkAreaDesc->bkDelete)
        return 0;

    if (pdbpub->directio)
    {
        /* all files need to be opened directio, so close them all then
           reopen them */

        pbkFile = pbkAreaDesc->bkftbl;
	if ( pbkAreaDesc->bkAreaType == DSMAREA_TYPE_DATA )
	{
            for( i = 0; i < pbkAreaDesc->bkNumFtbl; i++, pbkFile++)
	    {
	       /* Close the buffered file handle                       */
	       bkioClose(pcontext,
                         pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
			 0);
	       
	       /* open the file to get the REAL file descriptor */
               openReturn = bkioOpen( pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        NULL, OPEN_RELIABLE,
                        XTEXT(pdbcontext, pbkFile->ft_qname),
                              OPEN_RW,DEFLT_PERM);

               if ( openReturn < 0 )
               {
                   MSGN_CALLBACK(pcontext, bkMSG002,
                                 XTEXT(pdbcontext, pbkFile->ft_qname), errno);
                   return openReturn;
               }

               syncfd = bkioGetFd(pcontext,
                         pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                         UNBUFIO);

	       bkioSaveFd(pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        XTEXT(pdbcontext, pbkFile->ft_qname), syncfd, UNBUFIO);
	       
	       /* set the ft_openmode flag to UNBUFIO so the 2nd user
		  will know how to open this file. This file has just an 
		  UNBUFFERED fd on it  */
	       
	       pbkFile->ft_openmode = UNBUFIO;
            }
	}
        else if ( pbkAreaDesc->bkAreaType == DSMAREA_TYPE_BI)
	{
            for( i = 0; i < pbkAreaDesc->bkNumFtbl; i++, pbkFile++)
	    {
	       /* Close the buffered file handle                       */
	       bkioClose(pcontext,
                         pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
			 0);
	       
	       /* open the file to get the REAL file descriptor */
               openReturn = bkioOpen(pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        NULL,OPEN_RELIABLE, 
                        XTEXT(pdbcontext, pbkFile->ft_qname),
                              OPEN_RW,DEFLT_PERM);

               if ( openReturn < 0 )
               {
                   MSGN_CALLBACK(pcontext, bkMSG002,
                                 XTEXT(pdbcontext, pbkFile->ft_qname), errno);
                   return openReturn;

               }

	        
               /* retrieve the just opened fd to set up the bkiotbl right */
               syncfd = bkioGetFd(pcontext,
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                         UNBUFIO);

	       bkioSaveFd(pcontext, 
                       pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
			XTEXT(pdbcontext, pbkFile->ft_qname), syncfd, UNBUFIO);
	       /* set the ft_openmode flag to UNBUFIO so the 2nd user
		  will know how to open this file. This file has just an 
		  UNBUFFERED fd on it  */
	       
	       pbkFile->ft_openmode = UNBUFIO;
	    }
	}
	else if ( pbkAreaDesc->bkAreaType == DSMAREA_TYPE_AI )
	{
	   for( i = 0; i < pbkAreaDesc->bkNumFtbl; i++, pbkFile++)
	   {
	      /* Close the buffered file handle                       */
	      bkioClose(pcontext,
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
			0);

              /* open the file to get the REAL file descriptor */
              openReturn = bkioOpen( pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        NULL, OPEN_RELIABLE,
                        XTEXT(pdbcontext, pbkFile->ft_qname),
                              OPEN_RW, DEFLT_PERM);

              if ( openReturn < 0 )
              {
                  MSGN_CALLBACK(pcontext, bkMSG002,
                                XTEXT(pdbcontext, pbkFile->ft_qname), errno);
                  return openReturn;
              }

              syncfd = bkioGetFd(pcontext,
                       pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                       UNBUFIO);

	      
	      bkioSaveFd(pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        XTEXT(pdbcontext, pbkFile->ft_qname), syncfd, UNBUFIO);
	      
	      /* set the ft_openmode flag to UNBUFIO so the 2nd user
		 will know how to open this file. This file has just an 
		 UNBUFFERED fd on it  */
	      
	      pbkFile->ft_openmode = UNBUFIO;
	   }
	}
    }
    else
    {
        if ( !pdbpub->useraw || pdbcontext->argronly )
	    /* If -i or -r is set then reliable write is not needed */
	    return 0;

	if( pbkAreaDesc->bkAreaType == DSMAREA_TYPE_DATA )
	{
            /* Open first and last data extents for synchronous write
             int the schema area.  First for masterblock flush and
             last for extenting the area.  For user areas only need to
             open the last extent for synchronous io. */
            if( pbkAreaDesc->bkNumFtbl == 1 ||
                pbkAreaDesc->bkAreaNumber == DSMAREA_SCHEMA )
	    {
	       pbkFile = pbkAreaDesc->bkftbl;
               openReturn = bkioOpen( pcontext, 
                       pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        NULL, OPEN_RELIABLE,
                        XTEXT(pdbcontext, pbkFile->ft_qname),
                              OPEN_RW, DEFLT_PERM);

               if ( openReturn < 0 )
               {
                   MSGN_CALLBACK(pcontext, bkMSG002,
                               XTEXT(pdbcontext, pbkFile->ft_qname), errno);
                   return openReturn;
               }

               syncfd = bkioGetFd(pcontext,
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                         UNBUFIO);
	       
	       bkioSaveFd(pcontext, 
                       pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                      XTEXT(pdbcontext, pbkFile->ft_qname), syncfd, UNBUFONLY);
	       if(pbkAreaDesc->bkAreaNumber == DSMAREA_SCHEMA)
	       {
		 /* Save unbuffered file descriptor in the buffered slot
		    for the schema area so that all io to the schema
		    area will be synchronous.                         */
		 bkioSaveFd(pcontext, 
                      pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                     XTEXT(pdbcontext, pbkFile->ft_qname), syncfd, BUFIO); 
	       }
	       /* set the ft_openmode flag to BOTHIO so the 2nd user
		  will know how to open this file. This file has both an 
		  UNBUFFERED as well as a BUFFERED fd on it  */
	       pbkFile->ft_openmode = BOTHIO;
	    }
    
	    /* now opening the last extent */
	    if( pbkAreaDesc->bkNumFtbl > 1 )
	    {
	       pbkFile = pbkAreaDesc->bkftbl + (pbkAreaDesc->bkNumFtbl - 1);
	       
	       openReturn = bkioOpen( pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        NULL, OPEN_RELIABLE,
                        XTEXT(pdbcontext, pbkFile->ft_qname),
                              OPEN_RW, DEFLT_PERM);

                if ( openReturn < 0 )
                {
                    MSGN_CALLBACK(pcontext, bkMSG002,
                                  XTEXT(pdbcontext, pbkFile->ft_qname), errno);
                    return openReturn;
                }

               syncfd = bkioGetFd(pcontext,
                         pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                         UNBUFIO);
 
	       bkioSaveFd(pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        XTEXT(pdbcontext, pbkFile->ft_qname),
                              syncfd, UNBUFONLY);
	       
	       pbkFile->ft_openmode = BOTHIO;
	    }
        }
	else if (pbkAreaDesc->bkAreaType == DSMAREA_TYPE_BI )
	{
            /* Now open the bi files for reliable write                    */
            pbkFile = pbkAreaDesc->bkftbl;

	    for( i = 0; i < pbkAreaDesc->bkNumFtbl; i++, pbkFile++)
	    {
	       /* Close the buffered file handle                       */
	       bkioClose(pcontext,
                         pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
			 0);
	       
               /* open the file to get the REAL file descriptor */
               openReturn = bkioOpen( pcontext, 
                         pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                         NULL, OPEN_RELIABLE,
                         XTEXT(pdbcontext, pbkFile->ft_qname),
                               OPEN_RW, DEFLT_PERM);

               if ( openReturn < 0 )
               {
                   MSGN_CALLBACK(pcontext, bkMSG002,
                                 XTEXT(pdbcontext, pbkFile->ft_qname), errno);
                   return openReturn;
               }

               syncfd = bkioGetFd(pcontext,
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                        UNBUFIO);
 
	       bkioSaveFd(pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        XTEXT(pdbcontext, pbkFile->ft_qname), syncfd, UNBUFIO);
	       
	       /* set the ft_openmode flag to UNBUFIO so the 2nd user
		  will know how to open this file. This file has just an 
		  UNBUFFERED fd on it  */
	       
	       pbkFile->ft_openmode = UNBUFIO;
	       
	    }
	}
	else if ( pbkAreaDesc->bkAreaType == DSMAREA_TYPE_AI )
	{
            /* Open the ai files for unbuffered if necessary             */

	   if( pdbcontext->pmstrblk->mb_aisync == UNBUFFERED )
	   {
	      pbkFile = pbkAreaDesc->bkftbl;
	      
	      for( i = 0; i < pbkAreaDesc->bkNumFtbl; i++, pbkFile++)
	      {
		 /* Close the buffered file handle                       */
		 bkioClose( pcontext,
                            pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
			    0);
		 
                 /* open the file to get the REAL file descriptor */
                 openReturn = bkioOpen( pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        NULL, OPEN_RELIABLE,
                        XTEXT(pdbcontext, pbkFile->ft_qname),
                              OPEN_RW, DEFLT_PERM);

                 if ( openReturn < 0 )
                 {
                     MSGN_CALLBACK(pcontext, bkMSG002,
                                   XTEXT(pdbcontext, pbkFile->ft_qname), errno);
                     return openReturn;
                 }

                 syncfd = bkioGetFd(pcontext,
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
                        UNBUFIO);
		 
		 bkioSaveFd(pcontext, 
                        pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
                        XTEXT(pdbcontext, pbkFile->ft_qname), syncfd, UNBUFIO);
		 
		 /* set the ft_openmode flag to UNBUFIO so the 2nd user
		    will know how to open this file. This file has just an 
		    UNBUFFERED fd on it  */
		 
		 pbkFile->ft_openmode = UNBUFIO;
		 
	      }
	   }
        }
    }

    return 0;

}  /* end bkOpenFilesUnBuffered */


/* PROGRAM: bkUpdateExtentHeaders - Update the db open time stamp
 *                                  in the extent header blocks       
 *
 * RETURNS: 0 on success.
 *          DB_EXBAD on failure.
 */
LOCALF int
bkUpdateExtentHeaders(
	dsmContext_t	*pcontext, 
	LONG             openTime,  /* new open time */
        COUNT	 	 newDate,   /* data array index */
	bkAreaDescPtr_t *pbkAreaDescPtr)
{
    dbcontext_t       *pdbcontext = pcontext->pdbcontext;
    bkAreaDesc_t  *pbkAreaDesc;
    extentBlock_t *pextentBlock = (extentBlock_t *)0;
    ULONG       j;
    dsmStatus_t returnCode = DB_EXBAD;
    dsmStatus_t rc         = 0;
    LONG        blklog;
    bkftbl_t   *pbkFile;
    ULONG       currArea;
    ULONG       largestBlockSize = 0;

    /* traverse all areas in this database in reverse order.
     * NOTE: the CONTROL area MUST be processed last for date consistancy. 
     */
    for (currArea=pbkAreaDescPtr->bkNumAreaSlots;
         currArea > 0;
         currArea--)
    {
       /* get this area's description */
       pbkAreaDesc = XBKAREAD(pdbcontext, pbkAreaDescPtr->bk_qAreaDesc[currArea-1]);
       if (!pbkAreaDesc)
           continue;  /* no area in this slot */

       if (!(pbkFile = pbkAreaDesc->bkftbl) )
           continue;  /* no extents in this area */

       if (!(blklog = pbkAreaDesc->bkftbl->ft_blklog) )
           continue;  /* Is actually an error? */

       if (pbkFile->ft_type & BKSINGLE)
           continue;  /* no extents */

       /* if blocksize of current area is larger than any previously
        * allocated ten reallocate the pextentBlock to be of the
        * larger size.
        */
       if (pbkAreaDesc->bkBlockSize > largestBlockSize)
       {
           if (pextentBlock)
               utfree((TEXT *)pextentBlock);

           pextentBlock = (extentBlock_t *) utmalloc(pbkAreaDesc->bkBlockSize);
           if (!pextentBlock)
           {
 	       /* %BbkUpdateExtentHeaders: utmalloc allocation failure */
	       MSGN_CALLBACK(pcontext, bkMSG069);
               returnCode = DB_EXBAD;
               goto done;
           }
           largestBlockSize = pbkAreaDesc->bkBlockSize;
       }

       /* Update each extent for this area */
       for( j = 0; j < pbkAreaDesc->bkNumFtbl; j++, pbkFile++)
       {
          if (pbkAreaDesc->bkDelete)
              continue;

          rc = bkioRead (pcontext,
                      pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex],
 		      &pbkFile->ft_bkiostats, (LONG) 0, 
		      (TEXT *)pextentBlock, 1, 
		      blklog, BUFFERED);
          if( rc != 1 )
          {
 	     /* %BUnable to read extent header errno %d file = %s */
	     MSGN_CALLBACK(pcontext, bkMSG071,
                           errno, XTEXT(pdbcontext, pbkFile->ft_qname));
	     goto done;
          }
	  BK_FROM_STORE(&pextentBlock->bk_hdr);
          /* If this is the control area, swap the data verification value.
           * NOTE: the control area MUST be the last area updated.
           */
          if (pbkAreaDesc->bkAreaNumber == DSMAREA_CONTROL)
              pextentBlock->dateVerification = newDate;

          pextentBlock->lastOpenDate[newDate] = openTime;
          pextentBlock->creationDate[newDate] = openTime;
	  BK_TO_STORE(&pextentBlock->bk_hdr);
          rc = bkioWrite(pcontext,
                      pdbcontext->pbkfdtbl->pbkiotbl[pbkFile->ft_fdtblIndex], 
 		      &pbkFile->ft_bkiostats, (LONG) 0, 
		      (TEXT *)pextentBlock, 1, 
		      blklog, BUFFERED);
          if( rc != 1 )
          {
	     /* %BUnable to write extent header */
	     MSGN_CALLBACK(pcontext, bkMSG066,
                           rc ,XTEXT(pdbcontext, pbkFile->ft_qname));
	     goto done;
          }

       }  /* end extent traversal */

    }  /* end area traversal */

    returnCode = 0;

done:
    if (pextentBlock)
        utfree((TEXT *)pextentBlock);
    return returnCode;

}  /* end bkUpdateExtentHeaders */


/* PROGRAM: bkInitIoTable - Create the bkfdtbl->pbkiotbl array
 *
 * RETURN: NON-ZERO - success - number of first fd table entry assigned
 *                              by this call to bkInitIoTable
 *         0 - failure
 */
int
bkInitIoTable(
	dsmContext_t	*pcontext,
	int		 numslots)  /* number of slots requested */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    bkfdtbl_t	*pbkfdtbl = (bkfdtbl_t *)pdbcontext->pbkfdtbl;
    int  ret = 0;
    int  i, numToAllocate;
    
    if (pbkfdtbl == (bkfdtbl_t *)NULL)
    {
        /* first time through, let's get some slots */
        /* We always allocate a multiple of BKFD_CHUNKS */
        numToAllocate = ((numslots / BKFD_CHUNKS) + 1) * BKFD_CHUNKS;
        pdbcontext->pbkfdtbl = (bkfdtbl_t *)stRent(pcontext,
                             pdbcontext->privpool, BKFDSIZE(numToAllocate));
        if (pdbcontext->pbkfdtbl == (bkfdtbl_t *)NULL)
        {
            /* we failed, return failed status */
            return (0);
        }
        pbkfdtbl = (bkfdtbl_t *)pdbcontext->pbkfdtbl;
        pbkfdtbl->maxEntries = numToAllocate;
        pbkfdtbl->numused = 0;
    }

    if ((pbkfdtbl->numused  + numslots ) >= pbkfdtbl->maxEntries)
    {
        /* time to get more room in bkfdtbl->pbkiotbl array */
        /* get the number of new slots needed */
        numToAllocate = numslots + pbkfdtbl->numused;
        /* now make is a multiple of BKFD_CHUNKS */
        numToAllocate = ((numToAllocate / BKFD_CHUNKS) + 1) * BKFD_CHUNKS;

        pbkfdtbl = (bkfdtbl_t *)stRent(pcontext, pdbcontext->privpool, 
                                    BKFDSIZE(numToAllocate));
        if (pbkfdtbl == (bkfdtbl_t *)NULL)
        {
            /* we failed, return failed status */
            return (0);
        }

        /* copy the old info into the new memory and increase the number of
           slots in the table */
        bufcop(pbkfdtbl, pdbcontext->pbkfdtbl, 
               BKFDSIZE(pdbcontext->pbkfdtbl->maxEntries));

        pbkfdtbl->maxEntries = numToAllocate;
        pbkfdtbl->numused = pdbcontext->pbkfdtbl->numused;

        /* free the old memory and assign the new memory */
        stVacate(pcontext, pdbcontext->privpool,(TEXT *)pdbcontext->pbkfdtbl);
        pdbcontext->pbkfdtbl = pbkfdtbl;
    }

    ret = pbkfdtbl->numused;
    for (i = 0; i < numslots; i++)
    {
        /* place the pointer to the structure in the slot */
        pbkfdtbl->pbkiotbl[pbkfdtbl->numused++] = 
                         bkioGetIoTable(pcontext, pdbcontext->privpool);
    }

    return (ret);

}  /* end bkInitIoTable */


#if OPSYS == UNIX
/* PROGRAM: bkPutIPC - open .db, read mstrblk and store shared memory
 *                     and semaphore identifiers
 *
 * RETURNS:  0 Success
 *          -1 Failure
 * PARAMETERS:
 *      bkIPCData_t *pbkIPCData         structure containing IPC values 
 *      TEXT        *pdbname            Name of database
 */
LONG
bkPutIPC(
	dsmContext_t	*pcontext,
	TEXT		*dbname,
	bkIPCData_t	*pbkIPCData)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    mstrblk_t *pmstr = 0;           /* storage for masterblock */

    int    returnCode;              /* return code for bkPutIPC() */
    int    retRWmb = 0;
    fileHandle_t   dbfd = INVALID_FILE_HANDLE;   /* fd for MV database  */


    /* Initialize to failure return code for this function    */
    /* returnCode will be reset once all steps are successful */
    returnCode = -1;

    pmstr = (mstrblk_t *) utmalloc(MAXDBBLOCKSIZE);

    if ((retRWmb = bkReadMasterBlock(pcontext, dbname, pmstr, dbfd)))
    {
        /* Unable to read master block file = %s, ret = %d */
        MSGN_CALLBACK(pcontext, bkMSG032, dbname, retRWmb);
        returnCode = retRWmb;
        goto done;
    }

    /* We don't need to do this if its a void m/v database */
    if ( !(pmstr->mb_dbstate & DBDSMVOID) )
    {
        /* check the database version number */
        if (bkDbVersionCheck (pcontext, pmstr->mb_dbvers) == -1)
        {
            /* db versions don't match */
            /* return failure          */
            goto done;
        }

        /* Write out the shared memory and semaphore ids to the mstrblk */
        if (pdbcontext->usertype & BROKER)
        {
            pmstr->mb_shmid = pbkIPCData->NewShmId;
            pmstr->mb_semid = pbkIPCData->NewSemId;
            
            if ((retRWmb = bkWriteMasterBlock(pcontext, dbname, pmstr,
                                              INVALID_FILE_HANDLE)))
            {
                /* Unable to write master block file = %s, ret = %d */
                MSGN_CALLBACK(pcontext, bkMSG032, dbname, retRWmb);
                goto done;
            }
        }
        returnCode = 0;
    }
    else
    {
       /* This was a procopy operation, its ok nothing was updated */
       returnCode = 0;
    }

done:
    if (pmstr)
        utfree((TEXT *)pmstr);

    return returnCode;

}  /* end bkPutIPC */


/* PROGRAM: bkGetIPC - open .db, read mstrblk and read shared memory
 *                     and semaphore identifiers
 *
 * RETURNS:  0 Success
 *          -1 Failure
 * PARAMETERS:
 *      bkIPCData_t *pbkIPCData         structure containing IPC values 
 *      TEXT        *pdbname            Name of database
 */
LONG
bkGetIPC(
	dsmContext_t	*pcontext,
	TEXT		*pdbname,
	bkIPCData_t	*pbkIPCData)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    mstrblk_t   mstr;
    int         returnCode;            /* return code for bkGetIPC() */
    int         retReadMb;


    /* Initialize to failure return code for this function    */
    /* returnCode will be reset once all steps are successful */
    returnCode = -1;

    /* read the masterblock from the database */
    retReadMb = bkReadMasterBlock(pcontext, pdbname, &mstr,INVALID_FILE_HANDLE);
    if (retReadMb != BK_RDMB_SUCCESS)
    {
        if (retReadMb == BK_RDMB_READ_FAILED)
        {
            /* Read of masterblock failed */
            /* Display message for non-self service clients */
            if (!(pcontext->pdbcontext->usertype & SELFSERVE))
            {
                MSGN_CALLBACK(pcontext, bkMSG032, pdbname, -1);
            }
        }

        /* return failure */
        return (returnCode);
    }

    /* We don't need to do this if its a void m/v database */
    if ( !(mstr.mb_dbstate & DBDSMVOID) )
    {
        /* check the database version number */
        if (bkDbVersionCheck (pcontext, mstr.mb_dbvers) == -1)
        {
            /* db versions don't match */
            /* return failure          */
            return (returnCode);
        }

        /* Store the shared memory and semaphore ids from the mstrblk */
        /* brokers and single user behaves the same                   */
        /* (pdbcontext->usertype == 0) tests for single user             */

        if ((pdbcontext->usertype == BROKER) || (pdbcontext->usertype == BATCHUSER)
             || (pdbcontext->usertype == 0))
        {
            pbkIPCData->OldShmId = mstr.mb_shmid;
            pbkIPCData->OldSemId = mstr.mb_semid;
        }
        else
        {
            pbkIPCData->NewShmId = mstr.mb_shmid;
            pbkIPCData->NewSemId = mstr.mb_semid;
        }

    }

    /* set return code to success    */
    returnCode = 0;

    return returnCode;

}  /* end bkGetIPC */

#endif

/* PROGRAM: bkMarkDbSynced - Reads the master block, updates the
 *                           synced flag, then writes out the
 *                           master block.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkMarkDbSynced(dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int	           rc;
    mstrblk_t      mblock;

    rc = bkReadMasterBlock(pcontext, pdbcontext->pdbname, 
                              &mblock, INVALID_FILE_HANDLE);
    if (rc != BK_RDMB_SUCCESS)
    {
        /* failed to open the master block, just return */
        return;
    }

    /* set the master block to synced */
    mblock.mb_shutdown = 1;
    mblock.mb_bistate = BITRUNC;

    rc = bkWriteMasterBlock(pcontext, pdbcontext->pdbname, &mblock,
                            INVALID_FILE_HANDLE);
    return;

} /* end bkMarkDbSynced */

/* PROGRAM: bkAreaClose - close the files associated with the area
 *
 * RETURNS: 0 - Success, -1 Failure
 */
int
bkAreaClose(dsmContext_t *pcontext, dsmArea_t area)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int	           rc;
    bkAreaDescPtr_t    *pbkAreaDescPtr;
    bkAreaDesc_t       *pbkAreaDesc;
    bkftbl_t           *ptbl = (bkftbl_t *)NULL;

    /* get the area descriptor pointers */
    pbkAreaDescPtr = XBKAREADP(pdbcontext, 
                            pdbcontext->pbkctl->bk_qbkAreaDescPtr);

    /* get the area descriptor for the passed area */
    pbkAreaDesc = XBKAREAD(pdbcontext, pbkAreaDescPtr->bk_qAreaDesc[area]);
    if (pbkAreaDesc == (bkAreaDesc_t *)NULL)
    {
        /* invalid area */
        return -1;
    }

    rc = bkGetAreaFtbl(pcontext, area, &ptbl);
       
    if (ptbl)
    {
       for (; ptbl->ft_qname; ptbl++)
       {  
           /* close the files */
           rc = bkioClose(pcontext,
                    pdbcontext->pbkfdtbl->pbkiotbl[ptbl->ft_fdtblIndex], 0);
       }
    }
    else
    {
        /* no files associated with this area */
        return -1;
    }
    
    return 0;
}


/* PROGRAM: bkAreaOpen - open the files associated with the area
 *
 * RETURNS: 0 - Success, -1 Failure
 */
int
bkAreaOpen(dsmContext_t *pcontext, dsmArea_t area, int refresh)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    int	           rc;
    bkAreaDescPtr_t    *pbkAreaDescPtr;
    bkAreaDesc_t       *pbkAreaDesc;
    bkftbl_t           *ptbl = (bkftbl_t *)NULL;

    /* get the area descriptor pointers */
    pbkAreaDescPtr = XBKAREADP(pdbcontext, 
                            pdbcontext->pbkctl->bk_qbkAreaDescPtr);

    /* get the area descriptor for the passed area */
    pbkAreaDesc = XBKAREAD(pdbcontext, pbkAreaDescPtr->bk_qAreaDesc[area]);
    if (pbkAreaDesc == (bkAreaDesc_t *)NULL)
    {
        /* invalid area */
        return -1;
    }

    rc = bkGetAreaFtbl(pcontext, area, &ptbl);
       
    if (ptbl)
    {
        for (; ptbl->ft_qname; ptbl++)
        {  
            /* open the files */
            rc = bkOpenOneFile(pcontext, ptbl, OPEN_RW);
            if (rc < 0)
            {
	        MSGN_CALLBACK(pcontext, bkMSG002,
                           XTEXT(pdbcontext, ptbl->ft_qname),errno);
            }
        }
    }
    else
    {
        /* no files associated with this area */
        return -1;
    }

    /* reset file sizes and extent amounts */
    rc = bkSetFiles(pcontext, pbkAreaDesc, pdbcontext->pmstrblk);

    /* fix up the area descriptor if refresh is requested */
    if (!rc && refresh)
    {
        bmBufHandle_t  objHandle;
        objectBlock_t *pobjectBlock;
        bkAreaDesc_t  *pbkAreaDesc;

        objHandle = bmLocateBlock(pcontext, area,
                              BLK2DBKEY(BKGET_AREARECBITS(area)), TOREAD);
        pobjectBlock = (objectBlock_t *)bmGetBlockPointer(pcontext, objHandle);

        pbkAreaDesc = BK_GET_AREA_DESC(pcontext->pdbcontext, area);

        bkAreaSetDesc(pobjectBlock, pbkAreaDesc);

        bmReleaseBuffer(pcontext, objHandle);
    }

    return rc;
}


/* PROGRAM: bkAreaSetDesc - fill in some members of the area descriptor
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkAreaSetDesc(
    objectBlock_t *pobjectBlock,
    bkAreaDesc_t  *pbkAreaDesc)
{
    pbkAreaDesc->bkMaxDbkey = (DBKEY)(pobjectBlock->totalBlocks+1) <<
                               pbkAreaDesc->bkAreaRecbits;
    pbkAreaDesc->bkHiWaterBlock  = pobjectBlock->hiWaterBlock;
    pbkAreaDesc->bkChainFirst[FREECHN]  = pobjectBlock->chainFirst[FREECHN];
    pbkAreaDesc->bkChainFirst[RMCHN]    = pobjectBlock->chainFirst[RMCHN];
    pbkAreaDesc->bkChainFirst[LOCKCHN]  = pobjectBlock->chainFirst[LOCKCHN];
    pbkAreaDesc->bkNumBlocksOnChain[FREECHN] =
                 pobjectBlock->numBlocksOnChain[FREECHN];
    pbkAreaDesc->bkNumBlocksOnChain[RMCHN]   =
                 pobjectBlock->numBlocksOnChain[RMCHN];
    pbkAreaDesc->bkNumBlocksOnChain[LOCKCHN] =
                 pobjectBlock->numBlocksOnChain[LOCKCHN];
}

/* PROGRAM: bkRemoveAreaRecords - remove all records associated with
 *                       marked for deletion areas.
 *
 *
 *
 * RETURNS: 0 on Success, -1 on Failure
 */
DSMVOID
bkRemoveAreaRecords(dsmContext_t *pcontext)
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dbshm_t         *pdbpub = pdbcontext->pdbpub;
    bkAreaDesc_t    *pbkArea;
    bkAreaDescPtr_t *pbkAreas;
    ULONG            area;

    pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
    for ( area = DSMAREA_DEFAULT ; area < pbkAreas->bkNumAreaSlots; area++ )
    {
        pbkArea = BK_GET_AREA_DESC(pdbcontext, area);
        if ((pbkArea) && (pbkArea->bkDelete))
        {
            /* delete the records */
            dbDeleteAreaRecords(pcontext, area);
            /* free up the area */
            stVacate(pcontext, XSTPOOL(pdbcontext, pdbpub->qdbpool),
                          (TEXT *)pbkArea);

            pbkAreas->bk_qAreaDesc[area] = 0;

        }
    }
    return;
}

#if 0  /* we can't do this yet because I removed the pointer to the bkftbl 
          from the bkiotbl structure.  For single volume the .db info resides
          in the 0th slot of the BKDATA chain of bkftbl's but for multi-volume
          we don't hold a slot for the .db file.  When we fix this, we can 
          then start keeping accurate track of I/O's on the .db file 
          - MikeF  BUM */
#if SHMEMORY
/* PROGRAM: bkUpdateDbCounters - Update the db iotbl counters.  in order
 *                   to read the master block we have to create a temporary
 *                   bkftbl so bkio function will work.  Since we have this
 *                   data we should include it with the promon stats.
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
bkUpdateDbCounters(
	dsmContext_t *pcontext,
	bkftbl_t  *ptbkftbl,  /* pointer to temporary bkftbl */
	bkiotbl_t *pbkiotbl)  /* index to a bkiotbl structure */
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        bkftbl_t	*pbkftbl;		/* pointer to temporary bkftbl */

    if (!pbkiotbl)
    {
        /* I/O table not initialized. */
        MSGN_CALLBACK(pcontext, bkMSG064);
        return;
    }

    /* get the shared memory file table */
    pbkftbl = (XBKFTBL(pdbcontext, bkioGetFileTable(pbkiotbl)));

    /* if the temp structure is the same as the public one, 
       don't do anything */
    if (pbkftbl == ptbkftbl)
        return;

    /* update all the counters */
    bkioUpdateCounters(pcontext, pbkftbl->ft_bkiostats, ptbkftbl->ft_bkiostats);

    return;

}  /* end bkUpdateDbCounters */

#endif
#endif   /* if 0 */
