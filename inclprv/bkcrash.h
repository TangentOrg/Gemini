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

#ifndef BKCRASH_H
#define BKCRASH_H


#define BKCRASHNUMSIZE 1024     /* length of string representing the */
                                /* crash number, perhaps overly generous */

struct dsmContext;

typedef struct crashtest 
{ 
	unsigned	cseed;      /* random number gen seed */ 
	int		ntries;     /* numtries - counter */
	float	        probab;     /* probability */
	int		crash;      /* crash < 0 => crash at transaction end
				       crash = 0 => don't deliberately crash
				       crash > 0 => crash on write
				    */
        int             backout;    /* 0 = no backout / 1 = backout */
} crashtest_t;

#define SEEDDFLT   123456789
#define PROBDFLT   10	
#define BK_CRASHMAGIC 1379111315       /* magic number to enable crash code */


/* function prototypes */
DSMVOID bkCrashTry(struct dsmContext *pcontext, TEXT *where); 
DSMVOID bkCrashTm(struct dsmContext *pcontext);
DSMVOID bkBackoutExit(struct dsmContext *pcontext, TEXT *where, int clientpid);



#endif /* ifndef BKCRASH_H  */
