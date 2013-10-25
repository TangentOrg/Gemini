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

/*
 * Revision 1.3  1995/12/22 14:44:53  richb
 * Mod #40320 by richb
 * Cleaned up macros
 *
 * Revision 1.2  95/03/02  09:16:33  jao
 * This checkin versions the dbmgr code as it stood after re-integrating
 * back into SCC from CCM.  Any questions regarding this checkin should
 * be directed to members of the DB Development Team (db-dev). 
 *  */
/*
 * $Header: /usr1/mattg/dev/mysql2/mysql/gemini/incl/RCS/bkchk.h,v 1.1 2000/10/02 14:50:38 mikef Exp $
 */

/* bkchk.h - define macro for bk i/o test
*/

#if !PSC_ANSI_COMPILER || defined(BKBUM_H)

#ifndef BKCHK
#define BKCHK 0
#else
#undef BKCHK
#define BKCHK 1
#endif

#if BKCHK
#define BKVERIFY(whocalled, pdbcontext)	bkverify((whocalled), (pdbcontext));
#else
#define BKVERIFY(whocalled, pdbcontext)	;
#endif

#else
    "bkchk.h must be included via bkpub.h."
#endif  /* #if !PSC_ANSI_COMPILER || defined(BKBUM_H) */

