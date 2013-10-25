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

#ifndef CXPRV_H
#define CXPRV_H

#include "bkpub.h"
#include "bmpub.h"         /* For bmBufHandle_t */
#include "cxpub.h"
#include "dsmpub.h"
#include "rlpub.h"

struct lockId;

/**********************************************************/
/* cxprv.h - Private function prototypes for CX subsystem. */
/**********************************************************/

/* The maximum size of the hi order bytes of the dbkey that will need
   to be encoded into a key entry.  Which is the size of the dbkey less the last byte
   and less the lead byte which for a 64 bit dbkey will surely never be non-zero.  */
#define  CX_MAX_X    sizeof(DBKEY) - 2   

/*************************************************************/
/* Private definitions and data structures                   */
/*************************************************************/

typedef struct ixhdr
{
	GTBOOL          ih_top;         /* 1 if top level block */
	GTBOOL          ih_bot;         /* 1 if bottom level    */
	COUNT           ih_ixnum;       /* number of index file */
        LONG            ih_lockCount;   /* delete lock count
					   for future use
					   currently set to -1    */
	COUNT           ih_nment;       /* number of keys in blk*/
	COUNT           ih_lnent;       /* total length of ents */
} ixhdr_t;


/* structure of an index block */

#define IXTOP ix_hdr.ih_top
#define IXBOT ix_hdr.ih_bot
#define IXNUM ix_hdr.ih_ixnum
#define IXNUMENT ix_hdr.ih_nment
#define IXLENENT ix_hdr.ih_lnent

#define MAXIXCHRS  \
 (pcontext->pdbcontext->pdbpub->dbblksize - sizeof(struct bkhdr) - \
  sizeof(struct ixhdr))  /* 484 */


/*-----------------------------*/
/* structure of an index block */
/*-----------------------------*/

typedef struct ixblk
{
        struct bkhdr    bk_hdr;
        struct ixhdr    ix_hdr;
        TEXT            ixchars[1];/* placeholder for entry space
				      actual size is determined at runtime,
				      as BlockSize is runtime option.
				      size = MAXIXCHRS number of bytes.
				    */
} cxblock_t;

#define CXBLK cxblock_t


/*---------------------------------*/
/* struct of 2nd block of database */
/* dont use this to allocate stg   */
/*---------------------------------*/

typedef struct itblk { 
         struct bkhdr          bk_hdr;
         DBKEY          it_dbkey[1];  /* placeholder for dbkey table -
					 actual size is computed at runtime
					 based on BlockSize.
				      */
      /* TEXT           it_uniq[IXTBLLEN];  follows 'it_dbkey' table.
					    the actual size of it_dbkey table 
					    is not known until runtime.
       */
} itblk_t;


/*---------------------------------------*/
/* index anchor length in 'itblk' struct */
/*---------------------------------------*/

#define IXANCHRLEN   (sizeof(DBKEY) + sizeof(TEXT) )


/*-----------------------*/
/* parm to cxTableUpdate */
/*-----------------------*/

#define IXUNIQ          1  /* it is a unique index */
#define IXNOCHG         2
#define IXWORDIX        64 /* it is a word index */

#define MAXINFSZ        255 /* Max info length */
/* #define MAXENTSZ        3 + MAXKEYSZ + MAXINFSZ  */
#define MAXENTSZ        LRGENTHDRSZ + MAXKEYSZ + MAXINFSZ /* Max entry length */
#define MAXWRDSZ        32 /* Max word size */
#define ROOTS           16
#define MAXLEVELS       16 /* Max number of levels in the B-tree */


/* Compressed index entry */
/* the entry structure is:

        Size    Symbol  Description
      (Bytes)
      _______   ______  _______________________________________
         1      cs      Compressed Size
         1      ks      Key Size
         1      is      Info Size
        Var.    key     The key string
        Var.    Info    The Info part of the entry
*/

