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

#ifndef PSCSYS_H
#define PSCSYS_H 1

#include "dstdstr.h"
/*
* Definitions, sizes, lengths, etc. that differ by operating system.
*/
#define RUNTIME_BLKSIZE 1
#include "pscclang.h"
#include "pscdtype.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAXKCOMP 16     /* max components per key */
#define MAXNAM   32     /*max len of file name, fld name or kywd*/
#define MAXMSG   400     /* formerly 80, max len of a screen msg  */
#define MAXSNM	 12	/* max stream name*/
#define MSGBUFSZ MAXMSG + 112 /* msg buffer should have safety room */
#define MAXDBS   240    /*max databases a client can connect to -h*/
#define DFTDBS   5      /*default databases a client can connect to*/

#define MAXJOINTHRU 10   /* Max tables to join directly by a gateway server */

/* Max length of a database nickname. NOTE: mb_nickname in the master block */
/* must be > then MAXNICKNM.					            */
#define MAXNICKNM 8

/* when getting an ICB, if the largest available ICB    */
/* is smaller than stdbufsz, then do garbage collection     */
#define STDBUFSZ 2048

#define MINMSGSZ 350    /* minimum size of ipc message fragmt must
			allow for struct dbkey + 2 keys*/

#define CURSID  short
#define NULLCURSID (-1)
#define QUERYID  short
#define NULLQUERYID (-1)
#define DSHID	short
#define NOFILES  -200

#define MAXPOS   0x7fffffffL    /* maximum positive number */
#ifndef ABS
#define ABS(x)   ((x)<0?-(x):(x))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<=(b)?(a):(b))
#endif


/* if the port supports remote-AS/400 gateway, turn AS4GTWY on */
#if IBMWIN || OS2_INTEL || IBMRIOS || SUN4 || ALPHAOSF
#undef AS4GTWY
#define AS4GTWY 1
#else
#undef AS4GTWY
#define AS4GTWY 0
#endif

#ifndef DEMO
#define DEMO 0
#endif

#ifndef DEBUG
#define DEBUG 0
#else
#undef DEBUG
#define DEBUG 1
#endif

#ifndef APPLSV
#define APPLSV	0
#endif

/* KSU/TEX 1/19/94 ASIAN FLAG SETS BUILD FOR DOUBLE BYTE SUPPORT */
/* To build the ASIAN version V7 compile with /DASIAN compiler flag.
	ASIAN defines flag DBCS which is referenced everywhere with
	#if DBCS.
	DBCS_WIN3, DBCS_OTHER are also used for specific ASIAN operating
	systems */

#if ASIAN
#undef DBCS
#define DBCS 1
#endif

#define MAXVARSZ 32000
#define MINRECSZ  1024   /* minimum size of argmaxrc		 */
#if OPSYS==BTOS
#define DEFRECSZ   2048  /* Default maximum record size		 */
#define MAXRECSZ  32000  /* maximum record size			 */
#define MAXMSGSZ   1024  /* Default maximum size of ipc msg frag.*/
#define MAXTOTMSG 32600  /* maximum size of ipc complete message */
#else
#define MAXRECSZ 32000
#define MAXMSGSZ 1024   /* maximum size of ipc message fragment */
#define MAXTOTMSG 32600 /* maximum size of ipc complete message */
#endif

#if (OPSYS != BTOS && OPSYS != VM)
#ifdef KEYCAPTURE
#undef KEYCAPTURE
#endif
#define KEYCAPTURE
#endif

#if OPSYS==MPEIX
#undef KEYCAPTURE 
#endif 
 
#if OPSYS == VM
#undef KEYCAPTURE
#endif
#if OPSYS == MS_DOS || OPSYS==WIN32API || OPSYS==OS2
#undef KEYCAPTURE
#endif
/* key capture has been taken out of all operating systems but we might want
   to put it back in, so I left the logic above in. - CAP 7-17-92 */
#undef KEYCAPTURE

