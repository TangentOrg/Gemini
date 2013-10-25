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

#ifndef UTCPUB_H
#define UTCPUB_H

/* utchrpub.h */

TEXT DLLEXPORT *chrcop (
			TEXT   *t,
                        TEXT    chr,
                        int     n);

int DLLEXPORT dblank (TEXT *t);

int DLLEXPORT dlblank (TEXT    *t);

/* utcrc.c */

DLLEXPORT UCOUNT calc_crc (UCOUNT  crc,    /* seed crc from prior blocks */
                           TEXT    *pdata, /* ptr to block of data to be crc'd */
                           COUNT   len);    /* length of data block */

#define UTENDBUFLEN 17
DLLEXPORT TEXT * utend(TEXT *pswd,  /*password to be encoded*/
                       int  lenp, /*length of pswd (in case its not ascii)*/
		       TEXT *pendbuf); /*output buf, must be UTENDBUFLEN*/


/* utctlc.c */

DLLEXPORT DSMVOID utsctlc            (DSMVOID);
 
DLLEXPORT int utssoftctlc         (DSMVOID);
 
DLLEXPORT int utpctlc             (DSMVOID);
 
#define IOENDERR        1
#define IOSTOP          2
#define IOERROR         4
#define IOENDKEY        8
#define IOSOFTSTOP     16

#endif /* UTCPUB_H */