/* CXEAP flags */
#define        CXFIND           1               /* find entry with given key    */
#define        CXPUT            2               /* put an entry in the B-tree   */
#define        CXDEL            4               /* delete a B-tree entry        */
#define        CXNEXT           8               /*(8) find next entry           */
#define        CXPREV           16              /*(16) find previous entry      */
#define        CXFIRST          32              /*(32) find next entry          */
#define        CXLAST           64              /*(64) find previous entry      */
#define        CXKILL           128             /* kill the whole index         */
#define        CXOPMASK         255             /* mask for op code             */
/* do not change above this line (see cxdown) *****/
#define        CXRETDATA        256 /* return the INFO part of the entry */
#define        CXSUBSTR         512 /* use only substring (prefix) of the key */
#define        CXTAG            1024    /* a record TAG, identifying a record */
#define        CXUNIQE          2048    /* a uniqe index entry */
#define        CXLOCK           4096    /* an index lock */
#define        CXNOLOG          8192    /* no logical note is needed */
#define        CXREBLD          16384   /*index rebuild by filling blocks */
#define        CXGREQ           (CXNEXT | FNDGE)        /* find greater equal */
#define        CXGET            CXFIND+CXRETDATA /* find and return INFO */
#define        CXGETNEXT        CXNEXT+CXRETDATA /* find next and return INFO */
#define        CXGETPREV        CXPREV+CXRETDATA /* find prev and return data */
#define        CXPUTTAG         CXPUT+CXTAG /* put a given TAG (recid) */
#define        CXDELTAG         CXDEL+CXTAG /* delete a given TAG (recid) */

/* ERROR codes */
/*IXDUPKEY 1 - as defined in ixctl.h */
#define CXSucc          128      /* success */
#define CXFail          256      /* failure */

/**************************** block definition ***************************/

/***** entry access parameter structure ***********************************/
typedef struct cxeap            /* entry access parameter structure */
{
        ULONG           flags;
        TEXT            *pinpkey;
        TEXT            *pdata;
        TEXT            *poutkey;
        dsmKey_t        *pkykey;
        cxcursor_t      *pcursor;
        bmBufHandle_t   bftorel;       /* buffer to be released */
        DBKEY           blknum;
        UCOUNT          inkeysz;
        UCOUNT          substrsz;       /* substr length, for substr option */
        UCOUNT          datasz;
        UCOUNT          datamax;
        UCOUNT          outkeysz;
        UCOUNT          mxoutkey;
        UCOUNT          errcode;
        UCOUNT          state;
}cxeap_t;

#define CXEAP cxeap_t


typedef struct cxbap            /* cxtree block access parameter structure */
{
        UCOUNT          flags;
        UCOUNT          errcode;
        DBKEY           blknum;
        bmBufHandle_t   bufHandle;
        cxblock_t       *pblkbuf;
} cxbap_t;

#define CXBAP cxbap_t

/**************************** UFIND parameters ********************/

typedef struct findp            /* cxtree entry find parameter structure */
{
        DBKEY           blknum;
        bmBufHandle_t   bufHandle;
        cxblock_t       *pblkbuf;
        TEXT            *position;
        TEXT            *pinpkey;
        TEXT            *pinpdata;
        dsmKey_t        *pkykey;
        TEXT            *pprv;
        TEXT            *pprvprv;
        UCOUNT          inkeysz;
        UCOUNT          csofnext;
        UCOUNT          csofprev;
        UCOUNT          indatasz;
        UCOUNT          flags;
	    COUNT		    filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
} findp_t;

#define FINDP findp_t

#define INIT_S_FINDP_FILLERS( fp ) (fp).filler1 = 0;


/* an entry of a level in the downlist - points to blocks in
        one level of the cxtree */
typedef struct inlevel /* down list info for each level of the B-tree */
{
	DBKEY           dbk;    /* blk number of current block */
	DBKEY           nxtprvbk;/* blk number of next or prev block */
} inlevel_t;

#define INLEVEL inlevel_t


typedef struct downlist
{
	ULONG   area;   /* Storage area the index is in.    */
	UCOUNT  currlev;/* current level, if root level = 0 */
	UCOUNT  locklev;/* number of locked levels */
	INLEVEL level[MAXLEVELS];       /* level info */
} downlist_t;

#define DOWNLIST downlist_t


#ifndef CXINITLV
  /*--------------------------------------------*/
  /* Note that this is also defined in cxpub.h. */
  /*--------------------------------------------*/

