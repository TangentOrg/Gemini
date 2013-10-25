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

#ifndef SCASA_H
#define SCASA_H

/*
 *  Table number range definitions for User, System, and Virtual Tables
 */
#define SCTBL_USER_LAST		((COUNT)  32767) /* 0x7FFF  last user table */
#define SCTBL_USER_FIRST	((COUNT)      1) /* 0x0001  first user table */
#define SCTBL_ZERO		((COUNT)      0) /* 0x0000  reserved */
#define SCTBL_SYS_FIRST		((COUNT)     -1) /* 0xFFFF  first system table */
#define SCTBL_SYS_LAST		((COUNT) -16383) /* 0xC001  last system table */
#define SCTBL_SYS_RESERVED	((COUNT) -16384) /* 0xC000  reserved */
#define SCTBL_VST_FIRST		((COUNT) -16385) /* 0xBFFF  first virtual table*/
#define SCTBL_VST_LAST		((COUNT) -28767) /* last virtual table */
#define SCTBL_TEMPTABLE_FIRST  ((COUNT) -28768) /* first temp result tbl */
#define SCTBL_TEMPTABLE_LAST   ((COUNT) -30816) /* last temp result tbl */
#define SCTBL_RESERVED_LAST	((COUNT) -32768) /* 0x8000  reserved */

/*
 * Index number range definitions for User, System, and Virtual Indexes
 *
 */
#define SCIDX_USER_LAST		((COUNT)  32767) /* 0x7FFF  last user index */
#define SCIDX_USER_FIRST	((COUNT)      1) /* 0x0001  first user index */
#define SCIDX_ZERO		((COUNT)      0) /* 0x0000  reserved */
#define SCIDX_SYS_FIRST		((COUNT)     -1) /* 0xFFFF  first system index */
#define SCIDX_SYS_LAST		((COUNT) -16383) /* 0xC001  last system index */
#define SCIDX_SYS_RESERVED	((COUNT) -16384) /* 0xC000  reserved */
#define SCIDX_VSI_FIRST		((COUNT) -16385) /* 0xBFFF  first virtual index*/
#define SCIDX_VSI_LAST		((COUNT) -28767) /* last virtual index */
#define SCIDX_TEMPINDEX_FIRST  ((COUNT) SCTBL_TEMPTABLE_FIRST) /* first temp result idx */
#define SCIDX_TEMPINDEX_LAST   ((COUNT) SCTBL_TEMPTABLE_LAST) /* last temp result idx */
#define SCIDX_RESERVED_LAST	((COUNT) -32768) /* 0x8000  reserved */

#define RM_IS_VST SCTBL_IS_VST  /* TODO: Remove once rm.c is available */

/* fast tests to check if tables/indexes are virtual */
#define SCTBL_IS_VST(table) (((dsmObject_t)(table) <= SCTBL_VST_FIRST) && \
 ((dsmObject_t)(table) > SCTBL_VST_LAST))
#define SCIDX_IS_VSI(index) (((dsmObject_t)(index) <= SCIDX_VSI_FIRST ) && \
 ((dsmObject_t)(index) > SCIDX_VSI_LAST))
/* tests to check if table/index is for a temporary table  */
#define SCTBL_IS_TEMP_OBJECT(obj) (((dsmObject_t)(obj)<SCTBL_TEMPTABLE_FIRST)&&\
 ((dsmObject_t)(obj) > SCTBL_TEMPTABLE_LAST))

/* SCTBL - new ASA table numbers */
#define SCTBL_ASA_FIRST		-70
#define SCTBL_OBJECT            -70     /* object table */
#define SCTBL_AREA              -71     /* area table */
#define SCTBL_EXTENT            -72     /* extent table */
#define SCTBL_ASA_LAST          -72

/* SCANAME - new ASA area names. */
/* Note - name of default area is shared with SQL92. */
#define SCANAME_CONTROL         "Control Area"
#define SCANAME_SCHEMA          "Schema Area"
#define SCANAME_BI              "Primary Recovery Area"

#define SCANAME_DEFAULT         SCANAME_SCHEMA


/* SCIDX - new ASA index numbers */
#define SCIDX_ASA_FIRST		1026
#define SCIDX_OBJID             1026    /* index on db-recid, object number,  
                                           object type, and object associate */
#define SCIDX_AREA              1027    /* index on area number */
#define SCIDX_EXTAREA           1028    /* index on extent and area numbers */
#define SCIDX_INDEX_STATS       1029    /* Index of index stats        */
#define SCIDX_ASA_LAST          1030

