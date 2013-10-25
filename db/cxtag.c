
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

/*************************************************************/
/* cx - Compressed Index module.                             */
/* cxtag.c - handles lists of least significant bytes of     */
/*           record id tags which are stored in the          */
/*           INFO part of the B-tree entries. The lists are  */
/*           stored in the INFO part as a byte list          */
/*           with one byte per tag up to 32 tags per entry,  */
/*           or as a 32 bytes bit map preceded by a count    */
/*           byte, where tag is represented by a "on" bit.   */
/*           The bit map may represent from 33 to 256 tags.  */
/*           The bit map is always 32 bytes long             */
/*           The bit map is preceeded by a tag count byte.   */
/*************************************************************/

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "bkpub.h"
#include "dsmpub.h"
#include "cxpub.h"
#include "cxprv.h"

/*****************************************************************/
/* PROGRAM: cxAddBit - add the bit that represents the given tag.
 *          The bit list is a 1 byte bit count followed by a 
 *          32 byte bit map.
 *
 * RETURNS: DSMVOID
 */ 
DSMVOID
cxAddBit(TEXT   *plist,         /* bit list pointer */
         TEXT    tagbyte)       /* bit no. to set */
{
FAST    TEXT    mask;
FAST    TEXT    *pos;

    pos = plist + 1 + (tagbyte >> (TEXT)3);
    mask = (1 << (tagbyte & 7));        /* set the respective bit */
    if ((*pos & mask) == 0)             /* bit not already on */
    {
        *pos |=  mask;                  /* set it */
        if (*plist == 255)
            *plist = 0;                 /* a count of zero means 256 */
        else
            *plist += 1;
    }
}

/*******************************************************************/
/* PROGRAM: cxDeleteBit - delete the bit that represents the given tag
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxDeleteBit(TEXT        *plist, /* to the list of bits, 1st byte is a bit count
                           followed by 32 bytes bit map */ 
         TEXT    tagbyte)       /* the least significant byte of the tag */
{
FAST    TEXT    *pos;
FAST    TEXT    mask;

    pos = plist + 1 + (tagbyte >> (TEXT)3);
    mask = 1 << (tagbyte & 7);  /* mask with the respective bit set on */
    if (*pos & mask)            /* bit is on */
    {
        *pos &= ~mask;          /* turn off the respective bit */
        if (*plist == 0)
            *plist = 255;       /* zero bit counts stands for 256 */
        else
            *plist -= 1;        /* decrement bit count */
    }
}

/********************************************************************/
/* PROGRAM: cxFindTag - search for a given tag in the list or bit map
 *
 * RETURNS:
 *      0                       if found and op == CXFIND
 *      tag value(0-255)        if found for CXNEXT or CXPREV
 *      -1                      if not found for CXFIND
 *      -1                      if there is no next or prev in given entry
 *                              for CXNEXT or CXPREV
 */
int
cxFindTag(TEXT  *pent, /* to the B-tree entry with list of bytes or bit map */
         TEXT    tagbyte,       /* the least significant byte of the tag */
         int     op)            /* operation: CXFIND, CXNEXT or CXPREV */
{
FAST    int     bit;
FAST    TEXT    *pos;
        int      offset;
/*        int     ks, is; */
        int     is;

        int     i;

/*    pos = pent + 1;              skip cs byte */ 
/*    ks = *pos++; */
/*    is = *pos++; */
/*    pos += ks;                   to info */ 
    is  = IS_GET(pent);
    pos = pent + HS_GET(pent) + KS_GET(pent);     /* skip to info-part */
    if (is == 33)               /* if bit map */
    {
        pos++;                  /* skip bit count, point to bit map */
        if (op & (CXFIRST | CXLAST))
        {
            if (op & CXFIRST)
            {
                offset = 0;
                bit = -1;
                goto cx_next;
            }
            offset = 31;                /* CXLAST */
            bit = 8;
            goto cx_prev;
        }

        offset = tagbyte >> (TEXT)3;    /* offset in bytes */
        bit = tagbyte & 7;              /* bit no. (0 - 7) in the byte */

        if (op & CXNEXT)
            goto cx_next;
        if (op & CXPREV)
            goto cx_prev;
                                        /* must be CXFIND */
        pos += offset;                  /* to the respective byte */
        if (*pos & (1 << bit))          /* check the respective bit */
            return 0;   
        return -1;                      /* bit is not on */

    cx_next:                            /* CXNEXT */
        for(pos += offset; offset < 32; offset++, pos++, bit = -1)
        {
            if (*pos & 255)             /* if any bit is on */
                while(++bit < 8)
                    if(*pos & (1 << bit))
                        return((offset << 3) + bit);
        }
        return -1;                      /* no next bit is on */

    cx_prev:                            /* CXPREV */
        for(pos += offset; offset >= 0; offset--, pos--, bit = 8)
        {
            if (*pos & 255)             /* if any bit is on */
                while(--bit >= 0)
                    if(*pos & (1 << bit))
                        return((offset << 3) + bit);
        }
        return -1;                      /* no previous bit is on */
    }
                                        /* got a BYTE LIST */
    if (op & (CXFIRST | CXLAST))
    {
        if (op & CXFIRST)
            return *pos; 
        return *(pos + is -1);          /* CXLAST */
    }

    for (i = 0; *pos < tagbyte; pos++)
        if (++i == is)
            break;

    if (op & CXNEXT)
    {
        if (i == is)
            return -1;                  /* next value does not exist */
        if (*pos == tagbyte)
        {
            if (i + 1 == is)
                return -1;              /* next value does not exist */
            pos++;                      /* to next tag */
        }
        return *pos; 
    }

    if (op & CXPREV)
    {
        if (i == 0)
            return -1;                  /* previous value does not exist */
        if (i < is)
            return *(pos - 1);
        return *pos;
    }

    if (*pos == tagbyte)                /* must be CXFIND */
        return 0;                       /* found */
    return -1;                          /* tagbyte does not exist */
}

/*****************************************************************************/
/* PROGRAM: cxToList - converts a 32 byte bit map w/32 bits on to a byte list.
 *          converts 32 bytes bit map with exactly 32 "on" bits to a byte list
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxToList(TEXT   *pos)
{
FAST    int     i;
FAST    int     j;
        TEXT    temp[33];

    bufcop(temp, pos+1, 32);
    for (i = 0; i < 32; i++)
        for (j = 0; j < 8; j++)
        {
            if (*(&temp[0] + i) & (1 << j))
                *(pos++) = (i << 3) + j;
        }
}

/********************************************************/
/* PROGRAM: cxToMap - convert the byte list to a bit map
 *
 * RETURNS: DSMVOID
 */
DSMVOID
cxToMap(TEXT    *pos)
{
FAST    int     i;
        TEXT    temp[33];

        /* convert byte list to a 32 bytes bit map preceeded by a bit count*/
        for (i = 0; i < 33; i++) 
            temp[i] = 0;

        for (i = 0; i < 33; i++) 
            cxAddBit(temp, pos[i]);     

        bufcop(pos, temp, 33);
}
