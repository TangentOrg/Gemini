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

#if WINNT
/*  performs system dependent traceback for Windows NT
 *
 */
#include <time.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <tchar.h>
#include <imagehlp.h>
#include <string.h>
#include "dbconfig.h"
#include "utospub.h"

/*  This routine is called from utcore, and is necessary to force  */
/*  the inclusion of the exit handler object.  This file is  */
/*  part of ut.lib and the object is never reference within code. */
extern "C" { void uttraceback(void){} }


class dbut_PROExceptionHandler
{
	public:
      
      dbut_PROExceptionHandler( );
      ~dbut_PROExceptionHandler( );
     
	private:

      /* entry point where control comes on an unhandled exception */
      static LONG WINAPI PROUnhandledExceptionFilter(
                            PEXCEPTION_POINTERS pExceptionInfo );

      /* where report info is extracted and generated     */
      static void GenerateExceptionReport( PEXCEPTION_POINTERS pExceptionInfo );

      /* Helper functions */
      static LPTSTR GetExceptionString( DWORD dwCode );
      static GBOOL GetLogicalAddress(PVOID addr, PTSTR szModule, DWORD len,
                                    DWORD& section, DWORD& offset );
      static void IntelStackWalk( PCONTEXT pContext );
      #if 1
      static void ImagehlpStackWalk( PCONTEXT pContext );
      #endif
      static int __cdecl _tprintf(const TCHAR * format, ...);

      #if 1
      static GBOOL InitImagehlpFunctions( void );
      #endif
          
      /* Variables used by the class */
      static TCHAR m_szLogFileName[MAX_PATH];
      static LPTOP_LEVEL_EXCEPTION_FILTER m_previousFilter;
      static HANDLE m_hReportFile;

      #if 1
      /* Make typedefs for some IMAGEHLP.DLL functions so that we can use them */
      /* with GetProcAddress */
      typedef GBOOL (__stdcall * SYMINITIALIZEPROC)( HANDLE, LPSTR, GBOOL );
      typedef GBOOL (__stdcall *SYMCLEANUPPROC)( HANDLE );

      typedef GBOOL (__stdcall * STACKWALKPROC)
                   ( DWORD, HANDLE, HANDLE, LPSTACKFRAME, LPVOID,
                    PREAD_PROCESS_MEMORY_ROUTINE,PFUNCTION_TABLE_ACCESS_ROUTINE,
                    PGET_MODULE_BASE_ROUTINE, PTRANSLATE_ADDRESS_ROUTINE );

      typedef LPVOID (__stdcall *SYMFUNCTIONTABLEACCESSPROC)( HANDLE, DWORD );

      typedef DWORD (__stdcall *SYMGETMODULEBASEPROC)( HANDLE, DWORD );

      typedef GBOOL (__stdcall *SYMGETSYMFROMADDRPROC)
                                    ( HANDLE, DWORD, PDWORD, PIMAGEHLP_SYMBOL );

      static SYMINITIALIZEPROC _SymInitialize;
      static SYMCLEANUPPROC _SymCleanup;
      static STACKWALKPROC _StackWalk;
      static SYMFUNCTIONTABLEACCESSPROC _SymFunctionTableAccess;
      static SYMGETMODULEBASEPROC _SymGetModuleBase;
      static SYMGETSYMFROMADDRPROC _SymGetSymFromAddr;

      #endif
};

/* global instance of class */
extern dbut_PROExceptionHandler g_dbut_PROExceptionHandler;  


/*============================== Global Variables ============================= */
/* */
/* Declare the static variables of the dbut_PROExceptionHandler class */
/* */
TCHAR dbut_PROExceptionHandler::m_szLogFileName[MAX_PATH];
LPTOP_LEVEL_EXCEPTION_FILTER dbut_PROExceptionHandler::m_previousFilter;
HANDLE dbut_PROExceptionHandler::m_hReportFile;

dbut_PROExceptionHandler::SYMINITIALIZEPROC 
                       dbut_PROExceptionHandler::_SymInitialize = 0;

dbut_PROExceptionHandler::SYMCLEANUPPROC 
                       dbut_PROExceptionHandler::_SymCleanup = 0;

dbut_PROExceptionHandler::STACKWALKPROC 
                       dbut_PROExceptionHandler::_StackWalk = 0;

