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

#include "dbconfig.h"   /* database manager */

#include "recpub.h"     /* public interface */
#include "utmpub.h"
#include "svcspub.h"


#define VECTORPFX  ( 1 + sizeof(COUNT) )

/* 
 * FILE: recput.c - contains the field put routines of the record services
 * 
 * General Description for ALL Put functions herein:
 *          Given a pointer to the record in a buffer, the maximum size of
 *          the buffer, the column id and the new value in SQL Engine format,
 *          the old value of the field is replaced by the new value.
 * 
 *          A column with the same column id must already exist in the row,
 *          else return an error.
 *  
 *          If there is enough space in the buffer, the value is converted
 *          to that of Progress and replaces the old value with the new.
 *          This may require to shift part of the row if the length of the
 *          new value is not the same as that of the old value and may
 *          require an update to some of the skip table entries.
 *  
 *          If there is not enough space in the buffer an error is returned
 *          and the required buffer size is returned in *precSize.
 *          The caller will have to allocate a new buffer and call again.
 *
 * RETURNS: Returns total size of the formatted record in *preSize.  If the
 *          record doesn't fit in the buffer then returns total size needed.
 *          The caller must check if the returned size is greater than the
 *          max size of the buffer, in which case this procedure must be called
 *          again with a large enough buffer.
 */


/* PROGRAM: recPutField - stores specified data type in record
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 *          RECINVALIDTYPE
 */
recStatus_t
recPutField(
        TEXT          *precordBuffer, /* IN/OUT record buffer */
        ULONG          maxSize,      /* IN  size of input buffer */
        ULONG         *precSize,     /* OUT size of new record or size needed */
        ULONG          fieldNumber,  /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcDataType_t *pdataType) /* IN  value to assign to the field */
{

    recStatus_t returnCode;

    switch(pdataType->type)
    {
      case(SVCBLOB):
      case(SVCBINARY):
      case(SVCBYTESTRING):
      case(SVCCHARACTER):
        returnCode = recPutBYTES(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, &pdataType->f.svcByteString,
                                 pdataType->indicator);
      break;
      case(SVCTINY):
        returnCode = recPutTINY(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, pdataType->f.svcTiny,
                                 pdataType->indicator);
      break;
      case(SVCSMALL):
        returnCode = recPutSMALL(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, pdataType->f.svcSmall,
                                 pdataType->indicator);
      break;
      case(SVCBOOL):
        returnCode = recPutBOOL(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, pdataType->f.svcBool,
                                 pdataType->indicator);
      break;
      case(SVCLONG):
        returnCode = recPutLONG(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, pdataType->f.svcLong,
                                 pdataType->indicator);
      break;
      case(SVCBIG):
        returnCode = recPutBIG(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, pdataType->f.svcBig,
                                 pdataType->indicator);
      break;
      case(SVCFLOAT):
        returnCode = recPutFLOAT(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, pdataType->f.svcFloat,
                                 pdataType->indicator);
      break;
      case(SVCDOUBLE):
        returnCode = recPutDOUBLE(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, pdataType->f.svcDouble,
                                 pdataType->indicator);
      break;
      case(SVCDATE):
        returnCode = recPutDATE(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, &pdataType->f.svcDate,
                                 pdataType->indicator);
      break;
      case(SVCTIME):
        returnCode = recPutTIME(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, &pdataType->f.svcTime,
                                 pdataType->indicator);
      break;
      case(SVCTIMESTAMP):
        returnCode = recPutTIMESTAMP(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, &pdataType->f.svcTimeStamp,
                                 pdataType->indicator);
      break;
      case(SVCDECIMAL):
        returnCode = recPutDECIMAL(precordBuffer, maxSize, precSize,
                                 fieldNumber, arrayIndex, &pdataType->f.svcDecimal,
                                 pdataType->indicator);
      break;
      default:
        returnCode = RECINVALIDTYPE;
      break;
    }

    return returnCode;

}  /* end recPutField */


