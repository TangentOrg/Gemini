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

#ifndef DTYPE_H
#define DTYPE_H

#define	INVALID_FLD  0          /* Type code never to be used. */
#define	CHRFLD	1
#define	DATEFLD	2
#define	BOOLFLD	3
#define	INTFLD	4
#define	DECFLD	5
#define	FLTFLD	6
#define	DBKFLD	7
#define RAWFLD	8
#define IMGFLD	9
#define WHNDFLD 10   /* WIDGET-HANDLE */
#define MEMPTRFLD 11 /* MEMPTR */
#define SQLDYNPRM 12 /* SQL DYNAMIC PARM used mainly by smdtypn() for 
		        dynamically prepared statements */
#define ROWIDFLD  13 /* ROWID type */ 
#define COHNDFLD 14   /* COMPONENT-HANDLE or COM-HANDLE */
#define TTABLEFLD 15  /* Open4GL - Temp-table parameter type. */
#define UNKNOWNFLD 16 /* Open4GL- UNKNOWN parameter type. */
#define TTABHDLFLD 17  /* Open4GL - Temp-table handle parameter type. */
#define BLOBFLD 18  /* lvarbinary or blob */
#define CLOBFLD 19  /* lvarchar or character blob */


#define HIGHDTYPE 20	/* this must be >= the highest progress datatype */
/* 
 * The following defines are ONLY for mapping the permitted datatypes 
 * for DLL procedures to progress datatypes.
 */
/* (temporarily, also used for some SQL92 types, until 4GL
supports them). */

#define BYTEFLD	  HIGHDTYPE + 1
#define SHORTFLD  HIGHDTYPE + 2
#define LONGFFLD  HIGHDTYPE + 3
#define FLOATFLD  HIGHDTYPE + 4
#define DOUBLEFLD HIGHDTYPE + 5
#define USHORTFLD HIGHDTYPE + 6
#define UBYTEFLD  HIGHDTYPE + 7
#define CURRENCYFLD   HIGHDTYPE + 8
#define ERRORCODEFLD  HIGHDTYPE + 9
#define IUNKNOWNFLD   HIGHDTYPE + 10
#define FIXCHARFLD    HIGHDTYPE + 11
#define BIGINTFLD     HIGHDTYPE + 12
#define TIMEFLD   HIGHDTYPE + 13
#define TIMESTAMPFLD  HIGHDTYPE + 14
#define FIXRAWFLD     HIGHDTYPE + 15

/* end DLL datatypes. */

#define IS_SQL_TYPE(nFld) \
     ((nFld == BYTEFLD)      || \
      (nFld == SHORTFLD)     || \
      (nFld == FLOATFLD)     || \
      (nFld == DOUBLEFLD)    || \
      (nFld == FIXCHARFLD)   || \
      (nFld == BIGINTFLD)    || \
      (nFld == TIMEFLD)      || \
      (nFld == TIMESTAMPFLD) || \
      (nFld == FIXRAWFLD))

/* Symbolic names for the character representation of data types
   (_data-type values). This is for sharing with the SQL92 engine. */

#define CHRFLD_NM "character"
#define COM_HDL_NM "com-handle"
#define COMPNNT_HDL_NM "component-handle"
#define DATEFLD_NM "date"
#define DECFLD_NM "decimal"
#define HANDLE_NM "handle"
#define INTFLD_NM "integer"
#define BOOLFLD_NM "logical"
#define BLOBFLD_NM "lvarbinary"
#define CLOBFLD_NM "lvarchar"
#define MEMPTRFLD_NM "memptr"
#define RAWFLD_NM "raw"
#define DBKFLD_NM "recid"
#define ROWIDFLD_NM "rowid"
#define WHNDFLD_NM "widget-handle"

/* DLL types (temporarily, also used for some SQL92 types, until 4GL
supports them). */

#define BYTEFLD_NM "byte"
#define DOUBLEFLD_NM "double"
#define FLOATFLD_NM "float"
#define LONGFLD_NM "long"
#define SHORTFLD_NM "short"
#define USHORTFLD_NM "unsigned-short"

#define CURRENCYFLD_NM "currency"
#define ERRORCODEFLD_NM "error-code"
#define IUNKNOWNFLD_NM "iunknown"
#define UBYTEFLD_NM "unsigned-byte"

#define FIXCHARFLD_NM "fixchar"
#define BIGINTFLD_NM "bigint"
#define TIMEFLD_NM "time"
#define TIMESTAMPFLD_NM "timestamp"
#define FIXRAWFLD_NM "fixraw"


/* values for _File._Cache field bits */
#define FLBTMAND	1	/* mand bit in db's before vers 9, 83?,now UNUSED! */
#define FLBTKEY		2	/* Key field */
#define FLBTASGN	4	/* Assign trigger exists for this field */
#define FLBTSASGN       8       /* session assign trigger exists for field */
#define FLBTTRIGS	FLBTASGN /* Sum of all trigger bits for fields */
#define FLBTBLOB   	16	/* this column is either a BLOBFLD or CLOBFLD*/


#endif  /* ifndef DTYPE_H */



