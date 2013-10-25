
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
#include "dstd.h"
#include "geminikey.h"
#include "cxpub.h"
#include "utspub.h"
 

/***************************************************************************/
/***                   Gemini KeyServices Functions                    ***/
/***************************************************************************/
/* PROGRAM: Sct - copies two least significant byte of an int to storage, 
  like in a data record
 *
 * RETURNS: VOID
 */
void
Sct (unsigned char	*pt,	/* ptr to target storage area for COUNT */
	 int  ct)	    /* value to be stored */
{
    *++pt = (unsigned char)ct;
    ct >>= 8;
    *--pt = (unsigned char)ct;
}
/***************************************************************************/
/* PROGRAM: bufferCop - copies one buffer to another
 *
 * RETURNS: VOID
 */
void
bufferCopy (
		unsigned char	*pt,	/* ptr to target storage area */
		unsigned char	*ps,	/* ptr to target storage area */
		int  bytes)	    /* number of bytes to be moved */
{
	int i;
    for (i=0; i<bytes; i++, pt++, ps++)
	{
		*pt = *ps;
	}
}

/***********************************************************************/
/* PROGRAM: gemKeyInit - Initialize leading bytes of key
 *
 */
int
gemKeyInit(unsigned char *pKeyBuf, int *pKeyStringLen, short index)
{
    pKeyBuf[FULLKEYHDRSZ] = 0;
    pKeyBuf[FULLKEYHDRSZ+1] = 0;
    sct(pKeyBuf + FULLKEYHDRSZ+2,index);
    /* For the six byte header of ts, ks, is on every key and
       the two fun bytes which are guaranteed
       to be zero and the two byte index number */
    *pKeyStringLen = FULLKEYHDRSZ+4;

     return 0;
}

/* PROGRAM: gemKeyLow - Create a lower than any possible key low key
 */
int
gemKeyLow(unsigned char *pkeyBuf, int *pkeyStringLen, short index)
{
    int  rc;
    int  keylen;

    rc = gemKeyInit(pkeyBuf, pkeyStringLen, index);
    rc = gemKeyAddLow(&pkeyBuf[*pkeyStringLen],&keylen);
   *pkeyStringLen += keylen;
    return 0;
}

/* PROGRAM: gemKeyAddLow - Add a lower than any possible key low component
*/
int gemKeyAddLow(unsigned char *pkeyBuf, int *pkeyStringLen)
{
    pkeyBuf[0] = 0x01;
    pkeyBuf[1] = 0x01;
    pkeyBuf[2] = 0x00;
    *pkeyStringLen = 3;

    return 0;
}

/* PROGRAM: gemKeyHigh - Create a higher than any possible key, key
*/
int gemKeyHigh(unsigned char *pkeyBuf, int *pkeyStringLen, short index)
{
   int rc;
   int keylen;

   rc = gemKeyInit(pkeyBuf, pkeyStringLen, index);
   rc = gemKeyAddHigh(&pkeyBuf[*pkeyStringLen],&keylen);
   *pkeyStringLen += keylen;
   return rc;
}

/* PROGRAM: gemKeyAddHigh - Add a higher than any possible key high component
 */
int gemKeyAddHigh(unsigned char *pkeyBuf, int *pkeyStringLen)
{
    pkeyBuf[0] = 0xff;
    pkeyBuf[1] = 0xff;
    pkeyBuf[2] = 0xff;
    pkeyBuf[3] = 0x00;
    *pkeyStringLen = 4;
    return 0;
}


/***********************************************************************/
/* PROGRAM: gemFieldToIxComponent -
 *
 * Given a MySQL field of any data type, this procedure generate 
 * an Index component according to its respective value and type.
 * The component is in its final format as it should be in the index entry,
 * including a NULL component separator.
 *
 * RETURNS:
 */