/* PROGRAM: recPutTINY - stores a Tiny integer in a record in the same
 *                       way a Long integer is stored
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 */
recStatus_t
recPutTINY(
        TEXT      *precordBuffer,    /* IN/OUT record buffer */
        ULONG      maxSize,          /* IN  size of input buffer */
        ULONG     *precSize,         /* OUT size of new record or size needed */
        ULONG      fieldNumber,      /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcTiny_t  fieldValue,       /* IN  value to assign to the field */
        ULONG      indicator)        /* IN  indicator flag */
{
    svcLong_t   longValue = fieldValue;

    return ( recPutLONG(precordBuffer, maxSize,precSize,
                      fieldNumber, arrayIndex, longValue, indicator) );


}  /* end recPutTINY */


/* PROGRAM: recPutSMALL - stores a Small integer (COUNT) in a record in the same
 *                       way a Long integer is stored
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 *
 */
recStatus_t
recPutSMALL(
        TEXT        *precordBuffer,  /* IN/OUT record buffer */
        ULONG        maxSize,        /* IN  size of input buffer */
        ULONG       *precSize,       /* OUT size of new record or size needed */
        ULONG        fieldNumber,    /* IN  number of field within record */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcSmall_t   fieldValue,     /* IN  value to assign to the field */
        ULONG        indicator)      /* IN  indicator flag */
{
    svcLong_t   longValue = fieldValue;

    return ( recPutLONG(precordBuffer, maxSize, precSize,
                      fieldNumber, arrayIndex, longValue, indicator) );

}  /* end recPutSMALL */


/* PROGRAM: recPutLONG - Update a Field of a type LONG 
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 */
recStatus_t
recPutLONG(
        TEXT      *precordBuffer,   /* IN/OUT record buffer */
        ULONG      maxSize,         /* IN  size of input buffer */
        ULONG     *precSize,        /* OUT size of new record or size needed */
        ULONG      fieldNumber,     /* IN  number of field within record */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcLong_t  fieldValue,     /* IN  value to assign to the field */
        ULONG      indicator)       /* IN  indicator flag */
{
    /*** General steps to update a field in a record buffer ***/
    /* 1. Position to the field in the record.
     * 2. Convert data value to storage format.
     * 3. Call record manager to update the record.
     * 4. Update size with current record size or size of record needed.
     * 5. return pass or fail.
     */

    TTINY buffer[2*sizeof(LONG)];      /* buffer to storage format field value*/

    TEXT  *pfieldBuffer = &buffer[0];  /* ptr to working buffer */
    TEXT  *pfield;                     /* Pointer to field within record */
    TEXT  *pArray;   /* ptr to begining of array within record */


    pfield = recFindField(precordBuffer, fieldNumber);
    if (pfield == NULL)
        return RECFLDNOTFOUND;

    pfieldBuffer = recFormatLONG(pfieldBuffer, sizeof(buffer), fieldValue,
                                 indicator);

    if (*pfield == VECTOR)
    {
        pArray = pfield;
        pfield = recFindArrayElement(pfield, arrayIndex);
        if (pfield == NULL)
            return RECFLDNOTFOUND;
        *precSize = recUpdateArray(precordBuffer, pArray,  *precSize,
                          maxSize, pfield, pfieldBuffer);
    }
    else
        *precSize = recUpdate(precordBuffer, *precSize, maxSize, pfield,
                          pfieldBuffer);

    if (*precSize > maxSize)
        return RECTOOSMALL;  /* no space or template requested */

    return RECSUCCESS;

}  /* end recPutLONG */


/* PROGRAM: recPutBIG - stores a Big integer (64 bits)  in a record
 *                       It gets a structure
 *                             typedef struct {long ll; long hl} rec_big_t;
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 */
recStatus_t
recPutBIG(
        TEXT     *precordBuffer,   /* IN/OUT record buffer */
        ULONG     maxSize,         /* IN  size of input buffer */
        ULONG    *precSize,        /* OUT size of new record or size needed */
        ULONG     fieldNumber,     /* IN  number of field within record */
        ULONG     arrayIndex,      /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcBig_t  bigValue,        /* IN  value to assign to the field */
        ULONG     indicator)       /* IN  indicator flag */
{
    TTINY buffer[2*sizeof(svcBig_t)];  /* buffer to storage format field value*/

    TEXT  *pfieldBuffer = &buffer[0];  /* ptr to working buffer */
    TEXT  *pfield;                     /* Pointer to field within record 
                                       when taking it as a separate LONG */
    TEXT  *pArray;   /* ptr to begining of array within record */

    pfield = recFindField(precordBuffer, fieldNumber);
    if (pfield == NULL)
        return RECFLDNOTFOUND;

    pfieldBuffer = recFormatBIG(pfieldBuffer, sizeof(buffer), bigValue,
                                indicator);

    if (*pfield == VECTOR)
    {
        pArray = pfield;
        pfield = recFindArrayElement(pfield, arrayIndex);
        if (pfield == NULL)
            return RECFLDNOTFOUND;
        *precSize = recUpdateArray(precordBuffer, pArray,  *precSize,
                          maxSize, pfield, pfieldBuffer);
    }
    else
        *precSize = recUpdate(precordBuffer, *precSize, maxSize, pfield,
                          pfieldBuffer);

    if (*precSize > maxSize)
        return RECTOOSMALL;  /* no space or template requested */

    return RECSUCCESS;


}  /* end recPutBIG */


