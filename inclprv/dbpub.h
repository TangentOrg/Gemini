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

#ifndef DBPUB_H
#define DBPUB_H

/**********************************************************/
/* dbpub.h - Public function prototypes for DB subsystem. */
/**********************************************************/

/*****************************************************************************/
/* dbmgr.h - misc return codes, parm and function prototypes for db routines */
/*****************************************************************************/

/* WARNING -- When making up new error codes, check for conflicts in */
/*  fdmgr.h  dstd.h					     */
/*********************************************************************/

#include "dsmpub.h"
#include "shmpub.h"
#include "svcspub.h"

struct cursid;
struct dbcontext;
struct ditem;
struct fldtbl;
struct icb;
struct rnfind;
struct rnfndctl;
struct usrctl;
struct omDescriptor;


/* The following are definitions for mode value of dbExit() */
/* NOTE: Currently, these values match the values in drexit.h */
#define DB_DUMP_EXIT  2
#define DB_NICE_EXIT  8

/* The following are definitions for exitCode value of dbExit() */
/* NOTE: Currently, these values match the values in drexit.h */
#define DB_EXGOOD    0  /* EXGOOD    - exit with 0 return code             */
#define DB_EXBAD     2  /* EXBAD     - general purpose bad exit code       */
#define DB_EXNOSERV  8  /* EXNOSERV  - attempt to start client w/no server */
#define DB_EXSNGLUSE 14 /* EXSNGLUSE - db is already in use by a single user */
#define DB_EXSRVUSE  16 /* EXSRVUSE  - db is already in use by a server    */
#define DB_EXLIMBO   20 /* EXLIMBO   - Databases has limbo transactions    */


#define FNDOK		0     /* ixfnd: found a match			*/
#define FNDAMBIG	-1213 /* ixfnd: find found more than 1 match	*/
#define FNDFAIL		-1214 /* ixfnd: not found			*/
#define ENDLOOP		-1215 /* ret code from dbnxt at end of loop	*/

#define AHEADORNG	-1220 /* ret code from dbnxt if ahead of range	*/
#define LKTBFULL	-1221 /* ret code from lklock			*/
#define NOSTG		-1222 /* server has insufficient storage	*/
#define RECTOOBIG	-1223 /* attempt to write big record to database*/
#define CURSORERR	-1224 /* no more available cursors , increase -c */
#define CRSORERRO	-1225 /* no more available cursors , increase
				 open_cursors in init.ora */

#define QRYPAUSE	-1239 /* query paused to give others a chance*/

#define QBWERRHI        -2100 /* lowest (negative) error - not to be used */
#define QBWERR2         -2102 /* leading operand */
#define QBWERR3         -2103 /* unexpected right parenthesis */
#define QBWERR4         -2104 /* missing operator after an operand */
#define QBWERR5         -2105 /* missing right parenthesis */
#define QBWERR6         -2106 /* more than 16 CONTAINS clauses in a query */
#define QBSTOOCOMPLEX   -2107 /* PREV, LAST or FIRST w/o SCROLLING */
#define QBWWCARDERR     -2108 /* '*' not at the end of a word */
#define QR_STACKOVF	-2202	/* progress stack overflow in qr*.c */
#define QR_NOMEM	-2203	/* stget() failed in qr*.c */


/* mode bits to be passed to dbSetOpen to open a database */
#define DBOPENFILE  DSM_DB_OPENFILE /* open the files only	*/
#define DBOPENDB    DSM_DB_OPENDB /* do crash recovery	*/
#define DBSINGLE    DSM_DB_SINGLE /* open for single-user	*/
#define DBMULTI     DSM_DB_MULTI /* open for multi-user	*/
#define DBRDONLY    DSM_DB_RDONLY /* open in read-only mode */
#define DBSCHMC     DSM_DB_SCHMC /* do the schema cache	*/
#define DBCONVERS   DSM_DB_CONVERS /* agree to open a db w/CONVERS version */
#define DB2PHASE    DSM_DB_2PHASE /* Perform 2phase commit recovery */
#define DBAIEND     DSM_DB_AIEND /* Ending AI needs some special treatment */
#define DBNOWORD    DSM_DB_NOWORD /* don't abort if word-break table is bad */
#define DBTLEND     DSM_DB_TLEND /* Ending TL needs some special treatment */
#define DBXLCH      DSM_DB_XLCH /* Character conversn needs special treatment */
#define DBAIROLL    DSM_DB_AIROLL /* AI roll forward needs special treatment */
#define DBAIBEGIN   DSM_DB_AIBEGIN /* AI begin needs some special treatment   */
#define DBNOXLTABLES DSM_DB_NOXLTABLES /* Don't try to get translation and
                                        * collation tables out of _Db record */