/**/
/* Define optional system specific switches, as follow:

   PGRPCTL      on if this system supports the "setpgrp" call.

   DUP2         on if this system supports the "dup2" subroutine
                call; Otherwise the F_DUPFD function of the subroutine
                "fcntl" is used.

   FCNTL        on if this system has the fcntl subroutine

   NOPUTENV     on for systems that can't utilize putenv().

   ULIMIT       on if the operating system imposes file size and
                process size limits which can be changed with the
                "ulimit" subroutine.

   UTCLEXI      on if the FIOCLEX function of the "ioctl" subroutine
                is supported; Otherwise the F_SETFD function of
                the "fcntl" subroutine is used.

   STERMIO      on if the system has a /usr/include/sys/termio.h
                include file which contains a description of the
                termio structure used in "ioctl" and "waitcs"
                subroutine calls.

   TERMIO       on if the system has a /usr/include/termio.h
                include file which contains a description of the
                termio structure used in "ioctl" and "waitcs"
                subroutine calls. If neither STERMIO or TERMIO
                is set, then a sgtty structure from
                /usr/include/sgtty.h is used.

   HPBAUDS	on if the machine supports a baud rate selection mask
		in the termio structure similar to that supported
		in Hewlett-Packard's HP/UX, ie 3600 & 7200 baud support

   LDISC        on if the system supports the TIOCSETD and TIOCGETD
                ioctl calls to set and obtain the terminal line
                discipline mode.

   JOBSTP       on if the system supports the SIGTTOU and SIGTSTP
                signals used to stop jobs.

   KERNEL       sometimes required to tell system include files that
                they are being included outside of kernel context.

   DIRBSD42     on if the system supports the directory(3) set of calls,
                specifically opendir, closedir, and readdir.

   SIGBSD42     on if the system supports the BSD 4.2 version of signal
                handling, i.e. the sigvec, sigpause, and sigblock system
                subroutine calls.

   AIOBSD42     on if the system supports the BSD 4.2 version of non-blocking
                i/o hadling. This includes the FNDELAY flag set by a
                fcntl system call specifying F_SETFL, the EWOULDBLOCK
                return from the read system routine, and the select
                system routine. This implies that a read will not be
                interrupted by a caught signal.

   OPRBSD42     on if only members of a terminal's process group may read
                from the terminal. This requires the TIOCGPGRP and
                TIOCSPGRP functions of the ioctl system subroutine.

   FSBSD42      on if the system uses the BSD 4.2 file system format. The
                chkfs utility can be used to determine this.

   NFSBSD42     on if the system supports the BSD 4.2 networking file
                system. This is primarily used to get the fs.h and inode.h
                files from /sys/ufs instead of <sys/>.

   ALVARIES     on if the compiler in use varies its structure alignment
                to match the structure, ie aligns to the largest individual
                element within the structure.

   PIPES        on if pipes are used to communicate between the server
                and its children.

   MSGQUEUES    on if System V message queues and semaphores are used
                to communicate between the server and its clients.

   BSDSOCKETS	on if BSD 4.2 SOCKETS facilities used betwn server/client
   NETBSESS	on if NETBIOS SESSION facilities used betwn server/client
   NETBDG	on if NETBIOS DATAGRM facilities used betwn server/client
   BTOSEXCH	on if BTOS EXCHANGES facilities used betwn server/client

   OPENET       on if INTEL OPEN NET  facilities used betwn server/client

   SWRTAVAIL    on if the synchronous write (swrite) call is supported.
   RAWAVAIL	on if UNIX style raw i/o is available

   SYS5REL2     on if the machine has ATT System 5 Release 2 operating
                system or derivative

   REGWINDOW	on if the machine uses register windows to pass arguments


   DOSSCRN      on if the screen is memory mapped instead of RS232

   UPFILENAM	TRUE if filenames are uppercase, case insensitive.

   BUFSCRN      TRUE if buffering writes to screen

   CLIB_VARARGS on if the varargs(3) mechanism is used in drmsg and utfprtf.
                This should eventually be on for all machines but the
                only machines that require it are RISC. New ports whether
                or not they are RISC based should use this.

   NOTSTSET     TRUE if machine does not have a tstset (tas) instruction.
                This uses extra system semaphores.

   MIPSTAS      TRUE if we are using the test & set algorithm proposed by mips.

   DECTAS	True is using DEC/Ultrix's atomic_op call

   HEAPSIZ      Minimum amount of heap (in kilobytes)needed - used to
                determine the location of shared memory - set to NULL if
		not applicable

   USE_SIGACTION
   USE_SIGVEC
   USE_SINAL	Specifies which type of signal handling calls to use
		on Unix systems. The preferred one is SIGACTION.
		SIGNAL is unreliable and should not be used unless
		absolutely necessary.

   HAS_GETTIMEOFDAY
   HAS_FTIME
   HAS_TIMES    specifies which function to call to get time in
                fmetime (). We like gettimeofday() best and it is
                there on most recent UNIX systems.

   PSC_TABLESIZE 
		Set this to PSC_GETDTABLESIZE if your machine supports
		the getdtablesize call, Set this to PSC_GETRLIMIT if 
		your machine supports the getrlimit call
*/

