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

/* FILE: recget.c - contains record retrieval manipulation support
 *
 */
    
/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"   /* database manager */

#include "recpub.h"     /* public interface */
#include "svcspub.h"
#include "utdpub.h"
#include "utspub.h"     /* for xlngv */


/*** LOCAL FUNCTION PROTOTYPES ***/

LOCALF recStatus_t recGetFieldIndicator(TEXT *precordBuffer, ULONG fieldNumber,
                          ULONG arrayIndex, ULONG *pindicator, TEXT **ppfield);

/*** END LOCAL FUNCTION PROTOTYPES ***/

#define VECTORPFX  ( 1 + sizeof(COUNT) )


/* PROGRAM: recGetTableNum -- extracts the table (a.k.a file) number
 *          from a progress recod.
 *          Use instead of fmgeti, because this is a lot faster.
 *
 * RETURNS:
 *      0               error.
 *      non 0           the file no.
 */
COUNT
recGetTableNum (TEXT *ps)
{
    recStatus_t retCode;
    svcLong_t   longValue;
    ULONG      indicator;

    if (*(recFieldFirst(ps)) == VECTOR)
        retCode = recGetLONG(ps, (ULONG)1, (ULONG)REC_TBLNUM,
                             &longValue, &indicator);
    else
        retCode = recGetLONG(ps, (ULONG)1, (ULONG)0, &longValue, &indicator);
 
    if (indicator == SVCUNKNOWN || retCode == RECFLDNOTFOUND)
        return 0;
   else
        return (COUNT)longValue;

}  /* end recGetTableNum */


/* PROGRAM: recGetField - get data tiny integer value out of record.
 *              gets it as a long and converts it to tiny
 *
 * RETURNS: data value in pdataType structure.
 *          RECSUCCESS
 *          RECINVALIDTYPE
 */
recStatus_t
recGetField(
        TEXT          *precordBuffer, /* IN  record buffer */
        ULONG          fieldNumber,   /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcDataType_t *pdataType)     /* OUT value retrieved from field */
{
    recStatus_t returnCode;

    switch(pdataType->type)
    {
      case(SVCBLOB):
      case(SVCBINARY):
      case(SVCBYTESTRING):
      case(SVCCHARACTER):
        returnCode = recGetBYTES(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcByteString,
                                &pdataType->indicator);
      break;
      case(SVCTINY):
        returnCode = recGetTINY(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcTiny,
                                &pdataType->indicator);
      break;
      case(SVCSMALL):
        returnCode = recGetSMALL(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcSmall,
                                &pdataType->indicator);
      break;
      case(SVCBOOL):
        returnCode = recGetBOOL(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcBool,
                                &pdataType->indicator);
      break;
      case(SVCLONG):
        returnCode = recGetLONG(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcLong,
                                &pdataType->indicator);
      break;
      case(SVCBIG):
        returnCode = recGetBIG(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcBig,
                                &pdataType->indicator);
      break;
      case(SVCFLOAT):
        returnCode = recGetFLOAT(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcFloat,
                                &pdataType->indicator);
      break;
      case(SVCDOUBLE):
        returnCode = recGetDOUBLE(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcDouble,
                                &pdataType->indicator);
      break;
      case(SVCDATE):
        returnCode = recGetDATE(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcDate,
                                &pdataType->indicator);
      break;
      case(SVCTIME):
        returnCode = recGetTIME(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcTime,
                                &pdataType->indicator);
      break;
      case(SVCTIMESTAMP):
        returnCode = recGetTIMESTAMP(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcTimeStamp,
                                &pdataType->indicator);
      break;
      case(SVCDECIMAL):
        returnCode = recGetDECIMAL(precordBuffer, fieldNumber,arrayIndex,
                                &pdataType->f.svcDecimal,
                                &pdataType->indicator);
      break;
      default:
          returnCode = RECINVALIDTYPE;
      break;
    }
  
    return returnCode;

}  /* end recGetField */


/* PROGRAM: recGetTINY - get data tiny integer value out of record.
 *              gets it as a long and converts it to tiny
 *
 * RETURNS: TINY data value.
 */
