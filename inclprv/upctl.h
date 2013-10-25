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

#ifndef UPCTL_H
#define UPCTL_H

/***********************************************************************
*                                                                      *
* upctl.h Universal Progress Characters (UPSCII) and language control  *
* jrs -- added case-sensitive tables 7/89                              *
*                                                                      *
***********************************************************************/

#include "dbctl.h"
#include "utkwmgr.h"

#define ENGLISH         0 /* must be zero */
#define BASIC           0 /* "most european languages" */
#define DANISH          1
#define NORWEGIAN       1
#define SWEDISH         2
#define FINNISH         2
#ifndef USERCOLL
#define USERCOLL        3
#endif
#define LASTLANG        4 /* size of language array */

#define UPPER           '\001'
#define LOWER           '\0'

/* definition of the UPSCII (IBM CodePage 850) accented letters */
#define CP850_AgraveU   183   /* A (Upper case) with grave accent "\" */
#define CP850_AgraveL   133   /* a (Lower case) with grave accent "\" */
#define CP850_AacuteU   181   /* A (Upper case) with acute accent "/" */
#define CP850_AacuteL   160   /* a (Lower case) with acute accent "/" */
#define CP850_AhatU     182   /* A (Upper case) with hat accent "^" */
#define CP850_AhatL     131   /* a (Lower case) with hat accent "^" */
#define CP850_AtildeU   199   /* A (Upper case) with tilde  "~" */
#define CP850_AtildeL   198   /* a (Lower case) with tilde  "~" */
#define CP850_AumlautU  142   /* A (Upper case) with umlaut ".." */
#define CP850_AumlautL  132   /* a (Lower case) with umlaut ".." */
#define CP850_AringU    143   /* A (Upper case) with ring above */
#define CP850_AringL    134   /* a (Lower case) with ring above */
#define CP850_AE_U      146   /* AE (Upper case) lingature */
#define CP850_AE_L      145   /* ae (Lower case) lingature */
#define CP850_CedillaU  128   /* C (Upper case) with cedilla */
#define CP850_CedillaL  135   /* C (LOwer case) with cedilla */
#define CP850_EgraveU   212   /* E (Upper case) with grave accent "\" */
#define CP850_EgraveL   138   /* e (Lower case) with grave accent "\" */
#define CP850_EacuteU   144   /* E (Upper case) with acute accent "/" */
#define CP850_EacuteL   130   /* e (Lower case) with acute accent "/" */
#define CP850_EhatU     210   /* E (Upper case) with hat accent "^" */
#define CP850_EhatL     136   /* e (Lower case) with hat accent "^" */
#define CP850_EumlautU  211   /* E (Upper case) with umlaut ".." */
#define CP850_EumlautL  137   /* e (Lower case) with umlaut ".." */
#define CP850_IgraveU   222   /* I (Upper case) with grave accent "\" */
#define CP850_IgraveL   141   /* i (Lower case) with grave accent "\" */
#define CP850_IacuteU   214   /* I (Upper case) with acute accent "/" */
#define CP850_IacuteL   161   /* i (Lower case) with acute accent "/" */
#define CP850_IhatU     215   /* I (Upper case) with hat accent "^" */
#define CP850_IhatL     140   /* i (Lower case) with hat accent "^" */
#define CP850_IumlautU  216   /* I (Upper case) with umlaut ".." */
#define CP850_IumlautL  139   /* i (Lower case) with umlaut ".." */
#define CP850_EthU	209   /* D (Upper case) with bar; letter "eth" */
#define CP850_EthL	208   /* D (lower case) with bar; letter "eth" */
#define CP850_NtildeU   165   /* N (Upper case) with tilde "~" */
#define CP850_NtildeL   164   /* n (Lower case) with tilde "~" */
#define CP850_OgraveU   227   /* O (Upper case) with grave accent "\" */
#define CP850_OgraveL   149   /* o (Lower case) with grave accent "\" */
#define CP850_OacuteU   224   /* O (Upper case) with acute accent "/" */
#define CP850_OacuteL   162   /* o (Lower case) with acute accent "/" */
#define CP850_OhatU     226   /* O (Upper case) with hat accent "^" */
#define CP850_OhatL     147   /* o (Lower case) with hat accent "^" */
#define CP850_OtildeU   229   /* O (Upper case) with tilde  "~" */
#define CP850_OtildeL   228   /* o (Lower case) with tilde  "~" */
#define CP850_OumlautU  153   /* O (Upper case) with umlaut ".." */
#define CP850_OumlautL  148   /* o (Lower case) with umlaut ".." */
#define CP850_OobliqU   157   /* O (Upper case) with oblique
				 stroke "/" inside */
#define CP850_OobliqL   155   /* o (Lower case) with oblique
				 stroke "/" inside */
#define CP850_Ssharp    225   /* ss German sharp s (double s)
				 looks like beta */
#define CP850_UgraveU   235   /* U (Upper case) with grave accent "\" */
#define CP850_UgraveL   151   /* u (Lower case) with grave accent "\" */
#define CP850_UacuteU   233   /* U (Upper case) with acute accent "/" */
#define CP850_UacuteL   163   /* u (Lower case) with acute accent "/" */
#define CP850_UhatU     234   /* U (Upper case) with hat accent "^" */
#define CP850_UhatL     150   /* u (Lower case) with hat accent "^" */
#define CP850_UumlautU  154   /* U (Upper case) with umlaut ".." */
#define CP850_UumlautL  129   /* u (Lower case) with umlaut ".." */
#define CP850_YacuteU   237   /* Y (Upper case) with acute accent "/" */
#define CP850_YacuteL   236   /* y (Lower case) with acute accent "/" */
#define CP850_ThornL    231   /* Lower-case 'thorn' */
#define CP850_ThornU    232   /* Upper-case 'thorn' */
#define CP850_YumlautU    ?   /* Y (Upper case) with umlaut ".." */
#define CP850_YumlautL  152   /* y (Lower case) with umlaut ".." */

#define CP850_Pound	156   /* British pound sign */
#define CP850_Mult      158   /* multiplication sign */
#define CP850_Florin    159   /* Dutch florin symbol */
#define CP850_FemOrd    166   /* Spanish feminine ordinal indicator */
#define CP850_MascOrd   167   /* Spanish masculine ordinal indicator */
#define CP850_InvHook   168   /* Inverted question mark */
#define CP850_RegTM     169   /* Registered trademark sign */
#define CP850_Not	170   /* Logical 'not' sign */
#define CP850_Half      171   /* one-half symbol */
#define CP850_Quarter   172   /* one-quarter symbol */
#define CP850_InvBang   173   /* Inverted exclamation point */
#define CP850_LDblAngle 174   /* Left double angle */
#define CP850_RDblAngle 175   /* Right double angle */
#define CP850_Copyright 184   /* Copyright symbol */
#define CP850_Cent      189   /* Cent sign */
#define CP850_Yen	190   /* Yen sign */
#define CP850_Currency  207   /* Currency sign */
#define CP850_BBar      221   /* Broken vertical bar */
#define CP850_Mu        230   /* Greek lower-case mu */
#define CP850_Acute     239   /* Acute accent */
#define CP850_PlusMinus 241   /* Plus/minus symbol */
#define CP850_Overline  238   /** Overline */
#define CP850_SylHyph   240   /* Syllable hyphen */
#define CP850_3Quarter  243   /* Three-quarter symbol */
#define CP850_Para	244   /* Paragraph symbol */
#define CP850_Section   245   /* Section symbol */
#define CP850_Division  246   /* Division symbol */
#define CP850_Cedilla   247   /* Cedilla symbol (without a "C") */
#define CP850_Degree    248   /* Degree symbol */
#define CP850_Umlaut    249   /* Umlaut diacritical mark */
#define CP850_MidDot    250   /* A centered middle dot */
#define CP850_Super1    251   /* Superscript 1 */
#define CP850_Super3    252   /* Superscript 3 */
#define CP850_Super2    253   /* Superscript 2 */
#define CP850_ReqSpace  255   /* Required space */

/* definition of the ISO-Latin-1 character set */
#define ISOL1_AgraveU   192   /* A (Upper case) with grave accent "\" */
#define ISOL1_AgraveL   224   /* a (Lower case) with grave accent "\" */
#define ISOL1_AacuteU   193   /* A (Upper case) with acute accent "/" */
#define ISOL1_AacuteL   225   /* a (Lower case) with acute accent "/" */
#define ISOL1_AhatU     194   /* A (Upper case) with hat accent "^" */
#define ISOL1_AhatL     226   /* a (Lower case) with hat accent "^" */
#define ISOL1_AtildeU   195   /* A (Upper case) with tilde  "~" */
#define ISOL1_AtildeL   227   /* a (Lower case) with tilde  "~" */
#define ISOL1_AumlautU  196   /* A (Upper case) with umlaut ".." */
#define ISOL1_AumlautL  228   /* a (Lower case) with umlaut ".." */
#define ISOL1_AringU    197   /* A (Upper case) with ring above */
#define ISOL1_AringL    229   /* a (Lower case) with ring above */
#define ISOL1_AE_U      198   /* AE (Upper case) lingature */
#define ISOL1_AE_L      230   /* ae (Lower case) lingature */
#define ISOL1_CedillaU  199   /* C (Upper case) with cedilla */
#define ISOL1_CedillaL  231   /* C (LOwer case) with cedilla */
#define ISOL1_EgraveU   200   /* E (Upper case) with grave accent "\" */
#define ISOL1_EgraveL   232   /* e (Lower case) with grave accent "\" */
#define ISOL1_EacuteU   201   /* E (Upper case) with acute accent "/" */
#define ISOL1_EacuteL   233   /* e (Lower case) with acute accent "/" */
#define ISOL1_EhatU     202   /* E (Upper case) with hat accent "^" */
#define ISOL1_EhatL     234   /* e (Lower case) with hat accent "^" */
#define ISOL1_EumlautU  203   /* E (Upper case) with umlaut ".." */
#define ISOL1_EumlautL  235   /* e (Lower case) with umlaut ".." */
#define ISOL1_IgraveU   204   /* I (Upper case) with grave accent "\" */
#define ISOL1_IgraveL   236   /* i (Lower case) with grave accent "\" */
#define ISOL1_IacuteU   205   /* I (Upper case) with acute accent "/" */
#define ISOL1_IacuteL   237   /* i (Lower case) with acute accent "/" */
#define ISOL1_IhatU     206   /* I (Upper case) with hat accent "^" */
#define ISOL1_IhatL     238   /* i (Lower case) with hat accent "^" */
#define ISOL1_IumlautU  207   /* I (Upper case) with umlaut ".." */
#define ISOL1_IumlautL  239   /* i (Lower case) with umlaut ".." */
#define ISOL1_EthU	208   /* D (Upper case) with bar; letter "eth" */
#define ISOL1_EthL	240   /* D (lower case) with bar; letter "eth" */
#define ISOL1_NtildeU   209   /* N (Upper case) with tilde "~" */
#define ISOL1_NtildeL   241   /* n (Lower case) with tilde "~" */
#define ISOL1_OgraveU   210   /* O (Upper case) with grave accent "\" */
#define ISOL1_OgraveL   242   /* o (Lower case) with grave accent "\" */
#define ISOL1_OacuteU   211   /* O (Upper case) with acute accent "/" */
#define ISOL1_OacuteL   243   /* o (Lower case) with acute accent "/" */
#define ISOL1_OhatU     212   /* O (Upper case) with hat accent "^" */
#define ISOL1_OhatL     244   /* o (Lower case) with hat accent "^" */
#define ISOL1_OtildeU   213   /* O (Upper case) with tilde  "~" */
#define ISOL1_OtildeL   245   /* o (Lower case) with tilde  "~" */
#define ISOL1_OumlautU  214   /* O (Upper case) with umlaut ".." */
#define ISOL1_OumlautL  246   /* o (Lower case) with umlaut ".." */
#define ISOL1_OobliqU   216   /* O (Upper case) with oblique
				 stroke "/" inside */
