
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
#include "omprv.h"
#include "dbcontext.h"
#include "rlpub.h"
#include "utospub.h"
#include "rlprv.h"
#include "scasa.h"
#include "dbmsg.h"

/* PROGRAM: omUndo -- logical backout for both dynamic and crash
 *          This program does a logical undo of an om operation
 *
 * RETURNS: DSMVOID
 */
DSMVOID
omUndo (
        dsmContext_t    *pcontext,
        RLNOTE          *pnote)
{
    objectNote_t *pobjectNote = (objectNote_t *)pnote;

    if(pcontext->pdbcontext->prlctl->rlwarmstrt)
      if(SCTBL_IS_TEMP_OBJECT(pobjectNote->objectNumber))
	return;

    switch(pnote->rlcode)
    {
      case RL_OBJECT_CREATE:
      case RL_OBJECT_UPDATE:
        /* undoing a create or update -
         * must remove entry from cache if it exists.
         */
        omDelete(pcontext, pobjectNote->objectType, pobjectNote->tableNum, 
                 pobjectNote->objectNumber);
        break;
      default:
        FATAL_MSGN_CALLBACK(pcontext,dbFTL007,pnote->rlcode);
        break;
    }

}  /* end omUndo */
