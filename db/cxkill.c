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

#include "dbconfig.h"
#include "bkpub.h"
#include "dbpub.h"
#include "bmpub.h"  /* for bmReleaseBuffer call */
#include "cxmsg.h"
#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif
#include "lkpub.h"
#include "ixmsg.h"
#include "dbcontext.h"
#include "cxpub.h"
#include "cxprv.h"

#include "usrctl.h"  /* temp for DMSSETCONTEXT */
#include "utbpub.h"
#include "utspub.h"
#include "objblk.h"
#include "bkmgr.h"
#include "latpub.h"
#include "rlprv.h"
#include "rmprv.h"
#include "utmpub.h"
#include "geminikey.h"

/****************************************************************/
/*** cxKill - procedure to delete all the records of an index ***/
/***            file and finally delete the index file also   ***/
/*** cxunkil - procedure to do a logical backout of cxKill    ***/
/****************************************************************/

LOCAL latch_t extTreeListLatch;  /* process private latch for linked 
                                                  list routines */

/**** LOCAL FUNCTION PROTOTYPE DEFINITIIONS ****/

/*** this procedure has a recursive internal procedure ***/
LOCALF DSMVOID     cxKillKids      (dsmContext_t *pcontext, ULONG area,
                                 DBKEY mydbkey, COUNT ixnum,
                                 int delrecs, DBKEY rootdbk, int tableArea);

LOCALF TTINY *  cxFindDbKey     (dsmContext_t *pcontext, cxblock_t *pixblk,
                                 TTINY *pdbKey);

/**** END LOCAL FUNCTION PROTOTYPE DEFINITIONS ****/

/**********************************************************/
/* PROGRAM: cxKill - delete an index file & all its blocks
 *
 * RETURNS: DSMVOID
 */

DSMVOID
cxKill(
        dsmContext_t    *pcontext,
        dsmArea_t        area, /* storage area the index            */
        dsmDbkey_t       rootDbkey,  /* dbkey of root block of the index  */
        COUNT            ixnum,  /* index number of idx to be deleted */
        int              delrecs, /* 1 - delete data records 0 - don't */
                                 /* Only set to 1 for temp-tables   */
        dsmObject_t      tableNum,
        int              tableArea ) /* area number if delrecs==1 else 0 */
{
        IXFINOTE         note;

    TRACE_CB(pcontext, "cxKill")
#if SHMEMORY
    if (pcontext->pdbcontext->resyncing || lkservcon(pcontext))
        return; /* resyncing in progress */
#endif

    /* update delete status information in usrctl */
    pcontext->pdbcontext->pusrctl->uc_counter = 0;


    cxProcessDeleteChain( pcontext, tableNum, area );

    rlTXElock( pcontext,RL_TXE_UPDATE,RL_MISC_OP );

    /* kill the block and its kids recursively */
    cxKillKids(pcontext, area, rootDbkey, ixnum, delrecs, rootDbkey, tableArea);

    /* this last binote will trigger logical    */
    /* undo when reading bi file backwards      */
    INITNOTE( note, RL_IXKIL, RL_LOGBI );
    INIT_S_IXFINOTE_FILLERS( note );	/* bug 20000114-034 by dmeyer in v9.1b */

    note.ixArea = area;
    note.ixnum = ixnum;
    note.ixdbk = rootDbkey;
    note.ixunique = 0;	/* bug 20000114-034 by dmeyer in v9.1b */

    rlLogAndDo(pcontext, (RLNOTE *)&note, 0, 0, PNULL );


    rlTXEunlock( pcontext );
}

/**********************************************************************/
/* PROGRAM: cxKillKids - recursively kills kids and the the block itself
 *
 * RETURNS: DSMVOID
 */

/******************************************************************/
/*** cxKillKids - this routine is used recursively.  it is given        ***/
/***            a dbkey of an index block.  It kills all its    ***/
/***            kids, then the block itself                     ***/
/******************************************************************/

