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
#include "dsmpub.h"
#include "cxpub.h"
#include "dbpub.h"
#include "cxmsg.h"
#include "upmsg.h"
#include "bkpub.h"
#include "cxprv.h"
#include "bmpub.h"
#include "dbcontext.h"
#include "usrctl.h"
#include "ompub.h"

#if 0
NEEDS TABLE NUMBER FOR OM CALLS

/*************************************************************************/
/* PROGRAM: cxCheckBlock - check the consistency of an index block  
 *
 * RETURNS:
 *    0         The number of errors found in the block  
 *              has been queued.
 *
 */

int
cxCheckBlock(
	dsmContext_t	*pcontext,
	CXBLK		*pixblk )
{
	COUNT	lnent = pixblk->IXLENENT;	/* total length of entries */
	TEXT	*position; 		        /* block contents */
	TEXT	*pend;				/* prt to end of block */
	int	number_of_entries;              /* actual size of block */
	int	hs;				/* header size            */
	int	cs;				/* compaction of key    */
	int	ks;				/* key remaining        */
	int	is;				/* data size            */
	int	prevksz;			/* size of the previous key */
	int	elem;				/* element counter */
	TEXT	ecnt;
	COUNT	ixnum = pixblk->IXNUM;
	COUNT   errors_found = 0;               /* total errors found */
	omDescriptor_t indexDesc;
	dsmStatus_t   rc;

    /* if there is a root block, check if it is actually the root block
	for this index                                                 */
    if (pixblk->IXTOP)				/* this is a root block */
    {
	rc = omFetch(pcontext,DSMOBJECT_MIXINDEX,ixnum,&indexDesc);
	if( rc )
	    return 1;

	if (indexDesc.objectRoot != pixblk->BKDBKEY)
	{
	    /* %Bindex %d: block %D marked as root, but anchor is block %D */
	    MSGN_CALLBACK(pcontext, upMSG179,
                          ixnum, pixblk->BKDBKEY, indexDesc.objectRoot);
	    errors_found++;
	}
    }

    /* check if the block thinks it has a ligitimate amount of data */
    if(lnent < 0 || lnent > (int)MAXIXCHRS)
    {
	/* %Bindex %d, block %D: bad data length - %d, MAXIXCHRS is %d */
	MSGN_CALLBACK(pcontext, upMSG180,
                      ixnum, pixblk->BKDBKEY, lnent, (COUNT)MAXIXCHRS);
	errors_found++;
	return( errors_found );
    }

    position = &pixblk->ixchars[0];
    pend = position + lnent;
    number_of_entries = 0;
    prevksz = 0;
    elem = 0;
    while(position < pend)
    {
/*	cs = (int)position[0];		 compression size */ 
/*	ks = (int)position[1];		 size of uncompressed part of key */ 
/*	is = (int)position[2];		 information size */ 
    CS_KS_IS_HS_GET(position);

	/* if the compression of the key is greater then the size of the 
	   previous key, error */
	if(cs > prevksz)
	{
	    /* %Bindex %d, block %D, element no. %i: bad compression size */
            MSGN_CALLBACK(pcontext, upMSG372,
                          pixblk->IXNUM, pixblk->bk_hdr.bk_dbkey, elem);
            /* %B  prev size = %i, cs = %i, ks = %i, is = %i, key count = %i */
            MSGN_CALLBACK(pcontext, upMSG184,
                          prevksz, cs, ks, is, number_of_entries);
	    errors_found++;
	}

	if(pixblk->IXBOT == 0)		/* non-leaf block */
	{
	    if(cs == 0 && ks == 0)	/* dummy entry */
	    {
		if(number_of_entries != 0) /* must be the first entry */
		{
		    /* %Bindex %d, block %D, element no. %i: */
		    /* zero key size in non-leaf */
                    MSGN_CALLBACK(pcontext, upMSG373,
                                  pixblk->IXNUM, pixblk->bk_hdr.bk_dbkey, elem);
                    /* %B  prev size = %i, cs = %i, ks = %i, is = %i, key count = %i */
                    MSGN_CALLBACK(pcontext, upMSG184,
                                  prevksz, cs, ks, is, number_of_entries);
	            errors_found++;
		}
	    }
	    /* if the uncompacted size is greater then the max   */
	    else if(ks < 1 || ks + cs > MAXKEYSZ)
		 {
		    /* %Bindex %d, block %D, element no. %i: */
		    /* bad key size in non-leaf */
                    MSGN_CALLBACK(pcontext, upMSG374,
                                  pixblk->IXNUM, pixblk->bk_hdr.bk_dbkey, elem);
                    /* %B  prev size = %i, cs = %i, ks = %i, is = %i, key count = %i */
                    MSGN_CALLBACK(pcontext, upMSG184,
                                  prevksz, cs, ks, is, number_of_entries);
	            errors_found++;
		  }

#ifdef VARIABLE_DBKEY
/*	    if(is != ( *(position + ks + is + ENTHDRSZ - 1) & 0x0f) )  info size must be same as last nibble */ 
	    if(is != ( *(position + ks + is + hs - 1) & 0x0f) ) /* info size must be same as last nibble */
#else
	    if(is != 4)			/* info size must be 4 */
#endif  /* VARIABLE_DBKEY */
	    {	
		/* %Bindex %d, block %D, element no. %i: */
		/* bad info size in non-leaf */
                MSGN_CALLBACK(pcontext, upMSG375,
                              pixblk->IXNUM, pixblk->bk_hdr.bk_dbkey, elem);
                /* %B  prev size = %i, cs = %i, ks = %i, is = %i, key count = %i */
                MSGN_CALLBACK(pcontext, upMSG184,
                              prevksz, cs, ks, is, number_of_entries);
	        errors_found++;
	    }

	    number_of_entries++;
	}
	else				/* leaf block */
	{
	    if(cs == 0 && ks == 0)	/* dummy entry */
	    {
		/* must be the first entry */
		if(number_of_entries != 0 || is != 0)
		{
		    /* %Bindex %d, block %D, element no. %i: */
		    /* bad dummy key in leaf */
                    MSGN_CALLBACK(pcontext, upMSG376,
                                 pixblk->IXNUM, pixblk->bk_hdr.bk_dbkey, elem);
                    /* %B  prev size = %i, cs = %i, ks = %i, is = %i, key count = %i */
                    MSGN_CALLBACK(pcontext, upMSG184,
                                  prevksz, cs, ks, is, number_of_entries);
	            errors_found++;
		}

		number_of_entries++;
	    }
	    else			/* a real entry */
	    {
		if(ks < 1 || ks + cs > MAXKEYSZ)
		{
		    /* %Bindex %d, block %D, element no. %i: */
		    /* bad key size in leaf */
                    MSGN_CALLBACK(pcontext, upMSG377,
                                  pixblk->IXNUM, pixblk->bk_hdr.bk_dbkey, elem);
                    /* %B  prev size = %i, cs = %i, ks = %i, is = %i, key count = %i */
                    MSGN_CALLBACK(pcontext, upMSG184,
                                  prevksz, cs, ks, is, number_of_entries);
	            errors_found++;
		}

		if(is > 33)  /* 33 indicates use of bitmap. cannot be larger */
		{
		    /* %Bindex %d, block %D, element no. %i: */
		    /* bad info size in leaf */
                    MSGN_CALLBACK(pcontext, upMSG378,
                                  pixblk->IXNUM, pixblk->bk_hdr.bk_dbkey, elem);
                    /* %B  prev size = %i, cs = %i, ks = %i, is = %i, key count = %i */
                    MSGN_CALLBACK(pcontext, upMSG184,
                                  prevksz, cs, ks, is, number_of_entries);
	            errors_found++;
		}

		if(is == 33)
		{
		    /* number of entries is the first byte of the data area
			0 represents 256                   */
/*		    ecnt = position[ENTHDRSZ + ks]; */
		    ecnt = position[hs + ks];
		    number_of_entries += (ecnt == 0 ? 256 : ecnt);
		}
		else
                    /* at the leaf level, the data is in one byte tags */
		    number_of_entries += is;
	    }
	}
	prevksz = cs + ks;
/*	position += ENTHDRSZ + ks + is; */
	position += hs + ks + is;
	elem++;
    }

    if(position != pend)
    {
	/* %Bindex %d, block %D: block end mismatch: p = %p, pend = %p */
	MSGN_CALLBACK(pcontext, upMSG181,
                      ixnum, pixblk->BKDBKEY, position, pend);
	errors_found++;
    }
    if((COUNT)number_of_entries != pixblk->IXNUMENT)
    {
	/* %Bindex %d, block %D: entry count mismatch: nment = %d, found %i */
	MSGN_CALLBACK(pcontext, upMSG182, ixnum,
                      pixblk->BKDBKEY, pixblk->IXNUMENT, number_of_entries);
	errors_found++;
    }

    return(errors_found);
}


