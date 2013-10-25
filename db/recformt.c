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

/* FILE: recformt.c - contains record formatting support
 *
 */
    
/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"   /* database manager */

#include "recpub.h"     /* public interface */
#include "svcspub.h"
#include "utcpub.h"     /* for chrcop */
#include "utdpub.h"
#include "utspub.h"     /* for slngv */

#define FASSERT(cond,msg)

/*** LOCAL FUNCTION PROTOTYPES ***/

LOCALF TEXT *recFormatIndicator(TEXT *pBuffer, ULONG bufSize, ULONG indicator);

/*** END LOCAL FUNCTION PROTOTYPES ***/

#define VECTORPFX  ( 1 + sizeof(COUNT) )

/* PROGRAM: recFormat - format a new record with a number of null fields
 *
 * Record is formatted with leading length, skip table (opt) and null fields.
 * This is the fastest way to materialize a template record or array field
 * in memory.
 *
 * NOTE WELL: This should not be used for materializing record templates since
 *            it does not set up the first field of the record using the new
 *            record format (where the first field is a vector). Since this
 *            function is used to materialize an array field, it should not be
 *            changed to make the first field a vector.
 *
 * If the returned size is greater than maxsize there was not enough space 
 * in the provided buffer and the required buffer size is returned.
 *
 * RETURNS: total size of formatted record
 *          if record does not fit in buffer then returns total desired size
 */
ULONG
recFormat(
        TEXT *pRecord,  /* pointer to record buffer */
        ULONG maxSize,  /* size of buffer */
        ULONG nulls)      /* number of null fields required */
{
   TEXT *ps = pRecord;          /* working pointer */
   ULONG skipEntries;           /* number of skip table entries */
   ULONG skipSize;              /* total size of skip table */
   ULONG recSize;               /* current record size */
   ULONG offset;                /* offset into record */


   /* check assumption made when using xct/sct functions here */
   FASSERT((SKIPENTLEN == 2), "unexpected skiptable entry size");
   FASSERT((SKIPPFXLEN == 3), "unexpected skiptable prefix size");
   
   /* determine number of skip table entries needed and size of skip table */
   if (nulls >= SKIPUNIT)
   {
      skipEntries = nulls / SKIPUNIT;   /* assumes file number field */
      skipSize = (skipEntries * SKIPENTLEN) + SKIPPFXLEN;
   }
   else
   {
      skipEntries = 0;
      skipSize = 0;
   }

   /* determine total formatted record size (skipSize + nulls + ENDREC) */
   recSize = skipSize + nulls + 1;
   
   if (recSize > maxSize)
   {
       /* not enough space, return needed size */
       return recSize;
   }

   /* build the formatted record starting with skip table if needed */ 
   if (skipSize > 0)
   {
       *ps = SKIPTBL;                           /* store skip flag and */
       /* BUM - Assuming ULONG skipEntries * SKIPENTLEN < 2^ 15 */
       /* store skip table length */
       sct(ps + 1, (COUNT)(skipEntries * SKIPENTLEN));
       
       ps += SKIPPFXLEN;                       /* move to first skip entry */

       for (offset = SKIPUNIT-1;   /* TODO: is this right? */
            skipEntries > 0; 
            skipEntries--, offset += SKIPUNIT, ps += SKIPENTLEN)
       {
           /* BUM - Assuming ULONG offset < 2^15 */
           sct(ps, (COUNT)offset);             /* store each table entry */
       }
   }
   
   /* blast in null field values */
   chrcop(ps, VUNKNOWN, nulls);
   ps += nulls;

   *ps = ENDREC;
   
   /* return the length of the record */
   recSize = (ps - pRecord) + 1;

   FASSERT(*(pRecord + recSize - 1) == ENDREC,
            "recFormat: invalid record detected");

   return recSize;
   
}  /* end recFormat */



/* PROGRAM: recGetArraySize- 
 * This function returns 1 less than the array size needed to store 
 * numEntries.
 *
 * RETURNS:  size of vector field - 1
 *          
 *
 */


