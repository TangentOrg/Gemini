
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "dbconfig.h"
#include "utospub.h"


#if  !(IBM || NW386 || WINNT || WIN95 || OS2_INTEL || IBMWIN)

#if SUN45 || SOLARISX86  /* This is the solaris code to get the stack trace */

#include <dlfcn.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/reg.h>
#include <sys/frame.h>

#if defined(sparc) | defined(__sparc)
#define FLUSHWIN() asm("ta 3");
#define FRAME_PTR_INDEX 1
#define SKIP_FRAMES 0
#endif

#if defined(i386) || defined(__i386)
#define FLUSHWIN()
#define FRAME_PTR_INDEX 3
#define SKIP_FRAMES 1
#endif

#if defined(ppc) || defined(__ppc)
#define FLUSHWIN()
#define FRAME_PTR_INDEX 0
#define SKIP_FRAMES 2
#endif

/* PROGRAM: uttraceback - performs system dependent traceback for solaris
 *
 * RETURNS: DSMVOID
 */
DSMVOID
uttraceback(int fd)
{
	struct frame 	*sp;
	jmp_buf	 	 env;
	int		 i;
	int		*iptr;
        int              syserror;
        int              amtWritten;
        int              stringSize;
        int              defaultSymbols;
	Dl_info	  	 info;
	char		*func;
	char		*lib;
	char 		 wrtbuf[1024];
	char 		*pbuf = wrtbuf;
	FLUSHWIN();

	setjmp(env);
	iptr = (int *)env;

	sp = (struct frame *) iptr[FRAME_PTR_INDEX];

	for (i=0; i < SKIP_FRAMES && sp; i++)
	{
	    sp = sp->fr_savfp;
	}

	i = 0;

	while(sp && sp->fr_savpc && ++i)
	{
	    if (dladdr((void *)sp->fr_savpc, &info) == 0)
	    {
	        func = "??";
	        lib = "??";
	    }
	    else
	    {
                /* It's possible that if no symbol is found to describe the
                   specified address, both the dli_sname and dli_saddr members
                   are set to 0. */
	        if ((func = (char *) info.dli_sname) == 0)
                {
                    func = "??";
                }
	        lib = (char *) info.dli_fname;
	    }
            memset(pbuf, '\0',sizeof(pbuf));

           /* Remember to be careful of the size of the write buffer
              by carefully checking the sizes of each of the items
              passed to sprintf to ensure we don't exceed it. */

            defaultSymbols = 13; /* used symbols in sprintf */

            /* Calculate the largest size of the proposed string */
            stringSize = strlen(lib) +          /* fault library name */
                         strlen(func) +         /* fault function name */
                         sizeof(unsigned int) + /* fault address */
                         defaultSymbols;        /* "%s() +0x%x from: %s\n" */
                         
            if (stringSize < 1024) /* stringSize doesn't include null term */
            {
	        sprintf(pbuf, "%s() +0x%x from: %s\n", func, 
		    (unsigned int)sp->fr_savpc - (unsigned int)info.dli_saddr,
                     lib);
                write(fd, (TEXT *)pbuf, strlen(pbuf));
            }
            else
            {
	        sprintf(pbuf, "uttraceback: output buffer overflow"); 
                write(fd, (TEXT *)pbuf, strlen(pbuf));
            }

	    sp = sp->fr_savfp;
	}
}

#endif	/* End of Solaris code for tracebacks */

#if HPUX
/* PROGRAM: uttraceback - performs system dependent traceback for hpux
 *
 * RETURNS: DSMVOID
 */
void
uttraceback(int fd)
{

    dup2(fd, 2);
    U_STACK_TRACE();
}

#endif


#if UNIX486V4  || LINUXX86
#define _SCO_DS
#endif

#if SCO || UNIX486V4 || LINUXX86

/* This program converts the output from a trace dump on Linux into
   the function calls that match the addresses.  Input to the program
   is the output from the nm command and the trace file.
*/

#define LINE_MAX 255

/* Define macros */
#define is_sp_tab(a) ((a) == ' ') || ((a) == '\t')
#define is_sp_tab_nl(a) ((a) == ' ') || ((a) == '\t') || ((a) == '\n') || ((a) == '\0')
#ifndef MIN
#define MIN(a,b) ((a)<=(b)?(a):(b))
#endif


