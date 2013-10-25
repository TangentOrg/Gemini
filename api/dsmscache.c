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
#include "dbcontext.h"
#include "bkpub.h"
#include "smstattr.h"    /* needed for mandatory fields */
#include "scstr.h"       /* needed for mandatory fields */
#include "dbmeta.h"      /* need for meta file ctl */
#include "dsmpub.h"
#include "dsmret.h"
#include "latpub.h"
#include "stmpub.h"

#include <setjmp.h>

#define XMETACTL(pdbcontext, qmetactl)   \
                 ((struct meta_filectl *)QP_TO_P(pdbcontext, qmetactl))

#if DEBUG
#define DEBUG_FPRINTF(stderr, string)\
    FPRINT( stderr, string);
#define DEBUG_FPRINTF2(stderr, string, p1, p2)\
    FPRINT( stderr, string, (p1), (p2) );
#else
#define DEBUG_FPRINTF(stderr, string)
#define DEBUG_FPRINTF2(stderr, string, p1, p2)
#endif


/* PROGRAM: dsmMandatoryFieldsGet - find cached file metadata.  This cache
 *                                  is stored in shared memory
 *
 * RETURNS: DSM_S_SUCCES
 *
 */
dsmStatus_t
dsmMandatoryFieldsGet(dsmContext_t *pcontext,
           struct meta_filectl     *pmetactl,    /* mand. table from shm    */
           dsmDbkey_t              fileDbkey,    /* dbkey of _file record   */
           int                     fileNumber,   /* file # of _file record  */
           dsmBoolean_t            fileKey,      /* dbkey/file# of _file    */
           dsmBoolean_t            copyFound,    /* flag to copy mand. flds */
           struct mand             *pmand)       /* storage for mand. array */
{
 
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dsmStatus_t returnCode;
    int                     metaSize;
 
    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmMandatoryFieldsGet");
        goto done;
    }

    MT_LOCK_SCC ();
 
    /* Check if the entry already exists */
    dsmMandFindEntry(pcontext, &pmetactl, fileDbkey, fileNumber, fileKey);

    if (pmetactl != NULL)
    {
        DEBUG_FPRINTF(stderr, "dbmanb: cache entry found, returning\n");
 
        if (copyFound != 0)
        {
            metaSize = (pmetactl->nmand+1)*sizeof(struct mand);
            bufcop((TEXT *)pmand, (TEXT *)pmetactl->mand, metaSize);
        }
    }
 
    MT_UNLK_SCC ();

    returnCode = DSM_S_SUCCESS;
done:

    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;

    return returnCode;
 
}

/* PROGRAM: dsmMandatoryFieldsCachePut - build mandatory field cache and it
 *                                       is stored in shared memory
 *
 * RETURNS: DSM_S_SUCCES
 *
 */
dsmStatus_t
dsmMandatoryFieldsPut(dsmContext_t *pcontext,
           struct meta_filectl *pInMetactl,    /* new metadata ctl for cache */
           struct meta_filectl *pOldmetactl,   /* old metadata ctl for cache */
           DL_VECT             *pdl_vect_in,   /* next dl column vector */
           DL_VECT             *pdl_col_in,    /* next dl columns list to use */
           int                 ndl_ent,        /* # delayed columns */
           int                 mandtblSize,    /* size of mand fld table */
           int                 useCache,       /* use shm cache ? */
           struct mand         *pmand)         /* storage for all mand flds */
{
    dbcontext_t           *pdbcontext = pcontext->pdbcontext;
    dbshm_t               *pdbshm     = pdbcontext->pdbpub;
    dsmStatus_t           returnCode;
    SHPTR                 *pmandhashtbl, qhashentry;
    struct meta_filectl   *pmetactl = NULL;
 
    pdbcontext->inservice++; /* postpone signal handler processing */

    SETJMP_ERROREXIT(pcontext, returnCode) /* Ensure error exit address set */

    if ((returnCode = dsmThreadSafeEntry(pcontext)) != DSM_S_SUCCESS)
    {
        returnCode = dsmEntryProcessError(pcontext, returnCode,
                      (TEXT *)"dsmMandatoryFieldsPut");
        goto done;
    }
 
    /* get the schema cache latch */
    MT_LOCK_SCC ();
 
    /* Check if the entry already exists */
    dsmMandFindEntry(pcontext, &pmetactl, pInMetactl->fildbk, 
                     pInMetactl->filno, FILE_IDENT_DBKEY);

