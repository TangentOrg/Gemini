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

#ifndef DITEM_H
#define DITEM_H


/* temp - clyde kessel */
#define CALCVLEN vlen
#define INPTVLEN vlen
#define STORVLEN vlen
#define SRTVLEN  vlen

struct vchr			/* var length chr or byte string */
{
	TEXT	vch[4];		/* string */
};

struct decstr			/* fixed point decimal */
{	GTBOOL	plus;		/* number is positive */
	TTINY	totdigits;	/* number of digits */
	TTINY	decimals;	/* number of decimal digits */
	TTINY	digit[1];	/* leading zero suppressed digits */
};
#define MAXDIGIT 50		/* max number of decimal digits */
#define MAXDECS	 15		/* max decimal precision */
#define FASTMAXDECS 10		/* max decimal precision for all but during
				   strict ansi sql compliance */

union calcstr			/* data types: */
{struct	vchr	chrs;		/* character */
	LONG	date;		/* date */
	GBOOL	true;		/* boolean */
	COUNT	smintval;	/* small integer (SQL92) */
	LONG	intval;		/* integer */
 struct	decstr	dec;		/* decimal */
/*	float	floatval;*/	/* floating point */
	DBKEY	dbkey;		/* dbkey */
};

struct keyst
{	COUNT	kidx;		/* index file number	*/
	COUNT	kcomps;		/* number of key comps	*/
	TEXT	pad[2];
	GTBOOL	ksubstr;	/* substr ok bit	*/
	TEXT	keystr[1];	/* room for a key	*/
};


/* data types (value of ditem.type) */

/* This section has been moved to dtype.h to be more readily shared
   with the SQL92 engine. Couldn't just share header file as is,
   because of all the extra includes brought in below.  */

#include "dtype.h"


/* data type categories */
/* These catagories are used MOSTLY to decide whether to		 */
/* left or right justify output data.  Currently, NBRCAT is the only one */
/* which is right justified.  If that ever changes,			 */
/* then a new mechanism should be created which specifies directly, for  */
/* each datatype, whether it is LEFT or RIGHT justified.  In the meantime*/
/* there is no point in changing how it works now.			 */
#define CHRCAT	1
#define DATECAT 2
#define BOOLCAT 3
#define NBRCAT  4
#define BINCAT  5	/* binary fields such as RAW, IMAGE, WIDGET-HANDLE */
/**/

union format			/* formats: */
{struct	vchr	stor;		/* storage */
 union	calcstr	calc;		/* calculation */
 struct	vchr	inpt;		/* input */
 struct	vchr	srt;		/* sort */
 struct keyst	key;		/* key	*/
};
#define DEFCLEN sizeof(union format)	/* default char space */
#define DEFDLEN (sizeof(union format)	/* default dec space */\
	- sizeof(struct decstr) + sizeof(TTINY))
#define DEFVLEN sizeof(union format)	/* default vchr space */

	/* 200 amount of space to allocate for a key ditem,
			   must satisfy aligment constraints as specified
			   by ALMASK. A multiple of 4 will do for now */
	/* Max index entry length SHOULD NEVER be > 240 */

#define MAXWIKEY (MAXKEYSZ - 16) /* max size of key string for word index */
#define DEFKLEN (sizeof(union format) \
	- sizeof(struct keyst) + sizeof(TEXT))
#define SZKEY(PKEY) \
	    ((PKEY)->f.key.keystr[0] + (sizeof(struct ditem) - DEFKLEN) )

#define MAXKDITM  (sizeof(struct ditem) + KEYSIZE)
#define DPAD_AUTOCONV	16 /* If this ditem may be in-place converted to any other 
							  type, this dpad value will allow new type to fit. */

/* formats (value of ditem.type) */
#define TY_KEY   108
#define	STORFRM	101
#define	INPTFRM	104
#if OPSYS==OS400 && AS400_EBCDIC
/* In the EBCDIC world the '?' has decimal value 111 which was causing a problem
   in the tokens.  Change NOTYPE to be 115 to fix this */
#define NOTYPE 115             /* flag for missing*/
#else
#define NOTYPE 111             /* flag for missing*/
#endif
#define RUNTYPE  112 /* type that is not know till run-time, e.g., vbx prop.*/

/* these also appear in keyinfo.h */
#ifndef ASRTFRM
#define ASRTFRM	105
#define DSRTFRM 106
#define UNSRTFRM 107
#endif

