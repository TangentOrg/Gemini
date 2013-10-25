
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


/*#include "collate.h"*/
#include "cxpub.h"  /* for cxkyut routines */
#include "ditem.h"
#include "keypub.h"
#include "keyprv.h"
#include "kypub.h"
#include "utcpub.h"   /* dblank interface */
#include "utdpub.h"
#include "utspub.h"

/*** NOTE: The following needs to be resolved with the internationalization
 *         group.
 */
#define CODE_PAGE_SIZE 20
#define MIN_TABLE_DATA_SIZE 256


/* used to store collation table data from binary file .cp */
struct collTableData {
        int collTableSize;
        int collTranVersion;
        TEXT collCodePage[CODE_PAGE_SIZE];
        TEXT collName[CODE_PAGE_SIZE];
        TEXT collCaseTableData[MIN_TABLE_DATA_SIZE];
        TEXT collCaseInTableData[MIN_TABLE_DATA_SIZE];
        struct collTableData *pNext;
};
/********************************************************/

/**** LOCAL FUNCTION PROTOTYPES ****/

#if 0
LOCALF DSMVOID      keyGetBig         (TEXT *pt, COUNT len, svcBig_t *pb);
LOCALF DSMVOID      keyGetDecimal     (TEXT *pt, COUNT len, svcDecimal_t *pd);
LOCALF DSMVOID      keyGetDouble      ( TEXT         *pt
                                   , COUNT         len
                                   , svcDouble_t  *pd
                                   );
LOCALF svcLong_t keyGetLong        ( TEXT *pt, COUNT len);
LOCALF DSMVOID      keyPutBig         ( struct ditem *pditem, svcBig_t *pb);
#endif

LOCALF int       keyPutBinary      ( struct ditem    *pditem
                                   , svcByteString_t *pb);
LOCALF DSMVOID      keyPutBool        ( struct ditem *pditem, svcBool_t b);
LOCALF int       keyPutDecimal     ( struct ditem *pditem, svcDecimal_t *pd);
LOCALF DSMVOID      keyPutDouble      ( struct ditem *pditem, svcDouble_t   dbl);
LOCALF DSMVOID      keyPutLong        ( struct ditem *pditem, svcLong_t l);
LOCALF int       keyPutText        ( struct ditem    *pditem
                                   , svcByteString_t *pb);

LOCALF TEXT       *sortdec  (TEXT *pt, TTINY *ps, TTINY len);

/**** END LOCAL FUNCTION PROTOTYPES ****/

/* KEYSIZE is 200.  This code can allow a larger size for the ditems.   */
/* #define MYMAXKEYSIZE    250                                          */
/* No, it can't! kyinsdb checks for KEYSIZE ... (th)                    */


/***************************************************************************/
/***                   KeyServices External Functions                    ***/
/***************************************************************************/

/***************************************************************************/
/* PROGRAM: keyBracketBuild - Build the two keys of a bracket, the Base key
 *                     and the Limit Key. 
 *
 * RETURNS: 
 *      KEY_SUCCESS             dsmKey structure contains keystring 
 *      KEY_BAD_COMPONENT       one or more components bad -> check indicators
 *      KEY_BAD_PARAMETER       pKey or pComponent = NULL, or pKey-keycomps <= 0
 *      KEY_BUFFER_TOO_SMALL    maxKeyLength is too small to hold key
 *      KEY_EXCEEDS_MAX_SIZE     generated key-string is bigger than KEYSIZE
 *      KEY_BAD_COMPONENT       invalid key component 
 *      KEY_BAD_COMPARE_OPERATOR     invalid compare operator
 *
 *      Given the components of an index key and a comparison operator, or   
 *      given components of two keys and a "between" operator, 
 *      the two baracket keys are constructed. The constructed keys may have
 *      different number of components that the original keys.    
 *      The comparison operator may be one of the following:       
 *           KEY_ALL     Used to scan the whole index, will get all entries
 *           KEY_EQ      Searched keys must be equal to the given key     
 *           KEY_GT      Searched keys must be greater than the given key
 *           KEY_GE      Searched keys must be greater or equal to the given key
 *           KEY_LE      Searched keys must be less than the given key
 *           KEY_LT      Searched keys must be less or equal to the given key
 *           KEY_BEGINS  Searched keys must begine with the  given key
 *           KEY_BETWEEN_GT_LT   Searched keys must be greater than the 1st
 *                               given key and less than the 2nd given key
 *           KEY_BETWEEN_GE_LT   Searched keys must be greater or equal to 1st
 *                               given key and less than the 2nd given key
 *           KEY_BETWEEN_GT_LE   Searched keys must be greater than the 1st
 *                               given key and less or equal to the 2nd key
 *           KEY_BETWEEN_GE_LE   Searched keys must be greater or equal to 1st
 *                               given key and less or equal to the 2nd key
 */
