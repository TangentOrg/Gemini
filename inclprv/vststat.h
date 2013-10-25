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

#ifndef VSTSTAT_H
#define VSTSTAT_H

/***************************************************/
/* definition of the VST schema, used by upvst     */
/***************************************************/

#include "dbutil.h"
#ifdef INCL_VST_SCHEMA_AND_KEYS
#include "ditem.h"
#endif /* INCL_VST_SCHEMA_AND_KEYS */
#include "scasa.h" /* TODO - where should this go */

GLOBAL FILSTAT vstfil[] =
/* Note: tdbk = template dbkey, ttype = table type (table/view) . */
/*name		 number    #key	#kcomp  #kfld  #fld  dbk   tdbk  area    ttype*/
{{"_Connect",    SCVST_CONNECT, 0,  0,    1,     18,  0,    0,   0, 0},  /* VST */
 {"_MstrBlk",    SCVST_MSTRBLK, 0,  0,    1,     24,  0,    0,   0, 0},  /* VST */
 {"_DbStatus",   SCVST_DBSTATUS,0,  0,    1,     39,  0,    0,   0, 0},  /* VST */
 {"_BuffStatus", SCVST_BFSTATUS,0,  0,    1,     11,  0,    0,   0, 0},  /* VST */
 {"_Logging",    SCVST_LOGGING, 0,  0,    1,     27,  0,    0,   0, 0},  /* VST */
 {"_Segments",   SCVST_SEGMENTS,0,  0,    1,     6,   0,    0,   0, 0},  /* VST */
 {"_Servers",    SCVST_SERVERS, 0,  0,    1,     10,  0,    0,   0, 0},  /* VST */
 {"_Startup",    SCVST_STARTUP, 0,  0,    1,     22,  0,    0,   0, 0},  /* VST */
 {"_Filelist",   SCVST_FILELIST,0,  0,    1,     8,   0,    0,   0, 0},  /* VST */
 {"_UserLock",   SCVST_USERLOCK,0,  0,    1,     8,   0,    0,   0, 0},  /* VST */
 {"_ActSummary", SCVST_ACTSUMMARY,0,0,    1,     19,  0,    0,   0, 0},  /* VST */
 {"_ActServer",  SCVST_ACTSERVER,0, 0,    1,     12,  0,    0,   0, 0},  /* VST */
 {"_ActBuffer",  SCVST_ACTBUFFER,0, 0,    1,     15,  0,    0,   0, 0},  /* VST */
 {"_ActPWs",     SCVST_ACTPWS,  0,  0,    1,     15,  0,    0,   0, 0},  /* VST */
 {"_ActBILog",   SCVST_ACTBILOG,0,  0,    1,     17,  0,    0,   0, 0},  /* VST */
 {"_ActAILog",   SCVST_ACTAILOG,0,  0,    1,     12,  0,    0,   0, 0},  /* VST */
 {"_ActLock",    SCVST_ACTLOCK, 0,  0,    1,     21,  0,    0,   0, 0},  /* VST */
 {"_ActIOType",  SCVST_ACTIOTYPE,0, 0,    1,     12,  0,    0,   0, 0},  /* VST */
 {"_ActIOFile",  SCVST_ACTIOFILE,0, 0,    1,     12,  0,    0,   0, 0},  /* VST */
 {"_ActSpace",   SCVST_ACTSPACE,0,  0,    1,     17,  0,    0,   0, 0},  /* VST */
 {"_ActIndex",   SCVST_ACTINDEX,0,  0,    1,     10,  0,    0,   0, 0},  /* VST */
 {"_ActRecord",  SCVST_ACTRECORD,0, 0,    1,     16,  0,    0,   0, 0},  /* VST */
 {"_ActOther",   SCVST_ACTOTHER,0,  0,    1,     8,   0,    0,   0, 0},  /* VST */
 {"_Block",      SCVST_BLOCK,   0,  0,    1,     10,  0,    0,   0, 0},  /* VST */
 {"_UserIO",     SCVST_USERIO,  0,  0,    1,     11,  0,    0,   0, 0},  /* VST */
 {"_LockReq",    SCVST_LOCKREQ, 0,  0,    1,     12,  0,    0,   0, 0},  /* VST */
 {"_Checkpoint", SCVST_CHKPT,   0,  0,    1,     9,   0,    0,   0, 0},  /* VST */
 {"_Lock",       SCVST_LOCK,    0,  0,    1,     8,   0,    0,   0, 0},  /* VST */
 {"_Trans",  SCVST_TRANSACTION, 0,  0,    1,     11,  0,    0,   0, 0},  /* VST */
 {"_License",    SCVST_LICENSE, 0,  0,    1,     11,  0,    0,   0, 0},  /* VST */
 {"_TableStat",  SCVST_TABLESTAT,0, 0,    1,     5,   0,    0,   0, 0},  /* VST */
 {"_IndexStat",  SCVST_INDEXSTAT,0, 0,    1,     6,   0,    0,   0, 0},  /* VST */
 {"_Latch",      SCVST_LATCH,   0,  0,    1,     12,  0,    0,   0, 0},  /* VST */
 {"_Resrc",      SCVST_RESRC,   0,  0,    1,     5,   0,    0,   0, 0},  /* VST */
 {"_TxeLock",    SCVST_TXE,     0,  0,    1,     8,   0,    0,   0, 0},  /* VST */
 {"_StatBase",   SCVST_STATBASE,0,  0,    1,     3,   0,    0,   0, 0},  /* VST */
 {"_UserStatus", SCVST_USERSTATUS,0,0,    1,     7,   0,    0,   0, 0},  /* VST */
 {"_MyConnection",SCVST_MYCONN, 0,  0,    1,     5,   0,    0,   0, 0},  /* VST */
 {"_AreaStatus",SCVST_AREASTAT, 0,  0,    1,     8,   0,    0,   0, 0},  /* VST */
};

#ifdef INCL_VST_SCHEMA_AND_KEYS
/*note-#key,#kcomp,#kfld,#fld,dbk and tdbk are provided by scdbg.c*/
/* File file fields */

GLOBAL FLDSTAT vstfld[] =
/*file name	type		pos    mand sys init
		format		dbkey	extent	  label	 help  Case */
{
/* VST Connect Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	Case */
{0,"_Connect-Id",     INTFLD,  SCVFD_CONID,       1,   1,  "?",  /* 0 */
                "->>>>>>>>>9",  0, 	0,       
                "Connect-Id",      ""},
{0,"_Connect-Usr",     INTFLD,  SCVFD_CONUSR,     1,   0,  "?",  /* 1 */
                "->>>>>>>>>9",  0, 	0,       
                "User-Id",      ""},
{0,"_Connect-Type",   CHRFLD,  SCVFD_CONTYPE,     1,   0,  "0",  /* 2 */
                "x(4)",         0, 	0,       
                "Type",      ""},   
{0,"_Connect-Name",   CHRFLD,  SCVFD_CONNAME,     1,   0,  "?",  /* 3 */
                "x(8)",         0, 	0,       
                "Name",      ""},  
{0,"_Connect-Device", CHRFLD,	SCVFD_CONDEV,      1,   0,  "?",  /* 4 */
		"x(12)",	0,	0,       
                "Device",	  ""},
{0,"_Connect-Time",   CHRFLD,	SCVFD_CONTIM,      1,   0,  "?",  /* 5 */
		"x(24)",	0,	0,       
                "Login time",	  ""},
{0,"_Connect-Pid",    INTFLD,	SCVFD_CONPID,      1,   0,  "0",  /* 6 */
		"->>>>>>>>>9",	0,	0,       
                "Pid",	  ""},
{0,"_Connect-Server", INTFLD,	SCVFD_CONSRV,      1,   0,  "0",  /* 7 */
		"->>>>>>>>>9",	0,	0,       
                "Srv",	  ""},
{0,"_Connect-Wait1",  INTFLD,	SCVFD_CONWAIT1,    1,   0,  "0", /* 8 */
		"->>>>>>>>>9",	0,	0,	 
                "Wait1",	  ""},
{0,"_Connect-Wait",   CHRFLD,  SCVFD_CONWAIT,     1,	0,  "0",  /* 9 */
                "x(4)",         0,      0,        
                "Wait",	  ""},
{0,"_Connect-TransId", INTFLD,	SCVFD_CONTASK,     1,   0,  "0",  /* 10 */
		"->>>>>>>>>9",	0,	0,	 
                "Trans id",	  ""},
{0,"_Connect-SemNum", INTFLD,	SCVFD_CONSEMNUM,   1,   0,  "0",  /* 11 */
		"->>>>>>>>>9",	0,	0,	 
                "SemNum",	  ""},
{0,"_Connect-semid", INTFLD,	SCVFD_CONSEMID,   1,   0,  "0",   /* 12 */
		"->>>>>>>>>9",	0,	0,	 
                "Semid",	  ""},
{0,"_Connect-Disconnect",INTFLD,SCVFD_CONDISC,    1,   0,  "0",  /* 13 */
		"->>>>>>>>>9",	0,	0,	 
                "Disconnect",	  ""},
{0,"_Connect-Resync", INTFLD,	SCVFD_CONRESYNC,   1,   0,  "0",  /* 14 */
		"->>>>>>>>>9",	0,	0,	 
                "Resyncing",	  ""},
{0,"_Connect-Interrupt", INTFLD,SCVFD_CONCTRLC,   1,   0,  "0",  /* 15 */
		"->>>>>>>>>9",	0,	0,	 
                "Interrupt",	  ""},
{0,"_Connect-2phase", INTFLD,  SCVFD_CON2PHASE,   1,   0,  "0",  /* 16 */
		"->>>>>>>>>9",	0,	0,	 
                "2phase",	  ""},
{0,"_Connect-Batch",   CHRFLD,	SCVFD_CONBATCH,    1,   0,  "?",  /* 17 */
		"x(3)",	0,	0,	 
                "Batch User",	  ""},
{0,"_Connect-Misc",    INTFLD,  SCVFD_CONMISC,    1,   0,  "0",  /* 18 */
		"->>>>>>>>>9",	0,	0,	 
                "Misc",	  ""},


/* VST Master Block Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{1,"_MstrBlk-Id",        INTFLD,  SCVFD_MBID,       1,   1,  "?",  /* 19 */
                "->>>>>>>>>9",  0, 	0,       "",      ""},
{1,"_MstrBlk-dbvers",    INTFLD,  SCVFD_MBDBVERS,   1,   0,  "0",  /* 20 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-dbstate",   INTFLD,  SCVFD_MBDBSTATE,  1,   0,  "0",  /* 21 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-tainted",   INTFLD,  SCVFD_MBTAINTED,  1,   0,  "0",  /* 22 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-integrity", INTFLD,  SCVFD_MBFLAGS,    1,   0,  "0",  /* 23 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-totblks",   INTFLD,  SCVFD_MBTOTBLKS,  1,   0,  "0",  /* 24 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-hiwater",   INTFLD,  SCVFD_MBHIWATER,  1,   0,  "0",  /* 25 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-biblksize", INTFLD,  SCVFD_MBBIBLKSIZE,1,   0,  "0",  /* 26 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-rlclsize",  INTFLD,  SCVFD_MBRLCLSIZE, 1,   0,  "0",  /* 27 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-aiblksize", INTFLD,  SCVFD_MBAIBLKSIZE,1,   0,  "0",  /* 28 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-lasttask",  INTFLD,  SCVFD_MBLASTTASK, 1,   0,  "0",  /* 29 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-cfilnum",   INTFLD,  SCVFD_MBCFILNUM,  1,   0,  "0",  /* 30 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-bistate",   INTFLD,  SCVFD_MBBISTATE,  1,   0,  "0",  /* 31 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-ibseq",     INTFLD,  SCVFD_MBIBSEQ,    1,   0,  "0",  /* 32 */
		"->>>>>>>>>9",	0,	0,	 "",	  ""},
{1,"_MstrBlk-crdate",    CHRFLD,  SCVFD_MBCRDATE,   1,   0,  "?",  /* 33 */
                "x(24)",        0,      0,       "",      ""},
{1,"_MstrBlk-oprdate",   CHRFLD,  SCVFD_MBOPRDATE,  1,   0,  "?",  /* 34 */
                "x(24)",        0,      0,       "",      ""},
{1,"_MstrBlk-oppdate",   CHRFLD,  SCVFD_MBOPPDATE,  1,   0,  "?",  /* 35 */
                "x(24)",        0,      0,       "",      ""},
{1,"_MstrBlk-fbdate",    CHRFLD,  SCVFD_MBFBDATE,   1,   0,  "?",  /* 36 */
                "x(24)",        0,      0,       "",      ""},
{1,"_MstrBlk-ibdate",    CHRFLD,  SCVFD_MBIBDATE,   1,   0,  "?",  /* 37 */
                "x(24)",       0,      0,       "",      ""},
{1,"_MstrBlk-timestamp", CHRFLD,  SCVFD_MBTIMESTAMP,1,   0,  "?",  /* 38 */
                "x(24)",        0,      0,       "",      ""},
{1,"_MstrBlk-biopen",    CHRFLD,  SCVFD_MBBIOPEN,   1,   0,  "?",  /* 39 */
                "x(24)",        0,      0,       "",      ""},
{1,"_MstrBlk-rltime",    CHRFLD,  SCVFD_MBRLTIME,   1,   0,  "?",  /* 40 */
                "x(24)",        0,      0,       "",      ""},
{1,"_MstrBlk-biprev",    CHRFLD,  SCVFD_MBBIPREV,   1,   0,  "?",  /* 41 */
                "x(24)",        0,      0,       "",      ""},
{1,"_MstrBlk-Misc",      INTFLD,  SCVFD_MBMISC,     1,   0,  "0",  /* 42 */
                "->>>>>>>>>9",  0, 	0,       
                "Misc",      ""},
		
