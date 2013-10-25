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

#ifndef PSCOS_H
#define PSCOS_H 1

/*
* Define what OS we're using. 
*/

/********************************/
/* Operating Systems supported  */
/********************************/
#include "pscmach.h"

#define UNIX     1
#define MS_DOS   2
#define VMS	 3
#define BTOS     4
#define VRX	 5
#define OS2	 6
#define VM       7
#define OS400    8
#define MPEIX    9
#define WIN32API 10

/* For each machine define the operating system in use, and if
   UNIX, the version as follows:

   UNIXV7       on if this is a Unix Version 7 derivative.

   UNIXSYS3     on if this is a UNIX System 3 derivative.

   UNIXSYS5     on if this is a UNIX System 5 derivative.

   UNIXBSD2     on if this is a Berkley BSD 4.2 derivative.

   VMS		on if this is VMS.

*/

#define SHMEMORY 1

#if OPSYS==WIN32API
/* Define DBUT_LARGEFILE to enable large file (> 2GB) support on Windows */
#define DBUT_LARGEFILE 1
#endif

#if SHMEMORY
#else /* not shm machines */
#define SHMMAX 	6 * 1024 /* max dbpool memory segment size to allocate #*/
#endif

#if CT | ATT3B2300 | TOWERXP | ATT3B5 | VME121 \
	| ALTOS3068 | ABC9000 | SPERRY80 | SUPERTEAM | RT \
	| PYTHON | SYSTEMVAT | CCI | M8000 | HP320 | DDE | PRIME316 \
	| HONEYWELL | BULLQ700 | NIXDORF | MEGADATA | NECXL32 | MAC_AUX \
	| SCO | DGMV | EDISA | DIAB | ATT3B2600 | TOWER700 | ATT386 \
	| SIEMENS | BULLXPS | PS2AIX | ALT386 | SYS25ICL | MOTOROLA \
	| ICLDRS | BULLDPX2 | ARIX825 | ARIXS90 | TI1500 | IBMRIOS | ICL6000 \
	| NCR486 | ICL3000 | CT386 | SGI | SEQUENTPTX | DG5225 | MOTOROLAV4 \
        | UNIX486V4 | SUN45 | LINUX
#define OPSYS    UNIX
#define UNIXV7   0
#define UNIXSYS3 0
#define UNIXSYS5 1
#define UNIXBSD2 0
#endif

#if ATT3B2300 | ATT3B5 | VME121 | TOWERXP \
	| ALTOS3068 | CT | SPERRY80 | RT | ICONPORT | PYTHON | SYSTEMVAT | CCI \
	| M8000 | DDE | PRIME316 | HONEYWELL | NIXDORF | MEGADATA | NECXL32 \
	| SCO | EDISA | DIAB | ATT3B2600 | BULLQ700 | PS2AIX | SYS25ICL \
	| BULLXPS | ICLDRS | BULLDPX2 | SIEMENS | ARIX825 | ARIXS90 | TI1500 \
	| IBMRIOS | ICL6000 | SGI | SEQUENTPTX | DG5225 | SUN45
#define SYS5REL2 1
#endif

#if SUN | PYRAMID | VAX | ICONPORT | HP840 | SEQUENT | SUN4 | ROADRUNNER \
	| MIPS | OLIVETTI | DECSTN | ULTRIX | HARRIS | DG88K  | HP825 \
	| ALPHAOSF
#define OPSYS    UNIX
#define UNIXV7   0
#define UNIXSYS3 0
#define UNIXSYS5 0
#define UNIXBSD2 1
#endif

/* NOTE: IBMWIN is undef'd below and IBM is def'd in its place */
#if IBMWIN			/* WINDOWS 3 environment */
#define OPSYS    MS_DOS
#define MSC      1
#define MSC6     1
#define UNIXV7   0
#define UNIXSYS3 0
#define UNIXSYS5 0
#define UNIXBSD2 0
#endif

#if WINNT_INTEL | WINNT_ALPHA  /* NT environment */
#define OPSYS    WIN32API
#define MSC      1
#define MSC6     0
#define UNIXV7   0
#define UNIXSYS3 0
#define UNIXSYS5 0
#define UNIXBSD2 0
#endif


#if IBM
#define OPSYS    MS_DOS
#define MSC      1
#define MSC6	 1		/* 0 for MSC v5 or 4, 1 for MSC v6+ */
#ifdef OS286
#undef OS286
#define OS286    1
#else
#define OS286    0
#endif
#define UNIXV7   0
#define UNIXSYS3 0
#define UNIXSYS5 0
#define UNIXBSD2 0
#endif

#if UNISYSB20
#define OPSYS	 BTOS
#define UNIXV7	 0
#define UNIXSYS3 0
#define UNIXSYS5 0
#define UNIXBSD2 0
#endif

#if OS2_INTEL
#define OPSYS    OS2
#define UNIXV7   0
#define UNIXSYS3 0
#define UNIXSYS5 0
#define UNIXBSD2 0
#endif

/* VMS not used any more  */
#undef VMS

#endif /* PSCOS_H */
