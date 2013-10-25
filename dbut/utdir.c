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

/* Table of Contents - directory utility functions:
 *
 * utIsDir - Tests if a given entry is a directory.
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
#include "utdpub.h"

#if OPSYS == UNIX
#include <sys/stat.h>
#endif

/* PROGRAM: utIsDir - Tests if a given entry is a directory.
 *
 * RETURNS: int
 */
int
utIsDir(TEXT *pname)
{
#if OPSYS == UNIX
    struct stat statbuf, *pstat;
    pstat = &statbuf;
    if (stat(pname, pstat))
      return 0;
    if (S_ISDIR(pstat->st_mode))
      return 1;
#elif OPSYS==WIN32API
    DWORD dwAttr;
 
    dwAttr = GetFileAttributes(pname);
 
    if (dwAttr == (DWORD)-1)
        return (0);
 
    if (dwAttr & FILE_ATTRIBUTE_DIRECTORY)
        return (1);
#endif /* OPSYS == UNIX */
    return 0;
}/* end utIsDir */
