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

#ifndef DSTD_H
#define DSTD_H 1

#include "pscmach.h"
#include "pscos.h"
#include "pscchip.h"
#include "pscclang.h"
#include "pscdtype.h"
#include "pscnap.h"
#include "pscnet.h"
#include "pschack.h"
#include "pscret.h"
#include "psctrace.h"
#include "pscsys.h"
#define MAXKEYSZ 1000
#define KEYSIZE MAXKEYSZ
#define FULLKEYHDRSZ  6  /* 6 bytes are used in  memory key header
                              in cursors and other in memory structures
                              to store sizes as follows:
                           2 bytes - the  total size stored with sct
                           2 bytes - the  size of the key (ks) stored with sct
                           2 byte  - the  size of the info (is)stored with sct
                             The header of an entry in an index block is 
			 different than this one */

#define INVALID_KEY_VALUE -1 
#define PRO_REENTRANT 1 

#if NUXI==1
#define COUNT_TO_STORE(STORE,VALUE)  *((UCOUNT*) (STORE))= (UCOUNT) (VALUE)
#define STORE_TO_COUNT(STORE)  (*((COUNT *) (STORE)))
#define LONG_TO_STORE(STORE,VALUE)  *((LONG *) (STORE)) = (LONG)(VALUE)
#define STORE_TO_LONG(STORE)     (*((LONG *) (STORE)))
#define LONG64_TO_STORE(STORE,VALUE) *((LONG64 *) (STORE)) = (LONG64)(VALUE)
#define STORE_TO_LONG64(STORE)    (*((LONG64 *) (STORE)))

#else
#define COUNT_TO_STORE(STORE,VALUE) { UCOUNT def_temp= (UCOUNT) (VALUE) ;\
                              *((TEXT*) (STORE))=  (TEXT)(def_temp); \
                              *((TEXT*) (STORE+1))=(TEXT)((def_temp >> 8)); }

#define STORE_TO_COUNT(STORE)    (COUNT) (((COUNT) ((TEXT) (STORE)[0])) +\
                                 ((COUNT) ((COUNT) (STORE)[1]) << 8))

#define LONG_TO_STORE(STORE,VALUE)  { *(STORE)=(char) ((VALUE));\
                                  *((STORE)+1)=(char) (((VALUE) >> 8));\
                                  *((STORE)+2)=(char) (((VALUE) >> 16));\
                                  *((STORE)+3)=(char) (((VALUE) >> 24)); }
#define STORE_TO_LONG(STORE)    (LONG) (((LONG) ((TEXT) (STORE)[0])) +\
                                (((LONG) ((TEXT) (STORE)[1]) << 8)) +\
                                (((LONG) ((TEXT) (STORE)[2]) << 16)) +\
                                (((LONG) ((TEXT) (STORE)[3]) << 24)))

#if 0 /* Use big endian on linux for testing the implementation on linux */
#define LONG_TO_STORE(STORE,VALUE)  { *((STORE)+3)=(char) ((VALUE));\
                                  *((STORE)+2)=(char) (((VALUE) >> 8));\
                                  *((STORE)+1)=(char) (((VALUE) >> 16));\
                                  *((STORE)+0)=(char) (((VALUE) >> 24)); }
#define STORE_TO_LONG(STORE)    (LONG) (((LONG) ((TEXT) (STORE)[3])) +\
                                (((LONG) ((TEXT) (STORE)[2]) << 8)) +\
                                (((LONG) ((TEXT) (STORE)[1]) << 16)) +\
                                (((LONG) ((TEXT) (STORE)[0]) << 24)))
#endif

#define LONG64_TO_STORE(STORE,VALUE) { ULONG xtemp1 = (ULONG)(VALUE); \
                                       ULONG xtemp2 = (ULONG)((VALUE) >> 32); \
                                       LONG_TO_STORE((STORE),xtemp1); \
                                       LONG_TO_STORE((STORE+4),xtemp2); \
                                      }
#define STORE_TO_LONG64(STORE) ((ULONG64)(((ULONG) ((TEXT) (STORE)[0])) +\
                                    (((ULONG) ((TEXT) (STORE)[1])) << 8) +\
                                    (((ULONG) ((TEXT) (STORE)[2])) << 16) +\
                                    (((ULONG) ((TEXT) (STORE)[3])) << 24)) +\
                        (((ULONG64) (((ULONG) ((TEXT) (STORE)[4])) +\
                                   (((ULONG) ((TEXT) (STORE)[5])) << 8) +\
                                   (((ULONG) ((TEXT) (STORE)[6])) << 16) +\
                                 (((ULONG) ((TEXT) (STORE)[7])) << 24))) <<\
                                    32))
#endif

#ifdef DBKEY64
#define DBKEY_TO_STORE(STORE,VALUE) LONG64_TO_STORE(STORE,VALUE)
#define STORE_TO_DBKEY(STORE) STORE_TO_LONG64(STORE)
#else
#define DBKEY_TO_STORE(STORE,VALUE) LONG_TO_STORE(STORE,VALUE)
#define STORE_TO_DBKEY(STORE) STORE_TO_LONG(STORE)
#endif
#define VARIABLE_DBKEY 1

#endif /* DSTD_H */