ULONG
recGetArraySize( ULONG numEntries)
{
    ULONG subRecSize;
    ULONG arraySizeSav;
 
    /* determine size needed for subRecord by passing 0 to recFormat
     * NOTE: We are expecting that if maxSize is passed as 0 that
     *       recFormat() will have no side effects.
     */
    arraySizeSav = recFormat(NULL, 0, numEntries);
 
    /* add up actual size needed - Prefix size, recSize, ENDREC
     * - 1 because we are replacing an existing null field
     */
    subRecSize = VECTORPFX + arraySizeSav;
    return (subRecSize - 1);
 

}
/********* This is an old procedure should be replaced by recInitArray ***/

/* PROGRAM: recFormatARRAY - format vector (extent) variables
 *
 * Proper format is:
 *     Array Prefix byte                  - 1 byte 0xFA
 *     Length of array 
 *       (including ENDREC
 *        including SKIPTBL
 *        excluding Prefix
 *        excluding Length)               - sizeof(COUNT)
 *     Skip table prefix byte (optional)  - 1 byte 0xE7
 *     Skip table length      (optional)  - sizeof(COUNT)
 *     Skip table             (optional)  - variable (sizeof(COUNT) per entry)
 *     Array Element 1 size and value     - variable
 *     Array Element...                   - variable
 *     ENDREC flag                        - 1 byte 0xFF
 *
 * RETURNS: size of current record
 *          or size needed to format record if error.
 */
ULONG
recFormatARRAY(
        TEXT *pRecord,
        ULONG recSize,
        ULONG maxSize,
        TEXT *pField,
        ULONG numEntries)
{
    ULONG subRecSize;
    ULONG arraySize;
    ULONG arraySizeSav;
    ULONG shiftSize;
    ULONG cursize;

    /* determine size needed for subRecord by passing 0 to recFormat 
     * NOTE: We are expecting that if maxSize is passed as 0 that
     *       recFormat() will have no side effects.
     */
    arraySizeSav = recFormat(pRecord, 0, numEntries);
    cursize = recFieldSize(pField);
    /* add up actual size needed - Prefix size, recSize, ENDREC
     * - 1 because we are replacing an existing null field
     */
    subRecSize = VECTORPFX + arraySizeSav;
    if ((subRecSize + recSize - cursize) > maxSize)
        return (subRecSize + recSize - cursize);

    shiftSize = subRecSize - cursize; /* new size - old size */

    /* shove existing data over to make room for array subrecord */
    recFieldShift(pRecord, recSize, pField, (COUNT)shiftSize);

    /* assign newly expanded record size */
    recSize += shiftSize;  
    if (recSize > maxSize)
        return recSize;

    *pField = VECTOR;  /* mark this as a VECTOR type entry */

    /* Now format the array as a "subrecord" of the record 
     * Start of subrecord buffer is pField+VECTORPFX
     * size is max size - VECTORPFX - sizeof(ENDREC)
     */
     arraySize = recFormat(pField+VECTORPFX, arraySizeSav,
                           numEntries);
     if ( arraySize != arraySizeSav )
     {
         /* If we get here, recFormat somehow now has different space
          * requirements than when we first called it! 
          * The record is therefore not properly built.
          * This should never happen. 
          */
         return ( recSize + arraySize-(arraySizeSav) );
     }

     /* store record length */
     sct(pField+1, (COUNT)arraySize);

    return recSize;

}  /* end recFormatARRAY */


/* PROGRAM: recFormatTINY - format TINY integer to a storage format
 * as a variable length LONG
 *
 * Formats a TINY type variable into record storage format.
 * Buffer is assumed to be AT LEAST 2 bytes long, one for the value and  
        one for the length byte.
 *
 * RETURNS: pointer to buffer containing formatted value
 */
TEXT *
recFormatTINY(
        TEXT      *pBuffer,
        ULONG      bufSize,
        svcTiny_t  value,
        ULONG      indicator)
{
    pBuffer = recFormatLONG(pBuffer, bufSize, (svcLong_t)value, indicator);

    return pBuffer;
        
} /* end recFormatTINY */