/* SCVST - new VST virtual table numbers */
#define SCVST_TABLE_FIRST	SCTBL_VST_FIRST
#define SCVST_CONNECT		SCVST_TABLE_FIRST /* connect virtual table */
#define SCVST_MSTRBLK		SCVST_TABLE_FIRST-1 /* Master Block VT */
#define SCVST_DBSTATUS		SCVST_TABLE_FIRST-2 /* Db Status VT */
#define SCVST_BFSTATUS		SCVST_TABLE_FIRST-3 /* Buffer Status VT */
#define SCVST_LOGGING		SCVST_TABLE_FIRST-4 /* Logging Status VT */
#define SCVST_SEGMENTS          SCVST_TABLE_FIRST-5 /* Shm Segments VT*/
#define SCVST_SERVERS           SCVST_TABLE_FIRST-6 /* DB Servers VT*/
#define SCVST_STARTUP           SCVST_TABLE_FIRST-7 /* DB Startup opts VT*/
#define SCVST_FILELIST          SCVST_TABLE_FIRST-8 /* DB files VT*/
#define SCVST_USERLOCK          SCVST_TABLE_FIRST-9 /* DB UserLocks VT*/
#define SCVST_ACTSUMMARY        SCVST_TABLE_FIRST-10 /* DB Activity Summary VT*/
#define SCVST_ACTSERVER         SCVST_TABLE_FIRST-11 /* Server Activity */
#define SCVST_ACTBUFFER         SCVST_TABLE_FIRST-12 /* Buffer Activity */
#define SCVST_ACTPWS            SCVST_TABLE_FIRST-13 /* Page Writer Activity */
#define SCVST_ACTBILOG          SCVST_TABLE_FIRST-14 /* BI Log Activity */
#define SCVST_ACTAILOG          SCVST_TABLE_FIRST-15 /* AI Log Activity */
#define SCVST_ACTLOCK           SCVST_TABLE_FIRST-16 /* Lock Activity */
#define SCVST_ACTIOTYPE         SCVST_TABLE_FIRST-17 /* IO Activity by Type */
#define SCVST_ACTIOFILE         SCVST_TABLE_FIRST-18 /* IO Activity by File */
#define SCVST_ACTSPACE          SCVST_TABLE_FIRST-19 /* Space Activity */
#define SCVST_ACTINDEX          SCVST_TABLE_FIRST-20 /* Index Activity */
#define SCVST_ACTRECORD         SCVST_TABLE_FIRST-21 /* Record Activity */
#define SCVST_ACTOTHER          SCVST_TABLE_FIRST-22 /* Other Activity */
#define SCVST_BLOCK             SCVST_TABLE_FIRST-23 /* Block Dump */
#define SCVST_USERIO            SCVST_TABLE_FIRST-24 /* User IO */
#define SCVST_LOCKREQ           SCVST_TABLE_FIRST-25 /* Lock Requests */
#define SCVST_CHKPT             SCVST_TABLE_FIRST-26 /* Checkpoints */
#define SCVST_LOCK              SCVST_TABLE_FIRST-27 /* LOCKS */
#define SCVST_TRANSACTION       SCVST_TABLE_FIRST-28 /* TRANSACTIONS */
#define SCVST_LICENSE           SCVST_TABLE_FIRST-29 /* License */
#define SCVST_TABLESTAT         SCVST_TABLE_FIRST-30 /* table statistics */
#define SCVST_INDEXSTAT         SCVST_TABLE_FIRST-31 /* index statistics */
#define SCVST_LATCH             SCVST_TABLE_FIRST-32 /* Latch Activity */
#define SCVST_RESRC             SCVST_TABLE_FIRST-33 /* Resources Activity */
#define SCVST_TXE               SCVST_TABLE_FIRST-34 /* TXE Activity     */
#define SCVST_STATBASE          SCVST_TABLE_FIRST-35 /* statist array bases */
#define SCVST_USERSTATUS        SCVST_TABLE_FIRST-36 /* user runtime stats */
#define SCVST_MYCONN            SCVST_TABLE_FIRST-37 /* this users connecion */
#define SCVST_AREASTAT          SCVST_TABLE_FIRST-38 /* Area statistics */

/* Next virtual table would go here ==>  SCVST_TABLE_FIRST-38 */
#define SCVST_TABLE_LAST	SCTBL_VST_LAST

/* SCVSI - new VSI virtual index numbers (same as their table numbers) */
#define SCVSI_INDEX_FIRST	SCVST_TABLE_FIRST
#define SCVSI_CONID		SCVST_CONNECT  /* connect id virtual index */
#define SCVSI_MBID		SCVST_MSTRBLK  /* Master Block id VI */
#define SCVSI_DBSID		SCVST_DBSTATUS /* DB Status id VI */
#define SCVSI_BFSID		SCVST_BFSTATUS /* Buffer Ststus id VI */
#define SCVSI_LOGID		SCVST_LOGGING  /* Logging Ststus id VI */
#define SCVSI_SEGID		SCVST_SEGMENTS /* Shm Segment id VI */
#define SCVSI_SRVID		SCVST_SERVERS  /* Server VI */
#define SCVSI_STRTID		SCVST_STARTUP  /* Startup opts id VI */
#define SCVSI_FILEID		SCVST_FILELIST /* File list id VI */
#define SCVSI_USERLOCKID	SCVST_USERLOCK    /* Lock id VI */
#define SCVSI_SUMID		SCVST_ACTSUMMARY  /* Activity Summary id VI */
#define SCVSI_ACTSRVID          SCVST_ACTSERVER   /* Server Activity */
#define SCVSI_ACTBUFFID         SCVST_ACTBUFFER   /* Buffer Activity */
#define SCVSI_ACTPWSID          SCVST_ACTPWS      /* Page Writer Activity */
#define SCVSI_ACTBIID           SCVST_ACTBILOG    /* BI Log Activity */
#define SCVSI_ACTAIID           SCVST_ACTAILOG    /* AI Log Activity */
#define SCVSI_ACTLCKID          SCVST_ACTLOCK     /* Lock Activity */
#define SCVSI_ACTTYPEID         SCVST_ACTIOTYPE   /* IO Activity by Type */
#define SCVSI_ACTFILEID         SCVST_ACTIOFILE   /* IO Activity by File */
#define SCVSI_ACTSPACEID        SCVST_ACTSPACE    /* Space Activity */
#define SCVSI_ACTIDXID          SCVST_ACTINDEX    /* Index Activity */
#define SCVSI_ACTRECID          SCVST_ACTRECORD   /* Record Activity */
#define SCVSI_ACTOTHERID        SCVST_ACTOTHER    /* Other Activity */
#define SCVSI_BLOCKID           SCVST_BLOCK       /* Block Dumps. */
#define SCVSI_USERIOID          SCVST_USERIO      /* User IO */
#define SCVSI_LOCKREQID         SCVST_LOCKREQ     /* Lock Requests */
#define SCVSI_CHKPTID           SCVST_CHKPT       /* Checkpoints */
#define SCVSI_LOCKID            SCVST_LOCK        /* Locks */
#define SCVSI_TRANSID           SCVST_TRANSACTION /* Transactions */
#define SCVSI_LICENSEID         SCVST_LICENSE     /* License */
#define SCVSI_TABID             SCVST_TABLESTAT   /* Table stats */
#define SCVSI_IDXID             SCVST_INDEXSTAT   /* Index stats */
#define SCVSI_LATCHID           SCVST_LATCH       /* Latch stats */
#define SCVSI_RESRCID           SCVST_RESRC       /* Resource Queue stats */
#define SCVSI_TXEID             SCVST_TXE         /* TXE stats */
#define SCVSI_STATBASID         SCVST_STATBASE
#define SCVSI_USERSTATUSID      SCVST_USERSTATUS
#define SCVSI_MYCONNID          SCVST_MYCONN
#define SCVSI_AREASTAT          SCVST_AREASTAT
#define SCVSI_INDEX_LAST	SCVST_TABLE_LAST