#define ISOL1_OobliqL   248   /* o (Lower case) with oblique
				 stroke "/" inside */
#define ISOL1_Ssharp    223   /* ss German sharp s (double s)
				 looks like beta */
#define ISOL1_UgraveU   217   /* U (Upper case) with grave accent "\" */
#define ISOL1_UgraveL   249   /* u (Lower case) with grave accent "\" */
#define ISOL1_UacuteU   218   /* U (Upper case) with acute accent "/" */
#define ISOL1_UacuteL   250   /* u (Lower case) with acute accent "/" */
#define ISOL1_UhatU     219   /* U (Upper case) with hat accent "^" */
#define ISOL1_UhatL     251   /* u (Lower case) with hat accent "^" */
#define ISOL1_UumlautU  220   /* U (Upper case) with umlaut ".." */
#define ISOL1_UumlautL  252   /* u (Lower case) with umlaut ".." */
#define ISOL1_YacuteU   221   /* Y (Upper case) with acute accent "/" */
#define ISOL1_YacuteL   253   /* y (Lower case) with acute accent "/" */
#define ISOL1_ThornU    222   /* Upper-case 'thorn' */
#define ISOL1_ThornL    254   /* Lower-case 'thorn' */
#define ISOL1_YumlautU    ?   /* Y (Upper case) with umlaut "..",
			       available in ISO-Latin-2, but not 1 */
#define ISOL1_YumlautL  255   /* y (Lower case) with umlaut ".." */

#define ISOL1_Pound     163   /* British pound sign */
#define ISOL1_Mult      215   /* Multiplication sign */
#define ISOL1_Florin    159   /* Dutch florin symbol */
#define ISOL1_FemOrd    170   /* Spanish feminine ordinal indicator */
#define ISOL1_MascOrd   186   /* Spanish masculine ordinal indicator */
#define ISOL1_InvHook   191   /* Inverted question mark */
#define ISOL1_RegTM     174   /* Registered trademark sign */
#define ISOL1_Not       172   /* Logical 'not' sign */
#define ISOL1_Half      189   /* one-half symbol */
#define ISOL1_Quarter   188   /* one-quarter symbol */
#define ISOL1_InvBang   161   /* Inverted exclamation point */
#define ISOL1_LDblAngle 171   /* Left double angle */
#define ISOL1_RDblAngle 187   /* Right double angle */
#define ISOL1_Copyright 169   /* Copyright symbol */
#define ISOL1_Cent      162   /* Cent sign */
#define ISOL1_Yen	165   /* Yen sign */
#define ISOL1_Currency  164   /* Currency sign */
#define ISOL1_BBar      166   /* Broken vertical bar */
#define ISOL1_Mu        181   /* Greek lower-case mu */
#define ISOL1_Acute     180   /* Acute accent */
#define ISOL1_PlusMinus 177   /* Plus/minus symbol */
#define ISOL1_Overline  175   /** Overline */
#define ISOL1_SylHyph   173   /* Syllable hyphen */
#define ISOL1_3Quarter  190   /* Three-quarter symbol */
#define ISOL1_Para	182   /* Paragraph symbol */
#define ISOL1_Section   167   /* Section symbol */
#define ISOL1_Division  247   /* Division symbol */
#define ISOL1_Cedilla   184   /* Cedilla symbol (without a "C") */
#define ISOL1_Degree    176   /* Degree symbol */
#define ISOL1_Umlaut    168   /* Umlaut diacritical mark */
#define ISOL1_MidDot    183   /* A centered middle dot */
#define ISOL1_Super1    185   /* Superscript 1 */
#define ISOL1_Super3    179   /* Superscript 3 */
#define ISOL1_Super2    178   /* Superscript 2 */
#define ISOL1_ReqSpace  160   /* Required space */

/* 
 * The following correspond to CP850 graphics characters.  They are 
 * mapped to unused areas of ISO-Latin-1, and will hopefully help keep 
 * the mapping close to one-to-one.
 */
#define ISOL1_Block1	128     /* 25% grey block */
#define ISOL1_Block2    129	/* 50% grey block */
#define ISOL1_Block3    130     /* 75% grey block */
#define ISOL1_VertSgl   131     /* Single vertical border line */
#define ISOL1_VertSglL  132     /* Single vertical border
				   w/left intersection */
#define ISOL1_VertDblL  133     /* Double vertical broder w/left
				   intersection */
#define ISOL1_VertDbl   134     /* Double vertical border line */
#define ISOL1_URCDbl    135     /* Upper right corner, double line */
#define ISOL1_LRCDbl    136	/* Lower right corder, double line */
#define ISOL1_URCSgl    137     /* Upper right corner, single line */
#define ISOL1_LLCSgl    138	/* Lower left corner, single line */
#define ISOL1_BTSgl     139	/* Bottom 'T', single line */
#define ISOL1_UTSgl	140     /* Top 'T', single line */
#define ISOL1_VertSglR  141	/* Single vertical border, with
				   right intersection */
#define ISOL1_HorizSgl  142     /* Single horizontal border line */
#define ISOL1_InterSgl  143   	/* Single line intersection */
#define ISOL1_LLCDbl    144     /* Lower left corner, double line */
#define ISOL1_ULCDbl    145     /* Upper left corner, double line */
#define ISOL1_BTDbl     146     /* Bottom 'T', double line */
#define ISOL1_UTDbl     147     /* Top 'T', double line */
#define ISOL1_VertDblR  148     /* Double vertical border w/ right
				   intersection */
#define ISOL1_HorizDbl  149	/* Double horizontal border line */
#define ISOL1_InterDbl  150     /* Double line intersection */
#define ISOL1_DotlessI  151     /* Dotless small i */
#define ISOL1_LRCSgl    152     /* Lower right corner, single line */
#define ISOL1_ULCSgl    153    	/* Upper left corner, single line */
#define ISOL1_SolidBlk  154     /* Solid block */
#define ISOL1_LowerBlk  155	/* Lower half of a solid block */
#define ISOL1_UpperBlk  156	/* Upper half of a solid block */
#define ISOL1_DblUnder  157     /* Double Underscore */
#define ISOL1_MidBox    158     /* Centered solid box */

#define CP850_Block1	176     /* 25% grey block */
#define CP850_Block2    177	/* 50% grey block */
#define CP850_Block3    178     /* 75% grey block */
#define CP850_VertSgl   179     /* Single vertical border line */
#define CP850_VertSglL  180     /* Single vertical border
				   w/left intersection */
#define CP850_VertDblL  185     /* Double vertical broder w/left
				   intersection */
#define CP850_VertDbl   186     /* Double vertical border line */
#define CP850_URCDbl    187     /* Upper right corner, double line */
#define CP850_LRCDbl    188	/* Lower right corder, double line */
#define CP850_URCSgl    191     /* Upper right corner, single line */
#define CP850_LLCSgl    192	/* Lower left corner, single line */
#define CP850_BTSgl     193	/* Bottom 'T', single line */
#define CP850_UTSgl	194     /* Top 'T', single line */
#define CP850_VertSglR  195	/* Single vertical border, with
				   right intersection */
#define CP850_HorizSgl  196     /* Single horizontal border line */
#define CP850_InterSgl  197   	/* Single line intersection */
#define CP850_LLCDbl    200     /* Lower left corner, double line */
#define CP850_ULCDbl    201     /* Upper left corner, double line */
#define CP850_BTDbl     202     /* Bottom 'T', double line */
#define CP850_UTDbl     203     /* Top 'T', double line */
#define CP850_VertDblR  204     /* Double vertical border w/ right
				   intersection */
#define CP850_HorizDbl  205	/* Double horizontal border line */
#define CP850_InterDbl  206     /* Double line intersection */
#define CP850_DotlessI  213     /* Dotless small i */
#define CP850_LRCSgl    217     /* Lower right corner, single line */
#define CP850_ULCSgl    218    	/* Upper left corner, single line */
#define CP850_SolidBlk  219     /* Solid block */
#define CP850_LowerBlk  220	/* Lower half of a solid block */
#define CP850_UpperBlk  223	/* Upper half of a solid block */
#define CP850_DblUnder  242     /* Double Underscore */
#define CP850_MidBox    254     /* Centered solid box */

struct  language 
{
    TEXT        upscii; /* UPSCII (8 bits) value */
    TEXT        weight; /* collating weight */
};

struct delim
{
    TEXT        substit; /* to substitute what follows ";" */
    TEXT        delimit; /* with the Progress delimiter */
};