/* VST Database Status Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{2,"_DbStatus-Id",       INTFLD,  SCVFD_DBSID,      1,   1,  "0",  /* 43 */
                "->>>>>>>>>9",  0, 	0,       
                "Database Id",      ""},
{2,"_DbStatus-state",    INTFLD,  SCVFD_DBSSTATE,   1,   0,  "0",  /* 44 */
                "->>>>>>>>>9",  0, 	0,       
                "Database state",      ""},
{2,"_DbStatus-tainted",  INTFLD,  SCVFD_DBSTAINT,   1,   0,  "0",  /* 45 */
                "->>>>>>>>>9",  0, 	0,       
                "Database damaged flags",      ""},
{2,"_DbStatus-IntFlags", INTFLD,  SCVFD_DBSINTFLG,  1,   0,  "0",  /* 46 */
                "->>>>>>>>>9",  0, 	0,       
                "Integrity flags",      ""},
{2,"_DbStatus-DbBlkSize",INTFLD,  SCVFD_DBSDBBLKSZ, 1,   0,  "0",  /* 47 */
                "->>>>>>>>>9",  0, 	0,       
                "Database block size",      ""},
{2,"_DbStatus-TotalBlks",INTFLD,  SCVFD_DBSTOTBLK,  1,   0,  "0",  /* 48 */
                "->>>>>>>>>9",  0, 	0,       
                "Number of blocks allocated",      ""},
{2,"_DbStatus-EmptyBlks",INTFLD,  SCVFD_DBSEMPTBLK, 1,   0,  "0",  /* 49 */
                "->>>>>>>>>9",  0, 	0,       
                "Empty blocks",      ""},
{2,"_DbStatus-FreeBlks", INTFLD,  SCVFD_DBSFREEBLK, 1,   0,  "0",  /* 50 */
                "->>>>>>>>>9",  0, 	0,       
                "Free blocks",      ""},
{2,"_DbStatus-RMFreeBlks",INTFLD, SCVFD_DBSRMFREEBLK,1,  0,  "0",  /* 51 */
                "->>>>>>>>>9",  0, 	0,       
                "RM blocks with free space",      ""},
{2,"_DbStatus-LastTran", INTFLD,  SCVFD_DBSLSTTRAN, 1,   0,  "0",  /* 52 */
                "->>>>>>>>>9",  0, 	0,       
                "Last transaction id",      ""},
{2,"_DbStatus-LastTable",INTFLD,  SCVFD_DBSLSTTBL,  1,   0,  "0",  /* 53 */
                "->>>>>>>>>9",  0, 	0,       
                "Highest table number defined",      ""},
{2,"_DbStatus-DbVers",   INTFLD,  SCVFD_DBSDBVERS,  1,   0,  "0",  /* 54 */
                "->>>>>>>>>9",  0, 	0,       
                "Database version number",      ""},
{2,"_DbStatus-DbVersMinor",INTFLD,SCVFD_DBSDBVERSMINOR,1,0,  "0",  /* 55 */
                "->>>>>>>>>9",  0, 	0,       
                "Database minor version number",      ""},
{2,"_DbStatus-ClVersMinor",INTFLD,SCVFD_DBSCLVERSMINOR,1,0,  "0",  /* 56 */
                "->>>>>>>>>9",  0, 	0,       
                "Client minor version number",      ""},
{2,"_DbStatus-ShmVers",  INTFLD,  SCVFD_DBSSHMVERS, 1,   0,  "0",  /* 57 */
                "->>>>>>>>>9",  0, 	0,       
                "Shared memory version number",      ""},
{2,"_DbStatus-Changed",  INTFLD,  SCVFD_DBSCHG,     1,   0,  "0",  /* 58 */
                "->>>>>>>>>9",  0, 	0,       
                "Sequence of last incremental",      ""},
{2,"_DbStatus-ibSeq",    INTFLD,  SCVFD_DBSIBSEQ,   1,   0,  "0",  /* 59 */
                "->>>>>>>>>9",  0, 	0,       
                "Sequence of last incremental",      ""},
{2,"_DbStatus-Integrity",CHRFLD,  SCVFD_DBSINTEGRITY,1,  0,  "?",  /* 60 */
                "x(12)",  0, 	0,       "",      
                "Integrity flags"},
{2,"_DbStatus-HiWater",  INTFLD,  SCVFD_DBSHIWATER, 1,   0,  "0",  /* 61 */
                "->>>>>>>>>9",  0, 	0,       
                "Database blocks high water mark",      ""},
{2,"_DbStatus-BiBlkSize",INTFLD,  SCVFD_DBSBIBLKSZ, 1,   0,  "0",  /* 62 */
                "->>>>>>>>>9",  0, 	0,       
                "Before image block size (bytes)",      ""},
{2,"_DbStatus-BiClSize", INTFLD,  SCVFD_DBSBICLSZ,  1,   0,  "0",  /* 63 */
                "->>>>>>>>>9",  0, 	0,       
                "Before image cluster size (kb)",      ""},
{2,"_DbStatus-AiBlkSize",INTFLD,  SCVFD_DBSAIBLKSZ, 1,   0,  "0",  /* 64 */
                "->>>>>>>>>9",  0, 	0,       
                "After image block size (bytes)",      ""},
{2,"_DbStatus-starttime",CHRFLD,  SCVFD_DBSSTIME,   1,   0,  "?",  /* 65 */
                "x(24)",        0, 	0,       
                "Database was started at",      ""},
{2,"_DbStatus-LastOpen", CHRFLD,  SCVFD_DBSLSTOPN,  1,   0,  "?",  /* 66 */
                "x(24)",        0, 	0,       
                "Most recent database open",      ""},
{2,"_DbStatus-PrevOpen", CHRFLD,  SCVFD_DBSPREVOPN, 1,   0,  "?",  /* 67 */
                "x(24)",        0, 	0,       
                "Previous database open",      ""},
{2,"_DbStatus-CacheStamp",CHRFLD,  SCVFD_DBSCSTAMP, 1,   0,  "?",  /* 68 */
                "x(24)",        0, 	0,       
                "Local cache file time stamp",      ""},
{2,"_DbStatus-fbDate",   CHRFLD,  SCVFD_DBSFBDATE,  1,   0,  "?",  /* 69 */
                "x(24)",        0, 	0,       
                "Most recent full backup",      ""},
{2,"_DbStatus-ibDate",   CHRFLD,  SCVFD_DBSIBDATE,  1,   0,  "?",  /* 70 */
                "x(24)",        0, 	0,       
                "Most recent incremental backup",      ""},
{2,"_DbStatus-CreateDate",CHRFLD, SCVFD_DBSCRDATE,  1,   0,  "?",  /* 71 */
                "x(24)",        0, 	0,       
                "Database created (multi-volume)",      ""},
{2,"_DbStatus-BiOpen",   CHRFLD,  SCVFD_DBSBIOPEN,  1,   0,  "?",  /* 72 */
                "x(24)",        0, 	0,       
                "Most recent .bi file open",      ""},
{2,"_DbStatus-BiTrunc",  CHRFLD,  SCVFD_DBSBITRUNC, 1,   0,  "?",  /* 73 */
                "x(24)",        0, 	0,       
                "Time since last truncate bi",      ""},
{2,"_DbStatus-Codepage", CHRFLD,  SCVFD_DBSCPAGE,   1,   0,  "?",  /* 74 */
                "x(20)",        0, 	0,       
                "Database Character Set",      ""},
{2,"_DbStatus-Collation",CHRFLD,  SCVFD_DBSCOLL,    1,   0,  "?",  /* 75 */
                "x(20)",        0, 	0,       
                "Database Collation Name",      ""},
{2,"_DbStatus-NumAreas", INTFLD,  SCVFD_DBSANUMARES,1,   0,  "0",  /* 76 */
                "->>>>>>>>>9",  0, 	0,       
                "Number of areas",      ""},
{2,"_DbStatus-NumLocks", INTFLD,  SCVFD_DBSANUMLCKS,1,   0,  "0",  /* 77 */
                "->>>>>>>>>9",  0, 	0,       
                "Lock table entries in use",      ""},
{2,"_DbStatus-MostLocks",INTFLD,  SCVFD_DBSAMOSTLCKS,1,  0,  "0",  /* 78 */
                "->>>>>>>>>9",  0, 	0,       
                "Lock table high water mark",      ""},
{2,"_DbStatus-SharedMemVer",INTFLD,SCVFD_DBSASHRVERS,1,   0,  "0",  /* 79 */
                "->>>>>>>>>9",  0, 	0,       
                "Shared Memory Version #",      ""},
{2,"_DbStatus-NumSems",  INTFLD,  SCVFD_DBSANUMSEMS,1,   0,  "0",  /* 80 */
                "->>>>>>>>>9",  0, 	0,       
                "Number of semaphores used",      ""},
{2,"_DbStatus-BiSize",   INTFLD,  SCVFD_DBSBISIZE,  1,   0,  "0",  /* 81 */
                "->>>>>>>>>9",  0, 	0,       
                "Logical .bi file size",      ""},
{2,"_DbStatus-Misc",     INTFLD,  SCVFD_DBSAMISC,   1,   0,  "0",  /* 82 */
                "->>>>>>>>>9",  0, 	0,       
                "Misc",      ""},


/* VST Buffer Status Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{3,"_BfStatus-Id",       INTFLD,  SCVFD_BFSID,      1,   1,  "0",  /* 83 */
                "->>>>>>>>>9",  0, 	0,       
                "Buffer id",      ""},
{3,"_BfStatus-TotBufs",  INTFLD,  SCVFD_BFSTOT,     1,   0,  "0",  /* 84 */
                "->>>>>>>>>9",  0, 	0,       
                "Total buffers",      ""},
{3,"_BfStatus-HashSize", INTFLD,  SCVFD_BFSHASH,    1,   0,  "0",  /* 85 */
                "->>>>>>>>>9",  0, 	0,       
                "Hash table size",      ""},
{3,"_BfStatus-UsedBuffs",INTFLD,  SCVFD_BFSUSED,    1,   0,  "0",  /* 86 */
                "->>>>>>>>>9",  0, 	0,       
                "Used buffers",      ""},
{3,"_BfStatus-LRU",      INTFLD,  SCVFD_BFSLRU,     1,   0,  "0",  /* 87 */
                "->>>>>>>>>9",  0, 	0,       
                "On lru chain",      ""},
{3,"_BfStatus-APWQ",     INTFLD,  SCVFD_BFSAPWQ,    1,   0,  "0",  /* 88 */
                "->>>>>>>>>9",  0, 	0,       
                "On apw queu",      ""},
{3,"_BfStatus-CKPQ",     INTFLD,  SCVFD_BFSCKPQ,    1,   0,  "0",  /* 89 */
                "->>>>>>>>>9",  0, 	0,       
                "On ckp queue",      ""},
{3,"_BfStatus-ModBuffs", INTFLD,  SCVFD_BFSMOD,     1,   0,  "0",  /* 90 */
                "->>>>>>>>>9",  0, 	0,       
                "Modified buffers",      ""},
{3,"_BfStatus-CKPMarked",INTFLD,  SCVFD_BFSCKPM,    1,   0,  "0",  /* 91 */
                "->>>>>>>>>9",  0, 	0,       
                "Marked for ckp",      ""},
{3,"_BfStatus-LastCkpNum",INTFLD, SCVFD_BFSLSTCKP,  1,   0,  "0",  /* 92 */
                "->>>>>>>>>9",  0, 	0,       
                "Last checkpoint number",      ""},
{3,"_BfStatus-Misc",     INTFLD,  SCVFD_BFSMISC,    1,   0,  "0",  /* 93 */
                "->>>>>>>>>9",  0, 	0,       
                "Misc",      ""},

/* VST Logging Status Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{4 ,"_Logging-Id",        INTFLD,  SCVFD_LOGID,      1,   1,  "0", /* 94 */
                "->>>>>>>>>9",  0, 	0,       
                "Logging Id",      ""},
{4,"_Logging-CrashProt", INTFLD,  SCVFD_LOGCRPROT,  1,   0,  "0",  /* 95 */
                "->>>>>>>>>9",  0, 	0,       
		"Crash protection",      ""},
{4,"_Logging-CommitDelay",INTFLD, SCVFD_LOGCOMMD,   1,   0,  "0",  /* 96 */
                "->>>>>>>>>9",  0, 	0,       
		"Delayed Commit",      ""},
{4,"_Logging-BiIO",      CHRFLD,  SCVFD_LOGBIIO,    1,   0,  "?",  /* 97 */
                "x(15)",        0, 	0,       
		"Before-image I/O",      ""},
{4,"_Logging-BiClAge",   INTFLD,  SCVFD_LOGBICLAGE, 1,   0,  "0",  /* 98 */
                "->>>>>>>>>9",  0, 	0,       
		"Before-image cluster age time",      ""},
{4,"_Logging-BiBlkSize", INTFLD,  SCVFD_LOGBIBLKSZ, 1,   0,  "0",  /* 99 */
                "->>>>>>>>>9",  0, 	0,       
		"Before-image block size",      ""},
{4,"_Logging-BiClSize",  INTFLD,  SCVFD_LOGBICLSZ,  1,   0,  "0",  /* 100 */
                "->>>>>>>>>9",  0, 	0,       
		"Before-image cluster size",      ""},
{4,"_Logging-BiExtents", INTFLD,  SCVFD_LOGBIEXTS,  1,   0,  "0",  /* 101 */
                "->>>>>>>>>9",  0, 	0,       
		"Number of before-image extents",      ""},
{4,"_Logging-BiLogSize", INTFLD,  SCVFD_LOGBILOGSZ, 1,   0,  "0",  /* 102 */
                "->>>>>>>>>9",  0, 	0,       
		"Before-image log size (kb)",      ""},
{4,"_Logging-BiBytesFree",INTFLD, SCVFD_LOGBIFREE,  1,   0,  "0",  /* 103 */
                "->>>>>>>>>9",  0, 	0,       
		"Bytes free in current cluster",      ""},
{4,"_Logging-LastCkp",   CHRFLD,  SCVFD_LOGLASTCKP, 1,   0,  "?",  /* 104 */
                "x(24)",        0, 	0,       
		"Last checkpoint was at",      ""},
{4,"_Logging-BiBuffs",   INTFLD,  SCVFD_LOGBIBUFFS, 1,   0,  "0",  /* 105 */
                "->>>>>>>>>9",  0, 	0,       
		"Number of BI buffers",      ""},
{4,"_Logging-BiFullBuffs",INTFLD, SCVFD_LOGBUFULL,  1,   0,  "0",  /* 106 */
                "->>>>>>>>>9",  0, 	0,       
		"Full buffers",      ""},
{4,"_Logging-AiJournal", CHRFLD,  SCVFD_LOGAIJ,     1,   0,  "?",  /* 107 */
                "x(3)",         0, 	0,       
		"After-image journalling",      ""},
{4,"_Logging-AiIO",      CHRFLD,  SCVFD_LOGAIIO,    1,   0,  "?",  /* 108 */
                "x(15)",        0, 	0,       
		"After-image I/O",      ""},
{4,"_Logging-AiBegin",   CHRFLD,  SCVFD_LOGAIBEGIN, 1,   0,  "?",  /* 109 */
                "x(24)",        0, 	0,       
		"After-image begin date",      ""},
{4,"_Logging-AiNew",     CHRFLD,  SCVFD_LOGAINEW,   1,   0,  "?",  /* 110 */
                "x(24)",        0, 	0,       
		"After-image new date",      ""},
{4,"_Logging-AiOpen",    CHRFLD,  SCVFD_LOGAIOPEN,  1,   0,  "?",  /* 111 */
                "x(24)",        0, 	0,       
		"After-image open date",      ""},
{4,"_Logging-AiGenNum",  INTFLD,  SCVFD_LOGAIGENNUM,1,   0,  "0",  /* 112 */
                "->>>>>>>>>9",  0, 	0,       
		"After-image generation number",      ""},
{4,"_Logging-AiExtents", INTFLD,  SCVFD_LOGAIEXTS,  1,   0,  "0",  /* 113 */
                "->>>>>>>>>9",  0, 	0,       
		"Number of after-image extents",      ""},
{4,"_Logging-AiCurrExt", INTFLD,  SCVFD_LOGAICURREXT,1,  0,  "0",  /* 114 */
                "->>>>>>>>>9",  0, 	0,       
		"Current after-image extent",      ""},
{4,"_Logging-AiBuffs",   INTFLD,  SCVFD_LOGAIBUFFS, 1,   0,  "0",  /* 115 */
                "->>>>>>>>>9",  0, 	0,       
		"Number of AI buffers",      ""},
{4,"_Logging-AiBlkSize", INTFLD,  SCVFD_LOGAIBLKSZ, 1,   0,  "0",  /* 116 */
                "->>>>>>>>>9",  0, 	0,       
		"After-image block size",      ""},
{4,"_Logging-AiLogSize", INTFLD,  SCVFD_LOGAILOGSZ, 1,   0,  "0",  /* 117 */
                "->>>>>>>>>9",  0, 	0,       
		"After-image log size",      ""},
{4,"_Logging-2PC",       CHRFLD,  SCVFD_LOG2PC,     1,   0,  "?",  /* 118 */
                "x(3)",         0, 	0,       
		"Two Phase Commit",      ""},
{4,"_Logging-2PCNickName",CHRFLD, SCVFD_LOG2PCNAME, 1,   0,  "?",  /* 119 */
                "x(24)",        0, 	0,       
		"Coordinator nickname",      ""},
{4,"_Logging-2PCPriority",INTFLD, SCVFD_LOG2PCPR,   1,   0,  "0",  /* 120 */
                "->>>>>>>>>9",  0, 	0,       
		"Coordinator priority",      ""},
{4,"_Logging-Misc",      INTFLD, SCVFD_LOGMISC,    1,   0,  "0",  /* 121 */
                "->>>>>>>>>9",  0, 	0,       
		"Misc",      ""},