#define DBCONTROL   DSM_DB_CONTROL /* Only open control area - 
                                    *no time stamp update */
#define DBREST      DSM_DB_REST /* Special treatment for restore */

#define DUMMY_TABLE 1  /* Get rid of this once table numbers are passed
			  from client to server for index and locking
			  functions.                                    */

/* Work Group License user value           */
#define DB_MAX_WORKGRP_USERS 65
#define DB_MAX_PERSONAL_DB_USERS 5	/* bug 19991122-007 addition */


#define SHUTDOWN_NO_STATUS      0       /* Initial value, indicates that      */
                                        /* shutdown has not started           */

#define SHUTDOWN_INITIATED      1       /* proshut has been started           */

#define SHUTDOWN_BROKER_WAIT    2       /* All other clients are disconnected */
                                        /* and proshut is now waiting for the */
                                        /* broker to disconnect               */

#define SHUTDOWN_BROKER_ACK     3       /* Broker acknowledges that proshut   */
                                        /* is waiting and will disconnect     */

#define SHUTDOWN_COMPLETE       4       /* Shutdown is complete and proshut   */
                                        /* is about to disconnect and exit    */

#define JUNIPER_TIMEOUT		30	/* time to wait for juniper to exit */
#define JUNIPER_BROKER		1	/* time to wait for juniper to exit */

/*************************************************************/
/* Public Function Prototypes for db.c                       */
/*************************************************************/

int     dbLockRow       (dsmContext_t *pcontext, COUNT tableNumber,
			 DBKEY recid, int lockMode,   
			 TEXT *tableName, int waitFlag);    

int dbUnlockRow		(dsmContext_t *pcontext, COUNT tableNumber,
			 DBKEY recid);

/*************************************************************/
/* Public Function Prototypes for dbopen.c                   */
/*************************************************************/

dsmStatus_t
	dbenv		(dsmContext_t *pContext, TEXT *prunmode, int mode);

dsmStatus_t
dbenv3(dsmContext_t *pcontext);

ULONG    dbGetPrime      (ULONG n);

int	dbInDlc		(dsmContext_t *pcontext, TEXT *pdbname, int allowDLC);

int	dblk		(dsmContext_t *pcontext, int lockmode, int dispmsg);

int	deadpid		(dsmContext_t *pcontext, int pid);

int	pidofsrv	(dsmContext_t *pcontext, struct usrctl *pusr);

/*************************************************************/
/* Public Function Prototypes for dbclose.c                  */
/*************************************************************/

dsmStatus_t dbExit	(dsmContext_t *pcontext, int mode, int exitCode);

dsmStatus_t dbenvout	(dsmContext_t *pContext, int exitflags);

DSMVOID dbunlk (dsmContext_t *pContext, struct dbcontext *pdbcontext);

DSMVOID dbSetClose ( dsmContext_t  *pcontext, int  exitflags);

int dbGetShutdown(struct dbcontext *pdbcontext);

LONG dbGetShmgone(struct dbcontext   *pdbcontext);

LONG dbGetInService(struct dbcontext *pdbcontext);

struct usrctl * dbGetUserctl(dsmContext_t *pcontext);


/*************************************************************/
/* Public Function Prototypes for dblang.c                   */
/*************************************************************/

int     _dblang         (DSMVOID);

/*************************************************************/
/* Public Function Prototypes for dblg.c                     */
/*************************************************************/

