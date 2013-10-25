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

/* Table of Contents - string manipulation utility functions:
 *
 *  stnclr    - Clears n bytes of storage at *pt
 *  stncop    - Copies at most n chars from s to t.
 *  stpcmp    - Compares 2 potentially different length strings.
 *  stpcmp    - Compares 2 potentially different length strings,
                case insensitive comparison
 *  stnmov    - Copies at most n characters from string s to string t
 *  scan      - Locates a character in a string and returns its location
 *  scann     - Locates a char in a buffer and returns its location
 *  term      - Locates the null that terminates a string
 *  caps      - Converts lower case chars to caps.
 *  capscop   - Copy characters with capitalization.
 *  stlen     - Returns len of null term string s, not counting term.
 *  p_stcopy  - copies string s to string t until null term moved.
 *  stmove    - copies string s to string t but doesnt move the null terminator
 *  firstByte - determine if pchar is the first byte of a character.
 *  stbcmp    - compares 2 potentially different length strings.
 *  stcomp    - compares 2 potentially different length strings.
 *  stncmp    - compares 2 strings and stops at unequal chars,
 *              nul term, or n, whichever comes first.
 *  utend     - given a password, encodes it to a 16-char printable str
 *  chsidx    - find index of delimiter in a string
 *  ststar    - matches string against pattern (*,.)
 *  stidx     - finds index of pattern in string
 *  rawbcmp   - compares 2 potentially different length byte sequences.
 *  IS_ALPHA  - determines if the value is a character, rather than
 *              a number, etc
 *
 */

/*
 * NOTE: This source MUST NOT have any external dependancies.
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include <stdio.h>
#include "dbconfig.h"
#include "utspub.h"
#include "utbpub.h"


#if OPSYS == UNIX
#include <ctype.h>
#endif

/* PROGRAM: stnclr - clears n bytes of storage at *pt
 *
 * RETURNS: DSMVOID
 */
DSMVOID 
stnclr (DSMVOID  *t,
        int    len)
{
#if OPSYS==WIN32API

    memset ( (char *) t, '\0', len );
}
#endif
#if OPSYS==UNIX
        TEXT    *end;
        TEXT    *p;

/* for small moves, its most efficient to just zero and leave.
   for big moves the overhead of breaking into LONG chunks
   is worthwhile. And this fixes bug 95-05-04-001, where for
   1 byte clears, the LONG-chunk algorithm clears two bytes.
   tex. */

    p = (TEXT *)t;

    if (len < 20 )
    {
        while (--len >= 0 )       /* zero len bytes */
            *p++ = 0;
        return;  /* go home */
    }


    /* clear bytes up to a LONG boundary */
    while(   (int)p & (sizeof(LONG)-1)
          && --len >= 0)
    {   *p++ = 0;  }

    end = p + (len & ~(sizeof(LONG)-1) );  /* find the end of longs to zero*/
    len = len & (sizeof(LONG)-1);          /* remember what's left to do */

    /* clear all possible LONGs */
    while( p < end )
    {   *(LONG *)p = 0;
        p += sizeof(LONG);
    }

    /* clear any remaining odd bytes */
    while(--len >= 0) {*p++ = 0;}
}
#endif


/* PROGRAM: stncop  - copies at most n chars from s to t.
 *          Example: m=stncop(t,s,m); puts null char in t[m]
 *
 * RETURNS: num chars copie not counting null term char which it puts in t.
 */
int 
stncop (TEXT   *t,
        TEXT   *s,
        int     n)
{
     int     i;
     TEXT    *tbegin = t;

    if (n == 0) { *t = '\0'; return 0; }
    i = n;
    while ((*t++ = *s++))
    {
        if (--i == 0)
        {
            *t = '\0';
            break;
        }
    }
        if (McIS_LB(*(--t)) && firstByte(t, tbegin) )
        {
                *t = '\0';
                i++;
        }
    return n - i;
}

/* PROGRAM: stpcmp compares 2 potentially different length strings, assuming
 * the strings are delimited with null characters (\0). A case
 * insensitive compare is used which distinguishes partial match
 * from low, high and equal.
 *
 * RETURNS: 0 => t = s exactly
 *          1 => s > t
 *          2 => s < t
 *          3 => partial match, len(s) < len(t) && s = substr(t, len(s))
 */