/* PROGRAM: recPutBYTES - Update a Field of a type byte_t (TEXT)
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 */
recStatus_t
recPutBYTES(
        TEXT            *precordBuffer, /* IN/OUT record buffer */
        ULONG            maxSize,       /* IN  size of input buffer */
        ULONG           *precSize,      /* OUT size of new rec or size needed */
        ULONG            fieldNumber,   /* IN  number of field within record */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcByteString_t *pfieldValue,   /* IN  value to assign to the field */
        ULONG            indicator)     /* IN  indicator flag */
{
    /*** General steps to update a field in a record buffer ***/
    /* 1. Position to the field in the record.
     * 2. Convert data value to storage format.
     * 3. Call record manager to update the record.
     * 4. Update size with current record size or size of record needed.
     * 5. return pass or fail.
     */

    TEXT  *pfieldBuffer = (TEXT *)utmalloc(MAXRECSZ);
    TEXT  *pfield;                     /* Pointer to field within record */
    TEXT  *pArray;   /* ptr to begining of array within record */
    int    rc = RECSUCCESS;

    pfield = recFindField(precordBuffer, fieldNumber);
    if (pfield == NULL)
    {
        rc =  RECFLDNOTFOUND;
        goto done;
    }

    recFormatBYTES(pfieldBuffer, MAXRECSZ, pfieldValue->pbyte,
                                pfieldValue->length, indicator);

    if (*pfield == VECTOR)
    {
        pArray = pfield;
        pfield = recFindArrayElement(pfield, arrayIndex);
        if (pfield == NULL)
        {
            rc =  RECFLDNOTFOUND;
            goto done;
        }
        *precSize = recUpdateArray(precordBuffer, pArray,  *precSize,
                          maxSize, pfield, pfieldBuffer);
    }
    else
        *precSize = recUpdate(precordBuffer, *precSize, maxSize, pfield,
                          pfieldBuffer);

    if (*precSize > maxSize)
    {
        rc =  RECTOOSMALL;  /* no space or template requested */
        goto done;
    }

done:
    utfree(pfieldBuffer);
    return rc;

}  /* end recPutBYTES */


/* PROGRAM: recPutDECIMAL - Update a Field of a type DECIMAL
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 */
recStatus_t
recPutDECIMAL(
        TEXT         *precordBuffer, /* IN/OUT record buffer */
        ULONG         maxSize,       /* IN  size of input buffer */
        ULONG        *precSize,      /* OUT size of new record or size needed */
        ULONG         fieldNumber,   /* number of field within record */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcDecimal_t *pfieldValue,   /* value to assign to the field */
        ULONG         indicator)     /* IN  indicator flag */
{
    TTINY buffer[SVCMAXDIGIT];      /* buffer to storage format field value*/

    TEXT  *pfieldBuffer = &buffer[0];  /* ptr to working buffer */
    TEXT  *pfield;                     /* Pointer to field within record 
                                       when taking it as a separate LONG */
    TEXT  *pArray;   /* ptr to begining of array within record */

    pfield = recFindField(precordBuffer, fieldNumber);
    if (pfield == NULL)
        return RECFLDNOTFOUND;

    pfieldBuffer = recFormatDECIMAL(pfieldBuffer, sizeof(buffer), pfieldValue,
                                    indicator);

    if (*pfield == VECTOR)
    {
        pArray = pfield;
        pfield = recFindArrayElement(pfield, arrayIndex);
        if (pfield == NULL)
            return RECFLDNOTFOUND;
        *precSize = recUpdateArray(precordBuffer, pArray,  *precSize,
                          maxSize, pfield, pfieldBuffer);
    }
    else
        *precSize = recUpdate(precordBuffer, *precSize, maxSize, pfield,
                          pfieldBuffer);

    if (*precSize > maxSize)
        return RECTOOSMALL;  /* no space or template requested */

    return RECSUCCESS;

}  /* end recPutDECIMAL */


