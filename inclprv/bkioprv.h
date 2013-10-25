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

#ifndef BKIOPRV_H
#define BKIOPRV_H

/************************************************************/
/*  Private macros for bkio.c                               */
/************************************************************/

#include "bkiopub.h"
#include "dbutpub.h"

struct dsmContext;

/* What is the correct RELIABLE OPEN flag for this platform */
#if HPUX && defined(O_DSYNC)
#define BKIO_OSYNC O_DSYNC
#else
#define BKIO_OSYNC O_SYNC
#endif

/* bkiotbl slots are not maintained in a linked list.  There are several
   that get thrown away to due to opening, closing, and reopening again
   with different open flags.  BKIO_TABLE_SLOTS are MAXBKFDS + 5.
*/
#define BKIO_TABLE_SLOTS   261


/***************************************************************/
/* bkioprv.h - PRIVATE function prototypes for bkio subsystem. */
/***************************************************************/


/* NOTE: The parameter type and order of BKIO_PROTOTYPE and
 *       BKIO_OPEN_PROTOTYPE must be the same!
 */
#define BKIO_PROTOTYPE                                        \
            struct dsmContext *pcontext, bkiotbl_t *pbkiotbl, \
            bkiostats_t *bkiostats, gem_off_t offset,              \
            TEXT *pbuf, int size, int blklog

/* TODO: size and blklog should probably be ULONG */

#define BKIO_OPEN_PROTOTYPE                                   \
            struct dsmContext *pcontext, bkiotbl_t *pbkiotbl, \
            bkiostats_t *bkiostats, gem_off_t fileIOflags,         \
            TEXT *fileName, int openFlags, int createPermissions

/* Each file will have a bkiotbl associated with it that holds
 * information about how to do buffered and unbuffered reads/writes
 * as well as other information.  The bkftbl structre will have
 * an index to the correct for that particular file
 */
struct bkiotbl
{
    TEXT   fname[MAXUTFLEN+1]; /* file name */
    fileHandle_t   buffered_filePointer;    /* file desc for bufferred I/O  */
    fileHandle_t   unbuffered_filePointer;  /* file desc for unbuffered I/O */

    LONG   (*bufwrite)(BKIO_PROTOTYPE);     /* function for buffered writes */
    LONG   (*bufread)(BKIO_PROTOTYPE);      /* function for buffered reads */
    LONG   (*unbufwrite)(BKIO_PROTOTYPE);   /* function for unbuffered writes */
    LONG   (*unbufread)(BKIO_PROTOTYPE);    /* function for unbuffered reads */
    LONG   (*file_open)(BKIO_OPEN_PROTOTYPE);/* function for opening fd/handle */
};


/*************** FUNCTION PROTOTYPES FOR bkio.c **************/

LONG    bkioWriteBuffered   (BKIO_PROTOTYPE);
LONG    bkioReadBuffered    (BKIO_PROTOTYPE);
LONG    bkioWriteUnBuffered (BKIO_PROTOTYPE);
LONG    bkioReadUnBuffered  (BKIO_PROTOTYPE);
int     bkioTestLocalFile   (struct dsmContext *pcontext,
                             TEXT *pathname, TEXT *parent_dir);

#if DG5225
LONG    bkioWriteDirectDGUX (BKIO_PROTOTYPE);
LONG    bkioReadDirectDGUX  (BKIO_PROTOTYPE);
LONG    bkioOpenDGUX        (BKIO_OPEN_PROTOTYPE);
#ifdef BKIOFUNCTIONS
LONG    (*(bkioFunctions[BKIO_LAST_FUNCTION]))(BKIO_PROTOTYPE) =
                {
                 bkioWriteBuffered,
                 bkioReadBuffered,
                 bkioWriteUnBuffered,
                 bkioReadUnBuffered,
                 bkioOpenDGUX,
                };

#endif /* BKIOFUNCTIONS */
#endif

#if OPSYS==WIN32API
LONG    bkioWriteUnBufferedWIN32 (BKIO_PROTOTYPE);
LONG    bkioReadUnBufferedWIN32  (BKIO_PROTOTYPE);
LONG    bkioWriteBufferedWIN32   (BKIO_PROTOTYPE);
LONG    bkioReadBufferedWIN32    (BKIO_PROTOTYPE);
LONG    bkioOpenWIN32            (BKIO_OPEN_PROTOTYPE);
#ifdef BKIOFUNCTIONS
LOCALF LONG    (*(bkioFunctions[BKIO_LAST_FUNCTION]))(BKIO_PROTOTYPE) =
                {
                 bkioWriteBufferedWIN32,
                 bkioReadBufferedWIN32,
                 bkioWriteUnBufferedWIN32,
                 bkioReadUnBufferedWIN32,
                 bkioOpenWIN32,
                };
#endif   /* BKIOFUNCTIONS */
#endif

#if OPSYS==UNIX
#if !DG5225 
LONG    bkioOpenUNIX (BKIO_OPEN_PROTOTYPE);
#endif
#if (!DG5225 && !SEQUENTPTX)
#ifdef BKIOFUNCTIONS
LOCALF LONG (*(bkioFunctions[BKIO_LAST_FUNCTION]))(BKIO_PROTOTYPE) =
          {
              bkioWriteBuffered,
              bkioReadBuffered,
              bkioWriteUnBuffered,
              bkioReadUnBuffered,
              bkioOpenUNIX,
          };
#endif   /* BKIOFUNCTIONS */
#endif
#endif

#if SEQUENTPTX
LONG    bkioWriteDirectPTX  (BKIO_PROTOTYPE);
LONG    bkioReadDirectPTX   (BKIO_PROTOTYPE);
#ifdef BKIOFUNCTIONS
LONG    (*(bkioFunctions[BKIO_LAST_FUNCTION]))(BKIO_PROTOTYPE) =
                {
                 bkioWriteBuffered,
                 bkioReadBuffered,
                 bkioWriteDirectPTX,
                 bkioReadDirectPTX,
                 bkioOpenUNIX,
                };
#endif   /* BKIOFUNCTIONS */
#endif

#if OPSYS == VMS
LOCALF LONG    bkioWriteDirectVMS (BKIO_PROTOTYPE);
LOCALF LONG    bkioReadDirectVMS  (BKIO_PROTOTYPE);
        int    bkioOpenVMS        (BKIO_PROTOTYPE);
#ifdef BKIOFUNCTIONS
LOCALF LONG    (*(bkioFunctions[BKIO_LAST_FUNCTION]))(BKIO_PROTOTYPE) =
		{
		 bkioWriteBuffered,
		 bkioReadBuffered,
		 bkioWriteDirectVMS,
		 bkioReadDirectVMS,
		 bkioOpenVMS,
		 bkioClose,
		 bkioSync,
		};
#endif
#endif
/*************** STRUCTURES FOR bkiolk.c **************/

/*length of node name stored in .lk file for BSD machines*/
#define BKIO_NODESZ 30


#if OPSYS==VMS
/* declare structure for lock status block                      */
struct  lksb    {
                short   cond_value;
                short   reserved;
                int     lock_id;
                char    lock_val_blk[16];
                } ;
#endif
#endif  /* BKIOPRV_H */
