
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

#include "bmpub.h"
#include "mstrblk.h"
#include "cxpub.h"
#include "dbcontext.h"
#include "dbpub.h"
#include "dsmpub.h"
#include "lkpub.h"
#include "recpub.h"
#include "rmpub.h"
#include "scasa.h"
#include "usrctl.h"
#include "utfpub.h"
#include "utspub.h"
#include "bkprv.h"
#include "bkaread.h"

#define RECSIZE 4096
/* PROGRAM: dbAreaRecordCreate
 *
 *
 * RETURNS:
 */
dsmStatus_t
dbAreaRecordCreate(
        dsmContext_t  *pcontext,
        ULONG          areaVersion _UNUSED_,  /* not used yet */
        dsmArea_t      areaNumber,
        dsmText_t     *areaName,
        dsmAreaType_t  areaType,
        dsmRecid_t     areaBlock _UNUSED_,    /* not used yet */
        ULONG          areaAttr _UNUSED_,     /* not used yet */
        dsmExtent_t    areaExtents,
        dsmBlocksize_t blockSize,     /* IN are block size in bytes */
        dsmRecbits_t   areaRecbits,
        dsmRecid_t    *pareaRecid)   /* OUT - recid of new area record */
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode = DSM_S_SUCCESS;
    dsmDbkey_t     areaRoot;
    ULONG          recSize;

    svcDataType_t  component[2];
    svcDataType_t *pcomponent[2];
    svcByteString_t recTextValue, *precText;
    COUNT          maxKeyLength = 256;

    TEXT           areaRecord[RECSIZE];
    TEXT          *pareaRecord = &areaRecord[0];
    lockId_t       lockId;

    dsmBoolean_t   setTrans;

    AUTOKEY(key, 256)
 
    areaRoot = pdbcontext->pmstrblk->areaRoot;

    if (!areaRoot)
       goto badExit;

/* TODO: We are assuming here that the areaRoot exists
 *       but for prostrct create usage it may not.  We should test it
 *       and if it has no value, we should create the index and update 
 *       the control block.
 */
    /* materialize _Area template record */
    recSize = recFormat(pareaRecord, RECSIZE, SCFLD_AREAMIN-1);
    if (recSize > RECSIZE)
        goto done;

    /* Format misc field */
    returnCode = recInitArray(pareaRecord, RECSIZE,  &recSize,
                              SCFLD_AREAMISC, 8);

    if ( returnCode != RECSUCCESS)
        goto done;

    /**** Build the area record ****/

        /* 1st field is ALWAYS table number */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   1, (ULONG)0, SCTBL_AREA, 0) )
        goto badExit;
 
    /* set _Area-version */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREAVERS, (ULONG)0, 1, 0) )
        goto badExit;
 
    /* set _Area-number */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREANUM, (ULONG)0, areaNumber, 0) )
        goto badExit;

    recTextValue.pbyte  = areaName;
    recTextValue.length = stlen(areaName);
    /* TODO: remove the following when rec services is working properly! */
    recTextValue.size   = recTextValue.length;
    precText = &recTextValue;
    if (recPutBYTES(pareaRecord, RECSIZE, &recSize,
                    SCFLD_AREANAME, (ULONG)0, precText, 0) )
        goto badExit;
 
    /* set _Area-type */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREATYPE, (ULONG)0, areaType, 0) )
        goto badExit;
 
    /* set _Area-block */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREABLOCK, (ULONG)0, 0, 0) )
        goto badExit;
 
    /* set _Area-attrib */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREAATTR, (ULONG)0, 0, 0) )
        goto badExit;

    /* set _Area-system */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREASYS, (ULONG)0, 0, 0) )
        goto badExit;
 
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREABLKSZ, (ULONG)0, blockSize, 0) )
        goto badExit;
 
    /* set _Area-recbits */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREABITS, (ULONG)0, areaRecbits, 0) )
        goto badExit;

    /* set _Area-extents */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREAEXT, (ULONG)0, areaExtents, 0) )
        goto badExit;
 
    /* _Area record generation completed */
 
    recSize--;  /* omit the ENDREC from the recSize */

    /**** Write out the Area record ****/
    lockId.dbkey = rmCreate(pcontext, DSMAREA_CONTROL, SCTBL_AREA,
                            pareaRecord, (COUNT)recSize,
                            (UCOUNT)RM_LOCK_NEEDED);

    if (pareaRecid)
        *pareaRecid = lockId.dbkey;
 
    /**** Build the SCIDX_AREA index for the _Area Table ****/

    lockId.table = SCTBL_AREA;
 
    key.akey.area         = DSMAREA_CONTROL;
    key.akey.root         = areaRoot;
    key.akey.unique_index = DSMINDEX_UNIQUE;
    key.akey.word_index   = 0;
  
    key.akey.index    = SCIDX_AREA;
    key.akey.keycomps = 1;
    key.akey.ksubstr  = 0;
    pcomponent[0] = &component[0];
 
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = (LONG)areaNumber;
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &key.akey))
        goto done;
 
    if (pcontext->pusrctl->uc_task)
        setTrans = 0;
    else
    {
        pcontext->pusrctl->uc_task = 1;  /* fake out cxAddNL */
        setTrans = 1;
    }
    returnCode = cxAddEntry(pcontext, &key.akey, &lockId,
                            (TEXT *)"dsmAreaRecordCreate", 
                            SCTBL_AREA);

    if (setTrans)
        pcontext->pusrctl->uc_task = 0;

    if (returnCode == IXDUPKEY)
    {
        rmDelete (pcontext, SCTBL_AREA, lockId.dbkey, NULL);
        returnCode = DSM_S_AREA_ALREADY_ALLOCATED;
    }


   if (!pdbcontext->prlctl) /* if no logging, we'll do the flush right here. */
       bmFlushx(pcontext, FLUSH_ALL_AREAS, 1);

   /***** END AREA RECORD CREATION *****/

