
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
#include "ditem.h"
#include "keyprv.h"
#include "kypub.h"


/**************************************************************************
***
***     kyinitdb        initializes a ditem as a key ditem
***     kyins           inserts a data item into a key
**************************************************************************/

/* PROGRAM: kyinitdb - initialize ditem for ky
 *
 * RETURNS: 0 success
 *         -1 failure
 */
int
kyinitdb(
        struct ditem    *pkditem,
        COUNT            ixnum,
        COUNT            numcomps,
        GTBOOL            substrok)
{

    return (int) keyInitKDitem( pkditem, ixnum, numcomps, substrok );    
 
}  /* end kyinitdb */


/*****************************************************************/
/* PROGRAM: kyinsdb - routine to insert a data item into a key
 *
 * RETURNS:
 */
/*****************************************************************/
int
kyinsdb(
        struct ditem    *pkditem,
        struct ditem    *pditem,
        COUNT            compnum)
{

    keyStatus_t rc;
    
    rc = keyReplaceSDitemInKDitem(pkditem, pditem, compnum);
    
    if ( rc == DSM_S_SUCCESS )
        return 0;
    if ( rc == KEY_BUFFER_TOO_SMALL
      || rc == KEY_EXCEEDS_MAX_SIZE )
        return KYLNGKEY;
    else
        return -2;

}  /* end kyinsdb */
 