DSMVOID	dbLogClose	(dsmContext_t *pcontext);

int	dbLogOpen	(dsmContext_t *pcontext, TEXT *pname);

int	dbLogMessage	(dsmContext_t *pcontext, TEXT *pmsg);

#if OPSYS == VMS
int	opnlgfd		(int fd);
DSMVOID	clslgfd		(int fd);
#endif

/*************************************************************/
/* Public Function Prototypes for dblksch.c                  */
/*************************************************************/

int	dblksch		(dsmContext_t *pContext, int rqst);

/*************************************************************/
/* Public Function Prototypes for dblp.c                     */
/*************************************************************/


DSMVOID	 dblpkys	(int idxnum, DBKEY ownerdbk,
			 struct ditem **ppky1, struct ditem **ppky2);

struct rnfind *dbmemlp	(int filenum, int idxnum, DBKEY ownerdbk);

/*************************************************************/
/* Public Function Prototypes for dbusr.c                    */
/*************************************************************/

struct usrctl *
	 dbGetUsrctl	(dsmContext_t *pcontext,  int  usrtyp);

dsmStatus_t
	fillstr		(dsmContext_t *pContext, TEXT *instring, QTEXT *pq);

DSMVOID	dbDelUsrctl	(dsmContext_t *pcontext, struct usrctl *pusr);

int     dbSetOpen      (dsmContext_t    *pcontext,
                        TEXT            *prunmode,
                        int             mode);
dsmStatus_t dbUserDisconnect(dsmContext_t *pcontext, int exitflags);

DSMVOID dbUserError(dsmContext_t *pcontext, LONG errorStatus, int osError,
                 int autoerr, DSMVOID *msgDataA, DSMVOID *msgDataB);

/*************************************************************/
/* Public Function Prototypes for dbwdog.c                   */
/*************************************************************/
 
int dbWdogActive(dsmContext_t *pcontext); /* formally wdogactive() */
 
int dbWatchdog(dsmContext_t *pcontext);   /* formally watchdog() */
 
DSMVOID dbKillUsers(dsmContext_t *pcontext); /* formally brokkill() */

DSMVOID dbKillAll (dsmContext_t *pcontext, int logthem); /* formally brkillall() */

DSMVOID brdestroy (int pid);

/*************************************************************/
/* Public Function Prototypes for dbquiet.c                  */
/*************************************************************/

int dbQuietPoint(dsmContext_t *pcontext, UCOUNT quietRequest, ULONG quietArg);

/*************************************************************/
/* Public Function Prototypes for dbindex.c                  */
/*************************************************************/

dsmStatus_t
dbIndexMove(
        dsmContext_t *pcontext,           /* IN database context */
        dsmIndex_t   indexNum,            /* inex to move */
        dsmArea_t    newArea,             /* area to move the new index to */
        dsmObject_t  tableNum,            /* table associated with index */
        DBKEY        *newRootdbk);        /* new root of the index */

int
dbValidArea(
        dsmContext_t *pcontext,           /* IN database context   */
        dsmArea_t    area);               /* area to move index to */

dsmStatus_t dbIndexAnchor(dsmContext_t *pcontext, ULONG area, dsmText_t *pname,
                          DBKEY ixRoot, dsmText_t *pnewname, int mode);

/*************************************************************/
/* Public Function Prototypes for dbrecord.c                  */
/*************************************************************/

dsmStatus_t dbObjectRecordDelete(dsmContext_t *pcontext, 
                                 dsmObject_t objectNumber,
                                 dsmObjectType_t objectType,
                                 dsmObject_t        associate);

dsmStatus_t dbRecordGet(dsmContext_t	*pcontext,
	                dsmRecord_t	*pRecord, 
	                dsmMask_t	 getMode);

DSMVOID dbDeleteAreaRecords(dsmContext_t *pcontext, dsmArea_t area);

dsmStatus_t dbLockQueryInit(
        dsmContext_t *pcontext,     /* IN database context */
        int           getLock);     /* IN lock/unlock lock table */