#if OPSYS==UNIX || OPSYS==VMS
#define BUFSCRN 1
#else
#define BUFSCRN 0
#endif

/* For machines with 2 byte ints, fixes -s > 63 in drargs.c */
#if (OPSYS==MS_DOS && !NW386) || OPSYS==BTOS || XENIXAT
#define TWO_BYTE_INT 1
#else
#define TWO_BYTE_INT 0
#endif

#if OPSYS==VMS 
#define UPFILENAM   1
#elif OPSYS==MS_DOS || OPSYS==BTOS || OPSYS==OS2 || OPSYS==VM || OPSYS==WIN32API
#define UPFILENAM   1
#else
#define UPFILENAM   0
#endif

#if UNIXV7 || UNISYSB20 || VRX8600 || OPSYS==VM
#define FCNTL 0
#else
#define FCNTL 1
#endif

/* NOPUTENV for systems that can't utilize getenv() and putenv(). */
#if SEQUENT
#define NOPUTENV 1
#else
#define NOPUTENV 0
#endif

#if UNIXSYS3 | UNIXSYS5 | UNIXBSD2
#define PGRPCTL 1
#else
#define PGRPCTL 0
#endif

#if ICONPORT | HP840 | HONEYWELL | MEGADATA | NECXL32 | OPSYS==MPEIX
#define LOCKF	1
#else
#define LOCKF	0
#endif

#if OPSYS==UNIX
#define ULIMIT  1
#else
#define ULIMIT 0
#endif

#if (UNIXV7 | UNIXBSD2 | XENIXAT | INTEL286 | ALT286) & !(HP840 | \
     HARRIS)
#define UTCLEXI 1
#else
#define UTCLEXI 0
#endif

#if (UNIXSYS5 | HP840 | PYRAMID | MIPS | OLIVETTI | HARRIS | \
    SCO | SGI) & !EDISA & !LINUX
#define STERMIO 1
#else
#define STERMIO 0
#endif

#if UNIXSYS3 | EDISA | LINUX
#define TERMIO  1
#else
#define TERMIO 0
#endif

#if UNIXBSD2 & !(HP840 | PYRAMID | MIPS | XSEQUENT | HARRIS )
#define LDISC   1
#else
#define LDISC 0
#endif

#if UNIXBSD2 || MAC_AUX || SEQUENTPTX
#define JOBSTP  1
#else
#define JOBSTP 0
#endif

#if UNIXSYS5
#define KERNEL 0
#if MAC_AUX | SIEMENS | PS2AIX
#undef KERNEL
#endif
#endif

#if UNIXBSD2  | ALT286 | CCI | HP320 | SEQUENTPTX
#define FSBSD42 1
#else
#define FSBSD42 0
#endif

#if SUN | PYRAMID | SUN4 | ROADRUNNER | MIPS | OLIVETTI | DG88K | SGI | DG5225 \
	| SUN45
#define NFSBSD42 1
#else
#define NFSBSD42 0
#endif

#if UNIXBSD2 | CCI  | MAC_AUX | HP320
#define DIRBSD42 1
#else
#define DIRBSD42 0
#endif

#if UNIXBSD2 | CCI | MAC_AUX
#if ! ( OLIVETTI | HARRIS)
#define SIGBSD42 1
#else
#define SIGBSD42 0
#endif
#else
#define SIGBSD42 0
#endif

