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

#ifndef GEMINIKEY_H
#define GEMINIKEY_H
#include "dstd.h"

/************???????? MEED TO CHANGE ALL THE VALUES OF THE following #define */
#define SUCCESS				0
#define BUFFER_TOO_SMALL	2
#define BAD_FIELD_SIZE		3
#define BAD_DECIMAL_DIGIT     4
#define BAD_FIELD_TYPE        5
#define TRUNCATED_COMPONENT  6
#define BAD_PARAMETER        7
#define INVALID_DECIMAL_DIGITS  8

#define MYMAXKEYSIZE       254 /* The whole key includes idx #, and row ID */
#define MAXCOMPONENTSIZE   MAXKEYSZ /* the max size of each component */


#define GEM_TINYINT   11
#define GEM_SMALLINT  12
#define GEM_MEDIUMINT 13
#define GEM_INT       14
#define GEM_BIGINT    15
#define GEM_ENUM      16
#define GEM_SET       17
#define GEM_DATE      18
#define GEM_TIME      19
#define GEM_DATETIME  20
#define GEM_TIMESTAMP 21
#define GEM_YEAR      23
#define GEM_FLOAT      24
#define GEM_DOUBLE      25
#define GEM_DECIMAL    251
#define GEM_CHAR      26
#define GEM_VARCHAR      27
#define GEM_TINYBLOB      28
#define GEM_TINYTEXT      29
#define GEM_BLOB      30
#define GEM_TEXT      31
#define GEM_MEDIUMBLOB      32
#define GEM_MEDIUMTEXT      34
#define GEM_LONGBLOB      35
#define GEM_LONGTEXT      36
#define GEM_DBKEY         37

/* For use in C++ by SQL engine, define entire include file to have "C linkage" . */
/* This is technique used by all standard .h sources in /usr/include,...          */
#ifdef  __cplusplus
extern "C" {
#endif

/*-----------------------------------------------*/
int DLLEXPORT
gemFieldToIdxComponent( 
       unsigned char    *pField, 
                               /* IN, ptr to the data part of the field
                               in the MySQL row. This is useually the 1st
                               byte of the field. But in the case of VARCHAR
                               or BLOB the beginning of the field may be the
                               lenght or a pointer. In these case, pField
                               should point to the 1st byte of the actual data,
                               not to the 1st byte of the field.
                               In case of NATIONAL CHAR, or NATIONAL VARCHAR
                               where weights are used instead of the original
                               data, pField should point to the string of 
                               weights not the original data.  */
       unsigned long fldLength,		
                               /* IN, number of actual data bytes in  the field.
                               Not including other bytes, like legth bytes
                               of a VARCHAR field */
       int	fieldType,	/* IN  data type of the field */
       int	fieldIsNull,	/* IN, 1 if the field value is a NULL value */				   
       int	fieldIsUnsigned,/* IN, 1 if the field value is a NULL value */
       unsigned char *pCompBuffer, /* IN, ptr to area receiving the component */
       int	bufferMaxSize,	/* IN, max size of the component buffer */
       int	*pCompLength);	/* OUT, lenght in byte of the component */

/*-----------------------------------------------*/
int DLLEXPORT
gemIdxComponentToField(
       unsigned char  *pComponent, /* IN, ptr to the start of the component in the key*/
       int	fieldType,	/* IN  data type of the field */	   
       unsigned char    *pField, /* IN, ptr to the area receiving the 
                                    field data.  The size of the area must 
                                    be at least that of fieldSize */	
       int fieldSize,	 /* IN,  size of the receiving field 
                          e.g. 1 for TINY, 2 for SMALL, 3 for MEDIUM, etc. */
       int decimalDigits,  /* IN, (for DECIMAL type only) number of digit 
                              past the decimal point in the field */
       int *pfieldIsNull /* OUT, 1 if the field value is a NULL value */			   
        );  
  
/*-----------------------------------------------*/
int DLLEXPORT
gemKeyInit(unsigned char *pKeyBuf, int *pKeyStringLen, short index);

int DLLEXPORT gemKeyLow(unsigned char *pkeyBuf, int *pkeyStringLen, short index);

int DLLEXPORT gemKeyAddLow(unsigned char *pkeyBuf, int *pkeyStringLen);

int DLLEXPORT gemKeyHigh(unsigned char *pkeyBuf, int *pkeyStringLen, short index);

int DLLEXPORT gemKeyAddHigh(unsigned char *pkeyBuf, int *pkeyStringLen);
int DLLEXPORT 
gemDBKEYtoIdx(
	      DBKEY   dbkey,          /* IN - the 32 or 64 bits dbkey */
	      int	minimumSize,  
	      /* IN, minimum size of variable DBKEY in the key
		 including one extra byte for the exponent & size nibbles*/
	      TEXT   *pTo /* IN, ptr to area receiving the Variable size DBKEY */
	      );
void DLLEXPORT
gemDBKEYfromIdx(
		unsigned
		char    *pVarSizeDBKEY,	
		/* IN, ptr to the start of the variable DBKEY */
		DBKEY           *pdbkey); /* OUT - the DBKEY */

/*-----------------------------------------------*/

/* For use in C++ by SQL engine, define entire include file to have "C linkage" . */
/* This is technique used by all standard .h sources in /usr/include,...          */
#ifdef  __cplusplus
}
#endif
  
#endif /* GEMINIKEY_H  */