struct updnpair
{
    TEXT        lower; /* lower case of letter */
    TEXT        upper; /* upper case of letter */
};

struct TableNameMap
{
    TEXT *TableName;  /* Name of table */
    COUNT TypeNum;    /* Internal type code */
};

#include "lchdr.h"

/* Collation table type numbers */
#define COLL_BASIC	0
#define COLL_DANISH	1
#define COLL_SWEDISH    2
#define COLL_USERCOLL   3

#ifdef  UPGLOB
GLOBAL  struct  delim   PROCONST("upctl_data_TEXT") delimtbl[]=
        {
             {'&',        '@'},   
             {'(',        '{'},   
             {')',        '}'},   
             {'*',        '^'},   
             {'\'',       '`'},   
             {'<',        '['},   
             {'%',        '|'},   
             {'>',        ']'},   
             {'?',        '~'},   
            {'\0',       '\0'}
        };


        struct  language    PROCONST("upctl_data_TEXT")     ISOL1_basic[]=
            /*  upscii  weight  */
        {
                {ISOL1_AumlautL,'A'}, /* A-umlaut sorts as A */
                {ISOL1_AumlautU,'A'}, /* A-umlaut sorts as A */
                {ISOL1_OumlautL,'O'}, /* O-umlaut sorts as O */
                {ISOL1_OumlautU,'O'}, /* O-umlaut sorts as O */
                {ISOL1_UumlautL,'U'}, /* U-umlaut sorts as U */
                {ISOL1_UumlautU,'U'}, /* U-umlaut sorts as U */
                {'\0',   '\0'}
        };

        struct  language   PROCONST("upctl_data_TEXT")  ISOL1_csbasic[]=
            /*  upscii  weight  for case-sensitive fields */
        {
                {ISOL1_AumlautL,'a'}, /* a-umlaut sorts as a */
                {ISOL1_AumlautU,'A'}, /* A-umlaut sorts as A */
                {ISOL1_OumlautL,'o'}, /* o-umlaut sorts as o */
                {ISOL1_OumlautU,'O'}, /* O-umlaut sorts as O */
                {ISOL1_UumlautL,'u'}, /* u-umlaut sorts as u */
                {ISOL1_UumlautU,'U'}, /* U-umlaut sorts as U */
                {'\0',   '\0'}
        };

        struct  language   PROCONST("upctl_data_TEXT")   ISOL1_danish[]=
            /*  upscii  weight  */
        {
                {ISOL1_AE_L,   'x'}, /* AE  sorts > Z*/     
                {ISOL1_AE_U,   'x'}, /* AE  sorts > Z*/
                {ISOL1_OobliqL,'y'}, /* O-slash sorts > AE*/            
                {ISOL1_OobliqU,'y'}, /* O-slash sorts > AE*/
                {ISOL1_AringL, 'z'}, /* A-circle > O-slash*/
                {ISOL1_AringU, 'z'}, /* A-circle > O-slash*/
                {ISOL1_AumlautL,'x'}, /* A-umlaut sorts like AE_U */
                {ISOL1_AumlautU,'x'}, /* A-umlaut sorts like AE_U */
                {ISOL1_OumlautL,'y'}, /* O-umlaut sorts like OobliqU */
                {ISOL1_OumlautU,'y'}, /* O-umlaut sorts like OobliqU */
                {ISOL1_UumlautL,'Y'}, /* U-umlaut sorts like Y */
                {ISOL1_UumlautU,'Y'}, /* U-umlaut sorts like Y */
                {'\0',   '\0'}
        };

        struct  language    PROCONST("upctl_data_TEXT")   ISOL1_csdanish[]=
            /*   weight  for case-sensitive fields */
        /* 
         * Note: the punctuation characters are used because they 
         * conveniently fall after 'z' or 'Z', and aren't likely to 
         * conflict with other characters in the index.
         */
        {
                {ISOL1_AE_L,     '{'}, /* ae  sorts > z*/     
                {ISOL1_AE_U,     '['}, /* AE  sorts > Z*/
                {ISOL1_OobliqL,  '|'}, /* o-slash sorts > ae*/            
                {ISOL1_OobliqU,  '\\'}, /* O-slash sorts > AE*/
                {ISOL1_AringL,   '}'}, /* a-circle > o-slash*/
                {ISOL1_AringU,   ']'}, /* A-circle > O-slash*/
                {ISOL1_AumlautL, '{'}, /* a-umlaut sorts like AE_L */
                {ISOL1_AumlautU, '['}, /* A-umlaut sorts like AE_U */
                {ISOL1_OumlautL, '|'}, /* o-umlaut sorts like OobliqL */
                {ISOL1_OumlautU, '\\'}, /* O-umlaut sorts like OobliqU */
                {ISOL1_UumlautL, 'y'}, /* u-umlaut sorts like y */
                {ISOL1_UumlautU, 'Y'}, /* U-umlaut sorts like Y */
                {'\0',   '\0'}
        };

        struct  language  PROCONST("upctl_data_TEXT")  ISOL1_swedish[]=
            /*  upscii  weight  */
        {
                {ISOL1_AumlautL,'y'},/* A-umlaut > A-ring*/
                {ISOL1_AumlautU,'y'},/* A-umlaut > A-ring*/
                {ISOL1_OumlautL,'z'}, /* O-umlaut > A-umlaut*/
                {ISOL1_OumlautU,'z'}, /* O-umlaut > A-umlaut*/
                {ISOL1_AringL,  'x'}, /* A-ring sorts > Z */
                {ISOL1_AringU,  'x'}, /* A-ring sorts > Z */
                {ISOL1_UumlautL,'U'}, /* U-umlaut sorts as U */
                {ISOL1_UumlautU,'U'}, /* U-umlaut sorts as U */
                {ISOL1_EacuteL, 'E'}, /* E-accent sorts as E*/
                {ISOL1_EacuteU, 'E'}, /* E-accent sorts as E*/
                {'\0',   '\0'}

        };

        struct  language    PROCONST("upctl_data_TEXT") ISOL1_csswedish[]=
            /*  upscii  weight  for case-sensitive fields */
        /* 
         * Note: the punctuation characters are used because they 
         * conveniently fall after 'z' or 'Z', and aren't likely to 
         * conflict with other characters in the index.
         */
        {
                {ISOL1_AumlautL, '|'},/* a-umlaut > a-ring*/
                {ISOL1_AumlautU, '\\'},/* A-umlaut > A-ring*/
                {ISOL1_OumlautL, '}'}, /* o-umlaut > a-umlaut*/
                {ISOL1_OumlautU, ']'}, /* O-umlaut > A-umlaut*/
                {ISOL1_AringL,   '{'}, /* a-ring sorts > z */
                {ISOL1_AringU,   '['}, /* A-ring sorts > Z */
                {ISOL1_UumlautL, 'u'}, /* u-umlaut sorts as u */
                {ISOL1_UumlautU, 'U'}, /* U-umlaut sorts as U */
                {ISOL1_EacuteL,  'e'}, /* e-accent sorts as e*/
                {ISOL1_EacuteU,  'E'}, /* E-accent sorts as E*/
                {'\0',   '\0'}

        };


/* pointer to language table for ISO-Latin-1 */
GLOBAL  struct  language   PROCONST("upctl_data_TEXT") *ISOL1_plang[]=
        {
                ISOL1_basic,
                ISOL1_danish,
                ISOL1_swedish,
                NULL
        };

/* pointer to case-sens lang tbl for ISO-Latin-1 */
GLOBAL  struct  language   PROCONST("upctl_data_TEXT")  *ISOL1_pcslang[]= 
        {
                ISOL1_csbasic,
                ISOL1_csdanish,
                ISOL1_csswedish,
                NULL
        };

/*** uptable and dntable for both UNIX and MS-DOS***/
/**** to replace the tables in utsta.asm ***/
/* 
 * Note: there are identical tables, ISOL1_uptablelcl and 
 * CP850_uptablelcl, which follows these
 * Its purpose is to remain pure and pristine even if the global 
 * uptable has remappings which can cause Progress keyword lookup to 
 * fail (and hence have to compiler die painful multiple deaths). So, 
 * if uptable changes, uptablelcl must also be changed.
 */
