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

#ifndef CXPUB_H
#define CXPUB_H

/**********************************************************/
/* cxpub.h - Public function prototypes for CX subsystem. */
/**********************************************************/

#include "rlpub.h"
#include "dsmpub.h"
#include "keypub.h"  /* for key* prototypes and other key-stuff */


/***************************************************/
/***  declares for structures taken from ixctl.h ***/
/***************************************************/
struct fldtbl;

struct lockId;

/*************************************************************/
/* Public definitions and data structures                    */
/*************************************************************/

/* flags for cxKeyLimit(), defining the ditem type */
#define CX_LOWRANGE     1
#define CX_HIGHRANGE    2

#define CX_WILD         4               /* wildcard query */

#define ENTHDRSZ        3  /* Size of the entry header - 3 bytes: cs, ks, is */

/* down list initializer */
#define CXINITLV(pdownlst)      pdownlst->currlev = pdownlst->locklev = 0

/* used by index compact utility */
#define IDXBLK_MAX_UTILIZATION     100
#define IDXBLK_MIN_UTILIZATION      50
#define IDXBLK_DEFAULT_UTILIZATION  80

/************************************************/
/**  definitions for large index entries stuff **/
/************************************************/

#define LRGKEYSIZE      MAXKEYSZ /* largest possible key size */

#define LRGENTHDRSZ     6  /* size of an index entry header in case of a
				large key.  The index entry header in this case
                                cosists of:
                           1 byte -  with value LRGKEYFORMAT
                           2 bytes - for "cs" in UCOUNT stored with sct
                           2 bytes - for "ks" in UCOUNT stored with sct
                           1 byte -  for "is" in as TEXT      */

#define TS_OFFSET   0  /* offset to total size of the full key */
#define KS_OFFSET   2  /* offset to Key size of the full key */
#define IS_OFFSET   4  /* offset to Info size of the full key */


/* saved query message to be part of a query cursor @@*/
#include "axppack4.h"
typedef struct qrmsg {
        struct rnfind   *prnfind;       /* for selection by the server */
        struct dbmsg    *pdbmsg;        /* to saved msg header */
        int             msgsize;
        UCOUNT          qrmsgflags;     /* see definitions */
#if LONGPTR
        void            *extra;         /* extra storage req'd for LONGPTR */
#endif
        } QRMSG;

/* qrmsg.qrmsgflags definitions */
#define CX_QBWDUPS      1       /* query contains a QBW */

/* Index formats */
#define LRGKEYFORMAT 255    /* large key format, value that V8 CS will never get*/
#define MAXSMALLKEY  200    /* maximum size of a small key */

/************cx cursor ***************/
/* cursor - all cursor in cx are private and 1 level */
typedef struct cxcursor
{
        TEXT    *pkey;          /* pointer to the current key */
        struct dbcontext *pdbcontext;
        ULONG   area;           /* area the index is in       */
        LONG    table;          /* table the index is on      */
        ULONG   tableArea;      /* Area of the table the index is on */
        DBKEY   root;           /* Dbkey of the root block of the index.  */
        LONG    level;
        DBKEY   blknum;
        LONG64  updctr;
        COUNT   cxusrnum;       /* usrctl number */
        COUNT   ixnum;
        UCOUNT  offset;
        UCOUNT  flags;
        UCOUNT  kyareasz;       /* size of the area allocated for the key */
        UCOUNT  substrsz;       /* used for BEGINS and USING (with ABBRV.)*/
        dsmBoolean_t    unique_index;   /* substring ok bit */
        dsmBoolean_t    word_index;     /* is this a word index */
        dsmBoolean_t    multi_index;    /* Index on multiple classes/tables */
} cxcursor_t;
#include "axppackd.h"

#define NUM_CURSORS 32  /* allocate cursors in groups of 32 */
typedef struct cxCursorList
{
        struct cxCursorList *pnextCursorList;  /* ptr to next list of cursors */
        cxcursor_t     cursorEntry[NUM_CURSORS];
}  cxCursorList_t;

#define CXCURSOR        struct cxcursor

/* flags in CXCURSOR */
#define CXINUSE         1       /* cursor is in use */
#define CXBACK          2       /* cursor moved back */
#define CXFORWD         4       /* cursor moved forward */
#define CXAUXCURS       8       /* auxiliary cursor, for ambiguity checking */
#define CXNEWCURS       16      /* cursor not yet known to the client */
#define CXNOCURS        32      /* temp cursor, used when nocursor is on */

/* number of cursors */
#define MINCURS         10
#define MAXCURS         999     /* max for non-shared memory only */
#define MAXCURSENT      20      /* absolute maximum value for cursor depth */
#define DFTCURSENT      8       /* default value for cursor depth */
#define DFTCURSS        20      /* default for single user */
#define DFTCURSM        60      /* default for multi  user */
#define DFTCURSPUSR     4       /* default # cursors per user */

#define IXDUPKEY         1      /* from ixgen if key already exists     */
#define IXINCKEY        -2      /* from ixgen or ixfnd if incomplete    */
                                /* key was passed                       */