int
gemFieldToIdxComponent(
		unsigned
        char    *pField,		/* IN, ptr to the data part of the field
					   in the MySQL row. This is useually the 1st
					   byte of the field. But in the case of VARCHAR
					   or BLOB the beginning of the field may be the
					   lenght or a pointer. In these case, pField
					   should point to the 1st byte of the actual data,
					   not to the 1st byte of the field.
					   In case of NATIONAL CHAR, or NATIONAL VARCHAR
					   where weights are used instead of the original
					   data, pField should point to the string of weights
					   not the original data.
					*/
        unsigned 
		long		fldLength,		/* IN, number of actual data bytes in  the field.
							   Not including other bytes, like legth bytes
							   of a VARCHAR field 
							*/
		int		fieldType,		/* IN  data type of the field */	   
		int		fieldIsNull,	/* IN, 1 if the field value is a NULL value */				   
		int		fieldIsUnsigned,/* IN, 1 if the field value is a NULL value */
		unsigned
		char    *pCompBuffer,	/* IN, ptr to area receiving the component */
		int		bufferMaxSize,	/* IN, max size of the component buffer */
        int		*pCompLength)	/* OUT, lenght in byte of the component */
{
	unsigned char	buff[1000];
	unsigned char	*pBuffer = &buff[0];
	unsigned char	*pFrom, *pTo, *pFldEnd, *pMaxComponent, ch;
	unsigned int	significantByte;
	unsigned int    negative;    /* negative number */
	unsigned int	insignificantFloatBytes;
	unsigned int	rightNibble, rihghtMostNonzero, leftMostNonzero, decimalPointPosition; 
	int	            len, pos, exponent, i;
   
   /* sanity-checks */

    if (   pField == NULL  || pCompBuffer == NULL )
        return BAD_PARAMETER;

	if ( bufferMaxSize < 3 )
        return BUFFER_TOO_SMALL;

	if ( fieldIsNull )	/* the field value is NULL */
	{
		/* component must be sorted lower than any other value */
		pCompBuffer[0] = 0x01; /* escape */
		pCompBuffer[1] = 0xfb; /* Low Unknown */
		pCompBuffer[2] = 0x00; /* component separator */
        *pCompLength = 3;
        return SUCCESS;
	}

            
    switch(fieldType)
    {
    case GEM_ENUM:
    case GEM_SET:
    case GEM_DATE:
    case GEM_TIME:
    case GEM_DATETIME:
    case GEM_TIMESTAMP:
    case GEM_YEAR:
            fieldIsUnsigned = 1; /* the types above are always unsigned */
    case GEM_TINYINT:
    case GEM_SMALLINT:
    case GEM_MEDIUMINT:
    case GEM_INT:
	case GEM_BIGINT:
     /* All the above types can be handled the same way. All are
	stored in a "little Indian" byte order in the row field.

	The generated key component of all the above types will be as follows:

	The 1st nibble of the 1st byte(4 most significant bits) of the key 
	component holds the biased "exponent".
	   
	Leading 0x00 bytes of positive numbers are removed and not participat in
	the key component.  Same with leading 0xff bytes of negative numbers. 
	The last byte is never removed even if it has values of 0x00 or 0xff.

	Significant bytes of the original number are moved nible by nible
	to the key component from left to right. The 1st nibble of the number
	is moved the 2nd nibble of the key component( following the 
	biased nibble).
	The 2nd nibble of the number is moved to the 3rd nibble of the 
	key component. 3rd to 4th, etc.

	Last nibble contains the size of the component WITHOUT the extra
    bytes that are added if the component includes 0x00, 0x01 or 0xff.
    Therefore, to get the correct component size the number of escape
    sequence byte must be added to the last nibble.

	The exponent nibble is set with bias as follows:
	positive numbers exponent = 7 + # of significant bytes. 
	negative numbers exponent = 8 - # of significant bytes.
	   
	Value		in hex			signif. bytes	exponent	key-component
	 -257		0xfffffeff			2				6		0x 6f ef f3
	 -256		0xffffff00			1				7		0x 70 02
	 -2		0xfffffffe			1				7		0x 7f e2
	 -1		0xffffffff			1				7		0x 7f f2

	  0		0x00000000			1				8		0x 80 02
	  1		0x00000001			1				8		0x 80 12
	  2		0x00000002			1				8		0x 80 22
	256		0x00000100			2				9		0x 90 10 03
	x           	0x009abcde          		3				10      0x a9 ab cd e4
    y           0x00b00ff3          3               10      0x ab 01fe ff01 36

		NOTICE - the correct sort order of the key-components.
		NOTICE - the last byte will not get values 0x00, 0x01 or 0xff.
    */

	if ( fldLength > 8 || fldLength < 1)
		return BAD_FIELD_SIZE;

	exponent = 0;
    
	significantByte = 0; /* significant byte not encountered yet */

	/* Process the field bytes in a "Little-Indian" order */

	pFrom = &pField[fldLength - 1];

	if ( pFrom[0] & 0x80  /* negative  number */
  	     && ! fieldIsUnsigned )
	    negative = 1;
	else
	    negative = 0;

	pBuffer[0] = 0; /* Init exponent to 0 */

	for ( pTo = pBuffer, i = 0;
		(unsigned int)i < fldLength;
		i++, pFrom--)
	{
	    ch = *pFrom;
	    if ( ! negative)
            {
	        /* Positive integers */
		if (! significantByte)
		{
                    if ( ch == 0 
		    && (fldLength - i) > 1) /* last byte is always accepted */
			continue;		/* skip leading zeroes */
		    significantByte = 1; /* found 1st significant byte */
		}
	     }
	     else
	     {
	        /* Negative integers */
		if (! significantByte)
		{
	            if ( ch == 0xff
			&& (fldLength - i) > 1) /* last byte is always accepted */
			continue;		/* skip leading 0xff */
		    significantByte = 1; /* found 1st significant byte */
		}

            } /* negative */

	    *pTo |= ch >> 4; /* high nibble of ch to low nibble the left byte */

	    /* check for 0x00, 0x01 and 0xff to replace them */
	    if(  ((unsigned char)(*pTo + 1) < 3)
            && pTo != pBuffer)
            {	
	        if ( *pTo == 0x00 )
		{
                    /* significant 0 converts to 0x01fe */
		    *pTo++ = 0x01;
		    *pTo = 0xfe;
		}
		else if ( *pTo == 0x01)
		{
		   /* 0x01 byte converts to 0x01ff */
		    *pTo++ = 0x01;
		    *pTo = 0xff;
		}
		else
		if ( *pTo == 0xff)
		{
		    /* 0xff byte converts to 0xff01 */
		    pTo++;
		    *pTo = 0x01;
		}
        
	    }
	    pTo++;
	    *pTo = ch << 4; /* low nibble of ch to high nibble of the right byte */
                    
	    exponent++; /* count significant bytes in exponent */

	} /* end of moving bytes */


        /* at this point pTo is on the last byte */
	*pTo++ |= exponent + 1; 
        /* length of the  component ( without escape sequence bytes) to the last nibble */
	if ( negative )
		exponent = 8 - exponent;
	else
	    exponent += 7;
	*pBuffer |= exponent << 4; /* biased exponent to leading nibble */

	*pCompLength = pTo - pBuffer; /* length of component in bytes */


       break; /* end of integers */

    case GEM_FLOAT:
		if ( fldLength != 4 )
			return BAD_FIELD_SIZE;
    case GEM_DOUBLE:
      /* FLOAT and DOUBLE can be handles the same way.
	 The bytes are rearranged from little indian to big indian order.
	 The sign bit is flipped, it need to be 
	 1 for positive and 0 for negative numbers, for correct sort order.
	 in the case of negative number also all the bits are flipped,
	 0 bit become 1 and 1 bit becomes 0, again for correct sort order.
	 All insignificant trailing 0x00 bytes are eliminated from 
	 positive numbers
	 and all insignificant trailing 0xff bytes are eliminated from 
	 negative numbers.
      */
      if ( fieldType == GEM_DOUBLE && fldLength != 8 )
	return BAD_FIELD_SIZE;

      if ( pField[fldLength - 1] & 0x80)  /* negative  number */
	negative = 1;
      else
	negative = 0;

      /* count (in i) number of leading zero bytes */
      /* these are the insignificant zeroes of the floating point number */
      for ( i = 0; pField[i] == 0 && (unsigned int)i < (fldLength - 1); i++ ) ;
      insignificantFloatBytes = i;
 
      /* Move bytes in reverse order, from little-indian to big */
      for ( pTo = pBuffer, pFrom = pField + fldLength - 1,len = 0
	      ; (unsigned int)len < (fldLength - insignificantFloatBytes)
	      ;len++, pFrom-- )
      {
	*pTo = *pFrom;     /* move the byte */
	if ( negative )
	  *pTo ^= 0xff;  /* Negative number, flipp all bits */
	else
	  if ( pTo == pBuffer)
	    *pTo ^= 0x80;   /* sign bit = 1 for correct sort order*/


	switch(*pTo)
	{
	case 0x01:	/* 0x01 byte converts to 0x01ff */
	  *pTo++ = 0x01;
	  *pTo++ = 0xff;
	  break;

	case	0x00:    /* 0x00 byte converts to 0x01fe */
	  *pTo++ = 0x01;
	  *pTo++ = 0xfe;
	  break;

	case 0xff:		/* 0xff byte converts to 0xff01 */
	  *pTo++ = 0xff;
	  *pTo++ = 0x01;
	  break;
				
	default:
	  pTo++; /* leave the byte as is */          
	} /* end switch(*pTo) */
			
      } /* end of move bytes */


      if ( insignificantFloatBytes  /* insignificant bytes where eliminated */
	   && negative) /* Negative number */
      {
	*pTo++ = 0xff; /* for correct sort order of negative */
	*pTo++ = 0x03; /* for correct sort order of negative */	
      }
      *pCompLength = pTo - pBuffer; /* length of component in bytes */
      break;

    case GEM_DECIMAL:
		/* 
		In the field:

		A DECIMAL field is stored in the mySQL field in right justified 
		with blank (0x'20') bytes filler on the left. A negative number 
		has a '-' character to the left of the number (filler is to the left
		of the '-' minus sign.
		Each digit is represented by one byte ASCII value, 1 is 0x31,
		2 is 0x32, ...9 is 0x39. The decimal point is represented in it
		respective position by a '.' character.

		Max number of possible digits are 308, max value: digit * 10 ** 308.

        In The index component:

		In the key component the first one or two bytes are used to stored "exponent",
		followed by all the significant digits, two digit per byte (packed decimal), 
		one digit per nibble. 
		Only significant digits are used. All leading and trailing zeroes are
		eliminated.
		
		The significant digits in the nibbles are stored with a bias of 2 as follows: 
		digit 0 stored as 2, digit 1 stored as 3, digit 2 as 4, ...9 as 11 (0xb).

		For negative numbers the bits of all the digits are flipped.

        Following all the digits is a terminator nibble, 0 nibble for positive
		numbers and f nibble for negative numbers. A whole byte is added if
		the number of digits is an even number,  the byte is 0x0f for positive
		0xf0 for negative numbers.

		The "exponent" is constracted as follows:

        If the absolute value of the exponent (unbiased) is between 0 and 30
		the exponent occupies only one byte, as follows:
		   For positive decimal numbers:
		       bit 0 (most significant) is 1.
		       bit 1 is 0 to mark the byte as a "One byte exponent".
		       bits 2-7 hold the exponent value biased by 32.

		   For negative decimal numbers:
		       Constracted as for positive numbers, then all the exponent bits
               are flipped.

		If the absolute value of the exponent (unbiased) is greater than 31
		the exponent occupies two bytes, as follows:
         
		   (the bits are numberd from left to right, from 0 to 15.
		   bit 0 is the most left bit of the most left byte).
		   For positive decimal numbers:
		       bit 0 (most significant) is 1.
		       bit 1 is 1 to mark it as a "Two bytes exponent".
		       bit 2 is set to 0 (to avoid 0x00, 0x01, 0xff in the high byte).
		       bits 3-12 (10 bits) hold the exponent value biased by 512 .
		       bit 13 is always 0 (to avoid 0x00, 0x01, 0xff).
		       bit 14 is always 1 (to avoid 0x00, 0x01, 0xff).
		       bit 15 is always 0 (to avoid 0x00, 0x01, 0xff).

		   For negative decimal numbers:
		       Constracted as for positive numbers, then all the exponent bits are flipped.

          When looking at a finished decimal key component, you can tell if the exponent is in
		  one byte, or in two bytes by checking the two most significant bits ( bit 0 and bit 1)
		  as follows:
		    In a "Two bytes exponent" the two bits are the same. Either both
		        are 1, or both are 0.
		    A "One byte exponent" is recognized by bit 0 different from bit 1.

        SPECIAL CASE: A decimal number with the value 0, is a special case. It index component
		consist of only one byte with the value 0x80. This will make it sort higher than all
		the negative numbers, and lower than all the positive numbers.


		One byte exp:

               Value		      Exponent	      exp. byte	          Rest of 
		                        value   biased      in key            the key 
        Positive numbers:
		  0       =0*10**?        ?     0         0x00|0x80(+ sign)   none
		  .012    =.12*10**-1     -1    32-1=31   0x1f|0x80=0x9f      0x340f
		  .123    =.123*10**0      0         32   0x20|0x80=0xa0      0x3450      
		  1.23    =.123*10**1      1    32+1=33   0x21|0x80=0xa1      0x3450
		  12.3    =.123*10**2      2    34        0x22|0x80=0xa2      0x3450 
		  123.000 =.123*10**3      3    35        0x23|0x80=0xa3      0x3450
		        .123000*10**30     30   32+30=62  0x3e|0x80=0xbe      0x3450

		  Negative numbers:
		  -12.300 =-.123*10**2     2    32+2=34   0x22^0xff=0x4d      0xcba0                
		  -1.23   =-.123*10**1     1         33   0x21^0x7f=0x4e      0xcba0
		  -.123   =-.123*10**0     0         32   0x20^0x7f=0x4f      0xcba0
		  -.012   =-.12*10**-1    -1         31   0x1f^0x7f=0x60      0xcbf0


    Two bytes exponent:

          Value		             Exponent	          exp. byte	               Rest of 
		                       value    biased         in key                  the key
	Positive numbers:
	.123/(10**33)=.123*10**-33  -33  512-33=479 (0x01df*8)|0xc002=0xcefa       0x3450
	1.23*10**33=.123*10**34      34  512+34=546 (0x0222*8)|0xc002=0xd112       0x3450

	Negative numbers:
	-.123/(10**33)=.123*10**-33 -33  512-33=479 (0x01df*8)|0x4002^0x7fff=0x3105 0xcba0
	-1.23*(10**33)=.123*10**34   34  512+34=546 (0x0222*8)|0x4002^0x7fff=0x2eed 0xcba0
		  

		NOTICE: 
		The complex exponent design, the bias of each digit and the terminator nibble,
		assure that:
		1) No bytes will ever have values of 0x00, 0x01 or 0xff.
		2) Sorting keys by comparing bytes from left to right will
		produce the correct order according to the respective numerical values.
		3) By eliminating leading and trailing zeroes the component size is kept
		to a minimum.
		*/

		if ( fldLength > 308 || fldLength < 1) /* 308 max precision in MySQL*/
			return BAD_FIELD_SIZE;

		negative = 0;
		exponent = 0;
		rightNibble = 0;

		/* Scan from right to left to find the positions of the right-most-non-zero,
		the left-most-non-zero and the decimal point. Stop the scan when a filler space
		or a negative sign is encountered */
		rihghtMostNonzero = leftMostNonzero = 0;
		decimalPointPosition = fldLength + 1; /* assume decimal point to the right */
	
		for (pos = fldLength;  /* right most position */
		        (ch = pField[pos - 1]) != ' '  /* stop at a filler space */
			  && pos > 0;
				    pos--)
		{
			if (ch == '.')
			{
				decimalPointPosition = pos; /* position of the decimal */
				continue;
			}
		    else
			    if (ch == '-')
				{
					negative = 1; /* the field value is negative */
					break;
				}
				else
					if (ch > '0' && ch <= '9')
					{
						if (rihghtMostNonzero == 0)
						{
							rihghtMostNonzero = leftMostNonzero = pos; /* found right most non zero */
						}
						else
							leftMostNonzero = pos;  /* found left most non zero */
					}
					else
						if (ch == '0')
							continue;   /* ignore zeroes for now */
						else
				            return BAD_DECIMAL_DIGIT;

		}

		if (leftMostNonzero + rihghtMostNonzero ==0)
		{
			/* the number is zero, no non-zero digits found */
			*pBuffer = 0x80;
			*pCompLength = 1;
			break;
		}

		/* 
		calculat the exponet. 
		The exponent = 0 if the left most no zero digit is just to the right of the decimal point:
		   Exponent = 0 for  0.87, .23, 0.11111 etc.
		The exponent = -n  for a fraction where n = number of zeroes just right of the decimal point:
		   Exponent = -3 for 0.00087, .00023, 0.00011111 etc.
		The exponent = n (positive) for a number >= 1, where n = number of digits to the left of the
		decimal point:
		   Exponent = + 3 for 870.00, 870.11, 000230, 111.11 etc.
		*/

        exponent = decimalPointPosition - leftMostNonzero;
		if ( leftMostNonzero > decimalPointPosition) /* if it is a fraction, (number < 1) */
			exponent++; /* adjust to correct exxponent of a fraction */

				    
		/* 
		Move only significant digits from left to right into the component.
		Each digit in the field occupies one byte, but only half a byte (a nibble) in 
		the key component (like in packed decimal).  Each digit in its nibble is also biased by +2.
		Examples: the digit 0 becomes 0x2, 1 becomes 0x3, 9 becomes 0xb, etc.
		Digits of a negative number are 1 complimented (all bits are flipped) for correct sort.
		*/

		for ( pTo = pBuffer +2,  /* skip the 1st 2 bytes, it is for "exp" */
			  pFrom = &pField[ leftMostNonzero - 1 ],
			  rightNibble = 0,			  
			  len = rihghtMostNonzero - leftMostNonzero + 1;
		            len > 0;
					    len--, pFrom++
			)
		{
			/* get the digit and prepare it for the nibble in ch */
            ch = *pFrom;     /* get the digit */
			if (ch == '.')
				continue;
			ch += 2;     /* bias to avoid 0x00 and 0x01 */
			if ( negative )
				ch ^= 0xff; /* flip all bits for correct sort order */
			ch &= 0x0f;  /* remove the left nibble */


			/* move significant digit to their respective nibble */
			if ( rightNibble )
			{
				*pTo += ch; /* add right nibble to the already there left one */
				pTo++;
				rightNibble = 0;
			}
			else
			{
				/* left nibble */
				*pTo =  ch << 4; /* left  nibble to its place */
				rightNibble = 1;
			}
		}

		/* add terminator nibble of byte */
		if ( rightNibble )
		{
			if (negative)
			 *pTo |= 0x0f; /* terminator right nibble is 0xf */
			pTo++;
		}
		else
		{
				/* left nibble, needs an extra byte for terminator */
			if (negative)
				*pTo++ =  0xf0; /* negative terminator byte */
			else
				*pTo++ =  0x0f; /* positive terminator byte */
		}

		/* Construct the exponent to the left of the component */
		if ( exponent > -31 && exponent < 31)
		{
			/* exponent fits in one byte */
		
			exponent += 32; /* with bias 32 the exponent now have positive values  0 - 63 */
			
			if (negative)
				exponent ^= (int)0x7f; /* flipp the bits except the sign bit, it stays 0 */
			else
				exponent += 128; /* set bit 0 to 1, to mark positive number */
		    
			*(pBuffer++ + 1) = (unsigned char)exponent; /* eponent to 2nd byte, skip the 1st byte */
		}
		else
		{
			/* exponent needs two bytes */
			exponent += 512; /* now the exponent value are between (512 - 308) to (512 + 308) */
			exponent <<= 3;  /* to bits 3 through 12 */
			exponent += 16384; /* bit 1 set to 1 to mark a two bytes exponent */
			exponent +=2;      /* bit 14 is always set to 1 (to avoid 0x00, 0x01, 0xff)*/
			if (negative)
				exponent ^= (int)0x7fff; /* flipp the bits except the sign bit, it stays 0 */

			else
				exponent += 32768;  /* set bit 0 is set to 1, to mark positive number */
			
			Sct(pBuffer, (short)exponent); /* exponent to the leding two bytes of the component */
		}

		*pCompLength = pTo - pBuffer; /* length of component in bytes */

        break;

    case GEM_CHAR:
    case GEM_VARCHAR:
		if ( fldLength > 255 )
			return BAD_FIELD_SIZE;

		/* find number of useful bytes (without the filler space bytes) */
		/* But make to sure to leave one blank if the field is nothing
		   but blanks.   */
		for (; pField[fldLength -1] == ' ' && fldLength > 1; fldLength-- ) ;
		/* at this point fldLength is not the original size of the field,
		it is the size after eliminating filler spaces */

    case GEM_TINYBLOB:
    case GEM_TINYTEXT:
		if ( fldLength > 255 )
			return BAD_FIELD_SIZE;
    case GEM_BLOB:
    case GEM_TEXT:
		if ( fldLength >= (unsigned long)(1<<16) )
			return BAD_FIELD_SIZE;
    case GEM_MEDIUMBLOB:
    case GEM_MEDIUMTEXT:
		if ( fldLength >= (unsigned long)(1<<24) )
			return BAD_FIELD_SIZE;
    case GEM_LONGBLOB:
    case GEM_LONGTEXT:
		if ( fldLength > (unsigned long)((long)(-1)))
			return BAD_FIELD_SIZE;

		len = 0;

		/* Move bytes in the same order, replace 0x00, 0x01, 0xff */
		for ( pTo = pBuffer, pFrom = pField
			, pFldEnd = pField + fldLength -1
			, pMaxComponent = pBuffer + MAXCOMPONENTSIZE
									+ 1 /* to cause the return of TRUNCATEDCOMPONENT status */
			    ; pFrom <= pFldEnd   && pTo <= pMaxComponent									
				    ;pFrom++ )
		{
           switch(*pFrom)
			{
              case 0x01:	/* 0x01 byte converts to 0x01ff */
				  len += 2;
				  *pTo++ = 0x01;
				  *pTo++ = 0xff;
			      break;

              case	0x00:    /* 0x00 byte converts to 0x01fe */
			      *pTo++ = 0x01;
			      *pTo++ = 0xfe;
				  break;

              case 0xff:		/* 0xff byte converts to 0xff01 */
				  *pTo++ = 0xff;
				  *pTo++ = 0x01;
                  break;
				
			  default:
				  *pTo++ = *pFrom; /* leave the byte as is */ 
				  
			} /* end switch(*pFrom) */
		} /* end of move bytes */
		
		*pCompLength = pTo - pBuffer; /* length of component in bytes */

        break;


    default:
        return BAD_FIELD_TYPE;
                
    }   /* switch(fieldType) */


	
	/* add the componnents separator */
	pBuffer[ *pCompLength ] = 0x00;
	*pCompLength += 1;

	
	/* check if receiving buffer big enough */
	if ( *pCompLength > bufferMaxSize)
		return BUFFER_TOO_SMALL;

	/* return the component to the user supplied buffer */
	bufferCopy( pCompBuffer, pBuffer, *pCompLength );


	/* check if component excceds maximum allowed 
	   max key size is 254, the component is limited to MAXCOMPONENTSIZE
	   to allow for overhead: index number, key header, large row id, etc. */
	if ( *pCompLength > MAXCOMPONENTSIZE)
		return TRUNCATED_COMPONENT;
	else
		return SUCCESS;


}  /* end gemFieldToIdxComponent */