/* VST Shared Memory Segments Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{5,"_Segment-Id",        INTFLD,  SCVFD_SEGID,      1,   1,  "0",  /* 122 */
                "->>>>>>>>>9",  0, 	0,       
                "Segment Number",      ""},
{5,"_Segment-SegId",     INTFLD,  SCVFD_SEGSEGID,   1,   0,  "0",  /* 123 */
                "->>>>>>>>>9",  0, 	0,       
                "Segment Id",      ""},
{5,"_Segment-SegSize",   INTFLD,  SCVFD_SEGSEGSZ,   1,   0,  "0",  /* 124 */
                "->>>>>>>>>9",  0, 	0,       
                "Segment Size",      ""},
{5,"_Segment-BytesUsed", INTFLD,  SCVFD_SEGUSED,    1,   0,  "0",  /* 125 */
                "->>>>>>>>>9",  0, 	0,       
                "Used",      ""},
{5,"_Segment-ByteFree",  INTFLD,  SCVFD_SEGFREE,    1,   0,  "0",  /* 126 */
                "->>>>>>>>>9",  0, 	0,       
                "Free",      ""},
{5,"_Segment-Misc",      INTFLD,  SCVFD_SEGMISC,    1,   0,  "0",  /* 127 */
                "->>>>>>>>>9",  0, 	0,       
                "Misc",      ""},

/* VST DB Server Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{6,"_Server-Id",         INTFLD,  SCVFD_SRVID,      1,   1,  "0",  /* 128 */
                "->>>>>>>>>9",  0, 	0,       
                "",      ""},
{6,"_Server-Num",        INTFLD,  SCVFD_SRVNUM,     1,   0,  "0",  /* 129 */
                "->>>>>>>>>9",  0, 	0,       
                "Server Number",      ""},
{6,"_Server-Pid",        INTFLD,  SCVFD_SRVPID,     1,   0,  "0",  /* 130 */
                "->>>>>>>>>9",  0, 	0,       
                "Server Pid",      ""},
{6,"_Server-Type",       CHRFLD,  SCVFD_SRVTYPE,    1,   0,  "?",  /* 131 */
                "x(8)",         0, 	0,       
                "Server Type",      ""},
{6,"_Server-Protocol",   CHRFLD,  SCVFD_SRVPROTOCOL,1,   0,  "?",  /* 132 */
                "x(20)",        0, 	0,       
                "Server Protocol",      ""},
{6,"_Server-Logins",     INTFLD,  SCVFD_SRVLOGINS,  1,   0,  "0",  /* 133 */
                "->>>>>>>>>9",  0, 	0,       
                "Logins",      ""},
{6,"_Server-CurrUsers",  INTFLD,  SCVFD_SRVCURRUSRS,1,   0,  "0",  /* 134 */
                "->>>>>>>>>9",  0, 	0,       
                "Current Users",      ""},
{6,"_Server-MaxUsers",   INTFLD,  SCVFD_SRVMAXUSRS, 1,   0,  "0",  /* 135 */
                "->>>>>>>>>9",  0, 	0,       
                "Max Users",      ""},
{6,"_Server-PortNum",    INTFLD,  SCVFD_SRVPORTNUM, 1,   0,  "0",  /* 136 */
                "->>>>>>>>>9",  0, 	0,       
                "Server Port Number",      ""},
{6,"_Server-Misc",       INTFLD,  SCVFD_SRVMISC,    1,   0,  "0",  /* 137 */
                "->>>>>>>>>9",  0, 	0,       
                "Misc",      ""},


/* VST DB Startup Opts Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{7,"_Startup-Id",        INTFLD,  SCVFD_STRTID,     1,   1,  "0",  /* 138 */
                "->>>>>>>>>9",  0, 	0,       
                "",      ""},
{7,"_Startup-AiName",    CHRFLD,  SCVFD_STRTAINAME, 1,   0,  "?",  /* 139 */
                "x(24)",        0, 	0,       
                "After-image file name (-a)",      ""},
{7,"_Startup-Buffs",     INTFLD,  SCVFD_STRTBUFFS,  1,   0,  "0",  /* 140 */
                "->>>>>>>>>9",        0, 	0,       
                "Number of database buffers (-B)",      ""},
{7,"_Startup-AiBuffs",   INTFLD,  SCVFD_STRTAIBUFFS,1,   0,  "0",  /* 141 */
                "->>>>>>>>>9",        0, 	0,       
                "Number of after image buffers (-aibufs)",      ""},
{7,"_Startup-BiBuffs",   INTFLD,  SCVFD_STRTBIBUFFS,1,   0,  "0",  /* 142 */
                "->>>>>>>>>9",        0, 	0,       
                "Number of before image buffers (-bibufs)",      ""},
{7,"_Startup-BiName",    CHRFLD,  SCVFD_STRTBINAME, 1,   0,  "?",  /* 143 */
                "x(24)",              0, 	0,       
                "Before-image file name (-g)",      ""},
{7,"_Startup-BiTrunc",   INTFLD,  SCVFD_STRTBITRUNC,1,   0,  "0",  /* 144 */
                "->>>>>>>>>9",        0, 	0,       
                "Before-image truncate interval (-G)",      ""},
{7,"_Startup-CrashProt", INTFLD,  SCVFD_STRTCRPROT, 1,   0,  "0",  /* 145 */
                "->>>>>>>>>9",        0, 	0,       
                "No crash protection (-i)",      ""},
{7,"_Startup-LockTable", INTFLD,  SCVFD_STRTLOCKS,  1,   0,  "0",  /* 146 */
                "->>>>>>>>>9",        0, 	0,       
                "Current size of locking table (-L)",      ""},
{7,"_Startup-MaxClients",INTFLD,  SCVFD_STRTMAXCLTS,1,   0,  "0",  /* 147 */
                "->>>>>>>>>9",        0, 	0,       
                "Maximum number of clients per server (-Ma)",      ""},
{7,"_Startup-BiDelay",   INTFLD,  SCVFD_STRTBIDELAY,1,   0,  "0",  /* 148 */
                "->>>>>>>>>9",        0, 	0,       
                "Delay of before-image flush (-Mf)",      ""},
{7,"_Startup-MaxServers",INTFLD,  SCVFD_STRTMAXSRVS,1,   0,  "0",  /* 149 */
                "->>>>>>>>>9",        0, 	0,       
                "Maximum number of servers (-Mn)",      ""},
{7,"_Startup-MaxUsers",  INTFLD,  SCVFD_STRTMAXUSERS,1,  0,  "0",  /* 150 */
                "->>>>>>>>>9",        0, 	0,       
                "Maximum number of users (-n)",      ""},
{7,"_Startup-BiIO",      INTFLD,  SCVFD_STRTBIIO,    1,   0,  "0",  /* 151 */
                "->>>>>>>>>9",        0, 	0,       
                "Before-image file I/O (-r -R)",      ""},
{7,"_Startup-APWQTime",  INTFLD,  SCVFD_STRTAPWQ,    1,   0,  "0",  /* 152 */
                "->>>>>>>>>9",        0, 	0,       
                "APW queue check time",      ""},
{7,"_Startup-APWSTime",  INTFLD,  SCVFD_STRTAPWS,    1,   0,  "0",  /* 153 */
                "->>>>>>>>>9",        0, 	0,       
                "APW scan time",      ""},
{7,"_Startup-APWBuffs",  INTFLD,  SCVFD_STRTAPWBUFFS,1,   0,  "0",  /* 154 */
                "->>>>>>>>>9",        0, 	0,       
                "APW buffers to scan",      ""},
{7,"_Startup-APWMaxWrites",INTFLD,SCVFD_STRTAPWW,    1,   0,  "0",  /* 155 */
                "->>>>>>>>>9",        0, 	0,       
                "APW max writes / scan",      ""},
{7,"_Startup-Spin",      INTFLD,  SCVFD_STRTSPIN,    1,   0,  "0",  /* 156 */
                "->>>>>>>>>9",        0, 	0,       
                "Spinlock tries before timeout",      ""},
{7,"_Startup-Directio", INTFLD,  SCVFD_STRTDIRECTIO, 1,   0,  "0",  /* 157 */
                "->>>>>>>>>9",        0,        0,
                "Started with -directio option",      ""},
{7,"_Startup-Misc",      INTFLD,  SCVFD_STRTMISC,    1,   0,  "0",  /* 158 */
                "->>>>>>>>>9",        0, 	0,       
                "Misc",      ""},

/* VST DB File list Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{8,"_FileList-Id",       INTFLD,  SCVFD_FILEID,      1,   1,  "0",  /* 159 */
                "->>>>>>>>>9",  0, 	0,       
                "File Id",      ""},
{8,"_FileList-Size",     INTFLD,  SCVFD_FILESZ,      1,   0,  "0",  /* 160 */
                "->>>>>>>>>9",  0, 	0,       
                "File Size",      ""},
{8,"_FileList-Extend",   INTFLD,  SCVFD_FILEEXTEND,  1,   0,  "0",  /* 161 */
                "->>>>>>>>>9",  0, 	0,       
                "File Extend",      ""},
{8,"_FileList-LogicalSz",INTFLD,  SCVFD_FILELSIZE,   1,   0,  "0",  /* 162 */
                "->>>>>>>>>9",  0, 	0,       
                "Logical file size",      ""},
{8,"_FileList-BlkSize",  INTFLD,  SCVFD_FILEBLKSIZE, 1,   0,  "0",  /* 163 */
                "->>>>>>>>>9",  0, 	0,       
                "Block Size",      ""},
{8,"_FileList-Openmode", CHRFLD,  SCVFD_FILEOPENMODE,1,   0,  "?",  /* 164 */
                "x(5)",  0, 	0,       
                "Open Mode",      ""},
{8,"_FileList-Name",     CHRFLD,  SCVFD_FILENAME,    1,   0,  "?",  /* 165 */
                "x(50)",        0, 	0,       
                "Filename",      ""},
{8,"_FileList-Misc",    INTFLD,   SCVFD_FILEMISC,    1,   0,  "0",  /* 166 */
                "->>>>>>>>>9",  0, 	0,       
                "Misc",      ""},

/* VST DB User Lock Table Entries */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{9,"_UserLock-Id",      INTFLD,   SCVFD_USERLOCKID,  1,   1,  "0",  /* 167 */
                "->>>>>>>>>9",  0, 	0,       
                "",      ""},
{9,"_UserLock-Usr",     INTFLD,   SCVFD_USERLOCKUSR, 1,   0,  "0",  /* 168 */
                "->>>>>>>>>9",  0, 	0,       
                "Usr",      ""},
{9,"_UserLock-Name",    CHRFLD,   SCVFD_USERLOCKNAME,1,   0,  "?",  /* 169 */
                "x(10)",         0, 	0,       
                "Name",      ""},
{9,"_UserLock-Recid",   INTFLD,   SCVFD_USERLOCKRECID,1,  0,  "0",  /* 170 */
                "->>>>>>>>>9",  0, 	USERLOCKARRAYMAX,       
                "Recid",      ""},
{9,"_UserLock-Type",    CHRFLD,   SCVFD_USERLOCKTYPE,1,   0,  "?",  /* 171 */
                "x(10)",         0, 	USERLOCKARRAYMAX,       
                "Type",      ""},
{9,"_UserLock-Chain",   INTFLD,   SCVFD_USERLOCKCHAIN,1,  0,  "0",  /* 172 */
                "->>>>>>>>>9",  0, 	USERLOCKARRAYMAX,       
                "Chain",      ""},
{9,"_UserLock-Flags",   CHRFLD,   SCVFD_USERLOCKFLAG,1,   0,  "?",  /* 173 */
                "x(10)",        0, 	USERLOCKARRAYMAX,       
                "Flags",      ""},
{9,"_UserLock-Misc",    INTFLD,   SCVFD_USERLOCKMISC,1,   0,  "0",  /* 174 */
                "->>>>>>>>>9",  0, 	0,       
                "Misc",      ""},