/*************************************************************************/
/* PROGRAM: cxCheckRoot - checks whether the anchor block of an index is  
 *       marked as a root and belongs to the same index.
 *
 * RETURNS:
 *    -1        index does not exist 
 *     n	number of errors found 
 */

int
cxCheckRoot(
	dsmContext_t	*pcontext,
	int		 ixnum )
{
    CXBLK  *proot;                   /* pointer to the root block  */
    DBKEY  root_dbk;                 /* hold the DBKEY of the root */
    int    errors_found = 0;
    bmBufHandle_t ixblkHandle;
    omDescriptor_t indexDesc;
    dsmStatus_t    rc;

    rc = omFetch(pcontext, DSMOBJECT_MIXINDEX, (COUNT)ixnum, &indexDesc);
    if( rc )
	return -1;

    root_dbk = indexDesc.objectRoot;
    
    if (root_dbk == 0)      /* this index doesn't exist */
	return (-1); 

    /* is it a valid block */

    ixblkHandle = bmLocateBlock(pcontext, indexDesc.area,root_dbk,TOREAD);
    proot = (CXBLK *)bmGetBlockPointer(pcontext,ixblkHandle);

    if (proot->bk_hdr.bk_type != IDXBLK ) /* is it an index block */
    {
        MSGN_CALLBACK(pcontext, upMSG293, ixnum );
        errors_found++;
    }
    else
    {
        if ( proot->IXNUM != ixnum )  /* is it the right index block */
        {
            /* %Bindex %d: anchor block %D has the wrong index no. - %d */
            MSGN_CALLBACK(pcontext, upMSG196, ixnum, root_dbk, proot->IXNUM);
            errors_found++;
        }
        else
            if (proot->IXTOP == 0) /* is it the root block */
            {
                /* %Bindex %d: anchor block %D is not a root.  TOP %d, BOT %d */
                MSGN_CALLBACK(pcontext, upMSG197, ixnum,
                              proot->BKDBKEY, proot->IXTOP, proot->IXBOT);
                errors_found++;
            }

    }

    bmReleaseBuffer(pcontext, ixblkHandle);

    return(errors_found);

}
#endif 

