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

#ifndef KYPUB_H
#define KYPUB_H

/*****************************************************/
/***  kypub.h -- entries in the key manager module ***/
/*****************************************************/

#include "dsmpub.h"

struct ditem;

/* kycmpdb and ixadv return codes (also declared in ixctl.h */
#ifndef FOUND
#define FOUND		0	/* keys match exactly */
#define SUBSTRING	1	/* *pkeystr is substr */
#define KEEPGOING	2	/* *pkeystr is larger */
#define LARGER		2	/* *pkeystr is larger */
#define TOOFAR		3	/* *pkeystr is smaller*/
#endif

#define KYLNGKEY	-1	/* return code from kyins */

typedef union {
    TEXT      *textField;
    LONG       longField;
    ULONG      dbkeyField;
} compValue_t;

/**********************************************************************
 PUBLIC INTERFACE FOR ky.c       
**********************************************************************/
DSMVOID kyinitKeydb	(dsmKey_t *pkkey, COUNT ixnum, COUNT numcomps,
			 GTBOOL substrok);

int kycmpdb		(TEXT *pkeystr, TEXT *pixchars);

int  kycountdb		(TEXT *pkeystr1, TEXT *pkeystr2, SMALL numcomps);

/**********************************************************************
 PUBLIC INTERFACE FOR kyut.c       
**********************************************************************/

int  kyinitdb		(struct ditem *pkditem, COUNT ixnum, COUNT numcomps,
			 GTBOOL substrok);

int kyinsdb		(struct ditem *pkditem, struct ditem *pditem,
			 COUNT compnum);

#endif  /* #ifndef KYPUB_H */

