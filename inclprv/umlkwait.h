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

#ifndef UMLKWAIT_H
#define UMLKWAIT_H

/*  Function prototypes for the subsystem.
 *
 *  NOTE:  external callers should call lkBusy, NOT umLockWait.
 *         umLockWait only contains the 4GL/interactive lock wait
 *         routine.
 */
int lkBusy FARGS((
	int   msgnum,	   /* identifies which message:
			         Record in use, Database in use, etc        */
	TEXT *pfilename,   /* substitution parameter in the lock message    */
	TEXT *puser,       /* substitution parameter in the lock message    */
	TEXT *ptty,        /* substitution parameter in the lock message    */
	int (*pcallback)(),/* callback routine into the dbmanager code      */
	LONG  timedelay,   /* desired callback frequency in milliseconds    */
	TEXT *pdbinfo      /* pointer to private dbmgr info		    */
	));
int
lkWaitOrContinue FARGS(
                 (int msgnum,         /* msg number for lock message to user */
                  TEXT *pfilename,    /* table name */
                  TEXT *puser,        /* lock holder's name */
                  TEXT *ptty,         /* lock holder's tty name */
                  int (*pLkCheckFunc)(), /* func to call to check lock state */
                  LONG timedelay,     /* time between checks */
                  TEXT *pdbinfo,      /* data to pass to check function */
                  LONG timeOut,       /* number of seconds before timeout */
                  int (*pUmCheckFunc)()/* User notification function      */
                  ));
                  
int umLockWait FARGS((
	int   msgnum,	   /* identifies which message:
			         Record in use, Database in use, etc        */
	TEXT *pfilename,   /* substitution parameter in the lock message    */
	TEXT *puser,       /* substitution parameter in the lock message    */
	TEXT *ptty,        /* substitution parameter in the lock message    */
	int (*pcallback)(),/* callback routine into the dbmanager code      */
	LONG  timedelay,   /* desired callback frequency in milliseconds    */
	TEXT *pdbinfo      /* pointer to private dbmgr info		    */
	));


#if 0
int
callback(
	TEXT  *pdbinfo,	      /* same pointer the dbmgr passed to wiLockWait */
	TEXT **ppuser,	      /* return non-NULL if lockholder name change   */
	TEXT **pptty,	      /* return non-NULL if lockholder tty change    */
	int    mode);	      /* LW_DELAY_RETURN or LW_IMMEDIATE_RETURN      */
#endif
	 


/* msgnum parameter to umLockWait() */
#define LW_RECLK_MSG   1  /* use message ioMSG026:
			    <filename> in use by <user> on <tty> ...        */
#define LW_SCHLK_MSG   2  /* use message ioMSG036:
			    Dictionary being changed by <user> on <tty> ... */
#define LW_DBLK_MSG    3  /* use message ioMSG020:
			    Database in use by <user> on <tty> ...          */

/* mode parameter to callback routine */
#define LW_IMMEDIATE_RETURN 1 /* check if granted, then return */
#define LW_DELAY_RETURN     2 /* wait till timer interrupt, then return */
#define LW_WAIT_RETURN      3 /* wait till granted or cancelled */

/* return values */
#define LW_CONTINUE_WAIT 1 /*the lock is still unavailable		      */
#define LW_END_WAIT      2 /*the lock is now available			      */
#define LW_STOP_WAIT     3 /*user press CTRL-C or callback retnd LW_STOP_WAIT */
#define LW_CANCEL_WAIT   4 /*user press STOP button in alert box	      */
#define LW_NOREPLY       5 /*timeout, database responding slowly	      */
#define LW_DISCON        6 /*database was disconnected			      */

#endif /* #ifndef UMLKWAIT_H */
