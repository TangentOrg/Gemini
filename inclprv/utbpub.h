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

#ifndef UTBPUB_H
#define UTBPUB_H

/* utbuff.c */

int DLLEXPORT bufcmp (
        TEXT *t,
        TEXT *s,
        int   n);

DSMVOID DLLEXPORT bufcopl (
	TEXT	*t,
	TEXT	*s,
	LONG	 n);

DSMVOID DLLEXPORT utblkcp (
             LONG	*t,
	     LONG	*s,
	     int	 n);

DSMVOID DLLEXPORT bufmov (
            TEXT	*ps,	/* start of buffer */
	    TEXT	*pe,	/* 1 past end of buffer */
	    int	    amt);	/* shift amount */

                
#endif /* UTBPUB_H */