/* VST DB Activity Summary Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{10,"_Summary-Id",      INTFLD,  SCVFD_SUMID,        1,   1,  "0",  /* 175 */
                "->>>>>>>>>9",  0, 	0,       
                "Activity Summary Id",      ""},
{10,"_Summary-Commits", INTFLD,  SCVFD_SUMCOMMIT,    1,   0,  "0",  /* 176 */
                "->>>>>>>>>9",  0, 	0,       
                "Commits",      ""},
{10,"_Summary-Undos",   INTFLD,  SCVFD_SUMUNDO,      1,   0,  "0",  /* 177 */
                "->>>>>>>>>9",  0, 	0,       
                "Undos",      ""},
{10,"_Summary-RecReads",INTFLD,  SCVFD_SUMRECREAD,   1,   0,  "0",  /* 178 */
                "->>>>>>>>>9",  0, 	0,       
                "Record Reads",      ""},
{10,"_Summary-RecUpd",  INTFLD,  SCVFD_SUMRECUPD,    1,   0,  "0",  /* 179 */
                "->>>>>>>>>9",  0, 	0,       
                "Record Updates",      ""},
{10,"_Summary-RecCreat",INTFLD,  SCVFD_SUMRECCREAT,  1,   0,  "0",  /* 180 */
                "->>>>>>>>>9",  0, 	0,       
                "Record Creates",      ""},
{10,"_Summary-RecDel",  INTFLD,  SCVFD_SUMRECDEL,    1,   0,  "0",  /* 181 */
                "->>>>>>>>>9",  0, 	0,       
                "Record Deletes",      ""},
{10,"_Summary-RecLock", INTFLD,  SCVFD_SUMRECLOCK,   1,   0,  "0",  /* 182 */
                "->>>>>>>>>9",  0, 	0,       
                "Record Locks",      ""},
{10,"_Summary-RecWait", INTFLD,  SCVFD_SUMRECWAIT,   1,   0,  "0",  /* 183 */
                "->>>>>>>>>9",  0, 	0,       
                "Record Waits",      ""},
{10,"_Summary-DbReads", INTFLD,  SCVFD_SUMDBREAD,    1,   0,  "0",  /* 184 */
                "->>>>>>>>>9",  0, 	0,       
                "DB Reads",      ""},
{10,"_Summary-DbWrites",INTFLD,  SCVFD_SUMDBWRITE,   1,   0,  "0",  /* 185 */
                "->>>>>>>>>9",  0, 	0,       
                "DB Writes",      ""},
{10,"_Summary-BiReads", INTFLD,  SCVFD_SUMBIREAD,    1,   0,  "0",  /* 186 */
                "->>>>>>>>>9",  0, 	0,       
                "BI Reads",      ""},
{10,"_Summary-BiWrites",INTFLD,  SCVFD_SUMBIWRITE,   1,   0,  "0",  /* 187 */
                "->>>>>>>>>9",  0, 	0,       
                "BI Writes",      ""},
{10,"_Summary-AiWrites",INTFLD,  SCVFD_SUMAIWRITE,   1,   0,  "0",  /* 188 */
                "->>>>>>>>>9",  0, 	0,       
                "AI Writes",      ""},
{10,"_Summary-Chkpts",  INTFLD,  SCVFD_SUMCHKPTS,    1,   0,  "0",  /* 189 */
                "->>>>>>>>>9",  0, 	0,       
                "Checkpoints",      ""},
{10,"_Summary-Flushed", INTFLD,  SCVFD_SUMFLUSHED,   1,   0,  "0",  /* 190 */
                "->>>>>>>>>9",  0, 	0,       
                "Flushed at chkpt",      ""},
{10,"_Summary-TransComm",INTFLD, SCVFD_SUMTRCOMMIT,  1,   0,  "0",  /* 191 */
                ">>>>>>>>>>9",        0, 	0,       
                "Transactions",      ""},
{10,"_Summary-UpTime",  INTFLD,  SCVFD_SUMUPTIME,    1,   0,  "0",  /* 192 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{10,"_Summary-DbAccesses",INTFLD,SCVFD_SUMDBACCESS,  1,   0,  "0",  /* 193 */
                "->>>>>>>>>9",  0, 	0,       
                "Db Acceses",      ""},
{10,"_Summary-Misc",    INTFLD,  SCVFD_SUMMISC,      1,   0,  "0",  /* 194 */
                "->>>>>>>>>9",  0, 	0,       
                "Misc",      ""},

/* VST DB Activity Summary Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{11, "_Server-Id",    INTFLD,  SCVFD_ACTSRVID,     1,   1,  "0",  /* 195 */
                "->>>>>>>>>9",  0,      0,
                "Server Id",      ""},
{11, "_Server-MsgRec",INTFLD,  SCVFD_ACTSRVMSGR,   1,   0,  "0",  /* 196 */
                "->>>>>>>>>9",  0,      0,
                "Messages received",      ""},
{11, "_Server-MsgSent",INTFLD, SCVFD_ACTSRVMSGS,   1,   0,  "0",  /* 197 */
                "->>>>>>>>>9",  0,      0,
                "Messages sent",      ""},
{11, "_Server-ByteRec",INTFLD, SCVFD_ACTSRVBYTER,  1,   0,  "0",  /* 198 */
                "->>>>>>>>>9",  0,      0,
                "Bytes received",      ""},
{11, "_Server-ByteSent",INTFLD,SCVFD_ACTSRVBYTES,  1,   0,  "0",  /* 199 */
                "->>>>>>>>>9",  0,      0,
                "Bytes sent",      ""},
{11, "_Server-RecRec",INTFLD,  SCVFD_ACTSRVRECR,   1,   0,  "0",  /* 200 */
                "->>>>>>>>>9",  0,      0,
                "Records received",      ""},
{11, "_Server-RecSent",INTFLD, SCVFD_ACTSRVRECS,   1,   0,  "0",  /* 201 */
                "->>>>>>>>>9",  0,      0,
                "Records sent",      ""},
{11, "_Server-QryRec",INTFLD,  SCVFD_ACTSRVQRYR,   1,   0,  "0",  /* 202 */
                "->>>>>>>>>9",  0,      0,
                "Queries received",      ""},
{11, "_Server-TimeSlice",INTFLD,SCVFD_ACTSRVTIMES, 1,   0,  "0",  /* 203 */
                "->>>>>>>>>9",  0,      0,
                "Time slices",      ""},
{11, "_Server-Trans",  INTFLD,  SCVFD_ACTSRVTRANS, 1,   0,  "0",  /* 204 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{11, "_Server-UpTime",  INTFLD,  SCVFD_ACTSRVUPTIME,1,  0,  "0",  /* 205 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{11, "_Server-Misc",  INTFLD,  SCVFD_ACTSRVMISC,   1,   0,  "0",  /* 206 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},


/* VST DB Activity Summary Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{12, "_Buffer-Id",      INTFLD,  SCVFD_ACTBUFID,     1,   1,  "0",  /* 207 */
                "->>>>>>>>>9",  0,      0,
                "Buffer Id",      ""},
{12, "_Buffer-LogicRds",INTFLD,  SCVFD_ACTBUFLREADS, 1,   0,  "0",  /* 208 */
                "->>>>>>>>>9",  0,      0,
                "Logical reads",      ""},
{12, "_Buffer-LogicWrts",INTFLD, SCVFD_ACTBUFLWRITES,1,   0,  "0",  /* 209 */
                "->>>>>>>>>9",  0,      0,
                "Logical writes",      ""},
{12, "_Buffer-OSRds",   INTFLD,  SCVFD_ACTBUFOSRDS,  1,   0,  "0",  /* 210 */
                "->>>>>>>>>9",  0,      0,
                "O/S reads",      ""},
{12, "_Buffer-OSWrts",  INTFLD,  SCVFD_ACTBUFOSWRTS, 1,   0,  "0",  /* 211 */
                "->>>>>>>>>9",  0,      0,
                "O/S writes",      ""},
{12, "_Buffer-Chkpts",  INTFLD,  SCVFD_ACTBUFCHKPTS, 1,   0,  "0",  /* 212 */
                "->>>>>>>>>9",  0,      0,
                "Checkpoints",      ""},
{12, "_Buffer-Marked",  INTFLD,  SCVFD_ACTBUFMARKED, 1,   0,  "0",  /* 213 */
                "->>>>>>>>>9",  0,      0,
                "Marked to checkpoint",      ""},
{12, "_Buffer-Flushed", INTFLD,  SCVFD_ACTBUFFLUSHED,1,   0,  "0",  /* 214 */
                "->>>>>>>>>9",  0,      0,
                "Flushed at checkpoint",      ""},
{12, "_Buffer-Deferred",INTFLD,  SCVFD_ACTBUFDEFFRD, 1,   0,  "0",  /* 215 */
                "->>>>>>>>>9",  0,      0,
                "Writes deferred",      ""},
{12, "_Buffer-LRUSkips",INTFLD,  SCVFD_ACTBUFLRUSKIPS,1,  0,  "0",  /* 216 */
                "->>>>>>>>>9",  0,      0,
                "LRU skips",      ""},
{12, "_Buffer-LRUWrts", INTFLD,  SCVFD_ACTBUFLRUWRTS,1,   0,  "0",  /* 217 */
                "->>>>>>>>>9",  0,      0,
                "LRU writes",      ""},
{12, "_Buffer-APWEnq",  INTFLD,  SCVFD_ACTBUFAPWENQ, 1,   0,  "0",  /* 218 */
                "->>>>>>>>>9",  0,      0,
                "APW enqueues",      ""},
{12, "_Buffer-Trans",   INTFLD,  SCVFD_ACTBUFTRANS,  1,   0,  "0",  /* 219 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{12, "_Buffer-UpTime",  INTFLD,  SCVFD_ACTBUFUPTIME, 1,   0,  "0",  /* 220 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{12, "_Buffer-Misc",    INTFLD,  SCVFD_ACTBUFMISC,   1,   0,  "0",  /* 221 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},


/* VST DB Activity Summary Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{13, "_PW-Id",          INTFLD,  SCVFD_ACTPWSID,     1,   1,  "0",  /* 222 */
                "->>>>>>>>>9",  0,      0,
                "Page Writer Id",      ""},
{13, "_PW-TotDBWrites", INTFLD,  SCVFD_ACTPWSTDBW,   1,   0,  "0",  /* 223 */
                "->>>>>>>>>9",  0,      0,
                "Total DB writes",      ""},
{13, "_PW-DBWrites",    INTFLD,  SCVFD_ACTPWSDBW,    1,   0,  "0",  /* 224 */
                "->>>>>>>>>9",  0,      0,
                "APW DB writes",      ""},
{13, "_PW-ScanWrites",  INTFLD,  SCVFD_ACTPWSSW,     1,   0,  "0",  /* 225 */
                "->>>>>>>>>9",  0,      0,
                "    scan writes",      ""},
{13, "_PW-ApwQWrites",  INTFLD,  SCVFD_ACTPWSQW,     1,   0,  "0",  /* 226 */
                "->>>>>>>>>9",  0,      0,
                "    apw queue writes",      ""},
{13, "_PW-CkpQWrites",  INTFLD,  SCVFD_ACTPWSCQW,    1,   0,  "0",  /* 227 */
                "->>>>>>>>>9",  0,      0,
                "    ckp queue writes",      ""},
{13, "_PW-ScanCycles",  INTFLD,  SCVFD_ACTPWSSC,     1,   0,  "0",  /* 228 */
                "->>>>>>>>>9",  0,      0,
                "    scan cycles",      ""},
{13, "_PW-BuffsScaned", INTFLD,  SCVFD_ACTPWSBS,     1,   0,  "0",  /* 229 */
                "->>>>>>>>>9",  0,      0,
                "    buffers scanned",      ""},
{13, "_PW-BufsCkp",     INTFLD,  SCVFD_ACTPWSBC,     1,   0,  "0",  /* 230 */
                "->>>>>>>>>9",  0,      0,
                "    bufs checkpointed",      ""},
{13, "_PW-Checkpoints", INTFLD,  SCVFD_ACTPWSCHKP,   1,   0,  "0",  /* 231 */
                "->>>>>>>>>9",  0,      0,
                "Checkpoints",      ""},
{13, "_PW-Marked",      INTFLD,  SCVFD_ACTPWSMARKD,  1,   0,  "0",  /* 232 */
                "->>>>>>>>>9",  0,      0,
                "Marked at checkpoint",      ""},
{13, "_PW-Flushed",     INTFLD,  SCVFD_ACTPWSFLUSHD, 1,   0,  "0",  /* 233 */
                "->>>>>>>>>9",  0,      0,
                "Flushed at checkpoint",      ""},
{13, "_PW-Trans",       INTFLD,  SCVFD_ACTPWSTRANS,  1,   0,  "0",  /* 234 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{13, "_PW-UpTime",      INTFLD,  SCVFD_ACTPWSUPTIME, 1,   0,  "0",  /* 235 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs) (in secs)",      ""},
{13, "_PW-Misc",        INTFLD,  SCVFD_ACTPWSMISC,   1,   0,  "0",  /* 236 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},


/* VST DB Activity Summary Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{14, "_BiLog-Id",       INTFLD,  SCVFD_ACTBIID,     1,   1,  "0",  /* 237 */
                "->>>>>>>>>9",  0,      0,
                "Logging Id",      ""},
{14, "_BiLog-TotalWrts",INTFLD,  SCVFD_ACTBITOTW,  1,   0,  "0",  /* 238 */
                "->>>>>>>>>9",  0,      0,
                "Total BI writes",      ""},
{14, "_BiLog-BIWWrites",INTFLD,  SCVFD_ACTBIW,     1,   0,  "0",  /* 239 */
                "->>>>>>>>>9",  0,      0,
                "BIW BI writes",      ""},
{14, "_BiLog-RecWriten",INTFLD,  SCVFD_ACTBIRECW,  1,   0,  "0",  /* 240 */
                "->>>>>>>>>9",  0,      0,
                "Records written",      ""},
{14, "_BiLog-BytesWrtn",INTFLD,  SCVFD_ACTBIBYTEW, 1,   0,  "0",  /* 241 */
                "->>>>>>>>>9",  0,      0,
                "Bytes written",      ""},
{14, "_BiLog-TotReads", INTFLD,  SCVFD_ACTBITOTR,  1,   0,  "0",  /* 242 */
                "->>>>>>>>>9",  0,      0,
                "Total BI Reads",      ""},
{14, "_BiLog-RecRead",  INTFLD,  SCVFD_ACTBIRECR,  1,   0,  "0",  /* 243 */
                "->>>>>>>>>9",  0,      0,
                "Records read",      ""},
{14, "_BiLog-BytesRead",INTFLD,  SCVFD_ACTBIBYTER, 1,   0,  "0",  /* 244 */
                "->>>>>>>>>9",  0,      0,
                "Bytes read",      ""},
{14, "_BiLog-ClstrClose",INTFLD, SCVFD_ACTBICLCL,  1,   0,  "0",  /* 245 */
                "->>>>>>>>>9",  0,      0,
                "Clusters closed",      ""},
{14, "_BiLog-BBuffWaits",INTFLD, SCVFD_ACTBIBBUFFW,1,   0,  "0",  /* 246 */
                "->>>>>>>>>9",  0,      0,
                "Busy buffer waits",      ""},
{14, "_BiLog-EBuffWaits",INTFLD, SCVFD_ACTBIEBUFFW,1,   0,  "0",  /* 247 */
                "->>>>>>>>>9",  0,      0,
                "Empty buffer waits",      ""},
{14, "_BiLog-ForceWaits",INTFLD, SCVFD_ACTFWAITS,  1,   0,  "0",  /* 248 */
                "->>>>>>>>>9",  0,      0,
                "Log force waits",      ""},
{14, "_BiLog-ForceWrts",INTFLD,  SCVFD_ACTFWRTS,   1,   0,  "0",  /* 249 */
                "->>>>>>>>>9",  0,      0,
                "Log force writes",      ""},
{14, "_BiLog-PartialWrts",INTFLD,SCVFD_ACTBIPWRTS, 1,   0,  "0",  /* 250 */
                "->>>>>>>>>9",  0,      0,
                "Partial writes",      ""},
{14, "_BiLog-Trans",    INTFLD,  SCVFD_ACTBITRANS, 1,   0,  "0",  /* 251 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{14, "_BiLog-UpTime",   INTFLD,  SCVFD_ACTBIUPTIME,1,   0,  "0",  /* 252 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{14, "_BiLog-Misc",     INTFLD,  SCVFD_ACTBIMISC,  1,   0,  "0",  /* 253 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},

/* VST DB Activity Summary Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{15, "_AiLog-Id",       INTFLD,  SCVFD_ACTAIID,  1,   1,  "0",  /* 254 */
                "->>>>>>>>>9",  0,      0,
                "AI Logging Id",      ""},
{15, "_AiLog-TotWrites",INTFLD,  SCVFD_ACTAITOTW,1,   0,  "0",  /* 255 */
                "->>>>>>>>>9",  0,      0,
                "Total AI writes",      ""},
{15, "_AiLog-AIWWrites",INTFLD,  SCVFD_ACTAIAIW, 1,   0,  "0",  /* 256 */
                "->>>>>>>>>9",  0,      0,
                "AIW AI writes",      ""},
{15, "_AiLog-RecWriten",INTFLD,  SCVFD_ACTAIRECW,1,   0,  "0",  /* 257 */
                "->>>>>>>>>9",  0,      0,
                "Records written",      ""},
{15, "_AiLog-BytesWritn",INTFLD, SCVFD_ACTAIBYTESW,1, 0,  "0",  /* 258 */
                "->>>>>>>>>9",  0,      0,
                "Bytes written",      ""},
{15, "_AiLog-BBuffWaits",INTFLD, SCVFD_ACTAIBWAIT,1,  0,  "0",  /* 259 */
                "->>>>>>>>>9",  0,      0,
                "Busy buffer waits",      ""},
{15, "_AiLog-NoBufAvail",INTFLD, SCVFD_ACTAINOBUF,1,  0,  "0",  /* 260 */
                "->>>>>>>>>9",  0,      0,
                "Buffer not avail",      ""},
{15, "_AiLog-PartialWrt",INTFLD, SCVFD_ACTAIPW,  1,   0,  "0",  /* 261 */
                "->>>>>>>>>9",  0,      0,
                "Partial writes",      ""},
{15, "_AiLog-ForceWaits",INTFLD, SCVFD_ACTAIFORCEW,1, 0,  "0",  /* 262 */
                "->>>>>>>>>9",  0,      0,
                "Log force waits",      ""},
{15, "_AiLog-Trans",    INTFLD,  SCVFD_ACTAITRANS, 1, 0,  "0",  /* 263 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{15, "_AiLog-UpTime",   INTFLD,  SCVFD_ACTAIUPTIME,1, 0,  "0",  /* 264 */
                ">>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{15, "_AiLog-Misc",      INTFLD, SCVFD_ACTAIMISC,1,   0,  "0",  /* 265 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},


/* VST DB Lock Activity Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{16, "_Lock-Id",        INTFLD,  SCVFD_ACTLCKID,     1,   1,  "0",  /* 266 */
                "->>>>>>>>>9",  0,      0,
                "Lock Id",      ""},
{16, "_Lock-ShrReq",    INTFLD,  SCVFD_ACTLCKSHRR,   1,   0,  "0",  /* 267 */
                "->>>>>>>>>9",  0,      0,
                "Share requests",      ""},
{16, "_Lock-ExclReq",   INTFLD,  SCVFD_ACTLCKEXCLR,  1,   0,  "0",  /* 268 */
                "->>>>>>>>>9",  0,      0,
                "Exclusive requests",      ""},
{16, "_Lock-UpgReq",    INTFLD,  SCVFD_ACTLCKUPGR,   1,   0,  "0",  /* 269 */
                "->>>>>>>>>9",  0,      0,
                "Upgrade requests",      ""},
{16, "_Lock-RecGetReq", INTFLD,  SCVFD_ACTLCKRECGETR,1,   0,  "0",  /* 270 */
                "->>>>>>>>>9",  0,      0,
                "Rec Get requests",      ""},
{16, "_Lock-ShrLock",   INTFLD,  SCVFD_ACTLCKSHRL,   1,   0,  "0",  /* 271 */
                "->>>>>>>>>9",  0,      0,
                "Share grants",      ""},
{16, "_Lock-ExclLock",  INTFLD,  SCVFD_ACTLCKEXCLL,  1,   0,  "0",  /* 272 */
                "->>>>>>>>>9",  0,      0,
                "Exclusive grants",      ""},
{16, "_Lock-UpgLock",   INTFLD,  SCVFD_ACTLCKUPGL,   1,   0,  "0",  /* 273 */
                "->>>>>>>>>9",  0,      0,
                "Upgrade grants",      ""},
{16, "_Lock-RecGetLock",INTFLD,  SCVFD_ACTLCKRECGETL,1,   0,  "0",  /* 274 */
                "->>>>>>>>>9",  0,      0,
                "Rec Get grants",      ""},
{16, "_Lock-ShrWait",   INTFLD,  SCVFD_ACTLCKSHRW,   1,   0,  "0",  /* 275 */
                "->>>>>>>>>9",  0,      0,
                "Share waits",      ""},
{16, "_Lock-ExclWait",  INTFLD,  SCVFD_ACTLCKEXCLW,  1,   0,  "0",  /* 276 */
                "->>>>>>>>>9",  0,      0,
                "Exclusive waits",      ""},
{16, "_Lock-UpgWait",   INTFLD,  SCVFD_ACTLCKUPGW,   1,   0,  "0",  /* 277 */
                "->>>>>>>>>9",  0,      0,
                "Upgrade waits",      ""},
{16, "_Lock-RecGetWait",INTFLD,  SCVFD_ACTLCKRECGETW,1,   0,  "0",  /* 278 */
                "->>>>>>>>>9",  0,      0,
                "Rec Get waits",      ""},
{16, "_Lock-CanclReq",  INTFLD,  SCVFD_ACTLCKCANCLREQ,1,  0,  "0",  /* 279 */
                "->>>>>>>>>9",  0,      0,
                "Requests cancelled",      ""},
{16, "_Lock-Downgrade", INTFLD,  SCVFD_ACTLCKDOWNGR, 1,   0,  "0",  /* 280 */
                "->>>>>>>>>9",  0,      0,
                "Downgrades",      ""},
{16, "_Lock-RedReq",    INTFLD,  SCVFD_ACTLCKREDREQ, 1,   0,  "0",  /* 281 */
                "->>>>>>>>>9",  0,      0,
                "Redundant requests",      ""},
{16, "_Lock-Trans",     INTFLD,  SCVFD_ACTLCKTRANS,  1,   0,  "0",  /* 282 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{16, "_Lock-UpTime",    INTFLD,  SCVFD_ACTLCKUPTIME, 1,   0,  "0",  /* 283 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{16, "_Lock-Misc",      INTFLD,  SCVFD_ACTLCKMISC,   1,   0,  "0",  /* 284 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},
{16, "_Lock-ExclFind",  INTFLD,  SCVFD_ACTLCKEXCFIND,   1,   0,  "0", /* 285 */
                "->>>>>>>>>9",  0,      0,
                "ExclFind",      ""},
{16, "_Lock-ShrFind",   INTFLD,  SCVFD_ACTLCKSHRFIND,   1,   0,  "0",  /* 286 */
                "->>>>>>>>>9",  0,      0,
                "ShrFind",      ""},


