
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

#include "pscret.h"
#include "dbconfig.h"

#include "bkaread.h"
#include "bkpub.h"
#include "bkprv.h"
#include "control.h"
#include "cxpub.h"
#include "dbcontext.h"
#include "dbpub.h"
#include "dsmpub.h"
#include "lkpub.h"
#include "recpub.h"
#include "rlpub.h"
#include "rmpub.h"
#include "scasa.h"
#include "stmpub.h"   /* storage management subsystem */
#include "stmprv.h"
#include "svcspub.h"
#include "usrctl.h"
#include "utfpub.h"
#include "utspub.h"

#include <setjmp.h>

/* PROGRAM: dsmAreaCreate - Create an area in the database
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
dsmAreaCreate(dsmContext_t 	*pcontext,     /* IN database context */
	      dsmBlocksize_t	blockSize,     /* IN are block size in bytes */
	      dsmAreaType_t	areaType,      /* IN type of area */
	      dsmArea_t		areaNumber,    /* IN area number */
              dsmRecbits_t	areaRecbits,   /* IN # of record bits */
              dsmText_t        *areaName)      /* IN name of new area */
{
    dbcontext_t          *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t           returnCode;

    pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmAreaCreate");
        goto done;
    }

    returnCode = dbAreaCreate(pcontext,blockSize,areaType,areaNumber,
                              areaRecbits, areaName);
    
 
done:
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;
    return returnCode;

}  /* end dsmAreaCreate */

/* PROGRAM: dsmAreaNew - Create an area in the database
 *
 *      dsmAreaNew finds an available area slot and creates a storage area
 *
 *      In addition, it will create the area record associated
 *      with this newly added area.
 *
 * RETURNS: DSM_S_SUCCESS on success
 *          DSM_S_AREA_NOT_INIT     area sub-system has not been initialized
 *          NOSTG                        storage allocation failure 
 */
dsmStatus_t 
dsmAreaNew(dsmContext_t 	*pcontext,     /* IN database context */
	      dsmBlocksize_t	blockSize,     /* IN are block size in bytes */
	      dsmAreaType_t	areaType,      /* IN type of area */
	      dsmArea_t		*pareaNumber,  /* OUT area number */
              dsmRecbits_t	areaRecbits,   /* IN # of record bits */
              dsmText_t        *areaName)      /* IN name of new area */
{
    dbcontext_t          *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t           returnCode;
    dsmArea_t             i;
    bkAreaDescPtr_t      *pbkAreas;
    
    pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmAreaCreate");
        goto done;
    }
    /* Find an available area slot                   */
    pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);
    
    if (pbkAreas == NULL)
    {
	/* Database probably hasn't been opened yet    */
	returnCode = DSM_S_AREA_NOT_INIT;
        goto done;
    }

    for( i = DSMAREA_BASE; i < pbkAreas->bkNumAreaSlots; i++)
    {
        if (pbkAreas->bk_qAreaDesc[i] == 0)
            break;
    }

    if ( i == pbkAreas->bkNumAreaSlots )
    {
        returnCode = DSM_S_OUT_OF_AREA_SLOTS;
        goto done;
    }
    *pareaNumber = i;
    
    returnCode = dbAreaCreate(pcontext,blockSize,areaType, i,
                              areaRecbits, areaName);
    
 
done:
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;
    return returnCode;
}


/* PROGRAM: dsmAreaDelete - Delete an area in the database
 *
 *      dsmAreaDelete() invalidates the area descriptor in the area cache.  
 *      This function does not cause any changes on disk.
 *
 * RETURNS: DSM_S_SUCCESS on success
 *          DSM_S_FAILURE on any failure
 */
