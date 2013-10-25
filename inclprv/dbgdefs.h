
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

#ifndef DBGDEFS_H
#define DBGDEFS_H

#ifndef __PSC_TRACE_DEFINED
#define __PSC_TRACE_DEFINED 1

  /*------------------------------------------------------*/
  /* The following test is whether this facility is being */
  /* disabled from the compiler switches or not.  By	  */
  /* default this facility is being compiled in to the	  */
  /* code to which the macros have been added.  In order  */
  /* to turn all tracing software off in the product one  */
  /* would on a unix machine add "-DDBG_TRACING_REMOVE"	  */
  /* to the compile line of a source.			  */
  /*------------------------------------------------------*/

#ifndef DBG_TRACING_REMOVE


#include <string.h>

  /*--------------------------------------------------------*/
  /* By default this source is expecting to be part of a    */
  /* PSC image build.  As such there are certain type	    */
  /* definitions and #define directives that we need to	    */
  /* come from the PSC source (hence the include of	    */
  /* pscdtype.h).  In the case where this is some stand-    */
  /* alone program that this is being included in then the  */
  /* same pieces that we depend on PSC to provide are being */
  /* given definitions that are expected to be right in	    */
  /* most circumstances.  Note, however, that the testing   */
  /* of platform is nowhere near as exhaustive and	    */
  /* accurate as in the PSC compile, so be careful in that  */
  /* "offline" sort of environment.			    */
  /*--------------------------------------------------------*/

#ifndef DBG_TRACING_LOCAL_COMPILE

#include "pscdtype.h"

#else

  /*------------------------------------------------------------*/
  /* Non-PSC compile.  The directive DBG_TRACING_LOCAL_COMPILE	*/
  /* was defined somewhere.					*/
  /*------------------------------------------------------------*/

#define WIN32API 1
#define UNIXAPI  2


#ifdef WIN32
typedef __int64         psc_long64_t;
#define OPSYS WIN32API
#else
typedef long long       psc_long64_t;
#define OPSYS UNIXAPI
#endif

#define GLOBAL         /* used outside fns for defining use	    */
#define IMPORT extern  /* used where outside-defined vars are ref`d */
#define LOCAL  static  /* used outside functions		    */
#define DSMVOID   void

#endif


  /*--------------------------------------------------------*/
  /* These are the environment variables that we expect to  */
  /* examine for setup information.			    */
  /*--------------------------------------------------------*/

#define __DBG_LOG_FILE_VARIABLE		"PSC_TRACE_FILESPEC"
#define __DBG_TRACE_SETTING_VARIABLE	"PSC_TRACE_SETTING"
#define __DBG_TRACE_ALL_DETAIL_VARIABLE	"PSC_SET_ALL_TRACE_DETAIL"
#define __DBG_TRACE_T1_DETAIL_VARIABLE	"PSC_SET_T1_TRACE_DETAIL"
#define __DBG_TRACE_T2_DETAIL_VARIABLE	"PSC_SET_T2_TRACE_DETAIL"
#define __DBG_TRACE_T3_DETAIL_VARIABLE	"PSC_SET_T3_TRACE_DETAIL"
#define __DBG_TRACE_T4_DETAIL_VARIABLE	"PSC_SET_T4_TRACE_DETAIL"
#define __DBG_DEFAULT_LOG_FILE		"./psc_%v_%d_%t_%p.log"


  /*---------------------------------------------------------*/
  /* These are the constants for interpreting the environent */
  /* variable values.  Done this way in case we have to do   */
  /* I18N work on this some day.  These will be treated in   */
  /* a case insensitive fashion.			     */
  /*---------------------------------------------------------*/

#define __DBG_TRACE_SETTING_NONE		"none"
#define __DBG_TRACE_SETTING_TERM_ONLY		"term"
#define __DBG_TRACE_SETTING_TERM_AND_LOG	"termandlog"
#define __DBG_TRACE_SETTING_LOG_ONLY		"log"

