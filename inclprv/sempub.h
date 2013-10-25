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

#ifndef SEMPUB_H
#define SEMPUB_H

/******************************************************************************
 *
 * DESCRIPTION:
 *      The SEM subsystem provides an operating system independent system-wide
 *      locking (SWL) mechanism to the rest of dbmgr.  This mechanism allows 
 *      for multiple processes to synchronize access to shared resources like
 *      datastructures in shared memory.
 *
 *      This file contains the public interface to the SEM subsystem.  No
 *      code outside of SEM may use any other function or datastructure defined
 *      in any other SEM source file.  The only exception to this is the LAT
 *      subsystem, which uses a few low-level SEM functions to implement
 *      latches on some systems.  The SEM public function are implemented in
 *      semadm.c.
 *
 *      Someday we should change the name of this subsystem to something else
 *      (SystemWideLock, SWL?).  When this happens the LAT subsystem can be
 *      pulled in and hidden as well.
 *      
 *
 * PUBLIC FUNCTIONS:
 *
 *  semCreate   - Create all semaphores for a database.  Called during 
 *                database open (by the first user to touch the database on a
 *                system). After the database is open, other users can
 *                "connect" to the semaphores.  This call also locks the login
 *                lock to prevent other users from logging while the database
 *                is being initialized.
 *  semCreate2  - Create semaphores, part 2.  On some operating systems, some
 *                initialization must occur after shared-memory and the usrctl
 *                structures are set up.  This function performs the secondary
 *                initialization.
 *  semDelete   - Remove all semaphores for a database.  Called during
 *                database close.
 *  semConnect  - Connect to the semaphores, previously created for the
 *                database.  Called by users connecting to the database after
 *                it is already open.
 *  semConnect2 - Connect to the semaphores, part 2.  On some operating
 *                systems, some initialization must occur after shared-memory
 *                and the usrctl structures are set up.  This function performs
 *                the secondary initialization.
 *  semRdyLog   - Called to mark that a database is ready for login.  This
 *                function is called during database open (by the first user to
 *                touch the database on a system) to indicate that all critical
 *                initialization is complete and that other users may connect
 *                to the database.
 *  semLockLog  - Lock the login lock, preventing other users from logging in
 *                or out while a new user is added or deleted from the shared
 *                memory usrctl array.
 *  semUnlkLog  - Unlock the login lock, allowing other users to log in or out.
 *
 *  semUsrGet   - make a private function, called by semConnect2.
 *  semUsrCr    - make a private function, called by semCreate2.
 *  semRelease  - does nothing.... to be deleted.
 *
 * To Do:
 *  - remove semRelease.  It does nothing on all operating systems.
 *  - On UNIX, semCreate has 2 parameters but just 1 on all other
 *    platforms.  All semCreate implementations should use the
 *    same number of parameters so callers don't need to be platform
 *    specific.
 *  - Should I change the name of semGetID to SemConnect?
 *  - Remove the #define's for semaphore constants from this file and put them
 *    in PVT.
 *  - SEM_UNDO is defined in sempvt.h.  This is a UNIX constant that is being
 *    redefined to the same value.... this should be removed.
 *  - SEMDSC datastructure needs to be moved to sempvt.h.  However there is 
 *    several places outside of SEM where it's fields are used; these places
 *    must be fixed first.
 *  - move more stuff to sempvt.h (ie semmgr.c functions & constants)
 *****************************************************************************/

#if OPSYS==UNIX
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#endif

#ifndef SHMPUB_H
#include "shmpub.h"
#endif

/* External Function Protypes for semadm.c */

struct dsmContext;
struct segment;

#if OPSYS==UNIX
int semCreate(struct dsmContext *pcontext, bkIPCData_t *pbkIPCData);
#else
int semCreate(struct dsmContext *pcontext);
#endif
int semRdyLog	(struct dsmContext *pcontext);
int semGetId	(struct dsmContext *pcontext);
DSMVOID semDelete	(struct dsmContext *pcontext);
DSMVOID semRelease	(struct dsmContext *pcontext);
int semUnlkLog	(struct dsmContext *pcontext);
int semLockLog	(struct dsmContext *pcontext);
int semUsrGet	(struct dsmContext *pcontext);
int semUsrCr	(struct dsmContext *pcontext);
#if OPSYS==UNIX
DSMVOID semCleanup(struct dsmContext *pcontext, struct segment  *pseg);
#endif

/* External Function Prototypes for semmgr.c */
/* BUM Work Around */
#define semSet utsemset
#define semAdd utsemadd
#define semValue utsemval
#define semCrSet utsemcr
#define semtst uttstsem
/* END BUM Work Around */

/* parms to semAdd */
#define SEM_RETRY   1	/* retry the wait if interrupted by EINTR */
#define SEM_NORETRY 0
#define SEM_NOUNDO 0

#if OPSYS==WIN32API
int	semAdd		(struct dsmContext *pcontext, ULONG semid,
			 HANDLE semnum, int value,
			 int undo, int plsretry); 
int	semSet		(struct dsmContext *pcontext, ULONG semid,
			 HANDLE semnum, int value); 
#else
int	semAdd		(struct dsmContext *pcontext, int semid,	
			 int semnum, int value,
			 int undo, int plsretry); 
int	semSet		(struct dsmContext *pcontext, int semid,
			 int semnum, int value); 
#endif

int	semValue	(struct dsmContext *pcontext, int semid,
			 int semnum);	
int	semCrSet	(struct dsmContext *pcontext, TEXT *pname,
			 char idchar, int *pnumsems, int minsems); 
int	semtst		(struct dsmContext *pcontext, int semid,
			 int semnum, int value);

ULONG	getSemidSize	(struct dsmContext *pcontext);


/***********************************************************************/
/* Public Structures and Defines                                       */
/***********************************************************************/

/*
 * a semaphore control structure used by semsys
 *
 * This is actually a private datastructure whose fields should not be
 * touched by any code outside SEM.
 * 
 * CLEANUP:  Code outside of SEM should be cleaned up so that it doesn't
 * any of these datastructure fields.  Once this is done, this struct can
 * be move to sempvt.h
 */
#define SEMDSC struct semdsc 

SEMDSC {
    TEXT  *pname;       /* The name used to create the semaphores */
    int   semid;        /* The identifier used to reference them  */
    int   numsems;      /* The number of semaphores in the set	  */
       };

typedef struct dbSemid
{
        LONG    size;
        LONG    id;
} semid_t;


#if NOTSTSET

#define LOGSEM  0 /* offset of service semaphore in the array of sems */
#define RDYSEM  1 /* offset of ready semaphore in the array of sems */
#define AUXSEM  2 /* offset of auxiliary semaphore in the array of sems */
#define SPINSEM 3 /* offset of spinfast semaphore */
#define DBSEM   4 /* offset of dbfast semaphore */
#define MTSEM   5 /* offset of mtfast semaphore */
#define DEBUGSEM 6 /* offset of debugfast semaphore */

#define FRSTUSRSEM 7 

#else 

#define LOGSEM  0 /* offset of service semaphore in the array of sems */
#define RDYSEM  1 /* offset of ready semaphore in the array of sems */
#define AUXSEM  2 /* offset of auxiliary semaphore in the array of sems */

#define FRSTUSRSEM 3 /*offset of 1st user's semaphore in the array of sems*/
#endif /* NOTSTSET */

#endif /* SEMPUB_H */