keyStatus_t
keyBracketBuild(
       COUNT          keyMode,          /* IN */
       dsmIndex_t     index,            /* IN */
       COUNT          numberComponents, /* IN - Number of componets in array */
       COUNT          keyOperator,      /* IN - KEY_BETWEEN_GT_LT, etc. */
       svcDataType_t *pComponent1[],    /* IN - Components of the key */
       svcDataType_t *pComponent2[],    /* IN - 2. Key for "between" queries */
       COUNT          maxBaseKeyLength, /* IN */
       dsmKey_t      *pBaseKey,         /* OUT */
       COUNT          maxLimitKeyLength,/* IN */
       dsmKey_t      *pLimitKey)        /* OUT */
{

       svcDataType_t     lowRange;
       svcDataType_t     highRange;
       svcDataType_t    *pLowRange[1];
       svcDataType_t    *pHighRange[1];
       svcDataType_t    *pAuxComponent1[17];
       svcDataType_t    *pAuxComponent2[17];
       svcDataType_t   **ppBaseComponent;
       svcDataType_t   **ppLimitComponent;
       svcDataType_t   **psave;
       keyStatus_t       rc;
       COUNT             i;

       /* Initialize array entry with a variable */
       pLowRange[0]  =  &lowRange;
       pHighRange[0] =  &highRange;

	lowRange.indicator    = SVCLOWRANGE;
	highRange.indicator   = SVCHIGHRANGE;
        pLimitKey->index      = index;
        pBaseKey->index       = index;
        pLimitKey->ksubstr    = 0;
        pBaseKey->ksubstr     = 0;
        pLimitKey->keycomps   = numberComponents;
        pBaseKey->keycomps    = numberComponents;

    /* if bracket limited on one side only - NO between operator */
    if (keyOperator < SMALLEST_BETWEEN_OP)
    {
        /* check if last component is descending */
        if (pComponent1[numberComponents-1]->indicator & SVCDESCENDING)
            switch(keyOperator)
            {
            /* reverse the operators for descending */
            case KEY_GE:
                keyOperator = KEY_LE;
                break;
            case KEY_LE:
                keyOperator = KEY_GE;
                break;
            case KEY_GT:
                keyOperator = KEY_LT;
                break;
            case KEY_LT:
                keyOperator = KEY_GT;
                break;
            default:
                break;
            }
        switch(keyOperator)
        {
        case KEY_ALL:
	    /* Base is a one component LowRange */
            ppBaseComponent    = pLowRange;
            pBaseKey->keycomps = 1;

	    /* Limit is a one component HighRange */
            ppLimitComponent    = pHighRange;
            pLimitKey->keycomps = 1;

            break;

        case KEY_EQ:
	    /* Base is the given key */
            ppBaseComponent = pComponent1;

            /* Limit is the given key + HighRange */
            for (i=0; i < numberComponents; i++)
		pAuxComponent1[i] = pComponent1[i];
            pAuxComponent1[i]   = pHighRange[0];
            ppLimitComponent    = pAuxComponent1;
            pLimitKey->keycomps = numberComponents + 1;

            break;

        case KEY_GE:
	    /* Base is the given key */
            ppBaseComponent = pComponent1;

	    /* Limit is a one component HighRange */
            ppLimitComponent    = pHighRange;
            pLimitKey->keycomps = 1;

            break;

        case KEY_GT:
	    /* Base is the given key + HighRange */
            for (i=0; i < numberComponents; i++)
		pAuxComponent1[i] = pComponent1[i];
            pAuxComponent1[i]  = pHighRange[0];
            ppBaseComponent    = pAuxComponent1;
            pBaseKey->keycomps = numberComponents + 1;

	    /* Limit is a one component HighRange */
            ppLimitComponent    = pHighRange;
            pLimitKey->keycomps = 1;

            break;

        case KEY_LE:
	    /* Base is a one component LowRange */
            ppBaseComponent    = pLowRange;
            pBaseKey->keycomps = 1;

            /* Limit is the given key + HighRange */
            for (i=0; i < numberComponents; i++)
		pAuxComponent1[i] = pComponent1[i];
            pAuxComponent1[i]   = pHighRange[0];
            ppLimitComponent    = pAuxComponent1;
            pLimitKey->keycomps = numberComponents + 1;

            break;

        case KEY_LT:
	    /* Base is a one component LowRange */
            ppBaseComponent    = pLowRange;
            pBaseKey->keycomps = 1;

	    /* Limit is the given key */
            ppLimitComponent = pComponent1;

            break;

        case KEY_BEGINS:
	    /* Base is the given key */
            ppBaseComponent = pComponent1;

            /* Limit is the given key concatenated with HighRange */
            /* here we just generate the key, concatenation later */
            ppLimitComponent = pComponent1;

            break;

        default:
            return KEY_BAD_COMPARE_OPERATOR;
        }
    }
    else
    {             /* if BETWEEN operator  */
        /* Check consistancy of the components in the two keys */
        for (i=0; i < numberComponents; i++)
        {
            if ((pComponent1[i])->indicator != (pComponent2[i])->indicator) 
            return KEY_BAD_COMPONENT;
        }
        /* check if last component is descending */
        if (pComponent1[numberComponents-1]->indicator & SVCDESCENDING)
        {
            /* switch the input keys */
            psave       = pComponent1;
            pComponent1 = pComponent2;
            pComponent2 = psave;
        }

        switch(keyOperator) /* prepare Base components */
        {
        case KEY_BETWEEN_GE_LT:
        case KEY_BETWEEN_GE_LE:
	    /* Base is the given key */
            ppBaseComponent = pComponent1;

            break;

        case KEY_BETWEEN_GT_LT:
        case KEY_BETWEEN_GT_LE:
	    /* Base is the given key + HighRange */
            for (i=0; i < numberComponents; i++)
		pAuxComponent1[i] = pComponent1[i];
            pAuxComponent1[i]  = pHighRange[0];
            ppBaseComponent    = pAuxComponent1;
            pBaseKey->keycomps = numberComponents + 1;

            break;
        default:
            return KEY_BAD_COMPARE_OPERATOR;
        }

        switch(keyOperator) /* prepare Limit components */
        {
        case KEY_BETWEEN_GE_LE:
        case KEY_BETWEEN_GT_LE:
            /* Limit is the given key + HighRange */
            for (i=0; i < numberComponents; i++)
		pAuxComponent2[i] = pComponent2[i];
            pAuxComponent2[i]   = pHighRange[0];
            ppLimitComponent    = pAuxComponent2;
            pLimitKey->keycomps = numberComponents + 1;

            break;

        case KEY_BETWEEN_GT_LT:
        case KEY_BETWEEN_GE_LT:
	    /* Limit is the given key */
            ppLimitComponent = pComponent2;

            break;

        default:
            return KEY_BAD_COMPARE_OPERATOR;
        }
    }

    rc = keyBuild(keyMode, ppBaseComponent, maxBaseKeyLength, pBaseKey);
    if ( rc != KEY_SUCCESS )
	return rc;

    rc = keyBuild(keyMode, ppLimitComponent, maxLimitKeyLength, pLimitKey);
    if ( rc != KEY_SUCCESS )
	return rc;
    if (keyOperator == KEY_BEGINS)
    {
        if (pLimitKey->keyLen + (COUNT)sizeof(dsmKey_t) <= maxLimitKeyLength)
        {
            /* concatenate HIGHRANGE */
            pLimitKey->keystr[pLimitKey->keyLen] = HIGHRANGE;
            pLimitKey->keyLen++;
        }
        else
            return KEY_EXCEEDS_MAX_SIZE;
    }
    return rc;
}


