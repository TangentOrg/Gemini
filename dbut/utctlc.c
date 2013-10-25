
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

/* Table of Contents - ctl-c utility functions:
 *
 *  utsctlc     - set the Control-C bit on in intflag
 *  utssoftctlc - set the soft Control-C bit on in intflag.
 *  utpctlc     - query whether the Control-C bit is in X event queue
 *                or set in intflag. Called while waiting for a record
 *                lock.
 * 
 */

 /*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "utcpub.h"


/* PROGRAM: utsctlc - set the Control-C bit on in intflag
 *          This routine is called for Progress-simulated ^Cs.
 *
 * RETURNS: DSMVOID
 */
DSMVOID 
utsctlc()
{

}

/* PROGRAM: utssoftctlc - set the soft Control-C bit on in intflag.
 *          It should be called when some internal STOP condition occurs.
 *
 * RETURNS: int
 */
int
utssoftctlc()
{

    return(0);
}

/* PROGRAM: utpctlc - query whether the Control-C bit is in X event queue
 *                    or set in intflag. Called while waiting for a record
 *                    lock
 *
 * RETURNS: int
 */
int
utpctlc()
{

    return (0);
}
