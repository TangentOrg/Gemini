
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

/* FILE: rec.c - contains record manipulation support
 *
 * This file contains functions to manipulate the internal record structure
 * supported by the record manager.  Functions expect a variable size record
 * buffer to be passed and do not expect the record buffer to move while 
 * being manipulated.  All functions that expand a record return the new
 * record size.
 *
 * The record format is described in the following bnf:
 *
 * record = [skip-table] [field...]
 * skip-table = skip-table-prefix [skip-table-entry...]
 * skip-table-prefix = SKPTBL skip-table-length
 * skip-table-length = number of entries * SKIPENTLEN
 * skip-table-entry  = offset from the start if the 1st field
 *                     store in SKIPENTLEN bytes.
 *
 * This was initially created to support virtual system tables, long term
 * goal is to replace corresponding bf functions as indicated.
 *
 * TODO:  need to make into its own general purpose subsystem
 *
 */
    
/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"   /* database manager */

#include "recpub.h"     /* public interface */
#include "svcspub.h"
#include "utbpub.h"     /* bufmov interface */
#include "utcpub.h"     /* for chrcop */
#include "utspub.h"     /* sct interface */


/*** LOCAL FUNCTION PROTOTYPES ***/

/*** END LOCAL FUNCTION PROTOTYPES ***/

#define VECTORPFX  ( 1 + sizeof(COUNT) )

/* PROGRAM: recRecordNew - just like recRecordInit() but also sets the table
 * schema version number. This is the prefered function to call for creating
 * a new template record.
 *          Given a table id, number of fields and a pointer to a record
 *          buffer and its length, an empty record is created
 *          (a template record where all fields are filled with null values).
 *
 * RETURNS: RECSUCCESS and size of formatted record in *precSize.
 *          If newly formatted record is too large to fit in the input buffer, 
 *          then return size needed and RECTOOSMALL is returned.
 */
recStatus_t
recRecordNew(
	recTable_t	 tableId,	/* the table number */
	TEXT		*pRecord,	/* pointer to record buffer */
	ULONG		 maxSize,	/* size of record buffer */
	ULONG		*precSize,	/* OUT size of rec (or size needed) */
	ULONG		 numFields,	/* # of NULL fields required */
	ULONG		 tableSchemaVers)/* table schema version */
{
    recStatus_t retcode;

    retcode = recRecordInit(tableId, pRecord, maxSize, precSize, numFields);

    if (retcode == RECSUCCESS && tableId > 0)
        retcode = recPutLONG(pRecord, maxSize, precSize,
                             (ULONG)1, (ULONG)REC_TBLVERS, tableSchemaVers, 0);
    return retcode;
}

/* PROGRAM: recRecordInit - initialize a record and insert table number
 *
 *          Given a table id, number of fields and a pointer to a record
 *          buffer and its length, an empty record is created
 *          (a template record where all fields are filled with null values).
 *
 * RETURNS: RECSUCCESS and size of formatted record in *precSize.
 *          If newly formatted record is too large to fit in the input buffer, 
 *          then return size needed and RECTOOSMALL is returned.
 */