/***************************************************************************/
/* PROGRAM: keyBuild - take an array of svcDataType_t components
 *                     build a key and stick it into the also
 *                     provided dsmKey_t structure 
 *
 * RETURNS: 
 *      KEY_SUCCESS             dsmKey structure contains keystring 
 *      KEY_BAD_COMPONENT       one or more components bad -> check indicators
 *      KEY_BAD_PARAMETER       pKey or pComponent = NULL, or pKey-keycomps <= 0
 *      KEY_BUFFER_TOO_SMALL    maxKeyLength is too small to hold key
 *      KEY_EXCEEDS_MAX_SIZE     generated key-string is bigger than KEYSIZE
 *
 *      First convert the components to a v6 key ditem (ix format)
 *      Then use cxDitemToStruct to convert it to a v7 dsmKey_t (cx format)
 *      The only members in the dsmKey_t structure we change are:
 *      + keyLen        ( the length of this key-entry )
 *      + unknown_comp  ( does this key-entry contain any unknown values? )
 *      + keystr        ( the actual key-entry )
 */
keyStatus_t
keyBuild(
       COUNT            keyMode,       /* IN */
       svcDataType_t   *pComponent[],  /* IN */
       COUNT            maxKeyLength,  /* IN */
       dsmKey_t        *pKey)          /* IN/OUT */
{
    ULONG                type;
    ULONG                indicator;
    COUNT                currentComp;
    GTBOOL                badComponents = 0;
    svcLong_t            l;
    dsmStatus_t          rc;
    svcDataType_t       *pComp;

    
    AUTODITM(s, KEYSIZE)
    AUTODITM(k, KEYSIZE)

    INITDITM(s)
    INITDITM(k)

    /* sanity-checks */

    if ( pKey           == NULL 
      || ( pComponent   == NULL && pKey->keycomps != 0 )
      || pKey->keycomps < 0 
       )
        return KEY_BAD_PARAMETER;

    /* initialize ditem to hold v6-format key-string */

    stnclr (k.aditem.f.key.keystr, 4);
    keyInitKDitem(&k.aditem, pKey->index, pKey->keycomps, pKey->ksubstr);


    /* generate v6-format key-string */

    for (currentComp = 0; currentComp < pKey->keycomps; currentComp++)
    {
        pComp     = pComponent[currentComp];

        if ( pComp == NULL )
            return KEY_BAD_PARAMETER;

        indicator = pComp->indicator;

        /* initialize output indicator-bits */
        indicator &= ~(SVCBADTYPE | SVCBADVALUE);

        if (indicator & (SVCUNKNOWN | SVCSQLNULL))
            s.aditem.flag = UNKNOWN;
        else 
            s.aditem.flag = 0;

        if ( indicator & SVCHIGHRANGE )
            s.aditem.flag = HIRANGE;
        else if ( indicator & SVCLOWRANGE )
            s.aditem.flag = LORANGE;
        else if ( (indicator & SVCUNKNOWN) 
          || (indicator & SVCSQLNULL) )
            ;
#if 0
        else if ( indicator & SVCHIGH )
            s.aditem.flag = UNKNOWN + HIGH;
        else if ( indicator & SVCLOW )
            s.aditem.flag = UNKNOWN + LOW;
#endif
        else
        {
            type      = pComp->type;
            
            switch(type)
            {
            case SVCBYTESTRING:
            case SVCCHARACTER:
                s.aditem.dflag2 = (indicator & SVCCASESENSITIVE) ? CASEDITM
                                                                 : 0;
                rc = keyPutText(&s.aditem, &pComp->f.svcByteString);
                if ( rc )
                    return rc;
                break;

            case SVCBINARY:
                rc = keyPutBinary(&s.aditem, &pComp->f.svcByteString);
                if ( rc )
                    return rc;
                break;

            case SVCBOOL:
                keyPutBool (&s.aditem, (svcBool_t) pComp->f.svcBool);
                break;

            case SVCTINY:
                keyPutLong (&s.aditem, (svcLong_t) pComp->f.svcTiny);

                break;

            case SVCSMALL:
                keyPutLong (&s.aditem, (svcLong_t) pComp->f.svcSmall);
                break;

            case SVCLONG:
                keyPutLong (&s.aditem, (svcLong_t) pComp->f.svcLong);
                break;

#if 0
            case SVCBIG:
                keyPutBig (&s.aditem, &pComp->f.svcBig);
                break;
#endif

            case SVCFLOAT:
                /* This is actually stored the same as a double */
                keyPutDouble ( &s.aditem, (svcDouble_t)pComp->f.svcFloat);
                break;

            case SVCDOUBLE:
                keyPutDouble ( &s.aditem, pComp->f.svcDouble);
                break;

            case SVCDATE:
                if ( utDateToLong(&l, (utDate_t *)(&pComp->f.svcDate)) < 0)
                {
                    indicator |= SVCBADVALUE;
                    badComponents++;
                }
                else
                    keyPutLong (&s.aditem, (svcLong_t) (l - DATEBASE));
                break;

            case SVCTIME: 
                utTimeToLong(&l, (utTime_t *)(&pComp->f.svcTime));
                keyPutLong (&s.aditem, (svcLong_t) l);
                break;

#if 0
            case SVCTIMESTAMP:
              {
                svcBig_t big;

                /* Store this as a big.  The high order part is for the date
                 * and the time is in the low order part. */

                if ( utDateToLong(&l,
                             (utDate_t *)(&pComp->f.svcTimeStamp.tsDate)) < 0)
                {
                    indicator |= SVCBADVALUE;
                    badComponents++;
                }
                else
                {
                    big.hl = l - DATEBASE;
                    utTimeToLong(&big.ll,
                                 (utTime_t *)(&pComp->f.svcTimeStamp.tsTime));
                    keyPutBig (&s.aditem, &big);
                }
              }
              break;
#endif

            case SVCDECIMAL:
                if (keyPutDecimal (&s.aditem, &pComp->f.svcDecimal))
                    return KEY_EXCEEDS_MAX_SIZE;
                break;

            default:
                indicator |= SVCBADTYPE;
                badComponents++;
                break;
                
            }   /* switch(type) */

        }       /* if  indicator = "regular" component */

        s.aditem.type = (indicator & SVCDESCENDING) ? DSRTFRM : ASRTFRM;

        pComp->indicator = indicator;

        /* convert storage- into key-ditem and insert value into key-string */
        rc = keyReplaceSDitemInKDitem( &k.aditem
                                     , &s.aditem
                                     , (COUNT)(currentComp+1)
                                     );
        if ( rc != DSM_S_SUCCESS )
            return rc;

    }   /* for each component */


    /* convert v6-format keystring into new format and stick it into dsmKey */

    if ( badComponents > 0 )
            return KEY_BAD_COMPONENT;
    else if ( (COUNT)*(TTINY *)k.aditem.f.key.keystr > maxKeyLength )
        return KEY_EXCEEDS_MAX_SIZE;
    
    else if (keyMode == SVCV8SORTKEY)
    {
        bufcop( pKey->keystr
              , k.aditem.f.key.keystr
              , *(TTINY *)k.aditem.f.key.keystr
              );
        return DSM_S_SUCCESS;
    }
    else
        return keyConvertIxToCx ( pKey
                                , pKey->index
                                , pKey->ksubstr
                                , 0 /*pKey->word_index*/
                                , k.aditem.f.key.keystr
                                , maxKeyLength
                               );


}  /* end keyBuild */


