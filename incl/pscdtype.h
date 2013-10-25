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

#ifndef PSCDTYPE_H
#define PSCDTYPE_H 1

/*
* Data type definitions.
*/

#include "pscchip.h"

/* TRUE/FALSE for boolean values. */
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef DSMVOID
#define DSMVOID	void
#endif

/* End-of-string character. */
#ifndef EOS
#define EOS '\0'
#endif

/* Newline character. */
#ifndef NL
#define NL '\n'
#endif

/* NULL pointer. */
#ifndef NULL
#include <stddef.h>
#endif

/* End of file. */
#ifndef EOF
#define EOF (-1)
#endif

/*
* Progress-defined type qualifiers:
*
* #define PSC_CONST	const type qualifier.
* #define PSC_VOLATILE	volatile type qualifier.
*/

#if PSC_ANSI_LEVEL > 0

#define PSC_CONST	const
#define PSC_VOLATILE	volatile

#else

#define PSC_CONST
#define PSC_VOLATILE

#endif

/*
* Progress-defined data types:
*
* typedef psc_bit_t	Bit field.
* typedef psc_bool_t 	Contains non-zero/zero or TRUE/FALSE. Emphasis is on 
*			speed.
* typedef psc_tbool_t	Like psc_bool_t, but emphasis is on small size.
* typedef psc_bytes_t	For address arithmtic-16 on PC.
* typedef psc_long_t	Signed 32-bit integer.
* typedef psc_ulong_t	Unsigned 32-bit integer. E.g. for bit pushing.
* typedef psc_long64_t  Signed 64-bit integer.
* typedef psc_count_t	For counting from -32k through +32k.
* typedef psc_ucount_t  For counting from 0 through 64k.
* typedef psc_double_t	Double-precision floating point.
* typedef psc_metach_t  Can hold a psc_short_t, EOF, or value from 0 through
*			256.
* typedef psc_text_t	8-bit character.
* typedef psc_utext_t	8-bit character.
* typedef psc_tiny_t	For counting from -128 through 127. Emphasis is on 
*			small size.
* typedef psc_ttiny_t	Same as psc_tiny_t.
* typedef psc_small_t	Like psc_tiny_t, but emphasis is on speed.
* typedef psc_textc_t   Text constant.
* typedef psc_rtlchar_t What the Run Time Library likes for chars. 
*/

/*==========================================================*/
#if UNIX64
#define PAD64(byte_count)       TEXT pad64[byte_count];
#else
#define PAD64(byte_count)
#endif

#if OPSYS==WIN32API
typedef __int64         psc_long64_t;
#elif UNIX64
typedef long            psc_long64_t;
#else
typedef long long       psc_long64_t;
#endif

#if OPSYS==WIN32API
typedef unsigned __int64 psc_ulong64_t;
#elif UNIX64
typedef unsigned long    psc_ulong64_t;
#else
typedef unsigned long long psc_ulong64_t;
#endif

#if CHIP==M68000 || CHIP==VAXCPU || CHIP==AlphaAXP
typedef unsigned int    psc_bit_t;
typedef short           psc_bool_t;
typedef short           psc_count_t;
typedef double          psc_double_t;
typedef short           psc_metach_t; 
typedef short           psc_small_t;
typedef char            psc_tbool_t;
typedef unsigned char   psc_text_t;
typedef unsigned char   psc_utext_t;
typedef char            psc_tiny_t;
typedef unsigned char   psc_ttiny_t;
typedef unsigned short  psc_ucount_t;
typedef char            psc_rtlchar_t;



#if UNIX64
typedef unsigned int   psc_bytes_t;
typedef int            psc_long_t;
typedef unsigned int   psc_ulong_t;
#else
typedef unsigned long   psc_bytes_t;
typedef long            psc_long_t;
typedef unsigned long   psc_ulong_t;
#endif

/*define quasi-storage classes*/
#define XNEAR

#if M8000 || TOWER700 || UNIX64
#define FAST
#else
#define FAST    register
#endif

