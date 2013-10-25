# Microsoft Developer Studio Project File - Name="gemini" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=gemini - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "gemini.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "gemini.mak" CFG="gemini - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "gemini - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "gemini - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "gemini - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GEMINI_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "incl" /I "inclprv" /I "msgs" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GEMINI_EXPORTS" /D WINNT=1 /D WIN95=1 /D WINNT_INTEL=1 /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386

!ELSEIF  "$(CFG)" == "gemini - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GEMINI_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "inclprv" /I "incl" /I "msgs" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GEMINI_EXPORTS" /D WINNT=1 /D WIN95=1 /D WINNT_INTEL=1 /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "gemini - Win32 Release"
# Name "gemini - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\db\bkcheck.c
# End Source File
# Begin Source File

SOURCE=.\db\bkcrashtest.c
# End Source File
# Begin Source File

SOURCE=.\db\bkio.c
# End Source File
# Begin Source File

SOURCE=.\db\bkiolk.c
# End Source File
# Begin Source File

SOURCE=.\db\bkiotop.c
# End Source File
# Begin Source File

SOURCE=.\db\bklog.c
# End Source File
# Begin Source File

SOURCE=.\db\bkopen.c
# End Source File
# Begin Source File

SOURCE=.\db\bkrec.c
# End Source File
# Begin Source File

SOURCE=.\db\bkrepl.c
# End Source File
# Begin Source File

SOURCE=.\db\bksample.c
# End Source File
# Begin Source File

SOURCE=.\db\bksubs.c
# End Source File
# Begin Source File

SOURCE=.\db\bmapw.c
# End Source File
# Begin Source File

SOURCE=.\db\bmbuf.c
# End Source File
# Begin Source File

SOURCE=.\db\cx.c
# End Source File
# Begin Source File

SOURCE=.\db\cxaux.c
# End Source File
# Begin Source File

SOURCE=.\db\cxblock.c
# End Source File
# Begin Source File

SOURCE=.\db\cxcompact.c
# End Source File
# Begin Source File

SOURCE=.\db\cxdo.c
# End Source File
# Begin Source File

SOURCE=.\db\cxfil.c
# End Source File
# Begin Source File

SOURCE=.\db\cxgen.c
# End Source File
# Begin Source File

SOURCE=.\db\cxkill.c
# End Source File
# Begin Source File

SOURCE=.\db\cxky.c
# End Source File
# Begin Source File

SOURCE=.\db\cxkyut.c
# End Source File
# Begin Source File

SOURCE=.\db\cxmove.c
# End Source File
# Begin Source File

SOURCE=.\db\cxnxt.c
# End Source File
# Begin Source File

SOURCE=.\db\cxtag.c
# End Source File
# Begin Source File

SOURCE=.\db\cxundo.c
# End Source File
# Begin Source File

SOURCE=.\vst\cxvsi.c
# End Source File
# Begin Source File

SOURCE=.\db\db.c
# End Source File
# Begin Source File

SOURCE=.\db\dbarea.c
# End Source File
# Begin Source File

SOURCE=.\db\dbblob.c
# End Source File
# Begin Source File

SOURCE=.\db\dbclose.c
# End Source File
# Begin Source File

SOURCE=.\db\dbindex.c
# End Source File
# Begin Source File

SOURCE=.\db\dblang.c
# End Source File
# Begin Source File

SOURCE=.\db\dblksch.c
# End Source File
# Begin Source File

SOURCE=.\db\dbobject.c
# End Source File
# Begin Source File

SOURCE=.\db\dbopen.c
# End Source File
# Begin Source File

SOURCE=.\db\dbquiet.c
# End Source File
# Begin Source File

SOURCE=.\db\dbrecord.c
# End Source File
# Begin Source File

SOURCE=.\db\dbtable.c
# End Source File
# Begin Source File

SOURCE=.\db\dbuser.c
# End Source File
# Begin Source File

SOURCE=.\db\dbwdog.c
# End Source File
# Begin Source File