#if 0
/***********************************************************************/
/* PROGRAM: keyGetComponents - diassembles a keystring into its components
 * 
 *      the keystring provided within the dsmKey_t stucture gets disassembled
 *      into separate components which get stuck into pComponent-array
 *
 * RETURNS:
 *      KEY_SUCCESS             dsmKey structure contains keystring 
 *      KEY_BAD_COMPONENT       one or more components bad -> check indicators
 *      KEY_BAD_PARAMETER     pKey or pComponent is NULL, or pKey->keyLen <= 0 
 *      
 */
keyStatus_t
keyGetComponents(
        COUNT           keyMode,       /* IN */
        svcI18N_t      *obsolete,         /* IN */
        dsmKey_t       *pKey,          /* IN */
        svcDataType_t  *pComponent[])  /* IN/OUT */
{
    ULONG                type;
    dsmStatus_t          rc;
    COUNT                currentComp;
    COUNT                len, i;
    TEXT                *pt;
    GTBOOL                badComponents = 0;
    svcDataType_t       *pComp;
    svcDouble_t          dbl;
    svcLong_t            l;

    /* define ditems */

    AUTODITM(d, KEYSIZE)
    AUTODITM(s, KEYSIZE)
    AUTODITM(k, KEYSIZE)


    /* sanity-checks */

    if ( !pKey 
      || !pComponent 
      || pKey->keyLen < 0 )
        return KEY_BAD_PARAMETER;
  

    /* initialize ditems */

    INITDITM(d)
    INITDITM(s)
    INITDITM(k)


    /* convert key to v6-ditems */

    rc = keyConvertCxToIx(k.aditem.f.key.keystr, pKey->keyLen, pKey->keystr);

    pt = k.aditem.f.key.keystr + 1;     /* Skip key len */
    len = 0;


    /* disassemble v6-format keystring into separate components */

    for ( currentComp = 0
        ; currentComp < pKey->keycomps
        ; currentComp++, pt += len)
    {
        pComp = pComponent[currentComp];
        
        /* Skip to next component only when length of key component known! */
        len = *pt++;    /* len is the length of this component. 
                         * pt points to it.
                         */
        
        if (pComp == NULL)
             continue;     /* ??? TODO: */

        /* initialize output indicator-bits */
        pComp->indicator = pComp->indicator & ~( SVCBADTYPE 
                                               | SVCBADVALUE
                                               | SVCTRUNCVALUE
                                               | SVCUNKNOWN
                                               | SVCSQLNULL
                                               );

        /* Due to the DSAFF 'feature', a descending zero length 
         * character component is stored IDENTICALLY to a component
         * that has the HIGHRANGE value.  That's why the LOWRANGE &
         * HIGHRANGE are not included in the test below.
         */

        if (len == 1 && (*pt == LOWMISSING || *pt == HIGHMISSING))
        {
            pComp->indicator |= SVCUNKNOWN | SVCSQLNULL;
            continue;
        }
        
        type      = pComp->type;


        /* descending -> invert component */
        if (pComp->indicator & SVCDESCENDING)
        {
            d.aditem.type = DSRTFRM;
            
            for (i = 0; i < len; i++)
                pt[i] ^= 0xff;
        } 
        else
            d.aditem.type = ASRTFRM;

        /* convert component from index/storage-format to dsm<type> format */
        switch(pComp->type)
        {
        case SVCBINARY:
        case SVCBYTESTRING:
                
            if (len > pComp->f.svcByteString.size) 
            {
                if (!(pComp->indicator & SVCDESCENDING) || 
                    (ULONG)(len-1) > pComp->f.svcByteString.size) 
                    pComp->indicator |= SVCTRUNCVALUE;
		    pComp->f.svcByteString.length = len;
                
            } 
            else 
                pComp->f.svcByteString.length =                 /* DSAFF */
                   (ULONG)(len - ((pComp->indicator & SVCDESCENDING) ? 1 : 0));

            bufcop( pComp->f.svcByteString.pbyte 
                  , pt 
                  , pComp->f.svcByteString.length
                  );
            break;

        case SVCCHARACTER:
            /* character components are in collation-format which can't be
             * converted back to real format -> BADCOMPONENT for getcomponent 
             */
            pComp->indicator |= SVCBADTYPE;
            badComponents++;
            break;

        case SVCBOOL:
            if (len != 1)
            {
                pComp->indicator |= SVCBADTYPE;
                badComponents++;
            }
            else
                pComp->f.svcBool = *pt;

            break;

        case SVCTINY:
            pComp->f.svcTiny  = (svcTiny_t) keyGetLong (pt, len);
            break;

        case SVCSMALL:
            pComp->f.svcSmall = (svcSmall_t) keyGetLong (pt, len);
            break;

        case SVCLONG:
            pComp->f.svcLong  = (svcLong_t)  keyGetLong (pt, len);
            break;

#if 0
        case SVCBIG:
            keyGetBig (pt, len, &pComp->f.svcBig);
            break;
#endif


        case SVCFLOAT:
            /* This is actually stored the same as a double */
            keyGetDouble (pt, len, &dbl);
            pComp->f.svcFloat  = (svcFloat_t)  dbl;
            break;

        case SVCDOUBLE:
            keyGetDouble (pt, len, &pComp->f.svcDouble);
            break;

        case SVCDATE:
            l = keyGetLong (pt, len) + DATEBASE;
            if (utLongToDate(l, (utDate_t *)(&pComp->f.svcDate)) != 0)
            {
                pComp->indicator |= SVCBADVALUE;
                badComponents++;
            }
            break;

        case SVCTIME: 
            l = keyGetLong (pt, len);
            utLongToTime(l, (utTime_t *)(&pComp->f.svcTime));
            break;

#if 0
        case SVCTIMESTAMP:
          {
            svcBig_t big;

            /* This is stored as a big.  The high order part is for the date
             * and the time is in the low order part. */

            keyGetBig (pt, len, &big);
            
            l = big.hl + DATEBASE; 
            if (utLongToDate( l,
                            (utDate_t *)(&pComp->f.svcTimeStamp.tsDate)) != 0)
            {
                pComp->indicator |= SVCBADVALUE;
                badComponents++;
            }
            utLongToTime(big.ll, (utTime_t *)(&pComp->f.svcTimeStamp.tsTime));
          }
          break;
#endif

        case SVCDECIMAL:
            keyGetDecimal (pt, len, &pComp->f.svcDecimal);
            break;

        default:
            pComp->indicator |= SVCBADTYPE;
            badComponents++;
            break;

        }       /* switch(pComp->type) */
        
    }   /* for each component */


    if ( badComponents == 0 )
        return KEY_SUCCESS;
    else 
        return KEY_BAD_COMPONENT;

}  /* end keyGetComponents */
#endif