int 
stpcmp (TEXT *t,
        TEXT *s)
{
                UCOUNT   c1;
                UCOUNT   c2;

    for (;;)	/* loop through both strings */
    {
	McNextUpper(t,c1);     /*Get next either upper or DBCS*/
	McNextUpper(s,c2);
	if (c1 == c2)	/* if current characters match */
	{
	    if (c1 == 0) return 0; /* and are null, equality */
	}
	else			/* otherwise check for unequal length */
	{
	    if (c1 == 0)	/* if t shorter than s */
		return 1;	/* then t is low */
	    else if (c2 == 0) /* if s shorter than t */
		return 3;	/* then partial match of s in t */
	    else	/* strings not terminated yet */
		return c1 > c2 ? 2 : 1; /* use normal rules */
	}
    }
}


/* PROGRAM: stnmov - copies at most n characters from string s to string t,
 *   but doesn't move the null terminator.
 *
 * RETURNS: DSMVOID
 */
DSMVOID 
stnmov (
     TEXT	*t,
     TEXT	*s,
     int	 n)
{
    TEXT        *tbegin = t;

    if (n == 0)  return;
    while (n-- && *s)
	*t++ = *s++;
	/* Clear the last first byte (leadbyte) */
	if ( McIS_LB(*(--t)) && firstByte(t, tbegin) )
		*t = '\0';
	return;
}

/* TODO: Note that the following two function scan scann HAVE external */
/* dependencies and need to have the external code removed.            */

/* PROGRAM: scan - locates a character in a string and returns its location
 *          scan is double byte enabled 11/26/94.  See also scann below
 *
 * RETURNS: location of character in a string
 */
TEXT * 
scan (TEXT	*ps,  /* ptr to the string */
      TEXT	 c)	  /* the character to be located */
{
    if (McUseDBCS(c))                /* Test if c requires a DBCS algorithm */
      while( *ps )                   /* Use the DBCS approach */
      {
	if (McIS_LB(*ps) )
	{   ps += 2;                 /* skip over DBC, it can't match c */
	}
	else
	{
	    if (*ps == c) return ps; /* Test whether the SBC is c*/
	    ps++;
	}
      }
    else              /* if ( McIS_LB(*ps)) */
      while(*ps)
	if (*ps++ == c) return --ps;
    return NULL;
}

/* PROGRAM: scann locates a char in a buffer and returns its location */
TEXT *
scann( TEXT    *ps,    /* ptr to the buffer       */
       TEXT     c,     /* character to be located */
       int      n)     /* buffer length           */
{
    n++;
    if (McUseDBCS(c))             /* Decide whether DBCS algorithm needed. */
      while(n > 0)                /* It is. This is the DBCS way... */
      {
        if (McIS_LB(*ps) )
        {   ps += 2;              /* skip past DBC */
            n -= 2;
        }
             else
        {
            if (*ps++ == c) return --ps;
            n--;
        }
      }
    else
      while(--n != 0)
        if (*ps++ == c) return --ps;
    return NULL;
}


/* PROGRAM: term - locates the null that terminates a string
 *
 * RETURNS: TEXT *
 */
TEXT * 
term (TEXT	*ps)	/* ptr to the string */
{
    while (*ps++);
    return --ps;
}


/* PROGRAM: caps - converts lower case chars to caps
 * Tex changed to return string byte length instead of being void. 10/14/94
 * The length returned is without the null, same as stlen returns.
 *
 * RETURNS: length
 */
int 
caps (TEXT	*ps)	/* null term string */
{
    TEXT	*pstart = ps;  /* save start position */

    while (*ps)
    {
      if (McIS_LB(*ps) )
          ps++;
      else
          *ps = toupper(*ps);
      ps++;
    }
return ( (int) (ps-pstart) );
}


#if 0
/* PROGRAM: capscop copy characters with capitalization
 *
 * RETURNS: string length, same as caps.
 */
int 
capscop (TEXT	*t,
         TEXT	*s)
{
    TEXT	*pstart = t;
#if DBCS
    while (*s)
    {
        if (McIS_LB(*s) )
        {                       /* No caps for dbcs */
            *t++ = *s++;
            *t++ = *s++;
        }
        else
            *t++ = rupper(*s++);
    }
#else
    while (*s)
    {
        *t++ = rupper(*s++);
    }
#endif /* DBCS */
    *t = '\0';  /* terminate t */
    return ((int) (t-pstart));   /* return string length, like stlen */
}
#endif /* #if 0 */

/* PROGRAM: stlen -- Returns len of null term string s, not counting term
 *
 * RETURNS: len of null term string s
 */
int 
stlen (TEXT	*s)
{
     TEXT	*p;
    p = s;
    while (*p++);
    return (int)( --p - s);
}