/* SCFLD_AREA - area table fields */
#define IDXAREA
#define IDXARNM
#define SCFLD_AREAVERS  2       /* area version */
#define SCFLD_AREANUM   3       /* area number */
#define SCFLD_AREANAME  4       /* area name */
#define SCFLD_AREATYPE  5       /* area type */
#define SCFLD_AREABLOCK 6       /* area dbkey */
#define SCFLD_AREAATTR  7       /* area attributes */
#define SCFLD_AREASYS   8       /* system area info */
#define SCFLD_AREABLKSZ 9       /* area block size */
#define SCFLD_AREABITS 10       /* area record bits */
#define SCFLD_AREAEXT  11       /* area extent count */
#define SCFLD_AREAMISC 12       /* unused - reserved */
#define SCFLD_AREAMIN  13       /* min area field position */

/* SCFLD_EXTENT - extent table fields */
#define IDXEXTAR
#define IDXEXTNM
#define SCFLD_EXTAREA   2       /* extent's area recid */
#define SCFLD_EXTVERS   3       /* extent version */
#define SCFLD_EXTNUM    4       /* extent number */
#define SCFLD_EXTANUM   5       /* extent's area number */
#define SCFLD_EXTANUM   5       /* extent's area number */
#define SCFLD_EXTTYPE   6       /* extent type */
#define SCFLD_EXTATTR   7       /* extent attributes */
#define SCFLD_EXTSYS    8       /* system extent info */
#define SCFLD_EXTSIZE   9       /* extent fixed size */
#define SCFLD_EXTPATH  10       /* extent path name */
#define SCFLD_EXTMISC  11       /* unused - reserved */
#define SCFLD_EXTMIN   12       /* min extent field position */

/* SCFLD_OBJ - object table fields */
#define IDXOBJ
#define SCFLD_OBJDB      2      /* dbkey of db record */
#define SCFLD_OBJTYPE    3      /* object type */
#define SCFLD_OBJNUM     4      /* object number */
#define SCFLD_OBJATYPE   5      /* type of associate object (optional) */
#define SCFLD_OBJASSOC   6      /* object id of associate object       */
#define SCFLD_OBJAREA    7      /* dbkey of area record */
#define SCFLD_OBJBLOCK   8      /* recid of object block */
#define SCFLD_OBJROOT    9      /* recid of object root data */
#define SCFLD_OBJATTR   10      /* object attributes */
#define SCFLD_OBJSYS    11      /* system object info */
#define SCFLD_OBJMISC   12      /* unsed - reserved */
#define SCFLD_OBJMIN    13      /* min object field position */
#define SCFLD_OBJNAME   14      /* string description of the object name */

/* SCVFD_CON - connect table fields */
#define SCVFD_CONID	 2	/* connection/user number */
#define SCVFD_CONUSR     3
#define SCVFD_CONTYPE	 4	/* connection type */
#define SCVFD_CONNAME	 5	/* connection name */
#define SCVFD_CONDEV	 6	/* connection tty or device */
#define SCVFD_CONTIM     7      /* connection time */
#define SCVFD_CONPID     8      /* connection pid */
#define SCVFD_CONSRV     9      /* connection server */
#define SCVFD_CONWAIT1	10	/* uc_wait1  - misc indicator for waiting usr */
#define SCVFD_CONWAIT	11	/* uc_wait   - reason user is waiting */
#define SCVFD_CONTASK	12	/* uc_tack   - task# if one is active, else 0 */
#define SCVFD_CONSEMNUM 13      /* uc_semnum - # of wait semaphores */
#define SCVFD_CONSEMID  14      /* uc_semnum - # of wait semaphores */
#define SCVFD_CONDISC   15      /* usrtodie  - user is to be killed */
#define SCVFD_CONRESYNC 16      /* resyncing - user is resyncing */
#define SCVFD_CONCTRLC  17      /* ctrlc     - user hit ctrlc */
#define SCVFD_CON2PHASE 18      /* uc_2phase - 2phase flags */
#define SCVFD_CONBATCH	19	/* BATH uc_Type */
#define SCVFD_CONMISC	20	/* */
#define SCVFD_CONMIN	21	/* min user field position */

/* SCVFD_MB - Master Block table fields */
#define SCVFD_MBID	   2	/* master block number */
#define SCVFD_MBDBVERS     3	/* version number of database */
#define SCVFD_MBDBSTATE    4	/* current state of database */
#define SCVFD_MBTAINTED    5	/* flags known damaged databases */
#define SCVFD_MBFLAGS      6	/* integrity indicators */
#define SCVFD_MBTOTBLKS    7	/* total # of BLKSIZE 4	sized blocks in db */
#define SCVFD_MBHIWATER    8	/* largest db blk used */
#define SCVFD_MBBIBLKSIZE  9	/* BI blocksize (kb), 0 means same as db */
#define SCVFD_MBRLCLSIZE  10	/* BI cluster size in 16k units. 0 same as 1 */
#define SCVFD_MBAIBLKSIZE 11 	/* After image blocksize */
#define SCVFD_MBLASTTASK  12	/* number of last database task given out */
#define SCVFD_MBCFILNUM   13	/* highest file # currently defined in db */
#define SCVFD_MBBISTATE   14    /* */
#define SCVFD_MBIBSEQ     15	/* incremental backup sequence no. */
#define SCVFD_MBCRDATE    16	/* date and time of database creation */
#define SCVFD_MBOPRDATE   17	/* date and time of most recent database open */
#define SCVFD_MBOPPDATE   18	/* date and time of previous database open */
#define SCVFD_MBFBDATE    19	/* date and time of last full backup of db */
#define SCVFD_MBIBDATE    20 	/* date and time of last incremental backup */
#define SCVFD_MBTIMESTAMP 21	/* schema time stamp */
#define SCVFD_MBBIOPEN    22	/* date bi file was last opnd */
#define SCVFD_MBRLTIME    23	/* latest recorded rl time */
#define SCVFD_MBBIPREV    24	/* prev value of mb_biopen */
#define SCVFD_MBMISC	  25	/* */
#define SCVFD_MBMIN	  26	/* min user field position */


