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
#include <stdlib.h>  /* for getenv */
#include "dbconfig.h"
#include "drpub.h"

#include <time.h>    /* for ctime */

#include "utmsg.h"

#include <errno.h>
#include "dbcontext.h"
#include "dbpub.h"
#include "dbprv.h"

#include "utepub.h"
#include "utfpub.h"
#include "utospub.h"
#include "utspub.h"
#include "uttpub.h"
#include "bkpub.h"
#include "utmsg.h"    /* utFTL003 */

#if OPSYS==UNIX
#include <unistd.h>  /* for getpib, getenv */
#endif

#include <fcntl.h>
#include "usrctl.h"
#include "latpub.h"

#if OPSYS==WIN32API
#include <io.h>
#include <process.h>  /* for getpid() */
#endif

/**** LOCAL FUNCTION PROTOTYPES ****/

LOCALF DSMVOID	wrtmsg		(dsmContext_t *pcontext, int fd,
                                 TEXT *msgbuf, int msglen);

LOCALF DSMVOID	wrtdate		(int lgfd);

/**** END LOCAL FUNCTION PROTOTYPES ****/


/* dblg.c - contains routine to manage the database .lg file		*/
/*	    for all versions which contain the database manager code	*/
/*	    (server, single-user, utility programs, etc)		*/
/*	    For serverless shared memory, there is an auxilliary program*/
/*	    dblgssm which opens the lg file descriptor for 2nd and later*/
/*	    users							*/

/* PROGRAM: dbLogOpen -- open the database log file (".lg").
 *
 * RETURNS:	0 - OK
 *	    non-0 - Unable to open the .lg file
 */
int
dbLogOpen(
        dsmContext_t *pcontext,
        TEXT         *pname)	/* name of the database */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
	TEXT	 namebuf[MAXPATHN+1];
	fileHandle_t	 tmpfd;
        int              errorStatus;
        LONG             retMakePath;

    if (pdbcontext->logopen)
        return 0;

    /* setup the fd cache */
    utInitFdCache(128);

    /* get name of log file, open it */
    retMakePath = utMakePathname( namebuf, sizeof(namebuf), pname, (TEXT *) ".lg");
    if (retMakePath != DBUT_S_SUCCESS)
    {
        /* Report error */
        dbUserError(pcontext, retMakePath, errno, 0,
                    (DSMVOID *)NULL, (DSMVOID *)NULL);
    }

    if ((tmpfd = utOsOpen (namebuf, OPEN_W_APPEND_TEXT,
	    DEFLT_PERM, OPEN_SEQL,&errorStatus)) == INVALID_FILE_HANDLE)
	if ( (tmpfd = utOsCreat (namebuf, CREATMODE,
		    CHG_OWNER|AUTOERR,&errorStatus)) != INVALID_FILE_HANDLE)
	{
	    utOsClose(tmpfd,OPEN_SEQL);
	    tmpfd = utOsOpen (namebuf, OPEN_W_APPEND_TEXT,
			DEFLT_PERM, OPEN_SEQL,&errorStatus);
	}
    if (tmpfd == INVALID_FILE_HANDLE)
    {
        MSGN_CALLBACK(pcontext, utMSG092, namebuf, errorStatus);
        return -1;
    }

#if OPSYS == UNIX
    /* writing to the .lg file is *VERY* important, so it
       doesn't participate in the fd cache.  To make this true
       we need to close the handle and do a regular open on it */
    utOsClose(tmpfd, 0);
    pdbcontext->lgfd = open(namebuf, OPEN_W_APPEND_TEXT);
    if (pdbcontext->lgfd < 0)
    {
        MSGN_CALLBACK(pcontext, utMSG092, namebuf, errorStatus);
        return -1;
    }
#else
    pdbcontext->lgfd = _open_osfhandle((LONG)tmpfd, O_APPEND | O_TEXT);
#endif

    /* prevent child processes from inheriting this file */
    utclex (tmpfd);

    pdbcontext->logopen = 1;
    return 0;

} /* end dbLogOpen */


/* PROGRAM: dbLogClose -- close the database log file (".lg"), clear
 *		the logopen switch.
 *
 * RETURNS: DSMVOID
 */
DSMVOID
dbLogClose (dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    if ( pdbcontext->logopen )
    {
	pdbcontext->logopen = 0;
	close(pdbcontext->lgfd);
    }

} /* end dbLogClose */


/* PROGRAM: wrtmsg - do the correct type of write call depending on opsys
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
wrtmsg (
        dsmContext_t *pcontext,
        int           fd,
	TEXT         *msgbuf,
	int           msglen)
{
    int    ret=0;         /* return code */
    int    retrycnt = 0;
