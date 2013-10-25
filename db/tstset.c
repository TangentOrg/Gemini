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

#include "dstd.h"

#if NOTSTSET
#include "mtctl.h"
#include "utsem.h"

tstset(ptr)
     TEXT *ptr;
{
    int semnum;
    
#if 0
    if(ptr == (TEXT *)&p_dbctl->pmtctl->spinfast)
	semnum = SPINSEM;
    else if (ptr == (TEXT *)&p_dbctl->pmtctl->dbfast)
	semnum = DBSEM;
    else if (ptr == (TEXT *)&p_dbctl->pmtctl->mtfast)
	semnum = MTSEM;
    else if (ptr == (TEXT *)&p_dbctl->pmtctl->debugfast)
	semnum = DEBUGSEM;
        else
	    msgn(mtFTL004); /* Illegal sempahore address */
    
    return ( uttstsem(p_dbctl->psemdsc->semid,semnum,-1));
#else
    GRONK This tstset function is obsolete!
#endif
}

tstclr(ptr)
     TEXT *ptr;
{
    int semnum;

#if 0
    if(ptr == (TEXT *)&p_dbctl->pmtctl->spinfast)
	semnum = SPINSEM;
    else if (ptr == (TEXT *)&p_dbctl->pmtctl->dbfast)
	semnum = DBSEM;
    else if (ptr == (TEXT *)&p_dbctl->pmtctl->mtfast)
	semnum = MTSEM;
    else if (ptr == (TEXT *)&p_dbctl->pmtctl->debugfast)
	semnum = DEBUGSEM;
    else
	msgn(mtFTL004); /* Illegal sempahore address */

    uttstsem(p_dbctl->psemdsc->semid,semnum,1);
#else
    GRONK This tstset function is obsolete!
#endif
}
#else /* !NOTSTSET */
/* stubs for lint to deal with calls to assembly routines */
#ifdef DOLINT
int
tstset(ptr)
     TEXT * ptr;
{MSGD("%GWRONG tstset.o");}
int
tstclr(ptr)
     TEXT *ptr;
{}
#endif
#endif /* end NOTSTSET */