/* SCVFD_DBS - Database Status table fields */
#define SCVFD_DBSID	    2	/* Database Status number */
#define SCVFD_DBSSTATE      3
#define SCVFD_DBSTAINT      4
#define SCVFD_DBSINTFLG     5
#define SCVFD_DBSDBBLKSZ    6
#define SCVFD_DBSTOTBLK     7
#define SCVFD_DBSEMPTBLK    8
#define SCVFD_DBSFREEBLK    9
#define SCVFD_DBSRMFREEBLK 10
#define SCVFD_DBSLSTTRAN   11
#define SCVFD_DBSLSTTBL    12
#define SCVFD_DBSDBVERS    13
#define SCVFD_DBSDBVERSMINOR 14
#define SCVFD_DBSCLVERSMINOR 15
#define SCVFD_DBSSHMVERS   16
#define SCVFD_DBSCHG       17
#define SCVFD_DBSIBSEQ     18
#define SCVFD_DBSINTEGRITY 19
#define SCVFD_DBSHIWATER   20
#define SCVFD_DBSBIBLKSZ   21
#define SCVFD_DBSBICLSZ    22
#define SCVFD_DBSAIBLKSZ   23

#define SCVFD_DBSSTIME     24
#define SCVFD_DBSLSTOPN    25
#define SCVFD_DBSPREVOPN   26
#define SCVFD_DBSCSTAMP    27
#define SCVFD_DBSFBDATE    28
#define SCVFD_DBSIBDATE    29
#define SCVFD_DBSCRDATE    30
#define SCVFD_DBSBIOPEN    31
#define SCVFD_DBSBISIZE    32
#define SCVFD_DBSBITRUNC   33

#define SCVFD_DBSCPAGE     34
#define SCVFD_DBSCOLL      35

#define SCVFD_DBSANUMARES  36
#define SCVFD_DBSANUMLCKS  37
#define SCVFD_DBSAMOSTLCKS 38
#define SCVFD_DBSASHRVERS  39
#define SCVFD_DBSANUMSEMS  40
#define SCVFD_DBSAMISC     41

#define SCVFD_DBSMIN       42


/* SCVFD_BFS - Database Status table fields */
#define SCVFD_BFSID	    2	/* Buffer Status number */
#define SCVFD_BFSTOT        3   /* */
#define SCVFD_BFSHASH       4   /* */
#define SCVFD_BFSUSED       5   /* */
#define SCVFD_BFSLRU        6   /* */
#define SCVFD_BFSAPWQ       7   /* */
#define SCVFD_BFSCKPQ       8   /* */
#define SCVFD_BFSMOD        9   /* */
#define SCVFD_BFSCKPM      10   /* */
#define SCVFD_BFSLSTCKP    11   /* */
#define SCVFD_BFSMISC      13   /* */
#define SCVFD_BFSMIN       14   /* */

/* SCVFD_LOG - Database Logging table fields */
#define SCVFD_LOGID	    2	/* Logging Status number */
#define SCVFD_LOGCRPROT     3   /* */
#define SCVFD_LOGCOMMD      4   /* */
#define SCVFD_LOGBIIO       5   /* */
#define SCVFD_LOGBICLAGE    6   /* */
#define SCVFD_LOGBIBLKSZ    7   /* */
#define SCVFD_LOGBICLSZ     8   /* */
#define SCVFD_LOGBIEXTS     9   /* */
#define SCVFD_LOGBILOGSZ   10   /* */
#define SCVFD_LOGBIFREE    11   /* */
#define SCVFD_LOGLASTCKP   12   /* */
#define SCVFD_LOGBIBUFFS   13   /* */
#define SCVFD_LOGBUFULL    14   /* */
#define SCVFD_LOGAIJ       15   /* */
#define SCVFD_LOGAIIO      16   /* */
#define SCVFD_LOGAIBEGIN   17   /* */
#define SCVFD_LOGAINEW     18   /* */
#define SCVFD_LOGAIOPEN    19   /* */
#define SCVFD_LOGAIGENNUM  20   /* */
#define SCVFD_LOGAIEXTS    21   /* */
#define SCVFD_LOGAICURREXT 22   /* */
#define SCVFD_LOGAIBUFFS   23   /* */
#define SCVFD_LOGAIBLKSZ   24   /* */
#define SCVFD_LOGAILOGSZ   25   /* */
#define SCVFD_LOG2PC       26   /* */
#define SCVFD_LOG2PCNAME   27   /* */
#define SCVFD_LOG2PCPR     28   /* */
#define SCVFD_LOGMISC      29   /* */
#define SCVFD_LOGMIN       30   /* */


/* SCVFD_SEG - Shared Memory Segment fields */
#define SCVFD_SEGID	    2	/* Shm Segment Status number */
#define SCVFD_SEGSEGID      3   /* */
#define SCVFD_SEGSEGSZ      4   /* */
#define SCVFD_SEGUSED       5   /* */
#define SCVFD_SEGFREE       6   /* */
#define SCVFD_SEGMISC       7   /* */
#define SCVFD_SEGMIN        8   /* */