/* VST DB IO Activity by Type Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{17, "_IOType-Id",      INTFLD,  SCVFD_ACTTYPEID,    1,   1,  "0",  /* 287 */
                "->>>>>>>>>9",  0,      0,
                "IO by Type Id",      ""},
{17, "_IOType-IdxRds",  INTFLD,  SCVFD_ACTTYPEIDXR,  1,   0,  "0",  /* 288 */
                "->>>>>>>>>9",  0,      0,
                "DB Index block Reads",      ""},
{17, "_IOType-DataReads",INTFLD, SCVFD_ACTTYPEDATAR, 1,   0,  "0",  /* 289 */
                "->>>>>>>>>9",  0,      0,
                "DB Data block Reads",      ""},
{17, "_IOType-BiRds",   INTFLD,  SCVFD_ACTTYPEBIR,   1,   0,  "0",  /* 290 */
                "->>>>>>>>>9",  0,      0,
                "BI reads",      ""},
{17, "_IOType-AiRds",   INTFLD,  SCVFD_ACTTYPEAIR,   1,   0,  "0",  /* 291 */
                "->>>>>>>>>9",  0,      0,
                "AI reads",      ""},
{17, "_IOType-IdxWrts", INTFLD,  SCVFD_ACTTYPEIDXW,  1,   0,  "0",  /* 292 */
                "->>>>>>>>>9",  0,      0,
                "DB Index block Writes",      ""},
{17, "_IOType-DataWrts",INTFLD,  SCVFD_ACTTYPEDATAW, 1,   0,  "0",  /* 293 */
                "->>>>>>>>>9",  0,      0,
                "DB Data block Writes",      ""},
{17, "_IOType-BiWrts",  INTFLD,  SCVFD_ACTTYPEBIW,   1,   0,  "0",  /* 294 */
                "->>>>>>>>>9",  0,      0,
                "BI writes",      ""},
{17, "_IOType-AiWrts",  INTFLD,  SCVFD_ACTTYPEAIW,   1,   0,  "0",  /* 295 */
                "->>>>>>>>>9",  0,      0,
                "AI writes",      ""},
{17, "_IOType-Trans",   INTFLD,  SCVFD_ACTTYPETRANS, 1,   0,  "0",  /* 296 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{17, "_IOType-UpTime",  INTFLD,  SCVFD_ACTTYPEUPTIME,1,   0,  "0",  /* 297 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{17, "_IOType-Misc",    INTFLD,  SCVFD_ACTTYPEMISC,  1,   0,  "0",  /* 298 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},

/* VST DB IO Activity by File Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{18, "_IOFile-Id",      INTFLD,  SCVFD_ACTFILEID,    1,   1,  "0",  /* 299 */
                "->>>>>>>>>9",  0,      0,
                "IO by File Id",      ""},
{18, "_IOFile-FileName",CHRFLD,  SCVFD_ACTFILENAME,  1,   0,  "?",  /* 300 */
                "x(50)",  0,      0,
                "File Name",      ""},
{18, "_IOFile-Reads",   INTFLD,  SCVFD_ACTFILEREADS, 1,   0,  "0",  /* 301 */
                "->>>>>>>>>9",  0,      0,
                "Reads",      ""},
{18, "_IOFile-Writes",  INTFLD,  SCVFD_ACTFILEWRITES,1,   0,  "0",  /* 302 */
                "->>>>>>>>>9",  0,      0,
                "Writes",      ""},
{18, "_IOFile-Extends", INTFLD,  SCVFD_ACTFILEEXTENDS,1,  0,  "0",  /* 303 */
                "->>>>>>>>>9",  0,      0,
                "Extends",      ""},
{18, "_IOFile-Trans",   INTFLD,  SCVFD_ACTFILETRANS, 1,   0,  "0",  /* 304 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{18, "_IOFile-BufWrites",INTFLD, SCVFD_ACTFILEBWRITE,1,   0,  "0",  /* 305 */
                "->>>>>>>>>9",  0,      0,
                "Buffered Writes",      ""},
{18, "_IOFile-UbufWrites",INTFLD,SCVFD_ACTFILEUBWRITE,1,  0,  "0",  /* 306 */
                "->>>>>>>>>9",  0,      0,
                "Unbuffered Writes",      ""},
{18, "_IOFile-BufReads", INTFLD, SCVFD_ACTFILEBREAD, 1,   0,  "0",  /* 307 */
                "->>>>>>>>>9",  0,      0,
                "Buffered Reads",      ""},
{18, "_IOFile-UbufReads",INTFLD, SCVFD_ACTFILEUBREADS,1,  0,  "0",  /* 308 */
                "->>>>>>>>>9",  0,      0,
                "Unbuffered Reads",      ""},
{18, "_IOFile-UpTime",  INTFLD,  SCVFD_ACTFILEUPTIME,1,   0,  "0",  /* 309 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{18, "_IOFile-Misc",    INTFLD,  SCVFD_ACTFILEMISC,  1,   0,  "0",  /* 310 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},

/* VST DB Space Activity Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{19, "_Space-Id",       INTFLD,  SCVFD_ACTSPACEID,   1,   1,  "0",  /* 311 */
                "->>>>>>>>>9",  0,      0,
                "Space Id",      ""},
{19, "_Space-DbExd",    INTFLD,  SCVFD_ACTSPACEDBEXD,1,   0,  "0",  /* 312 */
                "->>>>>>>>>9",  0,      0,
                "Database extends",      ""},
{19, "_Space-TakeFree", INTFLD,  SCVFD_ACTSPACETAKEF,1,   0,  "0",  /* 313 */
                "->>>>>>>>>9",  0,      0,
                "Take free block",      ""},
{19, "_Space-RetFree",  INTFLD,  SCVFD_ACTSPACERETF, 1,   0,  "0",  /* 314 */
                "->>>>>>>>>9",  0,      0,
                "Return free block",      ""},
{19, "_Space-AllocNewRm",INTFLD, SCVFD_ACTSPACENEWRM,1,   0,  "0",  /* 315 */
                "->>>>>>>>>9",  0,      0,
                "Alloc rm space",      ""},
{19, "_Space-FromRm",   INTFLD,  SCVFD_ACTSPACEFROMRM,1,  0,  "0",  /* 316 */
                "->>>>>>>>>9",  0,      0,
                "Alloc from rm",      ""},
{19, "_Space-FromFree", INTFLD,  SCVFD_ACTSPACEFROMFR,1,  0,  "0",  /* 317 */
                "->>>>>>>>>9",  0,      0,
                "Alloc from free",      ""},
{19, "_Space-BytesAlloc",INTFLD, SCVFD_ACTSPACEBYTES,1,   0,  "0",  /* 318 */
                "->>>>>>>>>9",  0,      0,
                "Bytes allocated",      ""},
{19, "_Space-Examined", INTFLD,  SCVFD_ACTSPACEEXAMINE,1, 0,  "0",  /* 319 */
                "->>>>>>>>>9",  0,      0,
                "rm blocks examined",      ""},
{19, "_Space-Removed",  INTFLD,  SCVFD_ACTSPACEREMOVED,1, 0,  "0",  /* 320 */
                "->>>>>>>>>9",  0,      0,
                "Remove from rm",      ""},
{19, "_Space-FrontAdd", INTFLD,  SCVFD_ACTSPACEFRNTADD,1, 0,  "0",  /* 321 */
                "->>>>>>>>>9",  0,      0,
                "Add to rm, front",      ""},
{19, "_Space-BackAdd",  INTFLD,  SCVFD_ACTSPACEBCKADD,1,  0,  "0",  /* 322 */
                "->>>>>>>>>9",  0,      0,
                "Add to rm, back",      ""},
{19, "_Space-Front2Back",INTFLD, SCVFD_ACTSPACEFR2BCK,1,  0,  "0",  /* 323 */
                "->>>>>>>>>9",  0,      0,
                "Move rm front to back",      ""},
{19, "_Space-Locked",   INTFLD,  SCVFD_ACTSPACELOCKD,1,   0,  "0",  /* 324 */
                "->>>>>>>>>9",  0,      0,
                "Remove locked rm entry",      ""},
{19, "_Space-Trans",    INTFLD,  SCVFD_ACTSPACETRANS,1,   0,  "0",  /* 325 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{19, "_Space-UpTime",   INTFLD,  SCVFD_ACTSPACEUPTIME,1,  0,  "0",  /* 326 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{19, "_Space-Misc",     INTFLD,  SCVFD_ACTSPACEMISC, 1,   0,  "0",  /* 327 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},

/* VST DB Index Activity Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{20, "_Index-Id",       INTFLD,  SCVFD_ACTIDXID,     1,   1,  "0",  /* 328 */
                "->>>>>>>>>9",  0,      0,
                "Index Id",      ""},
{20, "_Index-Find",     INTFLD,  SCVFD_ACTIDXFND,    1,   0,  "0",  /* 329 */
                "->>>>>>>>>9",  0,      0,
                "Find index entry",      ""},
{20, "_Index-Create",   INTFLD,  SCVFD_ACTIDXCREAT,  1,   0,  "0",  /* 330 */
                "->>>>>>>>>9",  0,      0,
                "Create index entry",      ""},
{20, "_Index-Delete",   INTFLD,  SCVFD_ACTIDXDEL,    1,   0,  "0",  /* 331 */
                "->>>>>>>>>9",  0,      0,
                "Delete index entry",      ""},
{20, "_Index-Remove",   INTFLD,  SCVFD_ACTIDXREM,    1,   0,  "0",  /* 332 */
                "->>>>>>>>>9",  0,      0,
                "Remove locked entry",      ""},
{20, "_Index-Splits",   INTFLD,  SCVFD_ACTIDXSPLIT,  1,   0,  "0",  /* 333 */
                "->>>>>>>>>9",  0,      0,
                "Split block",      ""},
{20, "_Index-Free",     INTFLD,  SCVFD_ACTIDXFREE,   1,   0,  "0",  /* 334 */
                "->>>>>>>>>9",  0,      0,
                "Free block",      ""},
{20, "_Index-Trans",    INTFLD,  SCVFD_ACTIDXTRANS,  1,   0,  "0",  /* 335 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{20, "_Index-UpTime",   INTFLD,  SCVFD_ACTIDXUPTIME, 1,   0,  "0",  /* 336 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{20, "_Index-Misc",     INTFLD,  SCVFD_ACTIDXMISC,   1,   0,  "0",  /* 337 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},

/* VST DB Record Activity Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{21, "_Record-Id",      INTFLD,  SCVFD_ACTRECID,     1,   1,  "0",  /* 338 */
                "->>>>>>>>>9",  0,      0,
                "Record Id",      ""},
{21, "_Record-RecRead", INTFLD,  SCVFD_ACTRECRREAD,  1,   0,  "0",  /* 339 */
                "->>>>>>>>>9",  0,      0,
                "Read record",      ""},
{21, "_Record-RecUpd",  INTFLD,  SCVFD_ACTRECRUPDD,  1,   0,  "0",  /* 340 */
                "->>>>>>>>>9",  0,      0,
                "Update record",      ""},
{21, "_Record-RecCreat",INTFLD,  SCVFD_ACTRECRCREAT, 1,   0,  "0",  /* 341 */
                "->>>>>>>>>9",  0,      0,
                "Create record",      ""},
{21, "_Record-RecDel",  INTFLD,  SCVFD_ACTRECRDEL,   1,   0,  "0",  /* 342 */
                "->>>>>>>>>9",  0,      0,
                "Delete record",      ""},
{21, "_Record-FragRead",INTFLD,  SCVFD_ACTRECFREAD,  1,   0,  "0",  /* 343 */
                "->>>>>>>>>9",  0,      0,
                "Fragments read",      ""},
{21, "_Record-FragCreat",INTFLD, SCVFD_ACTRECFCREAT, 1,   0,  "0",  /* 344 */
                "->>>>>>>>>9",  0,      0,
                "Fragments created",      ""},
{21, "_Record-FragDel",  INTFLD,  SCVFD_ACTRECFDEL,  1,   0,  "0",  /* 345 */
                "->>>>>>>>>9",  0,      0,
                "Fragments deleted",      ""},
{21, "_Record-FragUpd",  INTFLD,  SCVFD_ACTRECFUPD,  1,   0,  "0",  /* 346 */
                "->>>>>>>>>9",  0,      0,
                "Fragments updated",      ""},
{21, "_Record-BytesRead",INTFLD,  SCVFD_ACTRECBREAD, 1,   0,  "0",  /* 347 */
                "->>>>>>>>>9",  0,      0,
                "Bytes read",      ""},
{21, "_Record-BytesCreat",INTFLD, SCVFD_ACTRECBCREAT,1,   0,  "0",  /* 348 */
                "->>>>>>>>>9",  0,      0,
                "Bytes created",      ""},
{21, "_Record-BytesDel", INTFLD,  SCVFD_ACTRECBDEL,  1,   0,  "0",  /* 349 */
                "->>>>>>>>>9",  0,      0,
                "Bytes deleted",      ""},
{21, "_Record-BytesUpd", INTFLD,  SCVFD_ACTRECBUPD,  1,   0,  "0",  /* 350 */
                "->>>>>>>>>9",  0,      0,
                "Bytes updated",      ""},
{21, "_Record-Trans",    INTFLD,  SCVFD_ACTRECTRANS, 1,   0,  "0",  /* 351 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{21, "_Record-UpTime",   INTFLD,  SCVFD_ACTRECUPTIME,1,   0,  "0",  /* 352 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{21, "_Record-Misc",     INTFLD,  SCVFD_ACTRECMISC,  1,   0,  "0",  /* 353 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},

/* VST DB Other Activity Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{22, "_Other-Id",       INTFLD,  SCVFD_ACTOTHERID,   1,   1,  "0",  /* 354 */
                "->>>>>>>>>9",  0,      0,
                "Other Id",      ""},
{22, "_Other-Commit",   INTFLD,  SCVFD_ACTOTHERCOMMIT,1,  0,  "0",  /* 355 */
                "->>>>>>>>>9",  0,      0,
                "Commit",      ""},
{22, "_Other-Undo",     INTFLD,  SCVFD_ACTOTHERUNDO, 1,   0,  "0",  /* 356 */
                "->>>>>>>>>9",  0,      0,
                "Undo",      ""},
{22, "_Other-Wait",     INTFLD,  SCVFD_ACTOTHERWAIT, 1,   0,  "0",  /* 357 */
                "->>>>>>>>>9",  0,      0,
                "Wait on semaphore",      ""},
{22, "_Other-FlushMblk",INTFLD,  SCVFD_ACTOTHERFLUSH,1,   0,  "0",  /* 358 */
                "->>>>>>>>>9",  0,      0,
                "Flush master block",      ""},
{22, "_Other-Trans",    INTFLD,  SCVFD_ACTOTHERTRANS,1,   0,  "0",  /* 359 */
                "->>>>>>>>>9",  0,      0,
                "Transactions committed",      ""},
{22, "_Other-UpTime",   INTFLD,  SCVFD_ACTOTHERUPTIME,1,  0,  "0",  /* 360 */
                ">>>>>>>>>>9",        0, 	0,       
                "Database Up Time (in secs)",      ""},
{22, "_Other-Misc",     INTFLD,  SCVFD_ACTOTHERMISC, 1,   0,  "0",  /* 361 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},

/* VST DB Blcok Dump Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{23, "_Block-Id",      INTFLD,  SCVFD_BLOCKID,       1,   1,  "0",  /* 362 */
                "->>>>>>>>>9",  0,      0,
                "Block Id",      ""},
{23, "_Block-Dbkey",   INTFLD,  SCVFD_BLOCKDBKEY,    1,   0,  "0",  /* 363 */
                "->>>>>>>>>9",  0,      0,
                "DBKEY",      ""},
{23, "_Block-Type",    CHRFLD,  SCVFD_BLOCKTYPE,     1,   0,  "?",  /* 364 */
                "x(10)",        0,      0,
                "Type",      ""},
{23, "_Block-ChainType",CHRFLD, SCVFD_BLOCKCHAIN,    1,   0,  "?",  /* 365 */
                "x(10)",        0,      0,
                "Chain Type",      ""},
{23, "_Block-BkupCtr", INTFLD,  SCVFD_BLOCKBKUP,     1,   0,  "0",  /* 366 */
                "->>>>>>>>>9",  0,      0,
                "Backup Counter",      ""},
{23, "_Block-NextDbkey",INTFLD, SCVFD_BLOCKNEXT,     1,   0,  "0",  /* 367 */
                "->>>>>>>>>9",  0,      0,
                "Next Dbkey in Block",      ""},
{23, "_Block-Update",  INTFLD,  SCVFD_BLOCKUPDATE,   1,   0,  "0",  /* 368 */
                "->>>>>>>>>9",  0,      0,
                "Block Update Counter",      ""},
{23, "_Block-Area",    INTFLD,  SCVFD_BLOCKAREA,     1,   0,  "0",  /* 369 */
                "->>>>>>>>>9",  0,      0,
                "Area Number",      ""},
{23, "_Block-Block",   CHRFLD,  SCVFD_BLOCKBLOCK,    1,   0,  "?",  /* 370 */
                "x(1024)",  0,      0,
                "Block...",      ""},
{23, "_Block-Misc",    INTFLD,  SCVFD_BLOCKMISC,     1,   0,  "0",  /* 371 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},


/* VST DB IO Operation by User Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{24, "_UserIO-Id",      INTFLD,  SCVFD_USERIOID,     1,   1,  "0",  /* 372 */
                "->>>>>>>>>9",  0,      0,
                "UserIO Id",      ""},
{24, "_UserIO-Usr",     INTFLD,  SCVFD_USERIOUSR,    1,   0,  "0",  /* 373 */
                "->>>>>>>>>9",  0,      0,
                "Usr",      ""},
{24, "_UserIO-Name",    CHRFLD,  SCVFD_USERIONAME,   1,   0,  "?",  /* 374 */
                "x(10)",        0,      0,
                "Name",      ""},
{24, "_UserIO-DbAccess",INTFLD,  SCVFD_USERIODBACC,  1,   0,  "0",  /* 375 */
                "->>>>>>>>>9",  0,      0,
                "DB Access",      ""},
{24, "_UserIO-DbRead",  INTFLD,  SCVFD_USERIODBREAD, 1,   0,  "0",  /* 376 */
                "->>>>>>>>>9",  0,      0,
                "DB Read",      ""},
{24, "_UserIO-DbWrite", INTFLD,  SCVFD_USERIODBWRITE,1,   0,  "0",  /* 377 */
                "->>>>>>>>>9",  0,      0,
                "DB Write",      ""},
{24, "_UserIO-BiRead",  INTFLD,  SCVFD_USERIOBIREAD, 1,   0,  "0",  /* 378 */
                "->>>>>>>>>9",  0,      0,
                "BI Read",      ""},
{24, "_UserIO-BiWrite", INTFLD,  SCVFD_USERIOBIWRITE,1,   0,  "0",  /* 379 */
                "->>>>>>>>>9",  0,      0,
                "BI Write",      ""},
{24, "_UserIO-AiRead",  INTFLD,  SCVFD_USERIOAIREAD, 1,   0,  "0",  /* 380 */
                "->>>>>>>>>9",  0,      0,
                "AI Read",      ""},
{24, "_UserIO-AiWrite", INTFLD,  SCVFD_USERIOAIWRITE,1,   0,  "0",  /* 381 */
                "->>>>>>>>>9",  0,      0,
                "AI Write",      ""},
{24, "_UserIO-Misc",    INTFLD,  SCVFD_USERIOMISC,   1,   0,  "0",  /* 382 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},


/* VST DB Lock Requests by User Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{25, "_LockReq-Id",     INTFLD,  SCVFD_LOCKREQID,    1,   1,  "0",  /* 383 */
                "->>>>>>>>>9",  0,      0,
                "Lock Request Id",      ""},
{25, "_LockReq-Num",    INTFLD,  SCVFD_LOCKREQNUM,   1,   0,  "0",  /* 384 */
                "->>>>>>>>>9",  0,      0,
                "User Number",      ""},
{25, "_LockReq-Name",   CHRFLD,  SCVFD_LOCKREQNAME,  1,   0,  "?",  /* 385 */
                "x(10)",        0,      0,
                "User Name",      ""},
{25, "_LockReq-RecLock",INTFLD,  SCVFD_LOCKREQRLOCK, 1,   0,  "0",  /* 386 */
                "->>>>>>>>>9",  0,      0,
                "Record Locks",      ""},
{25, "_LockReq-RecWait",INTFLD,  SCVFD_LOCKREQRWAIT, 1,   0,  "0",  /* 387 */
                "->>>>>>>>>9",  0,      0,
                "Record Waits",      ""},
{25, "_LockReq-TrnLock",INTFLD,  SCVFD_LOCKREQTLOCK, 1,   0,  "0",  /* 388 */
                "->>>>>>>>>9",  0,      0,
                "Trans Locks",      ""},
{25, "_LockReq-TrnWait",INTFLD,  SCVFD_LOCKREQTWAIT, 1,   0,  "0",  /* 389 */
                "->>>>>>>>>9",  0,      0,
                "Trans Waits",      ""},
{25, "_LockReq-SchLock",INTFLD,  SCVFD_LOCKREQSLOCK, 1,   0,  "0",  /* 390 */
                "->>>>>>>>>9",  0,      0,
                "Schema Locks",      ""},
{25, "_LockReq-SchWait",INTFLD,  SCVFD_LOCKREQSWAIT, 1,   0,  "0",  /* 391 */
                "->>>>>>>>>9",  0,      0,
                "Schema Waits",      ""},
{25, "_LockReq-Misc",   INTFLD,  SCVFD_LOCKREQMISC,  1,   0,  "0",  /* 392 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},
{25, "_LockReq-ExclFind",INTFLD,  SCVFD_LOCKREQEXCFIND, 1,   0,  "0", /* 393 */
                "->>>>>>>>>9",  0,      0,
                "ExclFind",      ""},
{25, "_LockReq-ShrFind", INTFLD,  SCVFD_LOCKREQSHRFIND, 1,   0,  "0", /* 394 */
                "->>>>>>>>>9",  0,      0,
                "ShrFind",      ""},


