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

#ifndef DSMDEF_H
#define DSMDEF_H

/************************************************************* 
 * This header file contains all the necessary mode and flag
 * definitions used in support of the dsmAPI.
 *************************************************************/

/*************************************************************/
/* Area types                                                */
/*************************************************************/
#define DSMAREA_TYPE_INVALID	0	/* invalid area type */
#define DSMAREA_TYPE_CBI	2	/* control recovery area type */
#define DSMAREA_TYPE_BI		3	/* recovery area type */
#define DSMAREA_TYPE_TL		4	/* transaction log area type */
#define DSMAREA_TYPE_EVENT	5	/* event log area type */
#define DSMAREA_TYPE_DATA       6       /* database data blocks */
#define DSMAREA_TYPE_AI		7	/* rollforward recovery area type */

#define DSMAREA_TYPE_MIXED	255	/* mixture of storage objects
                                         * - future use 
                                         */
#define DSMAREA_TYPE_MAXIMUM	255	/* upper limit for now */

/*************************************************************/
/* Area and extent number ranges                             */
/*************************************************************/
#define DSMAREA_INVALID	0	/* invalid area */
#define DSMAREA_CONTROL	1	/* control area */
#define DSMAREA_CBI	2	/* control recovery area - physical schema */
#define DSMAREA_BI	3	/* primary recovery area - database */
#define DSMAREA_TL      4       /* Transaction log area -  2pc coordinator */
#define DSMAREA_TEMP    5       /* SQL temp result sets area */
#define DSMAREA_SCHEMA	1	/* schema area - first data area */
#define DSMAREA_DEFAULT 6       /* default data area */
#define DSMAREA_BASE	7	/* base area number for additional areas */
#define DSMAREA_MAXIMUM	10000   /* upper limit for now */

#define DSMEXTENT_INVALID	0	/* invalid extent */
#define DSMEXTENT_MINIMUM	1	/* initial extent */
#define DSMEXTENT_MAXIMUM	10000	/* max possible extent */

/*************************************************************/
/* Macro to check area type **********************************/
/*************************************************************/
#define IS_TEMP_AREA(area) ((area==DSMAREA_TEMP)?1:0)

/*************************************************************/
/* Table number ranges                                       */
/*************************************************************/
#define DSMTABLE_USER_LAST	32766	/* last user table */
#define DSMTABLE_USER_FIRST	1	/* first user table */
#define DSMTABLE_INVALID	0	/* invalid table number */
#define DSMTABLE_SYS_FIRST	-1	/* first system table */
#define DSMTABLE_SYS_LAST	-32767  /* last system table */

/*************************************************************/
/* Index number ranges                                       */
/*************************************************************/
#define DSMINDEX_USER_LAST      32767   /* last user index */
#define DSMINDEX_USER_FIRST     0       /* first user index */
#define DSMINDEX_INVALID        -1      /* invalid index number */
#define DSMINDEX_SYS_FIRST      -2      /* first system index */
#define DSMINDEX_SYS_LAST       -32767  /* last system index */

/*************************************************************/
/* Object types                                              */
/*************************************************************/
#define DSMOBJECT_INVALID	0	/* invalid object */
#define DSMOBJECT_INVALID_MIXEDTABLE  DSMTABLE_INVALID /* invalid mixedtable */
#define DSMOBJECT_INVALID_MIXEDINDEX  DSMINDEX_INVALID /* invalid mixedindex */
#define DSMOBJECT_INVALID_TABLE	DSMTABLE_INVALID /* invalid table object */
#define DSMOBJECT_INVALID_INDEX	DSMINDEX_INVALID /* invalid index object */

#define DSMOBJECT_TABLE		1	/* table object */
#define DSMOBJECT_INDEX		2	/* index object */
#define DSMOBJECT_BLOB		3	/* blob object */
#define DSMOBJECT_RECORD	4	/* record object (for locking) */
#define DSMOBJECT_JAVACLASS	5	/* Java class object) */
#define DSMOBJECT_JAVAINDEX	6	/* Java Index object */
#define DSMOBJECT_SCHEMA	7	/* Schema object lock */
#define DSMOBJECT_TEMP_TABLE    8       /* temporary table    */
#define DSMOBJECT_TEMP_INDEX    9       /* index on a temporary table */
#define DSMOBJECT_MIXED		1024	/* mixed object */

