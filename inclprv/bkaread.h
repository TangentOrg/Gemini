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

#ifndef BKAREAD_H
#define BKAREAD_H



#include "bkpub.h"
#include "dsmpub.h"

typedef struct bkAreaDesc
{
    ULONG        bkAreaNumber;       /* The Area Number this belongs to */
    ULONG        bkAreaType;         /* DSM?                            */
    ULONG        bkBlockSize;        /* Size of block in bytes for area */
    LONG         bkBackupNext;       /* next block to backup   */
    LONG         bkBackupLast;       /* last block to backup   */
    ULONG        bkNumFtbl;          /* The number of bkftbl allocated */
    ULONG        bkHiAfterUndo;      /* Hi water block after physical undo */
    ULONG        bkHiWaterBlock;     /* the area's block high water mark */
    DBKEY        bkMaxDbkey;         /* highest valid dbkey */
    DBKEY        bkChainFirst[3];    /* dbkey array for first block on chain */
    ULONG        bkNumBlocksOnChain[3]; /* # of blocks on each free chain */
    UCOUNT       bkAreaRecbits;      /* # of bits of dbkey for record offset */
    COUNT        bkDelete;           /* flag for being deleted or not */
    COUNT        bkStatus;           /* Has anything been updated in this area*/
    bkftbl_t     bkftbl[1];          /* file table anchors for each area 
                                        size is determined at runtime */
} bkAreaDesc_t;

/* use this macro for detrmine the size of bkAreaDesc_t */
#define BK_AREA_DESC_SIZE(x) (sizeof(bkAreaDesc_t) + \
                             ((x)-1) * sizeof(BKFTBL))

typedef struct bkAreaDescPtr
{
    ULONG        bkNumAreaSlots;     /* The number of areas allocated */
    QBKAREAD  bk_qAreaDesc[1];    /* file table anchors for each area 
                                        size is determined at runtime */
} bkAreaDescPtr_t;

/* use this macro for detrmine the size of bkAreaDescPtr_t */
#define BK_AREA_DESC_PTR_SIZE(x) (sizeof(bkAreaDescPtr_t) + \
                                 ((x)-1)*sizeof(QBKAREAD))


/**************************************************/
/*     entries in the database extent manager     */
/**************************************************/
/* passed as argument to indicate target of call */

#define BKAI    0       /* ai file */
#define BKDATA  1       /* data file */
#define BKBI    2       /* bi file */
#define BKTL    3       /* tl file */

#define BKETYPE 3       /* for extracting type */

/*************************************/
/* Area types                        */
/*************************************/
#define BK_AREA1      DSMAREA_CONTROL    /* Area containing the master block */
/*************************************/

/* useful macros */

#define    BK_GET_AREA_DESC(pdb, num) \
       XBKAREAD(pdb, \
 XBKAREADP(pdb, pdb->pbkctl->bk_qbkAreaDescPtr)->bk_qAreaDesc[(num)])

#if 0
#define BK_GET_AREA_FTBL(pdb, num) BK_GET_AREA_DESC(pdb, num)->bkftbl
#endif

#endif /* ifndef BKAREAD_H  */