/* PROGRAM: keyGetNextWord - extracts the first word from the given text.
 *   Also adjusts the pointer to the given text to the position from where 
 *   to start looking for the next word.
 *
 *   The extracted word is returned in the area pointed to by pword,
 *   which must be a TEXT array of length KEY_MAX_WI_SIZE + 1.  The first byte
 *   is the word length, and is followed by the word.
 *
 *   If the next word is longer than KEY_MAX_WI_SIZE, only KEY_MAX_WI_SIZE  
 *   bytes are returned.
 *
 * RETURNS: 
 *         KEY_SUCCESS          success
 *         KEY_WORD_TRUNCATED   Returned word is truncated
 *         KEY_NO_MORE_WORDS    No more words
 *         KEY_BAD_PARAMETER    one of parameters not defined
 */
keyStatus_t
qwGetNextWord(
        TEXT   **ppText,          /* pointer to pointer to 1. char of text */
        TEXT    *pNoMore,         /* position past the last char of text */
        TEXT    *pWord,           /* return the next word here */
        TEXT    *pWordBreakTable) /* ponter to the wordbreak table */
{
    TEXT    *pText      = *ppText;
    TEXT     tb;             /* the tokenizing table byte of cur char */
    int      wordLength = 0;
    TEXT     prev       = 0; /* attr of prev char, which must be handled */
    TEXT    *pLengthByte;    /* to word lengh byte */

    if ( pWordBreakTable == NULL
      || pNoMore         == NULL
      || pWord           == NULL
      || ppText          == NULL
       )
        return KEY_BAD_PARAMETER;
    
    pLengthByte = pWord++; /* skip the word length byte */
    for (; pText != pNoMore; pText++)
    /* loop ends when either end-of-text reached, or terminator encountered.
     * If word is larger KEY_MAX_WI_SIZE, don't add any more characters to it,
     * but continue to chip chars off of the text until the end-of text or
     * end-of-word is reached
     */
    {

/* I18N TODO: the following statement assumes 1 byte per character! */
        tb = pWordBreakTable[*pText];

        if (prev)               /* handle the previous char */
        {
            /* the previous character may or may not be part of the word */
            if ( ( tb == KEY_DIGIT  
                && ( prev == KEY_BEFORE_DIGIT 
                  || prev == KEY_BEFORE_LETTER_DIGIT
                   )
                 )
              || ( tb == KEY_LETTER 
                && ( prev == KEY_BEFORE_LETTER 
                  || prev == KEY_BEFORE_LETTER_DIGIT
                   )
                 )
               )
            {
                /* the previous character is part of the word */
                if (wordLength < KEY_MAX_WI_SIZE)     /* word not at max size yet */
                {
                    *pWord++ = *(pText - 1); /* include the prev char */
                    wordLength++;
                }
            }
            else if (wordLength > 0)       /* prev char is a separator */
                goto done;              /* the word is terminated */
            prev = 0;
        }

        switch (tb)
        {
        case KEY_TERMIN:
            if (wordLength > 0)            /* cur char is a separator */
                goto done;              /* the word is terminated */
            break;

        case KEY_LETTER:
        case KEY_DIGIT:
        case KEY_USE_IT:
            if (wordLength < KEY_MAX_WI_SIZE)     /* word didn't reach max size */
            {
                *pWord++ = *pText;      /* include this char */
                wordLength++;
            }
            break;

        case KEY_BEFORE_DIGIT:
        case KEY_BEFORE_LETTER:
        case KEY_BEFORE_LETTER_DIGIT:
            prev = tb;
        }
    }

done:
    if (wordLength == 0)
        return KEY_NO_MORE_WORDS;
    *pLengthByte = wordLength;
    *ppText      = pText;
    
    if (wordLength < KEY_MAX_WI_SIZE)
        return KEY_SUCCESS;
    else
        return KEY_WORD_TRUNCATED; /* not quite true, word-length  */
                                    /* could be just (KEY_MAX_WI_SIZE - 1) */

}  /* end keyGetNextWord */