#define __DBG_TRACE_LEVEL_NONE			"none"
#define __DBG_TRACE_LEVEL_MINIMUM		"minimum"
#define __DBG_TRACE_LEVEL_MEDIUM		"medium"
#define __DBG_TRACE_LEVEL_MAXIMUM		"maximum"



  /*------------------------------*/
  /* Some string length maximums. */
  /*------------------------------*/

	/*-----------------------------------*/
	/* Max length of the keywords above. */
	/*-----------------------------------*/

#define __DBG_MAX_TRACE_KEYWORD_LENGTH	30

	/*------------------------------------*/
	/* Max length of a log file filespec. */
	/*------------------------------------*/

#define __DBG_MAX_FILESPEC_LENGTH	1024

	/*------------------------------------------------*/
	/* Max length of an entry point or variable name. */
	/*------------------------------------------------*/

#define __DBG_MAX_ENTRY_LENGTH		64

	/*------------------------------------------------*/
	/* Max space to indent for routine nesting.  This */
	/* max is set so as to keep some printable part   */
        /* of the log on a page (in case the overflow	  */
	/* past column 128 does not wrap on either the	  */
	/* terminal window or a printer).		  */
	/*------------------------------------------------*/

#define __DBG_MAX_INDENT_LENGTH		116

	/*----------------------------------------------*/
	/* Used in breaking up very long string values. */
	/* When output of a string reaches this column	*/
	/* the output will "wrap" to the next log line.	*/
	/*----------------------------------------------*/

#define __DBG_MAX_LINE_LENGTH		256

	/*------------------------------------------------*/
	/* Max length printed from any string. This is    */
	/* set to keep the consumption of stack memory	  */
	/* from being too excessive (in anticipation of	  */
	/* limited stack space imposed by multi-threaded  */
	/* processes.  If there is need for more chars	  */
	/* than this it is suggested that a more liberal  */
	/* formatting entry point be created that does	  */
	/* a malloc of the space required.		  */
	/*------------------------------------------------*/

#define __DBG_MAX_STRING_LENGTH		2048


  /*-----------------------------------------------------------*/
  /* Since we are compiling with ANSI compilers it is expected */
  /* that the compiler supports function prototypes and the    */
  /* "#" stringize operator.				       */
  /*-----------------------------------------------------------*/

#define __DBG_HAS_STRINGIZE_OPERATOR  1



#ifdef __cplusplus

	/*--------------------------------------------------------------*/
	/* For C++ we are telling the compiler not to mangle the entry	*/
	/* names.  These are all C entry points.			*/
	/*--------------------------------------------------------------*/

    extern "C"
    {

#endif

	  /*-----------------------------------------*/
	  /* Function prototypes used by the macros. */
	  /*-----------------------------------------*/

	DSMVOID dbgTraceInit( char * );
	psc_long64_t dbgTimeAsInt();

	DSMVOID dbgEntry( char *, int );
	DSMVOID dbgExit( char *, psc_long64_t, int );
	DSMVOID dbgExitStatus( char *, long, psc_long64_t, int );
	DSMVOID dbgExitSFStatus(  );
	DSMVOID dbgStartSection( char *, int );
	DSMVOID dbgEndSection( char *, int );
	DSMVOID dbgMark( char *, int );
	DSMVOID dbgSkipLines( int, int );

	DSMVOID dbgValInt( char *, psc_long64_t, int );
	DSMVOID dbgValTrueFalse( char *, int, int );
	DSMVOID dbgValHex( char *, psc_long64_t, int );
	DSMVOID dbgValOct( char *, psc_long64_t, int );
	DSMVOID dbgValAll( char *, psc_long64_t, int );
	DSMVOID dbgValFloat( char *, double, int );
	DSMVOID dbgValDouble( char *, double, int );

	DSMVOID dbgValChar( char *, char *, int );
	DSMVOID dbgValCharN( char *, int, char *, int );
	DSMVOID dbgValCharNtrimmed( char *, int, char *, int );


	/*----------------------------------------------*/
	/* dbgTraceSetup inputs:			*/
	/*	-1 = logging off and close log file	*/
	/*	 0 = logging off, leave log file open	*/
	/*	 1 = logging on to display only		*/
	/*	 2 = logging on to display and log file	*/
	/*	 3 = logging on to log file only	*/
	/*----------------------------------------------*/


	DSMVOID dbgTraceSetup( int );
	DSMVOID dbgTraceStyle( int, int, int );


	/*-------------------------------------------------------------*/
	/* The following is the access to the trace control variables. */
	/*-------------------------------------------------------------*/

	IMPORT int dbgTraceLevel;
	IMPORT int dbgTraceLevelT1;
	IMPORT int dbgTraceLevelT2;
	IMPORT int dbgTraceLevelT3;
	IMPORT int dbgTraceLevelT4;

#ifdef __cplusplus

    }