/* PROGRAM: recFormatSMALL - format SMALL integer to a storage format
 * as a variable length LONG
 *
 * Formats a SMALLINT type variable into record storage format.
 * Buffer is assumed to be large enough to handle max size value (3 bytes).
        2 for the value and one for the length byte.
 *
 * RETURNS: pointer to buffer containing formatted value
 */
TEXT *
recFormatSMALL(
        TEXT       *pBuffer,
        ULONG       bufSize,
        svcSmall_t  value,
        ULONG       indicator)
{
    
    pBuffer = recFormatLONG(pBuffer, bufSize, (svcLong_t)value, indicator);
 
    return pBuffer;
       
} /* end recFormatSMALL */


#if 0
/* PROGRAM: recFormatBIG - format BIG integer to a storage format
 *
 * Formats a BIG type variable into record storage format.
 * Buffer is assumed to be large enough to handle max size value (9 bytes).
 *
 * RETURNS: pointer to buffer containing formatted value
 */
TEXT *
recFormatBIG(
        TEXT       *pBuffer,
        ULONG       bufSize,
        svcBig_t   *pBig,
        ULONG       indicator)
{
    GTBOOL    negative;      /* True if the whole number is negative */
    GTBOOL    llnegative;            /* True if the low-long is negatine */ 

    if (indicator)
        return recFormatIndicator(pBuffer, bufSize, indicator);

    if ( (pBig->hl == 0) && (pBig->ll == 0) )
    {
        /* zero is stored as single byte */
        *pBuffer = (TEXT)0;
        return pBuffer;
    }

    if (pBig->hl < 0)
        negative = 1;
    else
        negative = 0;
    if (pBig->ll < 0)
        llnegative = 1;
    else
        llnegative = 0;

    if ((negative == llnegative) && (pBig->hl == 0 || pBig->hl == -1))
    {
        /* the high LONG is empty, the value can fit in 32 bits */
        /* convert only the low LONG to a variable length format with slngv()*/
        /* This will produce length of 0 to 4 bytes */
        pBuffer = recFormatLONG(pBuffer, bufSize, (svcLong_t)(pBig->ll), 0);
    }
    else
    {
        /* Here we convert the high with slngv and the low with slng
         * The final length must be > 4 to diferenciate it from
         * the other case.
         */

        /* convert the high LONG to a variable length format */
        pBuffer = recFormatLONG(pBuffer, bufSize, (svcLong_t)(pBig->hl), 0);

        /* if the produced length is 0, create a leading NULL byte */
        if (*pBuffer == 0)
        {
            *pBuffer = 1;
            *(pBuffer+1) = 0;
        }

        /* convert  the low LONG to a fix length format with slng() */      
        slng(pBuffer + *pBuffer + 1, (LONG)(pBig->ll));
        *pBuffer += sizeof(LONG);

    }

    return pBuffer;
    
} /* end recFormatBIG */
#endif
    

/* PROGRAM: recFormatBIG - format LONG64 integer as variable length field
 *
 * Formats a LONG64 type variable into record storage format.
 * Buffer is assumed to be large enough to handle max size value (5 bytes).
 *
 * RETURNS: pointer to buffer containing formatted value
 */
TEXT *
recFormatBIG(
        TEXT      *pBuffer,
        ULONG      bufSize,
        svcBig_t   value,
        ULONG      indicator)
{
    if (indicator)
        return recFormatIndicator(pBuffer, bufSize, indicator);

    if (value == 0)
    {
        /* zero is stored as single byte */
        *pBuffer = (TEXT)0;
    }
    else
    {
       /* store length followed by variable length data */
        *pBuffer = (TEXT)slngv64(pBuffer+1, value);
    }
    
    return pBuffer;
        
} /* end recFormatBIG */

/* PROGRAM: recFormatLONG - format LONG integer as variable length field
 *
 * Formats a LONG type variable into record storage format.
 * Buffer is assumed to be large enough to handle max size value (5 bytes).
 *
 * RETURNS: pointer to buffer containing formatted value
 */