/* AIOBSD42 should only be truned on if absolutly necessary!!
   The way to test is to build a client and see if you can 
   break out of a pause (like for each cust: display cust)
   with ^C.  If you can't, you need to add your machine to 
   this list.  It used to be turned on for the following machine.
   -MikeF 3/30/95
#if UNIXBSD2 & ! (HP840 | XSEQUENT | DG88K | XSEQUENT| DG5225 )
*/
#if SUN4
#define AIOBSD42 1
#else
#define AIOBSD42 0
#endif

#if UNIXBSD2
#define OPRBSD42 1
#else
#define OPRBSD42 0
#endif

#if VAX | RT | PYTHON | HP840 | VRX8600 | PRIME316 | SEQUENT | SUN4 \
	| ROADRUNNER | SCO | MIPS | ULTRIX | DECSTN  | ATT386 \
	| PS2AIX | ALT386 | DG88K | IBMRIOS | ICL6000 | NCR486 | ICL3000 \
        | SEQUENTPTX | CT386 | SGI | DG5225 | HP825 | HP3000 | MOTOROLAV4 \
        | UNIX486V4 | SUN45 | ALPHAOSF
#define ALVARIES 1
#else
#define ALVARIES 0
#endif

#if ICONPORT | IBM9370
#define SWRITE 1
#else
#define SWRITE 0
#endif

#if HP320 | HP840
#define HPBAUDS 1
#else
#define HPBAUDS 0
#endif

/**/
/*********************************************************/
/**                                                     **/
/**     MAXUSRS and DEFTUSRS used to specify the        **/
/**     maximum number of users allowed and the         **/
/**     default used if -n parameter not specified.     **/
/**                                                     **/
/*********************************************************/

/* 32K Users is the limit for -n */
#define MAXUSRS 32000

#define DEFTUSRS 20

/* maximum file descriptor+1.  Specifies how many bits in select bit mask*/
#if ALPHAOSF
#include <sys/types.h>
#define MAXFD FD_SETSIZE
#else
#if PS2AIX | CT386
#define BSD43SOCKETS    1
#include <sys/types.h>
#define MAXFD FD_SETSIZE
#else
#if SUN4 | SUNOS4 | ROADRUNNER
#define BSD43SOCKETS    1
#define MAXFD 256
#else
#define BSD43SOCKETS    0
#if TOWER700
#define MAXFD 64
#else
#if OS2_INTEL
#define MAXFD 128
#else
#if OPSYS==WIN32API

#if defined(FD_SETSIZE)
#undef FD_SETSIZE
#endif
#define FD_SETSIZE 256

#define MAXFD FD_SETSIZE
#else
#if IBM
#if DOSWIN || BSDWINWIN3 || BSDLWPWIN3 || BSDHPWIN3
#define MAXFD 16
#else
#define MAXFD 32
#endif /* DOSWIN ... */
#else
#define MAXFD 256
#endif	/* IBM */
#endif /*WIN32API*/
#endif	/* OS2_INTEL */
#endif	/* tower700 */
#endif /* SUN4 ... */
#endif /* PS2AIX */
#endif /*  ALPHAOSF */

#if SOLARIS
#include <sys/types.h>
#endif

#define MAXMSK  ((( MAXFD - 1 ) / 32 ) + 1)

/**/
#if (UNIXV7 | SEQUENTPTX | UNIXBSD2 | XENIXAT | INTEL286 | SGI) \
    & !HP840 & !OLIVETTI
#define DUP2    1
#endif

#if OPSYS==BTOS
#define DUP2    0
#else
#if  MSC || OPSYS==MPEIX || OPSYS==OS2
#define DUP2    1
#endif
#ifndef DUP2
#define DUP2    0
#endif
#endif

#if OPSYS==MS_DOS || OPSYS==WIN32API
/* char   *longtoptr(); judd: Already prototyped in utdpub.h */
/* long   ptrtolong();  judd: Already prototyped in utdpub.h */
#define DOSSCRN 1
#else
#define DOSSCRN 0
#endif

/*************************/
/* Compiler/Chip defines */
/*************************/
/* SHORTINT means int is a short */
#if (IBM | XENIXAT | INTEL286 | ALT286 | SYSTEMVAT | UNISYSB20 | IBMWIN) \
    && !(TI1500)