/* remapping temporarily to match spec and code for redbird.
   this will be done correctly after next release */
#if 1
#define DSMOBJECT_MIXTABLE		DSMOBJECT_TABLE	/* mixed table */
#define DSMOBJECT_MIXINDEX		DSMOBJECT_INDEX	/* mixed index */
#else
#define DSMOBJECT_MIXTABLE		1025	/* mixed table */
#define DSMOBJECT_MIXINDEX		1026	/* mixed index */
#endif

/*************************************************************/
/* Table attributes                                          */
/*************************************************************/
#define DSMTABLE_SCANASSOCIATE   1  /* Table has an associate index
                                       for sequential scans          */

/*************************************************************/
/* Index attributes                                          */
/*************************************************************/
#define DSMINDEX_UNIQUE         1
#define DSMINDEX_WORD           2
#define DSMINDEX_MULTI_OBJECT   4  /* Index is on more than one class or table.
                                    */

/*************************************************************/
/* Public #defines for dsmblob.c                             */
/*************************************************************/
#define DSMBLOBMAXLEN   31999L          /* V1 max blob size */


/********************************************************************/
/* Transaction and Savepoint definitions.                           */
/********************************************************************/
#define DSMTXN_START    1       /* begin a tx             */

#define DSMTXN_PHASE1	22	/* prepare to commit */
#define DSMTXN_PHASE1Q2	23	/* query result of prepare */
#define DSMTXN_COMMIT	65	/* accept transaction */
#define	DSMTXN_ABORT	82	/* roll back but leave tx active */
#define DSMTXN_ABORTED  83      /* write commit note for an aborted tx */
#define DSMTXN_FORCE    84      /* Roll back tx that is in ready-to-commit state */
#define DSMTXN_SAVE	129	/* accept savepoint */
#define DSMTXN_UNSAVE	146	/* reject savepoint */

#define DSMTXN_SAVE_START	0	/* transaction start */
#define DSMTXN_SAVE_MINIMUM	1	/* minimum savepoint */
#define DSMTXN_SAVE_MAXIMUM     32767   /* maximum savepoint */

/*************************************************************/
/* types used in type parameter for dsmCursonFind (cxFind)   */
/*************************************************************/
#define DSMREGULAR 0
#define DSMPARTIAL 1
#define DSMUNIQUE  2
#define DSMDBKEY   3

#define DSMCURSOR_INVALID -1

/*************************************************************/
/* types used in type parameter for dsmCursonFind (cxNext)   */
/*************************************************************/
#define DSMGETTAG    1
/* the following are temporary kludges used to retain meaning */
#define DSMGETFIRST  2
#define DSMGETLAST   3

#define DSMGETGE     4

/*************************************************************/
/* types used in mode parameter in all Search routines       */
/*************************************************************/
#define DSMFINDFIRST 1
#define DSMFINDLAST  2
#define DSMFINDNEXT  3
#define DSMFINDPREV  4
#define DSMFINDGE    5
#define DSMFINDNEXTANDTSKGONE    6

/*************************************************************/
/*                                                           */
/*************************************************************/
#define DSMCREXTABLE  1                              

#define DSMUPDEXTABLE  1

/*************************************************************/
/* defs used for update mask parameter to dsmObjectUpdate    */
/*************************************************************/
#define DSM_OBJECT_UPDATE_ASSOCIATE 1
#define DSM_OBJECT_UPDATE_ASSOCIATE_TYPE 2
#define DSM_OBJECT_UPDATE_TABLE_AREA_NUMBER 4 /* change table to new area */
#define DSM_OBJECT_UPDATE_INDEX_AREA        8
#define DSM_OBJECT_UPDATE_ROOT             16
  
/*************************************************************/
/* Definitions for extentMode parameter for dsmExtentCreate */
/*************************************************************/
#define   EXRAW    1           /* extent is a raw device */
#define   EXNET    2           /* extent is on a reliable network file system */
#define   EXEXISTS 4           /* extent exists -- do not create              */
#define   EXFIXED  8           /* Fixed length extent                         */
#define   EXFORCE  16          /* Skip sanity checks and just do it           */

