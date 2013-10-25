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

#ifndef UTSPUB_H
#define UTSPUB_H

/* utstring.c string conversion utilities */

#if DBCS


#define McIS_LB(x) ( (GTBOOL) ccGetAttributeValue( \
  (DSMVOID *)NULL, (UCOUNT) ccAttrLead, (UCOUNT) (x), (int *)NULL ) )

#define McIS_TRAIL(x) ( (GTBOOL) ccGetAttributeValue( \
  (DSMVOID *)NULL, (UCOUNT) ccAttrTail, (UCOUNT) (x), (int *)NULL ) )

#define McNextUpper(ps,Wide)                                       \
                     {                                             \
                       if (McIS_LB(*ps))                           \
                       {                                           \
                           Wide= (UCOUNT) *ps;                     \
                           Wide = (Wide<<8) + (*++ps);             \
                           ps++;                                   \
                       }                                           \
                       else                                        \
                           Wide = toupper(*ps++);                  \
                     }

#define McUseDBCS(sbc) (McIS_TRAIL(sbc))

#else

#define McIS_LB(x) ( (GTBOOL) (0) )
#define McIS_TRAIL(x) ( (GTBOOL) (0) )
#define McNextUpper(ps,Wide) Wide = toupper(*ps++);
#define McUseDBCS(sbc) (McIS_TRAIL(sbc))

#endif

DLLEXPORT int chsidx (TEXT *delim,
                      TEXT *pstr);

DLLEXPORT DSMVOID stnclr (DSMVOID *t,
                       int  len);

DLLEXPORT int stncop (TEXT *t,
                      TEXT *s,
                      int  n);

DLLEXPORT int stpcmp (TEXT *t,
                      TEXT *s);

DLLEXPORT DSMVOID stnmov (TEXT  *t,
                       TEXT  *s,
                       int   n);

DLLEXPORT TEXT * scan (TEXT	*ps,  /* ptr to the string */
                       TEXT	 c);  /* the character to be located */

DLLEXPORT TEXT * scann (TEXT     *ps,  /* ptr to the string */
                       TEXT      c,   /* the character to be located */
                       int       n);  /* buffer length */

DLLEXPORT TEXT * term (TEXT	*ps);	/* ptr to the string */

DLLEXPORT int caps (TEXT	*ps);	/* null term string */

DLLEXPORT int capscop (TEXT	*t,
                       TEXT	*s);

DLLEXPORT int stlen (TEXT	*s);

DLLEXPORT TEXT * p_stcopy (TEXT	*t,
                           TEXT   *s);
DLLEXPORT TEXT * stmove (TEXT	*t,
                         TEXT   *s);

DLLEXPORT int firstByte(TEXT *pchar, 
                        TEXT *pstart);

DLLEXPORT int stbcmp (TEXT    *t,
                      TEXT    *s);

DLLEXPORT int stcomp (TEXT    *t,
                      TEXT    *s);
 
DLLEXPORT int stncmp (TEXT    *t,
                      TEXT    *s,
                      int     n);

DLLEXPORT int stncmpc (TEXT    *t,
                      TEXT    *s,
                      int     n);

DLLEXPORT int sticmp (TEXT    *t,
                      TEXT    *s);

/* utsysdep.c system dependent utilities */

DLLEXPORT DSMVOID slng(TEXT *pt,	/* ptr to storage area for result */
	                LONG lng);	/* long value to store */

DLLEXPORT DSMVOID slng64(TEXT *pt,
			 LONG64 lng);

DLLEXPORT LONG xlng (TEXT *pt);	/* storage area holding the LONG */

DLLEXPORT LONG64 xlng64 (TEXT *pt); 

DLLEXPORT COUNT xct (TEXT *pt);	/* ptr to storage area holding the COUNT */

DLLEXPORT DSMVOID sct (TEXT  *pt,	/* ptr to target storage area for COUNT */
	                COUNT ct);	    /* value to be stored */

DLLEXPORT int slngv(TEXT  *pt,	/* ptr to target storage area */
	                LONG  lng);	/* value to be stored */

DLLEXPORT int slngv64(TEXT    *pt,    /* ptr to target storage area */
                      LONG64   lng);  /* value to be stored */

DLLEXPORT LONG xlngv (TEXT  *pt,	/* storage area holding value */
	                  COUNT len);	/* number of bytes stored */

DLLEXPORT LONG64 xlngv64 (TEXT  *pt,	/* storage area holding value */
	                  COUNT len);	/* number of bytes stored */

DLLEXPORT void sdbl(TEXT *pt, double d);

DLLEXPORT void sflt(TEXT *pt, double d);

DLLEXPORT double xdbl(TEXT *pt);

DLLEXPORT float xflt(TEXT *pt);

DLLEXPORT int ststar(TEXT *ps, TEXT *pp, GBOOL caseflag);

DLLEXPORT int stidx(TEXT *ps1, TEXT *ps2, GBOOL caseflag);

DLLEXPORT int rawbcmp(TEXT *t, int lt, TEXT *s, int ls);

DLLEXPORT int stmatch(TEXT *pchs, TEXT *pmatch);

DLLEXPORT int IS_ALPHA(TEXT x, TEXT *pCpAttrib);

#endif /* UTSPUB_H */