#define CXINITLV( pdn )      pdn->currlev = pdn->locklev = 0
#endif



typedef struct cxnote
{
	RLNOTE     rlnote;
	TEXT       sdbk[ sizeof(DBKEY) ];
	dsmDbkey_t indexRoot;
	COUNT      offset;	/* offset of insert position from start of block buffer */
/* #ifdef LARGE_INDEX_ENTRIES_DISABLED */
/*	TEXT       cs;		 compression size of the inserted key */ 
/*	TEXT       extracs;	 additional compression of key next to the inserted */
/*	TEXT       indexAttributes;   Unique or word             */ 
/*	TEXT	   filler1;	 bug 20000114-034 by dmeyer in v9.1b */
/*	COUNT	   filler2;	 bug 20000114-034 by dmeyer in v9.1b */
/* #else */
	UCOUNT     cs;		/* compression size of the inserted key */
	UCOUNT     extracs;	/* additional compression of key next to the inserted */
        dsmObject_t  tableNum;  /* Table number the index is on    */
	TEXT       indexAttributes;  /* Unique or word             */
	TEXT	   tagOperation;
        
/*#endif  LARGE_INDEX_ENTRIES_DISABLED  */
} cxnote_t;

#define CXNOTE cxnote_t

/*#ifdef LARGE_INDEX_ENTRIES_DISABLED */
/*#define INIT_S_CXNOTE_FILLERS( cx ) (cx).filler1 = 0; (cx).filler2 = 0;  */
/*#else */
#define INIT_S_CXNOTE_FILLERS( cx ) (cx).filler1 = 0; /* bug 20000114-034 by dmeyer in v9.1b */
/*#endif  */


/*** Appears to be unused as of V9.1b and is thus compiled out ***/
#if 0
typedef struct cxtagnt {   
 	RLNOTE	rlnote;
 	COUNT	offset;		/* offset of insert position from start of the block buffer */
 	TEXT	tag;		/* the tag (least significant byte) of the dbkey */
} cxtagnt_t;
 
#define CXTAGNT cxtagnt_t
#endif


typedef struct cxsplit {
	RLNOTE	rlnote;
	struct	ixhdr ixhdr1;
	struct	ixhdr ixhdr2;
	TEXT	extracs;/* size to decompress the 1st key in new blk */
	TEXT	filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
	COUNT	filler2;	/* bug 20000114-034 by dmeyer in v9.1b */
} cxsplit_t;

#define CXSPLIT cxsplit_t

#define INIT_S_CXSPLIT_FILLERS( cx ) (cx).filler1 = 0; (cx).filler2 = 0; /* bug 20000114-034 by dmeyer in v9.1b */


/*** Appears to be unused as of V9.1b and is thus compiled out ***/
#if 0
typedef struct	ixknote {
	RLNOTE	rlnote;
	DBKEY	dbkey;
	COUNT	ixnum;
} ixknote_t;

#define IXKNOTE ixknote_t
#endif

typedef struct ixnewm {
	RLNOTE	rlnote;
	DBKEY	ixdbk;
	COUNT	ixnum;
	TEXT	ixbottom;
	TEXT	filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
} IXNEWM;

#define INIT_S_IXNEWM_FILLERS( ix ) (ix).filler1 = 0; /* bug 20000114-034 by dmeyer in v9.1b */


typedef struct ixfinote {
	RLNOTE	rlnote;
	ULONG	ixArea;
	DBKEY	ixdbk;
	dsmIndex_t ixnum;
        dsmTable_t table;
	TEXT	ixunique;
	TEXT	filler1;	/* bug 20000114-034 by dmeyer in v9.1b */
} IXFINOTE;

#define INIT_S_IXFINOTE_FILLERS( ix ) (ix).filler1 = 0; /* bug 20000114-034 by dmeyer in v9.1b */


typedef struct ixdbnote {
	RLNOTE rlnote;
	DBKEY  rootdbk;
} ixdbnote_t;

#define IXDBNOTE ixdbnote_t


typedef struct ixcpynote {
	RLNOTE rlnote;
	dsmIndex_t  ixNum;
} ixcpynote_t;