dsmStatus_t dbLockQueryGetLock(
        dsmContext_t *pcontext,     /* IN database context */
        dsmTable_t    tableNum,     /* IN table number */
        GEM_LOCKINFO *plockinfo);   /* OUT lock information */

/*************************************************************/
/* Public Function Prototypes for dbarea.c                   */
/*************************************************************/
dsmStatus_t
dbExtentRecordCreate(
        dsmContext_t   *pcontext,
        dsmArea_t       areaNumber,
        dsmExtent_t     extentNumber,
        LONG            extentSize,
        LONG            offset,   /* not used yet */
        ULONG           extentType,
        dsmText_t      *extentPath);
 
dsmStatus_t
dbExtentRecordDelete(
        dsmContext_t    *pcontext,
        dsmExtent_t      extentNumber,
        dsmArea_t        areaNumber);
 
dsmStatus_t
dbExtentRecordFixup(
        dsmContext_t   *pcontext,
        dsmArea_t       areaNumber,
        dsmExtent_t     extentId,
        dsmMask_t       extentType);
 
dsmStatus_t
dbAreaGetRecid(
        dsmContext_t *pcontext,
        dsmArea_t     areaNumber,
        dsmRecid_t   *pareaRecid,
        dsmMask_t     lockMode);
 
dsmStatus_t
dbExtentGetRecid(
        dsmContext_t *pcontext,
        dsmArea_t     areaNumber,
        dsmExtent_t   extentId,
        dsmRecid_t   *pextentRecid,
        dsmMask_t     lockMode);
 
dsmStatus_t
dbAreaRecordCreate(
        dsmContext_t  *pcontext,
        ULONG          areaVersion,  /* not used yet */
        dsmArea_t      areaNumber,
        dsmText_t     *areaName,
        dsmAreaType_t  areaType,
        dsmRecid_t     areaBlock,    /* not used yet */
        ULONG          areaAttr,     /* not used yet */
        dsmExtent_t    areaExtents,
        dsmBlocksize_t blockSize,     /* IN are block size in bytes */
        dsmRecbits_t   areaRecbits,
        dsmRecid_t    *pareaRecid);
 
dsmStatus_t
dbAreaRecordDelete(
        dsmContext_t    *pcontext,
        dsmArea_t        areaNumber);

/*************************************************************/
/* Public Function Prototypes for dbobject.c                 */
/*************************************************************/
dsmStatus_t
dbObjectUpdateRec(dsmContext_t *pcontext, dsmObject_t objectNumber,  
                  dsmObjectType_t objectType, 
                  struct omDescriptor *pobjectDesc, dsmMask_t updateMask,
                  dsmIndexList_t *pindexList, COUNT indexTotal, 
                  dsmArea_t indexArea);     

dsmStatus_t
dbObjectDeleteAssociate(dsmContext_t *pcontext, /* IN database context */
          dsmObject_t      objectNumber,        /* IN table number */
          dsmArea_t       *indexArea);          /* OUT area for the indexes */

dsmStatus_t
dbObjectDeleteByArea(dsmContext_t    *pcontext,   /* IN database context */
                        dsmArea_t    area);       /* IN area  to delete */

dsmStatus_t 
dbAreaCreate(dsmContext_t 	*pcontext,     /* IN database context */
	      dsmBlocksize_t	blockSize,     /* IN are block size in bytes */
	      dsmAreaType_t	areaType,      /* IN type of area */
	      dsmArea_t		areaNumber,    /* IN area number */
              dsmRecbits_t	areaRecbits,   /* IN # of record bits */
              dsmText_t        *areaName);      /* IN name of new area */

dsmStatus_t 
dbExtentCreate(dsmContext_t	*pcontext,	/* IN database context */
	        dsmArea_t 	areaNumber,	/* IN area for the extent */
	        dsmExtent_t	extentId,	/* IN extent id */
	        dsmSize_t 	initialSize,	/* IN initial size in blocks */
	        dsmMask_t	extentMode,	/* IN extent mode bit mask */
	        dsmText_t	*pname);	/* IN extent path name */