dbut_PROExceptionHandler::SYMFUNCTIONTABLEACCESSPROC
                       dbut_PROExceptionHandler::_SymFunctionTableAccess = 0;

dbut_PROExceptionHandler::SYMGETMODULEBASEPROC
                       dbut_PROExceptionHandler::_SymGetModuleBase = 0;

dbut_PROExceptionHandler::SYMGETSYMFROMADDRPROC
                       dbut_PROExceptionHandler::_SymGetSymFromAddr = 0;

/* Declare global instance of class */
dbut_PROExceptionHandler g_dbut_PROExceptionHandler;  


/*============================== Class Methods ============================= */

/*============= */
/* Constructor  */
/*============= */
dbut_PROExceptionHandler::dbut_PROExceptionHandler( )
{
    /* Install the unhandled exception filter function */
    m_previousFilter = SetUnhandledExceptionFilter(PROUnhandledExceptionFilter);

    /* Create the logfilename */
    _tcscpy(m_szLogFileName, _T("gemtrace.") );          
    int pid = _getpid();       /* get the pid */
    char spid[100];
    sprintf(spid, "%d", pid);  /* pids on NT are decimal not hex	 */
    _tcscat(m_szLogFileName, _T(spid) );
}

/*============ */
/* Destructor  */
/*============ */
dbut_PROExceptionHandler::~dbut_PROExceptionHandler( )
{
    SetUnhandledExceptionFilter( m_previousFilter );
}

/*=========================================================== */
/* Entry point where control comes on an unhandled exception  */
/*=========================================================== */
LONG WINAPI dbut_PROExceptionHandler::PROUnhandledExceptionFilter(
                             PEXCEPTION_POINTERS pExceptionInfo )
{
    m_hReportFile = CreateFile( m_szLogFileName,
                                GENERIC_WRITE,
                                0,
                                0,
                                OPEN_ALWAYS,
                                FILE_FLAG_WRITE_THROUGH,
                                0 );
    if ( m_hReportFile )
    {
        SetFilePointer( m_hReportFile, 0, 0, FILE_END );
        GenerateExceptionReport( pExceptionInfo );
        CloseHandle( m_hReportFile );
        m_hReportFile = 0;
    }

    if ( m_previousFilter )
        return m_previousFilter( pExceptionInfo );
    else
        return EXCEPTION_CONTINUE_SEARCH;
}

/*=========================================================================== */
/* Open the report file, and write the desired information to it.  Called by  */
/* PROUnhandledExceptionFilter                                                */
/*=========================================================================== */
void dbut_PROExceptionHandler::GenerateExceptionReport(
    PEXCEPTION_POINTERS pExceptionInfo )
{
    struct tm *newtime;
    time_t aclock;	
    time( &aclock );                 /* Get time in seconds */
    newtime = localtime( &aclock );  /* Convert time to struct */
                                    /* tm form */

    /* Set the environmental variable _NT_SYMBOL_PATH to  */
    /* locate symbol if not found locally.    */
    /* (Use the PATH as the definition.) */
    char *pathvar;
    pathvar = getenv( "PATH" );
    int pathlen = strlen(pathvar);
    char *symbolpath = new (char[pathlen+20]);
    if (symbolpath != NULL)
    {
	strcpy(symbolpath, "_NT_SYMBOL_PATH=");
	strcat(symbolpath, pathvar);
	putenv(symbolpath);
    }

    /* Start out with a banner */
    char tbuf[80];
    strcpy (tbuf, "Gemini stack trace as of ");
    strcat (tbuf,  asctime(newtime) );
    _tprintf( _T("/*=====================================================*/\n") );
    _tprintf( _T(tbuf));
    _tprintf( _T("/*=====================================================*/\n") );

    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;

    /* First print information about the type of fault */
    _tprintf(   _T("Exception code: %08X %s\n"),
                pExceptionRecord->ExceptionCode,
                GetExceptionString(pExceptionRecord->ExceptionCode) );

    /* Now print information about where the fault occured */
    TCHAR szFaultingModule[MAX_PATH];
    DWORD section, offset;
    GetLogicalAddress(  pExceptionRecord->ExceptionAddress,
                        szFaultingModule,
                        sizeof( szFaultingModule ),
                        section, offset );

    _tprintf( _T("Fault address:  %08X %02X:%08X %s\n"),
              pExceptionRecord->ExceptionAddress,
              section, offset, szFaultingModule );

    PCONTEXT pCtx = pExceptionInfo->ContextRecord;

    /* Show the registers */
    #ifdef _M_IX86  /* Intel Only! */
    _tprintf( _T("\nRegisters:\n") );

    _tprintf(_T("EAX:%08X\nEBX:%08X\nECX:%08X\nEDX:%08X\nESI:%08X\nEDI:%08X\n"),
             pCtx->Eax, pCtx->Ebx, pCtx->Ecx, pCtx->Edx, pCtx->Esi, pCtx->Edi );

    _tprintf( _T("CS:EIP:%04X:%08X\n"), pCtx->SegCs, pCtx->Eip );
    _tprintf( _T("SS:ESP:%04X:%08X  EBP:%08X\n"),
              pCtx->SegSs, pCtx->Esp, pCtx->Ebp );
    _tprintf( _T("DS:%04X  ES:%04X  FS:%04X  GS:%04X\n"),
              pCtx->SegDs, pCtx->SegEs, pCtx->SegFs, pCtx->SegGs );
    _tprintf( _T("Flags:%08X\n"), pCtx->EFlags );

    #endif

    if ( !InitImagehlpFunctions() )
    {
        OutputDebugString(_T("IMAGEHLP.DLL or its exported procs not found"));
        
        #ifdef _M_IX86  /* Intel Only! */
        /* Walk the stack using x86 specific code */
        IntelStackWalk( pCtx );
        #endif

        return;
    }

    ImagehlpStackWalk( pCtx );
    _SymCleanup( GetCurrentProcess() );
    _tprintf( _T("\n") );
}