GLOBAL  TEXT   ISOL1_uptable[256] = {
      0 , 1, 2,3,4,5,6,7
      ,   8,9,10,11,12,13,14,15
      ,  16,17,18,19,20,21,22,23
      ,  24,25,26,27,28,29,30,31
      ,  32,33,34,35,36,37,38,39
      ,  40,41,42,43,44,45,46,47
      ,  48,49,50,51,52,53,54,55
      ,  56,57,58,59,60,61,62,63

      /* 0x40 */
      ,  64,'A','B','C','D','E','F','G'
      /**,  64,65,66,67,68,69,70,71**/

      /* 0x48 */
      ,  'H','I','J','K','L','M','N','O'
      /**,  72,73,74,75,76,77,78,79**/

      /* 0x50 */
      ,  'P','Q','R','S','T','U','V','W'
      /**,  80,81,82,83,84,85,86,87**/

      /* 0x58 */
      ,  'X','Y','Z',91,92,93,94,95
      /**,  88,89,90,91,92,93,94,95**/

      /* 0x60 */
      ,  96,'A','B','C','D','E','F','G'
      /**,  96,65,66,67,68,69,70,71**/

      /* 0x68 */
      ,  'H','I','J','K','L','M','N','O'
      /**,  72,73,74,75,76,77,78,79**/

      /* 0x70 */
      ,  'P','Q','R','S','T','U','V','W'
      /**,  80,81,82,83,84,85,86,87**/

      /* 0x78 */
      ,  'X','Y','Z',123,124,125,126,127
      /**,  88,89,90,123,124,125,126,127**/

      /* 0x80 */
      ,  128,129,130,131,132,133,134,135

      /* 0x88 */
      ,  136,137,138,139,140,141,142,143

      /* 0x90 */
      ,  144,145,146,147,148,149,150,151

      /* 0x98 */
      ,  152,153,154,155,156,157,158,159

      /* 0xA0 */
      ,  160,161,162,163,164,165,166,167

      /* 0xA8 */
      ,  168,169,170,171,172,173,174,175

      /* 0xB0 */
      ,  176,177,178,179,180,181,182,183

      /* 0xB8 */
      ,  184,185,186,187,188,189,190,191
          
      /* 0xC0 */
      ,  ISOL1_AgraveU,  ISOL1_AacuteU, ISOL1_AhatU, ISOL1_AtildeU,
	 ISOL1_AumlautU, ISOL1_AringU,  ISOL1_AE_U,  ISOL1_CedillaU

      /* 0xC8 */          
      ,  ISOL1_EgraveU,  ISOL1_EacuteU, ISOL1_EhatU, ISOL1_EumlautU,
	 ISOL1_IgraveU,	 ISOL1_IacuteU, ISOL1_IhatU, ISOL1_IumlautU
	
      /* 0xD0 */
      ,  ISOL1_EthU,     ISOL1_NtildeU, ISOL1_OgraveU, ISOL1_OacuteU,
	 ISOL1_OhatU,    ISOL1_OtildeU, ISOL1_OumlautU, 0xD7

      /* 0xD8 */          
      ,  ISOL1_OobliqU,  ISOL1_UgraveU, ISOL1_UacuteU, ISOL1_UhatU,
	 ISOL1_UumlautU, ISOL1_YacuteU, ISOL1_ThornU,  ISOL1_Ssharp
          
      /* 0xE0 */
      ,  ISOL1_AgraveU,  ISOL1_AacuteU, ISOL1_AhatU, ISOL1_AtildeU,
	 ISOL1_AumlautU, ISOL1_AringU,  ISOL1_AE_U,  ISOL1_CedillaU

      /* 0xE8 */
      ,  ISOL1_EgraveU,  ISOL1_EacuteU, ISOL1_EhatU, ISOL1_EumlautU,
	 ISOL1_IgraveU,	 ISOL1_IacuteU, ISOL1_IhatU, ISOL1_IumlautU

      /* 0xF0 */
      ,  ISOL1_EthU,     ISOL1_NtildeU, ISOL1_OgraveU, ISOL1_OacuteU,
	 ISOL1_OhatU,    ISOL1_OtildeU, ISOL1_OumlautU, 0xF7

      /* 0xF8 */
      ,  ISOL1_OobliqU,  ISOL1_UgraveU, ISOL1_UacuteU, ISOL1_UhatU,
	 ISOL1_UumlautU, ISOL1_YacuteU, ISOL1_ThornU,  'Y'
      }; /* end ISOL1_uptable */

GLOBAL  TEXT  PROCONST("upctl_data_TEXT") ISOL1_uptablelcl[256] = {
        0,1,2,3,4,5,6,7
      ,   8,9,10,11,12,13,14,15
      ,  16,17,18,19,20,21,22,23
      ,  24,25,26,27,28,29,30,31
      ,  32,33,34,35,36,37,38,39
      ,  40,41,42,43,44,45,46,47
      ,  48,49,50,51,52,53,54,55
      ,  56,57,58,59,60,61,62,63

      /* 0x40 */
      ,  64,'A','B','C','D','E','F','G'
      /**,  64,65,66,67,68,69,70,71**/

      /* 0x48 */
      ,  'H','I','J','K','L','M','N','O'
      /**,  72,73,74,75,76,77,78,79**/

      /* 0x50 */
      ,  'P','Q','R','S','T','U','V','W'
      /**,  80,81,82,83,84,85,86,87**/

      /* 0x58 */
      ,  'X','Y','Z',91,92,93,94,95
      /**,  88,89,90,91,92,93,94,95**/

      /* 0x60 */
      ,  96,'A','B','C','D','E','F','G'
      /**,  96,65,66,67,68,69,70,71**/

      /* 0x68 */
      ,  'H','I','J','K','L','M','N','O'
      /**,  72,73,74,75,76,77,78,79**/

      /* 0x70 */
      ,  'P','Q','R','S','T','U','V','W'
      /**,  80,81,82,83,84,85,86,87**/

      /* 0x78 */
      ,  'X','Y','Z',123,124,125,126,127
      /**,  88,89,90,123,124,125,126,127**/

      /* 0x80 */
      ,  128,129,130,131,132,133,134,135

      /* 0x88 */
      ,  136,137,138,139,140,141,142,143

      /* 0x90 */
      ,  144,145,146,147,148,149,150,151

      /* 0x98 */
      ,  152,153,154,155,156,157,158,159

      /* 0xA0 */
      ,  160,161,162,163,164,165,166,167

      /* 0xA8 */
      ,  168,169,170,171,172,173,174,175

      /* 0xB0 */
      ,  176,177,178,179,180,181,182,183

      /* 0xB8 */
      ,  184,185,186,187,188,189,190,191
          
      /* 0xC0 */
      ,  ISOL1_AgraveU,  ISOL1_AacuteU, ISOL1_AhatU, ISOL1_AtildeU,
	 ISOL1_AumlautU, ISOL1_AringU,  ISOL1_AE_U,  ISOL1_CedillaU

      /* 0xC8 */          
      ,  ISOL1_EgraveU,  ISOL1_EacuteU, ISOL1_EhatU, ISOL1_EumlautU,
	 ISOL1_IgraveU,	 ISOL1_IacuteU, ISOL1_IhatU, ISOL1_IumlautU
	
      /* 0xD0 */
      ,  ISOL1_EthU,     ISOL1_NtildeU, ISOL1_OgraveU, ISOL1_OacuteU,
	 ISOL1_OhatU,    ISOL1_OtildeU, ISOL1_OumlautU, 0xD7

      /* 0xD8 */          
      ,  ISOL1_OobliqU,  ISOL1_UgraveU, ISOL1_UacuteU, ISOL1_UhatU,
	 ISOL1_UumlautU, ISOL1_YacuteU, ISOL1_ThornU,  ISOL1_Ssharp
          
      /* 0xE0 */
      ,  ISOL1_AgraveU,  ISOL1_AacuteU, ISOL1_AhatU, ISOL1_AtildeU,
	 ISOL1_AumlautU, ISOL1_AringU,  ISOL1_AE_U,  ISOL1_CedillaU

      /* 0xE8 */
      ,  ISOL1_EgraveU,  ISOL1_EacuteU, ISOL1_EhatU, ISOL1_EumlautU,
	 ISOL1_IgraveU,	 ISOL1_IacuteU, ISOL1_IhatU, ISOL1_IumlautU

      /* 0xF0 */
      ,  ISOL1_EthU,     ISOL1_NtildeU, ISOL1_OgraveU, ISOL1_OacuteU,
	 ISOL1_OhatU,    ISOL1_OtildeU, ISOL1_OumlautU, 0xF7

      /* 0xF8 */
      ,  ISOL1_OobliqU,  ISOL1_UgraveU, ISOL1_UacuteU, ISOL1_UhatU,
	 ISOL1_UumlautU, ISOL1_YacuteU, ISOL1_ThornU,  'Y'
      }; /* end ISOL1_uptablelcl */

GLOBAL  TEXT    ISOL1_dntable[256] = {
               0,   1, 2,3,4,5,6,7
      ,       8,9,10,11,12,13,14,15
      ,      16,17,18,19,20,21,22,23
      ,      24,25,26,27,28,29,30,31
      ,      32,33,34,35,36,37,38,39
      ,      40,41,42,43,44,45,46,47
      ,      48,49,50,51,52,53,54,55
      ,      56,57,58,59,60,61,62,63

      /* 0x40 */
      ,  64,'a','b','c','d','e','f','g'
      /**,      64,97,98,99,100,101,102,103**/

      /* 0x48 */
      ,  'h','i','j','k','l','m','n','o'
      /**,     104,105,106,107,108,109,110,111**/

      /* 0x50 */
      ,  'p','q','r','s','t','u','v','w'
      /**,     112,113,114,115,116,117,118,119**/

      /* 0x58 */
      ,  'x','y','z',91,92,93,94,95
      /**,     120,121,122,91,92,93,94,95**/

      /* 0x60 */
      ,  96,'a','b','c','d','e','f','g'
      /**,      96,97,98,99,100,101,102,103**/

      /* 0x68 */
      ,  'h','i','j','k','l','m','n','o'
      /**,     104,105,106,107,108,109,110,111**/

      /* 0x70 */
      ,  'p','q','r','s','t','u','v','w'
      /**,     112,113,114,115,116,117,118,119**/

      /* 0x78 */
      ,     'x','y','z',123,124,125,126,127
      /**,     120,121,122,123,124,125,126,127**/

      /* 0x80 */
      ,  128,129,130,131,132,133,134,135

      /* 0x88 */
      ,  136,137,138,139,140,141,142,143

      /* 0x90 */
      ,  144,145,146,147,148,149,150,151

      /* 0x98 */
      ,  152,153,154,155,156,157,158,159

      /* 0xA0 */
      ,  160,161,162,163,164,165,166,167
  
      /* 0xA8 */
      ,  168,169,170,171,172,173,174,175

      /* 0xB0 */
      ,  176,177,178,179,180,181,182,183

      /* 0xB8 */
      ,  184,185,186,187,188,189,190,191

      /* 0xC0 */
      ,  ISOL1_AgraveL,  ISOL1_AacuteL, ISOL1_AhatL, ISOL1_AtildeL,
	 ISOL1_AumlautL, ISOL1_AringL,  ISOL1_AE_L,  ISOL1_CedillaL

      /* 0xC8 */          
      ,  ISOL1_EgraveL,  ISOL1_EacuteL, ISOL1_EhatL, ISOL1_EumlautL,
	 ISOL1_IgraveL,	 ISOL1_IacuteL, ISOL1_IhatL, ISOL1_IumlautL
	
      /* 0xD0 */
      ,  ISOL1_EthL,     ISOL1_NtildeL, ISOL1_OgraveL, ISOL1_OacuteL,
	 ISOL1_OhatL,    ISOL1_OtildeL, ISOL1_OumlautL, 0xD7

      /* 0xD8 */          
      ,  ISOL1_OobliqL,  ISOL1_UgraveL, ISOL1_UacuteL, ISOL1_UhatL,
	 ISOL1_UumlautL, ISOL1_YacuteL, ISOL1_ThornL,  ISOL1_Ssharp
          
      /* 0xE0 */
      ,  ISOL1_AgraveL,  ISOL1_AacuteL, ISOL1_AhatL, ISOL1_AtildeL,
	 ISOL1_AumlautL, ISOL1_AringL,  ISOL1_AE_L,  ISOL1_CedillaL

      /* 0xE8 */
      ,  ISOL1_EgraveL,  ISOL1_EacuteL, ISOL1_EhatL, ISOL1_EumlautL,
	 ISOL1_IgraveL,	 ISOL1_IacuteL, ISOL1_IhatL, ISOL1_IumlautL

      /* 0xF0 */
      ,  ISOL1_EthL,     ISOL1_NtildeL, ISOL1_OgraveL, ISOL1_OacuteL,
	 ISOL1_OhatL,    ISOL1_OtildeL, ISOL1_OumlautL, 0xF7

      /* 0xF8 */
      ,  ISOL1_OobliqL,  ISOL1_UgraveL, ISOL1_UacuteL, ISOL1_UhatL,
	 ISOL1_UumlautL, ISOL1_YacuteL, ISOL1_ThornL,  ISOL1_YumlautL
      };  /* ISOL1_dntable */

