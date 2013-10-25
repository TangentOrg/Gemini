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

#ifndef DBCONFIG_H
#define DBCONFIG_H

#include "dstd.h"

#ifdef _NO_PROTO
#undef _NO_PROTO
#endif

#ifdef NULL
#undef NULL
#endif
#define NULL    0

/* Database manager specific information follows: */
#define WAITCNTLNG  22
/**************  Messaging Callback Macros ************************/

#define MSGN_CALLBACK dsmMsgnCallBack
#define FATAL_MSGN_CALLBACK dsmMsgnCallBack
#define MSGD_CALLBACK dsmMsgdCallBack
#define MSGT_CALLBACK dsmMsgtCallBack
#define GEM_MSGS_FILE "gemmsgs.db"
#define GEM_SYM_FILE  "bin/mysqld.sym"

/* NOTE: Tracing is currently enabled in psctrace.h */
/* Trace message callback macros */
#if TRACEIT
#define TRACE_CB(pc, TMSG)            MSGT_CALLBACK(pc, TMSG);
#define TRACE1_CB(pc, TMSG, TA1)      MSGT_CALLBACK(pc, TMSG, TA1);
#define TRACE2_CB(pc, TMSG, TA1, TA2) MSGT_CALLBACK(pc, TMSG, TA1, TA2);
#define TRACE3_CB(pc, TMSG, TA1, TA2, TA3) \
                                      MSGT_CALLBACK(pc, TMSG, TA1, TA2, TA3);
#else
#define TRACE_CB(pc, TMSG) ;
#define TRACE1_CB(pc, TMSG,TA1) ;
#define TRACE2_CB(pc, TMSG, TA1, TA2) ;
#define TRACE3_CB(pc, TMSG, TA1, TA2, TA3) ;
#endif

/* NOTE: Assert is currently enabled in assert.h */
/* Assert message callback macros */
/* ASSERT_ON should NEVER be defined for a port              */
#if ASSERT_ON
#define FASSERT_CB(pc, cond, failmsg)  \
    if (!(cond)) \
    { \
        MSGD_CALLBACK(pc, (TEXT *)"%BASSERT FAILURE: %s\n", failmsg); \
        MSGD_CALLBACK(pc, (TEXT *)"0GASSERT FAILURE: File: %s, Line %d\n", \
                                  __FILE__, __LINE__); \
    }
#else
#define FASSERT_CB(pc, cond,msg)
#endif

/**************  Messaging Callback Macros END ************************/

#endif  /* #ifndef DBCONFIG_H */