/*************************************************************************/
/* PROGRAM: cxGetIndexBlock - the requested block given by its dbkey and area
*  is located in the buffer and locked according to the requested lockmode.
*
* RETURNS: pointer to the block buffer in pbap->pblkbuf
*          buffer handle in pbap->bufHandle
*/
int
cxGetIndexBlock(
        dsmContext_t    *pcontext,
        cxbap_t         *pbap,          /* blk access parameters */
        ULONG            area,          /* area blk is in */
        DBKEY            dbkey,         /* dbkey of blk */
        UCOUNT           lockmode)     /* mode to lock blk buffer */
{
    pbap->blknum = dbkey;           
    pbap->bufHandle = bmLocateBlock(pcontext, area, dbkey, lockmode);
    pbap->pblkbuf = (cxblock_t *)bmGetBlockPointer(pcontext,pbap->bufHandle);

    return 0;
}
/* PROGRAM: cxVerifyIndexRoots - make sure that the index roots and number
 *                match what is passed
 *
 *
 * RETURNS:  -1 on Failure
 *            0 is everything matches
 *           +1 if there is a mismatch
 */
int
cxVerifyIndexRoots(dsmContext_t *pcontext, 
                   dsmText_t    *pname, 
                   LONG         *proot, 
                   LONG         *pindexNum,
                   dsmArea_t     objArea)
{
    ixAnchorBlock_t  *pindexAnchor;
    bmBufHandle_t     objHandle;
    CXBLK            *pRootBlock;
    COUNT             i;
    int               rc = 0;      /* assume success */

    /* check to see if the index root has changed */
    objHandle = bmLocateBlock(pcontext, objArea,
                            BLK3DBKEY(BKGET_AREARECBITS(objArea)), TOREAD);

    pindexAnchor = (ixAnchorBlock_t *)
                           bmGetBlockPointer(pcontext,objHandle);

    for (i = 0; i < IX_INDEX_MAX; i++)
    {
        if (stpcmp(pname, pindexAnchor->ixRoots[i].ixName) == 0)
        {
            /* found match */
            break;
        }
    }
    bmReleaseBuffer(pcontext, objHandle);

    if (i == IX_INDEX_MAX)
    {
        MSGN_CALLBACK(pcontext, bkMSG162, pname);
        MSGN_CALLBACK(pcontext, bkMSG163);
        for (i = 0; i < IX_INDEX_MAX; i++)
        {
            if (pindexAnchor->ixRoots[i].ixRoot != 0)
            {
                MSGN_CALLBACK(pcontext, bkMSG164, 
                    i, pindexAnchor->ixRoots[i].ixName, 
                    pindexAnchor->ixRoots[i].ixRoot);
            }
        }
        return -1;
    }

    if (pindexAnchor->ixRoots[i].ixRoot != *proot)
    {
        *proot = pindexAnchor->ixRoots[i].ixRoot;
         rc = 1;
    }

    /* find the root block and read the index number out of it */
    objHandle = bmLocateBlock(pcontext, objArea, *proot, TOREAD);
    pRootBlock = (CXBLK *)bmGetBlockPointer(pcontext,objHandle);

    /* make sure this is an index block */
    if (pRootBlock->bk_hdr.bk_type != IDXBLK ) /* is it an index block */
    {
        bmReleaseBuffer(pcontext, objHandle);
        MSGN_CALLBACK(pcontext, upMSG293, *pindexNum );
        return -1;
    }
    else
    {
        if ( pRootBlock->IXNUM != *pindexNum )
        {
            *pindexNum = pRootBlock->IXNUM;
             rc = 1;
        }
    }
    bmReleaseBuffer(pcontext, objHandle);

    return rc;
}
