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

#ifndef SHMPUB_H
#define SHMPUB_H

struct dbcontext;
struct stpool;
struct latch;
struct crashtest;

typedef unsigned long VLMsize_t;  /* These have to be 64-bit clean for VLM platfoms */



#if UNIX64
#define MAXSGTBL      127	/* max size of segment table */
#define SEGADDR     unsigned long      /* 64 bit segment address */
#else
#define MAXSGTBL      32	/* max size of segment table */
#define SEGADDR     ULONG	       /* segment address */
#endif  /* UNIX64 */



typedef struct shmTable		/* segment table control structure */
{
    COUNT ipub;			/* index to next available public slot */
    COUNT iprv;		        /* index to next available private slot */
    SEGADDR sgtbl[MAXSGTBL];	/* segment table */
    ULONG   sglen[MAXSGTBL];	/* length of the segment in the table */
} shmTable_t;

#if UNIX64
/* very large shared memory segments (1 Gb) */
 
#define SHPTR       unsigned long      /* 64 bit SHPTR datatype */
 
#define SHSGSHFT    30
#define OFSTMASK    (LONG)0x3FFFFFFF    /* offset mask */
#define PRIVATE      -2                 /* see utshsgaddll() */
#define MAXPRIVSLT    10                /* slot reserved for private pointers */
 
#else

#define SHPTR       ULONG	       /* SHPTR datatype */

/* big shared memory segments (128 mb) */
#define SHSGSHFT    27

#endif  /* UNIX64 */

#if OPSYS==UNIX || OPSYS == WIN32API
/* Holder for IPC data for server and clients */
typedef struct bkIPCData
{
    int     OldSemId;     /* Used to store semid of crashed broker */
    int     NewSemId;     /* Used to store semid for client connection */
    int     OldShmId;     /* Used to store shmid of crashed broker */
    int     NewShmId;     /* Used to store shmid for client connection */
}bkIPCData_t;
#endif /* OPSYS==UNIX  || WIN32API */

/* Declaration of functions */

struct dsmContext;

VLMsize_t shmGetShmSize	(struct dsmContext *pcontext, LONG blockSize);


int shmAttach	(struct dsmContext *pcontext);


#if OPSYS==UNIX
int shmCreate 	(struct dsmContext *pcontext, bkIPCData_t *pbkIPCData);
#else
int shmCreate 	(struct dsmContext *pcontext);
#endif

shmTable_t *shmCreateSegmentTable	(struct stpool *pstpool );

DSMVOID
shmDelete	(struct dsmContext *pcontext);

DSMVOID
shmDetach	(struct dsmContext *pcontext);

SHPTR  shmPtoQP	(struct dsmContext *pcontext, SEGADDR sgaddr);

LONG shmSegArea   (struct dbcontext *pdbcontext);

psc_user_number_t shmGetMaxUSRCTL(DSMVOID);


#ifdef HAVE_SHARED_MEMORY
/* pointer transform macros */
#define QP_TO_P(pdbcontext, qtr) \
                   ((pdbcontext)->pshsgctl->sgtbl[(qtr) >> SHSGSHFT] + (qtr))

#define P_TO_QP(pcontext, ptr)  shmPtoQP((pcontext), (SEGADDR)(ptr))
#else
#define QP_TO_P(pdbcontext, qtr) (qtr)
#define P_TO_QP(pcontext, ptr)   (ptr)
#endif

#define QSELF(pstruc)    (pstruc)->qself   /* self-referential shared ptr */


/* MACROS for extracting real ptrs for shared ptrs for major datastructures */

#define XBKCHAIN(pdb, qbkchain) ((struct bkchain *)QP_TO_P(pdb, qbkchain))
#define XBKCTL(pdb, qbkctl)    	((struct bkctl *)QP_TO_P(pdb, qbkctl))
#define XBFTBL(pdb, qbftbl)   	((struct bftbl *)QP_TO_P(pdb, qbftbl))
#define XBKTBL(pdb, qbktbl)  	((struct bktbl *)QP_TO_P(pdb, qbktbl))
#define XUSRCTL(pdb, qusrctl)	((struct usrctl *)QP_TO_P(pdb, qusrctl))
#define XUSRLATCH(pdb, qusrLatch) ((struct latch *)QP_TO_P(pdb, qusrLatch))
#define XXID(pdb, qxid)	((struct dbxaTransaction *)QP_TO_P(pdb, qxid))
#define XBKBUF(pdb, qbkbuf)   	((struct bkbuf *)QP_TO_P(pdb, qbkbuf))
#define XMSTRBLK(pdb, qmstrblk) \
                ((struct mstrblk *)XBKBUF(pdb, XBKTBL(pdb, qmstrblk)->bt_qbuf))