dsmStatus_t 
dbCursorCreate( 
        dsmContext_t *pcontext, /* IN/OUT database context */
        dsmTable_t    table,    /* IN table associated with cursor */
        dsmIndex_t    index,    /* IN index associated with cursor */
        dsmCursid_t  *pcursid,  /* OUT created cursor id */
        dsmCursid_t  *pcopy);   /* IN cursor id to copy */

dsmStatus_t 
dbCursorFind( 
        dsmContext_t *pcontext,         /* IN database context */
        dsmCursid_t  *pcursid,          /* IN/OUT cursor */
        dsmKey_t     *pkeyBase,         /* IN base key of bracket for find */
        dsmKey_t     *pkeylimit,        /* IN limit key of bracket for find */
        dsmMask_t     findType,         /* IN DSMPARTIAL DSMUNIQUE ...      */
        dsmMask_t     findMode,         /* IN find mode bit mask */
        dsmMask_t     lockMode,         /* IN lock mode bit mask */
        dsmObject_t  *pObjectNumber,    /* OUT object number the index is on */
        dsmRecid_t   *precid,           /* OUT recid found */
        dsmKey_t     *pKey);            /* OUT [optional] key found */

struct cxcursor;

dsmStatus_t 
dbCursorGet( 
        dsmContext_t *pcontext,      /* IN database context */
        dsmCursid_t   cursid,        /* IN cursor id to find */
        struct cxcursor  **pcursor); /* OUT pointer to cursor structure */

dsmStatus_t 
dbCursorDelete( 
        dsmContext_t  *pcontext,        /* IN database context */
        dsmCursid_t   *pcursid,         /* IN  pointer to cursor to delete */
        dsmBoolean_t  deleteAll);       /* IN delete all cursors indicator */

dsmStatus_t dsmRecordGet( 
        dsmContext_t *pContext,     /* IN database context */
        dsmRecord_t  *pRecord,      /* IN/OUT rec buffer to store record */
        dsmMask_t     getMode);     /* IN get mode bit mask */

dsmStatus_t
dbTableScan(
            dsmContext_t    *pcontext,        /* IN database context */
	    dsmRecord_t     *precordDesc,     /* IN/OUT row descriptor */
	    int              direction,       /* IN next, previous...  */
	    dsmMask_t        lockMode,        /* IN nolock share or excl */
            int              repair);         /* repair rm chain        */

dsmStatus_t
dbTableReset(dsmContext_t *pcontext, dsmTable_t tableNumber, int numIndexes,
               dsmIndex_t   *pindexes);

dsmStatus_t
dbTableAutoIncrement(dsmContext_t *pcontext, dsmTable_t tableNumber,
                      ULONG64 *pvalue,int update);
dsmStatus_t
dbTableAutoIncrementSet(dsmContext_t *pcontext, dsmTable_t tableNumber,
                      ULONG64 value);

dsmStatus_t
dbObjectRename(dsmContext_t *pcontext,   /* IN database context */
               dsmText_t    *pNewName,   /* IN new table name*/
               dsmObject_t   tableNum,   /* IN table name */
               dsmArea_t    *indexArea); /* OUT index area */

dsmStatus_t
dbObjectNameToNum(dsmContext_t       *pcontext,     /* IN database context */
                  dsmText_t          *psname,       /* IN object name */
                  svcLong_t          *pnum);        /* OUT object number */

dsmStatus_t
dbIndexStatsPut(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* IN key component number */
        LONG64        rowsPerKey);         /* IN rows per key value */

dsmStatus_t
dbIndexStatsGet(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* IN key component number */
        LONG64        *prowsPerKey);       /* OUT rows per key value */

dsmStatus_t
dbIndexStats(
        dsmContext_t  *pcontext,           /* IN database context */
	dsmTable_t     tableNumber,        /* IN table index is on */
        dsmIndex_t     indexNumber,        /* IN                   */
	int            component,          /* IN key component number */
        LONG64        *prowsPerKey,        /* IN/OUT rows per key value */
	int            put);               /* IN                     */

#endif  /* #ifndef DBPUB_H */