recStatus_t
recGetTINY(
        TEXT      *precordBuffer,     /* IN  record buffer */
        ULONG      fieldNumber,       /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcTiny_t *pfieldValue,       /* OUT value retrieved from field */
        ULONG      *pindicator)       /* OUT determines known/unknown value */
{
    recStatus_t returnCode;
    svcLong_t   longValue;

    returnCode =  recGetLONG(precordBuffer, fieldNumber, arrayIndex,
                             &longValue, pindicator);
 
    *pfieldValue = (svcTiny_t)longValue;
 
    return returnCode;

}  /* end recGetTINY */


/* PROGRAM: recGetSMALL - get data small integer (COUNT) value out of record.
 *              gets it as a long and converts it to count
 *
 * RETURNS: COUNT data value.
 */
recStatus_t
recGetSMALL(
        TEXT       *precordBuffer,     /* IN  record buffer */
        ULONG       fieldNumber,       /* IN  field number */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcSmall_t *pfieldValue,       /* OUT value retrieved from field */
        ULONG      *pindicator)        /* OUT determines known/unknown value */
{
    recStatus_t returnCode;
    svcLong_t   longValue;

    returnCode = recGetLONG(precordBuffer, fieldNumber,arrayIndex,
                            &longValue, pindicator);
 
    *pfieldValue = (svcSmall_t)longValue;
 
    return returnCode;

}  /* end recGetSMALL */


/* PROGRAM: recGetLONG - get data long value out of record.
 *
 * RETURNS: long data value.
 */
recStatus_t
recGetLONG(
        TEXT         *precordBuffer,  /* IN  record buffer */
        ULONG         fieldNumber,    /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcLong_t    *pfieldValue,    /* OUT value retrieved from field   */
        ULONG        *pindicator)     /* OUT determines known/unknown value */
{
    recStatus_t returnCode;
    TEXT *pfield;        /* ptr to current field pointer */

    returnCode = recGetFieldIndicator(precordBuffer,
                       fieldNumber, arrayIndex,
                       pindicator, &pfield);
    if ( (returnCode != RECSUCCESS) || *pindicator )
        return returnCode;

    *pfieldValue = ( xlngv(pfield+recFieldPrefix(pfield),
                           (COUNT)recFieldLength(pfield)) );

    return RECSUCCESS;

}  /* end recGetLONG */

/* PROGRAM: recGetBIG - get data LONG64 value out of record.
 *
 * RETURNS: long data value.
 */
recStatus_t
recGetBIG(
        TEXT         *precordBuffer,  /* IN  record buffer */
        ULONG         fieldNumber,    /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcBig_t     *pfieldValue,    /* OUT value retrieved from field   */
        ULONG        *pindicator)     /* OUT determines known/unknown value */
{
    recStatus_t returnCode;
    TEXT *pfield;        /* ptr to current field pointer */

    returnCode = recGetFieldIndicator(precordBuffer,
                       fieldNumber, arrayIndex,
                       pindicator, &pfield);
    if ( (returnCode != RECSUCCESS) || *pindicator )
        return returnCode;

    *pfieldValue = ( xlngv64(pfield+recFieldPrefix(pfield),
                           (COUNT)recFieldLength(pfield)) );

    return RECSUCCESS;

}  /* end recGetBIG */

#if 0
/* PROGRAM: recGetBIG - get data Big integer value out of record.
 *
 * RETURNS: 
 *      The resulted Big integer is returned in a structure of a type rec_big_t
 *          pointed by the parameter pBigValue.
 */
recStatus_t
recGetBIG(
        TEXT      *precordBuffer, /* IN  record buffer */
        ULONG      fieldNumber,   /* IN  field number */
        ULONG      arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcBig_t  *bigValue,      /* OUT value retrieved from field */
        ULONG     *pindicator)    /* OUT determines known/unknown value */
{
    recStatus_t returnCode;
    TEXT        *pfield;        /* ptr to current field pointer */
    COUNT       length;         /* field length */
    COUNT       prefix;

    returnCode = recGetFieldIndicator(precordBuffer,
                       fieldNumber, arrayIndex,
                       pindicator, &pfield);
    if ( (returnCode != RECSUCCESS) || *pindicator )
        return returnCode;

    length = (COUNT)recFieldLength(pfield);
    prefix = (COUNT)recFieldPrefix(pfield);

    if (length < 5)
    {
        /* This is the case where the value fits in one LONG */
       
        pBigValue->ll = xlngv(pfield+prefix,length);
        if(pBigValue->ll < 0) 
            pBigValue->hl = -1;
        else  
            pBigValue->hl = 0;
    }
    else
    {
        /* here we need to expand both the high and low LONGs */

        if (length == 5 && *(pfield+prefix) == 0)
            pBigValue->hl = 0;
        else
            pBigValue->hl = xlngv(pfield+prefix, (COUNT)(length - 4));

        pBigValue->ll = xlng(pfield+prefix+length-4);
    }

    return RECSUCCESS;

}  /* end recGetBIG */
#endif