typedef struct nminfo
{
    unsigned long  addr;
    char           name[32];
} nminfo_t;

static nminfo_t *nm;
static long nm_lines = 0;

static char get_line(FILE *fp, char *in_line)
{
    char in_char;
    int  i = 0;

    do {
        in_char = getc(fp);
        in_line[i++] = in_char;
    } while ((in_char != '\n') && (in_char != EOF));

    in_line[i] = '\0'; /* terminate the string */

    return(in_char);
}

static int LoadNmFile(FILE *nm_file, nminfo_t *nm)
{
    int		row = 0;
    int         col  = 0;
    int         len  = 0;
    int         tok_beg  = 0;
    char 	in_char = ' ';
    char 	in_line[LINE_MAX];
    char 	buffer[80];

    while (in_char != EOF)
    {
        memset(buffer,'\0',80);
        memset(in_line,'\0',LINE_MAX);

        in_char = get_line(nm_file,in_line);
        if (in_char == EOF)
        {
            continue;
        }
        col = 0;

        /* Find first non-blank character. */
        while (is_sp_tab(in_line[col]))
            col++;

        /* first token is addr */
        tok_beg = col;
        while(!is_sp_tab(in_line[col]))
            col++;

        strncpy(buffer, &in_line[tok_beg],
                MIN((int)sizeof(buffer)-1,col - tok_beg));
        buffer[col - tok_beg] = '\0';
        nm[row].addr = strtoul(buffer,(char **)0,16);

        /* Find first non-blank character. */
        while (is_sp_tab(in_line[col]))
            col++;

        /* If we see "T " or "t " or anything like it, skip it - it's 
        ** not the symbol (assuming here that there are no one-character
        ** symbols in the symbol file).
        */
        if (in_line[col + 1] == ' ')
        {
            col++;  /* skip the "T" */
            /* skip whitespace */
            while (is_sp_tab(in_line[col]))
                col++;
        }

        /* next token is name */
        tok_beg = col;
        while(!is_sp_tab(in_line[col]))
            col++;

        strncpy(nm[row].name, &in_line[tok_beg],
                MIN((int)sizeof(nm[row].name) -1, col - tok_beg));

        /* remove any white space at the end */
        col = len = strlen(nm[row].name);

        /* Find first non-blank character. */
        while (is_sp_tab_nl(nm[row].name[col]))
            col--;

        if (col != len)
        {
            /* we must have found atleast 1 blank so truncate the line */
            nm[row].name[col+1] = '\0';
        }

#if DEBUG1
        printf("address = %d %x, name = %s\n",nm[row].addr, nm[row].addr, nm[row].name);
#endif
        row++;
    }
    return(row); 
} 

int readsymbolfile(const char *pname)
{
    FILE        *nm_file;
    struct stat	 stbuf;
    int		 ret = 0;

    /* open NM file */
    if (!(nm_file = fopen(pname, "r")))
    {
        return 0;
    }

    /* need to get memory for the nm table */
    ret = fstat(fileno(nm_file), &stbuf);
    if (ret < 0)
    {
        return 0;
    }

    /* assume that the average line length in the symbol file is around
    ** 24 characters (it's probably much higher).  This should be changed
    ** sometime to not overallocate memory.
    */
    nm = (nminfo_t *)malloc((stbuf.st_size / 24) * sizeof(nminfo_t));
    if (nm == (nminfo_t *)0)
    {
        return 0;
    }

    nm_lines = LoadNmFile(nm_file,nm);

    fclose(nm_file);

    return 1;
}


#if DEBUG2
#define DEBUG3
#endif

static int getsymbol(unsigned long addr, char *symbol, int maxsymlen)
{
    long	low = 0;
    long	mid;
    long	hi = nm_lines - 1;

    if (!nm_lines)
        return 0;

    while(low < hi)
    {
#ifdef DEBUG3
        printf("nm[%d] = %d nm[%d] =  %d tr[%d] = %d\n",
               low, nm[low].addr,hi, nm[hi].addr,i,addr);
#endif
        /* recalc the mid point, by taking low + hi and dividing by 2 */
        mid = (low + hi + 1)/2;

        /* if the address is greater than the mid address, then
           move the lower part to the middle */
        if (addr < nm[mid].addr)
        {
            hi = mid - 1;
        }
        /* if the address is less than the mid point, then 
           move the hi point to the mid point */
        else 
        {
            low = mid;
        }
    }
    strncpy(symbol, nm[low].name, maxsymlen);
    symbol[maxsymlen - 1] = '\0';

//    printf("%8X ---> %s %s\n",tr[i].func, nm[low].name, tr[i].args);

    return 1;
}