SOURCE=.\db\dbxa.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmarea.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmblob.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmcon.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmcursr.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmindex.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmobj.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmrec.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmseq.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmshutdn.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmsrv.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmtable.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmtrans.c
# End Source File
# Begin Source File

SOURCE=.\api\dsmuser.c
# End Source File
# Begin Source File

SOURCE=.\db\geminifromkey.c
# End Source File
# Begin Source File

SOURCE=.\db\geminikey.c
# End Source File
# Begin Source File

SOURCE=.\db\keysvc.c
# End Source File
# Begin Source File

SOURCE=.\db\keyut.c
# End Source File
# Begin Source File

SOURCE=.\db\ky.c
# End Source File
# Begin Source File

SOURCE=.\db\kyut.c
# End Source File
# Begin Source File

SOURCE=.\db\latctl.c
# End Source File
# Begin Source File

SOURCE=.\db\lkmu.c
# End Source File
# Begin Source File

SOURCE=.\db\lkrecd.c
# End Source File
# Begin Source File

SOURCE=.\db\om.c
# End Source File
# Begin Source File

SOURCE=.\db\omundo.c
# End Source File
# Begin Source File

SOURCE=.\db\rec.c
# End Source File
# Begin Source File

SOURCE=.\db\recformt.c
# End Source File
# Begin Source File

SOURCE=.\db\recget.c
# End Source File
# Begin Source File

SOURCE=.\db\recput.c
# End Source File
# Begin Source File

SOURCE=.\db\rlai.c
# End Source File
# Begin Source File

SOURCE=.\db\rlaiovl.c
# End Source File
# Begin Source File

SOURCE=.\db\rlaoff.c
# End Source File
# Begin Source File

SOURCE=.\db\rlcl.c
# End Source File
# Begin Source File

SOURCE=.\db\rlmem.c
# End Source File
# Begin Source File

SOURCE=.\db\rlnote.c
# End Source File
# Begin Source File

SOURCE=.\db\rlrej.c
# End Source File
# Begin Source File

SOURCE=.\db\rlrw.c
# End Source File
# Begin Source File

SOURCE=.\db\rlset.c
# End Source File
# Begin Source File

SOURCE=.\db\rltl.c
# End Source File
# Begin Source File

SOURCE=.\db\rm.c
# End Source File
# Begin Source File

SOURCE=.\db\rmdoundo.c
# End Source File
# Begin Source File

SOURCE=.\vst\rmvst.c
# End Source File
# Begin Source File

SOURCE=.\db\semadm.c
# End Source File
# Begin Source File

SOURCE=.\db\semmgr.c
# End Source File
# Begin Source File

SOURCE=.\db\seqdundo.c
# End Source File
# Begin Source File

SOURCE=.\db\seqgen.c
# End Source File
# Begin Source File

SOURCE=.\db\shm.c
# End Source File
# Begin Source File

SOURCE=.\db\shmatt.c
# End Source File
# Begin Source File

SOURCE=.\db\shmcr.c
# End Source File
# Begin Source File

SOURCE=.\db\shmdel.c
# End Source File
# Begin Source File

SOURCE=.\db\shmpid.c
# End Source File
# Begin Source File

SOURCE=.\db\shmptoq.c
# End Source File
# Begin Source File

SOURCE=.\db\shmsiz.c
# End Source File
# Begin Source File

SOURCE=.\db\stm.c
# End Source File
# Begin Source File

SOURCE=.\db\tm.c
# End Source File
# Begin Source File

SOURCE=.\db\tmdoundo.c
# End Source File
# Begin Source File

SOURCE=.\db\tstset.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utbuff.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utchr.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utcore.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utcrc.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utctlc.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utdate.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utenv.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utf.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utfilesys.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utmem.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utmsg.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utosclose.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utosopen.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utosrw.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utstack.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utstackw.cpp
# End Source File
# Begin Source File

SOURCE=.\dbut\utstring.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utstub.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utsysdep.c
# End Source File
# Begin Source File

SOURCE=.\dbut\uttime.c
# End Source File
# Begin Source File

SOURCE=.\dbut\uttimer.c
# End Source File
# Begin Source File

SOURCE=.\dbut\utype.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
