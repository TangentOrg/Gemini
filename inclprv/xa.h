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

#ifndef XA_H
#define XA_H
/*
 * Start of x.h header - as copied from X/Open, CAE Specification
 * Distributed Transaction Processing: The XA Specification
 * ISBN 1 872630 24 3
 *
 */
/*
 * Transactin branch identification: XID and NULLXID:
 */
#define XIDDATASIZE     128     /* size in bytes  */
#define MAXGTRIDSIZE    64      /* maximum size in bytes of gtrid */
#define MAXBQUALSIZE    64      /* maximum size in bytes of bqual */
struct xid_t 
{
    long        formatID;       /* format identifier   */
    long        gtrid_length;   /* value from 1 through 64 */
    long        bqual_length;   /* value from 1 through 64 */
    char        data[XIDDATASIZE];
};
typedef struct xid_t  XID;
/*
 * A value of -1 in formatID means that the XID is null.
 */

/*
 * Declarations of routines by which RMs call TMs: (TDB)
 */

/*
 * XA Switch Data Structure (TBD)
 */

/*
 * Flag definitions for the RM switch
 */
#define TMNOFLAGS       0x00000000L
#define TMREGISTER      0x00000001L     /* resource manager dynamically
                                           registers */
#define TMNOMIGRATE     0x00000002L     /* resource manager does not support
                                           association migration */
#define TMUSEASYNC      0x00000004L     /* resource manager supports
                                           asynchronous operations */

/*
 * Flag definitions for the xa_ and ax_ routines
 */
/* use TMNOFLAGS, defined above, when not specifying other flags */
#define TMASYNC         0x80000000L     /* peform routine asynchronously */
#define TMONEPHASE      0x40000000L     /* caller is using one-phase commit
                                           optimisation */
#define TMFAIL          0x20000000L     /* dissociate caller and marks
                                           transaction branch rollback-only */
#define TMNOWAIT        0x10000000L     /* return if blocking condition exists */
#define TMRESUME        0x08000000L     /* caller is resuming association
                                           with suspended transaction branch */
#define TMSUCCESS       0x04000000L     /* dissociate caller from transaction
                                           branch  */
#define TMSUSPEND       0x02000000L     /* caller is suspending, not ending,
                                           association  */
#define TMSTARTSCAN     0x01000000L     /* start a recovery scan      */
#define TMENDRSCAN      0x00080000L     /* end a recovery scan */
#define TMMULTIPLE      0x00400000L     /* wait for any asynchronous operation */
#define TMJOIN          0x00100000L     /* caller intends to perform migration */

/*
 * xa_() return codes (resource manager reports to transaction manger
 */
#define XA_RBBASE       100             /* the inclusive lower bound of the
                                           rollback codes */
#define XA_RBROLLBACK   XA_RBBASE       /* the rollback was caused by an
                                           unspecified reason */
#define XA_RBCOMMFAIL   XA_RBASE+1      /* the rollback was caused by a
                                           communication failure */
#define XA_RBDEADLOCK   XA_RBBASE+2     /* a deadlock was detected */
#define XA_RBINTEGRITY  XA_RBBASE+3     /* a condition that violates the
                                           integrity of the resources
                                           was detected */
#define XA_RBOTHER      XA_RBBASE+4     /* the resource manager rolled back
                                           the transaction branch for a
                                           reason not on this list */
#define XA_RBPROTO      XA_RBBASE+5     /* a protocol error occurred in the
                                           resource manager */
#define XA_RBTIMEOUT    XA_RBBASE+6     /* a transaction branch took too
                                           long */
#define XA_RBTRANSIENT  XA_RBBASE+7     /* may retry the transaction branch */
#define XA_RBEND        XA_RBTRANSIENT  /* the inclusive upper bound of the
                                           rollback codes  */
#define XA_NOMIGRATE    9               /* resumption must occur where
                                           suspension occurred */
#define XA_HEURAZ       8               /* the transaction branch may have
                                           been heuristically completed */
#define XA_HEURCOM      7               /* the transaction branch has been
                                           heuristically completed */
#define XA_HEURRB       6               /* the transaction branch has been
                                           heuristically rolled back */
#define XA_HEURMIX      5               /* the transaction branch has been
                                        heuristically committed and rolled back */
#define XA_RETRY        4               /* routine returnned with no effect and
                                           may be reissued */
#define XA_RDONLY       3               /* the transaction branch was read-only
                                           and has been committed  */
#define XA_OK           0               /* normal execution */
#define XAER_ASYNC      -2              /* asynchoronous operation already
                                           outstanding */
#define XAER_RMERR      -3              /* a resource manager error occurred
                                           in the transaction branch */
#define XAER_NOTA       -4              /* the XID is not valid      */
#define XAER_INVAL      -5              /* invalid arguments were given */
#define XAER_PROTO      -6              /* routine invoked in an improper
                                           context */
#define XAER_RMFAIL     -7              /* resource manager unavailable */
#define XAER_DUPID      -8              /* the XID already exists       */
#define XAER_OUTSIDE    -9              /* resource manager doing work outside */

#endif  /* ifndef XA_H */