done:
    return returnCode;

badExit:
    return DSM_S_FAILURE;

}  /* end dbAreaRecordCreate */


/* PROGRAM: dbAreaRecordDelete
 *
 *
 * RETURNS:
 */
dsmStatus_t
dbAreaRecordDelete(
        dsmContext_t    *pcontext,
        dsmArea_t        areaNumber)
{

    dbcontext_t    *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t     returnCode = 0;
    dsmDbkey_t      areaRecid;
    dsmDbkey_t      areaRoot;
    lockId_t        lockId;
    svcDataType_t   component[2];
    svcDataType_t  *pcomponent[2];
    COUNT           maxKeyLength = 256;

    AUTOKEY(keyBase,  256)
    AUTOKEY(keyLimit, 256)
 

    areaRoot = pdbcontext->pmstrblk->areaRoot;

    if (!areaRoot)
    {
        returnCode = DSM_S_AREA_NOT_INIT;
        goto done;
    }

    /* Build index of area record */
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = areaNumber;

    pcomponent[0] = &component[0];

    keyBase.akey.area         = DSMAREA_CONTROL;
    keyBase.akey.root         = areaRoot;
    keyBase.akey.unique_index = 1;
    keyBase.akey.word_index   = 0;
    keyBase.akey.index        = SCIDX_AREA;
    keyBase.akey.keycomps     = 1;
    keyBase.akey.ksubstr      = 0;
 
    returnCode = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                           &keyBase.akey);
    if (returnCode)
    {
        returnCode = DSM_S_FAILURE;
        goto done;
    }

    component[1].indicator = SVCHIGHRANGE;
    pcomponent[1]          = &component[1];

    keyLimit.akey.area          = DSMAREA_CONTROL;
    keyLimit.akey.root          = areaRoot;
    keyLimit.akey.unique_index  = 1;
    keyLimit.akey.word_index    = 0;
    keyLimit.akey.index         = SCIDX_AREA;
    keyLimit.akey.keycomps      = 2;
    keyLimit.akey.ksubstr       = 1;

    returnCode = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                           &keyLimit.akey);
    if (returnCode)
    {
        returnCode = DSM_S_FAILURE;
        goto done;
    }
 
    /* Now do the Find EXCLUSIVE */
    returnCode = cxFind(pcontext, &keyBase.akey, &keyLimit.akey,
                        NULL /* No cursor */,
                        SCTBL_AREA, &areaRecid,
                        DSMUNIQUE, DSMFINDFIRST, DSM_LK_EXCL);
    if (returnCode)
        goto done;

    lockId.table  = SCTBL_AREA;
    lockId.dbkey = (DBKEY)areaRecid;
    returnCode = cxDeleteEntry(pcontext, &keyBase.akey, &lockId, 0, 
                               SCTBL_AREA, NULL);
    if (returnCode)
        goto done;
 
    /* Delete the associated Area Record */
    returnCode = (dsmStatus_t)rmDelete (pcontext, SCTBL_AREA,
                                        (DBKEY)areaRecid, NULL);
   if (returnCode)
      goto done; 

   if (!pdbcontext->prlctl) /* if no logging, we'll do the flush right here. */
       bmFlushx(pcontext, FLUSH_ALL_AREAS, 1);

done:
    return returnCode;

}  /* end dbAreaRecordDelete */

/* PROGRAM: dbExtentCreate - Create an extent in an area
 *
 *    dsmExtentCreate() creates an extent for the specified area.  
 *    The extent created is always the last extent in the area and the 
 *    extent id is returned in pextent.  When the extent is created, the 
 *    extentMode  is used to determine if a file should be created and/or 
 *    initialized.
 *
 *    The extentMode bits are defined as follows:
 *
 *      EXRAW	extent is a raw device that already exists, do not create 
 *              a file if not set extent is on standard filesystem
 *      EXNET	extent is on a reliable network filesystem
 *              if not set extent should not be on a network filesystem
 *      EXEXISTS extent is an existing file or raw device do not create or 
 *               intitialize
 *               if not set there should be no existing file
 *      EXFIXED	extent is fixed size (initial size is total size)
 *              if not set the extent is variable sized
 *      EXFORCE	operation should be forced to complete bypassing sanity checks.
 *              if not set all sanity checks are performed.
 *
 * One example if EXNET was not set, but EXFORCE was set then a create 
 * should succeed even though the extent will be created on a network device.
 *
 * If the extent is the first extent in an area, then part of initialization 
 * of on-disk information includes area structures using area cache information.
 *
 *
 * RETURNS: DSM_S_SUCCESS on success
 *          DSM_S_FAILURE on any failure
 */