/*************************************************************/
/* Public definitions for dsmcon                             */
/*************************************************************/
#define  DSM_TAGDB_BASE      0
#define  DSM_TAGUSER_BASE    1000000   /* User specific settings which
                                          can only be set after calling
                                          dsmUserConnect              */
#define  DSM_TAGCONTEXT_BASE 2000000   /* context specific settings   */

#define DSM_TAGDB_DBNAME                   DSM_TAGDB_BASE + 1
#define DSM_TAGDB_ACCESS_ENV               DSM_TAGDB_BASE + 2
#define DSM_TAGDB_ACCESS_TYPE              DSM_TAGDB_BASE + 3
#define DSM_TAGDB_MODULE_RESTRICTIONS      DSM_TAGDB_BASE + 4
#define DSM_TAGDB_MSG_NUMBER               DSM_TAGDB_BASE + 5
#define DSM_TAGDB_MSG_DISPLAY              DSM_TAGDB_BASE + 6
#define DSM_TAGDB_MSG_TRACE                DSM_TAGDB_BASE + 7
#define DSM_TAGDB_ENSSARSY_CALLBACK        DSM_TAGDB_BASE + 8

#define DSM_TAGDB_PROTOCOL_NAME            DSM_TAGDB_BASE + 9
#define DSM_TAGDB_HOST                     DSM_TAGDB_BASE + 10
#define DSM_TAGDB_SERVICE_NAME             DSM_TAGDB_BASE + 11
#define DSM_TAGDB_MAX_CLIENTS              DSM_TAGDB_BASE + 12
#define DSM_TAGDB_MAX_SERVERS              DSM_TAGDB_BASE + 13
#define DSM_TAGDB_MIN_CLIENTS_PER_SERVER   DSM_TAGDB_BASE + 14

#define DSM_TAGDB_MIN_SERVER_PORT          DSM_TAGDB_BASE + 15
#define DSM_TAGDB_MAX_SERVER_PORT          DSM_TAGDB_BASE + 16
#define DSM_TAGDB_MAX_SERVERS_PER_PROTOCOL DSM_TAGDB_BASE + 17
#define DSM_TAGDB_MAX_SERVERS_PER_BROKER   DSM_TAGDB_BASE + 18