#include "axppack4.h"
struct ditem
{	COUNT	vlen;		/* string length */
	TTINY	dflag2;		/* new V6 flagword */
	TTINY	type;		/* format or data type */
	UCOUNT	dpad;		/* padding alloc beyond ditem */
	COUNT	flag;		/* see below */
 union	format	f;		/* values */
};
#include "axppackd.h"

/* Values for dflag2: */
#define CASEDITM	1	/* case sensitive character ditem	 */
#define PERMDITM	2	/* profldu should not destroy this ditem */
#define DIPORTBL	4	/* r-code portable ditem		 */
#define DICONTAINS	8	/* the key is for a CONTAINS clause	 */
#define DINOFLDSILENT	16	/* don't show error if field not in record */
#define DIBEGINS	32	/* the key is for a BEGINS clause	 */

/* functions to declare and initialize auto ditems and arrays of ditems */

#define AUTODITM(PFX,PAD) \
struct PFX \
{struct	ditem	aditem; \
	TEXT	apad[PAD]; \
} PFX;
#define INITDITM(PFX) \
    PFX.aditem.dpad = sizeof(struct PFX) - sizeof(struct ditem); \
    PFX.aditem.vlen = 0; \
    PFX.aditem.dflag2 = 0;

/* Init ptr based ditem, for, e.g., { AUTODITM( myditem, KEYSIZE )} xxx_t;
 *      PDITEM - name of ptr. E.g., pxyz in   pxyz->myditem.
 *      MEMBER - structure member name used as struct name by AUTODITM.
 *                 E.g., myditem.
 * Thus, INIT_PDITM( pxyz, myditem)
 */
#define INIT_PDITM(PDITEM, MEMBER) \
    PDITEM->MEMBER.aditem.dpad   = sizeof(struct MEMBER) -  \
                                   sizeof(struct ditem);    \
    PDITEM->MEMBER.aditem.vlen   = 0; \
    PDITEM->MEMBER.aditem.dflag2 = 0;

#define AUTODITX(PFX,PAD,X) \
    struct PFX \
    {struct ditem   aditem; \
            TEXT    apad[PAD]; \
    } PFX[X];
#define INITDITX(PFX,I) \
    PFX[I].aditem.dpad = sizeof(struct PFX) - sizeof(struct ditem);    \
    PFX[I].aditem.vlen = 0; \
    PFX[I].aditem.dflag2 = 0;

/*flag masks */

/*these are in the ditem only(flag byte 1) */
#define LORANGE  (COUNT) 0x0100 /* range processing : low		*/
#define HIRANGE  (COUNT) 0x0200 /* range processing : high		*/
#define NOTCHGD  (COUNT) 0x0400	/* retn from io interface if not chgd	*/
#define NODATA   (COUNT) 0x0800 /* no data avail from frame field	*/
#define DESCIDX  (COUNT) 0x1000 /* this is a descending index */
#define DSAFF	 (COUNT) 0x2000 /* sort append 0xff			*/
#define OUTPAR   (COUNT) 0x4000 /* run-stored-proc INPUT-OUTPUT param   */
#if OPSYS==OS400
#define QBWWILD  (COUNT) 0x8000 /* QBW key contains a wildcard          */
#endif
#define VALUMASK (COUNT) 0x007f /* last byte only			*/

/*these are in the data record or in ditem (flag byte 2)		*/
#if OPSYS==OS2
#ifdef UNKNOWN
#undef UNKNOWN
#endif /* UNKNOWN */
#endif /* OS2 */
#define UNKNOWN	        2	/* value unknown			*/
#define MISSING		UNKNOWN	/* alias -- used to include NA		*/
#define LOW	        8	/* treat as < any known value		*/
#define HIGH	        16	/* treat as > any known value		*/
#define CDATE		4	/* current date (initial value = today)	*/
#define SQLNULL		32	/* SQL NULL keyword (not null value) */

/*never use 128 = 0x80 due to sign bit xtension*/

/* use this to test if there is no useful value in a ditem */

#define NOVALMSK ~(NOTCHGD + DSAFF + DESCIDX)

/*
 * Use the following to determine if the value of the ditem is known 
 * or unknown.
 */

#define ditem_UNKNOWN(d_item_flags)     ((d_item_flags & VALUMASK) & UNKNOWN)
#define ditem_KNOWN(d_item_flags)      !((d_item_flags & VALUMASK) & UNKNOWN)

#endif  /* ifndef DITEM_H */