LOCAL DSMVOID
cxKillKids(
        dsmContext_t  *pcontext,
        ULONG          area,
        DBKEY          mydbkey,
        COUNT          ixnum,
        int            delrecs,
        DBKEY          rootdbk,
        int            tableArea) /* area number of the table */
{
    cxblock_t     *pixblk;
    bmBufHandle_t  ixBufHandle;
    COUNT          entloc;
    TEXT          *pixchars;
    COUNT          entlen;
    DBKEY          entdbk;

    /* update delete status information in usrctl */
    pcontext->pdbcontext->pusrctl->uc_counter += 1;


    /* read my block */
    ixBufHandle = bmLocateBlock(pcontext, area, mydbkey, TOMODIFY);
    pixblk = (cxblock_t *)bmGetBlockPointer(pcontext,ixBufHandle);
    /* if this block is not owned by the correct index,      */
    /* it was probably because an idxrebuild was interrupted */
    if (   pixblk->BKTYPE == IDXBLK &&
        (pixblk->IXNUM  == ixnum))
    {
        /* scan all its kids */
        if (!(pixblk->IXBOT))
        for(entloc=0; entloc < pixblk->IXLENENT; entloc+=entlen)
        {
            /* get the next kid's dbkey and read the block */
            pixchars = pixblk->ixchars + entloc;
/*            entlen       = ENTHDRSZ + pixchars[1] + pixchars[2]; */
            entlen       = ENTRY_SIZE_COMPRESSED(pixchars);
#ifdef VARIABLE_DBKEY
            cxGetDBK(pcontext, pixchars, &entdbk);
#else
            entdbk       = (DBKEY)xdbkey(pixchars+entlen-4);
#endif  /* VARIABLE_DBKEY */

            cxKillKids(pcontext, area, entdbk, ixnum,delrecs,rootdbk, tableArea);

        }
        
        /* now delete the parent */
        cxDeleteBlock (pcontext, ixBufHandle, 
                       (UCOUNT) (RL_PHY | RL_LOGBI) , rootdbk);
    }
    bmReleaseBuffer(pcontext, ixBufHandle); /* release the buffer by 
                                         decrementing in use count*/
}


/*********************************************************************/
/* PROGRAM: cxFindDbKey - Finds the index entry which matches the input
 *                      dbkey and returns a pointer to the info part
 *                      of the index entry.
 *
 * RETURNS: a pointer to the info part of the index entry.
 */
LOCALF TTINY *
cxFindDbKey(
        dsmContext_t *pcontext,
        cxblock_t    *pixblk,
        TTINY        *pdbKey)
{
    TTINY  *pos;
    TTINY  *pend;
/*    TTINY   is; */
    int     ks,is, hs, cs;
#ifdef VARIABLE_DBKEY
    TTINY  *posNxt;
    DBKEY   entdbk;
#endif  /* VARIABLE_DBKEY */

    if( pixblk->ix_hdr.ih_bot == 1 )
    {
        /* %GcxFindDbKey: called to search a leaf block */
        FATAL_MSGN_CALLBACK(pcontext, cxFTL008);
    }
    
    pos = pixblk->ixchars;
    pend = pixblk->ixchars + pixblk->ix_hdr.ih_lnent;
    
    while ( pos < pend )
    {
        /* position to the info part of the key.     */
/*        is = pos[2]; */
        CS_KS_IS_HS_GET(pos);

#ifdef VARIABLE_DBKEY
/*        posNxt = pos + pos[1] + is + ENTHDRSZ;  to next entry */ 
        posNxt = pos + hs + is + ks; /* to next entry */
        FASSERT_CB(pcontext, is == ( *(posNxt - 1 ) & 0x0f ),
                   "key info size is not equal to 4");

        cxGetDBK(pcontext, pos, &entdbk);
        pos = posNxt;
/*        if( bufcmp(&entdbk,pdbKey,sizeof(DBKEY)) == 0 ) */
	if(entdbk == xdbkey((TEXT *)pdbKey))
            return (pos - is);
#else
        FASSERT_CB(pcontext, is == sizeof(DBKEY),
                   "key info size is not equal to 4");

/*        pos += pos[1] + ENTHDRSZ; */
        pos += ks + hs;
        if( bufcmp(pos,pdbKey,sizeof(DBKEY)) == 0 )
            return pos;
        pos += is;
#endif  /* VARIABLE_DBKEY */

    }
    /* %GcxFindDbKey: didn't find dbkey */
    FATAL_MSGN_CALLBACK(pcontext, cxFTL012);

    return NULL;
}




typedef struct ixhdr *PIXHDR;  /* sorry, clyde */


