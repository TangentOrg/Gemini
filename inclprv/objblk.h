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

#ifndef OBJECT_H
#define OBJECT_H

/******************************************************/
/* object.h - Structure definitions for Object Block. */
/******************************************************/

#include "bkpub.h"  /* for struct bkhdr */

typedef struct objectBlock
{
    struct bkhdr bk_hdr;       /* standard block header */
    ULONG objVersion;          /* Object version number  */
    ULONG objStatus;           /* open, closed, repairing */
    ULONG totalBlocks;         /* total number of blocks in the area */
    ULONG hiWaterBlock;        /* highest block number used for db block */
    DBKEY chainFirst[3];       /* dbkey array for first block on chain */
    DBKEY chainLast[3];        /* dbkey array for last block on rm chain
                                * or index delete chain */
    ULONG64  totalRows;        /* Total rows in the gemini table  */
    ULONG64  autoIncrement;    /* Sequence value for the table    */
    DBKEY statisticsRoot;      /* Root block to the index selectivity
				  statistics.                     */
    ULONG numBlocksOnChain[3]; /* number of blocks on each free chain */
} objectBlock_t;


/* NOTE: each of the above chains has as a valid index array value:
 * FREECHN, RMCHN, or LOCKCHN
 */

#endif  /* #ifndef OBJECT_H */