/***************************************************************************/
/***                   KeyServices Internal Functions                    ***/
/***************************************************************************/


/***************************************************************************/
/***                           Local Functions                           ***/
/***************************************************************************/

#if 0
/***********************************************************************/
/* PROGRAM: keyGetBig -
 *
 * RETURNS:
 */
LOCALF DSMVOID
keyGetBig(
        TEXT      *pt,
        COUNT     len,
        svcBig_t *pb)
{
    
    if (*pt > (TEXT)0x40)     /* Set the high byte for keyGetLong */
        *pt -= 0x40;
    else
        if ((*pt & 0xf0) == 0)
            *pt |= 0x50;
    
    if (len < 5) {
        /* Its stored like a long */

        pb->ll = keyGetLong (pt, len);

        /* Extend sign bit */

        pb->hl = (pb->ll & (ULONG)0x80000000) ? (ULONG)0xffffffff 
                                              : (ULONG)0x00000000;
    } else {
        pb->hl = keyGetLong (pt, (COUNT)(len - 4));
        pb->ll = xlng  (pt + len - 4);
    }

}  /* end keyGetBig */


/* PROGRAM: keyGetDecimal -
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
keyGetDecimal(
        TEXT         *pt,
        COUNT         len,
        svcDecimal_t *pd)
{
    int          i;
    int          exp;   /* Bias 32 exponent */
    TEXT        *pdig = pd->pdigit;
    int          storedLen;  /* number of digits in the key */
    int          realExp;

    pd->plus = (*pt & 0x80) ? 1 : 0;
    if (!pd->plus)
        for (i = 0; i < len; i++)
            pt[i] ^= 0xff;

    exp = 0x7f & *pt;

    /* 2 * (len - 1) Since 2 digits per byte, 1 byte for sign & exponent */
    storedLen = 2 * (len - 1);

    /* See if last 2 nibbles are occupied */
    if (storedLen) {
        if (pt[storedLen / (TEXT)2]  == 0)
            storedLen -= 2;
        else if ((pt[storedLen / (TEXT)2] & 0x0f) == 0)
            storedLen--;
    }
    
    if (!storedLen) {             /* The value 0.0 is a special case */
        pd->totalDigits = 1;
        pd->decimals = 0;
         *pdig = 0;
         return;
    }

    /* This is the real exponent! */
    realExp = exp - 32;
 
    if (realExp < storedLen)
        pd->decimals = storedLen - realExp;
    else
        pd->decimals = 0;

    pt++;
    
    *pdig = 0;

    while (storedLen < pd->decimals) {    /* Left zero fill */
        *pdig++ = 0;
        storedLen++;
    }
    
    pd->totalDigits = storedLen;

    /* right zero fill */
    if ((realExp - storedLen) > 0)
    {
        /* clear the result area */
        stnclr(pd->pdigit+storedLen, realExp - storedLen);
        pd->totalDigits += realExp - storedLen;
    }

    while (storedLen--) {
        *pdig++ = ((*pt >> (TEXT)4) & 0x0f) - 1;
        if (!storedLen--)
            break;
        *pdig++ = (*pt++  & 0x0f) - 1;
    }

}  /* end keyGetDecimal */


/* PROGRAM: keyGetDouble -
 *
 * RETURNS: DSMVOID
 */
LOCALF DSMVOID
keyGetDouble(
        TEXT        *pt,
        COUNT        len,
        svcDouble_t *pd)
{
    double       dbl;
    TEXT         tmp [sizeof (double)];
    
    bufcop (tmp, pt, sizeof (double));

    if ( tmp[0] & 0x80 )              /* positive (stored as negative) */
        tmp[0] ^= 0x80;
    else
    {
        tmp[0] ^= 0xff;                 /* 1/7 bits of sign/exponent */
        tmp[1] ^= 0xff;                 /* 4/52 bits of exponent/mantissa */
        tmp[2] ^= 0xff;
        tmp[3] ^= 0xff;
        tmp[4] ^= 0xff;
        tmp[5] ^= 0xff;
        tmp[6] ^= 0xff;
        tmp[7] ^= 0xff;
    }

    dbl = xdbl (tmp);
    *pd = (svcDouble_t) dbl;

}  /* end keyGetDouble */