#endif


      /*--------------------------------------------------------------------*/
      /* This macro needs to be executed once per image, preferably in the  */
      /* main() of the image and definitely before any tracing macros	    */
      /* other then the DBG_LOCAL_SETUP() is executed.			    */
      /*--------------------------------------------------------------------*/

#   define DBG_ESTABLISH_TRACE_SETUP( a ) dbgTraceInit( a )


      /*-----------------------------------------------------------------*/
      /* This macro needs to be executed once per routine after the last */
      /* local variable declaration and before the first tracing macro.	 */
      /* It sets up some local variables used internally by the tracing	 */
      /* to reduce the amount of coding required.			 */
      /*-----------------------------------------------------------------*/

#   define DBG_LOCAL_SETUP() \
	psc_long64_t __dbgEntryTime = 0; \
	int __dbgThreadIndex = 0; /* Thread index lookup will go here. */ \
	char __dbgLocalRoutineName [ __DBG_MAX_ENTRY_LENGTH ]; \
	__dbgLocalRoutineName[ 0 ] = EOS;


      /*----------------------------------------------------------------*/
      /* The following macros allow you to turn the C/C++ trace		*/
      /* logging on/off from your software.				*/
      /*----------------------------------------------------------------*/

#   define DBG_TURN_TRACE_OFF() (DSMVOID)dbgTraceSetup(-1)
#   define DBG_TURN_TRACE_ON_TO_TERM() (DSMVOID)dbgTraceSetup(1)
#   define DBG_TURN_TRACE_ON_TO_TERM_N_LOG( file ) (DSMVOID)dbgTraceSetup(2)
#   define DBG_TURN_TRACE_ON_TO_LOG( file ) (DSMVOID)dbgTraceSetup(3)


      /*----------------------------------------------------------------*/
      /* The following macros allow you to set the tracing level by	*/
      /* tier in the software.						*/
      /*----------------------------------------------------------------*/

#   define DBG_TRACE_LEVEL_NONE		0
#   define DBG_TRACE_LEVEL_BRIEF	1
#   define DBG_TRACE_LEVEL_MEDIUM	2
#   define DBG_TRACE_LEVEL_DETAILED	3

#   define DBG_SET_T1_TRACE_DETAIL_NONE()    dbgTraceLevelT1 = DBG_TRACE_LEVEL_NONE
#   define DBG_SET_T1_TRACE_DETAIL_MINIMUM() dbgTraceLevelT1 = DBG_TRACE_LEVEL_BRIEF
#   define DBG_SET_T1_TRACE_DETAIL_MEDIUM()  dbgTraceLevelT1 = DBG_TRACE_LEVEL_MEDIUM
#   define DBG_SET_T1_TRACE_DETAIL_MAXIMUM() dbgTraceLevelT1 = DBG_TRACE_LEVEL_DETAILED

#   define DBG_SET_T2_TRACE_DETAIL_NONE()    dbgTraceLevelT2 = DBG_TRACE_LEVEL_NONE
#   define DBG_SET_T2_TRACE_DETAIL_MINIMUM() dbgTraceLevelT2 = DBG_TRACE_LEVEL_BRIEF
#   define DBG_SET_T2_TRACE_DETAIL_MEDIUM()  dbgTraceLevelT2 = DBG_TRACE_LEVEL_MEDIUM
#   define DBG_SET_T2_TRACE_DETAIL_MAXIMUM() dbgTraceLevelT2 = DBG_TRACE_LEVEL_DETAILED