#if OPSYS==WIN32API
    /* Do STDOUT properly to allow redirection by parent process */
    if (1 == fd)
        {
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),
                      msgbuf, msglen, &ret, NULL);
            return;
        }
#endif  /* OPSYS == WIN32API */
retry:
    errno = 0;
    ret = write(fd, (char *) msgbuf, msglen);
               
    if (ret != msglen)
    {
        switch (errno)
        {
         case EFBIG:
                MSGN_CALLBACK(pcontext, utMSG178, "wrtmsg");
                break;
         case ENOSPC:
                MSGN_CALLBACK(pcontext, utMSG179);
                break;
         default:
            /* interrupted io to screen? */
            if (errno!=EIO && fd==1 && ++retrycnt < 100 && ret >= 0)
            {  msglen -= ret; msgbuf += ret; goto retry;  }
/* We get EINTR errno when we received a signal while doing a synchronous
   write.  For some OSs
   the write fails once and subsequently succeeds, while on others it
   forever after returns -1 with errno set to EINTR, thus the retry
   count */
            if( ++retrycnt < 100 && errno == EINTR && ret == -1 )
                goto retry;
        }
    }
 
    return;

} /* end wrtmsg */


/* PROGRAM: wrtdate - write the current long date to the log file
 *	    this is called when the .lg is opened and the first message
 *	    after midnite as long as it stays open
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
wrtdate(int lgfd) /* the log file descriptor */
{
    time_t  clock;
    TEXT    timeString[TIMESIZE];

    time (&clock);

    write(lgfd, "\n                ", 17);
    uttime(&clock, timeString, sizeof(timeString));
    /* restore \n because uttime replaces the \n with a \0. */
    timeString[24] = '\n';
    write(lgfd, timeString, 25);

} /* end wrtdate */


/* PROGRAM: dbLogMessage - write a message to the database log file
 *
 * RETURNS:    0 - OK
 *	    non-0 - Couldn't write the message to the log file
 */
#define MYBUFSZ 512	/* largest log file message */

int
dbLogMessage(
        dsmContext_t *pcontext,
        TEXT         *pmsg)	/* the message to be written		*/
{
        dbcontext_t *pdbcontext = pcontext->pdbcontext;
        dbshm_t *pdbpub = pdbcontext->pdbpub;
	TEXT	 msgbuf[MYBUFSZ+2];   /* 2 extra bytes for \r\n */
	int	 bufused;
	int	 ret = 0;
	LONG	 thisday;

    /* build the entire message in contiguous storage so that in */
    /* multi-thread, the messages wont inter-leave each other	 */
    thisday = uthms(msgbuf);	/* start with HH:MM:SS */
    msgbuf[8] = ' ';
    bufused = 9;
    if (pdbcontext)
	/* Add the appropriate prefix   */
    {
        /* add Server: or Usr NN: pfx*/
        bufused += dbLogAddPrefix(pcontext, msgbuf+bufused); 
    }
    bufused += stncop(msgbuf+bufused, pmsg, MYBUFSZ-bufused);
    msgbuf[bufused++] = '\n';

    /* add message to log */
    if (pdbcontext && pdbcontext->logopen)
    {   /* this piece of code to check if this is the first message */
	    /* written this day should really be protected by utlockdb  */
	    /* but utlockdb may call drmsg and that would cause infinite*/
	    /* loops.  The danger of 2 users doing this at the same tiem*/
	    /* is small and if they do, it merely messes up the .lg file*/
	    if (   !pdbcontext->shmgone && pdbcontext->pmtctl
	        && thisday != pdbpub->lastday)
	    {   pdbpub->lastday = thisday;
	        wrtdate(pdbcontext->lgfd);
	    }
        else
        {
            /* The code below prints the date to the log for gateways */
            /* Gateways do not allocate the mtctl structure so the code */
            /* above doesn't do the job.  For gateways, the lastday is */
            /* kept in the gwlgctl structure                         */
            if (!pdbcontext->shmgone && pdbcontext->pdbpub &&
                thisday != pdbcontext->pdbpub->lastday)
            {
                pdbcontext->pdbpub->lastday = thisday;
                wrtdate(pdbcontext->lgfd);
            }
        }
        wrtmsg(pcontext, pdbcontext->lgfd, msgbuf, bufused);

    }
    else ret = -1;

    /* find out if this is a server running in the foreground */
    if (pdbcontext->usertype & (BROKER | DBSERVER))
    {
        if((utCheckTerm()) == 1)
        {
	    wrtmsg(pcontext, 1, msgbuf, bufused);
            ret = 0;
        }
    }

    if (ret)
        wrtmsg(pcontext, 1, msgbuf, bufused);

    return ret;

} /* end dbLogMessage */