#define GLOBAL          /*used outside fns for defining use*/
#define IMPORT  extern  /*used where outside-defined vars are ref`d*/
#define INTERN  static  /*used inside functions*/
#define LOCAL   static  /*used outside functions*/
#define LOCALF  static  /*used outside functions*/

/* for portable r-code ALMASK = 3 for 68000 & NUXI systems */
#define ALMASK 3

#else
#endif /* CHIP==M68000 || CHIP==VAXCPU || CHIP==AlphaAXP */

/*==========================================================*/
#ifdef ASSERT_SIZEOF_STUFF

/*
   The types below require an exact number of bits and bytes 
   for closure over operations and external storage, such as 
   portable r-code. 

   If the type is the wrong size, force a compile time divide by zero.

*/

#define ASSERT_SIZEOF(type,size) (1 / (sizeof(type) == size) ? 1 : 0 ))

int isok_psc_long_t	= ASSERT_SIZEOF( psc_long_t,	4);
int isok_psc_ulong_t	= ASSERT_SIZEOF( psc_ulong_t,	4);
int isok_psc_count_t	= ASSERT_SIZEOF( psc_count_t,	2);
int isok_psc_ucount_t	= ASSERT_SIZEOF( psc_ucount_t,	2);
int isok_psc_metach_t	= ASSERT_SIZEOF( psc_metach_t,	2); 
int isok_psc_small_t	= ASSERT_SIZEOF( psc_small_t,	1);
int isok_psc_text_t	= ASSERT_SIZEOF( psc_text_t,	1);
int isok_psc_utext_t	= ASSERT_SIZEOF( psc_utext_t,	1);
int isok_psc_tiny_t	= ASSERT_SIZEOF( psc_tiny_t,	1);
int isok_psc_ttiny_t	= ASSERT_SIZEOF( psc_ttiny_t,	1);
#undef ASSERT_SIZEOF

#endif
/*==========================================================*/

#if SCO
#define MAXPATHLEN 1024
#endif

#if OPSYS==WIN32API
#define OEMRESOURCE /* allows the use of any _OBM* constants */
#define WIN32_LEAN_AND_MEAN
					/* excludes ddeml.h, nb30.h, winsock.h ... from windows.h */
#include "windows.h"
#undef TEXT
#undef ENOTEMPTY
#undef ENAMETOOLONG

#if defined(DLLEXPORT_CHEAT)
#undef DLLEXPORT_CHEAT
#endif
#define DLLEXPORT_CHEAT _declspec(dllexport)

#if defined(DLLEXPORT)
#undef DLLEXPORT
#endif
#define DLLEXPORT _declspec(dllexport)

#if defined(DLLIMPORT)
#undef DLLIMPORT
#endif
#define DLLIMPORT _declspec(dllimport)

#else

#if defined(DLLEXPORT_CHEAT)
#undef DLLEXPORT_CHEAT
#endif
#define DLLEXPORT_CHEAT 

#if defined(DLLEXPORT)
#undef DLLEXPORT
#endif
#define DLLEXPORT 

#if defined(DLLIMPORT)
#undef DLLIMPORT
#endif
#define DLLIMPORT 

#endif

/*==========================================================*/
#if CHIP==INT386
typedef unsigned int    psc_bit_t;
#if WINNT_INTEL
typedef int		psc_bool_t;
#else
typedef short           psc_bool_t;
#endif

typedef char            psc_tbool_t;
typedef char            psc_tiny_t;

typedef unsigned long   psc_bytes_t;
typedef unsigned long   psc_ulong_t;
typedef short           psc_count_t;
typedef double          psc_double_t;
typedef long            psc_long_t;
typedef short           psc_metach_t;
typedef short           psc_short_t;
typedef short           psc_small_t;
typedef unsigned char   psc_text_t;
typedef unsigned char   psc_utext_t;
typedef unsigned char   psc_ttiny_t;
typedef unsigned short  psc_ucount_t;
typedef char            psc_rtlchar_t;

#if PRIME316 || SEQUENT || ROADRUNNER || ATT386 || ALT386 || NCR486 \
 || ICL3000 || SEQUENTPTX || CT386 || WINNT_INTEL || OS2_INTEL || MSC6 \
 || UNIX486V4 || SOLARISX86
