
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

/* Table of Contents - file name manipulation utility functions:
 *
 *  utfchk_local     - does just what utfchk() does except it's passed the stat
 *                     buffer.
 *  utfchk           - check if a named file exists, is a regular file, and is 
 *                     accessible .
 *  utfabspath       - test if a path name is fully qualified
 *  utfComPressPath  - compress out ALL . and .. path elements
 *  utfComPressSlash - remove extra slash on the end.
 *  utfComPressPair  - decide if element and parent can be compressed
 *  utfChgSlash      - scan its parameter and xlate all / to \
 *  utfstrip         - strips suffices out of file names.
 *  utfsufix         - adds suffix to name.
 *  utfpsep          - decide whether to add dir separator.
 *  utfpcat          - add the name to the directory buffer
 *  utflen           - determine the length of an indicated file.
 *  utfcmp           - Compare filename componets.
 *  utfpath          - removes the last segment of the path passed
 *  utcmpsuf         - checks if name has a suffix
 *  utfcomp          - return pointer to start of final component of 
 *                     specified path name
 *  utfcomplen       - measures length of file basename
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
#include "utdpub.h"       /* for utIsDir() */
#include "utfpub.h"
#include "utspub.h"       /* for stlen, stbcmp */
#include "utmsg.h"


#if OPSYS==WIN32API || OPSYS == UNIX
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#if OPSYS == WIN32API
#include <io.h>
#endif

#if OPSYS==UNIX
#include <unistd.h>  /* for access */
#endif

#if OPSYS==VMS
#include <errno.h>
#include <types.h>
#include <stat.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <rms.h>
#include <libdef.h>
#include <file.h>
#endif
 
INTERN int utfchk_local(TEXT    *name,
                        int      mode,
                        struct   stat *pstatbuf);

/* PROGRAM: utfchk_local - does just what utfchk() does except it's 
 *                         passed the stat buffer. created in order 
 *                         to implement utfchk_modtime()
 */
LOCALF int
utfchk_local(TEXT    *name,
             int      mode,
             struct   stat *pstatbuf)
{
        
#if OPSYS==VMS
        int     m;      /* mode to open file in */
        int     f;
        errno = 0;
#endif

#if OPSYS==WIN32API
    /* check to see if filename is a "special dos directory i.e. a: a:. */
    if (( strlen (name ) == 2 && name[1] == ':' ) ||
        ( name[0] == '.' && name[1] == 0))  /* treat "." as directory */
    {
            return utMSG009;
    }

    if ( stat( name, pstatbuf ) ) 
    {
        return utMSG001;
    }

    if (utIsDir(name)) 
    {
       return utMSG009;
    }

    return 0;
#else
    /* access is a unix routine to check both existence and permssion */
    /* errno is an external variable which is set after error happens*/
    if (access((char *)name, mode))
    {
#if OPSYS==VMS
        /* on VMS, access seems to only check the protection mask for
           the file (if it exists). It is possible to have access to a
           file through other measn, even if the protection mask denies
           access. An ACL entry might grant you access. So might having
           BYPASS or SYSPRV, etc. In these cases, access() seems to return
           -1 and errno == 0. Therefore we must do a further check to see
           if we can open the file. If the open succeeds, then we have
           access to the file. ghb */

        if (errno == 0)
        {
            m = O_RDONLY;
            if (mode == UTFWRITE) m = O_RDWR;
            f = utopen ((char *)name, m, 0, OPEN_SEQL);
            if (f != -1)
            {
                /* file was successfully opened, so we must have access */

                close (f);
            }
            else
            {
                /* could not open file, so we do not have access */
            }
        }
        /*  allow decnet node - access returns %x0000ffff: */
        /*  check vaxc$errno - value should be SS$_IVDEVNAM  : BB 9/14/89 */

        if ( vaxc$errno == SS$_IVDEVNAM) /* invalid device name */
            { 
                    m = O_RDONLY;
                    if (mode == UTFWRITE) m = O_RDWR;
                    f = utopen((char *)name, m, 0, OPEN_SEQL);
                    if (f != -1)
                    {
                    /* file was successfully opened, so we must have access */
                       close (f);
                        return 0;
                     }
                     else
                     {
                        /* could not open file, so we do not have access */
                     }
                }
#endif /* OPSYS == VMS */
        switch (errno)
        {
         case 0:        break;

         case ENOENT:   return utMSG001;
         case EACCES:   return utMSG002;
         default:
#if OPSYS==VMS
        if (vaxc$errno == RMS$_SYN) /* bad file name syntax, return     */
                                    /* file not found message           */
                        return utMSG001;
#endif /* OPSYS == VMS */
                        return utMSG003;
        }
    }