/* SCVFD_SRV - Server fields */
#define SCVFD_SRVID	    2	/* Server id number */
#define SCVFD_SRVNUM        3   /* */
#define SCVFD_SRVPID        4   /* */
#define SCVFD_SRVTYPE       5   /* */
#define SCVFD_SRVPROTOCOL   6   /* */
#define SCVFD_SRVLOGINS     7   /* */
#define SCVFD_SRVCURRUSRS   8   /* */
#define SCVFD_SRVMAXUSRS    9   /* */
#define SCVFD_SRVPORTNUM   10   /* */
#define SCVFD_SRVMISC      11   /* */
#define SCVFD_SRVMIN       12   /* */


/* SCVFD_STRT - DB Startup Options fields */
#define SCVFD_STRTID	    2	/* DB Startup Options id number */
#define SCVFD_STRTAINAME    3
#define SCVFD_STRTBUFFS     4
#define SCVFD_STRTAIBUFFS   5
#define SCVFD_STRTBIBUFFS   6
#define SCVFD_STRTBINAME    7
#define SCVFD_STRTBITRUNC   8
#define SCVFD_STRTCRPROT    9
#define SCVFD_STRTLOCKS    10
#define SCVFD_STRTMAXCLTS  11
#define SCVFD_STRTBIDELAY  12
#define SCVFD_STRTMAXSRVS  13
#define SCVFD_STRTMAXUSERS 14
#define SCVFD_STRTBIIO     15
#define SCVFD_STRTAPWQ     16
#define SCVFD_STRTAPWS     17
#define SCVFD_STRTAPWBUFFS 18
#define SCVFD_STRTAPWW     19
#define SCVFD_STRTSPIN     20
#define SCVFD_STRTDIRECTIO 21
#define SCVFD_STRTMISC     22  /* */
#define SCVFD_STRTMIN      23  /* */

/* SCVFD_FILE - Database filelist table fields */
#define SCVFD_FILEID	    2	/* Database file number */
#define SCVFD_FILESZ        3   /* File size */
#define SCVFD_FILEEXTEND    4   /* File Extend */
#define SCVFD_FILELSIZE     5
#define SCVFD_FILEBLKSIZE   6
#define SCVFD_FILEOPENMODE  7
#define SCVFD_FILENAME      8   /* File Name */
#define SCVFD_FILEMISC      9   /* */
#define SCVFD_FILEMIN      10   /* */

/* SCVFD_USERLOCK - Database User Lock Table fields */

#define USERLOCKARRAYMAX   512  /* Num entries in each of the arrays */

#define SCVFD_USERLOCKID     2   /* */
#define SCVFD_USERLOCKUSR    3
#define SCVFD_USERLOCKNAME   4
#define SCVFD_USERLOCKRECID  5
#define SCVFD_USERLOCKTYPE   6
#define SCVFD_USERLOCKCHAIN  7
#define SCVFD_USERLOCKFLAG   8
#define SCVFD_USERLOCKMISC   9
#define SCVFD_USERLOCKMIN   10   /* */


/* SCVFD_SUM - Database Activity summary Table fields */
#define SCVFD_SUMID        2   /* */
#define SCVFD_SUMCOMMIT    3
#define SCVFD_SUMUNDO      4
#define SCVFD_SUMRECREAD   5
#define SCVFD_SUMRECUPD    6
#define SCVFD_SUMRECCREAT  7
#define SCVFD_SUMRECDEL    8
#define SCVFD_SUMRECLOCK   9
#define SCVFD_SUMRECWAIT  10
#define SCVFD_SUMDBREAD   11
#define SCVFD_SUMDBWRITE  12
#define SCVFD_SUMBIREAD   13
#define SCVFD_SUMBIWRITE  14
#define SCVFD_SUMAIWRITE  15
#define SCVFD_SUMCHKPTS   16
#define SCVFD_SUMFLUSHED  17
#define SCVFD_SUMTRCOMMIT 18
#define SCVFD_SUMUPTIME   19
#define SCVFD_SUMDBACCESS 20
#define SCVFD_SUMMISC     21
#define SCVFD_SUMMIN      22

/* SCVFD_ACTSRV - Database Server Activity Table fields */
#define SCVFD_ACTSRVID      2
#define SCVFD_ACTSRVMSGR    3
#define SCVFD_ACTSRVMSGS    4
#define SCVFD_ACTSRVBYTER   5
#define SCVFD_ACTSRVBYTES   6
#define SCVFD_ACTSRVRECR    7
#define SCVFD_ACTSRVRECS    8
#define SCVFD_ACTSRVQRYR    9
#define SCVFD_ACTSRVTIMES  10
#define SCVFD_ACTSRVTRANS  11
#define SCVFD_ACTSRVUPTIME 12
#define SCVFD_ACTSRVMISC   13
#define SCVFD_ACTSRVMIN    14

/* SCVFD_ACTBUF - Database Buffer Activity Table fields */
#define SCVFD_ACTBUFID        2
#define SCVFD_ACTBUFLREADS    3
#define SCVFD_ACTBUFLWRITES   4
#define SCVFD_ACTBUFOSRDS     5
#define SCVFD_ACTBUFOSWRTS    6
#define SCVFD_ACTBUFCHKPTS    7
#define SCVFD_ACTBUFMARKED    8
#define SCVFD_ACTBUFFLUSHED   9
#define SCVFD_ACTBUFDEFFRD   10
#define SCVFD_ACTBUFLRUSKIPS 11
#define SCVFD_ACTBUFLRUWRTS  12
#define SCVFD_ACTBUFAPWENQ   13
#define SCVFD_ACTBUFTRANS    14
#define SCVFD_ACTBUFUPTIME   15
#define SCVFD_ACTBUFMISC     16
#define SCVFD_ACTBUFMIN      17