GLOBAL  TEXT   PROCONST("upctl_data_TEXT")  ISOL1_dntablelcl[256] = {
                0,1,2,3,4,5,6,7
      ,       8,9,10,11,12,13,14,15
      ,      16,17,18,19,20,21,22,23
      ,      24,25,26,27,28,29,30,31
      ,      32,33,34,35,36,37,38,39
      ,      40,41,42,43,44,45,46,47
      ,      48,49,50,51,52,53,54,55
      ,      56,57,58,59,60,61,62,63

      /* 0x40 */
      ,  64,'a','b','c','d','e','f','g'
      /**,      64,97,98,99,100,101,102,103**/

      /* 0x48 */
      ,  'h','i','j','k','l','m','n','o'
      /**,     104,105,106,107,108,109,110,111**/

      /* 0x50 */
      ,  'p','q','r','s','t','u','v','w'
      /**,     112,113,114,115,116,117,118,119**/

      /* 0x58 */
      ,  'x','y','z',91,92,93,94,95
      /**,     120,121,122,91,92,93,94,95**/

      /* 0x60 */
      ,  96,'a','b','c','d','e','f','g'
      /**,      96,97,98,99,100,101,102,103**/

      /* 0x68 */
      ,  'h','i','j','k','l','m','n','o'
      /**,     104,105,106,107,108,109,110,111**/

      /* 0x70 */
      ,  'p','q','r','s','t','u','v','w'
      /**,     112,113,114,115,116,117,118,119**/

      /* 0x78 */
      ,     'x','y','z',123,124,125,126,127
      /**,     120,121,122,123,124,125,126,127**/

      /* 0x80 */
      ,  128,129,130,131,132,133,134,135

      /* 0x88 */
      ,  136,137,138,139,140,141,142,143

      /* 0x90 */
      ,  144,145,146,147,148,149,150,151

      /* 0x98 */
      ,  152,153,154,155,156,157,158,159

      /* 0xA0 */
      ,  160,161,162,163,164,165,166,167
  
      /* 0xA8 */
      ,  168,169,170,171,172,173,174,175

      /* 0xB0 */
      ,  176,177,178,179,180,181,182,183

      /* 0xB8 */
      ,  184,185,186,187,188,189,190,191

      /* 0xC0 */
      ,  ISOL1_AgraveL,  ISOL1_AacuteL, ISOL1_AhatL, ISOL1_AtildeL,
	 ISOL1_AumlautL, ISOL1_AringL,  ISOL1_AE_L,  ISOL1_CedillaL

      /* 0xC8 */          
      ,  ISOL1_EgraveL,  ISOL1_EacuteL, ISOL1_EhatL, ISOL1_EumlautL,
	 ISOL1_IgraveL,	 ISOL1_IacuteL, ISOL1_IhatL, ISOL1_IumlautL
	
      /* 0xD0 */
      ,  ISOL1_EthL,     ISOL1_NtildeL, ISOL1_OgraveL, ISOL1_OacuteL,
	 ISOL1_OhatL,    ISOL1_OtildeL, ISOL1_OumlautL, 0xD7

      /* 0xD8 */          
      ,  ISOL1_OobliqL,  ISOL1_UgraveL, ISOL1_UacuteL, ISOL1_UhatL,
	 ISOL1_UumlautL, ISOL1_YacuteL, ISOL1_ThornL,  ISOL1_Ssharp
          
      /* 0xE0 */
      ,  ISOL1_AgraveL,  ISOL1_AacuteL, ISOL1_AhatL, ISOL1_AtildeL,
	 ISOL1_AumlautL, ISOL1_AringL,  ISOL1_AE_L,  ISOL1_CedillaL

      /* 0xE8 */
      ,  ISOL1_EgraveL,  ISOL1_EacuteL, ISOL1_EhatL, ISOL1_EumlautL,
	 ISOL1_IgraveL,	 ISOL1_IacuteL, ISOL1_IhatL, ISOL1_IumlautL

      /* 0xF0 */
      ,  ISOL1_EthL,     ISOL1_NtildeL, ISOL1_OgraveL, ISOL1_OacuteL,
	 ISOL1_OhatL,    ISOL1_OtildeL, ISOL1_OumlautL, 0xF7

      /* 0xF8 */
      ,  ISOL1_OobliqL,  ISOL1_UgraveL, ISOL1_UacuteL, ISOL1_UhatL,
	 ISOL1_UumlautL, ISOL1_YacuteL, ISOL1_ThornL,  ISOL1_YumlautL
      };  /* ISOL1_dntablelcl */

#if 0
GLOBAL  TEXT    ISOL1_cstable[256] = {
        0,1,2,3,4,5,6,7
      ,   8,9,10,11,12,13,14,15
      ,  16,17,18,19,20,21,22,23
      ,  24,25,26,27,28,29,30,31
      ,  32,33,34,35,36,37,38,39
      ,  40,41,42,43,44,45,46,47
      ,  48,49,50,51,52,53,54,55
      ,  56,57,58,59,60,61,62,63

      /* 0x40 */
      ,  64,65,66,67,68,69,70,71

      /* 0x48 */
      ,  72,73,74,75,76,77,78,79

      /* 0x50 */
      ,  80,81,82,83,84,85,86,87

      /* 0x58 */
      ,  88,89,90,91,92,93,94,95

      /* 0x60 */
      ,  96,97,98,99,100,101,102,103

      /* 0x68 */
      ,  104,105,106,107,108,109,110,111

      /* 0x70 */
      ,  112,113,114,115,116,117,118,119

      /* 0x78 */
      ,  120,121,122,123,124,125,126,127

      /* 0x80 */
      ,  128,129,130,131,132,133,134,135

      /* 0x88 */
      ,  136,137,138,139,140,141,142,143

      /* 0x90 */
      ,  144,145,146,147,148,149,150,151

      /* 0x98 */
      ,  152,153,154,155,156,157,158,159

      /* 0xA0 */
      ,  160,161,162,163,164,165,166,167
  
      /* 0xA8 */
      ,  168,169,170,171,172,173,174,175

      /* 0xB0 */
      ,  176,177,178,179,180,181,182,183

      /* 0xB8 */
      ,  184,185,186,187,188,189,190,191

      /* 0xC0 */
      ,  ISOL1_AgraveU,  ISOL1_AacuteU,  ISOL1_AhatU,  ISOL1_AtildeU,
         ISOL1_AumlautU, ISOL1_AringU,   ISOL1_AE_U,   ISOL1_CedillaU

      /* 0xC8 */          
      ,  ISOL1_EgraveU,  ISOL1_EacuteU,  ISOL1_EhatU,  ISOL1_EumlautU,
         ISOL1_IgraveU,  ISOL1_IacuteU,  ISOL1_IhatU,  ISOL1_IumlautU
 
      /* 0xD0 */
      ,  ISOL1_EthU,     ISOL1_NtildeU,  ISOL1_OgraveU,  ISOL1_OacuteU,
         ISOL1_OhatU,    ISOL1_OtildeU,  ISOL1_OumlautU, 215

      /* 0xD8 */          
      ,  ISOL1_OobliqU,  ISOL1_UgraveU,  ISOL1_UacuteU,  ISOL1_UhatU,
         ISOL1_UumlautU, ISOL1_YacuteU,  ISOL1_ThornU,   ISOL1_Ssharp
          
      /* 0xE0 */
      ,  ISOL1_AgraveL,  ISOL1_AacuteL,  ISOL1_AhatL,  ISOL1_AtildeL,
         ISOL1_AumlautL, ISOL1_AringL,   ISOL1_AE_L,   ISOL1_CedillaL

      /* 0xE8 */
      ,  ISOL1_EgraveL,  ISOL1_EacuteL,  ISOL1_EhatL,  ISOL1_EumlautL,
         ISOL1_IgraveL,  ISOL1_IacuteL,  ISOL1_IhatL,  ISOL1_IumlautL

      /* 0xF0 */
      ,  ISOL1_EthL,     ISOL1_NtildeL,  ISOL1_OgraveL,  ISOL1_OacuteL,
         ISOL1_OhatL,    ISOL1_OtildeL,  ISOL1_OumlautL, 247

      /* 0xF8 */
      ,  ISOL1_OobliqL,  ISOL1_UgraveL,  ISOL1_UacuteL,  ISOL1_UhatL,
         ISOL1_UumlautL, ISOL1_YacuteL,  ISOL1_ThornL,   ISOL1_YumlautL

      };  /* end ISOL1_cstable */
#endif

/* 
 * Sort weight tables.  These tables are set up so that the sort 
 * weights are the same as the weights in V6.  This is so that keys 
 * will not have to be rebuilt (no index rebuild) when a V6 database 
 * is converted to V7.  This table has built into it what the 
 * "lcommon" patch table applied to the uptable in V6
 */

/* 
 * Note: Sort weight "0" is used (hard-coded) in dblang.c to indicate 
 * that the character gets special es-zet treatment: it worts as "SS"
 */