#define IXCPYNOTE ixcpynote_t


typedef struct cxCompactNote
{
	RLNOTE     rlnote;
	TEXT       sdbk[ sizeof(DBKEY) ];
	dsmDbkey_t indexRoot;
	COUNT      offset;            /* offset of insert position in blk */
	int        cs;                /* compression size of first key in buffer */
	int        csofnext;          /* cs of key after offset */
	int        numMovedEntries;   /* number entries being moved */
	int        moveBufferSize;    /* size of entries being moved */
	COUNT      lengthOfEntries;   /* original lenght of entries */
} cxCompactNote_t;



/*****************************************/
/*** declares for internal subroutines ***/
/*****************************************/

/* kycmp return codes (also declared in kymgr.h */

#ifndef FOUND
#define FOUND           0       /* exact match */
#define SUBSTRING       1
#define KEEPGOING       2
#define LARGER          2
#define TOOFAR          3
#endif

/************************************************/
/*#ifndef LARGE_INDEX_ENTRIES_DISABLED */
/************************************************/
/**  definitions for large index entries stuff **/
/************************************************/

#define INDEX_TAG_OPERATION       0x7f  /* used in do/undo notes */

/*---------------------------------------------*/
/*  Macroes for handling index entries in the b-tree blocks
    where entries can have either small or large key formats  ***/
/*---------------------------------------------*/
#define CS_KS_IS_HS_GET(p) if (*p == LRGKEYFORMAT)     \
                           {                               \
                               cs=xct(p + 1);              \
                               ks=xct(p + 3);              \
                               is=p[5];                    \
                               hs=LRGENTHDRSZ;             \
                           }                               \
                           else                            \
                           {                               \
                               cs=p[0];                    \
                               ks=p[1];                    \
                               is=p[2];                    \
                               hs=ENTHDRSZ;                \
                           }

#define CS_KS_IS_SET(p,f,cs,ks,is) if (f)                       \
                                     {                            \
                                         *p = LRGKEYFORMAT;   \
                                         sct(p + 1,cs);	          \
                                         sct(p + 3,ks);	          \
                                         p[5]=is;                 \
                                     }                            \
                                     else                         \
                                     {                            \
                                         p[0]=cs;                 \
                                         p[1]=ks;                 \
                                         p[2]=is;                 \
                                     } 

#define CS_GET(p)      ( (*p == LRGKEYFORMAT) ? xct((p) + 1) : (p)[0] )
#define CS_PUT(p,cs)   if (*p == LRGKEYFORMAT)       \
                           sct(p + 1,cs);                \
                       else                              \
                           p[0]=cs;
#define KS_GET(p)      ( (*p == LRGKEYFORMAT) ? xct(p + 3) : p[1] )
#define KS_PUT(p,ks)   if (*p == LRGKEYFORMAT)       \
                           sct(p + 3,ks);                \
                       else                               \
                           p[1]=ks;
#define IS_GET(p)      ( (*p == LRGKEYFORMAT) ? p[5] : p[2] )
#define IS_PUT(p,is)   if (*p == LRGKEYFORMAT)    \
                           p[5]=is;                  \
                       else                           \
                           p[2]=is;
#define HS_GET(p)      ( (*p == LRGKEYFORMAT) ? LRGENTHDRSZ : ENTHDRSZ )

#define ENTRY_DBKEY(p) ( (*p == LRGKEYFORMAT)                     \
                         ? LRGENTHDRSZ + xct(p + 3) - 3 \
                         : ENTHDRSZ    + p[1]       - 3)

#define ENTRY_INDEX_NUMBER(p)  ( (*p == LRGKEYFORMAT) \
                                 ? LRGENTHDRSZ + 2     \
                                 : ENTHDRSZ + 2)
/* for cxput LRGENTHDRSZ needs to be CURSORHDRSZ!!! */
#define ENTRY_INDEX_NUMBER_GET(p)  ( xct( (TEXT*)((*p == LRGKEYFORMAT) \
                                           ? p + LRGENTHDRSZ + 2     \
                                           : p + ENTHDRSZ + 2) ))