#ifdef _SCO_DS
/*
 * SCO_DS is defined only on OpenServer 5 and later.  The OSR5
 * compiler supports inline assembler functions.
 */
#if LINUXX86
int *getebp()
{
  int *ebp;
  asm("movl %%ebp, %0"
      :"=r"(ebp)
      :"r"(ebp));
  return ebp;
}
#else
asm int *getebp()
{
        movl    %ebp, %eax
}
#endif
#endif /* _SCO_DS */

/* PROGRAM: uttraceback - performs system dependent traceback for hpux
 *
 * RETURNS: DSMVOID
 */
void
uttraceback(int fd)
{
register int            *ebp;
         int            *esp;
         int             caller;
         int             syserror;
         int             amtWritten;
         char            wrtbuf[80];
         char           *pbuf = wrtbuf;
#if LINUXX86
         char            psym[80];
#endif /* LINUXX86 */

#ifdef _SCO_DS
    /* OSR5 or later */
    ebp = getebp();
#else
    /*
     * Older Microsoft compiler, first register variable is
     * always stored in ebx
     */
    _asm mov ebx, ebp;
#endif /* _SCO_DS */

    esp = ebp; /* for sanity check */
    caller = ebp[1]; /* Return addr of function that called stack_trace */

    while (ebp >= esp)
    {
        /*
         * Back up to previous frame, and print 4 integer arguments
         */
        ebp = (int *) *ebp;
        if (ebp)
        {
            memset(pbuf, '\0',sizeof(pbuf));

            if (getsymbol(caller, psym, sizeof(psym)))
            {
                sprintf(pbuf, "%x %s (x%x, x%x, x%x, x%x)\n",
                        caller, psym, ebp[2], ebp[3], ebp[4], ebp[5]);
            }
            else
            {
                sprintf(pbuf,"%x (x%x, x%x, x%x, x%x)\n",
                        caller, ebp[2], ebp[3], ebp[4], ebp[5]);
            }

            write(fd, (TEXT *)pbuf, strlen(pbuf));
            /*
             * Get next return address */
            caller = ebp[1];
        }
    }
}

#endif  /* End of SCO UNIX code for tracebacks */


#if ALPHAOSF
/*
  Digital Unix trace back for Progress.
  Only use reentrant system calls here (as much as possible)
  Allocate all scratch memory on the stack.
*/

#include <unistd.h>
#include <sym.h>
#include <excpt.h>
#include <sys/types.h>
#include <loader.h>
#include <a.out.h>
#include <elf_mips.h>
#include <alloca.h>


static ltohex(long lung, char **ppChar)
{ char *hex = "0123456789abcdef";
  unsigned long mask = 0xf000000000000000, shift;
  int hoz = 1;

  *(*ppChar)++ = '0';
  *(*ppChar)++ = 'x';
  for (shift = 60; mask; mask >>= 4, shift -= 4)
    { char nybble = (lung & mask) >> shift;
      if (hoz)
      { if (nybble || mask == 0xf)
          { hoz = 0;
            *(*ppChar)++ = hex[nybble];
          }
      }
      else *(*ppChar)++ = hex[nybble];
    }
}