/* PROGRAM: recPutFLOAT - Update a Field of a type FLOAT
 *                        (Single Precision Float)
 *
 * (Equates to Dharma REAL type)
 * It will be stored in the database as a machine independant 4 byte value.
 * This is equivilent to the C language float type.
 * It is stored as a variable length machine independant value whose
 * variable length will always be 4.  In other words, the stored value is 5
 * bytes consisting of a one byte length value which will always be 4 followed
 * by the 4 byte data value.
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 */
recStatus_t
recPutFLOAT(
        TEXT       *precordBuffer,  /* IN/OUT record buffer */
        ULONG       maxSize,        /* IN  size of input buffer */
        ULONG      *precSize,       /* OUT size of new record or size needed */
        ULONG       fieldNumber,    /* IN  number of field within record */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcFloat_t  fieldValue,     /* IN  value to assign to the field */
        ULONG       indicator)      /* IN  indicator flag */
{
    TTINY buffer[2*sizeof(LONG)];      /* buffer to storage format field value*/

    TEXT  *pfieldBuffer = &buffer[0];  /* ptr to working buffer */
    TEXT  *pfield;                     /* Pointer to field within record 
                                       when taking it as a separate LONG */
    TEXT  *pArray;   /* ptr to begining of array within record */

    pfield = recFindField(precordBuffer, fieldNumber);
    if (pfield == NULL)
        return RECFLDNOTFOUND;

    pfieldBuffer = recFormatFLOAT(pfieldBuffer, sizeof(buffer), &fieldValue,
                                  indicator);

    if (*pfield == VECTOR)
    {
        pArray = pfield;
        pfield = recFindArrayElement(pfield, arrayIndex);
        if (pfield == NULL)
            return RECFLDNOTFOUND;
        *precSize = recUpdateArray(precordBuffer, pArray,  *precSize,
                          maxSize, pfield, pfieldBuffer);
    }
    else
        *precSize = recUpdate(precordBuffer, *precSize, maxSize, pfield,
                          pfieldBuffer);

    if (*precSize > maxSize)
        return RECTOOSMALL;  /* no space or template requested */

    return RECSUCCESS;

}  /* end recPutFLOAT */


/* PROGRAM: recPutDOUBLE - Update a Field of a type DOUBLE
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 */
recStatus_t
recPutDOUBLE(
        TEXT        *precordBuffer,  /* IN/OUT record buffer */
        ULONG        maxSize,        /* IN  size of input buffer */
        ULONG       *precSize,       /* OUT size of new record or size needed */
        ULONG        fieldNumber,    /* IN  number of field within record */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcDouble_t  fieldValue,     /* IN  value to assign to the field */
        ULONG        indicator)      /* IN  indicator flag */
{
    TTINY buffer[3*sizeof(LONG)];      /* buffer to storage format field value*/

    TEXT  *pfieldBuffer = &buffer[0];  /* ptr to working buffer */
    TEXT  *pfield;                     /* Pointer to field within record 
                                       when taking it as a separate LONG */
    TEXT  *pArray;   /* ptr to begining of array within record */

    pfield = recFindField(precordBuffer, fieldNumber);
    if (pfield == NULL)
        return RECFLDNOTFOUND;

    pfieldBuffer = recFormatDOUBLE(pfieldBuffer, sizeof(buffer), &fieldValue,
                                   indicator);
    if (*pfield == VECTOR)
    {
        pArray = pfield;
        pfield = recFindArrayElement(pfield, arrayIndex);
        if (pfield == NULL)
            return RECFLDNOTFOUND;
        *precSize = recUpdateArray(precordBuffer, pArray,  *precSize,
                          maxSize, pfield, pfieldBuffer);
    }
    else
        *precSize = recUpdate(precordBuffer, *precSize, maxSize, pfield,
                          pfieldBuffer);

    if (*precSize > maxSize)
        return RECTOOSMALL;  /* no space or template requested */

    return RECSUCCESS;

}  /* end recPutDOUBLE */


