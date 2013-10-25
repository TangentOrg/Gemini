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

#ifndef BKPUB_H
#define BKPUB_H

/**********************************************************/
/* bkpub.h - Public function prototypes for BK subsystem. */
/**********************************************************/

#include "dsmpub.h"

struct bkAreaDesc;
struct bkbuf;
struct bkftbl;
struct bktbl;
struct dbcontext;
struct mstrblk;
struct extentBlock;


/*********************************************************/
/* Public structures & definitions for the  bk subsystem */

/* values for bkhdr.bk_frchn to indicate whether or not a block is
   on a chain, and if so, which one. */
#define FREECHN 0
#define RMCHN   1
#define LOCKCHN 2
#define NOCHN   127


#define BMBUM_H
#include "bmctl.h"
#undef BMBUM_H

#define BKBUM_H
#include "bkmgr.h"
#include "bmpub.h"
#include "bkchk.h"
#include "bkflst.h"
#include "bkiopub.h"
#include "bkftbl.h"
#include "bkaread.h"
#include "bkmsg.h"
#undef BKBUM_H

#include "mstrblk.h"

#ifndef BKFDTBL
#define BKFDTBL struct bkfdtbl
#endif

typedef struct bkfdtbl
{
    ULONG        numused;
    ULONG        maxEntries;
    bkiotbl_t   *pbkiotbl[1];
} bkfdtbl_t;

#define BKFDSIZE(x) (sizeof(BKFDTBL) + ((x)-1)*sizeof(bkiotbl_t *))

#if 0
typedef struct _bkNumFiles
{
    COUNT     numFiles[BKETYPE + 1];
}BK_NUM_FILES;
#endif


/* passed as argument to indicate type of checking needed in call */
#define BK_PRE_CHECK 1  /* check before change */
#define BK_END_CHECK 2  /* check after change */
#define BK_ALL_CHECK 3  /* check before and after (PRE+END) */


/*********************************************************/
/* Public Function Prototypes for bkrec.c                */
/*********************************************************/
int   bkBuildExtentTable
			(dsmContext_t *pcontext, LONG mode,
			 int areaType, ULONG area,
			 struct bkAreaDesc **pbkAreaDesc,
			 int numFiles, bkflst_t *pFileList,
			 struct extentBlock *pextentBlock,
			 struct mstrblk     *pcontrolBlock);

DSMVOID    bkclear        	(dsmContext_t *pcontext, struct bkbuf *pbkbuf);

DSMVOID	bkExtend	(dsmContext_t *pcontext, ULONG area,
			 bmBufHandle_t objHandle);

DSMVOID    bkfradd         (dsmContext_t *pcontext, bmBufHandle_t bufHandle,
			 int chain);
bmBufHandle_t bkfrrem   (dsmContext_t *pcontext, ULONG area,
			 int newowner, int chain);
DSMVOID    bkrmbot         (dsmContext_t *pcontext, bmBufHandle_t bufHandle);
DSMVOID    bkrmrem         (dsmContext_t *pcontext, bmBufHandle_t bufHandle);
DSMVOID    bktoend         (dsmContext_t *pcontext, bmBufHandle_t bufHandle);

LONG    bkGetTotBlks    (dsmContext_t *pcontext);
DSMVOID    bkPutMinorV	(dsmContext_t *pcontext, ULONG  mvers);
DSMVOID    bkGetMinorV	(dsmContext_t *pcontext, ULONG *pmvers);

/*********************************************************/
/* Public Function Prototypes for bkrepl.c               */
/*********************************************************/

DSMVOID    bkReplace	(dsmContext_t *pcontext, bmBufHandle_t bufHandle,
			 TEXT *pold, TEXT *pnew, COUNT len);

/********************************************************/
/* Public Function Prototypes for bksubs.c              */
/********************************************************/

dsmStatus_t
	 bkExtentCreate	(dsmContext_t *pcontext, struct bkAreaDesc *pbkArea,
			 dsmSize_t *pinitialSize, GTBOOL extentType,
			 dsmText_t *pname);

DSMVOID    bkaddr          (dsmContext_t *pcontext, struct bktbl *pbktbl,
                         int numblocks);
DSMVOID    bkCreate        (dsmContext_t *pcontext, struct bktbl *pbktbl);
LONG    bkflen          (dsmContext_t *pcontext, ULONG area);
int     bkFindAreaName  (dsmContext_t *pcontext, struct mstrblk
                         *pcontrolBlock, TEXT *areaName, ULONG *pareaNumber,
                         DBKEY *pdbkey, ULONG *poutSize);
DSMVOID    bkRead          (dsmContext_t *pcontext, struct bktbl *pbktbl);
DSMVOID    bktrunc         (dsmContext_t *pcontext, ULONG area);
DSMVOID    bkWrite         (dsmContext_t *pcontext, struct bktbl *pbktbl,
			 int buffmode);
LONG    bkxtn           (dsmContext_t *pcontext, ULONG area, LONG blocksWanted);
DSMVOID    bkxtnfree       (dsmContext_t *pcontext);
LONG	bkGetAreaFtbl	(dsmContext_t *pcontext, ULONG area,
			 struct bkftbl **pbkftbl);

typedef struct bkRowCountUpdate
{
  RLNOTE    rlnote;
  dsmTable_t table;
  TEXT      addIt;
}bkRowCountUpdate_t;

DSMVOID
bkRowCountUpdate(dsmContext_t *pcontext, dsmArea_t areaNumber, 
		 dsmTable_t  table, int addIt);