#define TEXTC   (TEXT *)
#else
#define TEXTC
#endif

#define XNEAR

#if PS2AIX || SCO
#define FAST
#else
#define FAST    register
#endif

#define GLOBAL          /*used outside fns for defining use*/
#define IMPORT  extern  /*used where outside-defined vars are ref`d*/
#define INTERN  static  /*used inside functions*/
#define LOCAL   static  /*used outside functions*/
#define LOCALF  static  /*used outside functions*/

#undef ALMASK
#define ALMASK 3

#endif /* CHIP==INT386 */
/*==========================================================*/

typedef unsigned long unsigned_long;  /* for 64 bits address arithmetic */

/*==========================================================*/
/*Data Types and Storage Classes for IBM */

#if CHIP==INT8086

#if MSC | XENIXAT | INTEL286 | ALT286 | SYSTEMVAT | CTMSC
typedef unsigned int    psc_bit_t;
typedef short           psc_bool_t;
typedef unsigned long   psc_bytes_t;
typedef unsigned long   psc_ulong_t;
typedef short           psc_count_t;
typedef double          psc_double_t;
typedef long            psc_long_t;
typedef short           psc_metach_t; 
typedef short           psc_small_t;
typedef char            psc_tbool_t;
typedef unsigned char   psc_text_t;
typedef unsigned char   psc_utext_t;
typedef char            psc_tiny_t;
typedef unsigned char   psc_ttiny_t;
typedef unsigned short  psc_ucount_t;
typedef char            psc_rtlchar_t;

#if MSC6 | CTMSC
#undef GBOOL
#define GBOOL    int
#else
#endif

#if SYSTEMVAT | CTMSC
#define TEXTC   (TEXT *) /*for constants*/
#else
#define TEXTC           /*for constants*/
#endif

/*define quasi-storage classes*/
#define XNEAR
#define FAST	register
#define GLOBAL          /*used outside fns for defining use*/
#define IMPORT  extern  /*used where outside-defined vars are ref`d*/
#define INTERN  static  /*used inside functions*/
#define LOCAL   static  /*used outside functions*/

#define LOCALF

#define ALMASK  3       /* shld be 1 on */

#endif /* MSC | XENIXAT | INTEL286 | ALT286 | SYSTEMVAT | CTMSC */

#endif /* CHIP==INT8086 */
/*==========================================================*/

/*==========================================================*/
/*Data Types and Storage Classes for WE32000 */

#if CHIP==WE32000
/*define quasi-types*/
#define BIT     unsigned        /* bit fields */
#define GBOOL    short   /*tested only for non-zero, emphasis on speed*/
#define BYTES   unsigned long /*for address arithmtic-16 on p.c.*/
#define ULONG   unsigned long /*for bit pushing*/
#define COUNT   short   /*for counting -32k to +32k*/
#define DOUBLE  double  /*double precision float pt*/
#define LONG    long    /*signed int -- 32 bits*/
#define METACH  short   /*short, EOF or [0 to 256] */
#define SMALL   short   /*like TINY, but emphasis on speed*/
#define GTBOOL   char    /*char,used like GBOOL, emphasis on size*/
#define TEXT    unsigned char   /*char*/
#define UTEXT   unsigned char
#define TINY    char    /*for counting -128 to 127 (save space)*/
#define TTINY   unsigned char   /* 0 to 127, emphasis on size */
#define UCOUNT  unsigned short /*for counting 0 to 64k*/
#define TEXTC   (TEXT *)  /*for constants*/

/*define quasi-storage classes*/
#define XNEAR
#define FAST    register
#define GLOBAL          /*used outside fns for defining use*/
#define IMPORT  extern  /*used where outside-defined vars are ref`d*/
#define INTERN  static  /*used inside functions*/
#define LOCAL   static  /*used outside functions*/
#define LOCALF  static  /*used outside functions*/
#define ALMASK  3       /*shd be 1 on half-word sys and 3 on fullwd sys*/

#endif /* CHIP==WE32000 */
/*==========================================================*/

#if OPSYS==WIN32API
#define PNULL ((DSMVOID *) 0)
#else
#define PNULL ((TEXT *) 0)
#endif