/* PROGRAM: recPutDATE - Update a Field of a type DATE
 *
 * The date is converted from recDate_t to the number of days since 05/02/50
 * by recFormatDATE()/recStoreDate().  It is then stored as a LONG.
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 *          RECBADYEAR
 *          RECBADMON
 *          RECBADDAY
 *          RECBADMODE
 */
recStatus_t
recPutDATE(
        TEXT        *precordBuffer,  /* IN/OUT  record buffer */
        ULONG        maxSize,        /* IN  size of input buffer */
        ULONG       *precSize,       /* OUT size of new record or size needed */
        ULONG        fieldNumber,    /* IN  number of field within record */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcDate_t   *pfieldValue,    /* IN  value to assign to the field */
        ULONG        indicator)      /* IN  indicator flag */
{
    recStatus_t returnCode;

    TTINY buffer[2*sizeof(LONG)];      /* buffer to storage format field value*/
 
    TEXT  *pfieldBuffer = &buffer[0];  /* ptr to working buffer */
    TEXT  *pfield;                     /* Pointer to field within record */
    TEXT  *pArray;   /* ptr to begining of array within record */
 
    pfield = recFindField(precordBuffer, fieldNumber);
    if (pfield == NULL)
        return RECFLDNOTFOUND;
 
    returnCode = recFormatDATE(pfieldBuffer, sizeof(buffer), pfieldValue,
                               indicator);
    if (returnCode != RECSUCCESS)
        return returnCode;

    if (*pfield == VECTOR)
    {
        pArray = pfield;
        pfield = recFindArrayElement(pfield, arrayIndex);
        if (pfield == NULL)
            return RECFLDNOTFOUND;
        *precSize = recUpdateArray(precordBuffer, pArray,  *precSize,
                          maxSize, pfield, pfieldBuffer);
    }
    else
        *precSize = recUpdate(precordBuffer, *precSize, maxSize, pfield,
                          pfieldBuffer);

    if (*precSize > maxSize)
         returnCode = RECTOOSMALL;  /* no space or template requested */
    else
        returnCode = RECSUCCESS;
 
    return returnCode;

}  /* end recPutDATE */


/* PROGRAM: recPutTIME - Update a Field of a type TIME
 *
 * retTime_t is converted to # of seconds since midnight by recFormatTIME()
 * The converted value is then stored as a LONG.
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 */
recStatus_t
recPutTIME(
        TEXT        *precordBuffer, /* IN/OUT  record buffer */
        ULONG        maxSize,       /* IN  size of input buffer */
        ULONG       *precSize,      /* OUT size of new record or size needed */
        ULONG        fieldNumber,   /* IN  number of field within record */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcTime_t   *pfieldValue,   /* IN  value to assign to the field */
        ULONG        indicator)     /* IN  indicator flag */
{
    recStatus_t returnCode;

    TTINY buffer[2*sizeof(LONG)];      /* buffer to storage format field value*/
 
    TEXT  *pfieldBuffer = &buffer[0];  /* ptr to working buffer */
    TEXT  *pfield;                     /* Pointer to field within record */
    TEXT  *pArray;   /* ptr to begining of array within record */
 
    pfield = recFindField(precordBuffer, fieldNumber);
    if (pfield == NULL)
        return RECFLDNOTFOUND;
 
    pfieldBuffer = recFormatTIME(pfieldBuffer, sizeof(buffer), pfieldValue,
                                 indicator);
    if (*pfield == VECTOR)
    {
        pArray = pfield;
        pfield = recFindArrayElement(pfield, arrayIndex);
        if (pfield == NULL)
            return RECFLDNOTFOUND;
        *precSize = recUpdateArray(precordBuffer, pArray,  *precSize,
                          maxSize, pfield, pfieldBuffer);
    }
    else
        *precSize = recUpdate(precordBuffer, *precSize, maxSize, pfield,
                          pfieldBuffer);

    if (*precSize > maxSize)
         returnCode = RECTOOSMALL;  /* no space or template requested */
    else
        returnCode = RECSUCCESS;
 
    return returnCode;

}  /* end recPutTIME */


