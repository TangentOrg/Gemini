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

#ifndef PSCCLANG_H
#define PSCCLANG_H 1

/*
*  C/C++ language and compiler defines.
*/

#include "pscos.h"

#ifndef _CRTIMP
#ifdef  _DLL
#define _CRTIMP __declspec(dllimport)
#else   /* ndef _DLL */
#define _CRTIMP
#endif  /* _DLL */
#endif  /* _CRTIMP */

#if defined(_DLL) && (_M_IX86)
#define __argc      (*__p___argc())     /* count of cmd line args */
#define __argv      (*__p___argv())     /* pointer to table of cmd line args */

_CRTIMP int *          __cdecl __p___argc(void);
_CRTIMP char ***       __cdecl __p___argv(void);
#elif defined(_M_ALPHA)
_CRTIMP extern int __argc;          /* count of cmd line args */
_CRTIMP extern char ** __argv;      /* pointer to table of cmd line args */
#endif

#ifndef PSC_ANSI_LEVEL 
#if __STDC__ || __cplusplus || _MSC_VER || __STRICT_ANSI__
#define PSC_ANSI_LEVEL 1
#else
#define PSC_ANSI_LEVEL 0
#endif
#endif

#ifndef PSC_ANSI_COMPILER

#if __STDC__ || __cplusplus || _MSC_VER || __STRICT_ANSI__
#define PSC_ANSI_COMPILER 1
#elif defined(__STDC__) || defined(__cplusplus) || defined(_MSC_VER) || defined(__STRICT_ANSI__)
#define PSC_ANSI_COMPILER 1
#else
#define PSC_ANSI_COMPILER 0
#endif


#endif

#define FARGS(a)	a

#ifndef PSC_DEPRECATED
#define PSC_DEPRECATED 1
#endif

#ifndef PSC_STDIO
#if PSC_ANSI_LEVEL >= 2
#define PSC_STDIO 1
#else
#define PSC_STDIO 0
#endif
#endif

#if SOLARIS
#define _REENTRANT 1
#endif

#endif /* PSCCLANG_H */