struct cursent {DBKEY   entdbk;         /* dbk of blk at this lvl       */
                TTINY   olddbk[sizeof(DBKEY)];/* dbk of blk when killed*/
                COUNT   entnum;         /* entry num in this idx block  */
               };
typedef struct cursent CURSENT;

struct treeState{
                  TEXT     fixing;
                  COUNT    fixlevel;
                  CURSENT *curlevel;
                  CURSENT  fixpath[MAXCURSENT];
                  IXFINOTE note;
                  COUNT    indexNum;
                  struct treeState  *next;
              };


/************************************************************************/
/* PROGRAM: findTreeState : given the head of state list and the index number 
 *          being sought this function returns the corresponding state node.
 *
 * RETURNS: the state node being sought
  The calls to findTreeState must be protected by:

  PL_LOCK_TREELIST(&extTreeListLatch)
  PL_UNLK_TREELIST(&extTreeListLatch)

  to provide mutually exclusive access to the linked list 
  structure
*/

struct treeState* findTreeState( struct treeState *head, 
                                 int indexNum )
{
         struct treeState* temp = head;

         if ( head == NULL )
         {
		return NULL;
         }

         while( temp != NULL )
         {
               if( temp->indexNum == indexNum )
               {
		return temp;
               }
               temp = temp->next;
         }
         return NULL;
}


/************************************************************************/
/* PROGRAM: addTreeState: add a state node to the linked list of states.
 *          
 *
 * RETURNS: the new list head
  The calls to addTreeState must be protected by:

  PL_LOCK_TREELIST(&extTreeListLatch)
  PL_UNLK_TREELIST(&extTreeListLatch)

  to provide mutually exclusive access to the linked list 
  structure
*/

struct treeState*  addTreeState( 
                                 struct treeState *head, 
                                 struct treeState *stateToAdd )
{
         stateToAdd->next = head;
         head = stateToAdd;
         return head;
}

/************************************************************************/
/* PROGRAM: deleteTreeState: add a state node to the linked list of states.
 *          
 *
 * RETURNS: the new list head
  The calls to deleteTreeState must be protected by:

  PL_LOCK_TREELIST(&extTreeListLatch)
  PL_UNLK_TREELIST(&extTreeListLatch)

  to provide mutually exclusive access to the linked list 
  structure
*/

struct treeState* deleteTreeState( 
                                   struct treeState *head,
                                   struct treeState **ppstateToDelete)
{
         struct treeState *stateToDelete = *ppstateToDelete;
         struct treeState *temp =head;
         struct treeState *temp1 = NULL;

         if ( temp == NULL ) 
         {
               /* printf("This can't be happening \n");*/
               return NULL;
         }

         if ( temp == stateToDelete)
         {
              head = head->next;
              utfree((TEXT*)temp);
              *ppstateToDelete = NULL;
              return head;
         }

         while( temp->next != NULL )
         {
             if ( temp->next == stateToDelete) 
             {
                 temp1 = temp->next;
                 temp->next = temp1->next;
                 utfree((TEXT*)temp1);
                 *ppstateToDelete = NULL;
                 return head;
             } 
         }
         return head;
}


/************************************************************************/
/* PROGRAM: getNthDBkey: add a state node to the linked list of states.
 *
 *
 * RETURNS: the dbkey of the nth entry in the index block
 *          0 if an error occurs
*/


DBKEY getNthDBkey( cxblock_t  *pblk, int n )
{

  int    entloc;
  int    entlen;
  DBKEY  entdbk;
  TEXT*  pixchars;
  int    i=1;

        for(entloc=0; entloc < pblk->IXLENENT; entloc+=entlen)
        {
            /* get the next kid's dbkey */
            pixchars = pblk->ixchars + entloc;
/*            entlen       = ENTHDRSZ + pixchars[1] + pixchars[2]; */
            entlen       = ENTRY_SIZE_COMPRESSED(pixchars);
#ifdef VARIABLE_DBKEY
            /* get the dbkey from the INFO area */
	    
	    /*gemDBKEYfromIdx( pixchars + *(pixchars+1) + ENTHDRSZ, &entdbk ); */
            gemDBKEYfromIdx( pixchars + KS_GET(pixchars) + HS_GET(pixchars),
			     &entdbk );
#else
            entdbk       = (DBKEY)xlng(pixchars+entlen-4);
#endif  /* VARIABLE_DBKEY */
            if( i == n ) return entdbk;
            i++;
        }
        return 0;
}



