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

#ifndef LKSCHMA_H
#define LKSCHMA_H

/*********************************************************/
/**							**/
/**	Schema lock manager declares. lkschma is a	**/
/**	multi function routine which takes an int	**/
/**	control parameter and returns a LONG result,	**/
/**	whose definition depends upon the requested	**/
/**	function, as follows:				**/
/**							**/
/**	SLSHARE - requests schema share lock. Called	**/
/**		prior to compiling a program to ensure	**/
/**		schema consistency, and prior to	**/
/**		executing a program which references	**/
/**		any database files.			**/
/**							**/
/**		The returned value is CTRLC if the user	**/
/**		abandons an enqueueing wait, zero	**/
/**		if the lock is granted and no schema	**/
/**		updations have occured, else SLCHG to	**/
/**		indicate the schema has changed. SLCHG	**/
/**		will be returned to the first SLSHARE	**/
/**		call AFTER the program that updates	**/
/**		the schema exits for the user that	**/
/**		updated the schema, at the first	**/
/**		SLSHARE call for any other users.	**/
/**							**/
/**	SLRELS - release a share schema lock. Always	**/
/**		returns a value of zero.		**/
/**							**/
/**	SLEXCL - obtain schema exclusive lock. Returns	**/
/**		a zero if the lock was granted,		**/
/**		CTRLC if the user abandoned an		**/
/**		enqueueing wait.			**/
/**							**/
/**	SLRELX - release exclusive lock and indicate	**/
/**		to all users that the schema has been	**/
/**		updated. Always returns a value of	**/
/**		zero.					**/
/**							**/
/**	SLGUARSER - obtain schema guaranteed serializability lock. Returns a
  		zero if the lock was granted, CTRLC if the user abandoned an
		enqueueing wait. Only allowed when running in ansisql
		complience mode.

	SLRELGS - release guaranteed serializability lock. Always returns a
		value of zero. Only allowed when running in ansisql
		complience mode.
							**/
/*********************************************************/

/**	internal flags:				**/

#define SHARECTR	1
#define EXCLCTR		2
#define GUARSERCTR      4
#define CTRMASK		7
#define LOCKRQST	8
#define LOCKRELS	16

/**	possible argument values:		**/
#define SHARESUB 0
#define EXCLSUB  1
#define GUARSERSUB 2

#define SLSHARE		(LOCKRQST+SHARECTR)

#define SLRELS		(LOCKRELS+SHARECTR)

#define SLEXCL		(LOCKRQST+EXCLCTR)

#define SLRELX		(LOCKRELS+EXCLCTR)

#define SLGUARSER       (LOCKRQST+GUARSERCTR)

#define SLRELGS         (LOCKRELS+GUARSERCTR)

/**	returned value:		**/
#define SLCHG		32	/* indicates schema changed */
#define SLDBGONE	64	/* indicates schema changed */
#define SLEXPIRED      128      /* indicates schema lock expired */

#endif  /* #ifndef LKSCHMA_H */
