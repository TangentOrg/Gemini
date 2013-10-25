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

#ifndef DRPUB_H
#define DRPUB_H

/*******************************************************************/
/* drpub.h - Public function prototypes for DR utility subsystem. */
/******************************************************************/
#include "drsig.h"  /* for sigHandler_t */

struct globcntxt;

/*************************************************************/
/* Public Function Prototypes for drrfutl.c                  */
/*************************************************************/
/* main */

/*************************************************************/
/* Public Function Prototypes for drscdbg.c                  */
/*************************************************************/
/* main */

/*************************************************************/
/* Public Function Prototypes for drshut.c                   */
/*************************************************************/
DSMVOID	drmain_init	(DSMVOID);
int     drmain_doit	(int argc, char *argv[]);

/*************************************************************/
/* Public Function Prototypes for drsig.c                    */
/*************************************************************/
#if OPSYS != BTOS	/* BTOS version is in the file drbtsig.c */
 
#if OPSYS==UNIX || OPSYS==VMS || OPSYS==MPEIX

DSMVOID	drHdlTerm	(DSMVOID);
DSMVOID	drExitOnTerm	(DSMVOID);
DSMVOID	drsigs		(DSMVOID);
DSMVOID	drsigh		(int sigNum);
DSMVOID	drSigDelayFork	(DSMVOID);
DSMVOID	drSigAllowFork	(DSMVOID);
int	drSigIgnInt	(DSMVOID);
DSMVOID	drSigSetInt	(DSMVOID);
DSMVOID	drSigDumInt	(DSMVOID);
DSMVOID	drSigRelay	(int sigNum, sigHandler_t aHandler);
DSMVOID	drSigResAlm	(DSMVOID);
DSMVOID    drSigReset      (DSMVOID);
#endif /* UNIX, VMS, MPEIX */


#if OPSYS==MS_DOS && WINSYS!=WIN3 && !NW386

DSMVOID	drsigs		(DSMVOID);
DSMVOID	drsigh		(DSMVOID);
DSMVOID	drsigclr	(DSMVOID);

#endif /* OPSYS==MS_DOS && WINSYS!=WIN3 && !NW386 */

#if OPSYS==MS_DOS && WINSYS==WIN3

DSMVOID	drsigs		(DSMVOID);
DSMVOID	drsigh		(DSMVOID);
DSMVOID	drsigclr	(DSMVOID);

#endif /* WIN 3 */

#if OPSYS==OS2

DSMVOID	drsigs		(DSMVOID);

#if 0
void	APIENTRY drsigh	(USHORT arg, USHORT code);
void	os2errexit	(USHORT err, char *msg);
DSMVOID	drsigs		(DSMVOID);
DSMVOID	APIENTRY drsigh	(USHORT sigarg, USHORT sigcode);
DSMVOID	os2errexit	(USHORT err, char *msg);
#endif /* OS2 */

#endif /* OS2 */

#if NW386

DSMVOID	nlmsigh		(DSMVOID);
DSMVOID	drsigs		(DSMVOID);

#endif /* NW386 */

#if OPSYS==WIN32API

DSMVOID	drsigh		(int sig);
DSMVOID	drsigs		(void);
DSMVOID    drSigReset      (void);
DSMVOID	drnsigs		(void);
DSMVOID	drSigHandler	(LPVOID inDbctl);

#endif /* WIN32API */

#if OPSYS == UNIX

DSMVOID	drSigMessageOn	(DSMVOID);

#endif /* OPSYS == UNIX */

#endif /* OPSYS != BTOS */

int	drSigLoad	(DSMVOID (*pfumHandleSignal)(DSMVOID));

int	init_sysglb2	(struct globcntxt **ppgc, GBOOL do_alloc_cntxt,
			 GBOOL do_alloc_sysglobs); 


#endif  /* #ifndef DRPUB_H */