#if PSC_ANSI_LEVEL == 0
typedef psc_text_t *psc_string_const_t;
#else
typedef PSC_CONST psc_text_t *PSC_CONST psc_string_const_t;
#endif
typedef psc_string_const_t psc_text_const_t;

#ifndef PNULL
#if PSC_ANSI_LEVEL > 0
#define PNULL ((void *) 0)
#else
#define PNULL ((TEXT *) 0)
#endif
#endif

/*
* Deprecated #defines.
*/
#ifndef BIT
#define BIT     psc_bit_t
#endif
#ifndef GBOOL
#define GBOOL    psc_bool_t
#endif
#ifndef BYTES
#define BYTES   psc_bytes_t
#endif
#ifndef ULONG
#define ULONG   psc_ulong_t
#endif
#ifndef LONG64
#define LONG64  psc_long64_t
#endif
#ifndef ULONG64
#define ULONG64 psc_ulong64_t
#endif
#ifndef COUNT
#define COUNT	psc_count_t   
#endif
#ifndef DOUBLE
#define DOUBLE  psc_double_t  
#endif
#ifndef LONG
#define LONG    psc_long_t  
#endif
#ifndef METACH
#define METACH  psc_metach_t  
#endif
#ifndef SMALL
#define SMALL   psc_small_t   
#endif
#ifndef GTBOOL
#define GTBOOL   psc_tbool_t 
#endif
#ifndef TEXT
#define TEXT    psc_text_t
#endif
#ifndef UTEXT
#define UTEXT   psc_utext_t 
#endif
#ifndef TINY
#define TINY    psc_tiny_t
#endif
#ifndef TTINY
#define TTINY  	psc_ttiny_t 
#endif
#ifndef UCOUNT
#define UCOUNT 	psc_ucount_t 
#endif
#ifndef TEXTC
#define TEXTC	(psc_text_const_t)
#endif
#ifndef STRINGC
#define STRINGC (psc_string_const_t)
#endif


/* Define EVENTID type for use in UIM, vt layers.
*        Tex I added it here, so wi.h and wievdef.h could be included
*        in an order-independent way. 
* This needs to be after COUNT and ULONG are defined
*/
#if DBCS

#if 0 /* The dbcs code isnt ready yet */
typedef psc_ulong_t	psc_eventid_t; 
#endif

typedef psc_count_t 	psc_eventid_t;
#else
typedef psc_count_t 	psc_eventid_t;
#endif

#define DBKEY64 

#ifdef DBKEY64
#define DBKEY   psc_long64_t    /* 8 byte dbkey MUST be a signed integer*/
#define BLKCNT  psc_long64_t
#define sdbkey  slng64    /* like slng, but for 8 bytes integer */
#define xdbkey  xlng64    /* like xlng, but for 8 bytes integer */
        /***** sdbkey and xdbkey do not yet exist and must be written.
               they are the equivalent of slng and xlng but for 
               64 bits integers ******/
#else
#define DBKEY   psc_long_t    /* 4 byte dbkey */
#define BLKCNT  ULONG
#define sdbkey  slng
#define xdbkey  xlng
#endif  /* DBKEY64 */
 
typedef struct  /* extended 8 byte db block address */
{
    ULONG    area;
    DBKEY    dbkey;
} xDbkey_t;
 
 
#if 1
 
/* use these definitions while psc_user_number_t is still 16 bits */
 
typedef COUNT psc_user_number_t;  /* will eventually be 32 bits */
 
#define xuser(x) ( xct( (TEXT *) (x) ) )
#define suser(x,i) ( sct( (TEXT *) (x), (UCOUNT) (i) ) )
 
#else
 
/* use these definitions when psc_user_number_t becomes 32 bits */
 
typedef LONG psc_user_number_t;  /* will eventually be 32 bits */
 
#define xuser(x) ( xlng( (TEXT *) (x) ) )
#define suser(x,i) ( slng( (TEXT *) (x), (ULONG (i) ) )
 
 
#endif

typedef psc_eventid_t	EVENTID;

#endif /* PSCDTYPE_H */
