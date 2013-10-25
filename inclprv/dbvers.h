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

#ifndef DBVERS_H
#define DBVERS_H

/*****************************************************************/
/**								**/
/**	dbvers.h - this file describes the mb_dbvers field	**/
/**	from the master block (first block of every     	**/
/**	database and file extent).				**/
/**								**/
/**	This field is used to ensure that a given       	**/
/**	database is compatible with the version of binaries	**/
/**	attempting to access the database.			**/
/**								**/
/**	Before changing this field, refer to the following	**/
/**	modules:						**/
/**								**/
/**	gemini/db/bkopen.c              			**/

/**	The versions currently in use and any downward		**/
/**	compatabilities are as follows:				**/
/**								**/
/**								**/
/**	Minor Versions                  			**/
/**	--------------------------------------			**/
/**								**/
/*****************************************************************/

     /* DBVERSN    01      widening RM dir entries to LONG 04/02/01 */
     /* DBVERSN    02      variable length dbkeys 04/20/01 */
     /* DBVERSN    03      added Index Anchor Block 04/20/01 */
     /* DBVERSN  3004      changed version to match MySQL release 4/24/01 */
     /* DBVERSN  3005      Added fields to block header   4/25/01         */
     /* DBVERSN  3006      Changes to master block        5/03/01 */
     /* DBVERSN  3007      64 bit rowid                  05/07/01 */
     /* DBVERSN  3008      Added table no. to onject index 5/07/01 */
     /* DBVERSN  3009      Changed number of records per block  5/10/01 */
     /* DBVERSN  3010      Removed tinyblob-in-row optimization 5/11/01 */
     /* DBVERSN  3011      64 bit rowid for blobs               5/20/01 */
     /* DBVERSN  3012      Fixed index stats store to use index only 5/23/01*/
#define DBVERSN  3013   /* Aligned on disk structures 5/30/01 */
#define OKDBVERSN(x) (x  == DBVERSN)

#endif  /* #ifndef DBVERS_H */
