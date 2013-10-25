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

#include <stdlib.h>
#include "geminikey.h"
#include "pscdtype.h"

/***********************************************************************/
/* PROGRAM: gemIdxComponentToField - Extracts a component from a Gemini key
 *      and converts it to the format of a field in mySQL row.
 *     
 *
 * RETURNS:
 *      SUCCESS             
 *      BAD_COMPONENT     Error found in the component??????????
 *      BAD_PARAMETER     A bad parameter value was given 
 *      BUFFER_TOO_SMALL  The field buffer is too small for the data value
 *      BAD_FIELD_TYPE    An invalid field type was given
 */
int
gemIdxComponentToField(
		unsigned
		char    *pComponent,	/* IN, ptr to the start of the component in the key*/
		int    	fieldType,	/* IN  data type of the field */	   
		unsigned
        char    *pField,                /* IN, ptr to the area receiving the field data.
					   The size of the area must be at least that of fieldSize */ 
		int	fieldSize,      /* IN,  size of the receiving field 
                                   e.g. 1 for TINY, 2 for SMALL, 3 for MEDIUM, etc. */
        int     decimalDigits,  /* IN, (for DECIMAL type only) number of digit past the 
                                    decimal point in the field */
		int		*pfieldIsNull	/* OUT, 1 if the field value is a NULL value */   
        )  
{
	unsigned 
    char	*pFrom, *pTo, *pFirst, ch, left, right, buf[310];
    int     i, len, negative, exponent;


		/* Check for a NULL value */
	if ( pComponent[1] == 0xfb && pComponent[0] == 0x01 && pComponent[2] == 0x00 )
    {
        *pfieldIsNull = 1;
	    return SUCCESS;
    }
    else
        *pfieldIsNull = 0;

    if ( fieldSize <= 0 )
        return BUFFER_TOO_SMALL;

    switch(fieldType)
    {
    case GEM_INT:
/*    case GEM_DBKEY: */
        /* The field of all the above INTEGER types will always be returned in a 
        "Little-Indian" byte order */

        pFrom = pComponent;
        pTo = pField + fieldSize - 1; /* start at the end of the field */

        ch = *pFrom >> 4; /* get exponent */
        if ( ch < 8 )
        {
            negative = 1;
            len = 8 - ch; /* len is number of significant bytes */
        }
        else
        {
            negative = 0;
            len = ch - 7; /* len is number of significant bytes */
        }

        if ( fieldSize < len )
            return BUFFER_TOO_SMALL;

        /* fill the eliminated high order bytes */
        for (i = 0; i < (fieldSize - len); i++)
        {
            if ( negative )
                *pTo-- = 0xff;
            else
                *pTo-- = 0x00;
        }

        /* move the significant part */
        for (ch = *pFrom, i = 0; i < len; i++  )
        {
            *pTo = ch << 4;  /* right nibble to left nibble of the field byte */
            pFrom++;
            ch = *pFrom;
            /* check for escape sequences */
            if ( (unsigned char)(ch + 1) < 3 )
            {
                pFrom++;  /* to skip one byte */
                if (ch == 0x01)
                {
                    ch = *pFrom - 0xfe;  /* 0xfe to 0x00, 0xff to 01 */ 
                }
            }
            *pTo-- += ch >> 4;/*left nibble to right nibble of the field byte */
        }
        break;

    case GEM_FLOAT:
    case GEM_DOUBLE:
        /* The field of all the above INTEGER types will always be returned in a 
        "Little-Indian" byte order */

        pFrom = pComponent;
        

        if (*pFrom & 0x80)
            negative = 0;
        else
            negative = 1;

        for ( pFirst = pTo = pField + fieldSize - 1; /* start at the end of the field */ 
                                            *pFrom != 0x00; pTo-- )
        {
            ch = *pFrom++;
            /* check for escape sequences */
            if ( (unsigned char)(ch + 1) < 2 )
            {
                if (ch == 0x01)
                {
                    ch = *pFrom - 0xfe;  /* 0xfe to 0x00, 0xff to 01 */ 
                }
                else  /* ch was 0xff */
                    if ( *pFrom == 0x03 )
                        break;  /* it is the end of a negative component */
            }
            if ( pTo < pField)
                return BAD_FIELD_SIZE;

			if ( negative )
				ch ^= 0xff;  /* Negative number, flipp all bits */
			else
				if ( pTo == pFirst)
                    ch ^= 0x80;   /* sign bit only */
            *pTo = ch;
        }

        while ( pTo >= pField )
            *pTo-- = 0x00;       /* fill insignificant zeroes */

        break;

    case GEM_DECIMAL:

        if ( decimalDigits < 0 )
            return INVALID_DECIMAL_DIGITS;
        if (decimalDigits > (fieldSize -2 ) )
            return BUFFER_TOO_SMALL;

        /* check for zero value */
        if ( *pComponent == 0x80 )
        {
            /* the value is zero */
            if (decimalDigits > 0 )
            {
                len = decimalDigits + 2;
            }
            else
                len = 1;

            if ( fieldSize < len )
                return BUFFER_TOO_SMALL;

            for ( i = fieldSize - len; i > 0; i-- )
                *pField++ = ' '; /* insert leading filler spaces */

            *pField++ = '0';
            if (len > 1)
            {
                *pField++ = '.';
                for ( i = decimalDigits; i > 0; i-- )
                    *pField++ = '0';
            }

            return SUCCESS;
        }
            
        pFrom = pComponent;
        pTo = &buf[0];

        ch = *pFrom; /* get first byte */

        /* find out if the number is negative */
        negative = (ch & 0x80) ^ 0x80;

        /* get the exponent */
        if ( (ch & 0x80) == (( ch & 0x40) << 1 ) )
        {
            /* two bytes exponent */
            exponent = ch & 0x1f;
            exponent <<= 5;
            pFrom++;
            exponent += *pFrom >> 3;
            if (negative)
            {
                exponent ^= 0x03ff; /* flip the exponent bits */
                *pTo++ = '-';
            }
            exponent -= 512;
        }
        else
        {
            /* one byte exponent */
            exponent = ch & 0x3f;
            if (negative)
            {
                exponent ^= 0x03f; /* flip the exponent bits */
                *pTo++ = '-';
            }
            exponent -= 32;
        }
        pFrom++; /* point to the digit bytes */

        if ( exponent <= 0 )
            *pTo++ = '0';

        if ( exponent < 0 )
        {
            *pTo++ = '.';

            for (i = exponent; i < 0; i++)
                *pTo++ = '0';
        }


        /* move digits */
        while (1)
        {
            ch = *pFrom++;
            if (negative)
                ch ^= 0xff; /* flip the bits */

            left = ch >> 4;
            if (left == 0 )
                break; /* no more digits */

            if (exponent == 0 )
                *pTo++ = '.';   /* insert decimal point */

            *pTo++ = left - 2 + '0';    /* digit to buffer */
            exponent--;

            right = ch & 0x0f;
            if (right == 0 )
                break; /* no more digits */

            if (exponent == 0 )
                *pTo++ = '.';   /* insert decimal point */

            *pTo++ = right - 2 + '0';    /* digit to buffer */
            exponent--;
        }
        for ( ; exponent > 0; exponent-- )
            *pTo++ = '0';   /* add trailing zeroes */

        if ( exponent == 0 && decimalDigits > 0 )
            *pTo++ = '.';   /* insert decimal point */

        for ( i = -exponent; i < decimalDigits; i++ )
            *pTo++ = '0';   /* fill missing trailing zeroes */

        len = pTo - &buf[0];

        if (len > fieldSize)
            return BUFFER_TOO_SMALL;

        for ( i = fieldSize - len; i > 0; i-- )
            *pField++ = ' '; /* insert leading filler spaces */

        for (pTo = &buf[0]; len > 0; len-- )
            *pField++ = *pTo++;

        break;
    case GEM_CHAR:
      /* Called for character fields with binary attribute  */
      pFrom = pComponent;
      pTo = pField;
      /* move the significant part */
      for (ch = *pFrom, i=0 ; ch; i++ )
      {
	/* check for escape sequences */
	if ( (unsigned char)(ch + 1) < 3 )
	{
	  pFrom++;  /* to skip one byte */
	  if (ch == 0x01)
	  {
	    ch = *pFrom - 0xfe;  /* 0xfe to 0x00, 0xff to 01 */ 
	  }
	}
	*pTo++ = ch;
	pFrom++;
	ch = *pFrom;
      }
      for(;i < fieldSize; i++)
	*pTo++ = ' ';
      break;  
  
    default:
        return BAD_FIELD_TYPE;
                
    }   /* switch(fieldType) */
	return SUCCESS;
}