/***********************************************************************/
#ifdef VARIABLE_DBKEY
/******************************************************************/
/* PROGRAM: cxDbkeyToLittleIndian - stores the DBKEY as a little-indian string
 *
 *    NOTICE:  dbkey must be defined as a 4 bytes integer or a 8 bytes integer
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxDbkeyToLittleIndian
    (TEXT	*pt,	/* ptr to storage area for result */
     DBKEY   dbkey) /* the record's dbkey             */
{
    int len;

    for (len = sizeof(DBKEY); len; len--)
    {
        *pt++ = (TEXT)dbkey;
        dbkey >>= 8;
    }
}   /* cxDbkeyToLittleIndian */
/***********************************************************************/
/* PROGRAM: gemDBKEYtoIdx -
 *
 * Given a DBKEY of a record, this procedure converts it to the variable size DBKEY 
 * format to be used in the index key of the record.
 * NOTICE!!! - use this only for the DBKEY of the record pointed by the 
 *             index entry NOT for a DBKEY which is a component of the key.
 *
 *  The size in bytes of the variable size DBKEY is set in its last nibble.
 *
 * RETURNS:  The size in bytes of the variable size DBKEY. 
 */
int
gemDBKEYtoIdx(
	      DBKEY   dbkey,          /* IN - the 32 or 64 bits dbkey */
	      int	minimumSize,  
	      /* IN, minimum size of variable DBKEY in the key
		 including one extra byte for the exponent & size nibbles*/
	      /* IN, ptr to area receiving the Variable size DBKEY */
	      TEXT   *pTo 
            )
{

    TEXT	*pFrom, ch;
	int     negative;    /* negative number */
	int	    exponent, i;
    int     extra;  /* additional bytes to meat the minimum required by minimumSize */
   
    TEXT    string[10];
    TEXT    *pStart = pTo;
    TEXT    *pos;
            
    /* The last nibble of the last byte of the key component hold the length
       in bytes of the Variable size DBKEY.

       The last nibble length is needed to handle a variable length
       DBKEY, and it also prevents the last byte from beeing 0x00, 0x01 or0xff.


       The exponent nibble is set with bias as follows:
       positive numbers exponent = 7 + # of significant bytes. 
       negative numbers exponent = 8 - # of significant bytes.
	   
       Value		in hex			signif. bytes	exponent	key-component
       -257		0xfffffeff			2				6		0x 6f ef f3
	 -256		0xffffff00			1				7		0x 70 02
	 -2			0xfffffffe			1				7		0x 7f e2
	 -1			0xffffffff			1				7		0x 7f f2

	  0			0x00000000			1				8		0x 80 02
	  1			0x00000001			1				8		0x 80 12
	  2			0x00000002			1				8		0x 80 22
	256		    0x00000100			2				9		0x 90 10 03
	x           0x009abcde          3				10      0x a9 ab cd e4

    */


	exponent = 0;
    if ( minimumSize < 2 )
        minimumSize = 2;
    if ( minimumSize > 9) 
        minimumSize = 9;

	if ( dbkey < 0 ) /* negative  number */
	    negative = 1;
	else
	    negative = 0;
    
	/* Process the bytes in a "Little-Indian" order */

    extra = minimumSize - (sizeof(DBKEY) + 1); /* the 1 is for the exponent and
                                               size nibbles, which are added later */
    ch = (negative) ? 0xff : 0x00;
    exponent = sizeof(DBKEY); /* count significant bytes in exponent */

    if ( extra > 0 ) /* if (for example) transaction number is stored in more
                                      than 5 bytes when DBKEY is 4 bytes */
    {
        /* need to extend the dbkey with extra bytes */
#if NUXI==1     /* if litle indian already */
        bufcop( &string[0], &dbkey, sizeof(DBKEY) );
#else
        cxDbkeyToLittleIndian( &string[0], dbkey );
#endif
        pos = &string[sizeof(DBKEY)];  /* position to extra bytes */
        for (i=0; i < extra; i++ )
            *pos++ = (negative) ? 0xff : 0x00;
        exponent += extra;  /* add to count of significant bytes in exponent */

        pFrom = pos - 1;
    }
    else
    {
        /* no extra bytes, byt ma need to eliminate leading 0x00 or 0xff */
#if NUXI==1     /* if litle indian already */
        /* start with most significant byte */
        pFrom = (TEXT *)&dbkey + sizeof(DBKEY) - 1;
#else
        cxDbkeyToLittleIndian( &string[0], dbkey );
        pFrom = &string[0] + sizeof(DBKEY) - 1; /* start with most significant byte */
#endif
        /* eliminate leading 0x00 or 0xff */
        for ( ; extra < 0 ; extra++)
        {
            if (*pFrom != ch)
                break;
            
            pFrom--;
            exponent--;  /* decrease count of significant bytes in exponent */
        }
    }

	pTo[0] = 0; /* Init exponent to 0 */

	for ( i = 0; i < exponent; i++, pFrom--)
	{
	    ch = *pFrom;
	    *pTo |= ch >> 4; /* high nibble of ch to low nibble the left byte */
	    pTo++;
	    *pTo = ch << 4; /* low nibble of ch to high nibble of the right byte */
                    
	} /* end of moving bytes */


    /* at this point pTo is on the last byte */
	*pTo |= exponent + 1;       /* length of the DBKEY to the last nibble */
	if ( negative )
		exponent = 8 - exponent;
	else
	    exponent += 7;
	*pStart |= exponent << 4; /* biased exponent to leading nibble */

	return *pTo & 0x0f;

}  /* end gemDBKEYtoIdx */
/***********************************************************************/
#endif  /* VARIABLE_DBKEY */


/***********************************************************************/