/* PROGRAM: dbLogAddPrefix - puts prefix for .lg file messages
 *
 *  PREFIXES will be in the form of "XXXXNNNNN: " where
 *  the XXXX equates to a unique type (SRV, Usr, WDOG, SHUT, ...)
 *  and NNNNN equates to the usrctl number.
 *
 *  If the type is more than 4 character (i.e. BACKUP), then
 *  we will use as much of the NNNNN space as possible to complete
 *  the name.  For example: If the usernumber is 1, then we have
 *  4 extra spaces we can use for the name.  If the usernumber is
 *  30040, the we have no extra spaces we can give the name, so if the
 *  name is > 4 characters long, then it will be truncated.
 *
 * RETURNS: number of characters for the prefix
 */
int
xdbLogAddPrefix(
        dsmContext_t *pcontext,
        TEXT         *pmsg)	/* add the prefix to the end of this msg */
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;

#if 0
    if( (pdbcontext->usertype & (DBSERVER+BROKER)) &&
       !(pdbcontext->usertype & (NAPPLSV+WDOG)))
    {
	if (pdbcontext->argnoshm || pdbcontext->psrvctl == NULL)
	    stcopy(pmsg, (TEXT *) "SERVER: ");
	else
	{
	    stcopy(pmsg, (TEXT *) "SRV 00: ");
	    utintbbn(pmsg+3, pdbcontext->psrvctl->sv_idx);
	}
	return 8;
    }
#endif
    if (pdbcontext->usertype & BIW)
    {
        stcopy (pmsg, (TEXT *) "BIW   : ");
        return (8);
    }
    else if (pdbcontext->usertype & AIW)
    {
        stcopy (pmsg, (TEXT *) "AIW   : ");
        return (8);
    }
    else if (pdbcontext->usertype & APW)
    {
        stcopy (pmsg, (TEXT *) "APW   : ");
        return (8);
    }
    else if (pdbcontext->usertype & WDOG)
    {
        stcopy (pmsg, (TEXT *) "WDOG  : ");
        return (8);
    }
    else if (pdbcontext->usertype & MONITOR)
    {
        stcopy (pmsg, (TEXT *) "MON   : ");
        return (8);
    }
    else if (pdbcontext->usertype & BACKUP)
    {
        stcopy (pmsg, (TEXT *) "BACKUP: ");
        return (8);
    }
    else if (pdbcontext->usertype & RFUTIL )
    {
	stcopy (pmsg, (TEXT *) "RFUTIL: ");
	return (8);
    }
    else if( (pdbcontext->usertype & (NAPPLSV+BROKER)) == (NAPPLSV+BROKER))
    {
	stcopy (pmsg, (TEXT *) "OIBRKR: ");
	return (8);
    }
    else if (pdbcontext->usertype & NAPPLSV)
    {
	stcopy (pmsg, (TEXT *) "OIDRVR: ");
	return (8);
    }
    else if (pdbcontext->usertype & DBSHUT)
    {
        stcopy (pmsg, (TEXT *) "SHUT  : ");
        return (8);
    }
    else
    if (   (pdbcontext->usertype & (SELFSERVE+REMOTECLIENT))
	&& !pdbcontext->arg1usr
	&& pcontext->pusrctl)
    {	stcopy(pmsg, (TEXT *) "Usr 00: ");
	utintbbn(pmsg+3, pcontext->pusrctl->uc_usrnum);
	return 8;
    }

    return 0;	/* no prefix needed */

} /* end dbLogAddPrefix */


/* PROGRAM: dbLogAddPrefix - puts prefix for .lg file messages
 *
 *  PREFIXES will be in the form of "XXXXNNNNN: " where
 *  the XXXX equates to a unique type (SRV, Usr, WDOG, SHUT, ...)
 *  and NNNNN equates to the usrctl number.
 *
 *  If the type is more than 4 character (i.e. BACKUP), then 
 *  we will use as much of the NNNNN space as possible to complete 
 *  the name.  For example: If the usernumber is 1, then we have
 *  4 extra spaces we can use for the name.  If the usernumber is 
 *  30040, the we have no extra spaces we can give the name, so if the
 *  name is > 4 characters long, then it will be truncated.
 *
 * RETURNS: number of characters for the prefix
 */
#define DBPREXLEN 11    /* this is the tatal size of the prefix area */
#define DBUSERNUMLEN 6	/* this is the total number of characters for usrnum 
                         * inclusive of the null terminator
                         */
