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

#ifndef DBCONTEXT_H
#define DBCONTEXT_H

/* dbcontext.h - control structures for a database */

#include "shmpub.h"
#include "sempub.h"
#include "latprv.h"
#include "bkcrash.h"

#define RUNMODLEN 32

struct dbshm;
struct omCtl;
struct qtInf;
struct shmemctl;
struct rltlCtl;
struct dbxaTransaction;

#define NOTFORNU struct notfornu

/* dbcontext - The private database control structure.			    */
/*	   Each process which is accessing a database (Server, Single-user, */
/*	   Self-service, Broker, Single-user utilities) has 1 private copy  */
/*	   of this structure.						    */

#include "axppack4.h"
typedef struct dbcontext 
{
	struct  shmemctl  *pmtctl;  /* points to shared-memory lock control */
	struct  dbcontext *pnextdb; /* ptr to next database for same process */
	struct	dbshm     *pdbpub; /* points to DBSHM structure for this db */
	STPOOL	*privpool;	/* private storage pool for this db         */
	SEMDSC	*psemdsc;	/* semaphore descriptor			    */
	SHMDSC	*pshmdsc;	/* shared memory descriptor		    */

	NOTFORNU *psrvctl;/* if this process is SERVER, pts to srvctl */
	USRCTL  *pusrctl;	/* points to current usrctl		    */
	USRCTL  *psvusrctl;	/* points to servers usrctl		    */
	MSTRBLK *pmstrblk;      /* private real ptr to master block         */
	BKCTL   *pbkctl;        /* private real ptr to bkctl                */
	RLCTL   *prlctl;	/* private real ptr to rlctl                */
	AICTL   *paictl;	/* private real ptr to aictl                */
        struct   rltlCtl *ptlctl; /* private real ptr to tlctl              */
	LKCTL   *plkctl;        /* private real ptr to lkctl                */
	USRCTL  *p1usrctl;	/* private real ptr to 1st usrctl           */
	NOTFORNU  *p1srvctl;      /* private real ptr to 1st srvctl           */
	TRANTAB *ptrantab;      /* private real ptr to transaction table    */
struct seqCtl   *pseqctl;       /* private real ptr to seqctl               */
	shmTable_t  *pshsgctl;	/* pointer shared segment control           */
        int      serverId;      /* Id of server if set */

#if OPSYS==WIN32API
        HANDLE   hmutex[32 /*MTL_MAX*/ + 1]; 
   				/* array for mt latch implementation        */
        HANDLE  *phsem;		/* usrctl semaphore table [fossey 12/10/93] */
#endif

	/* misc database control variables */
	TEXT runmode[RUNMODLEN];	/* char string description:	    */
	int	 bklkfd;	/* I have called bklk and it gave me this fd*/
	int	 bklkmode;	/* bklk has locked the db in this mode	    */
	GTBOOL	 fillmsg3;
	GTBOOL	 dbopnmsg;	/* A db opn msg went to .lg file	    */
        GTBOOL    fillmsg1;
        GTBOOL    fillmsg2;

	/* command line arguments which each process accessing a database   */
	/* may specify.  Each process will have one copy of this per db     */
	TEXT	*pserver;	/* -S servername			    */
	TEXT	*phost;		/* -H hostname				    */
	TEXT	*argsnet;	/* -N EXCELAN			init ""	    */
	TEXT	*pdbname;	/* dbname as accessed		    	    */
        TEXT    *pusrdbname;    /* dbname as user typed it in               */
        TEXT    *pusrpfname;    /* -pf pf file name                         */
	TEXT	*ptrigsrc;	/* -trig progresslib or directory	    */
	int	 argprbuf;	/* -O #priv bufs		init -1	    */
	int	 minsv;		/* -Mi min # of servers		init 1      */
	GTBOOL	 argforce;	/* -F					    */
        GTBOOL    confirmforce;  /* Confirmed that we want to force in       */
	GTBOOL	 arg1usr;	/* -1 or single-user module		    */
	GTBOOL	 argz1;		/* to test 2 phase commit (phase1) 	    */
	GTBOOL	 argz2;		/* to test 2 phase commit (phase2)	    */
        GTBOOL    fillarg1;
        GTBOOL    fillarg2;
        GTBOOL    fillarg3;
	UCOUNT	 usertype;	/* BROKER, DBSERVER, SELFSERVE, etc	    */
        COUNT    protovers;     /* netw protocol vrsion eg 12=5,13,14=6,20=7*/
        COUNT    minport;       /* minumum server port number  (-maxport) */
        COUNT    maxport;       /* maximum server port number  (-minport) */
        COUNT    maxsvprot;     /* maximum server per protocol (-Mp)      */
        COUNT    maxsvbrok;     /* maximum server per broker   (-Mpb)     */


	/* misc flags to prevent infinite loops.  Any resource which must   */
	/* be created and later deleted may have a flag here.  When the     */
	/* resource is created, the flag is turned on.  When the 1st attempt*/
	/* to delete the resource is done, the flag is turned off FIRST.    */
	/* The program which deletes the resource should do:		    */
	/*  1. check the flag and return if it off			    */
	/*  2. turn the flag off (thus preventing recursive calls which lead*/
	/*     to infinite loops					    */
	/*  3. delete the resource					    */
	GTBOOL	 brkklflg;	/* ON when logins begin, OFF by brokkill    */
	GTBOOL	 logopen;	/* 1 if .lg file is open		    */
        GTBOOL    filllog1;
        GTBOOL    filllog2;
	int	 lgfd;		/* file descriptor for <dbname>.lg	    */

        int      argzallowdlc;  /* allow procopy to copy to $DLC */
struct  bkfdtbl	*pbkfdtbl;	/* points to tbl of file descriptors	*/
	TEXT	 dbimflgs;	/* see #defines below */
	TEXT	*pusrnm;	/* USERID for this db			*/

	/* Multi-thread control info */
	GTBOOL	 dboutflg;
	GTBOOL	 resyncing;
	GTBOOL	 shmgone;
	GTBOOL	 fillthread1;
	GTBOOL	 fillthread2;
	GTBOOL	 fillthread3;

	int	 inservice;

        /* misc */
	TEXT	*pusernm;
	NOTFORNU *pcondb;
        TEXT    *pdbtype;       /* RMS, PROGRESS etc*/
	TTINY	 dbcode;
	GTBOOL	 privdbpub;	/* ON if we have a private dbpub in pgstpool*/
	GTBOOL	 argnoshm;	/* 1 if not using shm memory & mt/db locks*/
	GTBOOL	 argronly;	/* 1 if the database is a read only one   */
        TEXT    *pmandctl;      /* ptr to shared manditory field arrays */
        TEXT    *pnicknm;       /* 2phase commit nickname */
        GTBOOL    coord;         /* 1 if this db has a coord priority  */
        GTBOOL    fill2pc1;
        GTBOOL    fill2pc2;
        GTBOOL    fill2pc3;
	TEXT	*pCacheFileName;/* points to the local schema cache file name */
struct  trantabrc *ptrcmtab;    /* private  ptr to ready-to-commit table.  */
	TEXT	*pwtoktbl;	/* query by word tokenization table */
  
	/* 
	 * The following fields are used for controlling collation 
	 * sequences and internal character translations.
	 */

        TEXT    tempdir[MAXPATHN+1]; /* temp directory */
	struct  omCtl *pomCtl;  /* ptr to cache of _Object schema records   */
#if OPSYS==WIN32API
        HANDLE  proshutThread;  /* Handle for the proshut thread */
#endif
        LONG    lastday;         /* to save the todays date for the .lg file*/

	ULONG   accessEnv;       /* access Environment/type */
        ULONG   useCount;        /* Number of db contexts referencing this
                                    dbctl.                                 */
        ULONG   modrestr;        /* module restrictions                    */
        ULONG   znfs;            /* do not report nfs file system */
        ULONG   argMpte;         /* shared memory allocation tries */
        unsigned argheapsize;    /* requested heap sized adjustment */
        GTBOOL    unlinkTempDB;   /* unlink the temp dbs ? */

        /*** message subsystem callbacks ***/
        DSMVOID (*msgn_CallBack)();
        DSMVOID (*msgd_CallBack)();
        DSMVOID (*msgt_CallBack)();
        DSMVOID (*enssarsy_CallBack)();

        COUNT lastObjectNumber;   /* for PROCODET database only */
        latch_t dbcontextLatch;   /* process private latch for this dbcontext */
        crashtest_t *p1crashTestAnchor; /* Anchor list of crash structures */
        semid_t     *psemids;	  /* pointer to the shared semaphore ids */

        struct tablestat *ptabstat; /* table stats structure */
        struct indexstat *pidxstat; /* index stats structure */
    
        LONG    ttWordBreakNumber; /* For temp table db only */
        TEXT   *pdatadir;          /* Root directory for database directories */
        ULONG   rlclsize;          /* Cluster size for new BI file */
        TEXT   *pmsgsfile;         /* location of messages file */ 
} dbcontext_t;