/* for cxput LRGENTHDRSZ needs to be CURSORHDRSZ!!! */

#define ENTRY_SIZE_FULL(p)  ( (*p == LRGKEYFORMAT)                    \
                         ? xct(p + 1) + xct(p + 3) + p[5] + LRGENTHDRSZ   \
                         : p[0]       + p[1]       + p[2] + ENTHDRSZ)

#define ENTRY_SIZE_COMPRESSED(p)  ( (*p == LRGKEYFORMAT) \
                         ? xct(p + 3) + p[5] + LRGENTHDRSZ   \
                         : p[1]       + p[2] + ENTHDRSZ)

/*----------------------------------------------------------------*/
/*  Macroes for handling index entries in memory
    like keys in cursors, keys in EAP structure etc,
    where sizes are always two bytes for predictability        ***/
/*----------------------------------------------------------------*/

/* #endif   LARGE_INDEX_ENTRIES_DISABLED */ 
/************************************************/


/*************************************************************/
/* Private Function Prototypes for cx.c                      */
/*************************************************************/

int	cxGet		(dsmContext_t *pcontext, cxeap_t *peap);

int     cxGetDBK	(dsmContext_t *pcontext, TEXT *pos,  DBKEY *pdbkey);

int	cxNextPrevious	(dsmContext_t *pcontext, cxeap_t *peap);

int     cxPut           (dsmContext_t *pcontext, cxeap_t *peap, 
                         dsmObject_t tableNum);

int	cxRemoveEntry	(dsmContext_t *pcontext, 
                         dsmObject_t tableNum, findp_t *pfndp);  

int	cxRemove	(dsmContext_t *pcontext, 
                         dsmObject_t  tableNum, cxeap_t *peap);

int	cxSplit		(dsmContext_t *pcontext, cxeap_t *peap, findp_t *pfndp,
			 DOWNLIST *pdownlst, dsmObject_t tableNum,int tagonly);

int cxFindEntry    (dsmContext_t *pcontext, findp_t *pfndp);
int cxInsertEntry  (dsmContext_t *pcontext, findp_t *pfndp, 
                    dsmObject_t tableNum, int tagonly);
int cxReleaseAbove (dsmContext_t *pcontext, downlist_t *pdownlst);
int cxReleaseBelow (dsmContext_t *pcontext, downlist_t *pdownlst);
int cxSkipEntry    (dsmContext_t *pcontext, findp_t *pfndp, TEXT *pos,
                         TEXT *nextpos, TEXT *poskey);
int cxRemoveBlock (dsmContext_t *pcontext, cxeap_t *peap, findp_t *pfndp,
                          dsmObject_t tableNum, downlist_t *pdownlst);


/*************************************************************/
/* Private Function Prototypes for cxaux.c                   */
/*************************************************************/
int     cxRemoveFromDeleteChain
			(dsmContext_t *pcontext, 
                         dsmObject_t   tableNum, ULONG area ); 

int	cxFreeBlock	(dsmContext_t *pcontext, CXBAP *pbap);

int	cxGetNewBlock	(dsmContext_t *pcontext, dsmTable_t table,
			 dsmIndex_t index,
			 ULONG area, CXBAP *pbap);


DSMVOID
cxProcessDeleteChain( dsmContext_t *pcontext, 
                      dsmObject_t  tableNum, dsmArea_t area);


/*************************************************************/
/* Private Function Prototypes for cxdo.c                    */
/*************************************************************/

int     cxDeleteBlock   (dsmContext_t *pcontext, bmBufHandle_t bufHandle,
			 UCOUNT mode, DBKEY rootdbk);

int	cxDoSplit	(dsmContext_t *pcontext, 
			 dsmTable_t  table, FINDP *pfndp,
			 CXBAP *pbap, int leftents);

/*************************************************************/
/* Private Function Prototypes for cxfil.c                   */
/*************************************************************/

DBKEY    cxCreateMaster (dsmContext_t *pcontext, ULONG area,
			  DBKEY olddbk, COUNT ixnum, GTBOOL ixbottom);

/*************************************************************/
/* Private Function Prototypes for cxgen.c                   */
/*************************************************************/