#define SHORTINT 1
#else
#define SHORTINT 0
#endif /* IBM */

#if UNIX64
#define LONGPTR 1
#else
#define LONGPTR 0
#endif


/**/
/******************************************************************/
/******************************************************************/
#if PYRAMID | ULTRIX | VAX | ATT3B5 | NIXDORF | SEQUENT | SEQUENTPTX
#define BLKSIZE  8192
#define BLKLOG 13
#define MAXMBPATH 255	/* max pathname stored in master block. Do not
			   change. This must be a multiple of 4 - 1 to
			   allow for a trailing null so the next field
			   in the master block will be longword aligned. */
#else
#if OPSYS==WIN32API
#define BLKSIZE 8192
#define BLKLOG 13
#define MAXMBPATH 255	/* max pathname stored in master block. Do not
			   change. This must be a multiple of 4 - 1 to
			   allow for a trailing null so the next field
			   in the master block will be longword aligned. */
#else
#define BLKSIZE  8192
#define BLKLOG 13
#define MAXMBPATH 255	/* max pathname stored in master block. Do not
			   change. This must be a multiple of 4 - 1 to
			   allow for a trailing null so the next field
			   in the master block will be longword aligned. */
#endif /* WIN32API */
#endif /* PYRAMID */

#define DEFAULT_RECBITS  8

#define MAXDBBLOCKSIZE 8192     /* max database blocksize supported = 8K */
#define MAXBLKSIZECHRS 4   /* 8192 is the max size supported */

#if OPSYS==XWIN32API
#define MINBLKSIZE 4096   /* 4096 is the minimim block size */
#else
#define MINBLKSIZE 1024
#endif

#define QRYMINIDXS   6000  /**used as runtime range for qry statement idxs*/
#define QRYMAXIDXS   8000  /**used as runtime range for qry statement idxs*/

#if OPSYS==UNIX
#if PS2AIX
#define SUPERBOFF 4096
#define SUPERBSIZ 4096
#else
#if IBMRIOS
#define SUPERBOFF(dev)  (BSIZE)
#define SUPERBSIZ 4096
#else
#if INTEL286 | XENIXAT  | ALT286 | SCO | EDISA
#define SUPERBOFF 1024
#define SUPERBSIZ 1024
#else
#define SUPERBOFF 512
#define SUPERBSIZ 512
#endif
#endif
#endif
#endif

/***********/
/* Raw I/O */
/***********/
/* use raw i/o */
/* NOTE: turn this flag OFF when OSYNCFLG or DIRECTIO is on */
#if OPSYS==UNIX
#if INTEL286 | ICONPORT | DDE | ROADRUNNER | DGMV | HP840 | MIPS \
	| OLIVETTI | DIAB | DECSTN | ULTRIX | SUNOS4 | SUN4 | SCO \
	| ATT386 | TOWER700 | TOWER650 | TOWER800 | BULLXPS | SIEMENS \
	| BULLQ700 | PS2AIX | HARRIS | ALT386 | DG88K | SYS25ICL \
	| PRIME316 | MOTOROLA | ICLDRS | BULLDPX2 | ARIX825 | ARIXS90 | TI1500 \
	| IBMRIOS | ICL6000 | ATT3B2600 | NCR486 | ICL3000 | SEQUENTPTX \
	| CT386 | SGI | SEQUENT | DG5225 | HP825 | MOTOROLAV4 | UNIX486V4 \
	| SUN45 | ALPHAOSF | LINUX
#define RAWDEFLT 0
#else
#define RAWDEFLT 1
#endif
#else
#define RAWDEFLT 0
#endif