#include "axppackd.h"

/* for dbctl.usertype */
#define BROKER       1   /* just what it says */
#define DBSERVER     2   /* just what it says */
#define SELFSERVE    4   /* non "served" connection */
#define REMOTECLIENT 8   /* remote/network client */
#define DBSHUT       16  /* reserved for shutdown only */
#define DBUTIL       32  /* offline dbutil connection */
#define MONITOR      64  /* promon */
#define BACKUP       128 /* online backup */
#define BIW          256 /* before image writer */
#define AIW          512 /* after image writer */
#define APW         1024 /* asynchronous page writer */
#define WDOG        2048 /* watchdog */
#define NAPPLSV	    4096 /* application server*/
#define BATCHUSER   8192 /* non-interactive user */
#define RFUTIL     16384 /* online rfutil connection */

/* for dbctl.prodbimflgs  Used by PROGRESS DBIM */
#define PROXACTION	1	/* a transaction is active	*/
#define PROSERVED	2	/* remote database via a server	*/
#define DBENVOUTED	4	/* already called dbenvout */
#define DBAPPLSV	8	/* this is fake dbctl for client appl srvr*/
#define PRODISCON	16	/* a disconnect is under way */

/* for lock statistics */

#define LK_CT_SHR	0	/* share lock requests */
#define LK_CT_SHC	1	/* share lock conflicts */
#define LK_CT_SHG	2	/* share lock grants */
#define LK_CT_UPR	3	/* upgrade lock requests */
#define LK_CT_UPC	4	/* upgrade lock conflicts */
#define LK_CT_UPG	5	/* upgrade lock grants */
#define LK_CT_EXR	6	/* exclusive lock requests */
#define LK_CT_EXC	7	/* exclusive lock conflicts */
#define LK_CT_EXG	8	/* exclusive lock grants */
#define LK_CT_RGR	9	/* record-get lock requests */
#define LK_CT_RGC	10	/* record-get lock conflicts */
#define LK_CT_RGG	11	/* record-get lock grants */
#define LK_CT_DGG	12	/* ex to sh downgrades granted */
#define LK_CT_DRD	13	/* duplicate requests cancelled */
#define LK_CT_DRG	14	/* duplicate requests granted */
#define LK_CT_MAX	14	/* highest number */

