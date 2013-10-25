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

#ifndef PSCHACK_H
#define PSCHACK_H 1

/*
* Miscellaneous hacks and work-arounds.
*/

#ifndef	STCOPYDEF
#define	stcopy	p_stcopy	/* to avoid conflict with C-ISAM */
#endif

/****temporary - until all NEWMU refs eliminated ****/
#define NEWMU 1

/* Lint on the Sun has a bug:  does not understand -v option is argument
   is declared as FAST  -- -v option suppress complaints about unused
   arguments */
#ifdef DOLINT
#undef FAST
#define FAST
#endif

#endif /* PSCHACK_H  */
