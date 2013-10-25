
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

/* Table of Contents - file system utitity functions:
 *
 * utpwdSetPath   - Set current path (UNIX only).
 * utapath        - Given a file name, possibly fully qualified,
 *		            an optional suffix, a target buffer, and the 
 *                  length of the target buffer, put the pieces 
 *                  of the file name together into the output buffer.
 * utmypath       - path setup for use by gemini
 * utpwd          - Copy the path name of the current working directory.
 * utdrvstr       - adds the appropriate drive specification to path 
 *                  name.
 * utGetDriveType - Determines the type of the Drive given.
 * utgetdrv       - Get the current drive.
 * utgetdir       - Store current DOS directory in provided buffer.
 * utIsDir        - Tests if a given entry is a directory.
 * utMakePathname - concat 2 strings together and print a message if
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
#include "utfpub.h"       /* for utfCompressPath */
#include "utmpub.h"
#include "utspub.h"       /* for stlen, stncop, p_stcopy */

#if OPSYS==VMS
#include <ssdef.h>
#include <errno.h>
#include <rms.h>
#include <libdef.h>
#endif /* OPSYS == VMS */

#if OPSYS==UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif

/* PROGRAM: utapath - given a file name, possibly fully qualified,
 *		an optional suffix, a target buffer, and the length
 *		of the target buffer, put the pieces of the file name
 *		together into the output buffer. If the pathname is
 *		not fully qualified, prepend the current working
 *		directory.
 *
 *		Needs work for MSDOS.
 *
 * RETURNS: the output buffer updated, truncated if the result is
 *		too long
 *
 */
TEXT *
utapath (TEXT	*fullpath,	/* fully qualified path name */
    	 int	 pathlen,	/* length of answer buffer */
		 TEXT	*filename,	/* file name, possibly qualified */
		 TEXT	*suffix)	/* suffix for file name */

{
    TEXT *ptxt;		/* current location */
    int	 used = 0;	/* amount used */
    int	 drvno = 0;	
#if OPSYS==WIN32API

        int  i,
             j;
#endif

    /* if file name not fully qualified, get pathname of current
       working directory into answer buffer */
#if OPSYS==UNIX
    if (*filename != '/')
#endif

#if OPSYS==WIN32API
    /* handle drive specfier, if any */
    j = stlen(filename);    
    for (i = 1, ptxt = filename; i <= j; i++, ptxt++)
        if (*ptxt == ':') break;
    if (i < j && i != 2)       
    {     /* file server volume name is specified */
        bufcop(fullpath, filename, i);
        used += i;
        filename += i;
    }
    if ((i == 2) && (filename[1] == ':'))
    {	bufcop(fullpath, filename, 2);
	used += 2;
	drvno = toupper(*filename) - 64;      /* A = 1 B = 2 ..etc */
	filename += 2;
    }
    else
	drvno = 0;   /* means use current drive */

    if ((*filename != '/') && (*filename != '\\'))
#endif  /* OPSYS == WIN32API */
    {
        
        used += utpwd (fullpath+used, pathlen ,drvno);


        if (*filename)
        {
	        if((*(fullpath+used-1) != '\\') && (*(fullpath+used-1) != '/'))
	        {    ptxt = (TEXT *)stcopy ( (TEXT *)fullpath+used, (TEXT *)"/" );
	            ++used;
	        }
        }
    }

    ptxt = fullpath + used;

    /* move in filename and suffix */
    used += stncop ( ptxt, filename, (pathlen - used) );
    ptxt = fullpath + used;
    used += stncop ( ptxt, suffix, (pathlen - used) );
    used = utfCompressPath(fullpath);
    /* return pointer to null terminator */
    return (fullpath + used);
}

/* PROGRAM: utmypath - Creates a full path using the input components
 *
 */
DLLEXPORT TEXT *
utmypath(TEXT *pfullpath, TEXT *pdatadir,TEXT *prelname,TEXT *psuffix)
{
    int i = 0;
    TEXT dirsep;
    
#if OPSYS == WIN32API
        dirsep = '\\';
#else
        dirsep = '/';
#endif

    if (prelname[0] != dirsep
#if OPSYS == WIN32API
        && prelname[1] != ':'
#endif
        )
    {
        
        stncop(pfullpath, pdatadir, MAXUTFLEN);
        i = stlen(pdatadir);

        pfullpath[i] = dirsep;
        i++;
    }
        
    stncop(&pfullpath[i],prelname,MAXUTFLEN);
    i+= stlen(prelname);
    if(psuffix)
    {
        stncop(&pfullpath[i],psuffix,MAXUTFLEN);
        i += stlen(psuffix);
    }
    pfullpath[i] = '\0';
    i++;
    return &pfullpath[i];    
}

/* PROGRAM: utpwd - copy the path name of the current working directory
 *		into a buffer, truncating if necessary.
 *
 *		Arguments identify buffer location and length.
 *
 * RETURNS: the length of the pathname.
 */
int
utpwd (TEXT	 *pdir,	/* answer goes here */
       int        len _UNUSED_,   /* length of answer area */
       int        drvno _UNUSED_) /* drive number */
{
    TEXT	*ptxt = pdir;

    /* terminate string, if any errors encountered, leave it null */
    *pdir = '\0';

#if OPSYS==UNIX
    getcwd((char *)pdir,MAXUTFLEN);
#else  /* !UNIX */
#if OPSYS==WIN32API
    /* if no drive number specified use current */
    if(drvno == 0) 
    {
      drvno = utgetdrv();    
      utgetdir(drvno,pdir);
      utdrvstr(drvno,pdir);     /* add drive specification for directory path */
    }
    else
       utgetdir(drvno,pdir);
#endif


#endif  /* #else !UNIX */
    /* get rid of trailing newline, if any */
    ptxt = (TEXT *)(term(pdir) - 1);
    if ( *ptxt == '\n' && ptxt != pdir )
	*ptxt = '\0';

    /* return length of path name */
    return stlen( pdir );
}   