    stat ( (char *)name, pstatbuf );
    if ( (pstatbuf->st_mode & S_IFMT) == S_IFDIR) 
    {
        return utMSG009;
    }
    return 0;
#endif /* OPSYS == WIN32API */
}

/******************************************************************/
/* PROGRAM: utfchk - check if a named file exists,
 *                   is a regular file, and is accessible.
 *
 * RETURNS: int
 */
int
utfchk(TEXT    *name,
       int      mode)
{
    struct  stat     statbuf;

    return utfchk_local(name, mode,&statbuf);
}


/* PROGRAM: utfabspath - test if a path name is fully qualified
 *
 * RETURNS: int
 */
int
utfabspath (TEXT *pathname)      /* ptr to path name */
{
    switch (*pathname++)        /* first character */
    {

#if OPSYS==WIN32API
     case '\\':
#endif
     case '/':  return 1;
#if OPSYS==VMS
     case '[':  if (*pathname != '.') return 1;      /* relative dirs */
#endif
     case '.':  switch (*pathname++)    /* second character */
                {
#if OPSYS==WIN32API
                 case '\\':
#endif
                 case '\0':
                 case '/':   return 1;
                 case '.':   switch(*pathname)  /* third character */
                             {
#if OPSYS==WIN32API
                             case '\\':
#endif
                             case '/':
                             case '\0': return 1;
                             }
                }
                break;
#if OPSYS==WIN32API
     default:   if (*pathname == ':') return 1;
#endif
/* under VMS, if a colon is included in the filename, then we assume
* that this is an absolute pathname, ie no prepending of the value
* of propath is needed. this allows both Unix and VMS style path names
* to be used
*/
#if OPSYS==VMS
     default:   while (*pathname)
                        if (*pathname++ == ':') return 1;
#endif
    }
    return 0;
}

#if OPSYS == UNIX || OPSYS==WIN32API

/* PROGRAM: utfComPressPath - compress out ALL . and .. path elements
 *
 * RETURNS: number of characters used
 */
int
utfCompressPath(TEXT *pfullpath)
{
#if OPSYS==WIN32API
    /* make all those ugly slashes look the same */
    utfChgSlash (pfullpath);
    /* skip drive specification */
    if (isalpha(pfullpath[0]) && (pfullpath[1] ==':'))
    {
        utfCompressPair(pfullpath+2, PNULL);
        return (utfCompressSlash(pfullpath+2)+2);
    }
    /* allowing UNC name for OS2 & WIN32API */
    else
        if ((pfullpath[0] == '\\') && (pfullpath[1] =='\\'))
           return (utfCompressSlash(pfullpath));

 
    
#endif /* MS_DOS, OS2, WIN32API */
 
    {
           utfCompressPair(pfullpath, PNULL);
           return (utfCompressSlash(pfullpath));
    }
 
} /* utfCompressPath */

 /* PROGRAM: utfComPressSlash - remove extra slash on the end.
 *
 * RETURNS: number of characters used
 */
int
utfCompressSlash(TEXT *pfullpath)
{
        int     len = stlen(pfullpath);
 
        if ((len > 1) && (pfullpath[len-1] == DIRSEP))
                pfullpath[--len] = '\0';
 
        return (len);
} /* utfCompressSlash */


