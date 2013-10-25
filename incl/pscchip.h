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

#ifndef PSCCHIP_H
#define PSCCHIP_H 1

/********************************/
/* Chip the MACHINE is based on */
/********************************/

#include "pscos.h"

#define M68000   3
#define INT8086  4
#define WE32000  5
#define HPPA     6  /* HP risc chip */
#define INT386   7
#define VAXCPU   8
#define I9370     9
#define AlphaAXP 10

#if ULTRIX | DECSTN | VAX
	/* Watch out for long lines and #if's with backslashes */
#define CHIP     VAXCPU
#define NUXI     1
#else
#if SCO | ALT386 | PRIME316 | SEQUENT | ROADRUNNER | ATT386 | PS2AIX| NW386 \
           | NCR486 | ICL3000 | SEQUENTPTX | CT386 | SIEMENS | WINNT_INTEL \
	   | OS2_INTEL | UNIX486V4 | WINNT_ALPHA | SOLARISX86 | LINUXX86
#define CHIP INT386
#define NUXI    1
#else
#if IBM | XENIXAT | INTEL286 | ALT286 | SYSTEMVAT | UNISYSB20 | IBMWIN
#define CHIP     INT8086
#define NUXI     1
#else
#if ATT3B2300 | ATT3B5 | ATT3B2600
#define CHIP    WE32000
#define NUXI     0
#else
#if IBM9370
#define CHIP I9370
#define MUXI 0
#else
#if ALPHAOSF
#define CHIP AlphaAXP
#define NUXI     1
#else
#define CHIP     M68000
#define NUXI     0
#endif
#endif
#endif
#endif
#endif
#endif

#endif /* PSCCHIP_H */