dsmStatus_t 
dbExtentCreate(dsmContext_t	*pcontext,	/* IN database context */
	        dsmArea_t 	areaNumber,	/* IN area for the extent */
	        dsmExtent_t	extentId,	/* IN extent id */
	        dsmSize_t 	initialSize,	/* IN initial size in blocks */
	        dsmMask_t	extentMode,	/* IN extent mode bit mask */
	        dsmText_t	*pname)		/* IN extent path name */
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    bkAreaDesc_t  *pbkArea;
    dsmStatus_t    returnCode;
    GTBOOL          extentType;

    /* validate areaNumber */
    if ( (areaNumber > DSMAREA_MAXIMUM) ||
         ((areaNumber < DSMAREA_SCHEMA)  &&
          ((areaNumber != DSMAREA_BI) && (areaNumber != DSMAREA_TL) &&
           (areaNumber != DSMAREA_TEMP))) )
    {
        returnCode = DSM_S_INVALID_AREANUMBER;
        goto done;
    }

    /* Get a pointer to the area descriptor                             */
    pbkArea = BK_GET_AREA_DESC(pdbcontext, areaNumber);
    if( pbkArea == NULL )
    {
	returnCode = DSM_S_AREA_NULL;
        goto done;
    }

    if ( extentId != (pbkArea->bkNumFtbl + 1) )
    {
	/* Can create only the next extent                            */
        returnCode = DSM_S_INVALID_EXTENT_CREATE;
	goto done;
    }
#if 0
    if( extentMode & EXRAW )
    {
	extentType = BKPART;
    }
    else
    {
	extentType = BKUNIX;
    }
    if( extentMode & EXFIXED)
    {
	extentType |= BKFIXED;
    }
#endif
    extentType = (GTBOOL)extentMode;

    /* validate the areatype with the extent type */
    switch (extentType & BKETYPE)
    {
      case BKAI:
         if (pbkArea->bkAreaType != DSMAREA_TYPE_AI)
         {
             returnCode = DSM_S_INVALID_AREATYPE;
             goto done;
         }
         break;
      case BKTL:
         if( pbkArea->bkAreaType != DSMAREA_TYPE_TL)
         {
             returnCode = DSM_S_INVALID_AREATYPE;
             goto done;
         }
         break;
      case BKBI:
         if (pbkArea->bkAreaType != DSMAREA_TYPE_BI)
         {
             returnCode = DSM_S_INVALID_AREATYPE;
             goto done;
         }
         if (pdbcontext->prlctl) /* can't add bi if recovery enabled!! */
         {
             returnCode = DSM_S_RECOVERY_ENABLED;
             goto done;
         }
         break;
      case BKDATA:
         if (pbkArea->bkAreaType != DSMAREA_TYPE_DATA)
         {
             returnCode = DSM_S_INVALID_AREATYPE;
             goto done;
         }
         break;
      default:
        returnCode = DSM_S_INVALID_AREATYPE;
        goto done;
    }

    returnCode = bkExtentCreate(pcontext, pbkArea, &initialSize,
                                extentType, pname);
    if (returnCode)
        goto done;

    returnCode = dbExtentRecordCreate(pcontext, areaNumber, extentId,
                                       initialSize, 0, extentType, pname);
    if (returnCode)
        goto done;

    /* If not the only extent in this area then fixup existing records. */
    if ( ((extentType & BKETYPE) != BKAI) &&
         (extentId > 1) )
    {
        returnCode = dbExtentRecordFixup(pcontext, areaNumber,
                                          extentId, extentType);
    }

done:

    return returnCode;

}  /* end dbExtentCreate */

/* PROGRAM: dbExtentRecordFixup - fixup existing extent records after an add
 *
 */