/* PROGRAM: utfComPressPair - decide if element and parent can be compressed
 *
 * RETURNS: TRUE if compression was done, else FALSE.
 */
int
utfCompressPair(TEXT *ps,
                TEXT *parent)
{
        FAST    TEXT    *child          = ps;
                int     len             = stlen(ps);
 
        if (len == 0)
                return(0);
 
        /* compress double slashes */
        if ((parent) && (ps[0] == DIRSEP))
        {
                bufcop(ps, ps+1, len);
                return(utfCompressPair(ps, parent));
        } /* endif "//" */
 
        /* compress dots */
        if ((parent) && (ps[0] == '.'))
        {
                /* check for a . filename which can be eliminated */
                if ((ps[1] == DIRSEP) ||
                    (ps[1] == '\0'))
                {
                        int   used = (ps[1] ? 2: 1);
 
                        bufcop(ps, ps+used, len-used+1);
                        return(1);
                } /* endif "./" */
 
                /* check for a .. filename which eliminates the parent also */
                if ((parent[0] != '.') && (ps[1] == '.') &&
                    ((ps[2] == DIRSEP) ||
#if OPSYS==MS_DOS || OPSYS==OS2 || OPSYS==WIN32API
                     (ps[2] == '.') || /* for DOS ... support below */
#endif
                     (ps[2] == '\0')))
                {
                        int   used = (ps[2] ? 3 : 2);
 
#if OPSYS==MS_DOS || OPSYS==OS2 || OPSYS==WIN32API
                        /* DOS abreviates ..\.. using ... */
                        if (ps[2] == '.')
                                used = 1;
#endif
                        /* never eliminate root as a parent */
                        if (parent[0] == DIRSEP)
                                bufcop(ps, ps+used, len-used+1);
                        else
                                bufcop(parent, ps+used, len-used+1);
                        return(1);
                } /* endif "../" */
        }
 
        /* move on to next name */
        while((*child) && (*child++ != DIRSEP));
 
        /* if I can compress the child, try again */
        if (*child)
                if (utfCompressPair(child, ps))
                        return(utfCompressPair(ps, parent));
 
        return (0);
 
} /* utfCompressPair */
#if OPSYS==WIN32API

/* PROGRAM: utfChgSlash - scan its parameter and xlate all / to \
 *
 * RETURNS: DSMVOID
 */
DSMVOID
utfChgSlash(TEXT *pname)
{
    while(*pname)
    {   if(*pname == '/') *pname = '\\';
        pname++;
    }
}
#endif  /* OPSYS == WIN32API */
#endif  /* OPSYS == UNIX || OPSYS==WIN32API */

/* PROGRAM: utfstrip - strips suffices out of file names.
 *          Operates in place by searching for suffix and copying remainder
 *          of name over it.  If there is no suffix does nothing.
 *          If there is a different suffix  and were on DOS which cares
 *          about the suffix it returns PNULL
 *
 * RETURNS: TEXT *
 */
TEXT *
utfstrip (TEXT *ptr,
          TEXT *psuffix)
{
TEXT *tmp;

#if OPSYS==WIN32API
for ( tmp = term(ptr); tmp > ptr  && *tmp != '.'; tmp-- )
  ;
if ( *tmp == '.' )
  if ( stbcmp( tmp, psuffix ) == 0 )
    *tmp = '\0';
  else
    ptr = PNULL;
#else

int   sufflen;

tmp = term( ptr );
sufflen = stlen( psuffix );
if ( tmp >= ptr+sufflen)
  {
     tmp -= sufflen;
     if (stbcmp(tmp, psuffix) == 0)
                    *tmp = '\0';
  }
#endif
return ptr;
}

/* PROGRAM: utfsufix - adds suffix to name.
 *          It works in place and it's the callers responsibility
 *          to see that pname points to storage large enough for the
 *          combined lengths off pname and psuffix.
 *
 * RETURNS: If successful a pname 
 *          otherwise it returns PNULL
 */