/*====================================================================== */
/* Given an exception code, returns a pointer to a static string with a  */
/* description of the exception                                          */
/*====================================================================== */
LPTSTR dbut_PROExceptionHandler::GetExceptionString( DWORD dwCode )
{
    #define EXCEPTION( x ) case EXCEPTION_##x: return _T(#x);

    switch ( dwCode )
    {
        EXCEPTION( ACCESS_VIOLATION )
        EXCEPTION( DATATYPE_MISALIGNMENT )
        EXCEPTION( BREAKPOINT )
        EXCEPTION( SINGLE_STEP )
        EXCEPTION( ARRAY_BOUNDS_EXCEEDED )
        EXCEPTION( FLT_DENORMAL_OPERAND )
        EXCEPTION( FLT_DIVIDE_BY_ZERO )
        EXCEPTION( FLT_INEXACT_RESULT )
        EXCEPTION( FLT_INVALID_OPERATION )
        EXCEPTION( FLT_OVERFLOW )
        EXCEPTION( FLT_STACK_CHECK )
        EXCEPTION( FLT_UNDERFLOW )
        EXCEPTION( INT_DIVIDE_BY_ZERO )
        EXCEPTION( INT_OVERFLOW )
        EXCEPTION( PRIV_INSTRUCTION )
        EXCEPTION( IN_PAGE_ERROR )
        EXCEPTION( ILLEGAL_INSTRUCTION )
        EXCEPTION( NONCONTINUABLE_EXCEPTION )
        EXCEPTION( STACK_OVERFLOW )
        EXCEPTION( INVALID_DISPOSITION )
        EXCEPTION( GUARD_PAGE )
        EXCEPTION( INVALID_HANDLE )
    }

    /* If not one of the "known" exceptions, try to get the string */
    /* from NTDLL.DLL's message table. */

    static TCHAR szBuffer[512] = { 0 };

    FormatMessage(  FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
                    GetModuleHandle( _T("NTDLL.DLL") ),
                    dwCode, 0, szBuffer, sizeof( szBuffer ), 0 );
    return szBuffer;
}