#define XBKFTBL(pdb, qbkftbl)  	((struct bkftbl *)QP_TO_P(pdb, qbkftbl))
#define XBKAREAD(pdb, qbkAreaD) ((bkAreaDesc_t *)QP_TO_P(pdb, qbkAreaD))
#define XBKAREADP(pdb, qbkAreaDPtr) \
                                ((bkAreaDescPtr_t *)QP_TO_P(pdb, qbkAreaDPtr))
#define XBKFSYS(pdb, qbkfsys)  	((struct bkfsys *)QP_TO_P(pdb, qbkfsys))
#define XTEXT(pdb, qtext)      	((TEXT *)QP_TO_P(pdb, qtext))
#define XCHAR(pdb, qtext)      	((char *)QP_TO_P(pdb, qtext))
#define XRLBLK(pdb, qrlblk)    	((struct rlblk *)QP_TO_P(pdb, qrlblk))
#define XRLCTL(pdb, qrlctl)    	((struct rlctl *)QP_TO_P(pdb, qrlctl))
#define XAIBLK(pdb, qaiblk)    	((struct aiblk *)QP_TO_P(pdb, qaiblk))
#define XAICTL(pdb, qaictl)    	((struct aictl *)QP_TO_P(pdb, qaictl))
#define XTLBLK(pdb, qtlblk)     ((struct rltlBlockHeader *)QP_TO_P(pdb, qtlblk))                       
#define XTLCTL(pdb, qtlctl)     ((struct rltlCtl *)QP_TO_P(pdb, qtlctl))
#define XSTPOOL(pdb, qstpool)  	((struct stpool *)QP_TO_P(pdb, qstpool))
#define XITBLK(pdb, qitblk)    	((ITBLK *)QP_TO_P(pdb, qitblk))

#define XDBKEY(pdb, qdbkey)    	((DBKEY *)QP_TO_P(pdb, qdbkey))

#define XLKCTL(pdb, qlkctl)   	((struct lkctl *)QP_TO_P(pdb, qlkctl))
#define XLCK(pdb, qlk)        	((struct lck *)QP_TO_P(pdb, qlk))      
#define XMTCTL(pdb, qmtctl)    	((MTCTL *)QP_TO_P(pdb, qmtctl))
#define XSRVCTL(pdb, qsrvctl)  	((struct srvctl *)QP_TO_P(pdb, qsrvctl))
#define XTRANTAB(pdb, qtrantab) ((TRANTAB *)QP_TO_P(pdb, qtrantab))
#define XTRANTABRC(pdb, qtrantabrc) \
                                ((struct trantabrc *)QP_TO_P(pdb, qtrantabrc))
#define XABK(pdb, qabk)         ((struct abk *)QP_TO_P(pdb, qabk))
#define XSHMDSC(pdb, qshmdsc)   ((SHMDSC *)QP_TO_P(pdb, qshmdsc))
#define XRENTST(pdb, qrentst)   ((struct rentst *)QP_TO_P(pdb, qrentst))
#define XSEGMNT(pdb, qsegmnt)   ((struct segment *)QP_TO_P(pdb, qsegmnt))
#define XPIDLIST(pdb, qpidlist) ((PIDLIST *)QP_TO_P(pdb, qpidlist))
#define XSEQTBL(pdb, qseqtbl)   ((struct seqTbl *)QP_TO_P(pdb, qseqtbl))
#define XSEQCTL(pdb, qseqctl)  	((struct seqCtl *)QP_TO_P(pdb, qseqctl))
#define XSEMIDS(pdb, qsemids)  	((semid_t *)QP_TO_P(pdb, qsemids))
#define XOMCTL(pdb, qomctl)     ((struct omCtl *)QP_TO_P(pdb, qomctl))
#define XOMOBJECT(pdb, qomobject) ((struct omObject *)QP_TO_P(pdb, qomobject))
#define XCRASHANCHOR(pdb, qcrashanchor)	((struct crashtest *)QP_TO_P(pdb, qcrashanchor))
#define XTABLPTR(pdb, qtableptr)     ((struct tablestat *)QP_TO_P(pdb,qtableptr))
#define XIDXPTR(pdb, qindexptr)      ((struct indexstat *)QP_TO_P(pdb,qindexptr))
#define XRLBF(pdb, x)	((struct rlbftbl *)QP_TO_P(pdb, x))
#define XAIBUF(pdb, x)  ((struct aibuf *)QP_TO_P(pdb, x))
#define XBKAP(pdb, x)  ((struct bkAreaDescPrt *)QP_TO_P(pdb, x))
                                 
