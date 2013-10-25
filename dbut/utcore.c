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

#include <stdio.h>
#include "dbconfig.h"
#include "drsig.h"

#include "utepub.h"
#include "utfpub.h"
#include "utospub.h"
#include "utmsg.h"
#include "utspub.h"

#include "utospub.h"

#if OPSYS==WIN32API
#include <string.h>         /* for memmove */
#include <process.h>
#endif

#if OPSYS==UNIX
#include <time.h>       /* for localtime, strftime */
#include <errno.h>      /* for errno */
#include <unistd.h>     /* for getpid */
#include <sys/types.h>  /* for struct stat */
#include <sys/stat.h>   /* for stat call */
#endif



#if OPSYS==UNIX
/* PROGRAM: utcore */

DSMVOID
utcore ()
{
    int i;
    int ret;
    psc_rtlchar_t newname[MAXNAM];
    psc_rtlchar_t tmpname[MAXNAM];
    struct stat stbuf;

    /* Identify whether a corfile currently exists */
    ret = stat((char *)"core", &stbuf);
    if (ret == 0)
    {
        /* formulate new core file name from ctime to be:
         * core.(day of month)(Hour)(Minute)(Second)
         */
        strftime(newname, MAXNAM, "core.%d%Hutcore.cS",
                 localtime(&(stbuf.st_ctime)));
        /* Loop up to 5 times looking for a unique name.  After this loop,
         * a non-unique name would only occur if we dumped 5 times in one
         * second to this same directory.  In such a case, the current core
         * file will end up getting overwritten.
         */
        sprintf(tmpname, "%s", newname);
        for (i=1; i <= 5; i++)
        {
            ret = stat(newname, &stbuf);
            if (ret != 0) /* name is unique - rename core file */
            {
                ret = rename((char *)"core", newname);
                /* bad rtc may be due to lack of write access to directory
                 * in any case we either renamed it or we'll overwrite it
                 */
                break;
            }
            else  /* new name not unique, try again */ 
                sprintf(newname, "%s.%1d", tmpname, i);
         }
    }

    utsetuid(); /* in case we are super-user, downgrade */
    
    uttrace();

    signal(SIGQUIT, SIG_DFL);
    kill(getpid(), SIGQUIT);

}  /* end utcore (UNIX) */

#endif


#if OPSYS==WIN32API 
void uttraceback(void);

DSMVOID
utcore ()
{
    DWORD wkExcp = 0xe0000003;
    DWORD wkFlags = 0;
    DWORD wkArgs1 = 0;
    const DWORD *wkArgs2 = 0;

    uttraceback();  /* NOOP function on windows platform,
                          * included here to force the C++ exit handler 
                          * to be included  */
    RaiseException( wkExcp, wkFlags, wkArgs1, wkArgs2 );
        return;
}  /* end utcore (WIN32API) */

#endif


#if OPSYS==UNIX

/* PROGRAM: uttrace - print a stack trace to the specified file
 *
 * RETURNS: DSMVOID
 */
#define UT_TRACE_HEADING (TEXT *)"\n\nGEMINI stack trace as of "
DSMVOID
uttrace()
{
    int          fd;                /* file descriptor to print on */
    int          ret;
    LONG         clock;
    TEXT        *pdate;
    int          syserror;
    TEXT         fileName[MAXNAM];

    /* get a unique name by using the pid to generate a filename */
    stnclr(fileName, sizeof(fileName));
    sprintf((psc_rtlchar_t *)fileName, "%s.%d","gemtrace",getpid());

    fd = open(fileName, CREATE_RW_APPEND, DEFLT_PERM);
        
    if (fd < 0)
    {
        /* if the open failed, put a message out to the screen
         * and we will dump the stack to the screen 
         */
        printf("Failed to open file %s errno %d", fileName, syserror);
        fd = 1;
    }
   

    /* we should have a valid fileHandle, even though it might be stdout.
     * Time to call the routine to print the stack trace
     */
    ret = write(fd, UT_TRACE_HEADING,stlen(UT_TRACE_HEADING));

    if (ret != stlen(UT_TRACE_HEADING))
    {
        /* write failed, revert to stdout */
        fd = 1;
    }

    /* get the date and time so we can put it into the file */
    time(&clock);
    pdate = (TEXT *)ctime(&clock);

    ret = write(fd, pdate, stlen(pdate));

    uttraceback(fd);

    if (fd != 1)
    {
        /* cleanup */
        close(fd);
    }

}  /* end uttrace */

#endif  /* OPSYS == UNIX */
