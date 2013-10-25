
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

#include <stdarg.h>      /* MUST BE INCLUDED BEFORE dstd.h (ALPHA OS) */
#include <stdio.h>

#include "dbconfig.h"
#include "dbcontext.h"
#include "utepub.h"
#include "utfpub.h"
#include "uttpub.h"
#include "utospub.h"
#include "utspub.h"
#include "utmpub.h"


#if OPSYS==WIN32API
#include <stdlib.h>
#endif

#include <setjmp.h>
#include <errno.h>

LOCAL TEXT *gMsgTable = NULL;
LOCAL LONG *gMsgIndex = NULL;
LOCAL LONG gMsgCount = 0;

#define FATALBIT    0x0001
#define DUMPBIT     0x0002


/* PROGRAM: utMsgOutput - write a message to the log file
 */
LOCALF int
utMsgOutput(
    dsmContext_t         *pcontext,
    TEXT                 *outbuf,
    int                   msgbits)

{
    int ret = dbLogMessage(pcontext, outbuf);

    if ((msgbits & FATALBIT))
    {
      dbExit(pcontext, (msgbits & DUMPBIT) ? DB_DUMP_EXIT : 0, DB_EXBAD);
    }

    return ret;
}


/* PROGRAM: utMsgReadMessages - read the message file into global memory
 * RETURN: 0 on success, -1 on failure
*/
int
utMsgReadMessages( dsmContext_t         *pcontext)
{
    fileHandle_t  fd;
    int           errorStatus;
    int           bytesRead;
    TEXT          msghead[8];
    int           msgsLen;
    int           i;
    TEXT          pmsgfil[MAXPATHN+1];

    /* Look for the message db in the specified location (or current
    ** directory if not specified).  If it's not there, look for the 
    ** GEMMSGS environment variable.
    */
    if (pcontext->pdbcontext->pmsgsfile)
    {
        fd = utOsOpen(pcontext->pdbcontext->pmsgsfile,
                      OPEN_R, DEFLT_PERM, OPEN_SEQL, &errorStatus);
    }
    else
    {
        fd = utOsOpen((TEXT *)GEM_MSGS_FILE,
                      OPEN_R, DEFLT_PERM, OPEN_SEQL, &errorStatus);
    }

    if (fd == INVALID_FILE_HANDLE)
    {
        if (utgetenvwithsize((TEXT *)"GEMMSGS", pmsgfil, (MAXPATHN+1)))
        {
            return -1;
        }
        else
        {
            fd = utOsOpen(pmsgfil, OPEN_R, DEFLT_PERM, OPEN_SEQL, &errorStatus);
            if (fd == INVALID_FILE_HANDLE)
            {
                return -1;
            }
        }
    }

    /* read the header: number of messages and total size of messages */
    if (utReadFromFileSeq(fd, msghead, 8, &bytesRead, &errorStatus))
    {
        return -1;
    }

    /* all numbers are big-endian, so convert them */
    gMsgCount = xlng(msghead);
    msgsLen = xlng(msghead + 4);

    /* get some memory for the index and message table */
    gMsgIndex = (LONG*)utmalloc(gMsgCount * 4 * 2);
    gMsgTable = (TEXT*)utmalloc(msgsLen);

    /* read the index */
    if (utReadFromFileSeq(fd, (TEXT *)gMsgIndex, gMsgCount * 4 * 2, &bytesRead,
                          &errorStatus))
    {
        return -1;
    }

    /* read the message table */
    if (utReadFromFileSeq(fd, gMsgTable, msgsLen, &bytesRead, &errorStatus))
    {
        return -1;
    }

    /* convert indexes from big-endian to platform order */
    for (i = 0; i < gMsgCount * 2; i++)
    {
        gMsgIndex[i] = xlng((TEXT*)&gMsgIndex[i]);
    }

    utOsClose(fd, OPEN_SEQL);

    return 0;
}


DSMVOID
reverse ( TEXT    *ps)
{
    TEXT    *pt;
    TEXT     c ;
    int len;

    len = stlen(ps);

    for (pt = ps + len; --pt > ps; ps++)
    {
        c = *ps;
        *ps = *pt;
        *pt = c;
    }
}

int
intoan( FAST  COUNT  n, TEXT  s[])
{
TEXT	*ps;
GBOOL	minus;

    if ((minus = (n < 0))) n = -n;
    ps = s;
    do {
	*ps++ = n % 10 + '0';
    }
    while ((n /= 10) > 0);
    if (minus) *ps++ = '-';
    *ps = '\0';
    reverse(s);
    return (int)(ps - s);
}


/* PROGRAM: utMsgSub - do parameter substitution in a message template
 */