/* SYMBOLIC DEFINTIONS for shared ptr datatypes */
#ifdef HAVE_SHARED_MEMORY
#define QBKCHAIN SHPTR
#define QBKCTL   SHPTR
#define QBFTBL   SHPTR
#define QBKTBL   SHPTR
#define QUSRCTL  SHPTR
#define QUSRLATCH SHPTR
#define QBKBUF   SHPTR
#define QMSTRBLK SHPTR
#define QBKFTBL  SHPTR
#define QBKAREAD      SHPTR
#define QBKAREADP     SHPTR
#define QBKFSYS  SHPTR
#define QTEXT    SHPTR
#define QRLBLK   SHPTR
#define QRLCTL   SHPTR
#define QAIBLK   SHPTR
#define QAICTL   SHPTR
#define QTLCTL   SHPTR
#define QSTPOOL  SHPTR
#define QITBLK   SHPTR
#define QDBKEY   SHPTR
#define QLKCTL   SHPTR
#define QLCK     SHPTR
#define QDBKEY   SHPTR
#define QMTCTL   SHPTR
#define QSRVCTL  SHPTR
#define QTRANTAB SHPTR
#define QTRANTABRC SHPTR
#define QCURSENT SHPTR
#define QABK     SHPTR
#define QSHMDSC  SHPTR
#define QRENTST  SHPTR
#define QSEGMNT  SHPTR
#define QPIDLIST SHPTR
#define QMANDCTL SHPTR
#define QSEQTBL  SHPTR
#define QSEQCTL  SHPTR
#define QSECTL   SHPTR
#define QSEMIDS  SHPTR
#define QOMCTL   SHPTR
#define QOM_OBJECT SHPTR
#define QCRASHANCHOR SHPTR
#define QTBLARR  SHPTR
#define QIDXARR  SHPTR
#define QXID     SHPTR
#define QRLBF	SHPTR
#define QAIBUF          SHPTR
#define QBKAP          SHPTR
#else
#define QBKCHAIN      struct bkchain *
#define QBKCTL        struct bkctl *
#define QBFTBL        struct bftbl *
#define QBKTBL        struct bktbl *
#define QUSRCTL       struct usrctl *
#define QUSRLATCH     struct latch *
#define QBKBUF        struct bkbuf *
#define QMSTRBLK      struct mstrblk *
#define QBKFTBL       struct bkftbl *
#define QBKAREAD      struct bkAreaDesc *
#define QBKAREADP     struct bkAreaDescPtr *
#define QBKFSYS       struct bkfsys *
#define QTEXT         TEXT *
#define QRLBLK        struct rlblk *
#define QRLCTL        struct rlctl *
#define QAIBLK        struct aiblk *
#define QAICTL        struct aictl *
#define QTLCTL        struct tlctl *
#define QSTPOOL       struct stpool *
#define QITBLK        struct itblk *
#define QDBKEY        DBKEY *
#define QLKCTL        struct lkctk *
#define QLCK          struct lck *
#define QMTCTL        MTCTL *
#define QSRVCTL       struct srvctl *
#define QTRANTAB      TRANTAB *
#define QTRANTABRC    struct trantabrc *
#define QABK          struct abk *
#define QSHMDSC       SHMDSC *
#define QRENTST       struct rentst *
#define QSEGMNT       struct segmnt *
#define QPIDLIST      PIDLIST *
#define QMANDCTL      struct mandctl *
#define QSEQTBL       struct seqtbl *
#define QSEQCTL       struct seqctl *
#define QSECTL        struct sectl *
#define QSEMIDS       struct semids *
#define QOMCTL        struct omCtl *
#define QOM_OBJECT    struct omObject *
#define QCRASHANCHOR  struct crashanchor *
#define QTBLARR       struct tablestat *
#define QIDXARR       struct indexstat *
#define QXID          struct dbxaTransaction *
#define QRLBF	      struct rlbftbl *
#define QAIBUF        struct aibuf *
#define QBKAP         struct bkAreaDescPtr *
#endif  /* HAVE_SHARED_MEMORY */
                       
#endif  /* #ifndef SHMPUB_H */