dsmStatus_t 
dsmAreaDelete(dsmContext_t 	*pcontext,	/* IN database context */
	      dsmArea_t		areaNumber)     /* IN area to delete */
{
    dbcontext_t     *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t      returnCode;
    bkAreaDescPtr_t *pbkAreas;
#ifdef BK_MULTIPLE_EXTENTS
    bkAreaDesc_t    *pbkArea;
#endif
    bkAreaNote_t     biNote;
    
    pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmAreaDelete");
        goto done;
    }

    /* Get pointer to array of pointers to area descriptors in database
       shared memory                                                     */

    pbkAreas = XBKAREADP(pdbcontext, pdbcontext->pbkctl->bk_qbkAreaDescPtr);

    if (pbkAreas == NULL)
    {
	/* Database probably hasn't been opened yet    */
	returnCode = DSM_S_AREA_NOT_INIT;
        goto done;
    }

    if ( areaNumber >= pbkAreas->bkNumAreaSlots )
    {
	returnCode = DSM_S_AREA_NUMBER_TOO_LARGE;
        goto done;
    }
 
    if ( (areaNumber <= DSMAREA_SCHEMA) && (areaNumber != DSMAREA_TL) &&
	 (areaNumber != DSMAREA_TEMP))
    {
	returnCode = DSM_S_INVALID_AREANUMBER;
        goto done;
    }

#ifdef BK_MULTIPLE_EXTENTS
    pbkArea = XBKAREAD(pdbcontext, pbkAreas->bk_qAreaDesc[areaNumber]);

    if (pbkArea->bkNumFtbl)
    {
        /* extents exist for this area - can't delete it! */
        returnCode = DSM_S_AREA_HAS_EXTENTS;
        goto done;
    }
#endif

    /* NOTE: There could be a race condition here where a user is adding
     *       extents to this area after we have checked it and before
     *       we actually delete it.
     */
    INITNOTE( biNote, RL_BKAREA_DELETE, RL_LOGBI | RL_PHY );
    INIT_S_BKAREANOTE_FILLERS( biNote );	/* bug 20000114-034 by dmeyer in v9.1b */
    biNote.area        = areaNumber;
    rlLogAndDo( pcontext, (RLNOTE *)&biNote, 0, 0, NULL );

    /* NOTE: There could be a race condition here where a user is adding
     *       this area after we have deleted it from shm and before
     *       we have deleted its record.
     */
    /* now go delete the area record */
    returnCode = dbAreaRecordDelete(pcontext, areaNumber);

done:
     dsmThreadSafeExit(pcontext);

     pdbcontext->inservice--;
     return returnCode;

}  /* dsmAreaDelete */


/* PROGRAM: dsmExtentCreate - Create an extent in an area
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
dsmExtentCreate(dsmContext_t	*pcontext,	/* IN database context */
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

    pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmExtentCreate");
        goto done;
    }

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
    /* Flush the buffer pool since we need to make sure changes to
       the db file for new areas and extents get to disk right
       away.                                                 */
    bmFlushx(pcontext,DSMAREA_CONTROL,1);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;   /* Signals are OK now */
    return returnCode;

}  /* end dsmExtentCreate */


/* PROGRAM: dsmExtentDelete - Delete an extent from an area.
 *
 *    dsmExtentDelete() deletes the last extent for the specifed area 
 *    taking into account the information in extentMode.  If the EXFORCE 
 *    bit is zero, then the extent is deleted only if the high water mark 
 *    for the area has not yet entered the extent.  dsmExtentDelete() 
 *    invokes bkExtentDelete() to do the logical operation which actually 
 *    deletes the extent and logs the operation and then updates the 
 *    area/extent cache.
 *
 * RETURNS: DSM_S_SUCCESS on success
 *          DSM_S_FAILURE on any failure
 */