/* PROGRAM: recGetBYTES - Get value of a field of a type svcByteString_t (TEXT)
 *
 * RETURNS: RECSUCCESS
 *          RECFLDNOTFOUND
 *          RECFLDTRUNCATED
 */
recStatus_t
recGetBYTES(
        TEXT            *precordBuffer, /* IN  record buffer */
        ULONG            fieldNumber,   /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcByteString_t *pfieldValue,   /* IN/OUT value and length of field  */
        ULONG           *pindicator)    /* OUT determines known/unknown value */
{
    recStatus_t  returnCode;
    ULONG        getLength;
    TEXT        *pfield;        /* ptr to current field pointer */

    returnCode = recGetFieldIndicator(precordBuffer,
                       fieldNumber, arrayIndex,
                       pindicator, &pfield);
    if ( (returnCode != RECSUCCESS) || *pindicator )
        return returnCode;

    pfieldValue->length = recFieldLength(pfield);
    if (pfieldValue->length > pfieldValue->size)
    {
        *pindicator = SVCTRUNCVALUE;
        getLength = pfieldValue->size;
        returnCode = RECFLDTRUNCATED;
    }
    else
    {
        getLength = pfieldValue->length;
        returnCode = RECSUCCESS;
    }

    /* NOTE: the null terminator is not included in the string length.
     *       It will only be added if there is room.
     */
    /* now get the data */
    bufcop(pfieldValue->pbyte, pfield + recFieldPrefix(pfield),
           getLength);
    if (pfieldValue->size > getLength)
        pfieldValue->pbyte[getLength] = '\0';

    return returnCode;

}  /* end recGetBYTES */


/* PROGRAM: recGetDECIMAL - Get a Field of a type DECIMAL
 *
 * The first byte contains the length of the field.
 *
 * The decimal number is stored as a control byte followed by bytes
 * containing 2 decimal digits per byte (packed decimal).
 *
 * If the decimal number is negative,
 *    the control byte = # of digits to the right of the decimal point.
 * if the decimal number is positive,
 *    the  control byte = # of digits to the right of the decimal point + 0x80.
 *
 * Following the control byte is the packed decimal value.
 * If the number contains an uneven number of digits, the last byte is padded
 * with 0x0f.
 *
 * RETURNS: RECSUCCESS
 *          RECFLDNOTFOUND
 */
recStatus_t
recGetDECIMAL(
        TEXT           *precordBuffer, /* IN  record buffer */
        ULONG           fieldNumber,   /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcDecimal_t   *pfieldValue,   /* OUT value retrieved from field */
        ULONG          *pindicator)    /* OUT determines known/unknown value */
{
    recStatus_t returnCode;
    ULONG fieldLen;

    TEXT *pfield;        /* ptr to current field pointer */
    TEXT *psourceDigit;
    TEXT *plastDigit;
    TEXT *pdigitTarget;

    returnCode = recGetFieldIndicator(precordBuffer,
                       fieldNumber, arrayIndex,
                       pindicator, &pfield);
    if ( (returnCode != RECSUCCESS) || *pindicator )
        return returnCode;

    fieldLen = recFieldLength(pfield);

    if (fieldLen == 0)
    {
        pfieldValue->plus = 1;
        pfieldValue->totalDigits = 0;
        pfieldValue->decimals = 0;
        return RECSUCCESS;
    }
    psourceDigit = pfield + 1;
    plastDigit   = psourceDigit + fieldLen;
 
    pfieldValue->plus     = psourceDigit[0] & 0x80;
    pfieldValue->decimals = psourceDigit[0] & 0x7f;
 
    for (pdigitTarget = pfieldValue->pdigit;
         ++psourceDigit < plastDigit;
         pdigitTarget++)
    {
       *pdigitTarget = *psourceDigit >> 4;
       pdigitTarget++;
       *pdigitTarget = *psourceDigit & 0x0f;
    }
    if (*(pdigitTarget -1) == 0x0f)
        pdigitTarget--;
 
    pfieldValue->totalDigits = pdigitTarget - pfieldValue->pdigit;

    return RECSUCCESS;

}  /* end recGetDECIMAL */