/* PROGRAM: recPutTIMESTAMP - Update a Field of a type TIMESTAMP
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 */
recStatus_t
recPutTIMESTAMP(
        TEXT           *precordBuffer, /* IN/OUT record buffer */
        ULONG           maxSize,       /* IN size of input buffer */
        ULONG          *precSize,      /* OUT size of new rec or size needed */
        ULONG           fieldNumber,   /* IN number of field within record */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcTimeStamp_t *pfieldValue,   /* IN value to assign to the field */
        ULONG           indicator)     /* IN  indicator flag */
{
    recStatus_t returnCode;

    TTINY buffer[4*sizeof(LONG)];      /* buffer to storage format field value*/
 
    TEXT  *pfieldBuffer = &buffer[0];  /* ptr to working buffer */
    TEXT  *pfield;                     /* Pointer to field within record */
    TEXT  *pArray;   /* ptr to begining of array within record */
 
    pfield = recFindField(precordBuffer, fieldNumber);
    if (pfield == NULL)
        return RECFLDNOTFOUND;
 
    returnCode = recFormatTIMESTAMP(pfieldBuffer, sizeof(buffer),
                                    pfieldValue, indicator);
    if (returnCode != RECSUCCESS)
        return returnCode;

    if (*pfield == VECTOR)
    {
        pArray = pfield;
        pfield = recFindArrayElement(pfield, arrayIndex);
        if (pfield == NULL)
            return RECFLDNOTFOUND;
        *precSize = recUpdateArray(precordBuffer, pArray,  *precSize,
                          maxSize, pfield, pfieldBuffer);
    }
    else
        *precSize = recUpdate(precordBuffer, *precSize, maxSize, pfield,
                          pfieldBuffer);

    if (*precSize > maxSize)
         returnCode = RECTOOSMALL;  /* no space or template requested */
    else
        returnCode = RECSUCCESS;
 
    return returnCode;

}  /* end recPutTIMESTAMP */


/* PROGRAM: recPutBOOL - Update a Field of a type BOOL
 *
 *          Boolean is zero or one. If the value is zero,
 *          it is stored as a zero length integer. If it is
 *          one, it has a length byte of one and a value byte
 *          of one.
 *
 * RETURNS: RECSUCCESS
 *          RECTOOSMALL
 *          RECFLDNOTFOUND
 */
recStatus_t
recPutBOOL(
        TEXT      *precordBuffer,   /* IN/OUT  record buffer */
        ULONG      maxSize,         /* IN  size of input buffer */
        ULONG     *precSize,        /* OUT size of new record or size needed */
        ULONG      fieldNumber,     /* IN  number of field within record */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcBool_t  fieldValue,      /* IN  value to assign to the field */
        ULONG      indicator)       /* IN  indicator flag */
{
    /*** General steps to update a field in a record buffer ***/
    /* 1. Position to the field in the record.
     * 2. Convert data value to storage format.
     * 3. Call record manager to update the record.
     * 4. Update size with current record size or size of record needed.
     * 5. return pass or fail.
     *  All of the above is done by calling recPutBYTES().
     */

    /* only store true or false - not a "real value" */
    if (fieldValue)
        fieldValue = 1;

    return recPutTINY(precordBuffer, maxSize, precSize,
                      fieldNumber, arrayIndex, fieldValue, indicator);

}  /* end recPutBOOL */


/* PROGRAM: recSetTableNum - set the table number of the given record,
 * respecting the fact that the record format may be "old" (first field is
 * a scalar) or "new" (first field is a vector). 
 *
 * Returns RECSUCCESS if it suceeds, otherwise returns what recPutLONG() 
 * returns. If the record buffer is too small to fit the new value, the
 * size needed will be reflected in precSize.
 */

recStatus_t
recSetTableNum(TEXT *pRecord, ULONG maxSize, ULONG *precSize,svcLong_t tableid)
{
    return recPutLONG(pRecord, maxSize, precSize, (ULONG)1,
                      (ULONG) (*(recFieldFirst(pRecord)) == VECTOR
                               ? REC_TBLNUM : 0),
                      tableid, 0);
}