/* PROGRAM: utMakePathname - concat 2 strings together and print a message if
 *              the result is too big for the buffer
 * NOTE WELL:  The scope of this routine has been narrowed to include only
 *             its standard use to append a type suffix onto a file name.
 *             Using it for any other purpose may not work on VRX or for that
 *             matter anywhere else.  - gb - 9/30/87
 *
 * RETURNS:     -1     string too large for buffer to new string
 *               0     strings concat'd
 */
LONG
utMakePathname(TEXT  *buffer,
               int   buflen,
               TEXT  *pstr1,
               TEXT  *pstr2)
{
    TEXT *nbuffer;
 
    if ( stlen(pstr1) + stlen(pstr2) + 1 > buflen )
    {
        return DBUT_S_NAME_TOO_LONG;
    }
 
    nbuffer =  (TEXT *)stcopy(stcopy(buffer, pstr1), pstr2);
    return 0;
}

#if OPSYS==WIN32API
/* PROGRAM utdrvstr - adds the appropriate drive specification to path name
 *
 * RETURNS: int
 */
int
utdrvstr(drvno,pdir)
   int drvno;
   TEXT *pdir;
{
    TEXT drv_buf[MAXUTFLEN + 1];
    
    if(drvno > 0)
    {
        drv_buf[0] = drvno + 64;
	drv_buf[1] = ':';
	drv_buf[2] = '\0';

        stcopy(term(drv_buf),pdir);
        stcopy(pdir,drv_buf);
    }

    return 0;
}   

/* PROGRAM: utGetDriveType - Determines the type of the Drive given.
 *
 * RETURNS: int
 */
int
utGetDriveType (TEXT  DriveLetter)
{
    int    ret = 0;
    int    DriveType = 0;
    char   DriveRoot[4];   /* for the GetDriveType call */
 
   /* setup the drive identifier */
   DriveRoot[0] = DriveLetter;
   DriveRoot[1] = ':';
   DriveRoot[2] = '\\';
   DriveRoot[3] = '\0';
 
   DriveType = GetDriveType(DriveRoot);
 
   if (DriveType == DRIVE_REMOVABLE)
         return(REMOVABLE_DRIVE | LOCAL_DRIVE);
   if (DriveType == DRIVE_FIXED)
         return (FIXED_DRIVE | LOCAL_DRIVE);
   if (DriveType == DRIVE_REMOTE)
         return (FIXED_DRIVE | REMOTE_DRIVE);
   if (DriveType == DRIVE_CDROM)
         return(REMOVABLE_DRIVE | LOCAL_DRIVE);
   if (DriveType == DRIVE_RAMDISK)
         return (SUBSTITUTED_DRIVE | LOCAL_DRIVE);
   return(UNKNOWN_DRIVE);
}

/* PROGRAM: utgetdrv - get the current drive
 *
 * RETURNS: COUNT
 */
COUNT
utgetdrv()
{
    char temp[MAXUTFLEN];
 
    if (GetCurrentDirectory(MAXUTFLEN,temp) > MAXUTFLEN)
    {
      return 0;  /* Buffer not large enough, Punt */             /*  A=1 */
    }
 
    if (temp[0]>='A' && temp[0]<='Z')
    {
      return((COUNT)temp[0] - (COUNT)'A' + 1);
    }
    else
    {
      if (temp[0]>='a' && temp[0]<='z')
      {
        return((COUNT)temp[0] - (COUNT)'a' + 1);
      }
 
      return 0;  /* Not a normal drive, maybe a network path */  /*  A=1 */
    }
}
/* PROGRAM: utgetdir - store current DOS directory in provided buffer\
 *
 * RETURNS: COUNT
 */
COUNT
utgetdir (int   drvno,    /* A=1 */
          TEXT  *outptr)   /* where to store the results */
{
   DWORD dwCnt;
   char chOldDrive; 
   char drvltr[3];
   char *temp = drvltr;
 
   drvltr[0] = 'A';
   drvltr[1] = ':';
   drvltr[2] = '\0';

    /*
     * Check if the drive requested exists.
     *
     * "dwCnt" is a bitmask where bit 0 is drive A, bit 1 is drive B, ...
     * Just return zero(0) if the drive requested does not exist.
     */
    dwCnt = GetLogicalDrives();
    if (drvno > 0)
        if ( !(dwCnt & (1 << (drvno-1))))
          return 0;
 
    /* if drvno == 0, it maybe a network path, just continue..  */
   /*
     * Query the string (Not coded to support UNICODE yet)
     */
    if (GetCurrentDirectory(MAXUTFLEN,outptr) > MAXUTFLEN)
      return 0;  /* Buffer not large enough */
 
    /*
     * Try to get to the requested drive
     */
    chOldDrive = outptr[0];
    temp[0]=(char)((COUNT)'A' + drvno-1 );
    if (SetCurrentDirectory(temp))
    {
        
        /* Get the current directory on the requested drive */
        if (GetCurrentDirectory(MAXUTFLEN,outptr) > MAXUTFLEN)
        {
            return 0;
        }
 
        /* Remove the leading drive name (ie. A:), leaving the '\'*/
        stcopy(outptr,outptr+2);
        /* Bring us back to the original drive and directory */
        temp[0]=chOldDrive;
        SetCurrentDirectory(temp);
        return 1;
    }
    else
    {
        return 0;
    }
}
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
    if (stat((const char *)pname, pstat))
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