recStatus_t
recRecordInit(
	recTable_t	 tableId,	/* the table number */
	TEXT		*pRecord,	/* pointer to record buffer */
	ULONG		 maxSize,	/* size of record buffer */
	ULONG		*precSize,	/* OUT size of rec (or size needed) */
	ULONG		 numFields)	/* # of NULL fields required */
{
    recStatus_t returnCode;

    /* materialize template record */
    *precSize = recFormat(pRecord, maxSize, numFields);
    if (*precSize > maxSize)
        return RECTOOSMALL;

    /* if the tablenumber is negative, then it's a schema -- let's use the
     * old format since the new isn't necessary
     */
    if (tableId < 0)
        return recPutLONG(pRecord, maxSize, precSize,
                          1, (ULONG)0, (svcLong_t) tableId, 0);

    /* must be a non-schema table -- use the new format */
    returnCode = recInitArray(pRecord, maxSize, precSize, 
                              (ULONG) 1, /* 1st field is reserved for this */
                              (ULONG) REC_FLD1EXT);
    if (returnCode != RECSUCCESS)
        return returnCode;

    /* set the values in the vector */

    /* table number */
    returnCode = recPutLONG(pRecord, maxSize, precSize,
                            (ULONG) 1, (ULONG)REC_TBLNUM,
                            (svcLong_t) tableId, 0);
    if (returnCode != RECSUCCESS)
        return returnCode;

    /* set schema version to zero (since we don't have it) -- storing a zero
     * takes no more space than storing null, so this shouldn't fail due
     * to space
     */
    returnCode = recPutLONG(pRecord, maxSize, precSize,
                            (ULONG) 1, (ULONG)REC_TBLVERS, (svcLong_t) 0, 0);
    if (returnCode != RECSUCCESS)
        return returnCode;

    /* number of fields in the record */
    returnCode = recPutLONG(pRecord, maxSize, precSize,
                            (ULONG) 1, (ULONG)REC_FLDCOUNT,
                            (svcLong_t) numFields, 0);

    return returnCode;

}  /* end recRecordInit */

/* PROGRAM: recAddColumns - Add empty columns to an existing record 
 *
 *          Given a pointer to the record in a buffer, the maximum size of
 *          the buffer, the number of new columns to add.  The record is
 *          expanded with as many columns as requested.  The value of each
 *          new column is set to null.  If there is enough space in the buffer,
 *          the colums are added at the end of the record.  This may require
 *          the creation or update of  the skip table of the record.
 *          The possible change to the skip table is taken into
 *          account when checking the available space in the row buffer.
 *          If there is not enough space in the buffer,  return an error and
 *          the amount of space needed.  The caller will have to allocate a
 *          new buffer and call again.
 *
 * This basically replaces bfapp()
 *
 * RETURNS: Returns total size of formatted record in *preSize.
 *          If record does not fit in buffer then returns size needed.
 *          The caller must check the returned status and if it is
 *          RECTOOSMALL call this procedure again with a large enough buffer.
 * 
 */
recStatus_t
recAddColumns(
	TEXT		*pRecord,	/* pointer to record buffer */
	ULONG		 maxSize,	/* size of record buffer */
	ULONG		*precSize,	/* OUT size of rec (or size needed) */
	ULONG		 numFields)	/* # of NULL fields required */
{
    recStatus_t returnCode;
    svcLong_t   longValue;
    ULONG       indicator;
    TEXT *ps;
    ULONG newsize;
    ULONG delta;              /* amount of record size change */

    if (numFields == 0)
        return RECSUCCESS;

    if (maxSize < *precSize + numFields)
    {
        /* update precSize so caller knows how much bigger to make buffer */
        *precSize += numFields; 
        return RECTOOSMALL;
    }
 
    if (*(recFieldFirst(pRecord)) == VECTOR)
    {
        /* This record's in the new format for records where 1st field is
         * a vector. So we need to update the number of fields in the record
         */
        returnCode = recGetLONG(pRecord, (ULONG)1, (ULONG)REC_FLDCOUNT,
                                &longValue, &indicator);
        if (returnCode != RECFLDNOTFOUND && indicator != SVCUNKNOWN)
        {
            returnCode = recPutLONG(pRecord, maxSize, precSize,
                                    (ULONG) 1, (ULONG)REC_FLDCOUNT,
                                    numFields + longValue, 0);
            if (returnCode != RECSUCCESS)
                return returnCode;

            /* check size of buffer again since precSize may have changed */
            if (maxSize < *precSize + numFields)
            {
                newsize = *precSize + numFields;
                /* reset the fldcount to leave it as it was */
                returnCode = recPutLONG(pRecord, maxSize, precSize,
                                        (ULONG) 1, (ULONG)REC_FLDCOUNT,
                                        longValue, 0);
                *precSize = newsize;
                return RECTOOSMALL;
            }
        }
    }

    ps = pRecord + *precSize - 1; /* working pointer to ENDREC */
 
    chrcop(ps, VUNKNOWN, numFields);
    ps += numFields;
    *ps = ENDREC;

    *precSize += numFields;

    /*  update skip table */
    returnCode = recSkipUpdate(pRecord, maxSize, precSize, &delta);

    return returnCode;

}  /* end recAddColumns */

