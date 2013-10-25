
 /* Copyright (C) 2000 by NuSphere Corporation
 *
 * This is an unpublished work.  No license is granted to use this software. 
 *
 */

/* Always include gem_global.h first.  There are places where dbconfig.h
** is not the first include, so we can't put gem_config.h there.
*/
#include "gem_global.h"

#include "dbconfig.h"
#include "cxpub.h"

#if 0
/***************************************************************************/

/* PROGRAM: cxDitemToStruct - take a ditem containing a key (old format)
 *                            and build a dsmKey_t structure allocated above
 *
 *      This function overwrites only the following members of the dsmKey_t 
 *      structure, and leaves all the other members alone:
 *      * index
 *      * keycomps
 *      * keyLen
 *      * unknown_comp
 *      * descending_key
 *      * keystr
 *
 * RETURNS: DSM_S_SUCCESS     - success
 *          KEY_BAD_PARAMETER  - pditem not supplied
 *          return-codes from cxKeyToCx
 */
keyStatus_t
cxDitemToStruct (DITEM *pkditem,  /* points to the ditem, must be big enough */
                 dsmKey_t *pkkey) /* points to a the dsmKey_t */
{
    keyStatus_t rc;
 
    if (!pkditem)
        return KEY_BAD_PARAMETER;
 
    else if ( (rc = keyInitStructure( SVCV9IDXKEY
                                    , pkditem->f.key.kidx
                                    , pkditem->f.key.kcomps
                                    , KEY_NO_LENGTH_CHECK
                                    , 0 /* descending_key */
                                    , pkditem->f.key.ksubstr
                                    , pkkey
                                    ) ) != DSM_S_SUCCESS )
        return rc;

    else
    {  /* convert to new key format */
        rc = keyConvertIxToCx ( pkkey
                              , pkditem->f.key.kidx
                              , pkditem->f.key.ksubstr
                              , 0
                              , pkditem->f.key.keystr
                              , KEY_NO_LENGTH_CHECK
                              );
        return (rc);

    }  /* convert to new key format */


} /* cxDitemToStruct */
#endif

#if 0
/***************************************************************************/

/* PROGRAM: cxWordDitemToStruct - take a ditem containing a key (old format)
 *                            and build a dsmKey_t structure allocated above
 *
 *      This function overwrites only the following members of the dsmKey_t 
 *      structure, and leaves all the other members alone:
 *      * index
 *      * keycomps
 *      * keyLen
 *      * unknown_comp
 *      * descending_key
 *      * keystr
 *
 * RETURNS: DSM_S_SUCCESS     - success
 *          KEY_BAD_PARAMETER  - pditem not supplied
 *          return-codes from keyConvertIxToCx and keyInitStructure
 */
keyStatus_t
cxWordDitemToStruct(
        DITEM    *pkditem,   /* points to tokenized list of words */
        dsmKey_t *pkkey)     /* dsmKey to receive its version of
                                the list of words.                 */
{
    keyStatus_t  rc;
   
    if (!pkditem)
        return KEY_BAD_PARAMETER;
 
    else if ( (rc = keyInitStructure( SVCV9IDXKEY
                                    , pkditem->f.key.kidx
                                    , pkditem->f.key.kcomps
                                    , KEY_NO_LENGTH_CHECK
                                    , 0 /* descending_key */
                                    , pkditem->f.key.ksubstr
                                    , pkkey
                                    ) ) != DSM_S_SUCCESS )
        return rc;

    else
    {  /* convert to new key format */
        rc = keyConvertIxToCx ( pkkey
                              , pkditem->f.key.kidx
                              , pkditem->f.key.ksubstr
                              , KEY_WORD_INDEX
                              , pkditem->f.key.keystr
                              , KEY_NO_LENGTH_CHECK
                              );
        return (rc);

    }  /* convert to new key format */


}  /* end cxWordDitemToStruct */
#endif 

#if 0
/***********************************************************************/