GLOBAL TEXT PROCONST("upctl_data_TEXT") ISOL1_cisWeights[] =
{
    /*   0       1       2       3       4       5       6       7      */
    /* 0x00    0x01    0x02    0x03    0x04    0x05    0x06    0x07     */

         0,      1,      2,      3,      4,      5,      6,      7,    


    /*   8       9      10      11      12      13      14      15      */
    /* 0x08    0x09    0x0A    0x0B    0x0C    0x0D    0x0E    0x0F     */

         8,      9,     10,     11,     12,     13,     14,     15,    


    /*  16      17      18      19      20      21      22      23      */
    /* 0x10    0x11    0x12    0x13    0x14    0x15    0x16    0x17     */

        16,     17,     18,     19,     20,     21,     22,     23,    


    /*  24      25      26      27      28      29      30      31      */
    /* 0x18    0x19    0x1A    0x1B    0x1C    0x1D    0x1E    0x1F     */

        24,     25,     26,     27,     28,     29,     30,     31,    


    /*  32      33      34      35      36      37      38      39      */
    /* 0x20    0x21    0x22    0x23    0x24    0x25    0x26    0x27     */

        32,     33,     34,     35,     36,     37,     38,     39,    


    /*  40      41      42      43      44      45      46      47      */
    /* 0x28    0x29    0x2A    0x2B    0x2C    0x2D    0x2E    0x2F     */

        40,     41,     42,     43,     44,     45,     46,     47,    


    /*  48      49      50      51      52      53      54      55      */
    /* 0x30    0x31    0x32    0x33    0x34    0x35    0x36    0x37     */

        48,     49,     50,     51,     52,     53,     54,     55,    


    /*  56      57      58      59      60      61      62      63      */
    /* 0x38    0x39    0x3A    0x3B    0x3C    0x3D    0x3E    0x3F     */

        56,     57,     58,     59,     60,     61,     62,     63,    


    /*  64      65      66      67      68      69      70      71      */
    /* 0x40    0x41    0x42    0x43    0x44    0x45    0x46    0x47     */

        64,     65,     66,     67,     68,     69,     70,     71,    


    /*  72      73      74      75      76      77      78      79      */
    /* 0x48    0x49    0x4A    0x4B    0x4C    0x4D    0x4E    0x4F     */

        72,     73,     74,     75,     76,     77,     78,     79,    


    /*  80      81      82      83      84      85      86      87      */
    /* 0x50    0x51    0x52    0x53    0x54    0x55    0x56    0x57     */

        80,     81,     82,     83,     84,     85,     86,     87,    


    /*  88      89      90      91      92      93      94      95      */
    /* 0x58    0x59    0x5A    0x5B    0x5C    0x5D    0x5E    0x5F     */

        88,     89,     90,     91,     92,     93,     94,     95,    


    /*  96      97      98      99     100     101     102     103      */
    /* 0x60    0x61    0x62    0x63    0x64    0x65    0x66    0x67     */

        96,     65,     66,     67,     68,     69,     70,     71,    


    /* 104     105     106     107     108     109     110     111      */
    /* 0x68    0x69    0x6A    0x6B    0x6C    0x6D    0x6E    0x6F     */

        72,     73,     74,     75,     76,     77,     78,     79,    


    /* 112     113     114     115     116     117     118     119      */
    /* 0x70    0x71    0x72    0x73    0x74    0x75    0x76    0x77     */

        80,     81,     82,     83,     84,     85,     86,     87,    


    /* 120     121     122     123     124     125     126     127      */
    /* 0x78    0x79    0x7A    0x7B    0x7C    0x7D    0x7E    0x7F     */

        88,     89,     90,    123,    124,    125,    126,    127,    


    /* 128     129     130     131     132     133     134     135      */
    /* 0x80    0x81    0x82    0x83    0x84    0x85    0x86    0x87     */

       176,    177,    178,    179,    180,    185,    186,    187,    


    /* 136     137     138     139     140     141     142     143      */
    /* 0x88    0x89    0x8A    0x8B    0x8C    0x8D    0x8E    0x8F     */

       188,    191,    192,    193,    194,    195,    196,    197,    


    /* 144     145     146     147     148     149     150     151      */
    /* 0x90    0x91    0x92    0x93    0x94    0x95    0x96    0x97     */

       200,    201,    202,    203,    204,    205,    206,    213,    


    /* 152     153     154     155     156     157     158     159      */
    /* 0x98    0x99    0x9A    0x9B    0x9C    0x9D    0x9E    0x9F     */

       217,    218,    219,    220,    223,    242,    0,      158,    


    /* 160     161     162     163     164     165     166     167      */
    /* 0xA0    0xA1    0xA2    0xA3    0xA4    0xA5    0xA6    0xA7     */

       255,    173,    189,    156,    207,    190,    221,    245,    


    /* 168     169     170     171     172     173     174     175      */
    /* 0xA8    0xA9    0xAA    0xAB    0xAC    0xAD    0xAE    0xAF     */

       249,    184,    229,    174,    199,    240,    199,    238,    


    /* 176     177     178     179     180     181     182     183      */
    /* 0xB0    0xB1    0xB2    0xB3    0xB4    0xB5    0xB6    0xB7     */

       248,    241,    253,    252,    239,    230,    244,    250,    


    /* 184     185     186     187     188     189     190     191      */
    /* 0xB8    0xB9    0xBA    0xBB    0xBC    0xBD    0xBE    0xBF     */

       247,    251,    229,    175,    172,    171,    243,    168,    


    /* 192     193     194     195     196     197     198     199      */
    /* 0xC0    0xC1    0xC2    0xC3    0xC4    0xC5    0xC6    0xC7     */

        65,     65,     65,     65,    142,    143,    146,     67,    


    /* 200     201     202     203     204     205     206     207      */
    /* 0xC8    0xC9    0xCA    0xCB    0xCC    0xCD    0xCE    0xCF     */

        69,     69,     69,     69,     73,     73,     73,     73,    


    /* 208     209     210     211     212     213     214     215      */
    /* 0xD0    0xD1    0xD2    0xD3    0xD4    0xD5    0xD6    0xD7     */

       209,     78,     79,     79,     79,    229,    153,    158,    


    /* 216     217     218     219     220     221     222     223      */
    /* 0xD8    0xD9    0xDA    0xDB    0xDC    0xDD    0xDE    0xDF     */

       157,     85,     85,     79,    154,     89,    232,    0,    


    /* 224     225     226     227     228     229     230     231      */
    /* 0xE0    0xE1    0xE2    0xE3    0xE4    0xE5    0xE6    0xE7     */

        65,     65,     65,     65,    142,    143,    146,     67,    


    /* 232     233     234     235     236     237     238     239      */
    /* 0xE8    0xE9    0xEA    0xEB    0xEC    0xED    0xEE    0xEF     */

        69,     69,     69,     69,     73,     73,     73,     73,    


    /* 240     241     242     243     244     245     246     247      */
    /* 0xF0    0xF1    0xF2    0xF3    0xF4    0xF5    0xF6    0xF7     */

       208,     78,     79,     79,     79,    228,    153,    246,    


    /* 248     249     250     251     252     253     254     255      */
    /* 0xF8    0xF9    0xFA    0xFB    0xFC    0xFD    0xFE    0xFF     */

       157,     85,     85,     85,    154,     89,    231,     89,    


};  /* end ISOL1_cisWeights */