#ifdef VARIABLE_DBKEY
/***********************************************************************/
/* PROGRAM: gemDBKEYfromIdx - Extracts a DBKEY from a Gemini index key
 * 
 *     
 *
 * RETURNS:
 *      The significant bytes of the dbkey fron the key (less the last
 *      byte of the dbkey, which is in the INFO of the index entry          
 */
void
gemDBKEYfromIdx(
		TEXT    *pVarSizeDBKEY,	
		/* IN, ptr to the start of the variable DBKEY */
		DBKEY           *pdbkey) /* OUT - the DBKEY */
{  
       TEXT	*pFrom, *pTo, ch;
        int     i, negative, len;

        pFrom = pVarSizeDBKEY;
#if NUXI==1
        pTo = (unsigned char *)pdbkey + sizeof(DBKEY) - 1;
#else
        pTo = (unsigned char *)pdbkey;
#endif /* NUXI==1 */

        ch = *pFrom >> 4; /* get exponent */
        if ( ch < 8 )
        {
            negative = 1;
            len = 8 - ch; /* len is number of significant bytes */
        }
        else
        {
            negative = 0;
            len = ch - 7; /* len is number of significant bytes */
        }

        /* fill the eliminated high order bytes */
        for (i = 0; i < ((int)sizeof(DBKEY) - len); i++)
        {
            if ( negative )
                *pTo = 0xff;
            else
                *pTo = 0x00;
#if NUXI==1
            pTo--;
#else
            pTo++;
#endif /* NUXI==1 */
        }

        /* move the significant part */
        for (ch = *pFrom, i = 0; i < len; i++  )
        {
            *pTo = ch << 4;  /* right nibble to left nibble of the field byte */
            pFrom++;
            ch = *pFrom;
            *pTo += ch >> 4;/*left nibble to right nibble of the field byte */
#if NUXI==1
            pTo--;
#else
            pTo++;
#endif /* NUXI==1 */
        }
	/* Now transfer the part of the dbkey that is not part of the key */
	pFrom++;
	*pdbkey <<= 8;
	*pdbkey += *pFrom;
	return;
}  /* end gemDBKEYfromIdx */

#endif  /* VARIABLE_DBKEY */
/***************************************************************************/
/***                           Local Functions                           ***/
/***************************************************************************/