/*============================================================================== */
/* Given a linear address, locates the module, section, and offset containing   */
/* that address.                                                                */
/*                                                                              */
/* Note: the szModule paramater buffer is an output buffer of length specified  */
/* by the len parameter (in characters!)                                        */
/*============================================================================== */
GBOOL dbut_PROExceptionHandler::GetLogicalAddress(
        PVOID addr, PTSTR szModule, DWORD len, DWORD& section, DWORD& offset )
{
    MEMORY_BASIC_INFORMATION mbi;

    if ( !VirtualQuery( addr, &mbi, sizeof(mbi) ) )
        return FALSE;

    DWORD hMod = (DWORD)mbi.AllocationBase;

    if ( !GetModuleFileName( (HMODULE)hMod, szModule, len ) )
        return FALSE;

    /* Point to the DOS header in memory */
    PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)hMod;

    /* From the DOS header, find the NT (PE) header */
    PIMAGE_NT_HEADERS pNtHdr = (PIMAGE_NT_HEADERS)(hMod + pDosHdr->e_lfanew);

    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION( pNtHdr );

	/* RVA is offset from module load address */
    DWORD rva = (DWORD)addr - hMod; 

    /* Iterate through the section table, looking for the one that encompasses */
    /* the linear address. */
    for (   unsigned i = 0;
            i < pNtHdr->FileHeader.NumberOfSections;
            i++, pSection++ )
    {
        DWORD sectionStart = pSection->VirtualAddress;
        DWORD sectionEnd = sectionStart
                    + max(pSection->SizeOfRawData, pSection->Misc.VirtualSize);

        /* Is the address in this section??? */
        if ( (rva >= sectionStart) && (rva <= sectionEnd) )
        {
            /* Yes, address is in the section.  Calculate section and offset, */
            /* and store in the "section" & "offset" params, which were */
            /* passed by reference. */
            section = i+1;
            offset = rva - sectionStart;
            return TRUE;
        }
    }
    return FALSE;   /* Should never get here! */
}

/*============================================================ */
/* Walks the stack, and writes the results to the report file  */
/*============================================================ */
void dbut_PROExceptionHandler::IntelStackWalk( PCONTEXT pContext )
{
    _tprintf( _T("\nCall Stack:\n") );
    _tprintf( _T("Address   Frame     Logical addr  Module\n") );

    DWORD pc = pContext->Eip;
    PDWORD pFrame, pPrevFrame;
    
    pFrame = (PDWORD)pContext->Ebp;

    do
    {
        TCHAR szModule[MAX_PATH] = _T("");
        DWORD section = 0, offset = 0;

        GetLogicalAddress((PVOID)pc, szModule,sizeof(szModule),section,offset );

        _tprintf( _T("%08X  %08X  %04X:%08X %s\n"),
                  pc, pFrame, section, offset, szModule );

        pc = pFrame[1];
        pPrevFrame = pFrame;

        pFrame = (PDWORD)pFrame[0]; /* proceed to next higher frame on stack */

        if ( (DWORD)pFrame & 3 )    /* Frame pointer must be aligned on a */
            break;                  /* DWORD boundary.  Bail if not so. */

        if ( pFrame <= pPrevFrame )
            break;

        /* Can two DWORDs be read from the supposed frame address?           */
        if ( IsBadWritePtr(pFrame, sizeof(PVOID)*2) )
            break;

    } while ( 1 );
}

