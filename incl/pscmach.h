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

#ifndef PSCMACH_H
#define PSCMACH_H 1

/*
* Machine definitions.
*/

/* On Unix platforms, OPSYS is set in gem_config.h (generated during
** the configure process).  On Windows, OPSYS is set in gem_config_win.h.
** The rest of the platforms are set to zero below. 
*/

/**********************************************************************/
/* Machines no longer supported.  Can't just remove the defines until */
/* we remove ALL references in the source code!                       */
/**********************************************************************/

/***********************/
/* Machines we support */
/***********************/
#ifndef ALTOS3068
#define ALTOS3068       0
#endif
#ifndef IBM
#define IBM             0
#endif
#ifndef ATT3B2300
#define ATT3B2300       0
#endif
#ifndef SUN
#define SUN             0
#endif
#ifndef PYRAMID
#define PYRAMID         0
#endif
#ifndef VAX
#define VAX             0
#endif
#ifndef ULTRIX
#define ULTRIX          0
#endif
#ifndef DECSTN
#define DECSTN          0
#endif
#ifndef TOWERXP
#define TOWERXP         0       /* includes tower32, tower800, ... */
#endif
#ifndef TOWER32
#define TOWER32         0       /* enable compile tower32/600 specific code */
#endif
#ifndef TOWER650
#define TOWER650        0       /* enable compile tower650 specific code */
#endif
#ifndef TOWER800
#define TOWER800        0       /* enable compile tower800 specific code */
#endif
#ifndef TOWER700
#define TOWER700        0       /* enable compile tower700 specific code */
#endif
#ifndef INTEL286
#define INTEL286        0
#endif
#ifndef HP320
#define HP320           0
#endif
#ifndef ATT3B5
#define ATT3B5          0
#endif
#ifndef VME121
#define VME121          0
#endif
#ifndef ABC9000
#define ABC9000         0
#endif
#ifndef CT
#define CT              0       /* includes mini, mighty, mega */
#endif
#ifndef SPERRY80
#define SPERRY80        0
#endif
#ifndef SUPERTEAM
#define SUPERTEAM       0
#endif
#ifndef ALT286
#define ALT286          0
#endif
#ifndef M8000
#define M8000           0
#endif
#ifndef ICONPORT
#define ICONPORT        0
#endif
#ifndef RT
#define RT              0
#endif
#ifndef PYTHON
#define PYTHON          0
#endif
#ifndef SYSTEMVAT
#define SYSTEMVAT       0
#endif
#ifndef XENIXAT
#define XENIXAT         0
#endif
#ifndef CCI
#define CCI             0
#endif
#ifndef UNISYSB20
#define UNISYSB20       0
#endif
#ifndef VRX8600
#define VRX8600         0
#endif
#ifndef HP840
#define HP840           0       /* includes hp825 */
#endif
#ifndef HP825
#define HP825           0       /* enable compile hp825 specific code */
#endif
#ifndef HP3000
#define HP3000          0
#endif
#ifndef DDE
#define DDE             0
#endif
#ifndef PRIME316
#define PRIME316        0
#endif
#ifndef SEQUENT
#define SEQUENT         0
#endif
#ifndef SEQUENTPTX
#define SEQUENTPTX      0
#endif
#ifndef HONEYWELL
#define HONEYWELL       0
#endif
#ifndef NECXL32
#define NECXL32         0
#endif
#ifndef ALT386
#define ALT386          0
#endif
#ifndef SCO
#define SCO          0
#endif
#ifndef OS2AT
#define OS2AT           0
#endif
#ifndef ROADRUNNER
#define ROADRUNNER      0
#endif
#ifndef SUN4
#define SUN4            0
#endif
#ifndef MAC_AUX
#define MAC_AUX         0
#endif
#ifndef BULLQ700
#define BULLQ700        0
#endif
#ifndef BULLDPX2
#define BULLDPX2        0
#endif
#ifndef BULLXPS
#define BULLXPS         0
#endif
#ifndef SIEMENS
#define SIEMENS         0
#endif
#ifndef NIXDORF
#define NIXDORF         0
#endif
#ifndef MEGADATA
#define MEGADATA        0
#endif
#ifndef IBM9370
#define IBM9370         0
#endif
#ifndef DGMV
#define DGMV            0
#endif
#ifndef MIPS
#define MIPS            0
#endif
#ifndef SUNOS4
#define SUNOS4          0
#endif
#ifndef CTMIGHTY
#define CTMIGHTY        0
#endif
#ifndef EDISA
#define EDISA           0
#endif
#ifndef OLIVETTI
#define OLIVETTI        0
#endif
#ifndef DIAB
#define DIAB            0
#endif
#ifndef ARIX
#define ARIX            0       /* for dual port on arix825 w/ excelan */
#endif
#ifndef ARIX825
#define ARIX825         0
#endif
#ifndef ARIXS90
#define ARIXS90         0
#endif
#ifndef XSEQUENT
#define XSEQUENT        0
#endif
#ifndef DG88K
#define DG88K           0
#endif
#ifndef DG5225
#define DG5225          0
#endif
#ifndef ATT3B2600
#define ATT3B2600       0
#endif
#ifndef HARRIS
#define HARRIS          0
#endif
#ifndef ATT386
#define ATT386          0
#endif
#ifndef UNIX486V4
#define UNIX486V4       0
#endif
#ifndef NCR486
#define NCR486          0
#endif
#ifndef ICL3000
#define ICL3000         0
#endif
#ifndef MOTOROLAV4
#define MOTOROLAV4      0
#endif
#ifndef TI1500
#define TI1500          0
#endif
#ifndef PS2AIX
#define PS2AIX          0
#endif
#ifndef SYS25ICL
#define SYS25ICL        0
#endif
#ifndef ICL6000
#define ICL6000         0
#endif
#ifndef ICLDRS
#define ICLDRS          0
#endif
#ifndef MOTOROLA
#define MOTOROLA        0
#endif
#ifndef IBMRIOS
#define IBMRIOS         0
#endif
#ifndef NW386
#define NW386           0       /* Netware 386 machine */
#endif
#ifndef IBMWIN
#define IBMWIN          0       /* MS WINDOWS 3 machine */
#endif
#ifndef W95
#define W95             0
#endif
#ifndef SGI
#define SGI             0
#endif
#ifndef CT386
#define CT386           0
#endif
#ifndef AS400
#define AS400           0
#endif
#ifndef WINNT_INTEL
#define WINNT_INTEL     0       /* Windows NT on Intel */
#endif
#ifndef OS2_INTEL
#define OS2_INTEL       0       /* IBM OS/2 on Intel */
#endif
#ifndef SUN45
#define SUN45           0
#endif
#ifndef WINNT_ALPHA
#define WINNT_ALPHA	0	/* Windows NT on Alpha */
#endif
#ifndef ALPHAOSF
#define ALPHAOSF	0	/* Digital UNIX (formerly OSF/1) on Alpha */
#endif
#ifndef LINUX
#define LINUX           0
#endif
#ifndef LINUXX86
#define LINUXX86        0
#endif

#undef _UNALIGNED_
#define _UNALIGNED_

#endif /* PSCMACH_H */