dsmStatus_t
dbExtentRecordFixup(
        dsmContext_t   *pcontext,
        dsmArea_t       areaNumber, 
        dsmExtent_t     extentId,
        dsmMask_t       extentType)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t  returnCode = DSM_S_FAILURE;
    dsmStatus_t  ret;
    bkftbl_t    *pbkftbl;       /* ptr to file table entry */
    dsmRecid_t   extentRecid;
    LONG         length;        /* length in FLUNITS */
    gem_off_t    utfilesize;    /* filesize in bytes from utflen() */
    ULONG        recSize;
    bkAreaDescPtr_t  *pbkAreas;
    bkAreaDesc_t     *pbkArea;
    dsmText_t    extentRecord[RECSIZE];
    dsmText_t   *pextentRecord = &extentRecord[0];



    pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
    pbkArea = XBKAREAD(pdbcontext, pbkAreas->bk_qAreaDesc[areaNumber]);
 
    /* Perform "fixup" of existing extents */
    ret = bkGetAreaFtbl(pcontext, areaNumber, &pbkftbl);
    if (ret)
    {
        MSGD_CALLBACK(pcontext,
        (TEXT *)"dsmExtentRecordFixup: Error getting area extent descriptor.");
        goto done;
    }
 
    /* get the last existing extent of this area (before the add)
     * (NOTE: the extentId we are after is extentId - 1,
     *        but the pbkftbl offset is extentId - 2)
     */
    if (extentId > 1)
    {
        pbkftbl = pbkftbl + (extentId - 2);

        /* only need to do "fixup" if its not fixed length */

        if ( !(pbkftbl->ft_type & BKFIXED) )
        {
            switch (extentType & BKETYPE)
            {
              case BKDATA:
                /* fix its length at its current */
                length = (LONG)(pbkftbl->ft_curlen / FLUNITS);
                if (pbkArea->bkBlockSize > FLUNITS)
                    length = length + (pbkArea->bkBlockSize / FLUNITS);
                else
                    length = length + 1;
                break;

              case BKBI:
                /* fix its length at its current */
                /* Bug # 94-03-01-060 */
                if (utflen ((TEXT *)QP_TO_P(pdbcontext,
                           pbkftbl->ft_qname), &utfilesize) == 0)
                    length = (LONG)(utfilesize/FLUNITS);
                else
                {
                    MSGD_CALLBACK(pcontext,
                           (TEXT *)"Get file length error. file: %s",
                                QP_TO_P(pdbcontext, pbkftbl->ft_qname));
                    goto done;
                }

                /* Round length down to a multiple of 16 blocks  */
                /* The only time a variable length extent will not
                 * be a multiple of 16 blocks is if we crashed
                 * during the file extend.  Since this add may be
                 * to rectify the extend failure we'll round the length
                 * we save down since we don't want to fail trying to
                 * extend length of the file.
                 */
                length = length & (~15L);
                break;
              default:
                MSGD_CALLBACK(pcontext,
                             (TEXT *)"Attept to fixup invalid extent type: %d",
                              extentType);
                goto done;
            }

            if (length < MINFLEN)
            {
                MSGD_CALLBACK(pcontext,
                       (TEXT *)"%rLength of %s is %d block(s),",
                       QP_TO_P(pdbcontext, pbkftbl->ft_qname), length);
                MSGD_CALLBACK(pcontext, 
                       (TEXT *)"which is too small to be fixed.");
                MSGD_CALLBACK(pcontext,
                       (TEXT *)"%rMinimum length is %d blocks.", MINFLEN);
                goto done;
            }

/* TODO: The following changes to pbkftbl should be logged! */
            pbkftbl->ft_type |= BKFIXED;
            pbkftbl->ft_curlen = (gem_off_t)length * FLUNITS;

            returnCode = dbExtentGetRecid(pcontext, areaNumber, extentId-1,
                                           &extentRecid, DSM_LK_EXCL);
            if (returnCode)
                goto done;


            /* Now update extent record with new extent size and type */
            recSize = rmFetchRecord(pcontext, DSMAREA_CONTROL,
                                   extentRecid, pextentRecord,
                                   RECSIZE, 0);
            if ( recSize > RECSIZE)
            {
                returnCode = DSM_S_RECTOOBIG;
                goto done;
            }

            /* set _Extent-type */
            if (recPutTINY(pextentRecord, RECSIZE, &recSize,
                           SCFLD_EXTTYPE, (ULONG)0, pbkftbl->ft_type, 0))
            {
                returnCode = RECFAILURE;
                goto done;
            }

            /* set _Extent-size */
            if (recPutLONG(pextentRecord, RECSIZE, &recSize,
                           SCFLD_EXTSIZE, (ULONG)0, length, 0) )
            {
                returnCode = RECFAILURE;
                goto done;
            }

            returnCode = rmUpdateRecord(pcontext, SCTBL_EXTENT,
                                DSMAREA_CONTROL, extentRecid,
                                pextentRecord, (COUNT)recSize);
            if (returnCode)
                goto done;
       }
    }

   if (!pdbcontext->prlctl) /* if no logging, we'll do the flush right here. */
       bmFlushx(pcontext, FLUSH_ALL_AREAS, 1);

    returnCode = DSM_S_SUCCESS;

done:
    return returnCode;

}  /* end dbExtentRecordFixup */


/* PROGRAM: dbExtentGetRecid -
 *
 */
dsmStatus_t
dbExtentGetRecid(
        dsmContext_t *pcontext,
        dsmArea_t     areaNumber,
        dsmExtent_t   extentId,
        dsmRecid_t   *pextentRecid,
        dsmMask_t     lockMode)
{
    dsmStatus_t returnCode = DSM_S_FAILURE;

    dsmDbkey_t      extentRoot;
    svcDataType_t   component[3];
    svcDataType_t  *pcomponent[3];
    COUNT           maxKeyLength = 256;
 
    AUTOKEY(keyBase,  256)
    AUTOKEY(keyLimit, 256)


    extentRoot = pcontext->pdbcontext->pmstrblk->areaExtentRoot;

    if (!extentRoot)
    {
        returnCode = DSM_S_AREA_NOT_INIT;
        goto done;
    }

    /* search by area/extent number */
    keyBase.akey.area         = DSMAREA_CONTROL;
    keyBase.akey.root         = extentRoot;
    keyBase.akey.unique_index = 1;
    keyBase.akey.word_index   = 0;

    keyBase.akey.index    = SCIDX_EXTAREA;
    keyBase.akey.keycomps = 2;
    keyBase.akey.ksubstr  = 0;

    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = (LONG)areaNumber;

    component[1].type            = SVCSMALL;
    component[1].indicator       = 0;
    component[1].f.svcSmall       = extentId;
 
    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];

    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &keyBase.akey))
        goto done;
 
    keyLimit.akey.area          = DSMAREA_CONTROL;
    keyLimit.akey.root          = extentRoot;
    keyLimit.akey.unique_index  = 1;
    keyLimit.akey.word_index    = 0;

    keyLimit.akey.index     = SCIDX_EXTAREA;
    keyLimit.akey.keycomps  = 3;
    keyLimit.akey.ksubstr   = 0;

    component[2].indicator       = SVCHIGHRANGE;
    pcomponent[2] = &component[2];
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &keyLimit.akey))
        goto done;


    /* attempt to get first index entry within specified bracket */
    returnCode = cxFind (pcontext,
                  &keyBase.akey, &keyLimit.akey,
                  NULL, DSMAREA_CONTROL, pextentRecid,
                  DSMUNIQUE, DSMFINDFIRST, lockMode);
    if (returnCode != 0)
    {
        returnCode = DSM_S_AREA_NO_EXTENT;
        goto done;
    }

    returnCode = DSM_S_SUCCESS;