/* PROGRAM: recInitArray - Convert an empty column to an empty array
 *
 *          Given a pointer to the record in a buffer, the maximum size of
 *          the buffer, the field number of the empty field to be converted to 
 *          an array, and the extent of the array (number of columns in the 
 *          array. If there is enough space in the buffer,
 *          the colums of the array are added in place with UNKNOWN value.
 *          A skip table for the Array is created if needed. This require
 *          the update of  the skip table of the record.
 *          The possible change to the skip table is taken into
 *          account when checking the available space in the row buffer.
 *          If there is not enough space in the buffer,  return an error and
 *          the amount of space needed.  The caller will have to allocate a
 *          new buffer and call again.
 *
 * This basically replaces bfvctr()
 *
 * RETURNS: Returns total size of formatted record in *preSize.
 *          If record does not fit in buffer then returns size needed.
 *          The caller must check the returned status and if it is
 *          RECTOOSMALL call this procedure again with a large enough buffer.
 * 
 */
recStatus_t
recInitArray(
	TEXT		*precordBuffer, /* pointer to record buffer */
	ULONG		 maxSize,	/* size of record buffer */
	ULONG		*precSize,	/* IN/OUT size of rec (or size needed)*/
        ULONG            fieldNumber,   /* IN  number of field within record */
        ULONG            arrayExtent)   /* IN  number of members in the array */
{

    TEXT  *pfield;                     /* Pointer to field within record */

    pfield = recFindField(precordBuffer, fieldNumber);
    if (pfield == NULL)
        return RECFLDNOTFOUND;

       *precSize = recFormatARRAY(precordBuffer, *precSize, 
                                   maxSize, pfield,
                                   arrayExtent);

    if (*precSize > maxSize)
        return RECTOOSMALL;  /* no space or template requested */

    return RECSUCCESS;
}  /* end recInitArray */


/* PROGRAM: recFieldFirst - get pointer to first field in record
 *
 * Gets the first field in a record which is generally the table number
 * associated with the record.
 *
 * RETURNS: pointer to first field in record
 */
TEXT *
recFieldFirst(TEXT *pRecord)  /* ptr to start of record */
{
    if (*pRecord == SKIPTBL)
    {
        /* ptr past skip table entries */
        return pRecord + SKIPPFXLEN + EXCOUNT(pRecord+1);
    }
    else
    {
        /* no skip table, first field is right here */
        return pRecord;
    }
} /* end recFieldFirst */


/* PROGRAM: recFieldNext - get pointer to next field in record
 *
 * Gets a pointer to the next field in a record given a pointer
 * to a field.  This function does no error checking and can
 * return a pointer past the end of a record.
 *
 * RETURNS: pointer to next field in the record
 */
TEXT *
recFieldNext(TEXT *pField)  /* ptr to start of record */
{
    /* return pointer to next field */
    return pField + recFieldSize(pField);
    
} /* end recFieldFirst */


/* PROGRAM: recFindField - get pointer to specified field in record
 *
 * Gets a pointer to the specified field in a record given a pointer
 * to the begining of the record.  This function does no error checking
 * and can return a pointer past the end of a record.
 *
 * This is a copy of bffndfld()
 *
 * RETURNS: pointer to requested field in the record
 *          NULL - field not found
 */