/* NEW Raw i/o controller variables, RAWDEFLT, SWRITE will be phased out */
/* NOTE: RAWDEFLT should be OFF if this flag is ON */
#if DDE | ROADRUNNER | DGMV | HP840 | MIPS | OLIVETTI | DIAB | ULTRIX \
	| DECSTN | ATT3B2600 | SUNOS4 | SUN4 | SCO | ATT386 \
	| TOWER700 | TOWER650 | TOWER800 | BULLXPS | SIEMENS \
	| BULLQ700 | PS2AIX | HARRIS | ALT386 | DG88K | SYS25ICL \
	| PRIME316 | MOTOROLA | ICLDRS | BULLDPX2 | ARIX825 | ARIXS90 \
	| TI1500 | IBMRIOS | ICL6000 | NCR486 | ICL3000 \
        | CT386 | SGI | DG5225 | HP825 | MOTOROLAV4 | UNIX486V4 \
	| SUN45 | ALPHAOSF | LINUX 
#define OSYNCFLG	1
#else
#define OSYNCFLG	0
#endif

/* Direct i/o does not use the unix buffer pool. It is like doing raw
   i/o to regular files. It is a lot faster than using OSYNC */
/* NOTE: RAWDEFLT should be OFF if this flag is ON */
#if SEQUENT | SEQUENTPTX | DG5225
#define DIRECTIO 1
#else
#define DIRECTIO 0
#endif

#if CT | ICONPORT | OSYNCFLG
#define SWRTAVAIL 1
#else
#define SWRTAVAIL 0
#endif

#if RAWDEFLT
#define RAWAVAIL 1
#else
#define RAWAVAIL 0
#endif

#if SEQUENT | SEQUENTPTX
/* We can get high resolution timing info for performance monitoring
   at a modest cost (extra overhead has small impact on performance) */

#define HAS_US_TIME 1
#define INIUSCLK() usclk_init()
/* GETUSCLK defined by Sequent os */
#define US_MARK()       (GETUSCLK())
#endif

#if SUN4 || HP825
/* We have some functions that aren't too bad */

#define HAS_US_TIME 1
ULONG utrdusclk ();
#define INIUSCLK()
#define GETUSCLK() (ULONG)(utrdusclk())

#define US_MARK()       ((ULONG)utrdusclk())
#endif

#ifndef HAS_US_TIME
#define HAS_US_TIME 0
#define INIUSCLK()
#define GETUSCLK()      ((ULONG)0)
#define US_MARK()       ((ULONG)0)
#define TIMING 0
#endif

#if HAS_US_TIME
#define TIMING 1
#endif


#if SHORTINT
#define INTLONG(VAL) countl(VAL)
#else
#define INTLONG(VAL) (VAL)
#endif


/* I'm trying to get utf stuff to use this instead of MAXPATHN */
/* No one must ever change the values of MAXPATHN because it got */
/* put into the structure declaration for the db master block    */
#define MAXUTFLEN 300


/*unix like interface stuff */
#if OPSYS==BTOS
#define MAXPATHN 92
#define NBITSPINT	(sizeof(int) * 8)
#define PATHSEP ':'
#define DIRSEP	'>'
#define NULLDEV "[NUL]"
#define MAXFILN 48
#endif

#if OPSYS==UNIX
/* maximum length of a fully formed path name and
   maximum length of a file name component minus two */
#define MAXPATHN 255
#define PATHSEP ':'
#define DIRSEP '/'
#define NULLDEV "/dev/null"
#define MAXFILN 12
#endif

#if OPSYS==MPEIX      /* Posix-compliant: see <limits.h> */ /*EST,12/15/93*/
#define MAXFILN   NAME_MAX - 2        /* currently 253 */
#define MAXPATHN  PATH_MAX    /* currently 1023 */
#define DIRSEP    '/'
#define PATHSEP   ':'
#define NBITSPINT (sizeof(int) * CHAR_BIT)
#endif

#if OPSYS==MS_DOS
#define MAXPATHN 63
#define NBITSPINT       (sizeof(int) * 8)
#define PATHSEP ';'
#define DIRSEP  '\\'
#define NULLDEV "NUL:"
#define MAXFILN 12
#define  MAXFILROOT 8
#define MAXEXTN 3
#endif

#if OPSYS==WIN32API || OPSYS==OS2
#define MAXPATHN 255
#define PATHSEP ';'
#define DIRSEP '\\'
#define NULLDEV "NUL:"
#define MAXFILN 255
#endif

#if OPSYS==VMS
#define PATHSEP ','
#define DIRSEP '/'
#define LOGICALSEP ':'
#define MAXPATHN 63
#define NULLDEV "NL:"
#define MAXFILN 31
#endif