done:
    return returnCode;

}  /* end dbExtentGetRecid */

/* PROGRAM: dbAreaCreate - Create an area in the database
 *
 *      dsmAreaCreate() determines if the requested areaNumber is 
 *      available in the area cache and initializes the area descriptor 
 *      with areaType and blockSize information.
 *
 *      In addition, it will optionally create the area record associated
 *      with this newly added area.
 *
 * RETURNS: DSM_S_SUCCESS on success
 *          DSM_S_AREA_NOT_INIT     area sub-system has not been initialized
 *          DSM_S_AREA_NUMBER_TOO_LARGE  invalid area number passed in
 *          NOSTG                        storage allocation failure 
 */
dsmStatus_t 
dbAreaCreate(dsmContext_t 	*pcontext,     /* IN database context */
	      dsmBlocksize_t	blockSize,     /* IN are block size in bytes */
	      dsmAreaType_t	areaType,      /* IN type of area */
	      dsmArea_t		areaNumber,    /* IN area number */
              dsmRecbits_t	areaRecbits,   /* IN # of record bits */
              dsmText_t        *areaName)      /* IN name of new area */
{
    dbcontext_t          *pdbcontext = pcontext->pdbcontext;
    bkAreaDescPtr_t      *pbkAreas;
    bkAreaNote_t          biNote;
    dsmStatus_t           returnCode = DSM_S_SUCCESS;


    /* Get pointer to array of pointers to area descriptors in database
       shared memory                                                     */

    pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);

    if (pbkAreas == NULL)
    {
	/* Database probably hasn't been opened yet    */
	returnCode = DSM_S_AREA_NOT_INIT;
        goto done;
    }

    if( areaNumber >= pbkAreas->bkNumAreaSlots )
    {
	returnCode = DSM_S_AREA_NUMBER_TOO_LARGE;
        goto done;
    }

    /* NOTE: if DSM_S_AREA_ALREADY_ALLOCATED then this is an update
     *       of shared memory structutres with an area record creation.
     *       It is done this way because prostrct add uses shared memory
     *       structutres for .st validation without creating area records.
     */

    /* validate blocksize, areaType  and areaRecbits */
    switch (areaType)
    {
      case DSMAREA_TYPE_DATA:
        if (blockSize != (dsmBlocksize_t)BKGET_BLKSIZE(pdbcontext->pmstrblk) )
        {
            returnCode = DSM_S_INVALID_BLOCKSIZE;
            goto done;
        }
        /* validate recBits */
        if  (areaRecbits > (dsmRecbits_t)10) 
        {
            returnCode = DSM_S_INVALID_AREARECBITS;
            goto done;
        }
        break;
      case DSMAREA_TYPE_AI:
        if (blockSize != (dsmBlocksize_t)pdbcontext->pmstrblk->mb_aiblksize)
        {
            returnCode = DSM_S_INVALID_BLOCKSIZE;
            goto done;
        }
        areaRecbits = 0;
        break;
      case DSMAREA_TYPE_TL:
        blockSize = RLTL_BUFSIZE;
        areaRecbits = 0;
        break;

      case DSMAREA_TYPE_BI:
          areaRecbits = 0;
          break;
          
      case DSMAREA_TYPE_CBI:
      default:
        returnCode = DSM_S_INVALID_AREATYPE;
        break;
    }
    

    /* validate areaNumber */
    if ( ((areaNumber > DSMAREA_MAXIMUM) ||
         (areaNumber < DSMAREA_SCHEMA)) &&
         (areaNumber != DSMAREA_TL) &&
         (areaNumber != DSMAREA_TEMP))
    {
        returnCode = DSM_S_INVALID_AREANUMBER;
        goto done;
    }
    /* Does the area already exist   */
    if(pbkAreas->bk_qAreaDesc[areaNumber])
    {
      returnCode = DSM_S_AREA_ALREADY_ALLOCATED;
      goto done;
    }

    INITNOTE( biNote, RL_BKAREA_ADD, RL_LOGBI | RL_PHY );
    INIT_S_BKAREANOTE_FILLERS( biNote );	/* bug 20000114-034 by dmeyer in v9.1b */
    biNote.area        = areaNumber;
    biNote.areaType    = areaType;
    biNote.blockSize   = blockSize;
    biNote.areaRecbits = areaRecbits;
    rlLogAndDo(pcontext, (RLNOTE *)&biNote, 0, 0, NULL);
	

    /* now go create the area record  (if not database creation) */
    if (areaName)
    {
        returnCode = dbAreaRecordCreate(pcontext, 0,
                                     areaNumber, areaName, areaType,
                                     0, 0, 0,
                                     blockSize, areaRecbits, 0);
    }
 