/* VST DB Checkpoints Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{26, "_Checkpoint-Id",   INTFLD,  SCVFD_CHKPTID,     1,   1,  "0",  /* 395 */
                "->>>>>>>>>9",  0,      0,
                "Checkpoint Id",      ""},
{26, "_Checkpoint-Time", CHRFLD,  SCVFD_CHKPTTIME,   1,   0,  "0",  /* 396 */
                "x(24)",        0,      0,
                "Time",      ""},
{26, "_Checkpoint-Len",  INTFLD,  SCVFD_CHKPTLEN,    1,   0,  "0",  /* 397 */
                "->>>>>>>>>9",  0,      0,
                "Len",      ""},
{26, "_Checkpoint-Dirty",INTFLD,  SCVFD_CHKPTDIRTY,  1,   0,  "0",  /* 398 */
                "->>>>>>>>>9",  0,      0,
                "Dirty",      ""},
{26, "_Checkpoint-CptQ", INTFLD,  SCVFD_CHKPTCPTQ,   1,   0,  "0",  /* 399 */
                "->>>>>>>>>9",  0,      0,
                "CPT Q",      ""},
{26, "_Checkpoint-Scan", INTFLD,  SCVFD_CHKPTSCAN,   1,   0,  "0",  /* 400 */
                "->>>>>>>>>9",  0,      0,
                "Scan",      ""},
{26, "_Checkpoint-ApwQ", INTFLD,  SCVFD_CHKPTAPWQ,   1,   0,  "0",  /* 401 */
                "->>>>>>>>>9",  0,      0,
                "APW Q",      ""},
{26, "_Checkpoint-Flush",INTFLD,  SCVFD_CHKPTFLUSH,  1,   0,  "0",  /* 402 */
                "->>>>>>>>>9",  0,      0,
                "Flushes",      ""},
{26, "_Checkpoint-Misc", INTFLD,  SCVFD_CHKPTMISC,   1,   0,  "0",  /* 403 */
                "->>>>>>>>>9",  0,      0,
                "Misc",      ""},