/* mode parms for ixfnd */
#define FINDFRST        1       /* find the first match and return it   */
#define FINDSHRT        4       /* not all comps supplied               */
#define FINDGAP         8       /* find ent bfr 1st match,              */
                                /* ixfld uses FINDFRST+FINDGAP          */

/* lockmode parm for ixdel */
#define IXNOLOCK        0
#define IXLOCK          1
#define IXROLLF         4       /* logical IXDEL during roll forward.
                                   Put a lock on the index entry but don't
                                   delete it yet. */

/* cxCompactProcessParent() process request types */
#define CX_BLOCK_COMPACT   1
#define CX_BLOCK_REPORT    2

/**************************** CURSID value macros ***************************/
/* check whether the cursor is a QR cursor */
#define ISQRCURS(cursid)        (((cursid) & 0X4000) != 0)

/* mark a CURSID as a QR cursor */
#define MKQRCURS(cursid)        ((cursid) | 0X4000)

/* remove the QR cursor mark from a cursor */
#define UMKQRCURS(cursid)       ((cursid) & 0X3FFF)

/* return-codes for cxkyut.c functions */
typedef int  cxStatus_t;      /* type for status return from procedures 
                               * (for now it's int but should change to LONG
                               * during the cleanup...  (th) )
                               */

#define CX_BAD_PARAMETER    -1
#define CX_BUFFER_TOO_SMALL -2  /* buffer tohold key-string is too small */
#define CX_KYILLEGAL        -KEY_ILLEGAL


/*************************************************************/
/* Public Function Prototypes for cxfil.c                    */
/*************************************************************/

int	 cxCreateIndex	(dsmContext_t *pcontext, int ixunique, int ixnum,
			 ULONG area, 
                         dsmObject_t   tableNum,
                         DBKEY *pobjectRoot);

int	 cxGetUnique	(COUNT ixnum);

/*************************************************************/
/* Public Function Prototypes for cxgen.c                    */
/*************************************************************/

int	cxAddEntry	(dsmContext_t *pcontext, dsmKey_t *pkey,
			 struct lockId *plockId, TEXT *pname, 
                         dsmObject_t tableNum);

int	cxCheckEntries	(dsmContext_t *pcontext, int ixnum);

int	cxDeleteEntry	(dsmContext_t *pcontext, dsmKey_t *pkey,
			 struct lockId *plockId, int lockmode, 
                         dsmObject_t tableNum, TEXT *pname);

DSMVOID	cxDeleteLocks	(DSMVOID);

DSMVOID    cxIxrprSet(DSMVOID);

GBOOL    cxIxrprGet(DSMVOID);

LONG cxLogIndexCreate(dsmContext_t *pcontext, dsmArea_t area,
                      dsmIndex_t indexNumber,dsmTable_t tableNumber);

/*************************************************************/
/* Public Function Prototypes for cxkill.c                   */
/*************************************************************/

DSMVOID	cxKill		(dsmContext_t *pcontext, dsmArea_t area,
			 dsmDbkey_t rootDbkey, COUNT ixnum, int delrecs,
                         dsmObject_t tableNum, int tableArea);

/*************************************************************/
/* Public Function Prototypes for cxkyut.c                   */
/*************************************************************/

keyStatus_t cxConvertToIx       (TEXT *pt, TEXT *ps);

keyStatus_t cxDitemToStruct     (DITEM *pkditem, dsmKey_t *pkkey);

keyStatus_t cxWordDitemToStruct (DITEM *pkditem, dsmKey_t *pkkey); 

/*************************************************************/
/* Public Function Prototypes for cxky.c                     */
/*************************************************************/

int	cxIxKeyUnknown	(DITEM *poldkey);

DSMVOID	cxKeyLimit	(struct ditem *pnew, struct ditem *pold,
	                 TEXT *pkey, int flag);

DSMVOID	cxKeyLimitNew	(dsmKey_t *pnew, dsmKey_t *pold,
	                 TEXT *pkey, int flag);

int     cxKeyTruncate   (TTINY *pw, int maxkey);

int     cxConvertMode   (int mode, int *type);

/*************************************************************/
/* Public Function Prototypes for cxnxt.c                    */
/*************************************************************/

DSMVOID	cxBackUp	(dsmContext_t *pcontext, CURSID cursid);
 
DSMVOID	cxDeactivateCursor(dsmContext_t *pcontext, CURSID *pcursid);

DSMVOID	cxDeaCursors	(dsmContext_t *pcontext);

DBKEY
cxDsmKeyComponentToLong( TEXT *pdsmKeyStr );

int     cxFind		(dsmContext_t *pcontext, dsmKey_t *pkey_Base,
			 dsmKey_t *pkey_Limit, CURSID *pcursid, 
			 LONG     table,
			 dsmRecid_t *pdbkey, dsmMask_t fndType,
			 dsmMask_t fndmode, dsmMask_t lockmode);

int	cxFindFields	(dsmContext_t *pcontext, dsmKey_t *pkey,
			 struct fldtbl *pfldtbl, dsmRecid_t filedbk);