#define DSM_TAGDB_MSG_BUFFER_SIZE          DSM_TAGDB_BASE + 19
#define DSM_TAGDB_AI_BUFFERS               DSM_TAGDB_BASE + 20
#define DSM_TAGDB_AI_STALL                 DSM_TAGDB_BASE + 21
#define DSM_TAGDB_BI_BUFFERS               DSM_TAGDB_BASE + 22
#define DSM_TAGDB_BI_BUFFERED_WRITES       DSM_TAGDB_BASE + 23
#define DSM_TAGDB_BI_CLUSTER_AGE           DSM_TAGDB_BASE + 24
#define DSM_TAGDB_FLUSH_AT_COMMIT          DSM_TAGDB_BASE + 25
#define DSM_TAGDB_DB_BUFFERS               DSM_TAGDB_BASE + 26
#define DSM_TAGDB_CRASH_PROTECTION         DSM_TAGDB_BASE + 27
#define DSM_TAGDB_DIRECT_IO                DSM_TAGDB_BASE + 28
#define DSM_TAGDB_MAX_LOCK_ENTRIES         DSM_TAGDB_BASE + 29
#define DSM_TAGDB_HASH_TABLE_SIZE          DSM_TAGDB_BASE + 30
#define DSM_TAGDB_FORCE_ACCESS             DSM_TAGDB_BASE + 31
#define DSM_TAGDB_PWQ_SCAN_DELAY           DSM_TAGDB_BASE + 32
#define DSM_TAGDB_PWQ_MIN_WRITE            DSM_TAGDB_BASE + 33
#define DSM_TAGDB_PW_SCAN_DELAY            DSM_TAGDB_BASE + 34
#define DSM_TAGDB_PW_SCAN_SIZE             DSM_TAGDB_BASE + 35
#define DSM_TAGDB_PW_MAX_WRITE             DSM_TAGDB_BASE + 36
#define DSM_TAGDB_SHMEM_OVERFLOW_SIZE      DSM_TAGDB_BASE + 37
#define DSM_TAGDB_SPIN_AMOUNT              DSM_TAGDB_BASE + 38
#define DSM_TAGDB_MUX_LATCH                DSM_TAGDB_BASE + 39
#define DSM_TAGDB_MT_NAP_TIME              DSM_TAGDB_BASE + 40
#define DSM_TAGDB_MAX_MT_NAP_TIME          DSM_TAGDB_BASE + 41
#define DSM_TAGDB_COLLATION_NAME           DSM_TAGDB_BASE + 42
#define DSM_TAGDB_CODE_PAGE_NAME           DSM_TAGDB_BASE + 43
#define DSM_TAGDB_STORAGE_POOL             DSM_TAGDB_BASE + 44
#define DSM_TAGDB_SHUTDOWN                 DSM_TAGDB_BASE + 45
#define DSM_TAGDB_NO_SHM                   DSM_TAGDB_BASE + 46
#define DSM_TAGDB_MAX_USERS                DSM_TAGDB_BASE + 47
#define DSM_TAGDB_SEMAPHORE_SETS           DSM_TAGDB_BASE + 48
#define DSM_TAGDB_BROKER_TYPE              DSM_TAGDB_BASE + 49
#define DSM_TAGDB_SHUTDOWN_PID             DSM_TAGDB_BASE + 50
#define DSM_TAGDB_SHUTDOWN_INFO            DSM_TAGDB_BASE + 51
#define DSM_TAGDB_SHUTDOWN_STATUS          DSM_TAGDB_BASE + 52
#define DSM_TAGDB_TABLE_BASE               DSM_TAGDB_BASE + 53
#define DSM_TAGDB_TABLE_SIZE               DSM_TAGDB_BASE + 54
#define DSM_TAGDB_INDEX_BASE               DSM_TAGDB_BASE + 55
#define DSM_TAGDB_INDEX_SIZE               DSM_TAGDB_BASE + 56
#define DSM_TAGDB_SERVER_ID                DSM_TAGDB_BASE + 57
#define DSM_TAGDB_ZNFS                     DSM_TAGDB_BASE + 58
#define DSM_TAGDB_UNLINK_TEMPDB            DSM_TAGDB_BASE + 59
#define DSM_TAGDB_BI_THRESHOLD             DSM_TAGDB_BASE + 60
#define DSM_TAGDB_BI_STALL                 DSM_TAGDB_BASE + 61
#define DSM_TAGDB_USRFL                    DSM_TAGDB_BASE + 62
#define DSM_TAGDB_WB_TABLE                 DSM_TAGDB_BASE + 63
#define DSM_TAGDB_INUSE                    DSM_TAGDB_BASE + 64
#define DSM_TAGDB_TT_WB_NUM                DSM_TAGDB_BASE + 65
#define DSM_TAGDB_SET_SIGNAL_HANDLER       DSM_TAGDB_BASE + 66
#define DSM_TAGDB_DATADIR                  DSM_TAGDB_BASE + 67
#define DSM_TAGDB_GET_USRNUM               DSM_TAGDB_BASE + 68
#define DSM_TAGDB_BI_CLUSTER_SIZE          DSM_TAGDB_BASE + 69
#define DSM_TAGDB_DEFAULT_LOCK_TIMEOUT     DSM_TAGDB_BASE + 70
#define DSM_TAGDB_MSGS_FILE                DSM_TAGDB_BASE + 71
#define DSM_TAGDB_SYMFILE                  DSM_TAGDB_BASE + 72


