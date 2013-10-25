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

#ifndef RLTLPUB_H
#define RLTLPUB_H

#define RLTL_BUFSIZE   16384

#define RLTL_OP_COMMIT 1
#define RLTL_OP_SCAN   2

#if 0
typedef struct rltlCounter
{
    ULONG    hi;
    ULONG    lo;
}rltlCounter_t;
#endif
struct bktbl;

LONG rltlOpen( dsmContext_t *pcontext );

LONG rltlQon( dsmContext_t *pcontext );

LONG rltlClose(dsmContext_t *pcontext);

LONG rltlCheckBlock(dsmContext_t *pcontext, struct bktbl *pbktbl);

LONG rltlSpool(dsmContext_t *pcontext, dsmTrid_t trid);

LONG rltlSeek(dsmContext_t  *pcontext, ULONG blockNumber, ULONG offset);

LONG rltlScan(dsmContext_t *pcontext, dsmTrid_t trid, int operation);

LONG rltlOn(dsmContext_t *pcontext,
            TEXT          coordinator,
            TEXT         *pnickName,
            int           begin);

#endif /* RLTLPUB_H  */