/* PROGRAM: recGetFLOAT - Get a Field of a type FLOAT
 *                        (Single Precision Float)
 *
 * (Equates to Dharma REAL type)
 * This is equivilent to the C language float type.
 *
 * It was stored in the database as a 4 byte machine independant value whose
 * length is always 4.  In other words, the stored value is 5
 * bytes consisting of a one byte length value which will always be 4 followed
 * by the 4 byte data value.  It will be extracted from the record using xlngv.
 *
 * RETURNS: RECSUCCESS
 *          RECFLDNOTFOUND
 */
recStatus_t
recGetFLOAT(
        TEXT           *precordBuffer, /* IN  record buffer */
        ULONG           fieldNumber,   /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcFloat_t     *pfieldValue,   /* OUT value retrieved from field */
        ULONG          *pindicator)    /* OUT determines known/unknown value */
{
    recStatus_t returnCode;
    TEXT *pfield;        /* ptr to current field pointer */
    LONG longValue;

    returnCode = recGetFieldIndicator(precordBuffer,
                       fieldNumber, arrayIndex,
                       pindicator, &pfield);
    if ( (returnCode != RECSUCCESS) || *pindicator )
        return returnCode;

    longValue = xlng(pfield+recFieldPrefix(pfield));

    bufcop(pfieldValue, &longValue, sizeof(LONG));

    return RECSUCCESS;

}  /* end recGetFLOAT */


/* PROGRAM: recGetDOUBLE - Get a Field of a type DOUBLE
 *
 * Double precision value stored as length value (always 8),
 * slng() high part of value, slng() low part of value.
 *
 * RETURNS: RECSUCCESS
 *          RECFLDNOTFOUND
 */
recStatus_t
recGetDOUBLE(
        TEXT           *precordBuffer, /* IN  record buffer */
        ULONG           fieldNumber,   /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcDouble_t    *pfieldValue,   /* OUT value retrieved from field */
        ULONG          *pindicator)    /* OUT determines known/unknown value */
{
    recStatus_t returnCode;
    TEXT *pfield;        /* ptr to current field pointer */

    returnCode = recGetFieldIndicator(precordBuffer,
                       fieldNumber, arrayIndex,
                       pindicator, &pfield);
    if ( (returnCode != RECSUCCESS) || *pindicator )
        return returnCode;

    ((recDouble_t *)pfieldValue)->highValue  =
                          xlng(pfield+recFieldPrefix(pfield));

    ((recDouble_t *)pfieldValue)->lowValue  = 
                          xlng(pfield+recFieldPrefix(pfield)+ sizeof(LONG) );

    return RECSUCCESS;

}  /* end recGetDOUBLE */


/* PROGRAM: recGetDATE - Get a Field of a type DATE
 *
 * RETURNS: RECSUCCESS
 *          RECFLDNOTFOUND
 *          RECFAILURE  results from error during recExpandDate -
 *                      SDAYSIZE    sday is out of range
 *                      BADMODE     mode is not JULIAN or GREGORIAN
 */
recStatus_t
recGetDATE(
        TEXT           *precordBuffer, /* IN  record buffer */
        ULONG           fieldNumber,   /* IN field number   */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcDate_t      *pfieldValue,   /* OUT value retrieved from field */
        ULONG          *pindicator)    /* OUT determines known/unknown value */
{
    recStatus_t    returnCode;
    svcLong_t      sDate;

    returnCode = recGetLONG(precordBuffer, fieldNumber, arrayIndex,
                                &sDate, pindicator);
    if ((returnCode != RECSUCCESS) || *pindicator )
        return returnCode;

    sDate += DATEBASE;
    /* Convert LONG date back to structure format... */
    returnCode = utLongToDate(sDate, (utDate_t *)pfieldValue); 

    return returnCode;

}  /* end recGetDATE */


/* PROGRAM: recGetTIME - Get a Field of a type TIME
 *
 *          
 * RETURNS: RECSUCCESS
 *          RECFLDNOTFOUND
 */