    if (pmetactl != NULL)
    {
        /* entry had already been made */
        goto okdone;
    }

    /* get ptr to hash table array */
    if (pdbshm->qmandctl)
        pmandhashtbl = (SHPTR *) QP_TO_P(pdbcontext, pdbshm->qmandctl);
    else /* need to allocate it */
    {
        pmandhashtbl = (SHPTR *)
            stRent(pcontext, XSTPOOL(pdbcontext, pdbshm->qdbpool),
                   SIZE_MAND_HASH * sizeof(SHPTR));
        pdbshm->qmandctl = P_TO_QP(pcontext, pmandhashtbl);
    }

    /* allocate storage in shm */
    pmetactl = (struct meta_filectl *)stRent(pcontext,
                       XSTPOOL(pdbcontext, pdbshm->qdbpool),
                       (unsigned)(sizeof(struct meta_filectl) + mandtblSize));
 
    pmetactl->fildbk            = pInMetactl->fildbk;
    pmetactl->filno             = pInMetactl->filno;
    pmetactl->nmand             = pInMetactl->nmand;
    pmetactl->schemavers        = pInMetactl->schemavers;
    if ( (useCache != MANB_USECACHE_NONE) &&
         (mandtblSize > 0) )
    {
        bufcop(pmetactl->mand, pmand, (int) mandtblSize);
    }
 
    if ((useCache == MANB_USECACHE_PHYSICAL) &&
        (pOldmetactl != NULL))
    {
        pOldmetactl->filno          = 0;
        pOldmetactl->fildbk         = 0;
    }
 
    /* Chain this structure in shared memory.  The assumption is that
       an entry for this fildbk does not already exist on the cache. */
    qhashentry = pmandhashtbl[ABS(pmetactl->filno) % SIZE_MAND_HASH];
        
    pmetactl->qnext   = qhashentry;
    pmandhashtbl[ABS(pmetactl->filno) % SIZE_MAND_HASH] =
        P_TO_QP(pcontext, pmetactl);
 
    DEBUG_FPRINTF(pmetactl, "dbmanb (chained to sh mem):");

okdone:
    MT_UNLK_SCC ();

    returnCode = DSM_S_SUCCESS;

done:
    dsmThreadSafeExit(pcontext);
    pdbcontext->inservice--;

    return returnCode;
}

/* PROGRAM: dsmMandFindEntry - find the meta_filectl entry by dbkey or table
 * number (preferably by table number) in the shared memory mandatory
 * structures; fills in a pointer passed to it. ASSUMES that the caller has
 * obtained the appropriate (MT_LOCK_SCC()) latch before calling
 */
dsmStatus_t
dsmMandFindEntry(
               dsmContext_t         *pcontext,
               struct meta_filectl **ppmetactl,
               dsmDbkey_t           fileDbkey,    /* dbkey of _file record   */
               int                  fileNumber,   /* file # of _file record  */
               dsmBoolean_t         fileKey)      /* dbkey/file# of _file    */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    dbshm_t     *pdbshm     = pcontext->pdbcontext->pdbpub;
    SHPTR                 *pmandhashtbl, qhashentry;
    struct meta_filectl     *pmetactl = NULL;
    int tempi;

    if (pdbshm->qmandctl == 0)
    {
        *ppmetactl = NULL;
        return DSM_S_FAILURE;
    }

    pmandhashtbl = (SHPTR *) QP_TO_P(pdbcontext, pdbshm->qmandctl);
    if (fileKey == FILE_IDENT_FILNO)
    {
        qhashentry = pmandhashtbl[ABS(fileNumber) % SIZE_MAND_HASH];
        for (pmetactl = (META_FILECTL *)QP_TO_P(pdbcontext, qhashentry);
             (pmetactl != NULL) && (pmetactl->filno != fileNumber);
             pmetactl = XMETACTL(pdbcontext, pmetactl->qnext))
            ;
    }
    else
    {
        for (tempi = 0, pmetactl = NULL;
             pmetactl == NULL && tempi < SIZE_MAND_HASH; tempi++)
        {
            qhashentry = pmandhashtbl[tempi];
            for (pmetactl = (META_FILECTL *)QP_TO_P(pdbcontext, qhashentry);
                 pmetactl;
                 pmetactl = (META_FILECTL *)QP_TO_P(pdbcontext,
                                                    pmetactl->qnext))
            {
                  if (pmetactl->fildbk == fileDbkey)
                      break;
            }
        }
    }
    *ppmetactl = pmetactl;

    return DSM_S_SUCCESS;
}