GLOBAL TEXT PROCONST("upctl_data_TEXT") ISOL1_csWeights[] =
{
    /*   0       1       2       3       4       5       6       7      */
    /* 0x00    0x01    0x02    0x03    0x04    0x05    0x06    0x07     */

         0,      1,      2,      3,      4,      5,      6,      7,    


    /*   8       9      10      11      12      13      14      15      */
    /* 0x08    0x09    0x0A    0x0B    0x0C    0x0D    0x0E    0x0F     */

         8,      9,     10,     11,     12,     13,     14,     15,    


    /*  16      17      18      19      20      21      22      23      */
    /* 0x10    0x11    0x12    0x13    0x14    0x15    0x16    0x17     */

        16,     17,     18,     19,     20,     21,     22,     23,    


    /*  24      25      26      27      28      29      30      31      */
    /* 0x18    0x19    0x1A    0x1B    0x1C    0x1D    0x1E    0x1F     */

        24,     25,     26,     27,     28,     29,     30,     31,    


    /*  32      33      34      35      36      37      38      39      */
    /* 0x20    0x21    0x22    0x23    0x24    0x25    0x26    0x27     */

        32,     33,     34,     35,     36,     37,     38,     39,    


    /*  40      41      42      43      44      45      46      47      */
    /* 0x28    0x29    0x2A    0x2B    0x2C    0x2D    0x2E    0x2F     */

        40,     41,     42,     43,     44,     45,     46,     47,    


    /*  48      49      50      51      52      53      54      55      */
    /* 0x30    0x31    0x32    0x33    0x34    0x35    0x36    0x37     */

        48,     49,     50,     51,     52,     53,     54,     55,    


    /*  56      57      58      59      60      61      62      63      */
    /* 0x38    0x39    0x3A    0x3B    0x3C    0x3D    0x3E    0x3F     */

        56,     57,     58,     59,     60,     61,     62,     63,    


    /*  64      65      66      67      68      69      70      71      */
    /* 0x40    0x41    0x42    0x43    0x44    0x45    0x46    0x47     */

        64,     65,     66,     67,     68,     69,     70,     71,    


    /*  72      73      74      75      76      77      78      79      */
    /* 0x48    0x49    0x4A    0x4B    0x4C    0x4D    0x4E    0x4F     */

        72,     73,     74,     75,     76,     77,     78,     79,    


    /*  80      81      82      83      84      85      86      87      */
    /* 0x50    0x51    0x52    0x53    0x54    0x55    0x56    0x57     */

        80,     81,     82,     83,     84,     85,     86,     87,    


    /*  88      89      90      91      92      93      94      95      */
    /* 0x58    0x59    0x5A    0x5B    0x5C    0x5D    0x5E    0x5F     */

        88,     89,     90,     91,     92,     93,     94,     95,    


    /*  96      97      98      99     100     101     102     103      */
    /* 0x60    0x61    0x62    0x63    0x64    0x65    0x66    0x67     */

        96,     97,     98,     99,    100,    101,    102,    103,    


    /* 104     105     106     107     108     109     110     111      */
    /* 0x68    0x69    0x6A    0x6B    0x6C    0x6D    0x6E    0x6F     */

       104,    105,    106,    107,    108,    109,    110,    111,    


    /* 112     113     114     115     116     117     118     119      */
    /* 0x70    0x71    0x72    0x73    0x74    0x75    0x76    0x77     */

       112,    113,    114,    115,    116,    117,    118,    119,    


    /* 120     121     122     123     124     125     126     127      */
    /* 0x78    0x79    0x7A    0x7B    0x7C    0x7D    0x7E    0x7F     */

       120,    121,    122,    123,    124,    125,    126,    127,    


    /* 128     129     130     131     132     133     134     135      */
    /* 0x80    0x81    0x82    0x83    0x84    0x85    0x86    0x87     */

       176,    177,    178,    179,    180,    185,    186,    187,    


    /* 136     137     138     139     140     141     142     143      */
    /* 0x88    0x89    0x8A    0x8B    0x8C    0x8D    0x8E    0x8F     */

       188,    191,    192,    193,    194,    195,    196,    197,    


    /* 144     145     146     147     148     149     150     151      */
    /* 0x90    0x91    0x92    0x93    0x94    0x95    0x96    0x97     */

       200,    201,    202,    203,    204,    205,    206,    213,    


    /* 152     153     154     155     156     157     158     159      */
    /* 0x98    0x99    0x9A    0x9B    0x9C    0x9D    0x9E    0x9F     */

       217,    218,    219,    220,    223,    242,    0,      158,    


    /* 160     161     162     163     164     165     166     167      */
    /* 0xA0    0xA1    0xA2    0xA3    0xA4    0xA5    0xA6    0xA7     */

       255,    173,    189,    156,    207,    190,    221,    245,    


    /* 168     169     170     171     172     173     174     175      */
    /* 0xA8    0xA9    0xAA    0xAB    0xAC    0xAD    0xAE    0xAF     */

       249,    184,    229,    174,    199,    240,    198,    238,    


    /* 176     177     178     179     180     181     182     183      */
    /* 0xB0    0xB1    0xB2    0xB3    0xB4    0xB5    0xB6    0xB7     */

       248,    241,    253,    252,    239,    230,    244,    250,    


    /* 184     185     186     187     188     189     190     191      */
    /* 0xB8    0xB9    0xBA    0xBB    0xBC    0xBD    0xBE    0xBF     */

       247,    251,    229,    175,    172,    171,    243,    168,    


    /* 192     193     194     195     196     197     198     199      */
    /* 0xC0    0xC1    0xC2    0xC3    0xC4    0xC5    0xC6    0xC7     */

        65,     65,     65,     65,    142,    143,    146,     67,    


    /* 200     201     202     203     204     205     206     207      */
    /* 0xC8    0xC9    0xCA    0xCB    0xCC    0xCD    0xCE    0xCF     */

        69,     69,     69,     69,     73,     73,     73,     73,    


    /* 208     209     210     211     212     213     214     215      */
    /* 0xD0    0xD1    0xD2    0xD3    0xD4    0xD5    0xD6    0xD7     */

       209,     78,     79,     79,     79,    229,    153,    158,    


    /* 216     217     218     219     220     221     222     223      */
    /* 0xD8    0xD9    0xDA    0xDB    0xDC    0xDD    0xDE    0xDF     */

       157,     85,     85,     79,    154,     89,    232,    0,    


    /* 224     225     226     227     228     229     230     231      */
    /* 0xE0    0xE1    0xE2    0xE3    0xE4    0xE5    0xE6    0xE7     */

        97,     97,     97,     97,    132,    134,    145,     99,    


    /* 232     233     234     235     236     237     238     239      */
    /* 0xE8    0xE9    0xEA    0xEB    0xEC    0xED    0xEE    0xEF     */

       101,    101,    101,    101,    105,    105,    105,    105,    


    /* 240     241     242     243     244     245     246     247      */
    /* 0xF0    0xF1    0xF2    0xF3    0xF4    0xF5    0xF6    0xF7     */

       208,    110,    111,    111,    111,    228,    148,    246,    


    /* 248     249     250     251     252     253     254     255      */
    /* 0xF8    0xF9    0xFA    0xFB    0xFC    0xFD    0xFE    0xFF     */

       155,    117,    117,    117,    129,    121,    231,    121,    


};  /* end  ISOL1_csWeights */

/* Tables for a one-to-one mapping to/from Codepage 850/ISO-Latin-1  */
GLOBAL TEXT PROCONST("upctl_data_TEXT") CP850_to_ISOL1[] =
{
  /*   0 */   0,   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  /*  16 */   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  /*  32 */   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  /*  48 */   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
  /*  64 */   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  /*  80 */   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
  /*  96 */   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  /* 112 */  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  /* 128 */  ISOL1_CedillaU, ISOL1_UumlautL,  ISOL1_EacuteL,  ISOL1_AhatL,
  /* 132 */  ISOL1_AumlautL, ISOL1_AgraveL,   ISOL1_AringL,   ISOL1_CedillaL,
  /* 136 */  ISOL1_EhatL,    ISOL1_EumlautL,  ISOL1_EgraveL,  ISOL1_IumlautL,
  /* 140 */  ISOL1_IhatL,    ISOL1_IgraveL,   ISOL1_AumlautU, ISOL1_AringU,
  /* 144 */  ISOL1_EacuteU,  ISOL1_AE_L,      ISOL1_AE_U,     ISOL1_OhatL,
  /* 148 */  ISOL1_OumlautL, ISOL1_OgraveL,   ISOL1_UhatL,    ISOL1_UgraveL,
  /* 152 */  ISOL1_YumlautL, ISOL1_OumlautU,  ISOL1_UumlautU, ISOL1_OobliqL,
  /* 156 */  ISOL1_Pound,    ISOL1_OobliqU,   ISOL1_Mult,     ISOL1_Florin,
  /* 160 */  ISOL1_AacuteL,  ISOL1_IacuteL,   ISOL1_OacuteL,  ISOL1_UacuteL,
  /* 164 */  ISOL1_NtildeL,  ISOL1_NtildeU,   ISOL1_FemOrd,   ISOL1_MascOrd,
  /* 168 */  ISOL1_InvHook,  ISOL1_RegTM,     ISOL1_Not,      ISOL1_Half,
  /* 172 */  ISOL1_Quarter,  ISOL1_InvBang,   ISOL1_LDblAngle,ISOL1_RDblAngle,
  /* 176 */  ISOL1_Block1,   ISOL1_Block2,    ISOL1_Block3,   ISOL1_VertSgl,
  /* 180 */  ISOL1_VertSglL, ISOL1_AacuteU,   ISOL1_AhatU,    ISOL1_AgraveU,
  /* 184 */  ISOL1_Copyright,ISOL1_VertDblL,  ISOL1_VertDbl,  ISOL1_URCDbl,
  /* 188 */  ISOL1_LRCDbl,   ISOL1_Cent,      ISOL1_Yen,      ISOL1_URCSgl,
  /* 192 */  ISOL1_LLCSgl,   ISOL1_BTSgl,     ISOL1_UTSgl,    ISOL1_VertSglR,
  /* 196 */  ISOL1_HorizSgl, ISOL1_InterSgl,  ISOL1_AtildeL,  ISOL1_AtildeU,
  /* 200 */  ISOL1_LLCDbl,   ISOL1_ULCDbl,    ISOL1_BTDbl,    ISOL1_UTDbl,
  /* 204 */  ISOL1_VertDblR, ISOL1_HorizDbl,  ISOL1_InterDbl, ISOL1_Currency,
  /* 208 */  ISOL1_EthL,     ISOL1_EthU,      ISOL1_EhatU,    ISOL1_EumlautU,
  /* 212 */  ISOL1_EgraveU,  ISOL1_DotlessI,  ISOL1_IacuteU,  ISOL1_IhatU,
  /* 216 */  ISOL1_IumlautU, ISOL1_LRCSgl,    ISOL1_ULCSgl,   ISOL1_SolidBlk,
  /* 220 */  ISOL1_LowerBlk, ISOL1_BBar,      ISOL1_IgraveU,  ISOL1_UpperBlk,
  /* 224 */  ISOL1_OacuteU,  ISOL1_Ssharp,    ISOL1_OhatU,    ISOL1_OgraveU,
  /* 228 */  ISOL1_OtildeL,  ISOL1_OtildeU,   ISOL1_Mu,       ISOL1_ThornL,
  /* 232 */  ISOL1_ThornU,   ISOL1_UacuteU,   ISOL1_UhatU,    ISOL1_UgraveU,
  /* 236 */  ISOL1_YacuteL,  ISOL1_YacuteU,   ISOL1_Overline, ISOL1_Acute,
  /* 240 */  ISOL1_SylHyph,  ISOL1_PlusMinus, ISOL1_DblUnder, ISOL1_3Quarter,
  /* 244 */  ISOL1_Para,     ISOL1_Section,   ISOL1_Division, ISOL1_Cedilla,
  /* 248 */  ISOL1_Degree,   ISOL1_Umlaut,    ISOL1_MidDot,   ISOL1_Super1,
  /* 252 */  ISOL1_Super3,   ISOL1_Super2,    ISOL1_MidBox,   ISOL1_ReqSpace
} /* end CP850_to_ISOL1 */;

/* 
 * Note: the mapping of ISOL1 values 128-159 are 'control' codes.  
 * When mapping these to CP850, we map them to CP850 line-drawing 
 * characters in order to provide a one-to-one mapping between 
 * between CP850 and ISOL1.
 */
