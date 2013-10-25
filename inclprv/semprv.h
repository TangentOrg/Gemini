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

#ifndef SEMPRV_H
#define SEMPRV_H

/*************************************************************/
/* Function prototypes and other defs for the sem subsystem  */
/*************************************************************/

struct dsmContext;

ULONG semGetSemidSize	(struct dsmContext *pcontext);

int semGetSet		(struct dsmContext *pcontext, TEXT *pname,
			 char idchar, int noentmsg); 
#if OPSYS==UNIX
int semClear		(struct dsmContext *pcontext, TEXT *pname,
			 int semid);
#else
int semClear		(struct dsmContext *pcontext, TEXT *pname,
			 char idchar);
#endif
DSMVOID semChgOwn		(struct dsmContext *pcontext, int semid,
			 int uid);

#if OPSYS==WIN32API
#ifndef WAIT_TIMEOUT 
#define WAIT_TIMEOUT ERROR_TIMEOUT
#endif

HANDLE osOpenSemaphore(char *semname);

int osCloseSemaphore(HANDLE hsem);

HANDLE osCreateSemaphore(struct dsmContext *pcontext, char *semname);

LONG osReleaseSemaphore(struct dsmContext *pcontext, HANDLE hSem);

PSECURITY_ATTRIBUTES SecurityAttributes();


#ifdef NTDEBUGON
#define NTPRINTF ntprintf
#else
#define NTPRINTF
#endif

#endif	/* WIN32API */

#endif /* SEMPRV_H */