done:

    return returnCode;

}  /* end dbAreaCreate */

/* PROGRAM: dbAreaGetRecid -
 *
 */
dsmStatus_t
dbAreaGetRecid(
        dsmContext_t *pcontext,
        dsmArea_t     areaNumber,
        dsmRecid_t   *pareaRecid,
        dsmMask_t     lockMode)
{
    dsmStatus_t     returnCode = -1;
    dsmDbkey_t      areaRoot;
    svcDataType_t   component[2];
    svcDataType_t  *pcomponent[2];
    COUNT           maxKeyLength = 256;

    AUTOKEY(keyBase,  256)
    AUTOKEY(keyLimit, 256)
 
    areaRoot = pcontext->pdbcontext->pmstrblk->areaRoot;

    if (!areaRoot)
    {
        returnCode = DSM_S_AREA_NOT_INIT;
        goto done;
    }

    keyBase.akey.area         = DSMAREA_CONTROL;
    keyBase.akey.root         = areaRoot;
    keyBase.akey.unique_index = 1;
    keyBase.akey.word_index   = 0;
    keyBase.akey.index        = SCIDX_AREA;
    keyBase.akey.keycomps     = 1;
    keyBase.akey.ksubstr      = 0;
 
    /* Build index of area record */
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = areaNumber;

    pcomponent[0] = &component[0];

    returnCode = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                           &keyBase.akey);
    if (returnCode)
    {
        returnCode = DSM_S_FAILURE;
        goto done;
    }

    keyLimit.akey.area          = DSMAREA_CONTROL;
    keyLimit.akey.root          = areaRoot;
    keyLimit.akey.unique_index  = 1;
    keyLimit.akey.word_index    = 0;
    keyLimit.akey.index         = SCIDX_AREA;
    keyLimit.akey.keycomps      = 2;
    keyLimit.akey.ksubstr       = 1;

    component[1].indicator = SVCHIGHRANGE;
    pcomponent[1]          = &component[1];

    returnCode = keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength,
                           &keyLimit.akey);
    if (returnCode)
    {
        returnCode = DSM_S_FAILURE;
        goto done;
    }
 
    /* Now do the Find EXCLUSIVE */
    returnCode = cxFind(pcontext, &keyBase.akey, &keyLimit.akey,
                        NULL /* No cursor */,
                        SCTBL_AREA, pareaRecid,
                        DSMUNIQUE, DSMFINDFIRST, lockMode);
    if (returnCode)
        goto done;

done:
    return returnCode;

}  /* end dbAreaGetRecid */


/* PROGRAM: dbExtentRecordCreate - Create an extent record.
 *          
 * NOTE: Since the record is written to the recovery area,
 *       this operation is not recoverable! (6/18/98)
 *
 * RETURNS:
 */
