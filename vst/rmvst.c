
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

#include "dbconfig.h"	/* standard dbmgr header */
#include "rmpub.h"	/* public interface */
#include "rmprv.h"	/* private interface */
#include "rmmsg.h"	/* messages */

#include "recpub.h"
#include "dbmgr.h"   /* needed for LKTBFULL & RECTOOBIG */
#include "dbcontext.h"   /* needed for p_dbctl & rm[*]Ct & DBSERVER */
#include "dsmpub.h"
#include "scasa.h"      /* TODO - where is correct place for these */
#include "stmpub.h"
#include "stmprv.h"      /* needed for GLOBAL STPOOL *pgstpool */
#include "vstpub.h"
#include "usrctl.h"  /* needed for pusrctl */


/*** LOCAL FUNCTION PROTOTYPES ***/

LOCALF LONG rmValidVirtual(COUNT table);
LOCALF LONG rmVerifyTableNumber(dsmContext_t *pcontext,
				COUNT table, TEXT *pRecord);

/*** END LOCAL FUNCTION PROTOTYPES ***/

#if RMTRACE
#define MSGD    MSGD_CALLBACK
#endif

/* PROGRAM: rmCreateVirtual - creates a new virtual record
 *
 * Creates a virtual record for those virtual tables that allow the operation.
 * Used to initiate or initialize some function on a server.  The dbkey
 * returned can be used to further update or delete the record.
 *
 * The virtual table number is taken from the provided record buffer.
 *
 * RETURNS:   status code received from ????
 *	      DBKEY of new record created.
 *	      RMNOTFND if invalid table number
 */
DBKEY
rmCreateVirtual (
	dsmContext_t	*pcontext,
	COUNT		 table,		/* table number */
	TEXT		*pRecord,	/* record template */
	COUNT		 size)		/* size of template */
{
    DBKEY	ret;

#ifdef VST_ENABLED
    if (rmValidVirtual(table))
    {
	/* perform the VST create operation */
#if RMTRACE
	MSGD_CALLBACK(pcontext,
                     "%:rmCreateVirtual: Create VST record, table=%d",table);
#endif

        ret = vstCreate(pcontext, table, pRecord, size);
    }
    else
#endif  /* VST_ENABLED */
    {
	/* invalid request */
	ret = RMNOTFND;
    }
	
    return ret;

}  /* end rmCreateVirtual */


/* PROGRAM: rmDeleteVirtual - delete a virtual record
 *
 * RETURNS:	status from ???
 *		RMOK if successful
 *	        RMNOTFND if the record is not found
 *	   	RMNOTFND if invalid table number
 */
int
rmDeleteVirtual (
	dsmContext_t	*pcontext,
	COUNT		 table, /* table number */
	DBKEY		 dbkey)	/* record dbkey */
{
    LONG ret;

#ifdef VST_ENABLED
    /* validate that record is from a virtual table */
    if (rmValidVirtual(table))
    {
#if RMTRACE
    	MSGD(pcontext,
        "%BrmDeleteVirtual: Delete VST record table=%d dbkey=%D", table, dbkey);
#endif
        ret = vstDelete(pcontext, table, dbkey);

    }
    else
#endif  /* VST_ENABLED */
    {
	/* invalid VST request */
	ret = RMNOTFND;
    }

    return ret;
    
}  /* end rmDeleteVirtual */


/* PROGRAM: rmFetchVirtual - retrieve a virtual record
 *
 *	driver to dispatch requests for virtual records.
 *
 * RETURNS: recsize if successful
 *	    RMNOTFND if record not found
 *	    RMNOTFND if invalid table number
 */
int
rmFetchVirtual (
	dsmContext_t	*pcontext,
	COUNT		 table,         /* virtual table number */
        DBKEY		 dbkey, 	/* dbkey of record */
	TEXT		*pRecord,	/* target record buffer */
	COUNT		 size)		/* target record buffer size */
{
    LONG ret;

#ifdef VST_ENABLED
    if (rmValidVirtual(table))
    {
#if RMTRACE
    	MSGD(pcontext, "%LrmFetchVirtual: Fetch VST record. dbkey=%D", dbkey);
#endif

	ret = vstFetch(pcontext, table, dbkey, pRecord, size);
	
        /* If the record fit in the alloted size, then the 
         * table number MUST correspond.
         */
	if ((ret < size) && (!rmVerifyTableNumber(pcontext, table, pRecord)))
	{
	    /* table number did not match */
	    ret = RMNOTFND;
	}
			  
    }
    else
#endif  /* VST_ENABLED */
    {	
	/* invalid VST request */
	ret = RMNOTFND;
    }

    if (ret >= 0)
    {
	/* TODO: STATADD (rmvbrCt, (ULONG)(ret)); */
	/* TODO STATINC (rmvfrCt); */
    }
    
    return ret;
    
}  /* end rmFetchVirtual */


/* PROGRAM: rmUpdateVirtual - updates a virtual record
 *
 * Used to communicate changes to database server or utilities.  The table is
 * determined from the contents of the record.
 *
 * RETURNS: RMOK if successful
 *          RMNOTFND if can't find the record or not updatable
 *	    or error status received if ????
 */
int
rmUpdateVirtual (
	dsmContext_t	*pcontext,
	COUNT		 table,		/* table of record */
	DBKEY		 dbkey,		/* dbkey of record */
	TEXT		*pRecord,	/* record buffer */
	COUNT		 size)		/* size of record */
{
    LONG    ret;

    TRACE2_CB(pcontext, "rmUpdateVirtual: dbkey=%D, size=%d", dbkey, size)

#ifdef VST_ENABLED
    if (rmValidVirtual(table) && rmVerifyTableNumber(pcontext, table, pRecord))
    {
#if RMTRACE
	MSGD(pcontext,
                   "%LrmUpdateVirtual: Update VST table=%d, dbkey=%D, size=%d",
                   table, dbkey, size);
#endif

	ret = vstUpdate(pcontext, table, dbkey, pRecord, size);

    }
    else
#endif  /* VST_ENABLED */
    {
	/* not a supported VST */
	ret = RMNOTFND;
    }
    
    /* *** TODO: STATINC (rmvrpCt); */
    return ret;

}  /* end rmUpdateVirtual */


/* PROGRAM: rmValidVirtual - validate virtual table number
 *
 * RETURNS:   1 if valid, 0 if not valid
 */
LOCALF LONG
rmValidVirtual (
	COUNT table) /* table number */
{
    /* validate table requested, return if unexpected number */
    if (table > SCTBL_VST_FIRST || table < SCTBL_VST_LAST)
    {
	return 0;
    }

    return 1;

}  /* end rmValidVirtual */


/* PROGRAM: rmVerifyTableNumber - validate table number in a record
 *
 * RETURNS:   1 if valid, 0 if not valid
 */
LOCALF LONG
rmVerifyTableNumber (
	dsmContext_t	*pcontext _UNUSED_,
	COUNT		 table,   /* expected table number */
	TEXT		*pRecord)  /* record buffer */
{
    COUNT tableFound;
    
    tableFound = recGetTableNum(pRecord);
    
    /* they must match */
    if (table != tableFound)
    {
#if RMTRACE
	MSGD(pcontext, 
  "%LrmVerifyTableNumber: Invalid table number in record, found %d expected %d",
	     tableFound, table);
#endif
	return 0;
    }

    return 1;
    
}  /* end rmVerifyTableNumber */