/* PROGRAM: p_stcopy copies string s to string t until null term moved
 *
 * RETURNS: TEXT *
 */
TEXT * 
p_stcopy (TEXT	*t,
          TEXT   *s)
{
    while ((*t++ = *s++));
    return t-1;
}

/* PROGRAM: stmove  - copies string s to string t
 *                    but doesnt move the null terminator
 *
 * RETURNS: TEXT * 
 */
TEXT * 
stmove (TEXT	*t,
        TEXT   *s)
{
    while (*s) *t++ = *s++;
    return t;
}


/**** firstByte, IS_LB, and IS_TRAIL provide functions for double
      byte chars. */

/* PROGRAM: firstByte - determine if pchar is the first byte of a character.
 *
 * pstart points to the beginning of a string and pchar points
 * to a byte in that string.  This function determines whether
 * the byte pointed to by pchar is the first byte of a character.
 *
 * RETURNS: int
 */ 
int 
firstByte(TEXT	*pchar _UNUSED_,
          TEXT *pstart _UNUSED_)
{
#if DBCS
    UCOUNT k;          /* distance back from pchar */
    UCOUNT maxback;    /* max distance to go backwards */
    UCOUNT s;          /* distance from pstart, same as maxback-k */

    if (pstart == pchar)
	return 1; /* if the char is the start of the string, true */

    maxback = pchar - pstart;  /* Dont go before the beginning */

    /* Otherwise, start scanning backward until we reach a terminating
       byte.  If we have moved back over an even number of bytes, then
       the byte we were pointing to was the second byte of a DBC.
       If we have moved back over an odd number of bytes, then the byte
       we were pointing to is either an SBC or the first byte of a DBC. */

/* pointer arithmetic bug on windows- 95-01-19-035 fixed 1/19/95 tex
   so I made the following translations: s=maxback-k; k=maxback-s;
**  for (k = 1; (pchar - k) >= pstart && (McIS_LB(*(pchar - k))); k++)
    and changed from s>=0 to s>0 since decrementing UCOUNT 0 is 64K */

    for (s = maxback-1; (s > 0) && (McIS_LB(pstart[s])) ; s--)
	;
    if (s == 0)             /* Did we scan all the way back ?*/
    {                       /* Yes. Was first byte a leadbyte or not?*/
        if (McIS_LB(*pstart))
            k = maxback - 1;    /* Yes, then byte 0 was first Leadbyte */
                                /* and add or subtract 1 to fix parity */
        else
            k = maxback ;       /* No, so k = maxback - 0 */
    }
    else
        k = maxback - s;

    if (k & 1)               /* if k is odd */
	return 1;            /* pchar is a first byte */
    else
	return 0;
#else
   return 1;   /* for non-DBCS character sets, always true*/
#endif
}

/* PROGRAM: sticmp
 *  Compares two potentially different length strings with case
 *  insensitivity. Assumes strings null terminated. If one string
 *  is a truncated version of other string, the shorter string
 *  will always compare low.
 *  Comparing the binary values of a character will not make it
 *  sort properly. I changed this to simply return equal (0)
 *  or not equal (!0) before anyone became dependent on it. tex
 */
int
sticmp (TEXT *pt, TEXT *ps)
{
    /* return 1 if t < s, 0 if t = s, 1 if t > s */
    UCOUNT     tN, sN;
    for (;;)
    {
        McNextUpper(pt,tN);
        McNextUpper(ps,sN);
        if (tN != sN)
            return 1;    /* just true for not equal*/
            /*  return tN > sN ? 1 : -1;   */
        if (tN == 0)
            return 0; /* both strings ran out */
    }
}

/* PROGRAM: stbcmp - compares 2 potentially different length strings.
 * assumes strings are delimited with null character (\0). standard
 * comparison rules are followed unless one string terminates before
 * the other.  Lower case is treated as equal to upper case.
 * If the longer string equals the shorter string padded with trailing
 * blanks, the strings are still considered equal.
 * Made DBE 5/18/95 in Roadrunner, 11/15/95 7.3D, 9.0a, tex
 *
 * RETURNS: int
 * return -1 if t < s, 0 if t = s, 1 if t > s
 */