dsmStatus_t
dbExtentRecordCreate(
        dsmContext_t   *pcontext,
        dsmArea_t       areaNumber,
        dsmExtent_t     extentNumber,
        LONG            extentSize,
        LONG            offset _UNUSED_,   /* not used yet */
        ULONG           extentType,
        dsmText_t      *extentPath)
{
    dbcontext_t    *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t     returnCode = DSM_S_FAILURE;
    lockId_t        lockId;
    dsmDbkey_t      extentRoot;
    dsmDbkey_t      areaRecid;
    dsmText_t       recordBuffer[RECSIZE];
    dsmText_t      *pextentRecord = &recordBuffer[0];
    dsmText_t      *pareaRecord = &recordBuffer[0];
    ULONG           recSize;
    ULONG           areaExtents;
    dsmBoolean_t    setTrans;
    ULONG           indicator;
    bkAreaDescPtr_t      *pbkAreas;
    bkAreaDesc_t         *pbkArea;
 
    svcDataType_t   component[3];
    svcDataType_t  *pcomponent[3];
    COUNT           maxKeyLength = 256;

    svcByteString_t  recTextValue;
 
    AUTOKEY(keyBase, 256)

    if(areaNumber == DSMAREA_TEMP)
      /* Don't need area/extent records for the temporary table area */
      return 0;

    /* find area recid */
    returnCode = dbAreaGetRecid(pcontext, areaNumber, &areaRecid, DSM_LK_EXCL);
    if (returnCode)
        goto done;

    /* save for index key delete */
    extentRoot = pdbcontext->pmstrblk->areaExtentRoot;
 
    if (!extentRoot)
    {
        /* TODO: create the index rather than error out */
        returnCode = DSM_S_INVALID_ROOTBLOCK;
        goto done;
    }

    /* materialize _AreaExtent template record */
    recSize = recFormat(pextentRecord, RECSIZE, SCFLD_EXTMIN-1);
    if (recSize > RECSIZE)
        goto badExit;
 
    /* Format misc field */
 
    returnCode = recInitArray(pextentRecord, RECSIZE,  &recSize,
                              SCFLD_EXTMISC, 8);
    if ( returnCode != RECSUCCESS )
        goto done;

    /**** Fillin the extent record ****/
    /* 1st field is ALWAYS table number */
    if (recPutLONG(pextentRecord, RECSIZE, &recSize,
                   1, (ULONG)0, SCTBL_EXTENT, 0) )
        goto badExit;
 
    /* set _Area-recid */
    if (recPutLONG(pextentRecord, RECSIZE, &recSize,
                   SCFLD_EXTAREA, (ULONG)0, areaRecid, 0) )
        goto badExit;
 
    /* set _Extent-version */
    if (recPutLONG(pextentRecord, RECSIZE, &recSize,
                   SCFLD_EXTVERS, (ULONG)0, 1, 0) )
        goto badExit;
 
    /* set _Extent-number */
    if (recPutLONG(pextentRecord, RECSIZE, &recSize,
                   SCFLD_EXTNUM, (ULONG)0, extentNumber, 0) )
        goto badExit;
 
    /* set _Area-number */
    if (recPutLONG(pextentRecord, RECSIZE, &recSize,
                   SCFLD_EXTANUM, (ULONG)0, areaNumber, 0) )
        goto badExit;
 
    /* set _Extent-type */
    if (recPutLONG(pextentRecord, RECSIZE, &recSize,
                   SCFLD_EXTTYPE, (ULONG)0, extentType, 0) )
        goto badExit;
 
    /* set _Extent-attrib */
    if (recPutLONG(pextentRecord, RECSIZE, &recSize,
                   SCFLD_EXTATTR, (ULONG)0, (int)0, 0) )
        goto badExit;
 
    /* set _Extent-system */
    if (recPutLONG(pextentRecord, RECSIZE, &recSize,
                   SCFLD_EXTSYS, (ULONG)0, (int)0, 0) )
        goto badExit;
 
    /* set _Extent-size (if not fixed length then size = 0) */
    if ( !(extentType & BKFIXED) )
        extentSize = 0;
    else
    {
        /* The input size is in # of blocks.  We need to change
         * this into # of FLUNITS for bkaddr to work correctly.
         */
        pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
        pbkArea = XBKAREAD(pdbcontext, pbkAreas->bk_qAreaDesc[areaNumber]);
        extentSize = (extentSize * pbkArea->bkBlockSize) / FLUNITS;
    }
    if (recPutLONG(pextentRecord, RECSIZE, &recSize,
                   SCFLD_EXTSIZE, (ULONG)0, extentSize, 0) )
        goto badExit;

    /* set _Extent-path */
    recTextValue.pbyte  = extentPath;
    recTextValue.length = stlen(extentPath);
    /* TODO: remove the following when rec services is working properly! */
    recTextValue.size   = recTextValue.length;
    if (recPutBYTES(pextentRecord, RECSIZE, &recSize,
                    SCFLD_EXTPATH, (ULONG)0, &recTextValue, 0) )
        goto badExit;

    recSize--;  /* omit the ENDREC from the recSize */

    /**** _AreaExtent record generation completed ****/


    /**** Write out the extent record ****/
    lockId.dbkey = rmCreate(pcontext, DSMAREA_CONTROL, SCTBL_EXTENT,
                            pextentRecord, (COUNT)recSize,
                            (UCOUNT)RM_LOCK_NEEDED);

    lockId.table = SCTBL_EXTENT;

    /**** Build the SCIDX_EXTAREA index for the _areaExtent Table ****/
 
    keyBase.akey.area         = DSMAREA_CONTROL;
    keyBase.akey.root         = extentRoot;
    keyBase.akey.unique_index = DSMINDEX_UNIQUE;
    keyBase.akey.word_index   = 0;
 
    keyBase.akey.index    = SCIDX_EXTAREA;
    keyBase.akey.keycomps = 2;
    keyBase.akey.ksubstr  = 0;
 
    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];
 
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = (LONG)areaNumber;
 
    component[1].type            = SVCSMALL;
    component[1].indicator       = 0;
    component[1].f.svcSmall      = extentNumber;
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &keyBase.akey))
        goto badExit;
 
    if (pcontext->pusrctl->uc_task)
        setTrans = 0;
    else
    {
        pcontext->pusrctl->uc_task = 1;  /* fake out cxAddNL */
        setTrans = 1;
    }
    returnCode = cxAddEntry(pcontext, &keyBase.akey, &lockId, 
                            (TEXT *)"dsmExtentRecordCreate", 
                            SCTBL_EXTENT);

    if (setTrans)
        pcontext->pusrctl->uc_task = 0;

    if (returnCode == IXDUPKEY)
    {
        rmDelete (pcontext, SCTBL_EXTENT, lockId.dbkey, NULL);
        returnCode = DSM_S_INVALID_EXTENT_CREATE;
        goto done;
    }
 
 
    /***** END AREAEXTENT RECORD CREATION *****/

    /* Now update area record with number of extents and maxdbkey value */
    recSize = rmFetchRecord(pcontext, DSMAREA_CONTROL,
                               areaRecid, pareaRecord,
                               RECSIZE, 0);
    if ( recSize > RECSIZE)
        goto badExit;

    if (recGetLONG(recordBuffer, SCFLD_AREAEXT, (ULONG)0,
                   (LONG *)&areaExtents,  &indicator) )
        goto badExit;

    areaExtents++;

    /* set _Area-extents */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREAEXT, (ULONG)0, areaExtents, 0) )
        goto badExit;

    returnCode = rmUpdateRecord(pcontext, SCTBL_EXTENT, DSMAREA_CONTROL, areaRecid,
                        pareaRecord, (COUNT)recSize);

    if (returnCode)
        goto done;

   /* if no logging, we'll do the flush right here. */
   if (!pcontext->pdbcontext->prlctl)
       bmFlushx(pcontext, FLUSH_ALL_AREAS, 1);