/************************************************************************/
/* PROGRAM: insertBlock: insert the index block into the index being const-
 * ructed.
 *
 *
 * RETURNS: 0 upon success
 *          1 upon failure
*/


int
insertBlock(
             dsmContext_t    *pcontext,
             RLNOTE          *pnote,
             COUNT            dlen,
             TEXT            *pdata,
             cxblock_t       *pblk,
             struct treeState       *state)
{

  /* look at the children of the block passed in
     to see if any of those has the dbkey: pnote->rldbk
     if so then allocate a new block and insert and 
     update it's parent with the new dbkey
     perhaps assumes that the index lies wholly within an area
     Also, this algo's working is closely tied with that of cxKill,
     The order of deleteing in cxKill ( left to right in a block )
     has to be reverse of the order of undelete in this algo*/

  int         i=0;
  DBKEY       tempdbkey;
  DBKEY       tmpdbk1;
  cxblock_t   *ptmpblk;
  cxblock_t   *pixblk;
  bmBufHandle_t bufHandle;
  TEXT*       pos;
  ULONG       area;


  state->fixlevel++;
  state->curlevel++;
  state->curlevel->entdbk = pblk->BKDBKEY;
  bufcop(state->curlevel->olddbk,  &(pblk->BKDBKEY), sizeof(DBKEY));
  state->curlevel->entnum = (pblk->ix_hdr).ih_nment;



  if ( ((PIXHDR)pdata)->ih_top == 1)
  {
    /* this means that the top node of the tree is to be undeleted  
       so, just undelete it also remember that if the top node is being
       undeleted and gets a new dbkey then the rootdbk available in notes 
       is not to be used any further*/

    /* if its the root block do not release the buffer lock it will get
       released when the index unkill is complete*/

       area = xlng((TEXT *)&pnote->rlArea);
       bufHandle = bkfrrem(pcontext, area,IDXBLK, FREECHN);
       pixblk = (cxblock_t *)bmGetBlockPointer(pcontext,bufHandle);
       /* if there is a new root then the old dbkey is distinct
          from the new one and the new one needs to be set */
       state->curlevel->entdbk = pixblk->BKDBKEY;
       pnote->rlcode = RL_IXIBLK;
       sct((TEXT *)&pnote->rlflgtrn,  (COUNT)(RL_PHY));
       rlLogAndDo (pcontext, pnote, bufHandle, dlen, pdata );
       if( pcontext->pdbcontext->prlctl->rlwarmstrt ) 
           bmReleaseBuffer(pcontext, bufHandle);
       return 0;
  }