int	cxAddNL		(dsmContext_t *pcontext, dsmKey_t *pkey,
			 DBKEY dbkey, TEXT *pname, 
                         dsmObject_t  tableNum);

int	cxDelete	(dsmKey_t *pkey, struct lockId *plockId, int lockmode,
			 TEXT *pname);

int	cxDeleteNL	(dsmContext_t *pcontext, dsmKey_t *pkey,
			 DBKEY dbkey, int lockmode, 
                         dsmObject_t  tableNum, TEXT *pname );

int     cxKeyPrepare    (dsmContext_t *pcontext, dsmKey_t *pkey,
			 CXEAP *peap, DBKEY dbkey);

LONG    cxCheckLocks    (dsmContext_t *pcontext, bmBufHandle_t bufHandle);

LONG    cxDelLocks      (dsmContext_t *pcontext, bmBufHandle_t bufHandle,
                         dsmObject_t   tableNum, int upGrade);

/*************************************************************/
/* Private Function Prototypes for cxkill.c                  */
/*************************************************************/

DSMVOID    cxUnKill        (dsmContext_t *pcontext, RLNOTE *pnote,
			 COUNT dlen, TEXT *pdata);

/*************************************************************/
/* Private Function Prototypes for cxky.c                    */
/*************************************************************/

int	cxIxKeyCompare	(dsmContext_t *pcontext,
			 TEXT *pn, int ln, TEXT *po, int lo);

int	cxNewKeyCompare	(TEXT *pkey1, int lenkey1, TEXT *pkey2, int lenkey2);

int	cxKeyCompare	(TEXT *t, int lt, TEXT *s, int ls);

int	cxKeyFlag	(TEXT *pkey);

int	cxKeyToDitem	(DITEM *pkditem, TEXT *pkey);

int	cxKeyToStruct	(dsmKey_t *pkkey, TEXT *pkey);

int	cxKeyUnknown	(TEXT *pnew);

/*************************************************************/
/* Private Function Prototypes for cxnxt.c                   */
/*************************************************************/

int	cxSetCursor	(dsmContext_t *pcontext, CXEAP *peap,
 			 COUNT entlen, int cs);

/*************************************************************/
/* Private Function Prototypes for cxtag.c                   */
/*************************************************************/

DSMVOID	cxAddBit	(TEXT *plist, TEXT tagbyte);

DSMVOID	cxDeleteBit	(TEXT *plist, TEXT tagbyte);

DSMVOID	cxToList	(TEXT *pos);

DSMVOID	cxToMap		(TEXT *pos);

/*************************************************************/
/* Private Function Prototypes for cxblock.c                 */
/*************************************************************/

int cxCheckBlock        (dsmContext_t *pcontext, cxblock_t *pixblk );

int
cxGetIndexBlock(
        dsmContext_t    *pcontext,
        cxbap_t         *pbap,          /* blk access parameters */
        ULONG            area,          /* area blk is in */
        DBKEY            dbkey,         /* dbkey of blk */
        UCOUNT           lockmode);     /* mode to lock blk buffer */


/*************************************************************/
/* Private Function Prototypes for cxvsi.c                   */
/*************************************************************/

#ifdef VST_ENABLED
int cxFindVirtual(dsmContext_t *pcontext, CXCURSOR *pcursor, DBKEY *pdbkey, 
		  dsmKey_t *pdlo_key, dsmKey_t *pdhi_key, 
		  int fndmode, int lockmode);

int cxNextVirtual(dsmContext_t *pcontext, CXCURSOR *pcursor, DBKEY *pdbkey, 
		  dsmKey_t *pdlo_key, dsmKey_t *pdhi_key, 
		  int fndmode, int lockmode);
#endif  /* VST ENABLED */


DBKEY
cxGetRootAndArea( dsmContext_t *pcontext,dsmObject_t tableNum,
                cxeap_t      *peap );
 
bmBufHandle_t
cxLocateValidBlock( dsmContext_t    *pcontext,
                   cxeap_t         *peap,
                   DBKEY           *dbkey,
                   downlist_t      *pdownlst,
                   dsmObject_t      tableNum,
                   int             action);



#endif  /* #ifndef CXPRV_H */