/* PROGRAM: keyGetLong -
 *
 * RETURNS:
 */
LOCALF svcLong_t 
keyGetLong(
        TEXT  *pt,
        COUNT  len)
{
    LONG         l;
    GTBOOL        extend_sign = len <= 4 && ((*pt & (TEXT)0xf0) < 0x50);
    
    /*  Big Integers
     *  ------------
     * val-len  storage                 storage-length
     *  1       9x                              1
     *  1       a0 xx                           2
     *  2       ax xx                           2
     *  2       b0 xx xx                        3
     *  3       bx xx xx                        3
     *  3       c0 xx xx xx                     4
     *  4       cx xx xx xx                     4
     *  4       d0 xx xx xx xx                  5
     *  5       dx xx xx xx xx                  5
     *  5       e0 xx xx xx xx xx               6
     *  6       ex xx xx xx xx xx               6
     *  6       f0 xx xx xx xx xx xx            7
     *  7       f1 0x xx xx xx xx xx xx         8
     *  7       f1 xx xx xx xx xx xx xx         8
     *  8       f2 0x xx xx xx xx xx xx xx      9
     *  8       f2 xx xx xx xx xx xx xx xx      9
     *
     *  Long Integers
     *  -------------
     * val-len  storage                 storage-length
     *  1       5x                              1
     *  1       60 xx                           2
     *  2       6x xx                           2
     *  2       70 xx xx                        3
     *  3       7x xx xx                        3
     *  3       80 xx xx xx                     4
     *  4       8x xx xx xx                     4
     *  4       90 xx xx xx xx                  5
     */

    if ( (len > 4) 
      || ((*pt & 0xf0) == 0xf0) /* special case big stored in 7+ bytes */
       )
        l = xlngv (pt+1, 4);
    else {
        *pt &= 0x0f;        /* Remove length nibble */
    
        l = xlngv (pt, len);
    }

    if (extend_sign)
        l |= ((ULONG)0xfffffff0 << (len - 1) * 8);
    
    return l;

}  /* end keyGetLong */


/***********************************************************************/
/* PROGRAM: keyPutBig -
 *
 * RETURNS:
 */ 
LOCALF DSMVOID
keyPutBig(
        struct ditem *pditem,
        svcBig_t     *pb)
{
    COUNT       len;

    /* If the high order part is just sign extension from the
     * low order part */

    if ((!(pb->ll & (ULONG)0x80000000) && pb->hl == (ULONG)0x00000000) ||
        ( (pb->ll & (ULONG)0x80000000) && pb->hl == (ULONG)0xffffffff))
    {

        /* We can store it in 0 to 5 bytes */

        len = slngv(pditem->f.srt.vch, pb->ll);
    } else {

        /* Store the high order part in 1 to 5 bytes and the
         * low order part in 4 bytes */

        len = slngv(pditem->f.srt.vch, pb->hl);
        slng (pditem->f.srt.vch + len, pb->ll);
        len += sizeof (LONG);
    }

    if (len == 0)
	pditem->f.srt.vch[0] = 0x10; /* trigger expansion */
    else if (pb->hl < 0 && (pditem->f.srt.vch[0] & 0xf0) == 0xf0)
	pditem->f.srt.vch[0] &= 0x0f;

#if 0 /*------------------------------------------------------------ old*/
    if (pditem->f.srt.vch[0] & 0xf0)
#else /*------------------------------------------------------------ new*/     
       
    if ((pditem->f.srt.vch[0] & 0xf0)
    || (len > 6 && pb->hl >= 0)) /* positive numbers and len is 7 or 8 */
#endif/*------------------------------------------------------------ endnew*/
    {
	bufcop(pditem->f.srt.vch + 1, pditem->f.srt.vch, len);
	++len;
	pditem->f.srt.vch[0] = (pb->hl >= 0) ? 0 : 0x0f;
    }

    pditem->SRTVLEN = len;

    if (pb->hl >= 0)
#if 0 /*------------------------------------------------------------ old*/
	len += 8;
#else /*------------------------------------------------------------ new*/     
        {
            if (len < 7)
	        len += 8; /* add bias is len 0 to 6 */
            else
            {
                /* for len > 6 the length byte will be as follows:
                 *
                 *        len           1st byte
                 *         7             x'f0'
                 *         8             x'f1'
                 *         9             x'f2' 
                 */
                pditem->f.srt.vch[0] |= (len - 7);
                len = 15;            
            }
        }
#endif/*------------------------------------------------------------ endnew*/
    else
	len = 9 - len;

    pditem->f.srt.vch[0] |= (len << 4);
    pditem->flag = 0;

}  /* end keyPutBig */
#endif


/* PROGRAM: keyPutBool -
 *
 * RETURNS:
 */
LOCALF DSMVOID
keyPutBool(
        struct ditem *pditem,
        svcBool_t     b)
{
    pditem->f.srt.vch[0] = (b ? 1 : 0);
    pditem->SRTVLEN = 1;

}  /* end keyPutBool */


/* PROGRAM: keyPutDecimal -
 *
 * RETURNS: 0                    - success
 *          KEY_EXCEEDS_MAX_SIZE - maximum key length exceeded 
 */
