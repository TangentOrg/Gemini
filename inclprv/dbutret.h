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
#ifndef DBUTRET_H
#define DBUTRET_H


/************************************************************* 
 * This header file contains all the necessary mode and flag
 * definitions used in support of the dbutAPI.
 *************************************************************/
/* STATUS CODE CONVENTION                                    */
/* The range -30001 to -32000 is reserved for DBUT error      */
/*  returns.                                                 */
/* Status codes break down as follows:                       */
/*    DBUT_S_STATUSNAME          -NNNNNN                      */
/*************************************************************/



#define DBUT_S_SUCCESS                   0L
#define DBUT_S_NAME_TOO_LONG             -30001L
#define DBUT_S_SEEK_TO_FAILED            -30002L
#define DBUT_S_SEEK_FROM_FAILED          -30003L
#define DBUT_S_GET_OFFSET_FAILED         -30004L
#define DBUT_S_READ_FROM_FAILED          -30005L
#define DBUT_S_WRITE_TO_FAILED           -30006L
#define DBUT_S_TIMER_MAX_REQUESTS        -30007L
#define DBUT_S_OPEN_TTY_FAILED           -30008L
#define DBUT_S_NAP_UNDEFINED             -30009L
#define DBUT_S_TOO_MANY_PENDING_TIMERS   -30010L
#define DBUT_S_INVALID_WAIT_TIME         -30011L
#define DBUT_S_OPEN_FAILED               -30012L
#define DBUT_S_MAX_FILE_EXCEEDED         -30013L    /* UNIX maxfile exceeded */
#define DBUT_S_DISK_FULL                 -30014L    /* Insufficient disk space */
#define DBUT_S_REG_KEYNOTFOUND           -30015L    /* \PSC base key not found in registry */
#define DBUT_S_REG_QUERYFAILED           -30016L    /* query for registry value of key failed */
#define DBUT_S_REG_INVALIDSIZE           -30017L    /* query for key failed, target to small */
#define DBUT_S_INVALID_FILENAME          -30018L    /* invalid characters in filename */


#endif /* DBUTRET_H */