/*************************************************************/
/* Tags for the shared memory SRVCTL fields.                 */
/*************************************************************/
#define DSM_TAGSRV_IDX             1
#define DSM_TAGSRV_SVPID           2
#define DSM_TAGSRV_BKPID           4
#define DSM_TAGSRV_PORT            8
#define DSM_TAGSRV_MAXCL          16
#define DSM_TAGSRV_USRCNT         32
#define DSM_TAGSRV_MINPORT        64
#define DSM_TAGSRV_MAXPORT       128
#define DSM_TAGSRV_TERMINATE     256
#define DSM_TAGSRV_SERVTYPE      512
#define DSM_TAGSRV_MAXSVPROT    1024
#define DSM_TAGSRV_MAXSVBROK    2048
#define DSM_TAGSRV_GROUPID      4096
#define DSM_TAGSRV_LASTPORT     8192
#define DSM_TAGSRV_SVSTATUS    16384
#define DSM_TAGSRV_ALLFIELDS   32767

/* Constants for the serverGroupId.type */
#define SERVER_SQL	1
#define SERVER_4GL	2

/*************************************************************/
/* Values for the DSM_TAGDB_MODULE_RESTRICTIONS tag          */
/*************************************************************/
#define DSM_MODULE_DEMOONLY    32

/*************************************************************/
/* Values for the ACCESSENV tag                              */
/*************************************************************/
#define DSM_4GL_ENGINE    1
#define DSM_SQL_ENGINE    2
#define DSM_JAVA_ENGINE   4
#define DSM_SCDBG         8
#define DSM_JUNIPER_BROKER 16

/*************************************************************/
/* Valid values for the ACCESS_TYPE tag                      */
/*************************************************************/
#define DSM_ACCESS_STARTUP        1
#define DSM_ACCESS_SHARED         2
#define DSM_ACCESS_SINGLE_USER    3
#define DSM_ACCESS_SERVER         4

/***************************************************************/
/* Tag for setting the lock time out value in the user control */
/***************************************************************/
#define  DSM_TAGUSER_LOCK_TIMEOUT DSM_TAGUSER_BASE + 1
#define  DSM_TAGUSER_NAME         DSM_TAGUSER_BASE + 2
#define  DSM_TAGUSER_PASSWORD     DSM_TAGUSER_BASE + 3
#define  DSM_TAGUSER_SQLHLI       DSM_TAGUSER_BASE + 4
#define  DSM_TAGUSER_CRASHWRITES  DSM_TAGUSER_BASE + 5
#define  DSM_TAGUSER_CRASHSEED    DSM_TAGUSER_BASE + 6
#define  DSM_TAGUSER_CRASHBACKOUT DSM_TAGUSER_BASE + 7
#define  DSM_TAGUSER_ROBUFFERS    DSM_TAGUSER_BASE + 8
#define  DSM_TAGUSER_BLOCKLOCK    DSM_TAGUSER_BASE + 9
#define  DSM_TAGUSER_TX_NUMBER    DSM_TAGUSER_BASE + 10
#define  DSM_TAGUSER_PID          DSM_TAGUSER_BASE + 11

/***************************************************************/
/* Tag defintions which apply to the dsm context               */
/***************************************************************/
#define DSM_TAGCONTEXT_MSG_PARM      DSM_TAGCONTEXT_BASE + 1
#define DSM_TAGCONTEXT_MSG_CALLBACK  DSM_TAGCONTEXT_BASE + 2
#define DSM_TAGCONTEXT_EXIT_CALLBACK DSM_TAGCONTEXT_BASE + 3
#define DSM_TAGCONTEXT_NO_LOGGING    DSM_TAGCONTEXT_BASE + 4

/*************************************************************/
/* Mask values for copyMode parameter of dsmContextCopy      */
/*************************************************************/
#define  DSMCONTEXTBASE   1
#define  DSMCONTEXTDB     2
#define  DSMCONTEXTUSER   4
#define  DSMCONTEXTALL    7

/*************************************************************/
/* types dsmInitConnection servers                           */
/*************************************************************/
#define DSMENTERPRISE 1
#define DSMTRITON     2