/* PROGRAM: cxConvertToIx - converts a cx format component to an ix component
 * 
 *          converts one component (NULL terminated) from new cx format
 *          back to the old ix component (1st byte is the length)
 *
 * RETURNS: DSM_S_SUCCESS - success
 */
keyStatus_t
cxConvertToIx(
        TEXT  *pt,    /* to target area - old format */
        TEXT  *ps)    /* to source area - new format */
{
    return keyConvertCxToIxComponent(pt,ps);
    
#if 0
        int      len=0;
        TEXT    *plenbyte;
 
    for ( plenbyte = pt++; *ps; ps++, pt++, len++)
    {
        if (*ps == LOWESCAPE)
        {
            ps++;
            if(*ps == REPLACEZERO)
                *pt = 0;
            else
            if(*ps == NULLCOMPONENT)    /* null component */
            {
                pt--;
                len--;
            }
            else
            if(*ps == REPLACEONE)
                *pt = 1;
            else
            if (*ps == LOWMISSING)
                *pt = LOWMISSING;
        }
        else
        if (*ps == HIGHESCAPE)
        {
            ps++;
            if (*ps == REPLACEFF)
                *pt = 0xff;
            if (*ps == HIGHMISSING)
                *pt = HIGHMISSING;
        }
        else
        {
            *pt = *ps;
        }
    }

    *plenbyte = len;

    return KEY_SUCCESS;
#endif

}  /* end keyConvertToIx */
#endif


/************************************************************************/
/************************************************************************/
/************************************************************************/

#if 0

/************************************************************************/
/* PROGRAM: cxKeyToCx -- old entry-point to this function; new:= cxKeyToCxStr
 * 
 *      cxKeyToCx does not have a parameter for the max length of the string,
 *      so we pass KEYLEN to cxKeyToCxStr - cxKeyToCxStr is now also checking 
 *      for length overflows
 *      cxKeyToCxStr does fill in only keystr, keyLen and unknown_comp; so the
 *      remaining fields of the dsmKey_t structure get filled in here
 * 
 * RETURNS: DSM_S_SUCCESS  - success
 *          returnCode form cxKeyToCxStr
 * 
 */
cxStatus_t
cxKeyToCx (dsmKey_t *pkkey,     /* storamge manager key structure */
        DITEM   *pkditem,       /* points to the ix key          */
        int      wordList   )   /* ditem is a list words for a word index */
{

    LONG        rc;

    if ( (rc = cxKeyToCxStr(pkkey, pkditem, wordList, CX_NO_LENGTH_CHECK)) == 0 )
    {   /* fill in the rest of the fields */
        pkkey->index          = pkditem->f.key.kidx;
        pkkey->keycomps       = pkditem->f.key.kcomps;
        pkkey->descending_key = 0;
        pkkey->ksubstr        = pkditem->f.key.ksubstr;
    }

    return rc;
    
}  /* end cxKeyToCx */
 