TEXT *
recFormatLONG(
        TEXT      *pBuffer,
        ULONG      bufSize,
        svcLong_t  value,
        ULONG      indicator)
{
    if (indicator)
        return recFormatIndicator(pBuffer, bufSize, indicator);

    if (value == 0)
    {
        /* zero is stored as single byte */
        *pBuffer = (TEXT)0;
    }
    else
    {
        FASSERT(bufSize > 4,"buffer must be 5 bytes or more");
       /* store length followed by variable length data */
        *pBuffer = (TEXT)slngv(pBuffer+1, value);
    }
    
    return pBuffer;
        
} /* end recFormatLONG */


/* PROGRAM: recFormatBYTES - format BYTE string as variable length field
 *
 * Formats a TEXT type variable into record storage format.  Buffer is assumed
 * to be large enough to store one byte if buffer is not large enough, text 
 * string is truncated to fit in the buffer.
 *
 * RETURNS: pointer to buffer containing formatted value
 */
DSMVOID
recFormatBYTES(
        TEXT     *pBuffer,
        ULONG     bufSize,
        TEXT     *pValue,
        ULONG     length,
        ULONG     indicator)
{
    
    if (indicator)
    {
        recFormatIndicator(pBuffer, bufSize, indicator);
        return;
    }

    FASSERT(length + LONGPFXLEN < bufSize,
                "buffer must be large enough to hold bytes");

    /* truncate long fields */
    if (length + LONGPFXLEN > bufSize)
    {
        /* max length is buffer size */
        length = bufSize - LONGPFXLEN;
    }

    /* set field length */
    if (length < 1)
    {
        /* empty field */
        *pBuffer = (TEXT)0;
    }
    else if (length < PRO_MINLONG)
    {
        /* short field */
        /* BUM - Assuming ULONG length < 256 */
        *pBuffer++ = (TEXT)length;
    }
    else
    {
        /* long field - Field length is next two bytes */
        *pBuffer = LONGFLD; /* assign long field flag 0xe6 */
        /* BUM - Assuming ULONG length < 2^15 */
        sct(pBuffer + 1, (COUNT)length);

        pBuffer += LONGPFXLEN; /* incr past LONGFLD flag and length */
    }
            
    /* copy variable length data */
    bufcop(pBuffer, pValue, length);
    
    return;

} /* end recFormatBYTES */


/* PROGRAM: recFormatDECIMAL - format DECIMAL as packed char bytes
 *
 * The input is a character array which ic converted to packed decimal format.
 *
 * The packed decimal format is as follows:
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
 * RETURNS: pointer to buffer containing formatted value
 */
TEXT *
recFormatDECIMAL(
        TEXT         *pBuffer,
        ULONG         bufSize,
        svcDecimal_t *pValue,
        ULONG         indicator)
{
    TEXT *psourceDigit;
    TEXT *plastDigit;
    TEXT *pdigitTarget;
    TEXT  leadbyte;
    TEXT  temp;

    if (indicator)
        return recFormatIndicator(pBuffer, bufSize, indicator);

    if (pValue->totalDigits == 0)
    {
        FASSERT(bufSize >= 1,"buffer must be 1 bytes or more");
        /* zero is stored as single byte */
        *pBuffer = (TEXT)0;
        return pBuffer;
    }

    FASSERT(bufSize >= pValue->totalDigits, "buffer too small");

    /* NOTE: The following was basically copied from decsqez() */

    /* first byte consists of # of decimals and positive indicator */
    leadbyte = pValue->decimals;
    if (pValue->plus)
        leadbyte |= 0x80;

    /* If the input is in the char format (0x31 32 33 34) then we need to
     * subtract zero from each digit (or shift it left and right).
     * Otherwise (0x01 02 03 04) we can use it directly.
     */
    psourceDigit    = pValue->pdigit;
    plastDigit      = pValue->pdigit + pValue->totalDigits;
    pdigitTarget    = pBuffer + 1;
    *pdigitTarget++ = leadbyte;  /* assign control byte */

    while (psourceDigit < plastDigit)
    {
        /* save off first packed digit */
        temp = (*psourceDigit++) << 4;

        /* If there is another digit or it on - 
         * otherwise or on 0x0f to keep things in bytes
         */
        if (psourceDigit < plastDigit)
            *pdigitTarget = temp | (TEXT)(*psourceDigit << 4) >> 4;
        else
            *pdigitTarget = temp | (TEXT)0x0f;

        pdigitTarget++;
        psourceDigit++;
    }

    *pBuffer      = pdigitTarget - (pBuffer + 1); /* assign field length */
    
    return pBuffer;
        
} /* end recFormatDECIMAL */