/* DBLIC - The license statistics structure. This is part of dbshm_t */
typedef struct dblic9 
{
        ULONG cfgmaxuExceeded; /* # attempts to exceed user count */
        ULONG userThreshold; /* user defined user count */
        ULONG thresholdExceeded; /* # attempts to exceed above */
        ULONG maxConcurrent; /* max concurrent users */
        ULONG maxLastEntry; /* max concurrent users since last rpt entry */
        ULONG userTotal; /* total users connected since server startup */
        ULONG reportInterval; /* interval in seconds at which to write to rpt */
/* addition for License Logging feature - 97-02-04-035 */
        ULONG numValidLcnUsrs; /* max License Users as defined in cfg file */
        ULONG numCurActvCncts; /* current number of interactive connections */
        ULONG maxNumActvCncts; /* max number of interactiv connections for
                                      the past hour */
        ULONG minNumActvCncts; /* min number of interactiv
                                      connections for the past hour */
        ULONG numCurBatCncts;  /* current number of Batch connections */
        ULONG maxNumBatCncts;  /* max number of Batch connections for
                                     the past hour */
        ULONG minNumBatCncts;  /* min number of Batch
                                     connections for the past hour */
        ULONG numCurCncts;     /* current number of connections */
        ULONG maxNumCncts;     /* max number of connections for
                                     the past hour */
        ULONG minNumCncts;  /* min number of connections for the past hour */
} dblic_t;