/* SCVFD_ACTPWS - Database Page Writer Activity Table fields */
#define SCVFD_ACTPWSID      2
#define SCVFD_ACTPWSTDBW    3
#define SCVFD_ACTPWSDBW     4
#define SCVFD_ACTPWSSW      5
#define SCVFD_ACTPWSQW      6
#define SCVFD_ACTPWSCQW     7
#define SCVFD_ACTPWSSC      8
#define SCVFD_ACTPWSBS      9
#define SCVFD_ACTPWSBC     10
#define SCVFD_ACTPWSCHKP   11
#define SCVFD_ACTPWSMARKD  12
#define SCVFD_ACTPWSFLUSHD 13
#define SCVFD_ACTPWSTRANS  14
#define SCVFD_ACTPWSUPTIME 15
#define SCVFD_ACTPWSMISC   16
#define SCVFD_ACTPWSMIN    17

/* SCVFD_BILOG - Database BI Logging Activity Table fields */
#define SCVFD_ACTBIID      2
#define SCVFD_ACTBITOTW    3
#define SCVFD_ACTBIW       4
#define SCVFD_ACTBIRECW    5
#define SCVFD_ACTBIBYTEW   6
#define SCVFD_ACTBITOTR    7
#define SCVFD_ACTBIRECR    8
#define SCVFD_ACTBIBYTER   9
#define SCVFD_ACTBICLCL   10
#define SCVFD_ACTBIBBUFFW 11
#define SCVFD_ACTBIEBUFFW 12
#define SCVFD_ACTFWAITS   13
#define SCVFD_ACTFWRTS    14
#define SCVFD_ACTBIPWRTS  15
#define SCVFD_ACTBITRANS  16
#define SCVFD_ACTBIUPTIME 17
#define SCVFD_ACTBIMISC   18
#define SCVFD_ACTBIMIN    19

/* SCVFD_AILOG - Database AI Logging Activity Table fields */
#define SCVFD_ACTAIID      2
#define SCVFD_ACTAITOTW    3
#define SCVFD_ACTAIAIW     4
#define SCVFD_ACTAIRECW    5
#define SCVFD_ACTAIBYTESW  6
#define SCVFD_ACTAIBWAIT   7
#define SCVFD_ACTAINOBUF   8
#define SCVFD_ACTAIPW      9
#define SCVFD_ACTAIFORCEW 10
#define SCVFD_ACTAITRANS  11
#define SCVFD_ACTAIUPTIME 12
#define SCVFD_ACTAIMISC   13
#define SCVFD_ACTAIMIN    14

/* SCVFD_ACTLCK - Database Locking Activity Table fields */
#define SCVFD_ACTLCKID        2
#define SCVFD_ACTLCKSHRR      3
#define SCVFD_ACTLCKEXCLR     4
#define SCVFD_ACTLCKUPGR      5
#define SCVFD_ACTLCKRECGETR   6
#define SCVFD_ACTLCKSHRL      7
#define SCVFD_ACTLCKEXCLL     8
#define SCVFD_ACTLCKUPGL      9
#define SCVFD_ACTLCKRECGETL  10
#define SCVFD_ACTLCKSHRW     11
#define SCVFD_ACTLCKEXCLW    12
#define SCVFD_ACTLCKUPGW     13
#define SCVFD_ACTLCKRECGETW  14
#define SCVFD_ACTLCKCANCLREQ 15
#define SCVFD_ACTLCKDOWNGR   16
#define SCVFD_ACTLCKREDREQ   17
#define SCVFD_ACTLCKTRANS    18
#define SCVFD_ACTLCKUPTIME   19
#define SCVFD_ACTLCKEXCFIND  20
#define SCVFD_ACTLCKSHRFIND  21
#define SCVFD_ACTLCKMISC     22
#define SCVFD_ACTLCKMIN      23

/* SCVFD_ACTTYPE - Database IO Activity by Type Table fields */
#define SCVFD_ACTTYPEID      2
#define SCVFD_ACTTYPEIDXR    3
#define SCVFD_ACTTYPEDATAR   4
#define SCVFD_ACTTYPEBIR     5
#define SCVFD_ACTTYPEAIR     6
#define SCVFD_ACTTYPEIDXW    7
#define SCVFD_ACTTYPEDATAW   8
#define SCVFD_ACTTYPEBIW     9
#define SCVFD_ACTTYPEAIW    10
#define SCVFD_ACTTYPETRANS  11
#define SCVFD_ACTTYPEUPTIME 12
#define SCVFD_ACTTYPEMISC   13
#define SCVFD_ACTTYPEMIN    14

/* SCVFD_ACTFILE - Database IO Activity by File Table fields */
#define SCVFD_ACTFILEID         2
#define SCVFD_ACTFILENAME       3
#define SCVFD_ACTFILEREADS      4
#define SCVFD_ACTFILEWRITES     5
#define SCVFD_ACTFILEEXTENDS    6
#define SCVFD_ACTFILETRANS      7
#define SCVFD_ACTFILEBWRITE     8
#define SCVFD_ACTFILEUBWRITE    9
#define SCVFD_ACTFILEBREAD     10
#define SCVFD_ACTFILEUBREADS   11
#define SCVFD_ACTFILEUPTIME    12
#define SCVFD_ACTFILEMISC      13
#define SCVFD_ACTFILEMIN       14

/* SCVFD_ACTSPACE - Database Space Activity Table fields */
#define SCVFD_ACTSPACEID       2
#define SCVFD_ACTSPACEDBEXD    3
#define SCVFD_ACTSPACETAKEF    4
#define SCVFD_ACTSPACERETF     5
#define SCVFD_ACTSPACENEWRM    6
#define SCVFD_ACTSPACEFROMRM   7
#define SCVFD_ACTSPACEFROMFR   8
#define SCVFD_ACTSPACEBYTES    9
#define SCVFD_ACTSPACEEXAMINE 10
#define SCVFD_ACTSPACEREMOVED 11
#define SCVFD_ACTSPACEFRNTADD 12
#define SCVFD_ACTSPACEBCKADD  13
#define SCVFD_ACTSPACEFR2BCK  14
#define SCVFD_ACTSPACELOCKD   15
#define SCVFD_ACTSPACETRANS   16
#define SCVFD_ACTSPACEUPTIME  17
#define SCVFD_ACTSPACEMISC    18
#define SCVFD_ACTSPACEMIN     19

