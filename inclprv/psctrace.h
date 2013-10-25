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

#ifndef PSCTRACE_H
#define PSCTRACE_H 1

/*
* PROGRESS TRACE macros and definitions.
*/

#include "pscdtype.h"

/* include of ASSERT macros */
#include "assert.h"

#ifdef TRGLOBAL
GLOBAL GBOOL psctrace;
#else
IMPORT GBOOL psctrace;
#endif

#ifndef TRACEIT
#define TRACEIT 0
#else
#undef TRACEIT
#define TRACEIT 1
#endif

/*Trace message macros */
#if TRACEIT
#define TRACE(TMSG)             if (psctrace) msgt(TMSG);
#define TRACE1(TMSG,TA1)        if (psctrace) msgt(TMSG, TA1);
#define TRACE2(TMSG, TA1, TA2)  if (psctrace) msgt(TMSG, TA1, TA2);
#define TRACE3(TMSG, TA1, TA2, TA3) if (psctrace) msgt(TMSG, TA1, TA2, TA3);
#else
#define TRACE(TMSG) ;
#define TRACE1(TMSG,TA1) ;
#define TRACE2(TMSG, TA1, TA2) ;
#define TRACE3(TMSG, TA1, TA2, TA3) ;
#endif

#endif /* PSCTRACE_H */
