
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
#include "shmpub.h"
#include "dbpub.h"  /* for dbenv & deadpid call */
#include "drmsg.h"
#include "drprv.h"
#include "dsmpub.h"  /* for DSM API */
#include "dbcontext.h"
#include "mstrblk.h"
#include "usrctl.h"
#include "utepub.h"
#include "utcpub.h"
#include "utfpub.h"
#include "utmsg.h"
#include "utspub.h"
#include "uttpub.h"
#include "rlpub.h"
#include "rlprv.h"
#include "rlai.h"

/* PROGRAM: dbQuietPoint - evaluate the quiet request and
 *                         enable or disable the quiet point.
 *
 * RETURNS: 0 - success
 *          non-zero - failure
 */
int
dbQuietPoint(dsmContext_t *pcontext,
             UCOUNT        quietRequest,
             ULONG         quietArg)
{
    int returnCode = 0;       /* status of the quiet request */
    dbcontext_t  *pdbcontext = pcontext->pdbcontext;
    dbshm_t      *pdbshm = pdbcontext->pdbpub;
    usrctl_t     *pusrctl = pcontext->pusrctl;
    RLCTL        *prl = pdbcontext->prlctl;
    LONG64        bithreshold = 0;
    DOUBLE        bitholdBytes = 0;
    DOUBLE        bitholdSize = 0;
    TEXT          sizeId;
    DOUBLE        fromBitholdBytes = 0;
    DOUBLE        fromBitholdSize = 0;
    TEXT          fromSizeId;
    DOUBLE        toBitholdBytes = 0;
    DOUBLE        toBitholdSize = 0;
    TEXT          toSizeId;
    TEXT          printBuffer[13];
    TEXT          fromPrintBuffer[13];
    TEXT          toPrintBuffer[13];


    /* Before we attempt to enable or disable the quiescent point */
    /* we need to check for the existence of the broker.          */

        if (!deadpid(pcontext, pdbshm->brokerpid))
        {
            if (quietRequest == QUIET_POINT_REQUEST)
            {
                if (!pdbshm->quietpoint == QUIET_POINT_NORMAL)
                {
                    if ((pdbshm->quietpoint == QUIET_POINT_REQUEST) ||
                       (pdbshm->quietpoint == QUIET_POINT_ENABLED)) 
                    {
                        /* Quiet point has already been requested. */
                        MSGN_CALLBACK(pcontext, drMSG392);
                        returnCode = 4;
                    }
                     else
                    {
                        /* Protect against a second user trying to quiet */
                        /* Enabling the quiet point is in effect. */
                        MSGN_CALLBACK(pcontext, drMSG393);
                        returnCode = 6;
                    }
                }
                else
                /* Database is in the Normal Processing State */
                {
                    
                    /* Check if after imaging is enabled without */
                    /* the use of after image extents.           */
                    if ( rlaiqon(pcontext) == 1 && 
                         pdbcontext->pmstrblk->mb_aibusy_extent == 0 )
                    {
                        /* Cannot enable with single ai file */
                        MSGN_CALLBACK(pcontext, drMSG394);
                        returnCode = 2;
                    }
                    else  
                    {

                    /* Reset the shared memory variable to request    */
                    /* the enabling of the quiet point by the broker. */
                    pdbshm->quietpoint = QUIET_POINT_REQUEST;

                    /* Loop until the quietpoint state changes to */
                    /* QUIET_POINT_ENABLED */
                    for (;;)
                    {
                        /* Test for brokers existence and whether */
                        /* shutdown has already occurred. */
                        if (deadpid(pcontext, pdbshm->brokerpid)) 
                        {
                            MSGN_CALLBACK(pcontext, drMSG395);
                            returnCode = 16;
                            break;
                        }

                        /* Check to see if a shutdown was received. */
                        if (pusrctl->usrtodie)
                        {
                                MSGN_CALLBACK(pcontext, drMSG396);
                                returnCode = 16;
                                break;
                        }

                        if (pdbshm->quietpoint == QUIET_POINT_ENABLED)
                        {
                            pdbcontext->usertype |= DBSHUT;
                            returnCode = 0;
                            break;
                        }
                        utsleep(1);
                    }

                    } 
                }
            }
            /* Disable the Ouiet Point */
            else if (quietRequest == QUIET_POINT_DISABLE)
            {
                /* Database is in the Normal Processing State */
                if (pdbshm->quietpoint == QUIET_POINT_ENABLED)
                {
                    /* Reset the shared memory variable to request    */
                    /* the enabling of the quiet point by the broker. */
                    pdbshm->quietpoint = QUIET_POINT_DISABLE;

                    /* Loop until the quietpoint state changes to */
                    /* QUIET_POINT_ENABLED */
                    for (;;)
                    {
                        /* Test for brokers existence */
                        if (deadpid(pcontext, pdbshm->brokerpid))
                        {
                            MSGN_CALLBACK(pcontext, drMSG397);
                            pdbcontext->usertype |= DBSHUT;
                            returnCode = 16;
                            break;
                        }

                        /* Check to see if a shutdown was received. */
                        if (pusrctl->usrtodie)
                        {
                            MSGN_CALLBACK(pcontext, drMSG398);
                            pdbcontext->usertype |= DBSHUT;
                            returnCode = 16;
                            break;
                        }
                            
                        if (pdbshm->quietpoint == QUIET_POINT_NORMAL)
                        {
                            pdbcontext->usertype |= DBSHUT;
                            returnCode = 0;
                            break;
                        }
                        utsleep(1);
                    }
                }
                else if (pdbshm->quietpoint != QUIET_POINT_ENABLED)
                {
                    /* Quiet point does not need to be disabled. */
                    MSGN_CALLBACK(pcontext, drMSG399);
                    pdbcontext->usertype |= DBSHUT;
                    returnCode = 3;
                }
            }
            else if (quietRequest == QUIET_POINT_BITHRESHOLD)
            {
                 /* Round down bi threshold by a cluster if it is set */
                 bithreshold =
                     ( (((LONG64)quietArg * 1048576) / prl->rlbiblksize)
                          - (prl->rlclbytes / prl->rlbiblksize) );

                 if (bithreshold > 0 && bithreshold > prl->rlsize)
                     
                 {
                     /* Validate the bi threshold value */
                     if (pdbshm->bithold == bithreshold)
                     {
                         /* bithold size same, request to change rejected */
                         bitholdBytes =
                                    (DOUBLE)bithreshold * prl->rlbiblksize;
                         utConvertBytes(bitholdBytes,
                                        &bitholdSize, &sizeId);
                         sprintf((char *)printBuffer, "%-5.1f %cBytes",
                                               bitholdSize, sizeId);
                         MSGN_CALLBACK(pcontext, drMSG687, printBuffer);
                         MSGN_CALLBACK(pcontext, drMSG451);
                         returnCode = 3;
                     }
                     else if (pdbshm->bithold == 0 
                              || pdbshm->bithold > (ULONG)prl->rlsize)
                     {
                         if (pdbshm->quietpoint != QUIET_POINT_ENABLED)
                         {
                             /* Quiet point not enabled and stall did not occur.
                                Request to change bi threshold rejected */
                             MSGN_CALLBACK(pcontext, drMSG452);
                             MSGN_CALLBACK(pcontext, drMSG451);
                             returnCode = 3;
                         }
                         else
                         {
                             /* Make sure new bi threshold is larger than
                                bi file size */
                             if (bithreshold >= prl->rlsize)
                             {
                                 /* bi threshold changed from value to value */
                                 fromBitholdBytes =
                                    (DOUBLE)pdbshm->bithold * prl->rlbiblksize;
                                 utConvertBytes(fromBitholdBytes,
                                                &fromBitholdSize, &fromSizeId);
                                 sprintf((char *)fromPrintBuffer, "%-5.1f %cBytes",
                                               fromBitholdSize, fromSizeId);
                                 toBitholdBytes =
                                    (DOUBLE)bithreshold * prl->rlbiblksize;
                                 utConvertBytes(toBitholdBytes,
                                                &toBitholdSize, &toSizeId);
                                 sprintf((char *)toPrintBuffer, "%-5.1f %cBytes",
                                               toBitholdSize, toSizeId);
                                 MSGN_CALLBACK(pcontext, drMSG686,
                                               fromPrintBuffer, toPrintBuffer);
                                 pdbshm->bithold = bithreshold;
                                 returnCode = 0;
                             }
                             else
                             {
                                 /* Invalid value provided, request to change
                                    bi threshold rejected */
                                 MSGN_CALLBACK(pcontext, drMSG454);
                                 MSGN_CALLBACK(pcontext, drMSG451);
                                 returnCode = 3;
                             }
                         }
                     }
                     else if ((pdbshm->bithold) <= (ULONG)prl->rlsize) 
                     {
                         /* bi threshold reached */
                         /* bi threshold changed from value to value */
                         fromBitholdBytes =
                                    (DOUBLE)pdbshm->bithold * prl->rlbiblksize;
                         utConvertBytes(fromBitholdBytes,
                                        &fromBitholdSize, &fromSizeId);
                         sprintf((char *)fromPrintBuffer, "%-5.1f %cBytes",
                                                  fromBitholdSize, fromSizeId);
                         toBitholdBytes =
                                    (DOUBLE)bithreshold * prl->rlbiblksize;
                         utConvertBytes(toBitholdBytes,
                                        &toBitholdSize, &toSizeId);
                         sprintf((char *)toPrintBuffer, "%-5.1f %cBytes",
                                                toBitholdSize, toSizeId);
                         MSGN_CALLBACK(pcontext, drMSG686,
                                            fromPrintBuffer, toPrintBuffer);
                         
                         pdbshm->bithold = bithreshold;
                         pdbshm->quietpoint = QUIET_POINT_NORMAL;
                         returnCode = 0;
                     }
                     else
                     {
                         /* Invalid value provided, request to change
                            bi threshold rejected */
                         MSGN_CALLBACK(pcontext, drMSG454);
                         MSGN_CALLBACK(pcontext, drMSG451);
                         returnCode = 3;
                     }
                 }
                 else
                 {
                     /* Invalid value provided, request to change
                        bi threshold rejected */
                     MSGN_CALLBACK(pcontext, drMSG454);
                     MSGN_CALLBACK(pcontext, drMSG451);
                     returnCode = 3;
                 }
           }
        }
        else 
        /* The broker does not exist and shutdown may be in progress */
        {
            /* Broker is dead - abnormal shutdown of the database */
            MSGN_CALLBACK(pcontext, drMSG400);
            returnCode = 16;
        }

    return(returnCode);
}