void uttraceback(int fd)
{
  struct sigcontext context;
  unsigned long sc_pc;
  int              syserror;
  int              amtWritten;


  struct utsym
  { struct utsym *next;
    unsigned long start, end;
    long adjust;
    char *dynstr;
    Elf32_Sym *dynsym;
    int nSym;
  } utsymList = { 0, 0, 0, 0, 0, 0, 0 }, *utsym = &utsymList;

  /* extract ldr info into utsym list*/
  { ldr_process_t pid = ldr_my_process();
    ldr_module_t mid = LDR_NULL_MODULE;
    ldr_module_info_t minfo;
    ldr_region_info_t rinfo;
    size_t sz;

    for (ldr_next_module(pid, &mid);
         ! ldr_inq_module(pid, mid, &minfo, sizeof(minfo), &sz);
         ldr_next_module(pid, &mid) )
    { ldr_region_t rid = 0;

      while (rid < minfo.lmi_nregion)
      { ldr_inq_region(pid, mid, rid++, &rinfo, sizeof(rinfo), &sz);
        if (strcmp(rinfo.lri_name,".text")==0)
        { struct filehdr *fh = rinfo.lri_mapaddr;
          struct scnhdr *sh = (void*)((char *)(fh + 1) + fh->f_opthdr);
          int sid = 0;

          if (utsym->start) 
          { utsym->next = (void *)alloca(sizeof(*utsym));
            utsym = utsym->next;
            utsym->next = 0;
          }
          utsym->start = (unsigned long)rinfo.lri_mapaddr;
          utsym->end = utsym->start + rinfo.lri_size;
          utsym->adjust = (char *)rinfo.lri_vaddr - (char *)rinfo.lri_mapaddr;

          while (sid < fh->f_nscns)
          { if (strcmp(sh[sid].s_name,".dynstr") == 0)
                utsym->dynstr = (char *)sh[sid].s_vaddr - utsym->adjust;
            else if (strcmp(sh[sid].s_name,".dynsym") == 0)
            { utsym->nSym = sh[sid].s_size / sizeof(Elf32_Sym);
              utsym->dynsym = (void*)((char *)sh[sid].s_vaddr - utsym->adjust);
            }
            sid++;
          }
        }
      }
    }
    utsym = &utsymList;
  }


  /* virtually unwind contexts */
  for ( exc_capture_context(&context),
        exc_virtual_unwind(exc_lookup_function_entry(context.sc_pc), &context);
        sc_pc = context.sc_pc;
        exc_virtual_unwind(exc_lookup_function_entry(context.sc_pc), &context))
  { Elf32_Sym *best = 0;

    /* find best dynamic symbol name */
    for (utsym = &utsymList; utsym; utsym = utsym->next)
    { if (sc_pc >= utsym->start && sc_pc < utsym->end)
        { int nSym;
          Elf32_Sym *dynsym = utsym->dynsym;

          for (nSym = 0; nSym < utsym->nSym; dynsym++, nSym++)
            { if (ELF32_ST_TYPE(dynsym->st_info) == STT_FUNC &&
                  dynsym->st_value)
                if (sc_pc >= (dynsym->st_value - utsym->adjust))
                { if (best)
                  { if (dynsym->st_value > best->st_value)
                        best = dynsym;
                  }
                  else best = dynsym;
                }
            }
          break;
        }
    }

    /* output message in dbx friendly format */
    { char msg[48+19], *mptr = msg;

      ltohex(sc_pc-4,&mptr); *mptr++ = '/'; *mptr++ = 'i';
      *mptr++ = '\t';
      if (best) 
        { char *fname = utsym->dynstr + best->st_name;
          char *fptr = fname;

          while (mptr < msg+48+19 && *fptr) *mptr++ = *fptr++;
          *mptr++ = '('; *mptr++ = ')'; *mptr++ = '+';
          ltohex(sc_pc - 4 - (best->st_value - utsym->adjust),&mptr);
          if (fname[0] == 'm' &&
              fname[1] == 'a' &&
              fname[2] == 'i' &&
              fname[3] == 'n' &&
              fname[4] == 0) 
          { *mptr++ = '\n';
            write(fd, (TEXT *)msg, mptr-msgs);
            return;
          };
        }
      *mptr++ = '\n';
      write(fd, (TEXT *)msg, mptr-msg);
    }
  }
}


#endif /* ALPHAOSF */


#if !(SUN45 || SCO || UNIX486V4 || HP825 || ALPHAOSF || SOLARISX86 || LINUXX86)
void
uttraceback(int fd)
{
    int amtWritten, syserror;

    write(fd, (TEXT *)"\nAutomatic Stack Trace Functionality not supported\n", 
               strlen("\nAutomatic Stack Trace Functionality not supported\n"));
    return;
}
#endif

#endif /* !(IBM || NW386 || WINNT || WIN95 || OS2_INTEL || IBMWIN) */