TEXT *
utfsufix (TEXT *pname,
          TEXT *psuffix)
{
        int      namelen = stlen(pname);
        TEXT    *p;
    int     len = stlen ( psuffix );
#if OPSYS==WIN32API
    int     lib_marker_seen = 0; /* Number of '<' characters seen */
#endif    
    /* if the name already ends with the correct suffix, return itself */
    if (( namelen >= len )
        && stbcmp(pname+namelen-len, psuffix) == 0)
            return pname;

    /* if the name ends with an incorrect suffix, error message */
    for(p=pname+namelen-1; p>=pname; p--)
    {   
#if OPSYS==WIN32API
        if (*p == '<')
            ++lib_marker_seen; /* Track '<' characters in case this is a library */
        if (*p == '\\' || *p == '/') break; /* no suffix in basename, is OK */
#else
        if (*p == '/') break;               /* no suffix in basename, is OK */
#endif

#if OPSYS==VMS
        if ((*p == ']') || (*p == '>')) /* found dir delim., so basename */
                        break;          /* must be ok                    */
#endif
#if OPSYS==WIN32API
        if (*p == '.' && lib_marker_seen < 2) /* name has bad suffix, error */
        {   /*%s is not a valid %s filen*/
            return PNULL;
        }
#endif
    }

    /* copy the name and add the suffix and return a pointer to it */
    stcopy( term( pname ), psuffix );
    return pname;
}

/* PROGRAM: utfpsep - decide whether to add dir separator
 *          given a buffer with a directory, decide whether to
 *          add dir separator and if necessary do so
 *          (caller must allow extra space)
 *          pdirbuf must be null terminated and utfpsep leaves it so also
 *
 * RETURNS:  0 if no change was made
 *           1 if separator was added
 */
int
utfpsep(TEXT *pdirbuf)
{
#if OPSYS==VMS
        int      unixpathname=0;
#endif
    TEXT    *pbuf;
        int      rc=0;
    pbuf = term(pdirbuf);
#if OPSYS==VMS
    if ( scan (pdirbuf, '/') != NULL)
        unixpathname = 1;
#endif
    /* if a directory was added, provide / separator */
    if (   pbuf != pdirbuf
        && *(pbuf - 1) != '/'
#if OPSYS==WIN32API
        && *(pbuf - 1) != '\\'
        && *(pbuf - 1) != ':'
#endif
#if OPSYS==VMS
            && (unixpathname)
#endif
           )
          {  *pbuf++ = DIRSEP; *pbuf = '\0'; rc = 1;   }
#if OPSYS==VMS
    if (!(unixpathname))
    {    /* determine if dir  entry is a logical*/
        if ((scan(pdirbuf,'[') == NULL) && (pbuf != pdirbuf))
        {   *pbuf++ = LOGICALSEP; *pbuf = '\0'; rc = 1; }
    }
#endif
    return rc;
}


/* PROGRAM: utfpcat - add the name to the directory buffer
 *          given a buffer with a directory that already has its
 *          path separator, and another buffer with a name, add the name
 *          to the directory buffer (caller must insure both names fit
 *          in first buffer)
 *
 * RETURNS: DSMVOID
 */
DSMVOID
utfpcat(TEXT    *pdirbuf,
        TEXT    *pname)
{
    TEXT    *pbuf;
    pbuf = term(pdirbuf);
#if OPSYS==VMS
    if (scan(pdirbuf, '/') == NULL )     /* vms pathname */
    {
        if (*(pbuf - 1) == ']' && (*pname == '[') && (*(pname + 1) == '.'))
        {
            --pbuf;
            pname++;      /* [d1][.d2]f1 = [d1.d2]f1  */
        }
        if ( *(pbuf - 1) == ']' && (*(pbuf - 2) == '.') && 
             *pname == '['   && *(pname + 1) == '.')
        {
            pbuf -= 2;
            pname++;      /* [d1.][.d2]f1 = [d1.d2]f1  */
        }
    }
#endif

    stcopy(pbuf, pname);

#if OPSYS==WIN32API
    utfChgSlash(pdirbuf);   /* translate / to \ */
#endif /* OPSYS==WIN32API */

}

