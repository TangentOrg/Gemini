
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

#ifndef DBCODE_H
#define DBCODE_H

/* database type number -- to be put into the icb by xxxget and xxxfnd  */
/* WARNING: If the database manager DOES support transaction backout,   */
/*	    the database type number must be <= 127.		        */
/*	    If the database manager DOES NOT support transaction backout*/
/*	    the database type number must be >= 128 and <= 255		*/

#define PROCODE		 1
#define LVCODE		 2
#define NCRORACLE	 3
#define RMSCODE		 4
#define WRKVARCODE	 5
#define SYBCODE		12
#define ORACODE		13
#define NONSTOPSQL	21
#define	RDBCODE		22
#define	CISAMCODE	23
#define	NETISAMCODE	24
#define	CTOSISAMCODE	25
#define AS4CODE		26
#define PROCODET        27
#define OACCESSCODE     28      /* ODI's ObjectStore Gateway,    l. lovas */
#define GENERICCODE     29      /* Generic Gateway Architecture, l. lovas */
#define ODBCCODE        30      /* ODBC Gateway                           */
#define MSSCODE         31      /* MSS Gateway                  d.moloney */

#define APPLSVCODE	35	/* for the application Server Broker */

#endif  /* #ifndef DBCODE_H */