recStatus_t
recGetTIME(
        TEXT           *precordBuffer,  /* IN  record buffer */
        ULONG           fieldNumber,    /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcTime_t      *pfieldValue,    /* OUT value retrieved from field */
        ULONG          *pindicator)     /* OUT determines known/unknown value */
{
    recStatus_t returnCode;
    LONG        sTime = 0;

    returnCode = recGetLONG(precordBuffer, fieldNumber, arrayIndex,
                   (svcLong_t *)&sTime, pindicator);
    if ((returnCode != RECSUCCESS) || *pindicator )
        return returnCode;
    
    /* Convert LONG date back to structure format... */
    returnCode = utLongToTime(sTime, (utTime_t *)pfieldValue);

    return returnCode;

}  /* end recGetTIME */


/* PROGRAM: recGetTIMESTAMP - Get a Field of a type TIMESTAMP
 *
 * RETURNS: RECSUCCESS
 *          RECFLDNOTFOUND
 */
recStatus_t
recGetTIMESTAMP(
        TEXT           *precordBuffer,  /* IN  record buffer */
        ULONG           fieldNumber,    /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcTimeStamp_t *pfieldValue,    /* OUT value retrieved from field */
        ULONG          *pindicator)     /* OUT determines known/unknown value */
{
    recStatus_t returnCode;

    LONG sDate;
    LONG sTime;

    TEXT *pfield;        /* ptr to current field pointer */

    returnCode = recGetFieldIndicator(precordBuffer,
                       fieldNumber, arrayIndex,
                       pindicator, &pfield);
    if ( (returnCode != RECSUCCESS) || *pindicator )
        return returnCode;

    sDate  = xlng(pfield+recFieldPrefix(pfield) );
    sTime  = xlng(pfield+recFieldPrefix(pfield)+ sizeof(LONG) );

    returnCode = utLongToDate(sDate, (utDate_t *)(&pfieldValue->tsDate));
    if (returnCode)
        return returnCode;

    returnCode = utLongToTime(sTime, (utTime_t *)(&pfieldValue->tsTime));

    return returnCode;

}  /* end recGetTIMESTAMP */


/* PROGRAM: recGetBOOL - Get a Field of a type BOOL
 *
 *
 * RETURNS: RECSUCCESS
 *          RECFLDNOTFOUND
 */
recStatus_t
recGetBOOL(
        TEXT           *precordBuffer, /* IN  record buffer */
        ULONG           fieldNumber,   /* IN  field number  */
        ULONG          arrayIndex,    /* IN  index in the array for array filed
                                             NULL for regular field  */
        svcBool_t      *pfieldValue,   /* OUT value retrieved from field */
        ULONG          *pindicator)     /* OUT determines known/unknown value */
{
    recStatus_t returnCode;
    TEXT *pfield;        /* ptr to current field pointer */

    returnCode = recGetFieldIndicator(precordBuffer,
                       fieldNumber, arrayIndex,
                       pindicator, &pfield);
    if ( (returnCode != RECSUCCESS) || *pindicator )
        return returnCode;

    /* if the filed has a length, then its value it TRUE else FALSE */
     if ( *(pfield+recFieldPrefix(pfield)) )
         *pfieldValue = 1;
     else
         *pfieldValue = 0;

    return RECSUCCESS;

}  /* end recGetBOOL */


/* PROGRAM: recGetFieldIndicator - position to the field and process the prefix
 *
 * RETURNS: RECSUCCESS and indicator set to zero or indicator
 *          RECFLDNOTFOUND
 */
LOCALF recStatus_t
recGetFieldIndicator(
        TEXT           *precordBuffer, /* IN  record buffer */
        ULONG           fieldNumber,   /* IN  field number  */
        ULONG           arrayIndex,    /* IN  index in the array for array filed
                                              NULL for regular field  */
        ULONG          *pindicator,
        TEXT           **ppfield)      /* ptr to field located */
{
    *ppfield = recFindField(precordBuffer, fieldNumber);
    if (*ppfield == NULL)
        return RECFLDNOTFOUND;

    if (**ppfield == VECTOR)
    {
        *ppfield = recFindArrayElement(*ppfield, arrayIndex);
        if (*ppfield == NULL)
            return RECFLDNOTFOUND;
    }

    switch(**ppfield)
    {
      case VUNKNOWN:
        *pindicator = SVCUNKNOWN;
        break;
      case TODAY:
        *pindicator = SVCTODAY;
        break;
      default:
        *pindicator = 0;
        break;
    }

    return RECSUCCESS;

}  /* end recGetFieldIndicator */
