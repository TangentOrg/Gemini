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

/* PROGRAM: bkcrashtry - used to test PROGRESS recovery manager.         */
/*                    Once called, this program employs one of           */
/*                    two methods for determining whether or not         */
/*                    to crash.  The method is chosen by the start-up    */
/*                    options -Z n (meaning to crash on the n'th try) or */
/*                    -W n       (meaning to crash randomly)             */ 

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include <stdlib.h>
#include "dbconfig.h"
#include "dsmpub.h"
#include "dbcontext.h"
#include "bkcrash.h"
#include "latpub.h"
#include "rlpub.h"
#include "usrctl.h"
#include "utepub.h"

DSMVOID
bkCrashTry(dsmContext_t *pcontext, 
           TEXT	*where) 
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    crashtest_t *pcrashtest;
    usrctl_t    *pusr = pcontext->pusrctl;
    TEXT        pcrashmagic[BKCRASHNUMSIZE];
    int         crashVersion = 0;

    /* Verify magic crash magic number before granting access to function */
    if (!utgetenvwithsize((TEXT*)"BKCRASHNUMBER",
                          pcrashmagic,
                          BKCRASHNUMSIZE)) 
        crashVersion = atoi((psc_rtlchar_t *)pcrashmagic);

    if (crashVersion != BK_CRASHMAGIC)
    {
	  /* none of our business - just return */
          return;
    }

    /* find the crash structure for our user */
    if (pdbcontext->p1crashTestAnchor == NULL)
        /* return no crash structure allocated */
        return;

    pcrashtest = pdbcontext->p1crashTestAnchor + pusr->uc_usrnum;

    if ( pcrashtest->crash <= 0 )
	  /* none of our business - just return */
        return;		/*  */
  
    /* try to crash, keep track of number of tries */      

    pcrashtest->ntries++;
    if (pcrashtest->crash > 1)
    {
        /* indicates that crash should occur after "crash" times */

        if (pcrashtest->crash > pcrashtest->ntries) return;
        if (pcrashtest->crash <= pcrashtest->ntries) 
        {
           if (pcrashtest->crash < pcrashtest->ntries) 
           {
              /* Lets get out of here fast */
              MSGD_CALLBACK(pcontext,
                     (TEXT *)"%LCrash greater than ntries\n"); 
           } 
        }
        /* Lets get out of here fast */
        MSGD_CALLBACK(pcontext,
                     (TEXT *)"%BForced crash before %s %l\n", 
                      where, pcrashtest->ntries);

        if (pdbcontext->pdbpub)
            pdbcontext->pdbpub->shutdn = DB_EXBAD;

        exit(0);
    }
    if (pcrashtest->crash != 1) return;

    /* indicates Monte Carlo Method */

    srand(pcrashtest->cseed);
    pcrashtest->cseed = rand();

    if (pcrashtest->cseed >= (32726 * pcrashtest->probab)) return;

    /* going down, simulate hard crash */

    MSGD_CALLBACK(pcontext, 
                 (TEXT *)"%Bgoing down seed = %l ntries = %l caller = %s\n",
                 pcrashtest->cseed,pcrashtest->ntries, where);

    if (pdbcontext->pdbpub)
        pdbcontext->pdbpub->shutdn = DB_EXBAD;

    exit(0);
}	

/* Transaction oriented crash test */
DSMVOID
bkCrashTm (dsmContext_t *pcontext)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    crashtest_t *pcrashtest;
    usrctl_t    *pusr = pcontext->pusrctl;
    TEXT        pcrashmagic[BKCRASHNUMSIZE];
    int         crashVersion = 0;

    /* Verify magic crash magic number before granting access to function */
    if (!utgetenvwithsize((TEXT*)"BKCRASHNUMBER",
                          pcrashmagic,
                          BKCRASHNUMSIZE))
        crashVersion = atoi((psc_rtlchar_t *)pcrashmagic);

    if (crashVersion != BK_CRASHMAGIC)
    {
	  /* none of our business - just return */
          return;
    }

    /* find the crash structure for our user */
    if (pdbcontext->p1crashTestAnchor == NULL)
        /* return no crash structure allocated */
        return;

    pcrashtest = pdbcontext->p1crashTestAnchor + pusr->uc_usrnum;

    if (pcrashtest->crash >= 0)
	 /* It's for the other guy. */
         return;

    if (pcrashtest->crash == --pcrashtest->ntries)
    {
	MSGD_CALLBACK(pcontext,
                      (TEXT *)"%BForced crash before end of transaction %d\n",
	              -pcrashtest->ntries);

	rlbiflsh(pcontext, RL_FLUSH_ALL_BI_BUFFERS);

        if (pdbcontext->pdbpub)
            pdbcontext->pdbpub->shutdn = DB_EXBAD;

	exit(0);
    }
}

/* PROGRAM: bkBackoutExit - the purpose of this function is to provide   */
/*                        a method for the client to exit during the   */
/*                        first call to transaction backout.  This is  */
/*                        done to purposely test that the rest of the  */
/*                        client's backout process is detected and     */
/*                        and completed by other responsible processes */
DSMVOID
bkBackoutExit( dsmContext_t *pcontext,
               TEXT	*where,
               int     clientpid)
{
    dbcontext_t *pdbcontext = pcontext->pdbcontext;
    crashtest_t *pcrashtest;
    usrctl_t    *pusr = pcontext->pusrctl;
    TEXT        pcrashmagic[BKCRASHNUMSIZE];
    int         crashVersion = 0;

    /* Verify magic crash magic number before granting access to function */
    if (!utgetenvwithsize((TEXT*)"BKCRASHNUMBER",
                          pcrashmagic,
                          BKCRASHNUMSIZE))
        crashVersion = atoi((psc_rtlchar_t *)pcrashmagic);

    if (crashVersion != BK_CRASHMAGIC)
    {
	  /* none of our business - just return */
          return;
    }

    /* find the crash structure for our user */
    if (pdbcontext->p1crashTestAnchor == NULL)
        /* return no crash structure allocated */
        return;

    pcrashtest = pdbcontext->p1crashTestAnchor + pusr->uc_usrnum;

    if (pcrashtest->backout == 0)
	  /* none of our business - just return */
        return;		/*  */
  
    if (pcrashtest->backout == 1)
    {
        /* indicates that an early exit is occurring in the first call */ 
        /* to tmrej() */

        MSGD_CALLBACK(pcontext, 
                     (TEXT*) "%LForced exit before %s for pid %i\n", 
                     where, clientpid);

        exit(0);
    }
}
