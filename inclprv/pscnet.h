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

#ifndef PSCNET_H
#define PSCNET_H 1

/*NETWORKS -- can be turned on by machine.h or by -Dxxxx on compiler*/
/*the defaults for each opsys are turned back on below under opsys choices*/

#ifndef MCPSOCKETS
#define MCPSOCKETS	0
#endif
#ifndef DOSSOCKETS
#define DOSSOCKETS	0
#endif
#ifndef SPXSESS
#define SPXSESS		0
#endif
#ifndef MSGQUEUES
#define MSGQUEUES	0
#endif
#ifndef NETBSESS
#define NETBSESS	0
#endif
#ifndef PIPES
#define PIPES		0
#endif
#ifndef BSDSOCKETS
#define BSDSOCKETS	0
#endif
#ifndef DECNET
#define DECNET          0
#endif
#ifndef UCXSOCKETS
#define UCXSOCKETS	0
#endif
#ifndef BSDDOS
#define BSDDOS          0
#endif
#ifndef OPENET
#define OPENET          0
#endif
#ifndef DOSFTP
#define DOSFTP          0
#endif
#ifndef TLI
#define TLI		0
#endif
#ifndef TLI54
#define TLI54		0
#endif
#ifndef TLI_TCP
#define TLI_TCP         0
#endif

#ifndef TLI_SPX
#define TLI_SPX     0
#endif
#ifndef BSD3COM
#define BSD3COM         0
#endif
#ifndef DOSWIN
#define DOSWIN          0
#endif
#ifndef BSDHPWIN3
#define BSDHPWIN3       0
#endif
#ifndef BSDWINWIN3
#define BSDWINWIN3      0
#endif
#ifndef NOVXLN
#define NOVXLN         0
#endif
#ifndef BSDLWPWIN3
#define BSDLWPWIN3      0
#endif
#ifndef BSDWINSOCK
#define BSDWINSOCK      0
#endif
#ifndef TCPDOS
#define TCPDOS          0
#endif

#if UNIXSYS5 | ICONPORT

#if MAC_AUX | DGMV | ( CT & CTMIGHTY ) | DIAB | TOWER700 | ATT386 \
	| BULLQ700 | PS2AIX | ATT3B2600 | MOTOROLA | BULLDPX2 | SIEMENS \
	| ARIX825 | ARIXS90 | ICLDRS | IBMRIOS | CT386 | SGI | SCO | DG5225 \
	| SEQUENTPTX | NCR486 | MOTOROLAV4 | UNIX486V4 | SUN45 | ICL6000 \
	| LINUX 
#undef BSDSOCKETS
#define BSDSOCKETS 1
#else /* !MAC_AUX... */
#undef MSGQUEUES
#define MSGQUEUES 1
#endif /* MAC_AUX etc */

#else

#if UNIXBSD2
#undef BSDSOCKETS
#define BSDSOCKETS 1          /* shld be 1 for new versn */
#endif /* UNIXBSD2 */

#endif /* UNIXSYS5 or ICONPORT */

#if DDE | ICL6000 | NCR486 | TOWER700 | TOWER650 | ARIXS90 | SCO | DG88K \
        | ATT386 | ATT3B2600 | ICL3000 | SEQUENTPTX | SUN4 | SUN | SUNOS4 \
	| NW386 | DG5225 | MOTOROLAV4 | SUN45
#undef  TLI
#define TLI     1
#undef  TLI_TCP 
#define TLI_TCP 1
#endif

#if NW386
#undef  TLI_SPX
#define TLI_SPX 1
#endif

#if OPSYS == MS_DOS && WINSYS == WIN3
/* #undef  TLI */
/* #define TLI     1 */
#undef  TLI_SPX
#define TLI_SPX 1
#endif

#if NCR486 | ICL6000 | ICL3000 | MOTOROLAV4
#undef  TLI54
#define TLI54    1
#endif

#if TOWER650 | TOWER800 | BULLXPS | XENIXAT | NOTSCO386
#undef MCPSOCKETS
#define MCPSOCKETS 1
#endif

#if MCPSOCKETS
#undef MSGQUEUES
#define MSGQUEUES 0
#endif

#if OPSYS==MPEIX
#undef BSDSOCKETS
#define BSDSOCKETS 1
#endif  /* MPEIX */

#if CT386
#undef MSGQUEUES
#undef MCPSOCKETS
#undef TLI
#define TLI
#endif	/* CT386 */

#if BSDDOS || DOSFTP || BSD3COM || DOSWIN || BSDHPWIN3 || BSDWINWIN3 || NOVXLN || BSDLWPWIN3 || BSDWINSOCK || TCPDOS
#undef NETBSESS
#define NETBSESS 0
#undef BSDSOCKETS
#define BSDSOCKETS 1
#endif

/* named pipes */

#if OPSYS==UNIX
#undef PIPES
#define PIPES 1
#endif

#if OPSYS==__WIN32API           /* temporarily turnoff for V9 Build */
#undef BSDSOCKETS
#define BSDSOCKETS 1
#endif

/*
  HASNODELAY	TRUE for setsockopt TCP_NODELAY to correct silly window problem
                most BSD WILL have, most SysV WILL NOT have
*/
#if ARIXS90 | TOWER700 | IBM | IBMWIN | ATT3B2600 | SIEMENS | DIAB \
    | NW386 | OPSYS==MPEIX | OS2_INTEL
#define HASNODELAY	0
#else
#define HASNODELAY	1
#endif

/* Some machine have different default devices for the tli network
   device.  Most machine will use the defualt /dev/tcp, however others
   need to have a different one.  Affected source files are: nss.c
   nsstli.c ncstli.c
*/

#if TLI
#if ALT386 | SPRO386 | TI1500 | SCO
#define TLIDEV "/dev/inet/tcp"
#else
#define TLIDEV "/dev/tcp"
#endif
#endif

/* These are the prototypes for all of Networking code */
#ifdef NET_PROTO
#include "netproto.h"
#endif

#endif /* PSCNET_H */