int 
stbcmp (TEXT    *t,
        TEXT    *s)
{
    UCOUNT   c1;
    UCOUNT   c2;
 
    for (;;)    /* loop through both strings */
    {
        McNextUpper(t,c1);  /* Get next Upper or DBCS */
        McNextUpper(s,c2);
        if (c1 == c2)   /* if current characters match */
        {
            if (c1 == 0) return 0; /* and are null, equality */
        }
        else                    /* otherwise check for unequal length */
        {
            if (c1 == 0)        /* if t shorter than s */
            {
                s--;    /* back up s pointer once for while loop */
                while (*s++ == ' ');    /* skip trailing blanks */
                return *--s == '\0' ? 0 : -1; /* equal if all blanks */
            }
            else if (c2 == 0) /* if s shorter than t */
            {
                t--;    /* back up t pointer once for while loop */
                while (*t++ == ' ');    /* skip trailing blanks */
                return *--t == '\0' ? 0 : 1; /* equal if all blanks */
            }
            else        /* strings not terminated yet */
                return c1 > c2 ? 1 : -1;        /* use normal rules */
        }
    }
}

/* PROGRAM: stcomp - compares 2 potentially different length strings.
 *          assumes strings are delimited with null character (\0).
 *          if one string is truncated version of other string,
 *          the shorter string will always compare low.
 *
 * RETURNS: int
 * return -1 if t < s, 0 if t = s, 1 if t > s
 */
int
stcomp (TEXT    *t,
        TEXT    *s)
{
 
    for (;;)    /* loop through both strings until inequality found */
    {
        if (*t != *s++)
                return *t > *--s ? 1 : -1;
        if (*t++ == '\0') return 0; /* both strings ran out */
    }
}

/* PROGRAM: stncmp compares 2 strings and stops at unequal chars,
nul term, or n, whichever comes first.  Returns same as stcomp above. */
int
stncmp (TEXT    *t,
        TEXT    *s,
        int     n)
{
    for (;;)    /* loop through both strings until inequality found */
    {
        if (--n < 0) return 0;
        if (*t != *s++)
                return *t > *--s ? 1 : -1;
        if (*t++ == '\0') return 0; /* both strings ran out */
    }
}

/* PROGRAM: stncmpc compares 2 strings and stops at unequal chars,
   null terminator, or n, whichever comes first. CASE INSENSITIVE.
   returns -1 if t < s, 0 if t = s, 1 if t > s */
/* Both strings may contain DBCS characters */
 
int
stncmpc (
        TEXT    *t,
        TEXT    *s,
        int     n)
 
{
#if DBCS
/* MBCS_UTF8 */
    ULONG c1,c2;
    TEXT temp[4];
    TEXT bufCap[4];
    TEXT *ptr;
/* MBCS_UTF8 */
#else
    FAST                TEXT     c1;
    FAST                TEXT     c2;
#endif
 
    for (;;)    /* loop through both strings until inequality found */
    {
        if (--n < 0) return 0;
#if DBCS
        /* need MBCS_UTF8 case sensitive algorithm */
/* MBCS_UTF8 */
        /* save the coming char as wide */
        n -= McMB_LEN(*t); /* reduce n for tail-bytes */
        McNextWide(t, c1);
        /* get the c1 upper case value */
        ptr = temp; /* tmp saving c1*/
        McSaveWide(ptr,c1);
        (*ptr) = '\0';  /* terminate the buffer */
        capscopn(bufCap,temp,4); /* get the upper case */
        ptr = bufCap;
        McNextWide(ptr,c1); /* get the upper case wide value */
/* MBCS_UTF8 */
#else
        c1 = toupper(*t++); /* single-byte *.exes only */
#endif
 
#if DBCS
/* MBCS_UTF8 */
        /* save the coming char as wide */
        McNextWide(s, c2);
        /* get the c1 upper case value */
        ptr = temp; /* tmp saving c1*/
        McSaveWide(ptr,c2);
        (*ptr) = '\0';  /* terminate the buffer */
        capscopn(bufCap,temp,4); /* get the upper case */
        ptr = bufCap;
        McNextWide(ptr,c2); /* get the upper case wide value */
/* MBCS_UTF8 */
#else
        c2 = toupper(*s++); /* single-byte *.exes only */
#endif
 
        if (c1 != c2) return c1 > c2 ? 1 : -1;
#if DBCS
        if (c1 == 0 ) return 0; /* both strings ran out - MBCS_UTF8 */
#else
        if (c1 == '\0') return 0; /* both strings ran out */
#endif
    }
}


/* PROGRAM: chsidx -- find index of a delimiter in string (or 0)  */
/* Currently delim is restricted to being a single byte character */
/* However string pstr can contain DBCS and chsidx will not       */
/* mistake delim for a DBCS tailbyte. The index returned is the   */
/* Byte offset not the Character offset.                          */
 
