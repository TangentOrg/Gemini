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

#ifndef BUG_H
#define BUG_H

/************************************************/
/* bug.h - Public interface for BUG subsystem.  */
/************************************************/

/*
utbug.h defines the macro:

BUG(pcontext, tag, buf, lock, exit, rv) 

which outputs an internal error message which includes the file name and line
number where the anomaly was detected. 

The parameters are:

pcontext   A dsmContext_t pointer called pcontext.

tag        A manifest constant of the form bugMSGn which uniquely 
           identifies each particular use of the macro. These 
           constants are also defined at the end of this file. When 
	   adding a use of the BUG macro, the coder should add the 
	   new tag definition to the bottom of this file, using as 
	   it's value the value of the previous last tag plus one.

buf        Any arbitrary string provided by the coder. This string 
           will be appear in the message output by the macro.

lock       A boolean expression. If the expression evaluates to true, 
           the macro will cause the database to be locked. 

exit       A boolean expression. If the expression evaluates to true,
           the macro will cause the program to exit via the message 
           subsystem, and the macro will not return. Please note that
           exiting for frivolous reasons is considered bad form, 
	   especially exiting a server. Please use this option with
	   discretion.

rv         The value to be returned by the macro, if it returns. 

The macro assumes the existance of a dsmContext_t pointer called pcontext.

When using the BUG macro, please include comments at that point in the code
to explain the circumstances and any other pertinent information that might
be helpful to someone who investigates a report of the bug occuring. 
Remember, this person might be you!

*/


#include "utmsg.h"

#define BUG(tag, buf, lock, exit, rv) (                                      \
  MSGN_CALLBACK(pcontext, utMSG152),                                         \
  MSGN_CALLBACK(pcontext, utMSG153, (tag), __FILE__, __LINE__, (rv)),        \
  ((buf)  ? (MSGN_CALLBACK(pcontext, utMSG154, (buf)), (rv)) : (rv)),        \
  MSGN_CALLBACK(pcontext, utMSG155),                                         \
  ((lock) ? (pcontext->pdbcontext->pdbpub->shutdn = DB_EXBAD) : (rv)),       \
  ((exit) ? (FATAL_MSGN_CALLBACK(pcontext, utFTL032), (rv)) : (rv)))

/* Register tags starting here:                                             */
/*      tag      #   initials  comment                                      */

#define BUGTAG0 (0) /* tjk     Example of how to use tags                   */ 
#define BUGTAG1 (1)
#define BUGTAG2 (2)
#define BUGTAG3 (3)
#define BUGTAG4 (4)
#define BUGTAG5 (5)
#define BUGTAG6 (6)

/* End of registered tags.      */
#endif  /* #ifndef BUGR_H */



