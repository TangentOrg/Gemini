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

#ifndef DBDEFS_H
#define DBDEFS_H

#define RLCTL 		struct rlctl
#define SEMDSC 		struct semdsc
#define STPOOL		struct stpool
#define DITEM 		struct ditem
#define SHMDSC 		struct shmdsc
#define SRVCTL 		struct srvctl
#define USRCTL 		struct usrctl
#define MSTRBLK 	struct mstrblk
#define AICTL 		struct aictl
#define LKCTL 		struct lkctl
#define TRANTAB 	struct trantab
#define CONDB 		struct condb
#define BKCTL 		struct bkctl





/*server types for servtype , 0 means non-shmem one/and only server*/
#define SPAWNSRV  1   /*serves clients,spawned by broklog proc, parm -m1*/
#define MANUALSRV 2   /*serves clients,spawned by broklog proc, parm -m2
				    or just default for non-shmem*/
#define BROKLOG   3   /*brokers logins, parm -m3 or just default shmem*/

#endif /* #ifndef DBDEFS_H */