LOCALF int
keyPutDecimal(
        struct ditem *pditem,
        svcDecimal_t *pd)
{
    TEXT        *pt = pditem->f.srt.vch;
    int          i;

    *pt = /* digits to left of decimal */ 32 + pd->totalDigits - pd->decimals;
    pditem->SRTVLEN = sortdec(pt, pd->pdigit, pd->totalDigits) - pt;

    if (pditem->dpad + DEFVLEN <= (UCOUNT)(pditem->SRTVLEN))
        return KEY_EXCEEDS_MAX_SIZE;		/* maximum key length exceeded */

    *pt |= 0x80;
    if (pditem->SRTVLEN > 1 &&
        !(pt[pditem->SRTVLEN - 1] & 0x0e))
        /* trailing 0 nibble can be used as fence */
        pt[pditem->SRTVLEN - 1] &= 0xf0;
    else if (pd->plus)
        pditem->flag |= DSAFF;
    else /* negative value will require additional fence */
    {
        pt[pditem->SRTVLEN] = 0;
        pditem->SRTVLEN++;
    }

    if (!pd->plus) {
        for (i = 0; i < pditem->SRTVLEN; i++)
            *pt++ ^= 0xff;
    }
    return 0;

}  /* end keyPutDecimal */


/* PROGRAM: keyPutDouble -
 *
 * RETURNS:
 */
LOCALF DSMVOID
keyPutDouble(
        struct ditem *pditem,
        svcDouble_t   dbl)
{
    TEXT        *pt = pditem->f.srt.vch;
    
    pditem->SRTVLEN = (COUNT) sizeof (double);

    sdbl (pt, (double) dbl);

    if ( pt[0] & 0x80 )               /* negative */
    {   /* complement all bits */
        pt[0] ^= 0xff;                  /* 1/7 bits of sign/exponent */
        pt[1] ^= 0xff;                  /* 4/52 bits of exponent/mantissa */
        pt[2] ^= 0xff;
        pt[3] ^= 0xff;
        pt[4] ^= 0xff;
        pt[5] ^= 0xff;
        pt[6] ^= 0xff;
        pt[7] ^= 0xff;
    }
    else /* Complement only the sign bit - to force negative values low */
        pt[0] ^= 0x80;

    pditem->flag = 0;

}  /* end keyPutDouble */


/* PROGRAM: keyPutLong -
 *
 * RETURNS:
 */
LOCALF DSMVOID
keyPutLong(
        struct ditem *pditem,
        svcLong_t     l)
{
    COUNT       len;

    len = slngv(pditem->f.srt.vch, l);

    if (len == 0)
	pditem->f.srt.vch[0] = 0x10; /* trigger expansion */
    else if (l < 0 && (pditem->f.srt.vch[0] & 0xf0) == 0xf0)
	pditem->f.srt.vch[0] &= 0x0f;

    if (pditem->f.srt.vch[0] & 0xf0)
    {
	bufcop(pditem->f.srt.vch + 1, pditem->f.srt.vch, len);
	++len;
	pditem->f.srt.vch[0] = (l >= 0) ? 0 : 0x0f;
    }

    pditem->SRTVLEN = len;

    if (l >= 0)
	len += 4;
    else
	len = 5 - len;

    pditem->f.srt.vch[0] |= (len << 4);

}  /* end keyPutLong */


/* PROGRAM: keyPutBinary - 
 *
 * RETURNS: 0                    - success
 *          KEY_EXCEEDS_MAX_SIZE - maximum key length exceeded 
 *          KEY_BAD_I18N         - no collation table or no user defined sort
 */
LOCALF int
keyPutBinary(
        struct ditem    *pditem,
        svcByteString_t *pByteString)
{
    TEXT        *pt;

    pt = pditem->f.srt.vch;
    
    if (pditem->dpad + DEFVLEN < (UCOUNT)(pByteString->length))
        return KEY_EXCEEDS_MAX_SIZE;   /* maximum key length exceeded */
    bufcop (pt, pByteString->pbyte, pByteString->length);
    pditem->SRTVLEN = pByteString->length;
    pditem->flag   |= DSAFF;

    return 0;

}  /* end keyPutBinary */

 
/* PROGRAM: keyPutText - 
 *
 * RETURNS: 0                    - success
 *          KEY_EXCEEDS_MAX_SIZE - maximum key length exceeded 
 *          KEY_BAD_I18N         - no collation table or no user defined sort
 */
LOCALF int
keyPutText(
        struct ditem    *pditem,
        svcByteString_t *pByteString)
{
    TEXT        *pt;

    pt = pditem->f.srt.vch;
    
/* TODO: just truncate string??? */
    if ( pditem->dpad + DEFVLEN < (UCOUNT)(pByteString->length) )
        return KEY_EXCEEDS_MAX_SIZE;  /* maximum key length exceeded */

           /* no pCollation provided */
        bufcop (pt, pByteString->pbyte, pByteString->length);
        *(pt + pByteString->length) = '\0';
           /* no pCollation provided */

    pditem->SRTVLEN = pByteString->length;
    pditem->flag   |= DSAFF;

    return 0;

}  /* end keyPutText */

 

/* PROGRAM: sortdec -- local routine to collapse decimals
 *
 * RETURNS: TEXT *
 * (copied from fmsrt.c...)
 */
LOCALF TEXT *
sortdec(
        TEXT        *pt,
        TTINY       *ps,
        TTINY       len)
{
    TTINY       *pe = ps + len;
    int         pair;
    TTINY       *plast;
    int         suppress = 0;

    while (ps < pe && *ps == 0)
    {
	suppress++;
	ps++;
    }
    if (ps < pe)
	*pt++ -= suppress;
    else
	*pt++ = 0;

    /* NOTE: we mask the right nibble here because we don't know
     *       if the caller is passing 0x31 or 0x01 to represent
     *       decimal 1.  The sort only wokrs properly with 0x01.
     */
    plast = pt;
    while (ps < pe)
    {
	pair = (*ps++ & 0x0f) << 4;
	if (ps < pe)
        {
	    pair += *ps++ & 0x0f;
        }
	*pt++ = pair + 0x11;
	if (pair)
	    plast = pt;
    }

    return plast;

}  /* end sortdec */


/***********************************************************************/
