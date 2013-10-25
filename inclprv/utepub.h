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

#ifndef UTEPUB_H
#define UTEPUB_H

/* utenv.c functions */

DLLEXPORT int utgethostname(TEXT *pbuf,   /* ptr to buf for hostname */
                            int  len);    /* lenght of the buffer */


DLLEXPORT ULONG utgetpid(DSMVOID);
DLLEXPORT LONG   utgetenvwithsize (TEXT *envVariableName,
                                   TEXT *envVariableValue,
                                   LONG size);



DLLEXPORT TEXT *uttty(int fildes, TEXT *terminalDevice, int len, int *sysErrno);

DLLEXPORT TEXT *utuid(TEXT *userId, int len);

DLLEXPORT DSMVOID utsetuid(DSMVOID);

DLLEXPORT int  utCheckTerm(DSMVOID);

#if OPSYS == WIN32API
DLLEXPORT DWORD utGetStdHandleType(int i);
DLLEXPORT int   utDetermineVersion();
#endif /* OPSYS == WIN32API */

#endif /* UTEPUB_H */
