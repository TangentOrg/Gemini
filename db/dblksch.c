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
#include "shmpub.h"

#include "dbpub.h"
#include "drmsg.h"
#include "dsmpub.h"  /* for DSM API */
#include "lkpub.h"
#include "svmsg.h"
#include "lkschma.h"
#include "usrctl.h"
#include "latpub.h"
#include "dbcontext.h"
#include "stmpub.h" /* storage management subsystem */
#include "stmprv.h"


/* PROGRAM: dblksch -- coordinate schema locks for db service
 *
 * RETURNS: 0 on success
 *          DSM_S_INVALID_UNLOCK_REQUEST if unlock when no locks 
 *          DSM_S_RQSTQUED
 *          SLCHG if schema has changed underneath the user.
 */
int
dblksch (
	dsmContext_t *pcontext _UNUSED_,
	int	rqst _UNUSED_) /* indicates requested function. May be SLSHARE,
			   SLRELS, SLEXCL, SLRELX, SLGUARSER, or SLRELGS. */

{
    return 0;
} /* end dblksch */