/* SCVFD_ACTIDX - Database Index Activity Table fields */
#define SCVFD_ACTIDXID      2
#define SCVFD_ACTIDXFND     3
#define SCVFD_ACTIDXCREAT   4
#define SCVFD_ACTIDXDEL     5
#define SCVFD_ACTIDXREM     6
#define SCVFD_ACTIDXSPLIT   7
#define SCVFD_ACTIDXFREE    8
#define SCVFD_ACTIDXTRANS   9
#define SCVFD_ACTIDXUPTIME 10
#define SCVFD_ACTIDXMISC   11
#define SCVFD_ACTIDXMIN    12

/* SCVFD_ACTREC - Database Record Activity Table fields */
#define SCVFD_ACTRECID        2
#define SCVFD_ACTRECRREAD     3
#define SCVFD_ACTRECRUPDD     4
#define SCVFD_ACTRECRCREAT    5
#define SCVFD_ACTRECRDEL      6
#define SCVFD_ACTRECFREAD     7
#define SCVFD_ACTRECFCREAT    8
#define SCVFD_ACTRECFDEL      9
#define SCVFD_ACTRECFUPD     10
#define SCVFD_ACTRECBREAD    11
#define SCVFD_ACTRECBCREAT   12
#define SCVFD_ACTRECBDEL     13
#define SCVFD_ACTRECBUPD     14
#define SCVFD_ACTRECTRANS    15
#define SCVFD_ACTRECUPTIME   16
#define SCVFD_ACTRECMISC     17
#define SCVFD_ACTRECMIN      18

/* SCVFD_ACTOTHER - Database Other Activity Table fields */
#define SCVFD_ACTOTHERID     2
#define SCVFD_ACTOTHERCOMMIT 3
#define SCVFD_ACTOTHERUNDO   4
#define SCVFD_ACTOTHERWAIT   5
#define SCVFD_ACTOTHERFLUSH  6
#define SCVFD_ACTOTHERTRANS  7
#define SCVFD_ACTOTHERUPTIME 8
#define SCVFD_ACTOTHERMISC   9
#define SCVFD_ACTOTHERMIN   10

/* SCVFD_BLOCK - Database Block Dump Table */
#define SCVFD_BLOCKID        2
#define SCVFD_BLOCKDBKEY     3
#define SCVFD_BLOCKTYPE      4
#define SCVFD_BLOCKCHAIN     5
#define SCVFD_BLOCKBKUP      6
#define SCVFD_BLOCKNEXT	     7
#define SCVFD_BLOCKUPDATE    8
#define SCVFD_BLOCKAREA      9
#define SCVFD_BLOCKBLOCK    10
#define SCVFD_BLOCKMISC     11
#define SCVFD_BLOCKMIN      12

/* SCVFD_USERIO - Database User I/O Table fields */
#define SCVFD_USERIOID        2
#define SCVFD_USERIOUSR       3
#define SCVFD_USERIONAME      4
#define SCVFD_USERIODBACC     5
#define SCVFD_USERIODBREAD    6
#define SCVFD_USERIODBWRITE   7
#define SCVFD_USERIOBIREAD    8
#define SCVFD_USERIOBIWRITE   9
#define SCVFD_USERIOAIREAD   10
#define SCVFD_USERIOAIWRITE  11
#define SCVFD_USERIOMISC     12
#define SCVFD_USERIOMIN      13

/* SCVFD_ACTLOCKREQ - Database Lock Requests Table fields */
#define SCVFD_LOCKREQID      2
#define SCVFD_LOCKREQNUM     3
#define SCVFD_LOCKREQNAME    4
#define SCVFD_LOCKREQRLOCK   5
#define SCVFD_LOCKREQRWAIT   6
#define SCVFD_LOCKREQTLOCK   7
#define SCVFD_LOCKREQTWAIT   8
#define SCVFD_LOCKREQSLOCK   9
#define SCVFD_LOCKREQSWAIT  10
#define SCVFD_LOCKREQEXCFIND 11
#define SCVFD_LOCKREQSHRFIND 12
#define SCVFD_LOCKREQMISC   13
#define SCVFD_LOCKREQMIN    14

/* SCVFD_ACTCHKPT - Database Checkpoints Table fields */
#define SCVFD_CHKPTID      2
#define SCVFD_CHKPTTIME    3
#define SCVFD_CHKPTLEN     4
#define SCVFD_CHKPTDIRTY   5
#define SCVFD_CHKPTCPTQ    6
#define SCVFD_CHKPTSCAN    7
#define SCVFD_CHKPTAPWQ    8
#define SCVFD_CHKPTFLUSH   9
#define SCVFD_CHKPTMISC   10
#define SCVFD_CHKPTMIN    11

/* SCVFD_LOCK - Database Lock Table fields */
#define SCVFD_LOCKID       2
#define SCVFD_LOCKUSR      3
#define SCVFD_LOCKNAME     4
#define SCVFD_LOCKTYPE     5
#define SCVFD_LOCKTABLE    6
#define SCVFD_LOCKRECID    7
#define SCVFD_LOCKCHAIN    8
#define SCVFD_LOCKFLAGS    9
#define SCVFD_LOCKMISC    10
#define SCVFD_LOCKMIN     11

/* SCVFD_TRANS - Database Transaction Table fields */
#define SCVFD_TRANSID       2
#define SCVFD_TRANSSTATE    3
#define SCVFD_TRANSFLAG     4
#define SCVFD_TRANSUNUM     5
#define SCVFD_TRANSNUM      6
#define SCVFD_TRANSCNTR     7
#define SCVFD_TRANSTXTIME   8
#define SCVFD_TRANSCOORD    9
#define SCVFD_TRANSCOORDTX 10
#define SCVFD_TRANSUPTIME  11
#define SCVFD_TRANSMISC    12
#define SCVFD_TRANSMIN     13