  for(i=0; i< (pblk->ix_hdr).ih_nment; i++)
  {
        state->curlevel->entnum--;

	tempdbkey = getNthDBkey( pblk, (pblk->ix_hdr).ih_nment - i );
        if ( tempdbkey == (DBKEY)xdbkey((TEXT *)&pnote->rldbk) )
        {

           /* fix the block with tempdbkey and fix the parent: pblk */
           /* get a block to load the index fragment into */
           area = xlng((TEXT *)&pnote->rlArea);

           bufHandle = bkfrrem(pcontext, area,IDXBLK, FREECHN);
           pixblk = (cxblock_t *)bmGetBlockPointer(pcontext,bufHandle);

           pnote->rlcode = RL_IXIBLK;
           sct((TEXT *)&pnote->rlflgtrn,  (COUNT)(RL_PHY));
           rlLogAndDo (pcontext, pnote, bufHandle, dlen, pdata );
          

           if( ((PIXHDR)pdata)->ih_bot != 1 )
           {
                  state->fixlevel++;
                  state->curlevel++;
                  state->curlevel->entdbk = pixblk->BKDBKEY;
                  bufcop(state->curlevel->olddbk,  
                         (TEXT *)&pnote->rldbk, sizeof(DBKEY));
                  state->curlevel->entnum = ((PIXHDR)pdata)->ih_nment;
           }
           else
           {
               if( state->curlevel->entnum == 0 )
               {
                 do
                   if ( state->fixlevel == 1 )
                     /* We are now at the level of the master block
                        and should terminate the whole process.
                     */
                     {
                       cxIndexAnchorUpdate( pcontext, 
                                            area,
                                            ((PIXHDR)pdata)->ih_ixnum,
                                            DSMOBJECT_MIXINDEX,
                                            0,
                                            0,
                                            pixblk->BKDBKEY,
                                            DSM_OBJECT_UPDATE_ROOT);
                       bmReleaseBuffer( pcontext,
                                        bmFindBuffer(pcontext,
                                                     area,
                                                     state->curlevel->entdbk));
                       state->fixing = FALSE;
                       break;
                     }
                 while (state->fixlevel--, (--state->curlevel)->entnum == 0 );
               }
           }
               
           /* the following buffer release is being left out as a comment to 
              remind that the buffer corresponding to the first insertion 
              is not released untill the unkill is complete so that no other
              readers during online index unkill can access this index block
              and try to traverse down it ??????????
              THIS ISSUE NEEDS TO BE REVISITED */
           bmReleaseBuffer(pcontext, bufHandle);
           /* change the dbkey in the parent block of the block added */
	   sdbkey((TEXT *)(&tmpdbk1),tempdbkey);
           pos = cxFindDbKey(pcontext, pblk, (TTINY *)&tmpdbk1 );

           /* replace the dbk in the parent */
           sdbkey((TEXT *)(&tmpdbk1), pixblk->BKDBKEY); /*new son's dbk*/

           bufHandle =  bmLocateBlock(pcontext, xlng((TEXT *)&pnote->rlArea),
                          pblk->BKDBKEY, TOMODIFY );

           bkReplace(pcontext, bufHandle, pos, 
                     (TEXT *)&tmpdbk1, sizeof(DBKEY) );

           bmReleaseBuffer(pcontext, bufHandle); 

           return 0;
        }
        bufHandle =  bmLocateBlock(pcontext, xlng((TEXT *)&pnote->rlArea),
                                   tempdbkey, TOMODIFY );
        ptmpblk =  (cxblock_t *)bmGetBlockPointer(pcontext,bufHandle);
        if ( !insertBlock( pcontext, pnote, dlen, pdata, ptmpblk,state ) )
        {
            bmReleaseBuffer(pcontext, bufHandle);
            return 0;
        }
        bmReleaseBuffer(pcontext, bufHandle);
  }
  return 1;
}


/************************************************************************/
/* PROGRAM: insertFirstBlockToIndex: insert one block to the index.
 * 
 * RETURNS:  DSMVOID
*/


DSMVOID
insertFirstBlockToIndex(
                        dsmContext_t    *pcontext,
                        RLNOTE          *pnote,
                        COUNT            dlen,
                        TEXT            *pdata,
                        struct treeState       *state)
{
    cxblock_t     *ptmpblk;
    bmBufHandle_t bufHandle;
    dsmDbkey_t    rootdbk;    
    ULONG         area;


   /* Algorithm: preorder right to left traversal of the B-tree
      is needed to insert the block into the B-tree 
      details:
      1. get the index root from the index number
      2. call the insert algorithm on the root node with the blk
         sent in. */

   area = xlng((TEXT *)&pnote->rlArea);
   rootdbk = ((IXDBNOTE *)pnote)->rootdbk;
   bufHandle =  bmLocateBlock(pcontext, area, rootdbk, TOMODIFY );
   ptmpblk =  (cxblock_t *)bmGetBlockPointer(pcontext,bufHandle);
   insertBlock( pcontext, pnote, dlen, pdata, ptmpblk, state);
   bmReleaseBuffer(pcontext, bufHandle);

}

/************************************************************************/
/* PROGRAM: cxUnKill -- undo an cxIxKill, crash recovery or dynamic backout
 *
 * RETURNS: DSMVOID
 */