dsmStatus_t bkExtentUnlink(dsmContext_t *pcontext);
int         bkExtentRename(dsmContext_t *pcontext, 
                           dsmArea_t areaNumber, 
                           dsmText_t *pNewName);
DSMVOID
bkSyncFiles(dsmContext_t *pcontext);

#if NUXI==0   
/* Big Endian platform (high order byte has lowest address) */
DSMVOID bkFromStore(bkhdr_t *pblk);
#define BK_FROM_STORE(pblk) bkFromStore(pblk)
DSMVOID bkToStore(bkhdr_t *pblk);
#define BK_TO_STORE(pblk) bkToStore(pblk)
#else
/* Default is little endian (low order byte has lowest address) */
#define BK_FROM_STORE(pblk)
#define BK_TO_STORE(pblk)
#endif

/*********************************************************/
/* Public Function Prototypes for bkcheck.c              */
/*********************************************************/

#if !NW386
DSMVOID    bkverify         (TEXT *whocalled, struct dbcontext *pdbcontext);
#endif /* !NW386 */

#if BKCHK
DSMVOID    bkdfdtbl         (struct bkfdtbl *pbkfdtbl);
DSMVOID    bkdftbl          (struct bkftbl *pbkftbl, int ftype, COUNT extents,
                          struct mstrblk *pmstr);
DSMVOID    bkvftbl          (struct bkftbl *pbkftbl, int ftype, COUNT extents,
                          struct mstrblk *pmstr);
DSMVOID    bkvnormf         (struct bkftbl *pbkftbl, int ftype, COUNT extents,
                          COUNT curextent, struct mstrblk *pmstr);
DSMVOID    bkvsyncf         (struct bkftbl *pbkftbl, int ftype, COUNT extents,
                          COUNT curextent, struct mstrblk *pmstr);
DSMVOID    bkvfdio          (int wantio, int fd);
DSMVOID    bkvfiof          (int wantio, TEXT haveio);
DSMVOID    bkvfdlen         (LONG len, int fd);
DSMVOID    bkdftype         (int ftype, TEXT *pmisc);
#endif /* BKCHK */

/*********************************************************/
/* Public Function Prototypes for bkopen.c               */
/*********************************************************/

#define BK_RDMB_SUCCESS       0
#define BK_RDMB_OPEN_FAILED  -1
#define BK_RDMB_BAD_VERSION  -2
#define BK_RDMB_READ_FAILED   1

#define BK_WRMB_SUCCESS       0
#define BK_WRMB_OPEN_FAILED  -1
#define BK_WRMB_BAD_VERSION  -2
#define BK_WRMB_WRITE_FAILED  1

#if !NW386
DSMVOID    bksetc           (DSMVOID);
#endif /* !NW386 */

LONG    bkblklog         (LONG blksize);
int     bkCheckBlocksize (dsmContext_t *pcontext, int blockSize, int checkType,
                          struct bkftbl *pbkftbl, TEXT *dbname);
int	bkInitIoTable	 (dsmContext_t *pcontext, int numneeded);
int	bkInitFdTable	 (int numneeded);

DSMVOID    bkClose			(dsmContext_t *pcontext);
int     bkFixMasterBlock	(dsmContext_t *pcontext, struct mstrblk *pmstr);
int     bkInvalidateIndexes	(dsmContext_t *pcontext);
int     bkOpenFilesBuffered	(dsmContext_t *pcontext, GTBOOL readOnly,
				 struct bkAreaDesc *pbkAreaDesc, 
                                 struct mstrblk *pmstr);
int     bkOpenFilesUnBuffered	(dsmContext_t *pcontext,
				 struct bkAreaDesc *pbkAreaDesc);
int	bkGetDbBlockSize	(dsmContext_t *pcontext, 
				 fileHandle_t fd, TEXT *dbname);
int     bkReadMasterBlock	(dsmContext_t *pcontext, TEXT *dbname,
				 struct mstrblk *pmstr, 
                                 fileHandle_t dbfd);
int     bkWriteMasterBlock      (dsmContext_t *pcontext, TEXT *dbname,
                                 struct mstrblk *pmstr,
                                 fileHandle_t dbfd);
int     bkOpen			(dsmContext_t *pcontext, LONG mode);
int     bkOpen2ndUser		(dsmContext_t *pcontext);
int     bkTlFileTable		(struct dbcontext *pdbcontext);
DSMVOID bkModifyArea	        (dsmContext_t *pcontext, ULONG area);
DSMVOID bkCloseAreas	        (dsmContext_t *pcontext);

int         bkAreaClose(dsmContext_t *pcontext, dsmArea_t area);

int         bkAreaOpen(dsmContext_t *pcontext, dsmArea_t area, int refresh);

#if OPSYS==UNIX
LONG    bkGetIPC		(dsmContext_t *pcontext, TEXT *pdbname,
				 bkIPCData_t *pbkIPCData);
LONG    bkPutIPC		(dsmContext_t *pcontext, TEXT *pdbname,
				 bkIPCData_t *pbkIPCData);
#endif

DSMVOID   bkRemoveAreaRecords(dsmContext_t *pcontext);

DSMVOID
bkDeleteTempObject(dsmContext_t *pcontext, DBKEY firstBlock, DBKEY lastBlock,
		   LONG numBlocks);

/*********************************************************/
/* Public Function Prototypes for bklg.c                 */
/*********************************************************/
/* TODO: Some of the public function proto's are still located
 * in dbpub.h
 */
int     dbLogAddPrefix   (dsmContext_t *pcontext, TEXT *pmsg);
int     gwLogAddPrefix   (dsmContext_t *pcontext, TEXT *pmsg);

#endif /* ifndef BKPUB_H  */