/* VST DB Lock Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{27, "_Lock-Id",         INTFLD,  SCVFD_LOCKID,      1,   1,  "0",  /* 404 */
                "->>>>>>>>>9",  0,      0,
                "Lock Id",      ""},
{27, "_Lock-Usr",        INTFLD,  SCVFD_LOCKUSR,     1,   0,  "0",  /* 405 */
                "->>>>>>>>>9",  0,      0,
                "Usr",      ""},
{27, "_Lock-Name",       CHRFLD,  SCVFD_LOCKNAME,    1,   0,  "?",  /* 406 */
                "x(10)",        0,      0,
                "Name",      ""},
{27, "_Lock-Type",       CHRFLD,  SCVFD_LOCKTYPE,    1,   0,  "0",  /* 407 */
                "x(8)",         0,      0,
     "Type",      ""},
{27, "_Lock-Table",      INTFLD,  SCVFD_LOCKTABLE,   1,   0,  "0",  /* 408 */
                "->>>>>>>>>9",  0,      0,
                "Table",      ""},        
{27, "_Lock-RecId",      INTFLD,  SCVFD_LOCKRECID,   1,   0,  "0",  /* 409 */
                "->>>>>>>>>9",  0,      0,
                "RECID",      ""},
{27, "_Lock-Chain",      INTFLD,  SCVFD_LOCKCHAIN,   1,   0,  "0",  /* 410 */
                "->>>>>>>>>9",  0,      0,
                "Chain",      ""},
{27, "_Lock-Flags",      CHRFLD,  SCVFD_LOCKFLAGS,   1,   0,  "?",  /* 411 */
                "x(10)",        0,      0,
                "Flags",      ""},
{27, "_Lock-Misc",       INTFLD,  SCVFD_LOCKMISC,    1,   0,  "0",  /* 412 */
                "->>>>>>>>>9",  0,      0,
                "",      ""},

/* VST DB Transaction Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{28, "_Trans-Id",         INTFLD,  SCVFD_TRANSID,    1,   1,  "0",  /* 413 */
                "->>>>>>>>>9",  0,      0,
                "Trans Id",      ""},
{28, "_Trans-State",      CHRFLD,  SCVFD_TRANSSTATE, 1,   0,  "?",  /* 414 */
                "x(10)",  0,      0,
                "Trans State",      ""},
{28, "_Trans-Flags",      CHRFLD,  SCVFD_TRANSFLAG,  1,   0,  "?",  /* 415 */
                "x(10)",  0,      0,
                "Trans Flags",      ""},
{28, "_Trans-Usrnum",     INTFLD,  SCVFD_TRANSUNUM,  1,   0,  "0",  /* 416 */
                "->>>>>>>>>9",  0,      0,
                "Trans Usernum",      ""},
{28, "_Trans-Num",        INTFLD,  SCVFD_TRANSNUM,   1,   0,  "0",  /* 417 */
                "->>>>>>>>>9",  0,      0,
                "Trans Num",      ""},
{28, "_Trans-counter",    INTFLD,  SCVFD_TRANSCNTR,  1,   0,  "0",  /* 418 */
                "->>>>>>>>>9",  0,      0,
                "Trans rl counter",      ""},
{28, "_Trans-txtime",     CHRFLD,  SCVFD_TRANSTXTIME,1,   0,  "?",  /* 419 */
                "x(24)",  0,      0,
                "Trans Start Time",      ""},
{28, "_Trans-Coord",      CHRFLD,  SCVFD_TRANSCOORD, 1,   0,  "-",  /* 420 */
                "x(10)",  0,      0,
                "Coord Name",      ""},
{28, "_Trans-CoordTx",    INTFLD,  SCVFD_TRANSCOORDTX,1,  0,  "0",  /* 421 */
                "->>>>>>>>>9",  0,      0,
                "Coordinator Transaction",      ""},
{28, "_Trans-Duration",   INTFLD,  SCVFD_TRANSUPTIME,1,  0,  "0",   /* 422 */
                "->>>>>>>>>9",  0,      0,
                "Tx Duration (in secs)",      ""},
{28, "_Trans-Misc",       INTFLD,  SCVFD_TRANSMISC,  1,   0,  "0",  /* 423 */
                "->>>>>>>>>9",  0,      0,
                "Trans Misc",      ""},

/* VST License Table */

/*table name	type		pos                mand sys init
		format		dbkey	extent	 label	  help	*/
{29, "_Lic-Id", INTFLD,         SCVFD_LICID,          1,  1, "0",     /* 424 */
                "->>>>>>>>>9",  0,      0,
                "_Lic-Id",      ""},
{29, "_Lic-ValidUsers",   INTFLD,  SCVFD_LICVALIDUSR,    1,  0, "0",  /* 425 */
                "->>>>>>>>>9",  0,      0,
                "Max Valid Users",      ""},
{29, "_Lic-ActiveConns",  INTFLD,  SCVFD_LICACTIVECON,   1,  0, "0",  /* 426 */
                "->>>>>>>>>9",  0,      0,
                "Active Connections",      ""},
{29, "_Lic-MaxActive",    INTFLD,  SCVFD_LICMAXACTIVE,   1,  0, "0",  /* 427 */
                "->>>>>>>>>9",  0,      0,
                "Max Active Connections",      ""},
{29, "_Lic-MinActive",    INTFLD,  SCVFD_LICMINACTIVE,   1,  0, "0",  /* 428 */
                "->>>>>>>>>9",  0,      0,
                "Min Active Connections",      ""},
{29, "_Lic-BatchConns",   INTFLD,  SCVFD_LICBATCHCON,    1,  0, "0",  /* 429 */
                "->>>>>>>>>9",  0,      0,
                "Batch Connections",      ""},
{29, "_Lic-MaxBatch",     INTFLD,  SCVFD_LICMAXBATCH,    1,  0, "0",  /* 430 */
                "->>>>>>>>>9",  0,      0,
                "Max Batch Connections",      ""},
{29, "_Lic-MinBatch",     INTFLD,  SCVFD_LICMINBATCH,    1,  0, "0",  /* 431 */
                "->>>>>>>>>9",  0,      0,
                "Min Batch Connections",      ""},
{29, "_Lic-CurrConns",    INTFLD,  SCVFD_LICCURRCON,     1,  0, "0",  /* 432 */
                "->>>>>>>>>9",  0,      0,
                "Current Connections",      ""},
{29, "_Lic-MaxCurrent",    INTFLD,  SCVFD_LICMAXCURR,    1,  0, "0",  /* 433 */
                "->>>>>>>>>9",  0,      0,
                "Max Current Connections",      ""},
{29, "_Lic-MinCurrent",    INTFLD,  SCVFD_LICMINCURR,    1,  0, "0",  /* 434 */
                "->>>>>>>>>9",  0,      0,
                "Min Current Connections",      ""},
 
/* VST TableStat Table */
 
/*table name    type            pos                mand sys init
                format          dbkey   extent   label    help  */
{30,"_TableStat-id",   INTFLD,  SCVFD_TABSTATID,   1,   0,  "0",      /* 435 */
                "->>>>>>>>>9",  0,      0,
                "TableStat-id",      ""},
{30,"_TableStat-read",   INTFLD,  SCVFD_TABSTATREAD,   1,   0,  "0",  /* 436 */
                "->>>>>>>>>9",  0,      0,
                "read",      ""},
{30,"_TableStat-update", INTFLD,  SCVFD_TABSTATUPDATE, 1,   0,  "0",  /* 437 */
                "->>>>>>>>>9",  0,      0,
                "update",      ""},
{30,"_TableStat-create",  INTFLD, SCVFD_TABSTATCREATE, 1,   0,  "0",  /* 438 */
                "->>>>>>>>>9",  0,      0,
                "create",      ""},
{30,"_TableStat-delete",  INTFLD, SCVFD_TABSTATDELETE, 1,   0,  "0",  /* 439 */
                "->>>>>>>>>9",  0,      0,
                "delete",      ""},
 
/* VST IndexStat Table */
 
/*table name    type            pos                mand sys init
                format          dbkey   extent   label    help  */
{31,"_IndexStat-id",   INTFLD,  SCVFD_IDXSTATID,   1,   0,  "0",  /* 440 */
                "->>>>>>>>>9",  0,      0,
                "IndexStat-id",      ""},
{31,"_IndexStat-read",   INTFLD,  SCVFD_IDXSTATREAD,   1,   0,  "0",  /* 441 */
                "->>>>>>>>>9",  0,      0,
                "read",      ""},
{31,"_IndexStat-create",  INTFLD, SCVFD_IDXSTATCREATE, 1,   0,  "0",  /* 442 */
                "->>>>>>>>>9",  0,      0,
                "create",      ""},
{31,"_IndexStat-delete",  INTFLD, SCVFD_IDXSTATDELETE, 1,   0,  "0",  /* 443 */
                "->>>>>>>>>9",  0,      0,
                "delete",      ""},
{31,"_IndexStat-split", INTFLD,   SCVFD_IDXSTATSPLIT, 1,   0,  "0",  /* 444 */
                "->>>>>>>>>9",  0,      0,
                "split",      ""},
{31,"_IndexStat-blockdelete", INTFLD, SCVFD_IDXSTATBLKDEL, 1, 0,  "0", /* 445 */
                "->>>>>>>>>9",  0,      0,
                "blockdelete",      ""},
 
/* VST Latch Table */
 
/*table name    type            pos                mand sys init
                format          dbkey   extent   label    help  */
{32, "_Latch-Id", INTFLD,       SCVFD_LATCHID,       1,  1, "0",      /* 446 */
                "->>>>>>>>>9",  0,      0,
                "_Latch-Id",      ""},
{32, "_Latch-Name", CHRFLD,     SCVFD_LATCHNAME,     1,  0, "0",      /* 447 */
                "x(12)",  0,      0,
                "_Latch-Name",      ""},
{32, "_Latch-Hold", INTFLD,     SCVFD_LATCHHOLD,     1,  0, "0",      /* 448 */
                "->>>>>>>>>9",  0,      0,
                "_Latch-Hold",      ""},
{32, "_Latch-Qhold", INTFLD,    SCVFD_LATCHQHOLD,    1,  0, "0",      /* 449 */
                "->>>>>>>>>9",  0,      0,
                "_Latch-Qhold",      ""},
{32, "_Latch-Type", CHRFLD,     SCVFD_LATCHTYPE,     1,  0, "0",      /* 450 */
                "x(12)",  0,      0,
                "_Latch-Type",      ""},
{32, "_Latch-Wait", INTFLD,     SCVFD_LATCHWAIT,     1,  0, "0",      /* 451 */
                "->>>>>>>>>9",  0,      0,
                "_Latch-Wait",      ""},
{32, "_Latch-Lock", INTFLD,     SCVFD_LATCHLOCK,     1,  0, "0",      /* 452 */
                "->>>>>>>>>9",  0,      0,
                "_Latch-Lock",      ""},
{32, "_Latch-Spin", INTFLD,     SCVFD_LATCHSPIN,     1,  0, "0",      /* 453 */
                "->>>>>>>>>9",  0,      0,
                "_Latch-Spin",      ""},
{32, "_Latch-Busy", INTFLD,     SCVFD_LATCHBUSY,     1,  0, "0",      /* 454 */
                "->>>>>>>>>9",  0,      0,
                "_Latch-Busy",      ""},
{32, "_Latch-LockedT", INTFLD,  SCVFD_LATCHLOCKEDT,  1,  0, "0",      /* 455 */
                "->>>>>>>>>9",  0,      0,
                "_Latch-Locked-Time",      ""},
{32, "_Latch-LockT", INTFLD,    SCVFD_LATCHLOCKT,    1,  0, "0",      /* 456 */
                "->>>>>>>>>9",  0,      0,
                "_Latch-Lock-Time",      ""},
{32, "_Latch-WaitT", INTFLD,    SCVFD_LATCHWAITT,    1,  0, "0",      /* 457 */
                "->>>>>>>>>9",  0,      0,
                "_Latch-Wait-Time",      ""},
 
/* VST Resource Table */
 
/*table name    type            pos                mand sys init
                format          dbkey   extent   label    help  */
{33, "_Resrc-Id", INTFLD,       SCVFD_RESRCID,       1,  1, "0",      /* 458 */
                "->>>>>>>>>9",  0,      0,
                "_Resource-Id",      ""},
{33, "_Resrc-Name", CHRFLD,     SCVFD_RESRCNAME,     1,  0, "0",      /* 459 */
                "x(15)",  0,      0,
                "_Resource-Name",      ""},
{33, "_Resrc-lock", INTFLD,     SCVFD_RESRCLOCK,     1,  0, "0",      /* 460 */
                "->>>>>>>>>9",  0,      0,
                "_Resource-Lock",      ""},
{33, "_Resrc-wait", INTFLD,     SCVFD_RESRCWAIT,     1,  0, "0",      /* 461 */
                "->>>>>>>>>9",  0,      0,
                "_Resource-Wait",      ""},
{33, "_Resrc-time", INTFLD,     SCVFD_RESRCWTIME,    1,  0, "0",      /* 462 */
                "->>>>>>>>>9",  0,      0,
                "_Resource-Wait-Time",      ""},