/* DBSHM - The public database control structure.  In shared memory	    */
/*	   environments, there is 1 copy of this structure in shared memory */
/*	   In single-user or non-shared memory, this structure exists, but  */
/*	   is in private memory						    */
typedef struct dbshm 
{
	/* command line arguments which are shared among all processes	    */
	/* which access the same database.				    */
	QTEXT   qdbname;	/* absolute db pathname with .db suffix	    */
	QTEXT	qbiname;	/* absolute bi pathname with .bi suffix	    */
	QTEXT	qainame;	/* absolute ai pathname with .ai suffix	    */
	int	 argbkbuf;	/* -B #buffers				    */
	int	 argtpbuf;	/* -I tot # priv buffers		    */
	int	 rlagetime;	/* -G agetime		init 60		    */
	LONG	 arglknum;	/* -l #locks		init ARGLKDEF	    */
	int	 argcurs;	/* -c #cursors				    */
	int	 argnclts;	/* -n #users		init DEFTUSRS	    */
	int	 argnusr;	/* -n #usrs + -Mn #servers + 2		    */
	int	 maxsv;		/* -Ma max # of clients per server          */
	int	 argnsrv;	/* -Mn max # servs + 1	init 5		    */
        ULONG    maxxids;       /* -maxxids                                 */
	GTBOOL	 useraw;	/* -r or -R,   init RAWDEFLT | OSYNCFLG	    */
	GTBOOL	 fullinteg;	/* -i			init 1		    */
	COUNT	 tmenddelay;	/* -Mf #secs to delay tmend bi block flush  */

	int	 dbpubvers;	/* shared memory version number, see utshm.h*/
	QSTPOOL	 qdbpool;	/* points to the shared memory stpool hdr   */
	QUSRCTL  q1usrctl;	/* points to start of usrctl table	    */
	QSRVCTL	 q1srvctl;	/* points to start of srvctl table	    */
	QRLCTL	 qrlctl;	/* shared ptr to rlctl */
	QLKCTL   qlkctl;	/* shared ptr to lkctl */
	QBKCTL   qbkctl;	/* shared ptr to bkctl */
	QBKTBL   qmstrblk;	/* shared ptr to masterblk buffer control */
	TEXT	 dbIsHolder;    /* set if this is a schema holder database */
	TEXT     didchange;     /* Set when first note is logged after
                                   db startup                               */
	COUNT	 brokerCs;	/* broker's setting of ClientSwitch */
        QITBLK   qitblk1;	/* shared ptr to first index block */
	QTRANTAB qtrantab;
	QMTCTL	 qtmpmtctl;

	int	 numsems;
	int	 numsvbuf;
	LONG	 tmend_bi;	/* timestamp for unflushed tmend note	 */
	LONG	 tmend_ai;	/* timestamp for unflushed tmend note	 */
	int	 numcurs;	/* number of used cursors		 */
	int	 soreuse;	/* SCO V5.0 bug 95-09-05-057 storage var */
	QAICTL   qaictl;	/* points to after-image control table	 */
        QTLCTL   qtlctl;        /* points to transaction log control table */
	int	 slocks[3];	/* num SHARE, EX, and guarser schma locks
				   ownd by usrs*/
	QUSRCTL	 qwttsk;        /* chained users waiting for TASK        */
	QUSRCTL  qwtschma;	/* chained users waiting for SCHMA	 */
	LONG	 wbk_acc[13];	/* writen blocks counter		 */
	LONG	 rbk_acc[13];	/* read blocks counter			 */
	LONG	 abk_acc[13];	/* accessed blocks counter		 */
	TEXT	 br_usrfl;	/* Does the _User file have records?	 */
	TTINY	 br_language;	/* what language is this database?	 */
	int      argsdbuf;      /* -Mr record buffer size, init STDBUFSZ */
	int      argsdmsg;	/* -Mm ipc message buffer size,init MAXMSGSZ*/
	int      argmaxcl;	/* -Ma max client, init 0*/
	COUNT	 argmaxcs;	/* -cs cursor size (or max cursor ent)	 */
        QMANDCTL qmandctl;      /* qtr to shared manditory field arrays */
	COUNT	 argbibufs;	/* number of bi buffers */
	COUNT	 argnap;	/* min mtnap time */
	COUNT	 argnapinc;	/* mtnap increment time */
	COUNT	 argnapmax;	/* max mtnap time */
	COUNT	 argnapstep;	/* iterations betw mtnap time increments */
	COUNT	 argixlru;	/* % of buffer pool for ix blocks */
	ULONG	 arghash;	/* hash prime for buffer pool */
	LONG	 argspin;	/* Spin lock count before nap */
        GTBOOL    light_ai;      /* 1 if ai is used just as 2phase tl log */

	QTEXT	 qwtoktbl;	/* query by word  tokenization table */
	int	 directio;	/* 1 if we are to use direct io for db */
	QSEQCTL  qseqctl;	/* sequence generator control */
	/* more counters for promon */
        ULONG	waitTimes[WAITCNTLNG]; /* time spent waiting for things */

	ULONG	aibwCt;		/* bytes written to ai */
	ULONG	aidwCt;		/* ai dpendency write wait count */
	ULONG	ainwCt;		/* notes written to ai */
	ULONG	aipwCt;		/* partial ai buffers written */
	ULONG	aiwtCt;		/* ai buffer wait count */

	ULONG	bfckCt;		/* dirty buffers examined in cleaner */
	ULONG	bfclCt;		/* buffers cleaned */ 
	ULONG	bfflCt;		/* buffers written during flush */
	ULONG	bffoCt;		/* Buffers forced out */
	ULONG	bffsCt;		/* buffers scanned for free one */
	ULONG	bfitCt;		/* no. index blocks replaced in buffer pool */
	ULONG	bfmdCt;		/* buffers modified */
	ULONG	bfmmCt;		/* Modified buffer modified again */
	ULONG	bfrcCt;		/* Buffers cleaned  multiple times */
	ULONG	bfrdCt;		/* buffers read from disk */
	ULONG	bfscCt;		/* dirty buffer scans */
	ULONG	bfwrCt;		/* buffers replaced to disk */


	ULONG	bibcCt;		/* bi buffer clean count */
	ULONG	bibrCt;		/* bytes read from bi */
	ULONG	bibwCt;		/* bytes written to bi */
	ULONG	biclCt;		/* bi clusters closed */
	ULONG	bidwCt;		/* bi dependency wait count */
	ULONG	biflCt;		/* bi buffers flushed */
	ULONG	binaCt;		/* empty bi buffer not available */
	ULONG	binrCt;		/* bi notes read */
	ULONG	binwCt;		/* notes written to bi */
	ULONG	bipwCt;		/* partial bi buffers written */
	ULONG	biruCt;		/* bi clusters reused */
	ULONG	biwtCt;		/* waits for busy bi block */

	ULONG	commitCt;	/* transactions committed */
	ULONG	dlckCt;		/* dead locks deleted at tx end */
	ULONG	dbxtCt;		/* database extend blocks */
        ULONG   shrfndlkCt;     /* shared find locks */
        ULONG   exclfndlkCt;    /* exclusive find locks */

	ULONG	fradCt;		/* add block to free chain */
	ULONG	frrmCt;		/* remove block from free chain */

	ULONG	ixdlCt;		/* index entires deleted */
	ULONG	ixfnCt;		/* index entries found */
	ULONG	ixgnCt;		/* index entries generated */
	ULONG	ixrdCt;		/* index block reads */
	ULONG	ixwrCt;		/* index block writes */

	ULONG	mbflCt;		/* masterblock flush count */

	ULONG	aibcCt;		/* ai blocks cleaned by aiw */
	ULONG	ainaCt;		/* ai buffer not available */

        ULONG   rmalCt;         /* number of calls to rmfndsp */
        ULONG   rmbsCt;         /* rm blocks searched for space */
        ULONG   rmusCt;         /* record allocated from existing rm block */
	ULONG	rmadCt;		/* add block to rm free chain */
	ULONG	rmboCt;		/* block added to end of rm free chain */
	ULONG	rmdlCt;		/* rmdel's */
	ULONG	rmenCt;		/* block moved to end of rm free chain */
	ULONG	rmgtCt;		/* rmget's */
	ULONG	rmhnCt;		/* hold note deleted */
	ULONG	rmmkCt;		/* rmmak's */
	ULONG	rmrmCt;		/* remove block from rm free chain */
	ULONG	rmrpCt;		/* rmrep's */
        ULONG   rmbrCt;         /* rm bytes read by rmgetf */

	ULONG	semwCt;		/* Semaphore wait count */

	ULONG	undoCt;		/* transactions rolled back */

	ULONG	aiwtTm;		/* ai buffer wait time */
	ULONG	bidwTm;		/* bi dependency wait time */
	ULONG	birdTm;		/* bi read waiting time (usec) */
	ULONG	biwrTm;		/* bi write waiting time (usec) */
	ULONG	dbrdTm;		/* db read waiting time (usec) */
	ULONG	dbwrTm;		/* db write waiting time (usec) */
        ULONG   bfpcCt;         /* buffers checkpointed by page writer */
        ULONG   bfncCt;         /* buffers which would have been chkpted */
        ULONG   ixspCt;         /* index blocks split */
        ULONG   ixcmCt;         /* index blocks combined */
        ULONG   ixmtCt;         /* index / MT deadlocks avoided */
        ULONG   mtnwCt;         /* nowait attempts to lock MT */
        ULONG   tprcCt;         /* 2 phase ready to commit */
        ULONG   tpflCt;         /* 2 phase force logs */
        ULONG   tpcoCt;         /* 2 phase tx coordinated */
        ULONG   rmbdCt;         /* rm bytes deleted by rmdorem */
        ULONG   rmbuCt;         /* rm bytes updated by rmdochg */
        ULONG   rmbcCt;         /* rm bytes created by rmdoins */
        ULONG   rmfrCt;         /* rm fragments read by rmgetf */
        ULONG   rmfdCt;         /* rm fragments deleted by rmdorem */
        ULONG   rmfuCt;         /* rm fragments updated by rmdochg */
        ULONG   rmfcCt;         /* rm fragments created by rmdoins */
        ULONG   rmbaCt;         /* rm bytes allocated by rmfndsp */
	ULONG   mtrxCt;		/* microtransactions done */
	ULONG   pwqwCt;         /* page writer queue writes */
	ULONG   pwqaCt;         /* page writer enqueues */

	ULONG	lockCt[LK_CT_MAX+1]; /* lock table statistics counters */
	ULONG   pwcwCt;         /* page writer writes from ckp queue */
	ULONG   pwswCt;         /* page writer writes from scan loop */

	ULONG   cntSpare[6];    /* extras */
        QXID    qxidFree;       /* qpointer to xid free list  */
        QXID    qxidAlloc;      /* qpointer to list of active xids */
        LONG    pwscan;         /* apw buffer scan limit */
        COUNT   pwsdelay;       /* apw buffer scan delay (sec) */
        LONG    pwwmax;         /* apw max buffers to write per scan */
        COUNT   pwqdelay;       /* apw queue scan delay (ms) */
        COUNT   pwqmin;         /* apw min queue length before write */
        UCOUNT  quietpoint;    /* see defines below */
#define QUIET_POINT_NORMAL      0 /* Normal processing without quiet point */
#define QUIET_POINT_REQUEST     1 /* Request to enable quiet point */
#define QUIET_POINT_ENABLED     2 /* Quiet point has been enabled */
#define QUIET_POINT_DISABLE     3 /* Request to disable quiet point */
#define QUIET_POINT_BITHRESHOLD 4 /* request to change bi threshold */

        COUNT   argaibufs;      /* number of ai buffers */
        ULONG   arglkhash;      /* lock table hash */

	ULONG   configusers;    /* max "real" users from config file */
	ULONG   currentusers;   /* number of current "real" users */
        QTRANTABRC qtrantabrc;
	GTBOOL   aistall;     /* 1 - Don't exit if next ai extent full */
        dblic_t   dblic;     /* licensing statistics structure */
	TEXT	brokerCsName[20]; /* brokers -charset(clientSwitchName) value */
/* TODO: remove lastday when V8/V9 integration completed */
        LONG        lastday;     /* to save the todays date for the .lg file*/

        int         brokerpid;   /* process id of broker */
        GTBOOL       shutdn;      /* everybody must shutdown */
        int   brokertype;       /* Type of broker */
                                /*    0 = 4GL BROLER */
                                /*    1 = JUNIPER BROKER */
        int   shutdnInfo;       /* Initiator of shutdown - see defines below */
#define DATABASE_ERROR   1      /* db engine set flag due to an error */ 
#define WATCHDOG_ERROR   2      /* internal/external watchdog due to error */
#define PROSHUT_FORCE    4      /* proshut with -F */
#define PROSHUT_ABNORMAL 8      /* proshut normal escalated to force */ 
#define PROSHUT_NORMAL  16       
#define BROKER_NORMAL   32      /* Juniper set shutdn flag */
        int   shutdnpid;        /* process id of shutdown initiator */
        int   shutdnStatus;     /* current status of shutdown in-progress */
 
	LONG	 argmux;	/* 0 = use mtlatch, else use muxlatches */
        QOMCTL   qomCtl;        /* cache of _Object records   */
	ULONG   argomCacheSize; /* Number of entries in the object cache */ 
	ULONG   omHashPrime;    /* Entries in hash table for object cache */
        ULONG	dbblksize;	/* database blocksize               */
		
	/* The following 2 fields are a function of dbblksize */
	UCOUNT  dbCreateLim;    /* free space req'd to create a new record */
	UCOUNT  dbTossLim;      /* free space to guarantee on free list */

	UCOUNT	recbits;	/* record number of bits, if 8k block then 6
				   bits else RECBITS(=5) */
#if 0
	ULONG	rmoffset;	/* offset of record in block */
	ULONG	hold;		/* dbkey deleted by a task */
#endif
	/* index table parameters */
        COUNT   bwdelay;        /* nap time between sweeps of bi bufs for BIW*/
        COUNT   groupDelay;     /* client nap time for group commit     */
	UCOUNT	maxkeysize;	/* max key size depends of database blocksize */
	UCOUNT	maxinfosize;	/* max info size depends of database blocksize*/
        LONG    argxshm;        /* Excess Shared Memory */
        int     argbtbuf;       /* 4GL temp table buffers */

        ULONG   bithold;        /* threshold size for bi*/
        GTBOOL   bistall;        /* 1 - Don't exit if bi threshold reached  */

        ULONG   isbaseshifting;
        dsmTable_t   argtablebase;  /* statistics table base*/
        dsmTable_t   argtablesize; /* statistics table limit */
        dsmIndex_t   argindexbase;  /* statistics index base */
        dsmIndex_t   argindexsize; /* statistics index limit */
        QTBLARR qtablearray;
        QIDXARR qindexarray;

	ULONG	numsemids;	/* number of shared memory semaphore sents */
        QSEMIDS qsemids;	/* shared pointer to semids structure */

#ifdef PRO_REENTRANT
        QUSRLATCH q1usrLatch;   /* to start of private usrctl latch table   */
#endif
        QCRASHANCHOR q1crashAnchor; /* start of list of crash structs */
#if 0
    /* this will speed up logins for large number of users */
    /* see  dbuser.c. */
	psc_user_number_t freeRusr; /* next free usrctl for remote user */
	psc_user_number_t freeLusr; /* next free usrctl for local user */
#endif

        ULONG numROBuffers; /* number of private read only buffers reserved */
        LONG  loginSemid;   /* id for the login semaphore */
        LONG  lockWaitTimeOut;
} dbshm_t;

/* macros for statistics counters */

#define STATINC(x) ((pcontext->pdbcontext->pdbpub->x)++)
#define STATADD(x,n) pcontext->pdbcontext->pdbpub->x = \
                                       pcontext->pdbcontext->pdbpub->x + n
#define STATGET(x) pcontext->pdbcontext->pdbpub->x

/*values for dbpub.br_usrfl*/
#define HASUSREC 2

#ifdef EDBGLOB
LOCAL dbcontext_t dbcontext;
LOCAL dbshm_t dbshm;
GLOBAL dbcontext_t *p_dbcontext = &dbcontext;
GLOBAL dbshm_t *p_dbshm = &dbshm;
GLOBAL LONG     xerrno;
#else
IMPORT dbcontext_t *p_dbcontext;
IMPORT dbshm_t *p_dbshm;
IMPORT LONG     xerrno;
#endif

#endif /* DBCONTEXT_H */