DSMVOID
cxUnKill(
        dsmContext_t    *pcontext,
        RLNOTE          *pnote,
        COUNT            dlen,
        TEXT            *pdata)
{
static struct treeState *treeListHead =NULL;
struct treeState *state;
struct treeState *temp;


        cxblock_t   *pixblk;
        cxblock_t   *ptmpblk;
        DBKEY    tmpdbkey;
        TEXT    *pos;
        bmBufHandle_t bufHandle;
        ULONG    area;

   if ( pnote->rlcode != RL_IXUKL && pnote->rlcode != RL_IXIBLK )
   {
      if ( pnote->rlcode != RL_IXKIL )
      {

        PL_LOCK_TREELIST(&extTreeListLatch)
        temp = findTreeState( treeListHead, 
                              ((PIXHDR)pdata)->ih_ixnum );
        PL_UNLK_TREELIST(&extTreeListLatch)
        if ( temp != NULL )
        {
           state = temp;
        }
        else
        {
             /* If this is the first block of this index , this can 
                happen only in case of a partial delete of the index tree */
             state = (struct treeState*)utmalloc( sizeof( struct treeState ));
             state->fixing = TRUE;
             state->next = NULL;
             state->fixlevel = 0;
             state->curlevel = state->fixpath - 1;
             state->indexNum = ((PIXHDR)pdata)->ih_ixnum ;
             /********************************************************
             the state path created in this manner lacks the note info
             for each state but since the cxUnKill code doesn't rely on 
             the notes stored down the path this shouldn't be a problem
             ********************************************************/
             PL_LOCK_TREELIST(&extTreeListLatch)
             treeListHead = addTreeState(treeListHead, state );
             PL_UNLK_TREELIST(&extTreeListLatch)
	     insertFirstBlockToIndex( pcontext, pnote, dlen, pdata, state );
             return;
        }
      }
      else
      {
          /* undeleting a new index and one that was completely deleted*/
          state = (struct treeState*) utmalloc( sizeof( struct treeState ));
          state->next = NULL;
          if ( pnote->rlcode == RL_IXKIL )
              state->indexNum = ((IXFINOTE *)pnote)->ixnum ;
          else
              /* this will cause SEGV */
              state->indexNum = ((PIXHDR)pdata)->ih_ixnum ; 
      }
   }
 

switch ( pnote->rlcode )
  {
  case RL_IXUKL:   /* roll forward, same here */
    pnote->rlcode = RL_IXKIL;
    rlLogAndDo(pcontext, (RLNOTE *)pnote, 0, 0, PNULL );

    break;

  case RL_IXKIL:   /* roll back, possibly we should trap and verify */

/*    if (state->fixing)
        FATAL_MSGN_CALLBACK(pcontext, ixFTL011);
*/
    /* Setup to unkill an index */

    rlTXElock(pcontext,RL_TXE_UPDATE,RL_MISC_OP);

    PL_LOCK_TREELIST(&extTreeListLatch)
    treeListHead = addTreeState(treeListHead, state );
    PL_UNLK_TREELIST(&extTreeListLatch)

    state->fixing = TRUE;
    state->fixlevel = 0;
    state->curlevel = state->fixpath - 1;
    bufcop ( &state->note, pnote, sizeof( state->note ));


    /* turn the note around */
    pnote->rlcode = RL_IXUKL;
    rlLogAndDo(pcontext, (RLNOTE *)pnote, 0, 0, PNULL );

    break;

  case RL_IXIBLK:    /* roll forward, RL_PHY */
  /* 
       Since we are deleting an undeleted block, the note
       corresponding to the undelete operation must have
       the rootdbkey in it, since it was the same note
       used for deleting, undeleting with only the note 
       type changed.
  */

    /* get a block to load the index fragment into */
    area = xlng((TEXT *)&pnote->rlArea);

    bufHandle =  bmLocateBlock(pcontext, area, 
                               (DBKEY)xdbkey((TEXT*)&pnote->rldbk), TOMODIFY );

    ptmpblk =  (cxblock_t *)bmGetBlockPointer(pcontext,bufHandle);
    cxDeleteBlock (pcontext, bufHandle, 
                   (UCOUNT) (RL_PHY | RL_LOGBI) , 
                   ((IXDBNOTE *)pnote)->rootdbk);

    bmReleaseBuffer(pcontext, bufHandle); /* release the buffer by
                                         decrementing in use count*/
    break;

  case RL_IXDBLK:    /* roll back, these are somewhat self verifying as
                        the only get written RL_PHY|RL_LOGBI */
    if (!state->fixing)
        FATAL_MSGN_CALLBACK(pcontext, ixFTL010);
    /* Wheee... Here we go. Do undelete processing */

    /* Being a belt+suspenders kind of guy i'm going to verify
       that the first block that comes back is the master */

    /* get a block to load the index fragment into */
    area = xlng((TEXT *)&pnote->rlArea);

    bufHandle = bkfrrem(pcontext, area,IDXBLK, FREECHN);
    pixblk = (cxblock_t *)bmGetBlockPointer(pcontext,bufHandle);
    state->fixlevel++;
    state->curlevel++;
    state->curlevel->entdbk = pixblk->BKDBKEY;
    bufcop(state->curlevel->olddbk,  (TEXT *)&pnote->rldbk, sizeof(DBKEY));
    state->curlevel->entnum = ((PIXHDR)pdata)->ih_nment;

    if ( state->fixlevel == 1 && ((PIXHDR)pdata)->ih_top != 1 )
      FATAL_MSGN_CALLBACK(pcontext, ixFTL002);
    /* Fudge up the note to undo delete and DO it */
    pnote->rlcode = RL_IXIBLK;
    sct((TEXT *)&pnote->rlflgtrn,  (COUNT)(RL_PHY));
    rlLogAndDo (pcontext, pnote, bufHandle, dlen, pdata );


    /* if its the root block do not release the buffer lock it will get
       released when the index unkill is complete*/

    /* if this is warmstart (crash recovery) then the buffer 
       has to be released each time */
    if ( ((PIXHDR)pdata)->ih_top != 1  ||
         pcontext->pdbcontext->prlctl->rlwarmstrt )
       bmReleaseBuffer(pcontext, bufHandle); /* release the buffer by
                                                decrementing in use count*/

    /* If this isn't the index master block then we have to fix up
       the dbkey in the parent.  Otherwise it gets fixed in the
       index anchor table */
    if ( ((PIXHDR)pdata)->ih_top != 1 )
      {
        CURSENT *tmp = state->curlevel-1;
        bufHandle =  bmLocateBlock(pcontext, area, tmp->entdbk, TOMODIFY );
        ptmpblk =  (cxblock_t *)bmGetBlockPointer(pcontext,bufHandle);

        pos = cxFindDbKey(pcontext, ptmpblk,state->curlevel->olddbk);
        
        /* replace the dbk in the parrent */
        sdbkey((TEXT *)(&tmpdbkey), state->curlevel->entdbk); /* new son's dbk */

        bkReplace(pcontext, bufHandle, pos, (TEXT *)&tmpdbkey, sizeof(DBKEY) );

        tmp->entnum--;
        bmReleaseBuffer(pcontext, bufHandle); /* release the buffer by
                                              decrementing in use count*/
      }


    /* If the current level points to a leaf block or an interior block
       that for which al chldren have been processed then pop a level.
       If we pop the master then we must be done. */
    if ( ((PIXHDR)pdata)->ih_bot == 1 )  /* If at leaf start popping */
      {
        do
          if ( state->fixlevel == 1 )
            /* We are now at the level of the master block
               and should terminate the whole process.
            */
            {

              /* this buffer is not locked in crash recovery */
              if ( !pcontext->pdbcontext->prlctl->rlwarmstrt )
                  bmReleaseBuffer( pcontext,
                                 bmFindBuffer( pcontext,
                                               area,
                                               state->curlevel->entdbk));

              rlTXEunlock(pcontext);
              /* update the index anchor here */
              cxIndexAnchorUpdate( pcontext,
                                   area,
                                   ((PIXHDR)pdata)->ih_ixnum,
                                   DSMOBJECT_MIXINDEX,
                                   0,
                                   0,
                                   state->curlevel->entdbk,
                                   DSM_OBJECT_UPDATE_ROOT);


              PL_LOCK_TREELIST(&extTreeListLatch)
              treeListHead = deleteTreeState(treeListHead, &state );
              PL_UNLK_TREELIST(&extTreeListLatch)

              /* NOTE: Its unclear whether state->fixing is necessary ? */
              if (state)
                  state->fixing = FALSE;
              break;
            }
        while (state->fixlevel--, (--state->curlevel)->entnum == 0 );
      } /* end if leaf block */
    break;

  default:
    FATAL_MSGN_CALLBACK(pcontext, ixFTL012, pnote->rlcode);
  } /* end case */
}