/* VST Txe Table    */
 
/*table name    type            pos                mand sys init
                format          dbkey  extent   label    help  */
{34, "_Txe-Id", INTFLD,         SCVFD_TXEID ,     1,    1, "0",       /* 463 */
                "->>>>>>>>>9",  0,      0,       "",      ""},
{34, "_Txe-Type", CHRFLD,       SCVFD_TXETYPE ,   1,    0, " ",       /* 464 */
                "X(16)",        0,     SCVFD_TXE_ARRAY_SIZE, "",      ""},
{34, "_Txe-Locks",INTFLD,       SCVFD_TXELOCKS ,  1,    0, "0",       /* 465 */
                "->>>>>>>>>9",  0,     SCVFD_TXE_ARRAY_SIZE,"", ""},
{34, "_Txe-Lockss",INTFLD,      SCVFD_TXELOCKSS,  1,    0, "0",       /* 466 */
                "->>>>>>>>>9",  0,    SCVFD_TXE_ARRAY_SIZE, "", ""},
{34, "_Txe-Waits",INTFLD     ,  SCVFD_TXEWAITS,   1,    0, "0",       /* 467 */
                "->>>>>>>>>9",  0,    SCVFD_TXE_ARRAY_SIZE, "", ""},
{34, "_Txe-Waitss",INTFLD    ,  SCVFD_TXEWAITSS,  1,    0, "0",       /* 468 */
                "->>>>>>>>>9",  0,    SCVFD_TXE_ARRAY_SIZE, "", ""},
{34, "_Txe-Time",INTFLD      ,  SCVFD_TXELOCKT ,  1,    0, "0",       /* 469 */
                "->>>>>>>>>9",  0,    SCVFD_TXE_ARRAY_SIZE, "", ""},
{34, "_Txe-Wait-Time",INTFLD ,  SCVFD_TXEWAITT ,  1,    0, "0",       /* 470 */
                "->>>>>>>>>9",  0,    SCVFD_TXE_ARRAY_SIZE,"",  ""},

/* _StatBase fields */
/*table name    type            pos                mand sys init
                format          dbkey   extent   label    help  */
{35,"_StatBase-id", INTFLD,  SCVFD_STATBASID,   1,   1,  "0",      /* 471 */
                "->>>>>>>>>9",  0,      0,
                "StatBase-id",      ""},
{35,"_TableBase",   INTFLD,  SCVFD_STATTABBASE, 1,   0,  "0",      /* 472 */
                "->>>>>>>>>9",  0,      0,
                "_TableBase",      ""},
{35,"_IndexBase",   INTFLD,  SCVFD_STATIDXBASE, 1,   0,  "0",      /* 473 */
                "->>>>>>>>>9",  0,      0,
                "_IndexBase",      ""},

/* VST User Status Table */
/*table name    type            pos                mand sys init
                format          dbkey   extent   label    help  */
{36, "_UserStatus-UserId", INTFLD, SCVFD_USTATUSUSER,  0,  0, "?",     /* 474 */
                "->>>>>>>>>9",  0,      0,    "_UserId",      ""},
{36, "_UserStatus-ObjectId", INTFLD, SCVFD_USTATUSOBJECT,0,0, "?",     /* 475 */
                "->>>>>>>>>9",  0,      0,    "_ObjectId",    ""},
{36, "_UserStatus-ObjectType", INTFLD, SCVFD_USTATUSOBJTYPE,0, 0, "?", /* 476 */
                "->>>>>>>>>9",  0,      0,    "_ObjectType",  ""},
{36, "_UserStatus-State", INTFLD, SCVFD_USTATUSSTATE,  0,  0, "?",     /* 477 */
                "->>>>>>>>>9",  0,      0,    "_Phase",       ""},
{36, "_UserStatus-Counter", INTFLD, SCVFD_USTATUSCTR,  0,  0, "0",     /* 478 */
                "->>>>>>>>>9",  0,      0,    "_Counter",     ""},
{36, "_UserStatus-Target", INTFLD, SCVFD_USTATUSTARGET,0,  0, "?",     /* 479 */
                "->>>>>>>>>9",  0,      0,    "_Target",      ""},
{36, "_UserStatus-Operation", CHRFLD, SCVFD_USTATUSOP, 0,  0, "?",     /* 480 */
                "x(16)",        0,      0,    "_Operation",   ""},

/* VST My Connection Table */
/*table name    type            pos                mand sys init
                format          dbkey   extent   label    help  */
{37, "_MyConn-Id", INTFLD, SCVFD_MYCONNID,         0,    1,  "?",     /* 481 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
{37, "_MyConn-UserId",INTFLD,SCVFD_MYCONNUSERNUM,  0,    0,  "?",     /* 482 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
{37, "_MyConn-Pid", INTFLD, SCVFD_MYCONNPID,       0,    0,  "?",     /* 483 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
{37, "_MyConn-NumSeqBuffers",INTFLD, SCVFD_MYCONNNUMSBUFF,0,0,"?",    /* 484 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
{37, "_MyConn-UsedSeqBuffers",INTFLD, SCVFD_MYCONNUSEDSBUFF,0,0,"?",  /* 485 */
                ">>>>>>>>>9",    0,      0,    "",      ""},

/* VST Area Statistics */
/*table name    type            pos                mand sys init
                format          dbkey   extent   label    help  */
{38, "_AreaStatus-Id", INTFLD, SCVFD_AREASTATID,    0,    1,  "?",    /* 486 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
{38, "_AreaStatus-Areanum", INTFLD, SCVFD_AREASTATNUM,0,   0,"?",     /* 487 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
{38, "_AreaStatus-Areaname", CHRFLD, SCVFD_AREASTATNAME,0,   0,  "?", /* 488 */
                "X(32)",    0,      0,    "",      ""},
{38, "_AreaStatus-Totblocks", INTFLD, SCVFD_AREASTATBLOCKS,0,0,  "?", /* 489 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
{38, "_AreaStatus-Hiwater", INTFLD, SCVFD_AREASTATHIWATER,0, 0,  "?", /* 490 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
{38, "_AreaStatus-Extents", INTFLD, SCVFD_AREASTATEXTENTS,0, 0,  "?", /* 491 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
{38, "_AreaStatus-Lastextent",CHRFLD,SCVFD_AREASTATLASTEXTENT,0,0,"?",/* 492 */
                "X(32)",    0,      0,    "",      ""},
{38, "_AreaStatus-Freenum", INTFLD, SCVFD_AREASTATFREEBLOCKS,0,0,"?", /* 493 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
{38, "_AreaStatus-Rmnum", INTFLD, SCVFD_AREASTATRMBLOCKS,0,  0,  "?", /* 494 */
                ">>>>>>>>>9",    0,      0,    "",      ""},
};

GLOBAL KEYSTAT vstkeystat[] =
/*file name		uniq  prim num-c   idx	    kc-idx */
{ {0,  "_Connect-Id",   1,      1,    1,  SCVSI_CONID,  0},
  {1,  "_MstrBlk-Id",   1,      1,    1,  SCVSI_MBID,   1},
  {2,  "_DbStatus-Id",  1,      1,    1,  SCVSI_DBSID,  2},
  {3,  "_BfStatus-Id",  1,      1,    1,  SCVSI_BFSID,  3},
  {4,  "_Logging-Id",   1,      1,    1,  SCVSI_LOGID,  4},
  {5,  "_Segment-Id",   1,      1,    1,  SCVSI_SEGID,  5},
  {6,  "_Server-Id",    1,      1,    1,  SCVSI_SRVID,  6},
  {7,  "_Startup-Id",   1,      1,    1,  SCVSI_STRTID, 7},
  {8,  "_FileList-Id",  1,      1,    1,  SCVSI_FILEID, 8},
  {9,  "_UserLock-Id",  1,      1,    1,  SCVSI_USERLOCKID,9},
  {10, "_Summary-Id",   1,      1,    1,  SCVSI_SUMID,    10},
  {11, "_Server-Id",    1,      1,    1,  SCVSI_ACTSRVID, 11},
  {12, "_Buffer-Id",    1,      1,    1,  SCVSI_ACTBUFFID,12},
  {13, "_PW-Id",        1,      1,    1,  SCVSI_ACTPWSID, 13},
  {14, "_BiLog-Id",     1,      1,    1,  SCVSI_ACTBIID,  14},
  {15, "_AiLog-Id",     1,      1,    1,  SCVSI_ACTAIID,  15},
  {16, "_Lock-Id",      1,      1,    1,  SCVSI_ACTLCKID, 16},
  {17, "_IOType-Id",    1,      1,    1,  SCVSI_ACTTYPEID,17},
  {18, "_IOFile-Id",    1,      1,    1,  SCVSI_ACTFILEID,18},
  {19, "_Space-Id",     1,      1,    1,  SCVSI_ACTSPACEID,19},
  {20, "_Index-Id",     1,      1,    1,  SCVSI_ACTIDXID, 20},
  {21, "_Record-Id",    1,      1,    1,  SCVSI_ACTRECID, 21},
  {22, "_Other-Id",     1,      1,    1,  SCVSI_ACTOTHERID,22},
  {23, "_Block-Id",     1,      1,    1,  SCVSI_BLOCKID,  23},
  {24, "_UserIO-Id",    1,      1,    1,  SCVSI_USERIOID, 24},
  {25, "_LockReq-Id",   1,      1,    1,  SCVSI_LOCKREQID,25},
  {26, "_Checkpoint-Id",1,      1,    1,  SCVSI_CHKPTID,  26},
  {27, "_Lock-Id",      1,      1,    1,  SCVSI_LOCKID,   27},
  {28, "_Trans-Id",     1,      1,    1,  SCVSI_TRANSID,  28},
  {29, "_License",      1,      1,    1,  SCVSI_LICENSEID,29},
  {30, "_TableStat",    1,      1,    1,  SCVSI_TABID,    30},
  {31, "_IndexStat",    1,      1,    1,  SCVSI_IDXID,    31},
  {32, "_Latch-Id",     1,      1,    1,  SCVSI_LATCHID,  32},
  {33, "_Resrc-Id",     1,      1,    1,  SCVSI_RESRCID,  33},
  {34, "_Txe-id",       1,      1,    1,  SCVSI_TXEID,    34},
  {35, "_StatBase-id",  1,      1,    1,  SCVSI_STATBASID,35},
  {36, "_UserStatus-UserId",1,  1,    1,  SCVSI_USERSTATUSID,36},
  {37, "_MyConnection-Id",1,    1,    1,  SCVSI_MYCONNID, 37},
  {38, "_AreaStatus-Id",1,      1,    1,  SCVSI_AREASTAT, 38},

};

GLOBAL KCSTAT vstkcstat[] =
/* seq      substr  key_file     key_field   */
{{1,        0,     "_Connect",   "_Connect-id"},   /*  0 */
 {1,        0,     "_Mstrblk",   "_Mstrblk-id"},   /*  1 */
 {1,        0,     "_Dbstatus",  "_DbStatus-id"},  /*  2 */
 {1,        0,     "_Buffstatus","_BfStatus-id"},  /*  3 */
 {1,        0,     "_Logging",   "_Logging-id"},   /*  4 */
 {1,        0,     "_Segments",  "_Segment-id"},   /*  5 */
 {1,        0,     "_Servers",   "_Server-id"},    /*  6 */
 {1,        0,     "_Startup",   "_Startup-id"},   /*  7 */
 {1,        0,     "_Filelist",  "_Filelist-id"},  /*  8 */
 {1,        0,     "_Userlock",  "_UserLock-id"},  /*  9 */
 {1,        0,     "_ActSummary","_Summary-id"},   /* 10 */
 {1,        0,     "_Actserver", "_Server-id"},    /* 11 */
 {1,        0,     "_Actbuffer", "_Buffer-id"},    /* 12 */
 {1,        0,     "_Actpws",    "_PW-id"},        /* 13 */
 {1,        0,     "_Actbilog",  "_BiLog-id"},     /* 14 */
 {1,        0,     "_Actailog",  "_AiLog-id"},     /* 15 */
 {1,        0,     "_Actlock",   "_Lock-id"},      /* 16 */
 {1,        0,     "_Actiotype", "_IOType-id"},    /* 17 */
 {1,        0,     "_Actiofile", "_IOFile-id"},    /* 18 */
 {1,        0,     "_Actspace",  "_Space-id"},     /* 19 */
 {1,        0,     "_Actindex",  "_Index-id"},     /* 20 */
 {1,        0,     "_Actrecord", "_Record-id"},    /* 21 */
 {1,        0,     "_Actother",  "_Other-id"},     /* 22 */
 {1,        0,     "_Block",     "_Block-id"},     /* 23 */
 {1,        0,     "_Userio",    "_UserIO-id"},    /* 24 */
 {1,        0,     "_Lockreq",   "_LockReq-id"},   /* 25 */
 {1,        0,     "_Checkpoint","_Checkpoint-id"},/* 26 */
 {1,        0,     "_Lock",      "_Lock-id"},      /* 27 */
 {1,        0,     "_Trans",     "_Trans-id"},     /* 28 */
 {1,        0,     "_License",   "_Lic-id"},       /* 29 */
 {1,        0,     "_TableStat", "_TableStat-id"}, /* 30 */
 {1,        0,     "_IndexStat", "_IndexStat-id"}, /* 31 */
 {1,        0,     "_Latch",     "_Latch-id"},     /* 32 */
 {1,        0,     "_Resrc",     "_Resrc-id"},     /* 33 */
 {1,        0,     "_TxeLock",   "_Txe-id"},       /* 34 */
 {1,        0,     "_StatBase",  "_StatBase-id"},  /* 35 */
 {1,        0,     "_UserStatus","_UserStatus-UserId"},/* 36 */
 {1,        0,     "_MyConnection","_MyConn-Id"},  /* 37 */
 {1,        0,     "_AreaStatus","_AreaStatus-Id"},  /* 38 */

};

#define VSTNUMFLDS  (sizeof(vstfld) / sizeof(FLDSTAT))
#define VSTNUMKEYS  (sizeof(vstkeystat) / sizeof(KEYSTAT))
#define VSTNUMCKEYS (sizeof(vstkcstat) / sizeof(KCSTAT))
 
GLOBAL int vstnumflds =  VSTNUMFLDS;
GLOBAL int vstnumkeys =  VSTNUMKEYS;
GLOBAL int vstnumckeys = VSTNUMCKEYS;
#endif /* INCL_VST_SCHEMA_AND_KEYS */

#define VSTNUMFILS  (sizeof(vstfil) / sizeof(FILSTAT))
 
GLOBAL int vstnumfils =  VSTNUMFILS;
 
#endif  /* #ifndef VSTSTAT_H */