/*============================================================ */
/* Walks the stack, and writes the results to the report file  */
/*============================================================ */
void dbut_PROExceptionHandler::ImagehlpStackWalk( PCONTEXT pContext )
{
    _tprintf( _T("\nCall Stack:\n") );
    _tprintf( _T("Address   Frame\n") );

    /* Could use SymSetOptions here to add the SYMOPT_DEFERRED_LOADS flag */

    STACKFRAME sf;
    memset( &sf, 0, sizeof(sf) );

    /* Initialize the STACKFRAME structure for the first call.  This is only */
    /* necessary for Intel CPUs, and isn't mentioned in the documentation. */
    sf.AddrPC.Offset       = pContext->Eip;
    sf.AddrPC.Mode         = AddrModeFlat;
    sf.AddrStack.Offset    = pContext->Esp;
    sf.AddrStack.Mode      = AddrModeFlat;
    sf.AddrFrame.Offset    = pContext->Ebp;
    sf.AddrFrame.Mode      = AddrModeFlat;

    while ( 1 )
    {
        if ( ! _StackWalk(  IMAGE_FILE_MACHINE_I386,
                            GetCurrentProcess(),
                            GetCurrentThread(),
                            &sf,
                            pContext,
                            0,
                            _SymFunctionTableAccess,
                            _SymGetModuleBase,
                            0 ) )
            break;

        if ( 0 == sf.AddrFrame.Offset ) /* Basic sanity check to make sure */
            break;                      /* the frame is OK.  Bail if not. */

        _tprintf( _T("%08X  %08X  "), sf.AddrPC.Offset, sf.AddrFrame.Offset );

        /* IMAGEHLP is wacky, and requires you to pass in a pointer to an */
        /* IMAGEHLP_SYMBOL structure.  The problem is that this structure is */
        /* variable length.  That is, you determine how big the structure is */
        /* at runtime.  This means that you can't use sizeof(struct). */
        /* So...make a buffer that's big enough, and make a pointer */
        /* to the buffer.  We also need to initialize not one, but TWO */
        /* members of the structure before it can be used. */

        BYTE symbolBuffer[ sizeof(IMAGEHLP_SYMBOL) + 512 ];
        PIMAGEHLP_SYMBOL pSymbol = (PIMAGEHLP_SYMBOL)symbolBuffer;
        pSymbol->SizeOfStruct = sizeof(symbolBuffer);
        pSymbol->MaxNameLength = 512;
                        
        DWORD symDisplacement = 0;  /* Displacement of the input address, */
                                    /* relative to the start of the symbol */

        if ( _SymGetSymFromAddr(GetCurrentProcess(), sf.AddrPC.Offset,
                                &symDisplacement, pSymbol) )
        {
            _tprintf( _T("%hs+%X\n"), pSymbol->Name, symDisplacement );
        }
        else    /* No symbol found.  Print out the logical address instead. */
        {
            TCHAR szModule[MAX_PATH] = _T("");
            DWORD section = 0, offset = 0;

            GetLogicalAddress(  (PVOID)sf.AddrPC.Offset,
                                szModule, sizeof(szModule), section, offset );

            _tprintf( _T("%04X:%08X %s\n"),
                      section, offset, szModule );
        }
    }
}

/*============================================================================ */
/* Helper function that writes to the report file, and allows the user to use  */
/* printf style formating                                                      */
/*============================================================================ */
int __cdecl dbut_PROExceptionHandler::_tprintf(const TCHAR * format, ...)
{
    TCHAR szBuff[1024];
    int retValue;
    DWORD cbWritten;
    va_list argptr;
    va_start( argptr, format );
    retValue = wvsprintf( szBuff, format, argptr );
    va_end( argptr );
    WriteFile( m_hReportFile, szBuff, retValue * sizeof(TCHAR), &cbWritten, 0 );
    return retValue;
}

/*========================================================================= */
/* Load IMAGEHLP.DLL and get the address of functions in it that we'll use  */
/*========================================================================= */
GBOOL dbut_PROExceptionHandler::InitImagehlpFunctions( void )
{
    HMODULE hModImagehlp = LoadLibrary( _T("IMAGEHLP.DLL") );
    if ( !hModImagehlp )
        return FALSE;

    _SymInitialize = (SYMINITIALIZEPROC)GetProcAddress( hModImagehlp,
                                                        "SymInitialize" );
    if ( !_SymInitialize )
        return FALSE;

    _SymCleanup = (SYMCLEANUPPROC)GetProcAddress( hModImagehlp, "SymCleanup" );
    if ( !_SymCleanup )
        return FALSE;

    _StackWalk = (STACKWALKPROC)GetProcAddress( hModImagehlp, "StackWalk" );
    if ( !_StackWalk )
        return FALSE;

    _SymFunctionTableAccess = (SYMFUNCTIONTABLEACCESSPROC)
                        GetProcAddress( hModImagehlp, "SymFunctionTableAccess" );

    if ( !_SymFunctionTableAccess )
        return FALSE;

    _SymGetModuleBase=(SYMGETMODULEBASEPROC)GetProcAddress( hModImagehlp,
                                                            "SymGetModuleBase");
    if ( !_SymGetModuleBase )
        return FALSE;

    _SymGetSymFromAddr=(SYMGETSYMFROMADDRPROC)GetProcAddress( hModImagehlp,
                                                              "SymGetSymFromAddr" );
    if ( !_SymGetSymFromAddr )
        return FALSE;

    if ( !_SymInitialize( GetCurrentProcess(), 0, TRUE ) )
        return FALSE;

    return TRUE;        
}

#endif /* WINNT  */