int
dbLogAddPrefix( dsmContext_t *pcontext, TEXT    *pmsg)
{
    COUNT     i;
    COUNT     usrnum = -1;	/* either the usernnum or servernum */
    TEXT      numbuf[DBUSERNUMLEN];
    TEXT     *ptr;

    dbcontext_t *pdbcontext = pcontext->pdbcontext;

    if ( pdbcontext == NULL || !pdbcontext->usertype ) return 0;

    memset(numbuf, ' ', DBUSERNUMLEN);
    memset(pmsg, ' ', DBPREXLEN);


    if( (pdbcontext->usertype & (DBSERVER+BROKER)) &&
       !(pdbcontext->usertype & (NAPPLSV+WDOG)))
    {
	if (pdbcontext->argnoshm || pdbcontext->psrvctl == NULL)
        {
            if (pdbcontext->usertype & (BROKER))
            {
	        stcopy(pmsg, (TEXT *)"GEMINI   : ");
            }
            else
            {
	        stcopy(pmsg, (TEXT *)"SERVER   : ");
            }
            return DBPREXLEN;
        }
#if 0
	else
	{
            if ( (pdbcontext->usertype & (BROKER)) ||
                 (pdbcontext->psrvctl->servtype == BROKLOG) ) 
            { 
	          stcopy(pmsg, (TEXT *)"GEMINI     ");
            }
            else
            {
	          stcopy(pmsg, (TEXT *)"SRV        ");
            }
 
            if (pdbcontext->argnoshm || pdbcontext->shmgone)
            {
                /* at least put in the ": "  */
                /* point to the end of the prefix area */
                ptr = pmsg + DBPREXLEN - 1;

                /* put a ": " at the end of the buffer */
                *ptr-- = ' ';
                *ptr-- = ':';
    
                return DBPREXLEN;
            }
            else
            {
                usrnum = pdbcontext->psrvctl->sv_idx;
            }
	}
#endif
    }
    else if (pdbcontext->usertype & BIW)
    {
	stcopy (pmsg, (TEXT *)"BIW        ");
    }
    else if (pdbcontext->usertype & AIW)
    {
	stcopy (pmsg, (TEXT *)"AIW        ");
    }
    else if (pdbcontext->usertype & APW)
    {
	stcopy (pmsg, (TEXT *)"APW        ");
    }
    else if (pdbcontext->usertype & WDOG)
    {
	stcopy (pmsg, (TEXT *)"WDOG       ");
    }
    else if (pdbcontext->usertype & MONITOR)
    {
	stcopy (pmsg, (TEXT *)"MON        ");
    }
    else if (pdbcontext->usertype & BACKUP)
    {
	stcopy (pmsg, (TEXT *)"BACKUP     ");
    }
    else if (pdbcontext->usertype & RFUTIL )
    {
	stcopy (pmsg, (TEXT *)"RFUTIL     ");
    }
    /* JFJ - This is wrong DBUTIL is reserved for offline _dbutil 
       connections only. DBSHUT is reserved for shutdown. */
    else if( (pdbcontext->usertype & (DBUTIL+DBSHUT)) == (DBUTIL+DBSHUT))
    {
	stcopy (pmsg, (TEXT *)"QUIET      ");
    }
    else if (pdbcontext->usertype & DBSHUT)
    {
	stcopy (pmsg, (TEXT *)"SHUT       ");
    }
    else if (   (pdbcontext->usertype & (SELFSERVE+REMOTECLIENT))
	&& !pdbcontext->arg1usr && pcontext->pusrctl)
    {   
        stcopy (pmsg, (TEXT *)"Usr        ");
    }
    else
    {
        return 0;
    }

    if (usrnum < 0)
    {
        if (pcontext->pusrctl && !pdbcontext->shmgone)
        {
            usrnum = pcontext->pusrctl->uc_usrnum;
        }
        else
        {
            return DBPREXLEN;
        }

    }

    /* add the user number to the prefix */
    sprintf((psc_rtlchar_t *)numbuf,"%5d",usrnum);

    /* copy the usernumber into the buffer */

    /* point to the end of the prefix area */
    ptr = pmsg + DBPREXLEN - 1;
    
    /* put a ": " at the end of the buffer */
    *ptr-- = ' ';
    *ptr-- = ':';
    
    /* work backwards on the number buffer and copy the numbers until 
       you hit a blank.  This works because the formating for sprintf() */
    i = DBUSERNUMLEN -2;
    while(i >= 0 && numbuf[i] != ' ')
    {
        *ptr = numbuf[i];
        ptr--;
        i--;
    }

    return DBPREXLEN;

}