/*************************************************************/
/* Database open modes for dsmUserConnect                    */
/*************************************************************/
#define DSM_DB_OPENFILE  1 /* open the files only    */
#define DSM_DB_OPENDB    2 /* do crash recovery      */
#define DSM_DB_SINGLE    4 /* open for single-user   */
#define DSM_DB_MULTI     8 /* open for multi-user    */
#define DSM_DB_RDONLY   16 /* open in read-only mode */
#define DSM_DB_SCHMC    32 /* do the schema cache    */
#define DSM_DB_CONVERS  64 /* agree to open db w/CONVERS version see dbvers.h */
#define DSM_DB_2PHASE  128 /* Perform 2phase commit recovery */
#define DSM_DB_AIEND   256 /* Ending AI needs some special treatment */
#define DSM_DB_NOWORD  512 /* don't abort if the word-break table is bad */
#define DSM_DB_TLEND  1024 /* Ending TL needs some special treatment */
#define DSM_DB_XLCH   2048 /* Character conversion needs special treatment */
#define DSM_DB_AIROLL 4096 /* AI roll forward needs some special treatment */
#define DSM_DB_AIBEGIN 8192 /* AI begin needs some special treatment   */
#define DSM_DB_NOXLTABLES 16384 /* Don't try to get translation and collation
                            * tables out of _Db record */
#define DSM_DB_CONTROL 32768 /* Only open control area - no time stamp update */
#define DSM_DB_REST    65536 /* Special treatment for restore */

/*************************************************************/
/* Database exit modes for dsmUserDisconnect                 */
/*************************************************************/
#define DSMNICEBIT  8

/*************************************************************/
/* Database shutdown modes for DSM_TAGDB_SHUTDOWN            */
/*************************************************************/
#define DSM_SHUTDOWN_NORMAL   1
#define DSM_SHUTDOWN_ABNORMAL 2

/*************************************************************/
/* dsmSrvctlGetBest option flag values                       */
/*************************************************************/
#define DSM_GETBEST_ADDCLIENT 1  /* if set, increment # clients in server */
#define DSM_GETBEST_NEWSERVER 2  /* if set, create srvctl for new server */

/***************************************************************/
/* Lock mode values for dsmObjectLock among others             */
/***************************************************************/
/* lock flags. DSM_LK_SHARE, DSM_LK_SIX, DSM_LK_EXCL,
   DSM_LK_TYPE must not be changed.
 * ascending order implies a stronger lock
   The DSM_LK literals must have the same values as their equivalents
   in lkpub.h.
 */
#define DSM_LK_WAIT   0    /* wait for lock on lock conflict */
#define DSM_LK_NOLOCK 0    /* passed when a lock mode indication
                              is required and no locking is desired */
 
#define DSM_LK_SHARE 4     /* if not queued, this is a share lock, else
                              a share lock request.                         */
#define DSM_LK_SIX   8     /* Share lock on table with intent to get exclusive
                              locks on records in the table                 */
#define DSM_LK_EXCL  16     /* if not queued, this is an exclusive lock,
                              else an exclusive lock request. */
 
#define DSM_LK_TYPE  31     /* mask for extracting lock type */
 
#define DSM_LK_NOWAIT 32   /* return RECLKED immediately if
                              requested dbkey not available */
#define DSM_LK_AUTOREL 64  /* request for a record lock with auto-release */

#define DSM_LK_UPGRD 32     /* this is/was an upgrade request */
 
#define DSM_LK_LIMBO 64    /* this is a "limbo" lock. it will not be released
                              until database task end. */
 
#define DSM_LK_QUEUED 128   /* this is a queued request */
 
#define DSM_LK_NOHOLD 256   /* set with LKQUEUED. when queued request is
                              granted, child will request another, possibly
                              different lock. if the lock requested is
                              different, delete the queued lock */
 
#define DSM_LK_PURGED 512  /* this is a purged lock entry */
#define DSM_LK_INTASK 1024  /* record may be updated*/
#define DSM_LK_KEEP_LOCK 2048 /* Don't release at tx commit  */

/*************************************************************/
/* Definitions for dsmObjectUnlock lockMode parameter        */
/************************************************************/
#define DSM_UNLK_FREE  1  /* Unlock the record even though in a tx */