/* PROGRAM: recFormatFLOAT - format FLOAT as 4 byte length value
 *
 * Formats a FLOAT type variable into record storage format. (REAL for Dharma)
 * Buffer is assumed to be large enough to handle max size value (5 bytes).
 *
 * TODO: storing the double in a machine independant mannor as described
 *       above solves any big/little endian issues.  It does not address
 *       however that the format of the float may be represented
 *       differently on different system architectures.  This is an
 *       issue which needs further investigation.
 *
 * RETURNS: pointer to buffer containing formatted value
 */
TEXT *
recFormatFLOAT(
        TEXT       *pBuffer,
        ULONG       bufSize,
        svcFloat_t *pValue,
        ULONG       indicator)

{
    if (indicator)
        return recFormatIndicator(pBuffer, bufSize, indicator);

    FASSERT(bufSize >= 5,"buffer must be 5 bytes or more");
    /* store length followed by variable length data
     * NOTE: in order for slng to work properly on a float
     * the input must be a ptr to a float, casted to a 
     * long pointer and then dereferenced.  (Simply casting 
     * the float to a long would cause automatic conversion.)
     */
    slng(pBuffer+1, *(LONG *)pValue);
    *pBuffer = sizeof(LONG);
    
    return pBuffer;
        
} /* end recFormatFLOAT */


/* PROGRAM: recFormatDOUBLE - format DOUBLE as two 4 byte length value
 *
 * Formats a DOUBLE type variable into record storage format.
 * Buffer is assumed to be large enough to handle max size value (10 bytes).
 *
 * Each of the two words of the double are converted into a machine
 * independant format consisiting of the length and the value.
 * Note that the lenght of each value will always be 4, (the 
 * value is NOT stored as variable length).
 *
 * TODO: storing the double in a machine independant mannor as described
 *       above solves any big/little endian issues.  It does not address
 *       however that the format of the double may be represented
 *       differently on different system architectures.  This is an
 *       issue which needs further investigation.
 *
 * RETURNS: pointer to buffer containing formatted value
 */
TEXT *
recFormatDOUBLE(
        TEXT        *pBuffer,
        ULONG        bufSize,
        svcDouble_t *pValue,
        ULONG        indicator)
{
    if (indicator)
        return recFormatIndicator(pBuffer, bufSize, indicator);

    FASSERT(bufSize >= 10,"buffer must be 10 bytes or more");

    /* store length, high value, length, low value */
    *pBuffer = 2*sizeof(LONG);
    slng( pBuffer+1, ((recDouble_t *)pValue)->highValue );

    slng( pBuffer+1 + sizeof(LONG), ((recDouble_t *)pValue)->lowValue );

    return pBuffer;

} /* end recFormatDOUBLE */


/* PROGRAM: recFormatDATE - format DATE as variable length integer field
 *
 * Formats a DATE type variable into record storage format.
 * Buffer is assumed to be large enough to handle max size value (5 bytes).
 * The date is converted from svcDate_t to the number of days since 05/02/50.
 * It is then formatted to be stored as a LONG.
 *
 * RETURNS: pointer to buffer containing formatted value in ppBuffer
 *          RECSUCCESS success
 *          RECBADMODE
 *          RECBADMON
 *          RECBADDAY
 *          RECBADYEAR
 */