/*PROGRAM: utflen - determine the length of an indicated file.
 *
 *      For UNIX systems uses the stat call.
 *
 * RETURNS:    status
 *               0       file exists
 *               -1      Error - file either does not exist or
 *                       can not be stat'ed.
 */
int
utflen(TEXT *pname, gem_off_t *utfilesize)
{
#if OPSYS==WIN32API && defined(DBUT_LARGEFILE)
    struct _stati64 statbuf;
#else
    struct stat statbuf;
#endif
    int retcode;
 
#if OPSYS==WIN32API && defined(DBUT_LARGEFILE)
    if ((retcode = _stati64((char *)pname, &statbuf)) == 0)
#else
    if ((retcode = stat((char *)pname, &statbuf)) == 0)
#endif
    {
        *utfilesize = statbuf.st_size;
    }
    return (retcode);
}

/* PROGRAM: utfcmp  -  Compare filename componets
 *
 * RETURNS: int
 */
int
utfcmp(TEXT    *f1,
       TEXT    *f2)
{
#if UPFILENAM
        return stpcmp(f1, f2);
#else
        return stcomp(f1, f2);
#endif
}

/* PROGRAM: utfcomp - return pointer to start of final
 *          component of specified path name
 *
 * RETURNS: pointer to component of specified path name
 */
TEXT *
utfcomp (TEXT    *path_name)
{
    int      len;
#if OPSYS==VMS
                int     init_len;
#endif

#if OPSYS==VMS
    init_len =
#endif
    len      = stlen ( path_name );
    for ( path_name += len;
          --len >= 0;
        )
    {
        switch (*--path_name)
        {
          case '/':
#if OPSYS==WIN32API
         case '\\':
#endif
#if OPSYS==VMS
         case ':': /* some logical names have : as terminator,so if we're */
                   /* looking at the last char, keep going                */
                if (len + 1 == init_len) continue;
         case ']':
         case '>':
#endif
                        path_name++;    break;
         default:       continue;
        }
        break;
    }
#if OPSYS==WIN32API
    /* if only drive is given -- use cwd of the specified drive */
    if (stlen(path_name) == 2 && path_name[1] == ':')
        return (".");
    else if (stlen(path_name) > 2 && path_name[1] == ':')
        path_name += 2; /*get past drive spec if any*/
#endif
 
    return path_name;

}

/* PROGRAM: utfpath - removes the last segment of the path passed
 *                    eg: /name1/name2/name3  --->  /name1/name2
 *
 * RETURNS: TEXT *
 */
TEXT *
utfpath(TEXT *path_name)
{
    int      len;
    TEXT    *path_end;
 
    path_end = (TEXT *)path_name;
    len      = stlen (path_name);
    for (path_end += len; --len >= 0;)
    {
        switch (*--path_end)
        {
            case '/':
            case '\\':   
                *path_end = '\0';
                break;
            default:       
                continue;
        }
        break;
    }
 
    if (stlen(path_name) >= 0) /* make sure we have at least a directory. */
        return((TEXT *)path_name);
    else
        return(PNULL);
} /* utfpath */

/* PROGRAM: utcmpsuf - return true if name has suffix
 *
 * RETURNS: true if name has suffix
 */
int
utcmpsuf (TEXT *pname, 
          TEXT *psuffix)
{
TEXT *tmp;
    int   sufflen, 
          complen;

    tmp = utfcomp( pname );
    complen = utfcomplen( tmp );
    sufflen = stlen( psuffix );
    if ( sufflen >= complen )
        return 0;
 
#if UPFILENAM
return stncmpc( tmp + complen - sufflen, psuffix, sufflen ) ? 0 : 1;
#else
return stncmp ( tmp + complen - sufflen, psuffix, sufflen ) ? 0 : 1;
#endif
}

/* PROGRAM: utfcomplen - measures length of file basename
 *
 * RETURNS: int
 */
int
utfcomplen (TEXT *name)
{
    return stlen( name );
}