/*************************************************************/
/* Values for start up parameter defaults                    */
/*************************************************************/
#define DSM_DEFT_USRS         20
#define DSM_DEFT_LK_ENTRIES   8192
#define DSM_DEFT_CURSENT      8
#define DSM_DEFT_AIBUFS       50  /* -aibufs */
#define DSM_DEFT_BIBUFS       50  /* -aibufs */
#define DSM_DEFT_MAX_CLIENTS  5  /* default for max clients per server (-Ma) */ 
#define DSM_DEFT_MIN_CLIENTS_PER_SERVER 1 /* (-Mi) */
#define DSM_DEFT_NSERVERS     5  /* 4 servers + 1 broker (-Mn) */
#define DSM_DEFT_LAGTIME     60  /* (-G) */
#define DSM_DEFT_PWQDELAY   100  /* (-pwqdelay) */
#define DSM_DEFT_PWQMIN       1  /* (-pwqmin) */
#define DSM_DEFT_PWSDELAY     1  /* (-pwsdelay) */
#define DSM_DEFT_PWWMAX      25  /* (pwwmax) */

/*************************************************************/
/* Values for dsmLookupEnv call                              */
/*************************************************************/
#if OPSYS == WIN32API
/* this here for now out of convienence.  later it should all move */
#define BASEKEY_PROGRESS_TRITON "SOFTWARE\\Progress\\Triton\\1.0"
#define TRITON_INSTALL_DIR      "InstallRoot"
#define TRITON_CACHE_DIR        "LocalCacheRoot"

#endif /* OPSYS == WIN32API */

#define TRITON_INSTALL          "Triton_InstallRoot"
#define TRITON_CACHE            "Triton_LocalCacheRoot"

/*************************************************************/
/* Definitions for monitoring admin utility states           */
/*************************************************************/
#define DSMSTATE_LOCK_WAIT_TABLE         11
#define DSMSTATE_LOCK_WAIT_ADMIN         12
#define DSMSTATE_LOCK_WAIT_OBJECT        13
#define DSMSTATE_RECORDS_COPY            24
#define DSMSTATE_RECORDS_DELETE          26
#define DSMSTATE_INDEX_CREATE_SECONDARY  25
#define DSMSTATE_INDEX_KILL_ALL_OLD      27
#define DSMSTATE_INDEX_CREATE_NEW        31
#define DSMSTATE_INDEX_KILL_OLD          32
#define DSMSTATE_SCAN_DELETE_CHAIN       41
#define DSMSTATE_COMPACT_NONLEAF         42
#define DSMSTATE_COMPACT_LEAF            43
#define DSMSTATE_SCAN_RM                 51
#define DSMSTATE_SCAN_INDICES            52
#define DSMSTATE_IDX_COMPARE             53
#define DSMSTATE_IDX_BUILD               54

/* defines for obj_status in objblk.h */
#define DSM_OBJECT_OPEN                   1  /* Area is opened */
#define DSM_OBJECT_CLOSED                 2  /* Area is closed */
#define DSM_OBJECT_IN_REPAIR              4  /* Area is in repair process */

#define DSM_OBJECT_NAME_LEN              196 /* largest possible name stored in
                                                the storage object record */

/* defines for index and rows */
#define DSM_NULLCURSID (-1)
#define DSM_MAXRECSZ  32000  /* maximum record size			 */
#define DSM_BLKSIZE  8192
#define DSM_DEFAULT_RECBITS  8
#define DSM_MAXKEYSZ 1000

/*************************************************************/
/* Error exit setjmp macro                                   */
/*************************************************************/
#define SETJMP_ERROREXIT(pcontext, rc)                        \
    if (!(pcontext)->exitCallback)                            \
    {                                                         \
        /* No exit handling specified by the caller.          \
         * Perform exit processing via longjmp and bad rtc.   \
         */                                                   \
        if ( setjmp((pcontext)->savedEnv.jumpBuffer) )        \
        {                                                     \
            /* we've longjmp'd to here.                       \
             * Return to caller with error status             \
             */                                               \
            (pcontext)->savedEnv.jumpBufferValid = 0;         \
            (rc) = DSM_S_ERROR_EXIT;                          \
            goto done;                                        \
        }                                                     \
        (pcontext)->savedEnv.jumpBufferValid = 1;             \
    }                                                         


#endif /* DSMDEF_H  */
