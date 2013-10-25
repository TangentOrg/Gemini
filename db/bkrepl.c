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

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "bkpub.h"
#include "bkdoundo.h"
#include "dsmpub.h"
#include "rlpub.h"
#include "utspub.h"

/* PROGRAM: bkReplace - replace a portion of a block
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkReplace (
	dsmContext_t	*pcontext,
	bmBufHandle_t    bufHandle,
	TEXT		*pold,
	TEXT		*pnew,
	COUNT		 len)
{
   	TEXT *pblk;
	/* max size for BKRPNOTE + data in 255 and noteData area
	   is defined as a ULONG to ensure proper alignment. */
        ULONG noteData[(255/sizeof(ULONG)) + 1];
	BKRPNOTE *pnote = (BKRPNOTE *)noteData;
	COUNT	 notelen;


    /* make the note */
    /* OBSERVE - there is some unpleasantry here.  Compilers (Plexus et. al.)
       which pad structures to end on an odd byte are going to cause
       sizeof(BKRPNOTE) to evaluate so:
       RLNOTE	    12
       COUNT	     2
       TEXT	     1
       padding	     1 bytes for a total of fourteen.
       This is all very nice but it causes an extra byte to writen in the
       note and make the rl dump utility look bad.
    */
    pblk = (TEXT *)bmGetBlockPointer(pcontext,bufHandle);
    
    notelen = sizeof(BKRPNOTE) + len - sizeof(pnote->data);
    if ( notelen > 255 ) /* added to replace stkpsh, memory now on stack */
       FATAL_MSGN_CALLBACK(pcontext, bkFTL039, notelen );
    
    pnote->rlnote.rlcode = RL_BKREPL;
    sct((TEXT *)&pnote->rlnote.rlflgtrn, (COUNT)RL_PHY);
    pnote->rlnote.rllen = (TEXT)notelen; /* assumes notelen < 256 */
    pnote->rlnote.rlTrid = 0;

    /* copy the old value */
    pnote->offset = pold - pblk;
    bufcop ( pnote->data, pold, len );

    /* let er rip */
    rlLogAndDo (pcontext, (RLNOTE *)pnote, bufHandle, len, pnew );


}  /* end bkReplace */


/* PROGRAM: bkrpldo -
 *
 * RETURNS: DSMVOID
 */
DSMVOID
bkrpldo(
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		*pnote,
	bkbuf_t		*pblk,
	COUNT		 data_len,
	TEXT		 *pdata)
{
    bufcop ( (TEXT *)pblk + ((BKRPNOTE *)pnote)->offset, pdata, data_len );

}  /* end bkrpldo */


/* PROGRAM: bkrplundo -
 *
 * RETURNS: DSMVOID:
 */
DSMVOID
bkrplundo (
	dsmContext_t	*pcontext _UNUSED_,
	RLNOTE		 *pnote,
	bkbuf_t		 *pblk,
	COUNT		  data_len,
	TEXT		 *pdata _UNUSED_)       /* Not used */
{
    bufcop ( (TEXT *)pblk + ((BKRPNOTE *)pnote)->offset, 
                            ((BKRPNOTE *)pnote)->data, data_len );

}  /* end bkrplundo */