/************************************************************************/
/* PROGRAM: cxKeyToCxStr -- convert an ix key to the new fromat of cx
 *              also checks the key (instead of the old kychk procedure).
 *
 *      This function overwrites only the following members of the dsmKey_t 
 *      structure, and leaves all the other members alone:
 *      * keystr
 *      * keyLen
 *      * unknown_comp
 *
 *      The new key is developed in a key ditem. But
 *      the format of the keystr is as follows:
 *      1st byte - total length of key and info.
 *      2nd byte - size of the key (ks)
 *      3rd byte - size of the info (is)
 *      followed by the key, followed by the info.
 *
 *      The new cx format uses some special characters.
 *      The special characters are as follows:
 *
 *      x'00' - component separator, will sort lower that any value.
 *      x'01' - low escape character.
 *      x'ff' - High escape character.
 *
 *      When a special character is found in a regular component of a key,
 *      it will be replaced with an escape sequence that will preserve the
 *      collating sequence, as follows:
 *
 *      x'00' - will be replaced with escape sequence x'01fe'
 *      x'01' - will be replaced with escape sequence x'01ff'
 *      x'ff' - will be replaced with escape sequence x'ff01'
 *
 *      Additional escape sequences are:
 *
 *      x'0101' LOW-RANGE value, it will sort higher than component separator
 *                      but lower than any other value except.
 *                      NOTICE: a LOWRANGE in the old index, if not in the
 *                      1st component will cause the following conversion:
 *                      the last byte of the previous component will be
 *                      decremented by 1, and a HIGHRANGE value will be
 *                      concatennated to it.
 *      x'01f0' NULL component like "" char value, it will sort higher than
 *                      component separator and higher than LOW-RANGE, but
 *                      less than any other value.
 *      x'01fb' LOW-UNKNOWN value, it will sort higher than component separator
 *                                      but lower than any other value.
 *      x'fffc' HIGH-UNKNOWN value, it will sort higher than component separator
 *                      and higher than any other value except HIGH-RANGE.
 *      x'ffff' HIGH-RANGE value, it will sort higher than component separator
 *                      but higher than any other value.
 *
 * RETURNS:
 *       . DSM_S_SUCCESS  - success
 *       . CX_KYILLEGAL  (-0xfa) if an illegal component was found.
 *
 *      ************** changes to this procedure may require change to
 *              cxIxKeyCompare.  Both depend on the same new key format
 *              and any change in the key format should be reflected
 *              in both procedures.  For performance reasons
 *              they are not combined. *********
 */