TEXT *
recFindField(
        TEXT  *pRecord,   /* ptr to start of record */
        ULONG  fieldNum)  /* Field number */
{
    LONG    prefix;
    COUNT   skipTableLength;
    COUNT   fentry;
    ULONG   tmpFieldNum;
 
    if ( (fieldNum < 1) || (pRecord == NULL) )
        goto err;
 
    if (*pRecord == SKIPTBL)               /* if table exists */
    {
        pRecord++;
        skipTableLength = EXCOUNT(pRecord); /* get skip table length */
        if ((fentry = (COUNT)(fieldNum >> 4)))     /* entry=field/16 */
        {                                          /* field>15 */
            if ((fentry <<= 1) > skipTableLength) /* is skip table too short? */
               fentry = skipTableLength;
 
            pRecord += EXCOUNT(pRecord + fentry); /* skip by displacement */
            fieldNum -= (fentry << 3);     /* searched field modulo 16 */
        }
        else
            fieldNum--;    /* field -1, so field 1 = 0, field 2 = 1, etc. */
 
        pRecord += sizeof(COUNT) + skipTableLength; /* add skip table length */
    }
    else
        fieldNum--;        /* field -1, so field 1 = 0, field 2 = 1, etc. */
 
    while (fieldNum > 0)
    {
        fieldNum--;
        prefix = *pRecord++;
        if (prefix < PRO_MINLONG)   /* normal field: prefix is the size */
            pRecord += prefix;
        else if (prefix == VECTOR || prefix == LONGFLD)
            pRecord += EXCOUNT(pRecord) + sizeof(COUNT);
        else if (prefix == UNFETCHED_N)         /* N unfetched fields */
        {
            tmpFieldNum = EXCOUNT(pRecord) - 1;
            if (tmpFieldNum > fieldNum)
                goto err;  /* the desired field is not fetched */
            fieldNum -= tmpFieldNum;
            pRecord += sizeof(COUNT);
        }
        else if (prefix == ENDREC)
            goto err;
    }
 
    if (*pRecord > PRO_MINLONG)
        if (*pRecord == ENDREC      ||
            *pRecord == UNFETCHED_1 ||
            *pRecord == UNFETCHED_N)
            goto err;
 
    return pRecord;
 
err:
    return NULL;

}  /* end recFindField */


/* PROGRAM: recFindArrayElement - given a pointer to an array field and an
 * index to return, this returns a pointer to the field in the array.
 */
TEXT *
recFindArrayElement (TEXT *pField, ULONG ArrayIdx)
{
    if ((int)*pField != VECTOR)
        return NULL;

    return recFindField(pField + recFieldPrefix(pField), ArrayIdx);
}

/* PROGRAM: recFieldLength -- get length of field data - replaces bfflen()
 *
 * RETURNS: length of the field data in bytes
 */
ULONG
recFieldLength(TEXT *ps)  /* ptr to start of field */
{
    LONG     prefix;
    
    prefix = (LONG)*ps;
    
    /* short fields */
    if (prefix < PRO_MINLONG)
    {
        return prefix;
    }
    
    /* long fields */
    if (prefix == VECTOR || 
        prefix == LONGFLD || 
        prefix == SKIPTBL)
    {
        return EXCOUNT(ps + 1);
    }
    
    /* all the rest */
    return 0;
    
} /* recFieldLength */


/* PROGRAM: recFieldPrefix - get length of field prefix - replaces bfplen()
 *
 * RETURNS: length of field prefix in bytes
 */
ULONG
recFieldPrefix(TEXT *ps) /* ptr to start of field */
{
    LONG prefix;
    
    prefix = (LONG)*ps;
    
    /* long fields */
    if (prefix == VECTOR || 
        prefix == LONGFLD || 
        prefix == SKIPTBL)
    {
        return 1 + sizeof(COUNT);
    }
    
    /* all the rest */
    if (prefix)
        return 1;

    return 0;

} /* recFieldPrefix */


/* PROGRAM: recFieldSize - get total size of field - replaces bfFldSize()
 *
 * RETURNS: size of field header and data in bytes
 */
ULONG
recFieldSize(TEXT *ps)  /* ptr to start of field */
{
    LONG prefix;

    prefix = (LONG)*ps;

    /* short fields */
    if (prefix < PRO_MINLONG)
    {
        return prefix + 1;
    }
    
    /* long fields */
    if (prefix == VECTOR || 
        prefix == LONGFLD || 
        prefix == SKIPTBL)
    {
        return EXCOUNT(ps + 1) + sizeof(COUNT) + 1;
    }
    
    /* all the rest */
    return 1;
    
} /* recFieldSize */