#   define DBG_SET_T3_TRACE_DETAIL_NONE()    dbgTraceLevelT3 = DBG_TRACE_LEVEL_NONE
#   define DBG_SET_T3_TRACE_DETAIL_MINIMUM() dbgTraceLevelT3 = DBG_TRACE_LEVEL_BRIEF
#   define DBG_SET_T3_TRACE_DETAIL_MEDIUM()  dbgTraceLevelT3 = DBG_TRACE_LEVEL_MEDIUM
#   define DBG_SET_T3_TRACE_DETAIL_MAXIMUM() dbgTraceLevelT3 = DBG_TRACE_LEVEL_DETAILED

#   define DBG_SET_T4_TRACE_DETAIL_NONE()    dbgTraceLevelT4 = DBG_TRACE_LEVEL_NONE
#   define DBG_SET_T4_TRACE_DETAIL_MINIMUM() dbgTraceLevelT4 = DBG_TRACE_LEVEL_BRIEF
#   define DBG_SET_T4_TRACE_DETAIL_MEDIUM()  dbgTraceLevelT4 = DBG_TRACE_LEVEL_MEDIUM
#   define DBG_SET_T4_TRACE_DETAIL_MAXIMUM() dbgTraceLevelT4 = DBG_TRACE_LEVEL_DETAILED


	/*--------------------------------------------------------------*/
	/* The following macros allow you to group a section of		*/
	/* tracing code so that it will conditionally execute at	*/
	/* run time.  This grouping allows for fast non-execution	*/
	/* of the grouped code when the tracing is turned off.		*/
	/*--------------------------------------------------------------*/

