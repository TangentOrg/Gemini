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

#ifndef KEYPUB_H
#define KEYPUB_H

#include "dsmpub.h"
#include "svcspub.h"

/* For use in C++ by SQL engine, define entire include file to have "C linkage" . */
/* This is technique used by all standard .h sources in /usr/include,...          */
#ifdef  __cplusplus
extern "C" {
#endif

/*************************************************************/
/* constants                                                 */
/*************************************************************/

#define KEY_MAX_INDEX_COMPONENTS 16
#define KEY_MAX_KEY_SIZE         200 /* NOTE: this is a copy of the value in
                                      * cxprv.h's MAXKEYSZ; We will have
                                      * to consolidate these two #defines
                                      * also, in future the max-key-size will
                                      * be dependent on the block-size
                                      * so we might give here a maximal maximum
                                      * and additionaly have a function 
                                      * providing the maximum for a certain DB
                                      */

/* keyut:keyConvertIxToCx: value of "bufferLength" if no length available */
#define KEY_NO_LENGTH_CHECK  -1

#define KEY_WORD_INDEX  1       /* this index is a word-index */

/*************************************************************/
/* comparison operator for brackets                          */
/*************************************************************/

#define KEY_EQ 1030
#define KEY_GE 1031
#define KEY_GT 1032
#define KEY_LE 1033
#define KEY_LT 1034
#define KEY_BEGINS 6098
#define KEY_ALL 6392

#define           SMALLEST_BETWEEN_OP    10000 /* must be less than all between
                                            operatprs, higher than all other */
#define  KEY_BETWEEN_GT_LT      10001
#define  KEY_BETWEEN_GE_LT      10002
#define  KEY_BETWEEN_GT_LE      10003
#define  KEY_BETWEEN_GE_LE      10004

/*************************************************************/
/* WordIndex #defines (WordBreakTable Constants,...) */
/*************************************************************/

/* the QW defines are copied from glue/incl/qwctl.i ... */
#define QWTERMIN        1       /* represents a word terminator */
#define QWLETTER        2       /* represents a letter */
#define QWDIGIT         3       /* represents a digit */
#define QWUSEIT         4       /* character - use as is in the word index  */
#define QWBEFLET        5       /* use it if followed by a letter */
#define QWBEFDIG        6       /* use it if followed by a digit */
#define QWBEFLETDIG     7       /* use it if followed by a letter or a digit */
#define QWSKIPIT        8       /* skip it - ignore it */
#define QWSPECIAL       255     /* special treatment: call a procedure */
 
#define QW_TABSIZE      256
/* ... end of code from qwctl.i copied into here */

#define  KEY_BEFORE_DIGIT        QWBEFDIG
#define  KEY_BEFORE_LETTER       QWBEFLET
#define  KEY_BEFORE_LETTER_DIGIT QWBEFLETDIG
#define  KEY_DIGIT               QWDIGIT
#define  KEY_LETTER              QWLETTER
#define  KEY_TERMIN              QWTERMIN
#define  KEY_USE_IT              QWUSEIT
#define  KEY_MAX_WI_SIZE         MAXWIKEY

/*************************************************************/
/* Status typedef and return values for keysvc.c             */
/*************************************************************/

typedef LONG  keyStatus_t;        /* type for status return from procedures */

#define KEY_SUCCESS           0   /* Everything went ok */
#define KEY_BAD_COMPONENT    -1   /* one or more of the components are bad */
#define KEY_BAD_I18N         -2   /* something's wrong with pI18N */
#define KEY_BAD_PARAMETER    -3   /* one or more parameters invalid */
#define KEY_BUFFER_TOO_SMALL -4   /* maxKeyLength to small for generated key */
#define KEY_EXCEEDS_MAX_SIZE -5   /* key exceeds KEYSIZE */
#define KEY_BAD_COMPARE_OPERATOR -6   /* invalid compare operator */
#define KEY_WORD_TRUNCATED   -7   /* the original word is longer than MAXWIKEY */
                                  /* but only MAXWIKEY bytes returned */
#define KEY_NO_MORE_WORDS    -8   /* The text contained no more words */
#define KEY_ILLEGAL          0xfa /* illegal component found within ix-key */

/* old return-codes 
 * since we gave the .h file to the SQL folks, I leave the old codes in so I 
 * don't break any of their stuff. However, I will let them know that they 
 * changed, so they will use the new, correct ones from now on. When I'm sure
 * the old codes aren't used anywhere anymore, I will remove this whole section
 * (th) 
 */
#define KEYSUCCESS         KEY_SUCCESS
#define KEYBADCOMPONENT   KEY_BAD_COMPONENT
#define KEYBADI18N        KEY_BAD_I18N
#define KEYBADPARAMETER  KEY_BAD_PARAMETER
#define KEYBUFFTOOSMALL   KEY_BUFFER_TOO_SMALL
#define KEYEXCEEDSMAXSIZE KEY_EXCEEDS_MAX_SIZE


/*************************************************************/
/* Special internal stuff shared among ky,cx and key         */
/***  kyctl.h - internal gobbledy goop for the key manager ***/
/*************************************************************/

#define LKYFLAG      (TTINY) 0xf0 /* the lowest key flag value */
#define SHORTKEY     (TTINY) 0xf8
#define IDXLOCK      (TTINY) 0xf9
#define KYILLEGAL    KEY_ILLEGAL  /* references to be replaced with KEY_ILLEGAL */
#define LOWMISSING   (TTINY) 0xfb
#define HIGHMISSING  (TTINY) 0xfc
#define DUMMYENT     (TTINY) 0xfd
#define LOWRANGE     (TTINY) 0xfe
#define HIGHRANGE    (TTINY) 0xff

/*
 * new key format escape sequences
 */
#define COMPSEPAR	(TEXT)0x00 /* component separator */

/* high escape equences */
#define HIGHESCAPE	(TEXT)0xff /* high escape character */
#define REPLACEFF	(TEXT)0x01 /* HIGHESCAPE, 0x01 replaces a 0xff byte */
/* high range used to eliminate unknown values - 0xfb */

#ifdef CX_HANDLE_UNKNOWN_LIMITS
#define HIKNOWN		(TEXT)(HIGHMISSING - 1)
#endif /* CX_HANDLE_UNKNOWN_LIMITS */

/* low escape sequences */
#define LOWESCAPE	(TEXT)0x01 /* low escape character */
#define REPLACEZERO	(TEXT)0xfe /* LOWESCAPE, 0xfe replaces a 0 byte */
#define REPLACEONE	(TEXT)0xff /* LOWESCAPE, 0xff replaces a 1 byte */
#define NULLCOMPONENT	(TEXT)0xf0 /* LOWESCAPE, 0xf0 replaces a null comp. */
/* low range used to eliminate unknown values - 0xfc */
#ifdef CX_HANDLE_UNKNOWN_LIMITS
#define LOWKNOWN	(TEXT)(LOWMISSING + 1)
#endif /* CX_HANDLE_UNKNOWN_LIMITS */

/* from kypub.h */
#ifndef KYLNGKEY
#define KYLNGKEY -1
#endif /* KYLNGKEY */



/*************************************************************/
/* Public Function Prototypes for KeyServices                */
/*************************************************************/

keyStatus_t DLLEXPORT keyBracketBuild(
       COUNT keyMode,          /* IN  */
       dsmIndex_t     index,            /* IN  */
       COUNT          numberComponents, /* IN - Number of componets in array */
       COUNT          keyOperator,      /* IN - KEY_BETWEEN_GT_LT, etc. */
       svcDataType_t *pComponent1[],    /* IN - Components of the key */
       svcDataType_t *pComponent2[],    /* IN - 2. Key for "between" queries */
       COUNT          maxBaseKeyLength, /* IN  */
       dsmKey_t      *pBaseKey,         /* OUT */
       COUNT          maxLimitKeyLength,/* IN  */
       dsmKey_t      *pLimitKey);       /* OUT */

keyStatus_t DLLEXPORT keyBuild(
                COUNT            keyMode,       /* IN */
                svcDataType_t   *pComponent[],  /* IN */
                COUNT            maxKeyLength,  /* IN */
                dsmKey_t        *pKey);         /* IN/OUT */

keyStatus_t  DLLEXPORT keyConvertCxToIx(
        TEXT *pt,         /* OUT - target area - old format (v6, ix) */
        int   keysize,     /* IN  - Size of source area (incl 3 bytes info */ 
        TEXT *ps);        /* IN  - source area - new format (v7, cx) */

keyStatus_t  DLLEXPORT keyConvertCxToIxComponent ( 
        TEXT *pt,         /* OUT */
        TEXT *ps);        /* IN  */

keyStatus_t  DLLEXPORT keyConvertIxToCx(
        dsmKey_t       *pdsmKey,        /* OUT storamge manager key structure */
        dsmIndex_t      indexNumber,    /* IN  object-number of index*/
        dsmBoolean_t    ksubstr,        /* IN  substring ok bit */
        dsmBoolean_t    word_index,     /* IN  is this for a word index (i.e.
                                         *     keyString is list of words)
                                         */
        TEXT            keyString[],    /* IN  keyString in IX format */
        LONG            bufferSize);    /* IN  max possible length for keystr */

keyStatus_t DLLEXPORT keyGetComponents(
                 COUNT           keyMode,       /* IN */
                 svcI18N_t      *pI18N,         /* IN */
                 dsmKey_t       *pKey,          /* IN */
                 svcDataType_t  *pComponent[]); /* IN/OUT */

keyStatus_t DLLEXPORT qwGetNextWord(
        TEXT   **ppText,           /* pointer to pointer to 1. char of text */
        TEXT    *pNoMore,          /* position past the last char of text */
        TEXT    *pWord,            /* return the next word here */
        TEXT    *pWordBreakTable); /* ponter to the wordbreak table */

keyStatus_t DLLEXPORT keyInitStructure(
        COUNT           keyMode,        /* IN     - SVC???KEY */
        dsmIndex_t      indexNumber,    /* IN     - object-number of index*/
        COUNT           keycomps,       /* IN     - number of key components */
        COUNT           bufferLength,   /* IN     - Length of key string */
        dsmBoolean_t    descending_key, /* IN     - substring ok bit */
        dsmBoolean_t    ksubstr,        /* IN     - substring ok bit */
        dsmKey_t       *pKey);          /* in/OUT - structure to initialize */

/* For use in C++ by SQL engine, define entire include file to have "C linkage" . */
/* This is technique used by all standard .h sources in /usr/include,...          */
#ifdef  __cplusplus
}
#endif
                                     
DSMVOID kyinit(struct ditem *pkditem, COUNT ixnum, COUNT numcomps, GTBOOL substrok);

int kyins (struct ditem *pkditem, struct ditem *pditem, COUNT compnum);

#endif  /* ifndef KEYPUB_H */