/* SCVFD_LICENSE - License Table fields */
#define SCVFD_LICID         2
#define SCVFD_LICVALIDUSR   3
#define SCVFD_LICACTIVECON  4
#define SCVFD_LICMAXACTIVE  5
#define SCVFD_LICMINACTIVE  6
#define SCVFD_LICBATCHCON   7
#define SCVFD_LICMAXBATCH   8
#define SCVFD_LICMINBATCH   9
#define SCVFD_LICCURRCON   10
#define SCVFD_LICMAXCURR   11
#define SCVFD_LICMINCURR   12
#define SCVFD_LICMIN       13

/* SCVFD_TABLESTAT */
#define SCVFD_TABSTATID     2
#define SCVFD_TABSTATREAD   3
#define SCVFD_TABSTATUPDATE 4
#define SCVFD_TABSTATCREATE 5
#define SCVFD_TABSTATDELETE 6
#define SCVFD_TABSTATMIN    7
 
/* SCVFD_INDEXSTAT */
#define SCVFD_IDXSTATID     2
#define SCVFD_IDXSTATREAD   3
#define SCVFD_IDXSTATCREATE 4
#define SCVFD_IDXSTATDELETE 5
#define SCVFD_IDXSTATSPLIT  6
#define SCVFD_IDXSTATBLKDEL 7
#define SCVFD_IDXSTATMIN    8

/* SCVFD_LATCH - Latch table fields */
#define SCVFD_LATCHID       2    /* Latch id number  */
#define SCVFD_LATCHNAME     3    /* Latch name  */
#define SCVFD_LATCHHOLD     4    /* User number of last latch holder */
#define SCVFD_LATCHQHOLD    5    /* User number of last queue holder */
#define SCVFD_LATCHTYPE     6    /* Latch type */
#define SCVFD_LATCHWAIT     7    /* Latch wait count */
#define SCVFD_LATCHLOCK     8    /* Latch lock count */
#define SCVFD_LATCHSPIN     9    /* Latch spin count */
#define SCVFD_LATCHBUSY    10    /* Latch busy count */
#define SCVFD_LATCHLOCKEDT 11    /* For calculation of duration of lock */
#define SCVFD_LATCHLOCKT   12    /* Microseconds the latch was locked */
#define SCVFD_LATCHWAITT   13    /* Microseconds of wait or spin time */
#define SCVFD_LATCHMIN     14    /* min user field position */
 
/* SCVFD_RESRC - Resource table fields */
#define SCVFD_RESRCID       2    /* Resource id         */
#define SCVFD_RESRCNAME     3    /* Resource name       */
#define SCVFD_RESRCLOCK     4    /* Resource lock count */
#define SCVFD_RESRCWAIT     5    /* Resource wait count */
#define SCVFD_RESRCWTIME    6    /* Resource wait time  */
#define SCVFD_RESRCMIN      7    /* min user field position */
 
/* SCVFD_TXE - Transaction end lock activity fields */
/*  All of the fields after TXEID are array fields of this extent */
#define SCVFD_TXE_ARRAY_SIZE 9
 
#define SCVFD_TXEID         2    /* An id                       */
#define SCVFD_TXETYPE       3    /* Name of TXE lock type       */
#define SCVFD_TXELOCKS      4    /* Count of locks              */
#define SCVFD_TXELOCKSS     5    /* Count of concurrent lock grants */
#define SCVFD_TXEWAITS      6    /* Count of queued lock requests */
#define SCVFD_TXEWAITSS     7    /* Count of concurrent waits     */
#define SCVFD_TXELOCKT      8    /* Time in micro-seconds lock was held */
#define SCVFD_TXEWAITT      9    /* Time in micro-seconds lock was qued */
#define SCVFD_TXEMIN       10    /* min user field position             */

/* SCVFD_STATBASE */
#define SCVFD_STATBASID   2
#define SCVFD_STATTABBASE 3
#define SCVFD_STATIDXBASE 4
#define SCVFD_STATBASMIN  5
 
/* SCVFD_USERSTATUS */
#define SCVFD_USTATUSUSER    2  /* user number (also row-id) */
#define SCVFD_USTATUSOBJECT  3  /* object number */
#define SCVFD_USTATUSOBJTYPE 4  /* object type */
#define SCVFD_USTATUSSTATE   5  /* state of the utility */
#define SCVFD_USTATUSCTR     6  /* operation count */
#define SCVFD_USTATUSTARGET  7  /* counter end value */
#define SCVFD_USTATUSOP      8  /* operation name */
#define SCVFD_USTATUSMIN     9 

/* SCVFD_MYCONNECTION */
#define SCVFD_MYCONNID       2  /* also row-id */
#define SCVFD_MYCONNUSERNUM  3  /* user number */
#define SCVFD_MYCONNPID      4  /* user pid */
#define SCVFD_MYCONNNUMSBUFF 5  /* user number of sequential buffers
                                 * (updates allowed)
                                 */
#define SCVFD_MYCONNUSEDSBUFF 6
#define SCVFD_MYCONNMIN       7

/* SCVFD_AREASTAT */
#define SCVFD_AREASTATID         2  /* also row-id */
#define SCVFD_AREASTATNAME       3  /* area name */
#define SCVFD_AREASTATNUM        4  /* area Number */
#define SCVFD_AREASTATBLOCKS     5  /* area Number of blocks*/
#define SCVFD_AREASTATHIWATER    6  /* area hi water mark */
#define SCVFD_AREASTATEXTENTS    7  /* area Number of extents */
#define SCVFD_AREASTATLASTEXTENT 8  /* area Extent where the Hiwater resides */
#define SCVFD_AREASTATFREEBLOCKS 9  /* area Number of free blocks */
#define SCVFD_AREASTATRMBLOCKS   10 /* area Number of RM blocks */
#define SCVFD_AREASTATMIN        11 /* area Number */

#endif  /* #ifndef SCASA_H */

