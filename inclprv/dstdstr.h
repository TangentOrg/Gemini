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
#ifndef DSTDSTR_H
#define DSTDSTR_H


/*****************************************************************/
/** dstdstr.h -- standard structure name #defines for PROGRESS	**/
/**								**/
/**	This is included in dstd.h, so all programs have	**/
/**	these declares automatically				**/
/*****************************************************************/

#define AICTL	struct aictl	/* see rlai.h	 */
#define BKCTL	struct bkctl    /* see bkctl.h	*/
#define BKRNCTL	struct bkrnctl  /* see bkrnctl.h */
#define CC	struct cc	/* see ci.h	 */
#define CI	struct ci	/* see ci.h	 */
#define CONDB	struct condb	/* see condb.h	 */
#if 0	/****NOT NEEDED FOR THE NEW INDEX ******/
#define CURSOR	struct cursor   /* should not be used from V7 on */
#endif	/****NOT NEEDED FOR THE NEW INDEX ******/
#define DBCHARTBL struct dbchartbl /* see dbctl.h */
#define DBCTL	struct dbctl	/* see dbctl.h	 */
#define DBDES	struct dbdes	/* see scstr.h	 */
#define DITEM	struct ditem	/* see ditem.h	 */
#define FILDES	struct fildes	/* see scstr.h	 */
#define FLDDES	struct flddes	/* see scstr.h	 */
#define FXFIL	struct fxfil	/* see scfx.h	*/
#define ICB	struct icb	/* see icb.h	 */
#define IDXCOMP struct idxcomp	/* see scstr.h	 */
#define ITBLK	struct itblk    /* see ixctl.h */
#define KEYINFO struct keyinfo	/* see keyinfo.h */
#define KEYINIT	GTBOOL		/* see keyinfo.h */
#define KFLD	struct kfld	/* see keyinfo.h */
#define LKCTL	struct lkctl    /* see lkctl.h	*/
#define MAND	struct mand	/* see scstr.h	 */
#define MANDCTL struct mandctl  /* see scstr.h */
#define NSMHDR	struct nsmhdr	/* see nsmgr.h */
#define MSTRBLK	struct mstrblk  /* see mstrblk.h */
#define MTCTL	struct shmemctl /* see mtctl.h	*/
#define PIDLIST struct pidlist	/* see utshm.h	*/
#define RLCTL	struct rlctl    /* see rlctl.h */
#define SECTL   struct sectl    /* see sectl.h */
#define SEGHDR	struct seghdr	/* see utshm.h	*/
#define SEGMNT  struct segment	/* see utshm.h	*/
#define SEMDSC	struct semdsc   /* see utsem.h	*/
#define SHMDSC	struct shmdsc   /* see utshm.h	*/
#define SRVCTL	struct srvctl   /* see srvctl.h  */
#define STPOOL	struct stpool	/* see st.h	 */
#define STSHPTR	struct stshptr	/* see keyinfo.h */
#define TRANTAB	struct trantab  /* see tmtrntab.h */
#define USERWHO	struct userwho  /* see svpmctl.h */
#define USRCTL	struct usrctl   /* see usrctl.h  */
#define XREF	struct xref	/* see keyinfo.h */

#endif /* #ifndef DSTDSTR_H */
