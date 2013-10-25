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

#ifndef STMCHK_H
#define STMCHK_H

/* values for flags */
#ifdef ST_PURIFY
#define ST_CHAIN_SIZE   ( 12 * 1024 )
#endif
#define MAXPOOLPTR      1024
#define ST_NUMPOOLS     1
#define ST_USEMESSAGE   2
#define ST_SHOWSUMMARY  4
#define ST_SHOWUSAGE    8
#define ST_INITPLIST   16   /* Init poolCounts */

#define ST_DEFAULTS (ST_USEMESSAGE | ST_SHOWSUMMARY)

typedef struct
{
  unsigned long memused;
  unsigned long memunused;
  int numABK; /* The number of ABK's in the storage pool */
} POOLCOUNTS;

typedef struct
{
	STPOOL   *pstpool;   /* a pointer to a storage pool */
#ifdef ST_PURIFY
   TEXT     **pchain;
   int      numAllocated;
#if NW386
   int      idNumber;
#endif /* NW386 */
#endif /* ST_PURIFY */
}POOLPTRS;

/* prototypes */
DSMVOID stGetPoolSize FARGS((STPOOL *, POOLCOUNTS *, char *, char *, int));

int  stMemCheckPools();
int  stMemNumPools();
int  stMemUseMessage();
LONG stMemUsed();
LONG stMemShow();
LONG stSeEnableOn();
LONG stSeEnableOff();

#endif  /* STMCHK_H */