recStatus_t
recFormatDATE(
        TEXT       *pBuffer,
        ULONG       bufSize,
        svcDate_t  *pValue,
        ULONG       indicator)
{
    recStatus_t returnCode;
    svcLong_t   sDate;

    if (indicator)
    {
        recFormatIndicator(pBuffer, bufSize, indicator);
        return RECSUCCESS;
    }

    returnCode = utDateToLong(&sDate, (utDate_t *)pValue);
    if (returnCode)
    {
       switch(returnCode)
       {
         case(BADMODE):
           returnCode = RECBADMODE;
           break;
         case(BADYEAR):
           returnCode = RECBADYEAR;
           break;
         case(BADMON):
           returnCode = RECBADMON;
           break;
         case(BADDAY):
           returnCode = RECBADDAY;
           break;
       }
       return returnCode;
    }

    sDate -= DATEBASE;
    recFormatLONG(pBuffer, bufSize, sDate, 0);
    
    return returnCode;
        
} /* end recFormatDATE */


/* PROGRAM: recFormatTIME - format TIME as variable length integer field
 *
 * convert recTime_t to a LONG containing # of milliseconds since midnight
 * (time 0).  Store this value as a LONG
 *
 * RETURNS: pointer to buffer containing formatted value
 */
TEXT *
recFormatTIME(
        TEXT       *pBuffer,
        ULONG       bufSize,
        svcTime_t  *pValue,
        ULONG       indicator)
{

    svcLong_t sTime;

    if (indicator)
        return recFormatIndicator(pBuffer, bufSize, indicator);

    utTimeToLong(&sTime, (utTime_t *)pValue);
    pBuffer = recFormatLONG(pBuffer, bufSize, sTime, 0);
    
    return pBuffer;
        
} /* end recFormatTIME */


/* PROGRAM: recFormatTIMESTAMP - format TIMESTAMP as 2 variable
 *          length integer field
 *
 * convert recTimeStamp_t to a TIME and DATE
 *
 * RETURNS: pointer to buffer containing formatted value in ppBuffer
 *          RECSUCCESS
 *          RECBADMON
 *          RECBADDAY
 *          RECBADYEAR
 *          RECBADMODE
 */
recStatus_t
recFormatTIMESTAMP(
        TEXT           *pBuffer,
        ULONG           bufSize,
        svcTimeStamp_t *pValue,
        ULONG           indicator)
{

    recStatus_t returnCode;
    svcLong_t sTime;
    svcLong_t sDate;

    if (indicator)
    {
        recFormatIndicator(pBuffer, bufSize, indicator);
        return RECSUCCESS;
    }

    FASSERT(bufSize >= 10,"buffer must be 10 bytes or more");

    returnCode = utDateToLong(&sDate, (utDate_t *)(&pValue->tsDate));
    if (returnCode)
    {
       switch(returnCode)
       {
         case(BADMODE):
           returnCode = RECBADMODE;
         break;
         case(BADYEAR):
           returnCode = RECBADYEAR;
         break;
         case(BADMON):
           returnCode = RECBADMON;
         break;
         case(BADDAY):
           returnCode = RECBADDAY;
         break;
       }
       return returnCode;
    }

    returnCode = utTimeToLong(&sTime, (utTime_t *)(&pValue->tsTime));

    *pBuffer = 2*sizeof(LONG);
    slng( pBuffer+1, sDate );

    slng( pBuffer+1 + sizeof(LONG), sTime );

    return RECSUCCESS;
        
} /* end recFormatTIMESTAMP */


/* PROGRAM: recFormatIndicator - format Indicator Value
 *
 * RETURNS: pointer to buffer containing formatted value
 */
LOCALF TEXT *
recFormatIndicator(
    TEXT   *pBuffer,
    ULONG   bufSize _UNUSED_,
    ULONG   indicator)
{
    switch(indicator)
    {
      case SVCUNKNOWN:
        /* request to store UNKNOWN prefix value */
        FASSERT(bufSize >= 1, "buffer must be 1 byte or more");
        *pBuffer = VUNKNOWN;
        break;
      case SVCTODAY:
        /* request to store TODAY prefix value */
        FASSERT(bufSize >= 1, "buffer must be 1 byte or more");
        *pBuffer = TODAY;
        break;
      case 0:
        break;
      default:
        FASSERT(0, "Invalid indicator value in recFormat.");
        break;
    }

    return pBuffer;

}  /* end recFormatIndicator */