done:
    return returnCode;

badExit:
    return DSM_S_FAILURE;

}  /* end dbExtentRecordCreate */


/* PROGRAM: dbExtentRecordDelete - Delete an extent record.
 *          
 * NOTE: Since the record is written to the recovery area,
 *       this operation is not recoverable! (6/18/98)
 *
 * RETURNS:
 */
dsmStatus_t
dbExtentRecordDelete(
        dsmContext_t    *pcontext,
        dsmExtent_t      extentNumber,
        dsmArea_t        areaNumber)
{
    dsmStatus_t     returnCode = -1;
    dsmStatus_t     ret = -1;
    lockId_t        lockId;
    dsmDbkey_t      extentRoot;
    dsmDbkey_t      extentRecid;
    dsmDbkey_t      areaRecid;
    ULONG           indicator;
    ULONG           areaExtents;
    dsmText_t       recordBuffer[RECSIZE];
    dsmText_t      *pareaRecord = &recordBuffer[0];
    ULONG          recSize;
 
    svcDataType_t component[3];
    svcDataType_t *pcomponent[3];
    COUNT         maxKeyLength = 256;
 
    AUTOKEY(keyBase, 256)
    AUTOKEY(keyLimit, 256)

    /* find area recid */
    returnCode = dbAreaGetRecid(pcontext, areaNumber, &areaRecid, DSM_LK_EXCL);
    if (returnCode)
        goto done;

    /* save for index key delete */
    extentRoot = pcontext->pdbcontext->pmstrblk->areaExtentRoot;
 
    if (!extentRoot)
    {
        returnCode = DSM_S_INVALID_ROOTBLOCK;
        goto done;
    }
 
    /**** Remove the SCIDX_EXTAREA index entry for this record ****/
    keyBase.akey.area         = DSMAREA_CONTROL;
    keyBase.akey.root         = extentRoot;
    keyBase.akey.unique_index = DSMINDEX_UNIQUE;
    keyBase.akey.word_index   = 0;
    keyBase.akey.index        = SCIDX_EXTAREA;
    keyBase.akey.keycomps     = 2;
    keyBase.akey.ksubstr      = 0;
 
    pcomponent[0] = &component[0];
    pcomponent[1] = &component[1];
 
    component[0].type            = SVCLONG;
    component[0].indicator       = 0;
    component[0].f.svcLong       = (LONG)areaNumber;
 
    component[1].type            = SVCSMALL;
    component[1].indicator       = 0;
    component[1].f.svcSmall      = extentNumber;
 
    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &keyBase.akey))
        goto done;

    keyLimit.akey.area         = DSMAREA_CONTROL;
    keyLimit.akey.root         = extentRoot;
    keyLimit.akey.unique_index = DSMINDEX_UNIQUE;
    keyLimit.akey.word_index   = 0;
    keyLimit.akey.index        = SCIDX_EXTAREA;
    keyLimit.akey.keycomps     = 3;
    keyLimit.akey.ksubstr      = 0;

    component[2].indicator     = SVCHIGHRANGE;
    pcomponent[2]              = &component[2];

    if (keyBuild(SVCV8IDXKEY, pcomponent, maxKeyLength, &keyLimit.akey))
        goto done;

    /* Now do the Find EXCLUSIVE */
    ret = cxFind(pcontext, &keyBase.akey, &keyLimit.akey,
                 NULL /* No cursor */,
                 SCTBL_EXTENT, (dsmRecid_t *)&extentRecid,
                 DSMUNIQUE, DSMFINDFIRST, DSM_LK_EXCL);
    if (ret)
        goto done;
 
    /* remove its indexes as well */
    lockId.dbkey = extentRecid;
    lockId.table  = SCTBL_EXTENT;
 
    ret = cxDeleteEntry(pcontext, &keyBase.akey, &lockId, IXLOCK, 
                        SCTBL_EXTENT, NULL);
    if (ret)
        goto done;
 
    ret = rmDelete(pcontext, SCTBL_EXTENT, extentRecid, NULL);
    if (ret)
        goto done;
 
    /* Now update area record with number of extents value */
    recSize = rmFetchRecord(pcontext, DSMAREA_CONTROL,
                           areaRecid, pareaRecord,
                            RECSIZE, 0);
    if ( recSize > RECSIZE)
        goto done;

    if (recGetLONG(pareaRecord, SCFLD_AREAEXT, (ULONG)0,
                   (LONG *)&areaExtents,  &indicator) )
        goto done;

    areaExtents--;

    /* set _Area-extents */
    if (recPutLONG(pareaRecord, RECSIZE, &recSize,
                   SCFLD_AREAEXT, (ULONG)0, areaExtents, 0) )
    {
        returnCode = -1;
        goto done;
    }

    returnCode = rmUpdateRecord(pcontext, SCTBL_EXTENT,
                        DSMAREA_CONTROL, areaRecid,
                        pareaRecord, (COUNT)recSize);
    if (returnCode)
        goto done;

    /* if no logging, we'll do the flush right here. */
    if (!pcontext->pdbcontext->prlctl)
       bmFlushx(pcontext, FLUSH_ALL_AREAS, 1);

done:
    return returnCode;

}  /* end dbExtentRecordDelete */