dsmStatus_t 
dsmExtentDelete(dsmContext_t	*pcontext,	/* IN database context */
	        dsmArea_t	areaNumber)	/* area to delete */
{
    dbcontext_t      *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t       returnCode;
    dsmExtent_t       extentNumber;
    bkExtentAddNote_t extentNote;
    bkAreaDescPtr_t  *pbkAreas;
    bkAreaDesc_t     *pbkArea;

    pdbcontext->inservice++; /* "post-pone" signal handling while in DSM API */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmExtentDelete");
        goto done;
    }

    /* validate areaNumber */
    if ( (areaNumber > DSMAREA_MAXIMUM) ||
         ((areaNumber < DSMAREA_SCHEMA)  &&
	  (areaNumber != DSMAREA_TEMP) &&
          (areaNumber != DSMAREA_BI) && areaNumber != DSMAREA_TL) )
    {
        returnCode = DSM_S_INVALID_AREANUMBER;
        goto done;
    }

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

    pbkArea = XBKAREAD(pdbcontext, pbkAreas->bk_qAreaDesc[areaNumber]);
    if(!pbkArea)
      goto done;

    if (!pbkArea->bkNumFtbl)
    {
        /* No extents exist for this area - can't delete it! */
        returnCode = DSM_S_AREA_NO_EXTENT;
        goto done;
    }

    extentNumber = pbkArea->bkNumFtbl;

    INITNOTE(extentNote, RL_BKEXTENT_DELETE, RL_LOGBI | RL_PHY);
    extentNote.area = pbkArea->bkAreaNumber;
    extentNote.extentNumber = extentNumber;
    rlLogAndDo(pcontext, (RLNOTE *)&extentNote, 0, 0, NULL);

    /* TODO: fix up shared memory */

    /* Now go remove the associated extent record. */
    returnCode = dbExtentRecordDelete(pcontext, extentNumber, areaNumber);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--;   /* Signals are OK now */
    return returnCode;

}  /* end dsmExtentDelete */

/* PROGRAM: dsmExtentUnlink - delete physical files that have previously
 *                            been marked for deletion
 *
 *
 * RETURNS: DSM_S_SUCCESS on success
 *          DSM_S_FAILURE on any failure
 */
dsmStatus_t
dsmExtentUnlink(dsmContext_t *pcontext)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */
 
    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmExtentUnlink");
        goto done;
    }

    returnCode = bkExtentUnlink(pcontext);
    rlSynchronousCP(pcontext);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return (returnCode);

}

/* PROGRAM: dsmAreaClose - close the files assocatiated with the area
 *
 * RETURNS: DSM_S_SUCCESS on success
 *          DSM_S_FAILURE on any failure
 */
dsmStatus_t
dsmAreaClose(dsmContext_t *pcontext, dsmArea_t area)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */
 
    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmAreaClose");
        goto done;
    }

    returnCode = bkAreaClose(pcontext, area);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return (returnCode);

}


/* PROGRAM: dsmAreaOpen - re-opens the files assocatiated with the area
 *
 * RETURNS: DSM_S_SUCCESS on success
 *          DSM_S_FAILURE on any failure
 */
dsmStatus_t
dsmAreaOpen(dsmContext_t *pcontext, dsmArea_t area, int refresh)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */
 
    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmAreaOpen");
        goto done;
    }

    returnCode = bkAreaOpen(pcontext, area, refresh);

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return (returnCode);
}


/* PROGRAM: dsmAreaFlush - flush buffers associated with the area
 *
 * RETURNS: DSM_S_SUCCESS on success
 *          DSM_S_FAILURE on any failure
 */
dsmStatus_t
dsmAreaFlush(dsmContext_t *pcontext, dsmArea_t area, int flags)
{
    dbcontext_t   *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t    returnCode = DSM_S_SUCCESS;

    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */
 
    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmAreaFlush");
        goto done;
    }

    if ((flags & FLUSH_BUFFERS))
    {
        bmFlushx(pcontext, area, (flags & FLUSH_SYNC) ? 1 : 0);
    }

    if ((flags & FREE_BUFFERS))
    {
        bmAreaFreeBuffers(pcontext, area);
    }

done:
    dsmThreadSafeExit(pcontext);

    pdbcontext->inservice--; /* reset signal handler processing */

    return (returnCode);
}