/* PROGRAM: recFieldShift - shift a single field - similar to bfrshft()
 *
 * Shifts a single field by the requested amount either positive or negative 
 * and adjusts skip table if needed.  The contents of field are not changed
 * unless the field is made smaller.  This function does no error checking
 * so the buffer must contain a valid record and be large enough to hold 
 * a record expanded by the requested amount.
 *
 * RETURNS: pointer to expanded field 
 */
TEXT *
recFieldShift(
        TEXT  *pRecord, /* ptr to record buffer */
        ULONG recSize,  /* current size of record */
        TEXT  *pField,  /* ptr to field to expand */
        COUNT amount)   /* amount to shift field size */
{
    TEXT *pNext;       /* next byte */
    TEXT *pEnd;        /* past end of record */
    
    /* if nothing to do then return current field position */
    if (amount == 0) return pField;

    /* update skip table entries to reflect change in field size */
    recSkipShift(pRecord, pField, amount);
    
    /* get pointers to byte following shift field and end of record */
    pNext = pField + recFieldSize(pField);
    pEnd = pRecord + recSize;
    
    /* move data after the shift field to correct location */
    bufcop(pNext + amount,      /* target */ 
           pNext,               /* source - 1st field position to shift */
           pEnd - pNext);       /* size of data to shift */

    return pField;

}  /* end recFieldShift */


/* PROGRAM: recSkipShift - shift skip table entries - replaces skipu()
 *
 * This function shifts a skip table because a given field will change size.
 * The amount can be positive to expand or negative to shrink a record.
 * All the skip table entries following the one that points to the field
 * that is changing size will be adjusted as requested.   This function is
 * much faster than recomputing the entire skip table and does not change the
 * size of the skip table 
 *
 * RETURNS: nothing
 */
DSMVOID
recSkipShift(
        TEXT   *pRecord, /* ptr to record (skip table) */
        TEXT   *pField,  /* ptr to field to shift after */
        COUNT  amount)   /* amount to shift skip entries */
{
    TEXT *pEntry;       /* ptr to table entry */
    TEXT *pFirst;       /* ptr to first entry */
    TEXT *pPast;        /* ptr past last entry */
    ULONG offset;       /* offset into record */
    
    if (*pRecord == SKIPTBL)
    {
        /* ptr to first and past skip table entries */
        pFirst = pRecord + SKIPPFXLEN;       /* get 1st skip table entry */
        pPast = pFirst + EXCOUNT(pRecord+1); /* just past skip table (fields) */
        
        /* scan each skip table entry */
        for (pEntry = pFirst;
             pEntry < pPast;
             pEntry += SKIPENTLEN)
        {
            /* look for entries that point past field in question */
            offset = EXCOUNT(pEntry);  /* get skip table value
                                        * (skip value is field offset from 
                                        * first field in the record)
                                        */
            /* add skip table entry (field offset) to first field to see if 
             * the current skip table entry needs to be updated.  It will be
             * updated only if it's offset value points to a field past the
             * field being updated.
             */
            if (pPast + offset  > pField)
            {
                /* this entry needs to be shifted */
                /* BUM - Assuming ULONG offset + COUNT amount < 2^15 */
                sct(pEntry, (COUNT)(offset + amount));
            }
        }
    }
    
} /* recSkipShift */


/* PROGRAM: recSkipUpdate - update or create skip table - replaces bftblu()
 *
 * Updates or creates a skip table when fields are added to a record. New
 * skip table entries are created when needed causing the record to expand
 * in size.  The function assumes that the skip table if one exists is
 * consistant in size and currently existing values.  This function therefore
 * only supports new field additions the the end of the existing record.
 * Also, the input record size MUST match the record size including all
 * the newly added fields.
 *
 * RETURNS: RECSUCCESS  - size of new record in precSize
 *          RECTOOSMALL - number of bytes needed for expansion in precSize
 */
