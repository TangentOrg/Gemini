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

#ifndef RLTLPRV_H
#define RLTLPRV_H

#include "shmpub.h"         /* For definition of q pointers   */
#include "bmpub.h"
#include "rlpub.h"

typedef struct rltlCtl
{
    BKTBL         tlBuffer;
    ULONG         writeLocation;       /* Block number of current write */
    ULONG         writeOffset;         /* Offset within block of next write */
}rltlCtl_t;

typedef struct rltlBlockHeader
{
    COUNT    headerSize;
    COUNT    used;
    ULONG    blockNumber;
    ULONG    spare[2];     /* Guaranteed to be zero */
}rltlBlockHeader_t;

/* note used to start 2pc transaction logging            */
typedef struct rltlOnNote
{
    RLNOTE    rlnote;
    TEXT      begin;
    TEXT      coordinator;
    TEXT      nickName[MAXNICKNM+4];
}rltlOnNote_t;

#endif /* RLTLPRV_H */
