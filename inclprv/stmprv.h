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

#ifndef STMPRV_H
#define STMPRV_H 1

/*****************************************/
/** stm.h - Storage Management Routines **/
/*****************************************/

#include "shmpub.h"
#include "latprv.h"

#define ABKSIZE 976    /* allows total abk str to fit in 1008 (leaving
                          malloc 12 bytes to manage the block + 2 bytes for
                          bounds checking + 2 bytes for 4 byte alignment) */
#define ABK	struct abk
#define UPABK    union  upabk
#define UPTXT    union  uptxt
#define RENTST  struct rentst
#define RENTPRFX  struct rentprfx
#define UPRENTST union uprentst

#define CLEARPOOL 0x0001 /* indicates that this pool's blocks (abks) are
   cleared when they're read. useful for those pools which always stget/strent
   and don't use stgetd/strentd effectively. optimization to reduce the volume
   of calls to stnclr() */

/* The following unions are necessary because STPOOLs and their associated
   storage blocks can be located in either private memory or shared memory.  If
   the pool is in shared memory, it must be manipulated using shared ptrs
   (designated by being prefixed with q).  Otherwise, a real pointer is used.
   (designated by being prefixed with p)
*/

union uptxt			/* TEXT pointer */
{
    QTEXT  qtr;			/* shared pointer */
    TEXT   *ptr;		/* real pointer */
};

union upabk			/* Storage block pointer */
{
    QABK  qabk;			/* shared pointer */
    ABK  *pabk;			/* real pointer */
};

union uprentst
{
    QRENTST  qren;
    RENTST  *pren;
};

typedef struct	rentst
{
    UPRENTST next; /* used only in free chain */
} rentst_t;

typedef struct	rentprfx
{
    unsigned	abkoffst;	/* offset backwards to top of ABK	*/
    unsigned	rentlen;	/* rent area length			*/
} rentprfx_t;


/* NOTE we alway want area to be aligned on a double word boundry */
typedef struct abk		/*basic rcode storage block		*/
{	BYTES	 rmleft;	/*bytes left in data portion		*/
	BYTES    abksize;       /*usually set to ABKSIZE except for shmem*/
	UPTXT	 ua;		/*ptr to 1st avail byte			*/
	UPABK	 unabk;		/*next ptr				*/
	RENTPRFX dummyhdr;	/*dummy cell in stvacate free chain     */
	RENTST	 free;		/*ditto, must be immediately after dummyhdr */
				/*dummyhdr & free MUST NOT touch abk.area */
	TEXT	 abkmagic;
	TEXT	 pad[7];
	STPOOL	*pstpool;	/* points back to stpool header		*/
        
	TEXT	 area[ABKSIZE];	/*actual data area			*/
} abk_t;

typedef struct stpool		/*storage ctlstr for rcode		*/
{	UPABK	 uhabk;		/*hdr blk ptr				*/
 	UPABK	 utabk;		/*tail blk ptr				*/
	UPABK	 uxabk;		/*points to abk's which are full	*/
	STPOOL	*prevpool;	/*points to prev in a stack of pools	
				   - not use in shmem pools             */
	RENTST	 free;		/*free anchor for strent and stvacate	*/
	SHMDSC	 *pshmdsc;	/*shared pointer to the shared mem ctl	*/
        TTINY     poolflgs;     /*for noting characteristics of the pool */
        TTINY     whatpool;     /* for diagnostics use -- record pool id */
        BYTES    abksize;       /*if set, use this instead of ABKSIZE */
        latch_t  poolLatch;     /* process private latch for storage pools */
} stpool_t;

/* for strent and stvacate #*/
#define PRFXSIZE sizeof(RENTPRFX) /*size of rented area prefix#*/
#define	ABKMAGIC 37	/* magic number in abk.abkmagic			  */
#define	ENDMAG1 73	/* large strent in pgstpool with no ABK around it */
#define	ENDMAG2 74	/* strent in shared memory pool			  */
#define	ENDMAG3 75	/* all other strent space			  */
#define	VACMAGIC 97	/* magic number at end of vacated space		  */


#define MAXRENT 0x7ff0	/* maximum amount to pass to strent */
#define STROUND(x) (((x) + (sizeof(long)-1)) & ~(sizeof(long)-1) )
#define STROUND8(x) (((x) + (sizeof(double)-1)) & ~(sizeof(double)-1) )

#ifndef STPOOLDIAG
/* redefine calls stmIntpl() to be stmInitPool() */
#define stmIntpl(ppool) stmInitPool(pcontext, (ppool), (TTINY) 0, (BYTES) 0)
#else
/* we're in diagnostics mode, record what pool we're talking about */
#define stmIntpl(ppool) stInitPoolDiagnostic((ppool), (TTINY) 0, (BYTES) 0, \
                                            (TEXT *)__FILE__, (LONG) __LINE__);
#define stmInitPool(ppool, flg, abksize) \
    stInitPoolDiagnostic((ppool),(flg),(abksize), \
                         (TEXT *)__FILE__,(LONG) __LINE__);
#endif

#endif /* STMPRV_H */
