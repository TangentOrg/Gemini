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

#ifndef DRSIG_H
#define DRSIG_H

/**************************************************************/
/** drsig.h -- signal numbers used by server/child mechanism **/
/**************************************************************/

typedef void (*sigret_t)();

#ifndef SIGTRAP
#include <signal.h>
#endif

#define SVSIGEND        SIGIOT /* This should be removed. ghb */
#define SVSIGLGN        SIGSYS /* This should be removed. ghb */

#if OPSYS==WIN32API 
#define SVSIGC          SIGBREAK
#elif LINUX
#define SVSIGC          SIGUSR2
#else
#define SVSIGC          SIGEMT
#endif  /* WIN32API */


#ifdef GLOBSIG

GLOBAL int unixesc = 0; /* nonzero if we have a subprocess running,
			   and not limited to Unix(tm) */
GLOBAL GBOOL exit_sig=0; /* if 1, a signal(HUP, TERM,..) to exit was received */

#else  /* NOT GLOBSIG */

IMPORT int unixesc;
IMPORT GBOOL exit_sig; /* if 1, a signal(HUP, TERM,..) to exit was received */

#endif /* GLOBSIG */

/* signal handler function pointers */
typedef DSMVOID (*sigHandler_t)();

/* signal handler return value */
#define sigHandler_f DSMVOID

#endif  /* #ifndef DRSIG_H */