int
chsidx(TEXT *delim, TEXT *pstr)
{
TEXT    *ps;
TEXT    *pd;
#if DBCS
    for (ps = pstr; *ps; ps++)
    {
        if (McIS_LB(*ps))
            ps++ ;  /* forloop skips other byte. Skip dbcs, SBCS delim can't mat
ch */
        else
        {   /* Test that the current SBCS is not one of the delim */
            for (pd = delim; *pd ; pd++)
                if (*ps == *pd) return ((int) (ps - pstr + 1));
        }
    }
#else
    for (ps = pstr; *ps; ps++)
        for (pd = delim; *pd ; pd++)
            if (*ps == *pd) return ((int) (ps - pstr + 1));
#endif /* DBCS */
    return 0;
}

int
ststar (TEXT *ps, TEXT *pp, GBOOL caseflag)
{
                TEXT     cs;    /* string char */
                TEXT     cp;    /* pattern char */
    for (;;)
#if DBCS   /* tex 9/6/93 */
      if (McIS_LB(*pp))           /* if pattern char is dbcs */
                                /* then check for an exact match*/
        if ( (*pp == *ps) && (*(pp+1) == *(ps+1)) )
          {  pp += 2;           /* it was exact - continue pattern matching*/
             ps += 2;
             continue;
          }
        else return 0;         /* didn't match so error back */
      else                     /* pattern is not dbcs */
#endif
        switch(*pp++)
        {
         case '.':
#if DBCS   /* tex 9/6/93 */
                       if (McIS_LB(*ps))   /* dbcs in string? */
                         ps += 2;      /* yes skip 2 bytes */
                       else            /* no skip 1 */
#endif
        if (*ps++ == '\0')      return 0;
                        continue;
 
         case '*':      if (*pp == '\0')        return 1;
 
                /* 970619063: Skip multiple, sequential wildcards
                 * in the pattern string                (dmarlowe)
                 */
                while(*pp == '*')
                        pp++;
 
                while (*ps)
#if DBCS   /* tex 9/6/93 */
                         { if (ststar(ps, pp, caseflag))
                             return 1;
                           ps +=  (McIS_LB(*ps)) ? 2 : 1;
                         }
#else
                         if (ststar(ps++, pp, caseflag))
                             return 1;
#endif
                        return ststar(ps, pp, caseflag);
 
#if OPSYS==UNIX || OPSYS==BTOS || OPSYS==MPEIX
         case '\\':
#endif
         case '~':
#if DBCS   /* tex 9/6/93 */
                      if ( McIS_LB(*pp) )
                        continue;   /* dbcs will get handled at top of loop*/
#endif
                      ++pp;
         default:       if (caseflag)   /* No upcasing if case-sensitive */
                        { cs = *ps++; cp = *--pp; }
                        else
                        { cs = toupper(*ps++);
                          cp = toupper(*--pp);
                        }
                        if (cs != cp)           return 0;
                        if (cs == '\0')         return 1;
                        ++pp;
        }
}                       /* end ststar */

int
stidx (TEXT *ps1, TEXT *ps2, GBOOL caseflag)
{
                TEXT    *ps;            /* position in string */
                int      len;           /* length of pattern */
                TEXT    *pe;            /* last pos to check */
#if DBCS                                /* dbcs support for index */
                int dbc_count = 0;      /* counter for double byte chars */
#endif
 
    ps = ps1;
    if (!caseflag)      /* No upcaseing if case-sensitive. */
    {
      if ((len = caps(ps2))  == 0) return 0;
      pe = ps + caps(ps) + 1 - len;     /* ps +term(ps) - len(ps2) */
    }
    else
    {
      if ((len = stlen(ps2)) == 0) return 0;
      pe = term(ps) - len;
    }
    while (ps <= pe)
        {
#if DBCS
        if (bufcmp(ps, ps2, len) == 0)
          {
            return (int) (++ps - ps1 - dbc_count);
          }
        /* if the character is a lead_byte than increment dbc counter & ps */
        if (McIS_LB(*ps))
          {
            dbc_count++;
            ps++;
          }
        ps++;
#else
        if (bufcmp(ps++, ps2, len) == 0)
          return (int) (ps - ps1);
#endif
        }
    return 0;
}

/* return -1 if t < s, 0 if t = s, 1 if t > s */
int
rawbcmp (TEXT *t, int lt, TEXT *s, int ls)
{
 
    for (; lt && ls; t++, s++, lt--, ls--)
    {
        if (*t <  *s)
            return -1;
        if (*t > *s )
            return 1;
    }
 
    if (lt == ls)
        return 0;
    if (lt == 0)
        return -1;
    return 1;
}