int	cxFindGE	(dsmContext_t *pcontext,
			 CURSID *pcursid,
                         LONG table, dsmRecid_t *pdbkey, 
			 dsmKey_t *pdlo_key, dsmKey_t *pdhi_key,
			 int lockmode);

DSMVOID	cxForward	(dsmContext_t *pcontext, CURSID cursid);

DSMVOID	cxFreeCursors	(dsmContext_t *pcontext);

DSMVOID	cxGetAuxCursor	(dsmContext_t *pcontext, CURSID *pcursid);

CXCURSOR * cxGetCursor	(dsmContext_t *pcontext, CURSID *pcursid, COUNT ixnum);

int	cxNext		(dsmContext_t *pcontext,
			 CURSID *pcursid,
                         LONG table, dsmRecid_t *pdbkey,
			 dsmKey_t *pdlo_key, dsmKey_t *pdhi_key,
			 dsmMask_t fndType, dsmMask_t fndMode,
			 int lockmode);

/*************************************************************/
/* Public Function Prototypes for cxtag.c                    */
/*************************************************************/

int	cxFindTag	(TEXT *pent, TEXT tagbyte, int op);

/*************************************************************/
/* Public Function Prototypes for cxundo.c                   */
/*************************************************************/

DSMVOID	cxUndo	 	(dsmContext_t *pcontext, RLNOTE *pnote,
			 COUNT dlen,  TEXT *pdata);

/*************************************************************/
/* Public Function Prototypes for cxblock.c                  */
/*************************************************************/

int cxCheckRoot		(dsmContext_t *pcontext, int ixnum );

dsmStatus_t
cxGetIndexInfo(dsmContext_t *pContext, dsmObject_t tableNum, dsmKey_t *pKey);

int cxVerifyIndexRoots(dsmContext_t *pcontext, dsmText_t    *pname, 
                   LONG         *proot, LONG         *pindexNum,
                   dsmArea_t     objArea);

/*************************************************************/
/* Public Function Prototypes for cxcompact.c                */
/*************************************************************/

dsmStatus_t
cxCompactIndex(
        dsmContext_t  *pcontext,           /* IN database context */
        dsmCursid_t   *pcursid,            /* IN  pointer to cursor */
        ULONG          percentUtilization, /* IN % required space util. */
#ifdef DBKEY64
        psc_long64_t  *pblockCounter,      /* OUT # of blocks processed */
        psc_long64_t  *ptotalBytes,        /* OUT # of bytes utilized */
#else
        LONG          *pblockCounter,      /* OUT # of blocks processed */
        LONG          *ptotalBytes,        /* OUT # of bytes utilized */
#endif  /* DBKEY64 */
        dsmObject_t    tableNum,
        LONG           processRequest);    /* compact blks or blk report ? */

float
cxEstimateTree(
        dsmContext_t    *pcontext,
        TEXT            *pBaseKey,
        int              baseKeySize,
        TEXT            *pLimitKey,
        int              limitKeySize,
        ULONG            area,    /* the db area where the index is located */
        DBKEY            rootDBKEY         /* pointer to the root block */
        );

/*************************************************************/
/* Public Function Prototypes for cx.c                       */
/*************************************************************/

#if 0
/* obsolete 1-19-98 */
DSMVOID cxSetixparams	(dsmContext_t *pcontext, UCOUNT block_size,
			 COUNT *pixtbllen,
			 COUNT *pitblshft, COUNT *pitblmask,
			 UCOUNT *pmaxidxs);
#endif

int cxEntryAttr         (TEXT  *pIxEntry,   /* IN - must point to 1st byte of the index entry */
                        int    *pCS,        /* OUT - pointer to returned CS */
                        int    *pKS,        /* OUT - pointer to returned KS */
                        int    *pIS);       /* OUT - pointer to returned IS */
       
int cxEntrySize         (TEXT  *pIxEntry);   /* IN - must point to 1st byte of the index entry */

/*************************************************************/
/* Public Function Prototypes for cxmove.c                   */
/*************************************************************/

DBKEY
cxCopyIndex(dsmContext_t *pcontext,
            DBKEY        olddbk,      
            ULONG        sourceArea,  
            ULONG        targetArea);

DSMVOID
cxUndoBlockCopy( dsmContext_t *pcontext,
             RLNOTE       *pnote,
             COUNT        dlen,
             TEXT         *pdata);

dsmStatus_t 
cxIndexAnchorUpdate(
            dsmContext_t       *pcontext,   /* IN database context */
            dsmArea_t          area,        /* IN area */
            dsmObject_t        object,      /* IN  for object id */
            dsmObjectType_t    objectType,  /* IN object type */
            dsmObject_t        associate,   /* IN object number to use
                                               for sequential scan of tables*/
            dsmObjectType_t    associateType, /* IN object type of associate
                                                 usually an index           */

            dsmDbkey_t         rootBlock,   /* IN dbkey of object root
                                             * - for INDEX: index root block
                                             * - 1st possible rec in the table.
                                             */
            dsmMask_t           pupdateMask);/* bit mask identifying which
                                                 storage object attributes are
                                                 to be modified. */



#endif  /* #ifndef CXPUB_H */