#   define DBG_IF_TRACE_IS_ON()  \
		if ( dbgTraceLevel > DBG_TRACE_LEVEL_NONE ) {

#   define DBG_IF_T1_TRACE_IS_ON()  \
		if ( dbgTraceLevelT1 > DBG_TRACE_LEVEL_NONE ) {

#   define DBG_IF_T1_TRACE_IS_MINIMUM()  \
		if ( dbgTraceLevelT1 == DBG_TRACE_LEVEL_BRIEF ) {

#   define DBG_IF_T1_TRACE_IS_MEDIUM()  \
		if ( dbgTraceLevelT1 == DBG_TRACE_LEVEL_MEDIUM ) {

#   define DBG_IF_T1_TRACE_IS_MIN_OR_MEDIUM()  \
		if ( (dbgTraceLevelT1 > DBG_TRACE_LEVEL_NONE) && \
		     (dbgTraceLevelT1 <= DBG_TRACE_LEVEL_MEDIUM) ) {

#   define DBG_IF_T1_TRACE_IS_MEDIUM_OR_MAX()  \
		if ( dbgTraceLevelT1 >= DBG_TRACE_LEVEL_MEDIUM ) {

#   define DBG_IF_T1_TRACE_IS_MAXIMUM()  \
		if ( dbgTraceLevelT1 == DBG_TRACE_LEVEL_DETAILED ) {


#   define DBG_IF_T2_TRACE_IS_ON()  \
		if ( dbgTraceLevelT2 > DBG_TRACE_LEVEL_NONE ) {

#   define DBG_IF_T2_TRACE_IS_MINIMUM()  \
		if ( dbgTraceLevelT2 == DBG_TRACE_LEVEL_BRIEF ) {

#   define DBG_IF_T2_TRACE_IS_MEDIUM()  \
		if ( dbgTraceLevelT2 == DBG_TRACE_LEVEL_MEDIUM ) {

#   define DBG_IF_T2_TRACE_IS_MIN_OR_MEDIUM()  \
		if ( (dbgTraceLevelT2 > DBG_TRACE_LEVEL_NONE) && \
		     (dbgTraceLevelT2 <= DBG_TRACE_LEVEL_MEDIUM) ) {

#   define DBG_IF_T2_TRACE_IS_MEDIUM_OR_MAX()  \
		if ( dbgTraceLevelT2 >= DBG_TRACE_LEVEL_MEDIUM ) {

#   define DBG_IF_T2_TRACE_IS_MAXIMUM()  \
		if ( dbgTraceLevelT2 == DBG_TRACE_LEVEL_DETAILED ) {


#   define DBG_IF_T3_TRACE_IS_ON()  \
		if ( dbgTraceLevelT3 > DBG_TRACE_LEVEL_NONE ) {

#   define DBG_IF_T3_TRACE_IS_MINIMUM()  \
		if ( dbgTraceLevelT3 == DBG_TRACE_LEVEL_BRIEF ) {

#   define DBG_IF_T3_TRACE_IS_MEDIUM()  \
		if ( dbgTraceLevelT3 == DBG_TRACE_LEVEL_MEDIUM ) {

#   define DBG_IF_T3_TRACE_IS_MIN_OR_MEDIUM()  \
		if ( (dbgTraceLevelT3 > DBG_TRACE_LEVEL_NONE) && \
		     (dbgTraceLevelT3 <= DBG_TRACE_LEVEL_MEDIUM) ) {

#   define DBG_IF_T3_TRACE_IS_MEDIUM_OR_MAX()  \
		if ( dbgTraceLevelT3 >= DBG_TRACE_LEVEL_MEDIUM ) {

#   define DBG_IF_T3_TRACE_IS_MAXIMUM()  \
		if ( dbgTraceLevelT3 == DBG_TRACE_LEVEL_DETAILED ) {


#   define DBG_IF_T4_TRACE_IS_ON()  \
		if ( dbgTraceLevelT4 > DBG_TRACE_LEVEL_NONE ) {

#   define DBG_IF_T4_TRACE_IS_MINIMUM()  \
		if ( dbgTraceLevelT4 == DBG_TRACE_LEVEL_BRIEF ) {

#   define DBG_IF_T4_TRACE_IS_MEDIUM()  \
		if ( dbgTraceLevelT4 == DBG_TRACE_LEVEL_MEDIUM ) {

#   define DBG_IF_T4_TRACE_IS_MIN_OR_MEDIUM()  \
		if ( (dbgTraceLevelT4 > DBG_TRACE_LEVEL_NONE) && \
		     (dbgTraceLevelT4 <= DBG_TRACE_LEVEL_MEDIUM) ) {

#   define DBG_IF_T4_TRACE_IS_MEDIUM_OR_MAX()  \
		if ( dbgTraceLevelT4 >= DBG_TRACE_LEVEL_MEDIUM ) {

#   define DBG_IF_T4_TRACE_IS_MAXIMUM()  \
		if ( dbgTraceLevelT4 == DBG_TRACE_LEVEL_DETAILED ) {


#   define DBG_ELSE() } else {
#   define DBG_END_IF() }

	/*--------------------------------------------------------------*/
	/* This macro is for encapsulating regular C code so that if	*/
	/* the trace code is compiled out the encapsulated code will	*/
	/* be compiled out with it.					*/
	/*--------------------------------------------------------------*/

#   define DBG_OPTIONAL_COMPILE( code )  { code }


	/*---------------------------------------------------------------*/
	/* The following macros allow you to do routine logging.  Note	 */
	/* that the local variables __dbgEntryTime, __dbgThreadIndex,	 */
	/* and __dbgLocalRoutineName are defined in DBG_LOCAL_SETUP().	 */
	/*---------------------------------------------------------------*/

#   define DBG_ENTRY( routine ) { dbgEntry( routine, __dbgThreadIndex ); \
	(DSMVOID)strncpy( __dbgLocalRoutineName, routine,\
				__DBG_MAX_ENTRY_LENGTH-1 ); \
	__dbgEntryTime = dbgTimeAsInt(); }

#   define DBG_EXIT() \
	{ dbgExit( __dbgLocalRoutineName, __dbgEntryTime, __dbgThreadIndex ); }

#   define DBG_EXIT_WITH_STATUS( status ) \
	{ dbgExitStatus( __dbgLocalRoutineName, status, __dbgEntryTime, \
				__dbgThreadIndex ); }

#   define DBG_START_SECTION( routine ) dbgStartSection( routine, \
				__dbgThreadIndex )

#   define DBG_END_SECTION( routine ) dbgEndSection( routine, __dbgThreadIndex )

#   define DBG_MARK( string ) dbgMark( string, __dbgThreadIndex )

#   define DBG_SKIP( var ) dbgSkipLines( var, __dbgThreadIndex )


#ifndef __cplusplus

	/*--------------------------------------------------------------*/
	/* The following macros allow you to log variable values.	*/
	/* These macros are set up for the convenience of only having	*/
	/* to type the variable name once.  The macro takes care of	*/
	/* forming the variable's name string.  This formation of the	*/
	/* name string does not work from the DEC C++ compiler so the	*/
	/* macros are unavailable.  The "dbgVal*" routine		*/
	/* should be called directly giving both inputs.		*/
	/*								*/
	/* Note that in the following macros the references to "var"	*/
	/* are spread over multiple lines.  This is to avoid extra	*/
	/* spacing that is generated by the DEC C compiler which	*/
	/* propogate into the strings being formed.			*/
	/*--------------------------------------------------------------*/

#ifndef __DBG_HAS_STRINGIZE_OPERATOR 

#   define DBG_VALUE_INT( var )		dbgValInt( "\
var \
", (psc_long64_t)(var), __dbgThreadIndex )

#   define DBG_VALUE_TRUE_FALSE( var )	dbgValTrueFalse( "\
var \
", (int)(var), __dbgThreadIndex )

#   define DBG_VALUE_HEX( var )		dbgValHex( "\
var \
", (psc_long64_t)(var), __dbgThreadIndex )

#   define DBG_VALUE_OCT( var )		dbgValOct( "\
var \
", (psc_long64_t)(var), __dbgThreadIndex )

#   define DBG_VALUE_ALL( var )		dbgValAll( "\
var \
", (psc_long64_t)(var), __dbgThreadIndex )

#   define DBG_VALUE_FLOAT( var )	dbgValFloat( "\
var \
", (double)(var), __dbgThreadIndex )

#   define DBG_VALUE_DOUBLE( var )	dbgValDouble( "\
var \
", (double)(var), __dbgThreadIndex )

#   define DBG_VALUE_CHAR( var )	dbgValChar( "\
var \
", (char *)(var), __dbgThreadIndex )

#   define DBG_VALUE_CHAR_N( var, len )	dbgValCharN( "\
var \
", len, (char *)(var), __dbgThreadIndex )

#   define DBG_VALUE_CHAR_N_TRIMMED( var, len ) dbgValCharNtrimmed( "\
var \
", len, (char *)(var), __dbgThreadIndex )


#else

#   define DBG_VALUE_INT( var ) \
			dbgValInt( #var, (psc_long64_t)(var), __dbgThreadIndex )

#   define DBG_VALUE_TRUE_FALSE( var ) \
			dbgValTrueFalse( #var, (int)(var), __dbgThreadIndex )

#   define DBG_VALUE_HEX( var ) \
			dbgValHex( #var, (psc_long64_t)(var), __dbgThreadIndex )

#   define DBG_VALUE_OCT( var ) \
			dbgValOct( #var, (psc_long64_t)(var), __dbgThreadIndex )

#   define DBG_VALUE_ALL( var ) \
			dbgValAll( #var, (psc_long64_t)(var), __dbgThreadIndex )

#   define DBG_VALUE_FLOAT( var ) \
			dbgValFloat( #var, (double)(var), __dbgThreadIndex )

#   define DBG_VALUE_DOUBLE( var ) \
			dbgValDouble( #var, (double)(var), __dbgThreadIndex )

#   define DBG_VALUE_CHAR( var ) \
			dbgValChar( #var, (char *)(var), __dbgThreadIndex )

#   define DBG_VALUE_CHAR_N( var, len ) \
			dbgValCharN( #var, len, (char *)(var), __dbgThreadIndex )

#   define DBG_VALUE_CHAR_N_TRIMMED( var, len ) \
			dbgValCharNtrimmed( #var, len, (char *)(var), __dbgThreadIndex )


#endif

  /* end for the not C++ case	*/

#endif


#else

      /*----------------------------------------------------------------*/
      /* The trace support is turned OFF.  Make sure all the macros	*/
      /* expand to no code, just blank space.				*/
      /*----------------------------------------------------------------*/

#   define DBG_ESTABLISH_TRACE_SETUP( a )
#   define DBG_LOCAL_SETUP()
#   define DBG_TURN_TRACE_OFF()
#   define DBG_TURN_TRACE_ON_TO_TERM()
#   define DBG_TURN_TRACE_ON_TO_TERM_N_LOG()
#   define DBG_TURN_TRACE_ON_TO_LOG( file )

#   define DBG_IF_TRACE_IS_ON()
#   define DBG_IF_T1_TRACE_IS_ON()
#   define DBG_IF_T1_TRACE_IS_MINIMUM()
#   define DBG_IF_T1_TRACE_IS_MEDIUM()
#   define DBG_IF_T1_TRACE_IS_MIN_OR_MEDIUM()
#   define DBG_IF_T1_TRACE_IS_MEDIUM_OR_MAX()
#   define DBG_IF_T1_TRACE_IS_MAXIMUM()
#   define DBG_IF_T2_TRACE_IS_ON()
#   define DBG_IF_T2_TRACE_IS_MINIMUM()
#   define DBG_IF_T2_TRACE_IS_MEDIUM()
#   define DBG_IF_T2_TRACE_IS_MIN_OR_MEDIUM()
#   define DBG_IF_T2_TRACE_IS_MEDIUM_OR_MAX()
#   define DBG_IF_T2_TRACE_IS_MAXIMUM()
#   define DBG_IF_T3_TRACE_IS_ON()
#   define DBG_IF_T3_TRACE_IS_MINIMUM()
#   define DBG_IF_T3_TRACE_IS_MEDIUM()
#   define DBG_IF_T3_TRACE_IS_MIN_OR_MEDIUM()
#   define DBG_IF_T3_TRACE_IS_MEDIUM_OR_MAX()
#   define DBG_IF_T3_TRACE_IS_MAXIMUM()
#   define DBG_IF_T4_TRACE_IS_ON()
#   define DBG_IF_T4_TRACE_IS_MINIMUM()
#   define DBG_IF_T4_TRACE_IS_MEDIUM()
#   define DBG_IF_T4_TRACE_IS_MIN_OR_MEDIUM()
#   define DBG_IF_T4_TRACE_IS_MEDIUM_OR_MAX()
#   define DBG_IF_T4_TRACE_IS_MAXIMUM()
#   define DBG_ELSE()
#   define DBG_END_IF()
#   define DBG_OPTIONAL_COMPILE( code )

#   define DBG_ENTRY( routine )
#   define DBG_EXIT()
#   define DBG_EXIT_WITH_STATUS( status ) 
#   define DBG_START_SECTION( routine )
#   define DBG_END_SECTION( routine )
#   define DBG_MARK( string )
#   define DBG_SKIP( cnt )

#   define DBG_VALUE_INT( var )
#   define DBG_VALUE_HEX( var )
#   define DBG_VALUE_OCT( var )
#   define DBG_VALUE_ALL( var )
#   define DBG_VALUE_FLOAT( var )
#   define DBG_VALUE_DOUBLE( var )
#   define DBG_VALUE_TRUE_FALSE( var )
#   define DBG_VALUE_CHAR( var )
#   define DBG_VALUE_CHAR_N( var, len )
#   define DBG_VALUE_CHAR_N_TRIMMED( var, len )


  /* end for #else for no trace case		*/
#endif

  /* end for overall include file processing	*/
#endif

#endif /* #ifndef DBGDEFS_H */
