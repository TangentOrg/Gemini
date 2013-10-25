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

#ifndef PSCNAP_H
#define PSCNAP_H 1

/* Definitions for mt napping.

   NAP_TYPE	Chooses one of poll (), select (), napms (), or OS dependent
		call for subsecond naps. See mt.c for use. Run test programs
		to verify choice actually works.

   NAP_INIT	Default initial nap time. In milliseconds.
		Should be set to the minimum nap time possible.
		Most systems will round the value up to the appropriate time,
		so it is not too important to get it exactly right as long
		as you err on the LOW side.

   NAP_STEP	Default number of spinlock iterations between time increases.
		5 is probably OK.

   NAP_INCR	Default amount to increase nap time between steps. In
		milliseconds.
		Should be set to the minimum possible nap time.
		Run test programs to determin this value.
		10, 20, and 50 milliseconds are typical values found. See
		/usr/include/sys/param.h on Unix systems for value of HZ
		to get clock ticks per second.

   NAP_MAX	Default upper bound on nap time increases. In milliseconds.
		100 milliseconds should be fine for all.
*/

/* different ways we can nap for less than 1 second */

#define NAP_NONE	0 /* CANT NAP AT ALL !!!!! */
#define NAP_OS		1 /* nap using appropriate os call */
#define NAP_POLL	2 /* nap using poll () */
#define NAP_SELECT	3 /* nap using select () */
#define NAP_NAPMS	4 /* nap using napms () BEWARE: see mt.c */

/* see mt.c for how these are used */

#define NAP_INIT  10	/* milliseconds to sleep between lockaux tstset's */
#define NAP_INCR  10	/* milliseconds to increase sleep time */

#define NAP_STEP  5	/* sleeps between time increases */
#define NAP_MAX	  5000	/* max sleep time */

/* choose one nap type - run test programs to verify */
#if OS2_INTEL || OPSYS==WIN32API
#define NAP_TYPE NAP_OS

#else
#if ARIXS90 | ARIX825 | TI1500 | MOTOROLA | SUN | SUN4 | SEQUENTPTX | SCO \
	    | DG5225 | IBMRIOS | NCR486 | MOTOROLAV4 | UNIX486V4 | ALPHAOSF \
            | HPUX | LINUX
#define NAP_TYPE NAP_POLL
#else

#if PYRAMID | SEQUENT | ROADRUNNER | SIEMENS | HP825 | xIBMRIOS | OPSYS==MPEIX \
            | SUN45 | ICL6000
#define NAP_TYPE NAP_SELECT
#else

#if SGI | DIAB
#define NAP_TYPE NAP_NAPMS /* only use if nothing else works */
#else

#if PS2AIX | RT
#define NAP_TYPE NAP_NONE /* reverify these machines when new os available */

#else /* defaults to none if not listed above - has no way at all to nap */
#define NAP_TYPE NAP_NONE

#endif
#endif
#endif
#endif
#endif

#endif /* PSCNAP_H */