LOCALF int
utMsgSub(
    TEXT                 *msgbuf,
    TEXT                 *fmtbuf,
    va_list              *pparm)
{
    TEXT    *msgend;        /* points past end of msgbuf */
    TEXT    *bufstrt=msgbuf;/* points at start of buffer */
    TEXT    *ps;
    int      goodsub;
    int      msgbits = 0;

    /* case: This is a list of % parameters supported by msgn and msgd */

    /* a logical message can be a chain of fixed,length messages */
    msgend= msgbuf + MSGBUFSZ;

    while (msgbuf < msgend && *fmtbuf != '\n' && *fmtbuf != '\0')
    {   if( *fmtbuf != '%')
	    *msgbuf++ = *fmtbuf++;
	else
	{
	    goodsub = 1;
	    switch(*++fmtbuf)
	    {
	     case 'n': /* The parm is COUNT, display file whose fd==parm */
             case 'd': /* Display a COUNT value (ie a short int) */
		msgbuf += intoan((COUNT)va_arg(*pparm,int),msgbuf);
		break;

	     case 'D': /* DBKEY */
                msgbuf += intoall(va_arg(*pparm,LONG64),msgbuf);
		break;

	     case 'l': /* LONG */
                msgbuf += intoal(va_arg(*pparm,LONG),msgbuf);
		break;

	     case 'i': /* int */
		msgbuf += intoal(va_arg(*pparm,int), msgbuf);
		break;

	     case 't': /* GTBOOL or TTINY */
		msgbuf += intoan((COUNT)va_arg(*pparm,int), msgbuf);
		break;

	     case 'b': /* GBOOL */
		msgbuf += intoan((COUNT)va_arg(*pparm,int), msgbuf);
		break;

	     case 'c': /* character */
		*msgbuf++ = va_arg(*pparm,int);
		break;

	     case 's': /*char string, NULL parm is ok */
	     case 'F': /* parm is LONG field schema num, display label  */
	     case 'N': /* parm is LONG field schema num, display name   */
	     case 'C': /* parm is LONG file schema num, display name    */
		ps = (TEXT *)va_arg(*pparm, char *);
		if (ps!=NULL)
		    while (*ps != '\n' && *ps != '\0' && msgbuf < msgend)
			*msgbuf++ = *ps++;
		break;

	     case 'r': /* user formatting cmd: add a newline into message. */
		*msgbuf++ = '\n';
		break;

	     case 'T': /* insert enough blanks for a TAB */
		do {
		    *msgbuf++ = ' ';
		} while ((msgbuf-bufstrt) & 7);
		break;

	     case 'E': /* no parm needed, just display errno. */
		msgbuf += intoan((COUNT)errno, msgbuf);
		break;

             case 'L': /* log message - ignored (all messages are logged) */
                break;

             case 'G': /* fatal message with core dump */
                msgbits |= FATALBIT + DUMPBIT;
                break;

             case 'g': /* fatal message without core dump */
                msgbits |= FATALBIT;
                break;

	     default:
		*msgbuf++ = '%';
		*msgbuf++ = *fmtbuf;
		goodsub=0;
	    }
	    fmtbuf++;

	    /* strip the <...> stuff which appears after a % */
	    if (goodsub && *fmtbuf == '<')
	    while(*fmtbuf && *fmtbuf++ != '>')
	    ;
	}
    }
 
    *msgbuf = '\0';

    return msgbits;
}


/* PROGRAM: utMsgFind - get a message from the message file and do parameter
 *                      substitution in the message template
 */
LOCALF int
utMsgFind(
    dsmContext_t         *pcontext,
    int                   msgNumber,
    TEXT                 *outbuf)
{
    int msgOffset = -1;
    int i;
    TEXT *p;

    /* If we haven't loaded the messages db, load it now.  If we can't
    ** load it, return an error.
    */
    if (!gMsgTable)
    {
        if (utMsgReadMessages(pcontext))
            return 0;
    }

    /* Search the message index for a match.  The index is ordered
    ** pairs (msg number, msg offset).
    */
    for (i = 0; (i < gMsgCount * 2) && (msgOffset == -1); i += 2)
    {
        if (gMsgIndex[i] == msgNumber)
        {
            msgOffset = gMsgIndex[i + 1];
        }
    }

    /* if msgOffset hasn't been set, we didn't find the message */
    if (msgOffset == -1)
    {
        return 0;
    }

    /* Copy the message starting at the offset, stopping when we find
    ** the \n at the end.
    */
    for (p = outbuf; gMsgTable[msgOffset] != '\n'; msgOffset++, p++)
    {
        *p = gMsgTable[msgOffset];
    }

    /* Stick a null on the end of the message */
    *p = '\0';

    return 1;
}


/* PROGRAM: utDefaultMsgCallback - message call back switch for msgn, msgd etc
 */
int
utMsgDefaultCallback(
    DSMVOID                 *param _UNUSED_,
    dsmContext_t         *pcontext,
    TEXT                 *plogPrefix _UNUSED_,
    int                   msgNumber,
    va_list              *args)
{
    TEXT fmtbuf[1024];
    TEXT outbuf[1024];
    int  msgbits = 0;

    if (msgNumber == 0)
    {
        /* This is a msgd, just do the substitution */
        TEXT *pformat = (TEXT *)va_arg(*args, char *);
        msgbits = utMsgSub(outbuf, pformat, args);
    }
    else
    {
        /* found a msgn, get the message from the message file and
        ** do the substitution
        */
        if (!utMsgFind(pcontext, msgNumber, fmtbuf))
        {
            /* We didn't find the message (maybe the message db is missing)
            ** so just put out the error number for now.  This won't happen
            ** when we have GeminiInit load the message db and quit if it
            ** is missing.
            */
            sprintf((char *)outbuf, "Can't find message #%d", msgNumber);
        }
        else
        {
            msgbits = utMsgSub(outbuf, fmtbuf, args);
        }
    }

    /* Now write the message to the log file */
    utMsgOutput(pcontext, outbuf, msgbits);

    return 0;
}