#if OPSYS==VM
#define PATHSEP '.'
#define DIRSEP '.'
#define MAXPATHN 63
#define NULLDEV "/dev/null"
#define MAXFILN 8
#endif

#if OPSYS==VRX
#define PATHSEP ':'
#define DIRSEP '.'
#define MAXPATHN 128
#define NULLDEV "/dev/null"
#define MAXFILN 10
#endif

#if OPSYS==UNIX
/********************************************************/
/* used in sys/filsys.h but Fortune defines them wrong  */
/* should be defined in sys/param.h                     */
/********************************************************/
#define NICINOD  100    /* number of free inodes in super block */
#if INTEL286 | XENIXAT | ALT286 | SCO
#define NICFREE 100
#else
#define NICFREE  50     /* number of free blocks in super block */
#endif
#if INTEL286 | XENIXAT | ALT286 | SCO
/*****************************************************************/
/**     This value is defined in /usr/include/sys/param.h,      **/
/**     but this header file also defines variables which       **/
/**     conflict with those defined by dstd.h.                  **/
/*****************************************************************/
#if SCO
#define NSBFILL 371
#else
#define NSBFILL 370
#endif
#endif
#endif


#if OPSYS==MS_DOS
/*#ifndef NOVERLAY */
/*#define OVERLAY */
/*#else */
/*#undef OVERLAY */
/*#endif */
#else
#if OPSYS!=VMS
#undef OVERLAY
#endif
#endif



/* define max. allowed locks */
#define MAXLOCKS  (0x7fffffff / sizeof(struct lck) * 2)

#define MSGVERS 3
/* parameter to bkwrite and a few others */
#define BUFFERED   1
#define UNBUFFERED 0

#if (UNIXBSD2) && !SEQUENTPTX
#define GETHOSTN 1
#else
#define GETHOSTN 0
#endif

/*
   CLIB_VARARGS on if the varargs(3) mechanism is used in drmsg and utfprtf.
                This should eventually be on for all machines but the
                only machines that require it are RISC. New ports whether
                or not they are RISC based should use this.
*/
#if DGMV | MIPS | HP840 | DECSTN | BULLXPS | SIEMENS | PYRAMID | HARRIS \
	| DG88K | SUN4 | TI1500 | IBMRIOS | ICL6000 | SEQUENTPTX | SGI \
	| HP825 | DG5225 | OPSYS==MPEIX | OPSYS==WIN32API | OS2_INTEL \
	| SCO | NW386 | MOTOROLAV4 | UNIX486V4 | SUN45 | ALPHAOSF \
	| LINUX
#define CLIB_VARARGS 1
#else
#define CLIB_VARARGS 0
#endif

/*
   NOTSTSET     TRUE if machine does not have a tstset (tas) instruction.
                This uses extra system semaphores.
*/
#if DECSTN | ULTRIX
#define NOTSTSET 1
#else
#define NOTSTSET 0
#endif

/*
   MIPSTAS      TRUE if we are using the algorithm proposed by mips
*/
#if MIPS | XHP825 | XHP840 | XSIEMENS | XIBMRIOS 
#define MIPSTAS 1
#else
#define MIPSTAS 0
#endif

#if DECSTN         /* TRUE if using DEC/Ultrix atomic_op system call */
#define DECTAS 1
#endif

/* HEAPSIZ     Minimum size of the heap - used in determining location of
   Shared Memory - set to NULL if not applicable */
#if TOWERXP
#define  HEAPSIZ  256           /* K of heap */
#else
#if TOWER32 | TOWER650 | TOWER800 | TOWER700
#define  HEAPSIZ  500           /* K of heap */
#else
#define  HEAPSIZ  NULL
#endif
#endif



#if 1	/* Turned off 5/31/94 - MikeF */
#define HASAFUNIX 0
#else
/* Srini: for m/c's that do not do AF_UNIX but do AF_INET */
#if CTMIGHTY | HP320 | ATT3B2600 | DIAB | ATT386 | TOWER700 \
	| MOTOROLA | SIEMENS | ARIX825 | ARIXS90 | ICLDRS | DDE | OS2_INTEL \
	| NCR486 | ICL3000 | CT386 | NW386 | SCO | OPSYS==MPEIX  \
        | MOTOROLAV4 | UNIX486V4
