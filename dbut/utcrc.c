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

/* Table of Contents - crc encoding utility functions
 *
 * calc_crc - calculates crc-16 for a block of data
 * utend    - given a password, encodes it to a 16-char 
 *            printable str.
 *
 */

/*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "utcpub.h"
#include "utspub.h"


 UCOUNT  ctab[256] = {
         0, 49345, 49537,   320, 49921,   960,   640, 49729, 50689,  1728,
      1920, 51009,  1280, 50625, 50305,  1088, 52225,  3264,  3456, 52545,
      3840, 53185, 52865,  3648,  2560, 51905, 52097,  2880, 51457,  2496,
      2176, 51265, 55297,  6336,  6528, 55617,  6912, 56257, 55937,  6720,
      7680, 57025, 57217,  8000, 56577,  7616,  7296, 56385,  5120, 54465,
     54657,  5440, 55041,  6080,  5760, 54849, 53761,  4800,  4992, 54081,
      4352, 53697, 53377,  4160, 61441, 12480, 12672, 61761, 13056, 62401,
     62081, 12864, 13824, 63169, 63361, 14144, 62721, 13760, 13440, 62529,
     15360, 64705, 64897, 15680, 65281, 16320, 16000, 65089, 64001, 15040,
     15232, 64321, 14592, 63937, 63617, 14400, 10240, 59585, 59777, 10560,
     60161, 11200, 10880, 59969, 60929, 11968, 12160, 61249, 11520, 60865,
     60545, 11328, 58369,  9408,  9600, 58689,  9984, 59329, 59009,  9792,
      8704, 58049, 58241,  9024, 57601,  8640,  8320, 57409, 40961, 24768,
     24960, 41281, 25344, 41921, 41601, 25152, 26112, 42689, 42881, 26432,
     42241, 26048, 25728, 42049, 27648, 44225, 44417, 27968, 44801, 28608,
     28288, 44609, 43521, 27328, 27520, 43841, 26880, 43457, 43137, 26688,
     30720, 47297, 47489, 31040, 47873, 31680, 31360, 47681, 48641, 32448,
     32640, 48961, 32000, 48577, 48257, 31808, 46081, 29888, 30080, 46401,
     30464, 47041, 46721, 30272, 29184, 45761, 45953, 29504, 45313, 29120,
     28800, 45121, 20480, 37057, 37249, 20800, 37633, 21440, 21120, 37441,
     38401, 22208, 22400, 38721, 21760, 38337, 38017, 21568, 39937, 23744,
     23936, 40257, 24320, 40897, 40577, 24128, 23040, 39617, 39809, 23360,
     39169, 22976, 22656, 38977, 34817, 18624, 18816, 35137, 19200, 35777,
     35457, 19008, 19968, 36545, 36737, 20288, 36097, 19904, 19584, 35905,
     17408, 33985, 34177, 17728, 34561, 18368, 18048, 34369, 33281, 17088,
     17280, 33601, 16640, 33217, 32897, 16448};


/* PROGRAM: calc_crc - calculates crc-16 for a block of data
 *
 * RETURNS: UCOUNT
 */
UCOUNT
calc_crc (UCOUNT  crc,    /* seed crc from prior blocks */
          TEXT    *pdata, /* ptr to block of data to be crc'd */
          COUNT   len)    /* length of data block */
{
FAST    UCOUNT  crc8;  /* last 8 bits of old crc */
 
    while (--len >= 0)  /* loop over each byte in buffer */
    {
        crc8 = crc;
        crc8 &= 255;
        crc  >>= 8;
        crc &= 255;
        crc ^= ctab[crc8];
        crc ^= ctab[*pdata++];
    }
    return crc;
}

/* PROGRAM: utend - given a password, encodes it to a 16-char printable str
 * fills in last parm  with the string
 *
 * RETURNS:  0
 */
TEXT *
utend(TEXT    *pswd,  /*password to be encoded*/
      int      lenp,  /*len of pswd*/
      TEXT    *putendbuf)  /*must be at least 17 bytes!!!*/
{
        TEXT     buf[16]; /*where 16-chars are encoded*/
        UCOUNT   crc = 17;
        UCOUNT  *pcount;
        TEXT     byten;
        TEXT    *pch;
        TEXT    *pch1;
        int      i;
        int      len;
 
    stnclr(buf,16);
 
 
    for(i = 0; i < 5; i++)
    {
        pch = buf;
        pch1 = pswd;
        len = lenp;
        while(len--)
        {
            if (pch == buf + 16) pch = buf;  /*if more than 16, wrap*/
            *pch++ ^= *pch1++;   /*x-or*/
        }
 
        /*put calc-crc over each half-word section of 16 chars*/
        for (pcount = (UCOUNT *)buf + 7;
                    pcount >= (UCOUNT *)buf; pcount--)
        {
            crc = calc_crc(crc,buf,(COUNT)16);
            sct((TEXT*)pcount, crc);  /*overlay byte-by-byte onto halfword*/
        }
    }
 
    /*convert 16 bytes to be within the printing char rnge*/
    pch1 = (TEXT *)putendbuf;
    for(pch = buf + 15; pch >= buf; pch--)
    {
       byten = *pch & 0x7f;
       if ((byten > 64 && byten < 91) || (byten > 96 && byten < 123) )
           *pch1++ = byten;
       else *pch1++ =  'a' + (*pch >> 4);
    }
    *pch1 = '\0';
 
    return putendbuf;
}