recStatus_t
recSkipUpdate(
        TEXT  *pRecord,   /* ptr to record buffer */
        ULONG  maxSize,   /* max record size */
        ULONG *precSize,  /* new size of record or size needed */
        ULONG *pdelta)    /* change in record size */
{
    TEXT    *ps = pRecord;
    ULONG    skipSize = 0;
    ULONG    field;

    *pdelta = 0;
    field = SKIPUNIT;           /* to search for field # 16 */

    /* determine size of skip table */
    if (*ps == SKIPTBL)
    {
        ps++;
        skipSize = EXCOUNT(ps);
        if (skipSize > 0) /* if at least one table entry exist */
        {
            field++;            /* to search for the 17th field past skip */
            ps += skipSize;     /* points to last skip table entry */
            ps += EXCOUNT(ps);  /* add skip table entry offset */
        }
        ps += sizeof (COUNT);   /* add skip table length length,
                                 * pointing to 1st fld*/
    }

    for ( ; (ps = recFindField(ps, field)); field = SKIPUNIT+1)
    {
        /* more than 17 fields exist past the last skip */
        /* new table entry is needed */

        if (*pRecord != SKIPTBL)
        {
            /* skip table does not exist, create new one */

            /* ensure record buffer big enough */
            *precSize += sizeof(COUNT) + 1;
            if (*precSize > maxSize)
                return RECTOOSMALL;

            bufmov(pRecord, pRecord + (*precSize)+1, sizeof(COUNT)+1);
            ps += sizeof(COUNT) + 1;
            *pRecord = SKIPTBL;
        
            skipSize = 0;
            sct(pRecord+1, (COUNT)skipSize);    /* table length 0 */
            *pdelta += sizeof(COUNT) + 1;
        }

        /* entry does not exist, create new one */
        skipSize += sizeof(COUNT);
        sct(pRecord+1, (COUNT)skipSize);       /* increase length of table */

        /* ensure record buffer big enough */
        *precSize += sizeof(COUNT);
        if (*precSize > maxSize)
            return RECTOOSMALL;

        /* insert the new entry to the table */
        bufmov(pRecord + skipSize+1, pRecord + (*precSize)+1, sizeof(COUNT));
        ps += sizeof(COUNT);
        sct(pRecord + skipSize+1,
                         (COUNT)(ps - pRecord - skipSize - sizeof(COUNT)-1) );
        *pdelta += sizeof(COUNT);
    }

    return RECSUCCESS;

}  /* end recSkipUpdate */


/* PROGRAM: recUpdate- update a field in a record - similar to bfu()
 *
 * Updates a field from one value to another value adjusting the size of the
 * record and skip table appropriately.   The size and number of skip table
 * entries will not change, but the record whill shrink or grow based on the
 * size of the new value.   The value must already be in record storage format
 * with correct field size values.
 *
 * RETURNS: new record size
 *          if record does not fit in buffer then returns total desired size
 */
ULONG
recUpdate(
        TEXT  *pRecord, /* ptr to record buffer */
        ULONG recSize,  /* size of record */
        ULONG maxSize,  /* max size of record */
        TEXT  *pField,  /* ptr to field to update */
        TEXT  *pValue)  /* ptr to new field value */
{
    ULONG oldSize;  /* length of existing field */
    ULONG newSize;  /* length of new field */
    LONG  difSize;  /* difference between new and old */
    
    /* get size of new field and check that it will fit into record buffer */
    oldSize = recFieldSize(pField);
    newSize = recFieldSize(pValue);
    difSize = newSize - oldSize;
    
    /* only make change if it will fit in record buffer */
    if (recSize + difSize <= maxSize)
    {
        /* adjust field size and skip tables */
        /* BUM - Assuming LONG difSize < 2^15 */
        recFieldShift(pRecord, recSize, pField, (COUNT)difSize);
    
        /* copy new value into record */
        bufcop(pField, pValue, newSize);
    }

    /* always return expected size */
    return recSize + difSize;
    
} /* end recUpdate */


/* PROGRAM: recUpdateArray - update a field in a record - similar to bfu()
 *
 * Updates a field from one value to another value adjusting the size of the
 * record and skip table appropriately.   The size and number of skip table
 * entries will not change, but the record whill shrink or grow based on the
 * size of the new value.   The value must already be in record storage format
 * with correct field size values.
 *
 * RETURNS: new record size
 *          if record does not fit in buffer then returns total desired size
 */
