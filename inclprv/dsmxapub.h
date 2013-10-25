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

#ifndef DSMXAPUB_H
#define DSMXAPUB_H

#include "dsmdtype.h"
#include "xa.h"

#define  DSM_XIDDATASIZE        XIDDATASIZE
#define  DSM_MAXGTRIDSIZE       MAXGTRIDSIZE
#define  DSM_MAXBQUALSIZE       MAXBQUALSIZE

typedef struct xid_t dsmXID_t;

dsmStatus_t DLLEXPORT
dsmXAcommit(dsmContext_t *pcontext,dsmXID_t *pxid, LONG flags);

dsmStatus_t DLLEXPORT
dsmXAend(dsmContext_t *pcontext,dsmXID_t *pxid, LONG flags);

dsmStatus_t DLLEXPORT
dsmXAprepare(dsmContext_t *pcontext,dsmXID_t *pxid, LONG flags);

dsmStatus_t DLLEXPORT
dsmXArecover(dsmContext_t *pcontext,dsmXID_t *pxid, LONG xidArraySize,
         dsmXID_t  *pxids, LONG *pxidsReturned,
         LONG *pxidsSoFar, LONG flags);

dsmStatus_t DLLEXPORT
dsmXArollback(dsmContext_t *pcontext,dsmXID_t *pxid, LONG flags);

dsmStatus_t DLLEXPORT
dsmXAstart(dsmContext_t *pcontext,dsmXID_t *pxid, LONG flags);

/* Values for dsmXA flags parameter           */
#define DSM_TMNOFLAGS           TMNOFLAGS
#define DSM_TMRESUME            TMRESUME
#define DSM_TMJOIN              TMJOIN
#define DSM_TMFAIL              TMFAIL
#define DSM_TMSUCCESS           TMSUCCESS
#define DSM_TMONEPHASE          TMONEPHASE
#define DSM_TMSUSPEND           TMSUSPEND

/* dsmXA return values                        */
#define DSM_XA_RBROLLBACK       XA_RBROLLBACK

/* dsmXA error returns                        */
#define DSM_S_XAER_DUPID        XAER_DUPID
#define DSM_S_XAER_OUTSIDE      XAER_OUTSIDE
#define DSM_S_XAER_NOTA         XAER_NOTA
#define DSM_S_XAER_INVAL        XAER_INVAL
#define DSM_S_XAER_RMFAIL       XAER_RMFAIL
#define DSM_S_XAER_PROTO        XAER_PROTO

#endif /* ifndef DSMXAPUB_H */