#define HASAFUNIX 0
#else
#define HASAFUNIX 1
#endif
#endif /* if 1 - MikeF */

/* For machines that have C-ISAM. Used for C-ISAM .df file extractor code
   in upcisam.c.

   We're assuming that all Unix platforms have C-ISAM.  If there are machines
   with the Unix operating system that DON'T support C-ISAM,  modify the #if.
   For example:

   #if OPSYS==UNIX && !SOME_MACHINE
   #define PROCISM 1
   #else
   #define PROCISM 0
   #endif

*/
#if OPSYS==UNIX && !UNIX64
#define PROCISM 1
#else
#define PROCISM 0
#endif


/* WINSYS is now defined in a separate include file to facilitate compiling
** with Motif and other X related software.
*/



/* PROCONST is used to move data out of the default data segment (so that we
can get it under 64K so we can run multiple instances of Progress under
windows) and into the code segment.
Obviously, this data should be read-only. - Neil */



/* Select the signal handler functions to use for Unix. see drsig.c
*/
#define USE_SIGACTION    0
#define USE_SIGVEC       0
#define USE_SIGNAL       0


#ifndef bzero
#define bzero(s,n)      memset((s),(int)0,(n))
#endif

/* which time function to use in fmetime(). Most recent UNIX systems
   have gettimeofday(), but not all */

#if SUN4 || HP825 || IBMRIOS || DG5225 || MOTOROLAV4 || UNIX486V4 || SUN45 \
         || ICL6000 || ALPHAOSF || LINUX
#define HAS_GETTIMEOFDAY        1 /* we prefer this one if available */
#define HAS_FTIME               0
#define HAS_TIMES               0
#endif

#if VMS==OPSYS
#define HAS_GETTIMEOFDAY        0
#define HAS_FTIME               1
#define HAS_TIMES               0
#endif

#if (OPSYS==MS_DOS && !NW386) || SCO || OPSYS==WIN32API || OPSYS==OS2
#define HAS_GETTIMEOFDAY        0
#define HAS_FTIME               1
#define HAS_TIMES               0
#endif

#if SEQUENTPTX || NW386
#define HAS_GETTIMEOFDAY        0
#define HAS_FTIME               0
#define HAS_TIMES               1
#endif

#define PSC_GETDTABLESIZE	1
#define PSC_GETRLIMIT		2

#if SUN4 | IBMRIOS
#define PSC_TABLESIZE PSC_GETDTABLESIZE
#endif

#if HP825 | HPUX | SOLARIS | SUN45 | UNIX486V4 | ALPHAOSF | SCO \
          | LINUX
#define PSC_TABLESIZE PSC_GETRLIMIT
#endif


/*************************************************************************/
/*************************************************************************/
/***   SEE NOTE BELOW -Progress include files only after this line   *****/
/*************************************************************************/
/*************************************************************************/
/* Everything below here should be Progress include files! */

/* miscellaneous structure names for PROGRESS */
#if ANSI_LEVEL == 0




#endif /* ANSI_LEVEL == 0 */

#ifdef __cplusplus
}
#endif
/* If system has bcopy and it is faster, use it */

#include <string.h>
#if SUN4 | SEQUENTPTX | SEQUENT | HP825
#define bufcop(t,s,n) bcopy((char *)(s), (char *)(t), (int)(n))
#else
#define bufcop(t,s,n) memmove((char *)(t), (char *)(s), (int)(n))
#endif

#ifndef HAVE_OLBACKUP
#define HAVE_OLBACKUP 1
#endif

#ifndef VARIABLE_LOG_CLUSTER_SIZE
#define VARIABLE_LOG_CLUSTER_SIZE
#endif

/*************************************************************************/
/*************************************************************************/
/***   SEE NOTE ABOVE -Progress include files only in this section   *****/
/***   Nothing should go below here!!!!!!                            *****/
/*************************************************************************/
/*************************************************************************/

#endif /* PSCSYS_H */