cxStatus_t
cxKeyToCxStr (dsmKey_t *pkkey, /* OUT storamge manager key structure */
        DITEM   *pkditem,      /* IN  points to the ix key          */
        int      wordList,     /* IN  ditem is a list words for a word index */
        LONG     bufferSize) /* IN  max possible length for keystr */
{
    TEXT        *pfrom;         /* points to the source key */
    TEXT        *pce;           /* points to the end of a source component */
    TEXT        *pto;           /* points to the target key */
    TEXT        *pend;          /* points to the end of the source key */
    TEXT         unknown    = 0;
    TEXT        *p_start_to;
    TEXT        *pWordStart = 0;
    LONG        availableBuffer = 0x0fff;  /* for check before adding more
                                            * bytes to keystr
                                            */
    
 
    pkkey->unknown_comp = 0;
    pto                 = pkkey->keystr;
    if ( bufferSize != CX_NO_LENGTH_CHECK )
        availableBuffer = bufferSize - pkditem->f.key.keystr[0];

    
    if ( wordList )
    {       /* word-index */
        pfrom = pkditem->f.key.keystr + 1;
        pend  = pfrom + *pfrom + 1; /* word list len does not include trailing
                                     * length byte.
                                     */
    }       /* word-index */
    else
    {       /* !word-index */
        CX_LENGTH_CHECK(6,availableBuffer,bufferSize);
        pfrom = pkditem->f.key.keystr;
/*        pto   += 3;  skip the 1st byte which is used for the total size */
                    /* skip the 2nd byte which is used for the key size */
                    /* skip the 3nd byte which is used for the info size */
        pto += FULLKEYHDRSZ;
        
        *pto++ = 0; /* mark the new format & define the type of the index = 0 */
        *pto++ = 0; /* mark the new format */
        sct(pto, pkditem->f.key.kidx);  /* index number */
        pto += 2;
 
        pend = pfrom + *pfrom - 1;  /* the key lng byte at the end */
    }       /* !word-index */
 
    pfrom++;    /* to 1st component */

    if (pfrom == pend)          /* the key is empty, used for default key */
    {
        CX_LENGTH_CHECK(2,availableBuffer,bufferSize);
        *pto++ = LOWESCAPE;
        *pto++ = NULLCOMPONENT;
        *pto++ = 0;
        goto done;
    }
 
    p_start_to = pto;           /* start of new key */

    do
    {   /* while (pfrom != pend); / * scan the key components and convert */

        if ( wordList )
        {       /* word-index */
            CX_LENGTH_CHECK(6,availableBuffer,bufferSize);
            /* For each word in the ditem we create a whole key string.    */
            pWordStart = pto;
/*            pto +=3;  skip the 1st byte which is used for the total size */
                     /* skip the 2nd byte which is used for the key size */
                     /* skip the 2nd byte which is used for the info size */
            pto += FULLKEYHDRSZ;
 
            *pto++ =0; /* mark the new format  and define the type of the index
= 0 */
            *pto++ =0; /* mark the new format */
            sct(pto, pkditem->f.key.kidx);      /* index number */
            pto += 2;
        }       /* word-index */
 
        /* scan all the key components and convert */
        if (*pfrom <= LKYFLAG)          /* it is an ordinary component */
        {
            if (*pfrom == 0)            /* a null component */
            {
                CX_LENGTH_CHECK(1,availableBuffer,bufferSize);
                *pto++ = LOWESCAPE;
                *pto++ = NULLCOMPONENT;
                pfrom++;
            }
            else
            for (pce = pfrom + *pfrom, pfrom++; pfrom <= pce; pfrom++, pto++)
            {
                /* NOTE: the following checks whether ch equals 0xff, 0 or 1 */
                /* It relies on overflow to change the range to 0, 1, 2 so   */
                /* that it can be done in one if statement.   This is rather */
                /* dirty, but since the key conversion is called frequently, */
                /* it seems to be worth it */
                if ((TEXT)(*pfrom + 1) > 2)
                {
                    *pto = *pfrom;
                }
                
                else if (*pfrom < 2)
                {
                    CX_LENGTH_CHECK(1,availableBuffer,bufferSize);
                    *pto++ = LOWESCAPE;
                    *pto = REPLACEZERO + *pfrom; /* REPLACEZERO or REPLACEONE */
                }
                else            /* *pfrom must be 0xff */
                {
                    CX_LENGTH_CHECK(1,availableBuffer,bufferSize);
                    *pto++ = HIGHESCAPE;
                    if (pkditem->f.key.ksubstr && pfrom == pend - 2)
                        *pto = 0XFF;
                    else
                        *pto = REPLACEFF;
                }

            }   /* for each byte in the string */

        }                               /* it is an ordinary component */

        else                            /* it is a special flag */
        {
            switch (*pfrom)
            {
            case  LOWRANGE:
                if (pto != p_start_to)  /* not 1'st component */
                    pto--;
                break;
            case HIGHRANGE:
                CX_LENGTH_CHECK(1,availableBuffer,bufferSize);
                *pto++ = HIGHESCAPE;
                *pto++ = HIGHRANGE;
                break;
            case HIGHMISSING:
                CX_LENGTH_CHECK(1,availableBuffer,bufferSize);
                *pto++ = HIGHESCAPE;
                *pto++ = HIGHMISSING;
                unknown=1;
                break;
            case LOWMISSING:
                CX_LENGTH_CHECK(1,availableBuffer,bufferSize);
                *pto++ = LOWESCAPE;
                *pto++ = LOWMISSING;
                unknown=1;
                break;
            case KEY_ILLEGAL:
                return CX_KYILLEGAL; /* illegal key */
            default:
#if 0
                FATAL_MSGN_CALLBACK(pcontext, cxFTL011, *pfrom);
#else
                return CX_KYILLEGAL; /* illegal key */
#endif
            }

            pfrom++;

        }                            /* it is a special flag */

        /* end of component */

        *pto++ = COMPSEPAR;     /* null separator */

        if (wordList )
        {
            /* Add in the length for a word list key    */
            *pWordStart = pto - pWordStart - 3;
            *(pWordStart + 1) = pto - pWordStart - 3;
        }
 
    }  while (pfrom != pend); /* scan the key components and convert */
 
  done:
 
    pkkey->keyLen = pto - pkkey->keystr - 3;
    if (unknown)
        pkkey->unknown_comp = 1;
    else
        pkkey->unknown_comp = 0;
 
    return DSM_S_SUCCESS;

}  /* end cxKeyToCxStr */

/***********************************************************************/

#endif