ULONG
recUpdateArray(
        TEXT  *pRecord,  /* ptr to record buffer */
        TEXT  *pArray,   /* ptr to begining of array within record */
        ULONG  recSize,  /* size of record */
        ULONG  maxSize,  /* max size of record */
        TEXT  *pField,   /* ptr to field to update */
        TEXT  *pValue)   /* ptr to new field value */
{
    ULONG oldSize;  /* length of existing field */
    ULONG newSize;  /* length of new field */
    LONG  difSize;  /* difference between new and old */
    

    /* 1. get difference in size
     * 2. If the new size > oldsize then shove fields on the right
     *    further right (expanding the record)
     * 3. if the newsize < oldsize then shove fields on the right to
     *    the left (contracting the record)
     * 4. Update the field in the record.
     * 5. return new size of record.
     */

    /* get size of new field & check that it will fit into record buffer */
    oldSize = recFieldSize(pField);
    newSize = recFieldSize(pValue);
    difSize = newSize - oldSize;

    /* only make change if it will fit in record buffer */
    if (recSize + difSize > maxSize)
        return (recSize + difSize);
    
    if (difSize > 0)
    {
        /* new size > oldsize, shove appropriate fields to the right */

        /* adjust field size and skip tables 
         * of record then of subrecord
         */
        /* BUM - Assuming LONG difSize < 2^15 */
        recFieldShift(pRecord, recSize, pArray, (COUNT)difSize);

        /* BUM - Assuming LONG difSize < 2^15 */
        recFieldShift(pArray + VECTORPFX, EXCOUNT(pArray+1), pField,
                      (COUNT)difSize);
        sct(pArray + 1, (COUNT)(recFieldLength(pArray) + difSize));
    }
    else if (difSize < 0)
    {
        /* newsize < oldsize then shove appropriate fields to the left */

        /* adjust field size and skip tables 
         * of subrecord then of record
         */

        /* BUM - Assuming LONG difSize < 2^15 */
        recFieldShift(pArray + VECTORPFX, EXCOUNT(pArray+1), pField,
                     (COUNT)difSize);

        /* BUM - Assuming LONG difSize < 2^15 */
        recFieldShift(pRecord, recSize, pArray, (COUNT)difSize);

        sct(pArray + 1, (COUNT)(recFieldLength(pArray) + difSize));
    }

    /* Update the field in the record. regardless of the above shifting, pfield
     * still points to the right place since the shifting only expands or
     * contracts stuff that is to the field's right
     */
    /* copy new value into record */
    bufcop(pField, pValue, newSize);

    /* return new size of record. */
    return (recSize + difSize);
    
} /* end recUpdateArray */

/* PROGRAM: recFieldCount - returns the number of physical fields in the given
 * record. Assumes that it should count the fields even if the first field has
 * been converted to an array (and therefore should have the count in it)
 */
COUNT
recFieldCount(
              TEXT   *ps)  /* pointer to record buffer */
{
    COUNT       skipsize, fldcnt;

    if (*ps != SKIPTBL)
        fldcnt = 0; /* 16 or fewer fields */
    else
    {
        ps++;
        skipsize = EXCOUNT(ps);
        if (skipsize == 0)
            fldcnt = 0;
        else
        {
            fldcnt = (skipsize * SKIPUNIT / SKIPENTLEN) - 1;
            ps += skipsize; /* ps now pts to last skip table entry */
            ps += EXCOUNT(ps); /* add skip length from table entry */
        }
        ps += sizeof(COUNT); /* add skip table length length -- now ps
                                points at 1st fld in last 16 field set */
    }

    /* count how many fields are in the last 16 field set */
    while ( *ps != ENDREC )
    {
        if (*ps == UNFETCHED_N)
        {
            ps++;
            fldcnt += EXCOUNT(ps);
            ps += sizeof(COUNT);
        }
        else /* normal case -- just count the fields */
        {
            fldcnt++;
            ps = recFieldNext(ps);
        }
    }

    return fldcnt;
}        
