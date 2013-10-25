
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
#include "bkpub.h"
#include "bmpub.h"
#include "rlpub.h"
#include "rlai.h"
#include "mstrblk.h"

#include "utspub.h"

/*
 * PROGRAM: rlaoff -- turns off after imaging and returns status
 *
 * This program turns off after imaging by clearing the information in the
 * masterblock used by the ai subsystems.  It does not write the masterblock
 * back out, so that must happen before it is really turned off.
 *
 * RETURNS: 0 if ai was turned off, 1 if it was never on in the first place
 */
int
rlaoff(struct mstrblk *pmstr)
{
    /* clear information in masterblock for after imaging, except dates */
    pmstr->mb_aisync = 0;
    pmstr->mb_aiwrtloc = 0;
    pmstr->mb_aictr = 0;
    pmstr->mb_aiflgs = 0;
    pmstr->mb_aibusy_extent = 0;

    if (pmstr->mb_ai.ainew == 0)
    {
	/* dates are clear, after imaging was not on */
	 return 1;
     }

    /* after imaging was on, clear remaining dates */
    stnclr((TEXT *)&pmstr->mb_ai, sizeof(AIDATES));
    return 0;

} /* rlaoff */
 