GLOBAL TEXT  PROCONST("upctl_data_TEXT") ISOL1_to_CP850[] =
{
  /*   0 */   0,   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
  /*  16 */   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  /*  32 */   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  /*  48 */   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
  /*  64 */   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  /*  80 */   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
  /*  96 */   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  /* 112 */  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  /* 128 */  CP850_Block1,   CP850_Block2,    CP850_Block3,   CP850_VertSgl,
  /* 132 */  CP850_VertSglL, CP850_VertDblL,  CP850_VertDbl,  CP850_URCDbl,
  /* 136 */  CP850_LRCDbl,   CP850_URCSgl,    CP850_LLCSgl,   CP850_BTSgl,
  /* 140 */  CP850_UTSgl,    CP850_VertSglR,  CP850_HorizSgl, CP850_InterSgl,
  /* 144 */  CP850_LLCDbl,   CP850_ULCDbl,    CP850_BTDbl,    CP850_UTDbl,
  /* 148 */  CP850_VertDblR, CP850_HorizDbl,  CP850_InterDbl, CP850_DotlessI,
  /* 152 */  CP850_LRCSgl,   CP850_ULCSgl,    CP850_SolidBlk, CP850_LowerBlk,
  /* 156 */  CP850_UpperBlk, CP850_DblUnder,  CP850_MidBox,   CP850_Florin,
  /* 160 */  CP850_ReqSpace, CP850_InvBang,   CP850_Cent,     CP850_Pound,
  /* 164 */  CP850_Currency, CP850_Yen,       CP850_BBar,     CP850_Section,
  /* 168 */  CP850_Umlaut,   CP850_Copyright, CP850_FemOrd,   CP850_LDblAngle,
  /* 172 */  CP850_Not,      CP850_SylHyph,   CP850_RegTM,    CP850_Overline,
  /* 176 */  CP850_Degree,   CP850_PlusMinus, CP850_Super2,   CP850_Super3,
  /* 180 */  CP850_Acute,    CP850_Mu,        CP850_Para,     CP850_MidDot,
  /* 184 */  CP850_Cedilla,  CP850_Super1,    CP850_MascOrd,  CP850_RDblAngle,
  /* 188 */  CP850_Quarter,  CP850_Half,      CP850_3Quarter, CP850_InvHook,
  /* 192 */  CP850_AgraveU,  CP850_AacuteU,   CP850_AhatU,    CP850_AtildeU,
  /* 196 */  CP850_AumlautU, CP850_AringU,    CP850_AE_U,     CP850_CedillaU,
  /* 200 */  CP850_EgraveU,  CP850_EacuteU,   CP850_EhatU,    CP850_EumlautU,
  /* 204 */  CP850_IgraveU,  CP850_IacuteU,   CP850_IhatU,    CP850_IumlautU,
  /* 208 */  CP850_EthU,     CP850_NtildeU,   CP850_OgraveU,  CP850_OacuteU,
  /* 212 */  CP850_OhatU,    CP850_OtildeU,   CP850_OumlautU, CP850_Mult,
  /* 216 */  CP850_OobliqU,  CP850_UgraveU,   CP850_UacuteU,  CP850_UhatU,
  /* 220 */  CP850_UumlautU, CP850_YacuteU,   CP850_ThornU,   CP850_Ssharp,
  /* 224 */  CP850_AgraveL,  CP850_AacuteL,   CP850_AhatL,    CP850_AtildeL,
  /* 228 */  CP850_AumlautL, CP850_AringL,    CP850_AE_L,     CP850_CedillaL,
  /* 232 */  CP850_EgraveL,  CP850_EacuteL,   CP850_EhatL,    CP850_EumlautL,
  /* 236 */  CP850_IgraveL,  CP850_IacuteL,   CP850_IhatL,    CP850_IumlautL,
  /* 240 */  CP850_EthL,     CP850_NtildeL,   CP850_OgraveL,  CP850_OacuteL,
  /* 244 */  CP850_OhatL,    CP850_OtildeL,   CP850_OumlautL, CP850_Division,
  /* 248 */  CP850_OobliqL,  CP850_UgraveL,   CP850_UacuteL,  CP850_UhatL,
  /* 252 */  CP850_UumlautL, CP850_YacuteL,   CP850_ThornL,   CP850_YumlautL
} /* end ISOL1_to_CP850 */;

/* These tables are used to hold "global" (non-db) versions of the
 * collation tables.
 */
GLOBAL TEXT *GlobalUptable = ISOL1_uptable;
GLOBAL TEXT *GlobalDntable = ISOL1_dntable;

/* 
 * Note that there are #defines below for the positions in this table
 * If this table changes, the #defines should change also. Note that 
 * there are some "aliases" where two names map to the same number.  
 * The "official" ones are listed first (they match the #defines 
 * below) which are found in the various search routines */

/* FOR Microsoft C compiler, we need to put the entry count in between [] */
GLOBAL struct TableNameMap XlateTableNames[7] =
{
    {(TEXT *) "Undefined",	XLATE_NONE},
    {(TEXT *) "ISO 8859-1",     XLATE_ISO_LATIN_1},
    {(TEXT *) "IBM850",         XLATE_CODEPAGE_850},
    {(TEXT *) "None",           XLATE_NONE},
    {(TEXT *) "ISO-Latin-1",	XLATE_ISO_LATIN_1},
    {(TEXT *) "Codepage 850",	XLATE_CODEPAGE_850},
    {0, 0}
};

/* This table contains a list of known collation table names  */
GLOBAL struct TableNameMap CollTableNames[5] =
{
    {(TEXT *) "Basic",	      COLL_BASIC},
    {(TEXT *) "Danish",       COLL_DANISH},
    {(TEXT *) "Swedish",      COLL_SWEDISH},
    {(TEXT *) "User Defined", COLL_USERCOLL},
    {0, 0}
};

#else
/* pointer to language table */
IMPORT struct language PROCONST("upctl_data_TEXT") *ISOL1_plang[LASTLANG];
IMPORT struct language PROCONST("upctl_data_TEXT") *ISOL1_pcslang[LASTLANG];
/*IMPORT struct language  ISOL1_lcommon[];  [bff] No longer used???*/
/*IMPORT struct language  ISOL1_cscommon[]; ditto                 */
/*IMPORT struct    delim  ISOL1_delimtbl[]; ditto 3               */
IMPORT            TEXT  ISOL1_uptable[256];
IMPORT		      TEXT  PROCONST("upctl_data_TEXT") ISOL1_uptablelcl[];
IMPORT            TEXT  ISOL1_dntable[256];
IMPORT            TEXT  ISOL1_cstable[256];
IMPORT            TEXT  PROCONST("upctl_data_TEXT") ISOL1_cisWeights[];
IMPORT            TEXT  PROCONST("upctl_data_TEXT") ISOL1_csWeights[];

IMPORT  struct  delim   PROCONST("upctl_data_TEXT") delimtbl[];

IMPORT TEXT PROCONST("upctl_data_TEXT") CP850_to_ISOL1[];
IMPORT TEXT PROCONST("upctl_data_TEXT") ISOL1_to_CP850[];

IMPORT struct TableNameMap XlateTableNames[7];
IMPORT struct TableNameMap CollTableNames[5];
IMPORT TEXT *GlobalUptable;
IMPORT TEXT *GlobalDntable;
#endif

/* positions in the XlateTableNames list  */
#define MAPTABLE_NONE		0
#define MAPTABLE_ISOL1		1
#define MAPTABLE_CP850		2

/* positions in the CollTableNames list */
#define MAPTABLE_BASIC          0
#define MAPTABLE_DANISH         1
#define MAPTABLE_SWEDISH        2
#define MAPTABLE_USERCOLL       3

#if OPSYS != VM
#define rupper(a) GlobalUptable[(a)]
#define rlower(a) GlobalDntable[(a)]
#else
#define rupper(a) toupper (a)
#define rlower(a) tolower (a)
#endif

/* proutil 2phase options */
#define UTSCAN     0x0001
#define UTCOMMIT   0x0002
#define UTRECOVER  0x0004
#define UTBEGIN    0x0008
#define UTMODIFY   0x0010
#define UTEND      0x0020
#define UTTPNEW    0x0040
#define UTTPCRD    0x0080

/* misc options */
#define UTOBI       0x0400
#define UTALL       0x0800
#define UTAREA      0x4000

/* binary load options      */
#define UT_BUILD    0x0001  /* "build" noise word  */
#define UT_INDEXES  0x0002  /* Build indexes while binary loading */

/* proutil convchar options */
#define UTANALYZE   0x0100
#define UTCONVERT   0x0200
#define UTISO88591  0x0400
#define UTIBM850    0x0800
#define UTUNDEFINED 0x1000
#define UTCHARSCAN  0x2100

#define UMAXLANG  29

#ifdef UTOPTNSGLOB
/* NOTE: make sure language codes are non zero */
/* TODO: each utility should get its own table */
GLOBAL struct keyword utoptns[UMAXLANG] =
{
{(TEXT *) "all",            0, 3,  UTALL},
{(TEXT *) "analyze",        0, 4,  UTANALYZE},
{(TEXT *) "areaa",          0, 4,  UTAREA},
{(TEXT *) "begin",          0, 4,  UTBEGIN},
{(TEXT *) "bi",             0, 2,  UTOBI},
{(TEXT *) "canada",         0, 4,  BASIC + 1},
{(TEXT *) "charscan",       0, 8,  UTCHARSCAN},
{(TEXT *) "commit",         0, 4,  UTCOMMIT},
{(TEXT *) "convert",        0, 4,  UTCONVERT},
{(TEXT *) "danish",         0, 4,  DANISH + 1},
{(TEXT *) "dutch",          0, 4,  BASIC + 1},
{(TEXT *) "end",            0, 3,  UTEND},
{(TEXT *) "english",        0, 4,  ENGLISH + 1},
{(TEXT *) "finnish",        0, 4,  FINNISH + 1},
{(TEXT *) "french",         0, 4,  BASIC + 1},
{(TEXT *) "german",         0, 4,  BASIC + 1},
{(TEXT *) "ibm850",         0, 6,  UTIBM850},
{(TEXT *) "iso8859-1",      0, 9,  UTISO88591},
{(TEXT *) "italian",        0, 4,  BASIC + 1},
{(TEXT *) "modify",         0, 4,  UTMODIFY},
{(TEXT *) "new",            0, 3,  UTTPNEW},
{(TEXT *) "norwegian",      0, 4,  NORWEGIAN + 1},
{(TEXT *) "recover",        0, 4,  UTRECOVER},
{(TEXT *) "scan",           0, 4,  UTSCAN},
{(TEXT *) "spanish",        0, 4,  BASIC + 1},
{(TEXT *) "swedish",        0, 4,  SWEDISH + 1},
{(TEXT *) "swiss",          0, 4,  BASIC + 1},
{(TEXT *) "undefined",      0, 9,  UTUNDEFINED},
{(TEXT *) "user-defined",   0, 4,  USERCOLL + 1}
};
#else
#if OPSYS == VMS || OPSYS == BTOS
#ifndef NUMKEY
#include "utkwmgr.h"
#endif
#endif
IMPORT struct keyword utoptns[UMAXLANG];
#endif

#endif /*UPCTL_H*/
