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

    /* **********************************************************
    | Test and set, test and clear routine to be used for mutual
    | exclusion on shared memory data structures. They are not
    | used during single-user mode Progress.
    |
    | These functions are *highly* machine dependent. For them to
    | work correctly, the underlying hardware mechanisms must be
    | understood and used correctly. Do not change them unless
    | you *fully* understand what you are doing. In particular,
    | some systems have alignment requirements for the locks.
    | mtctl.h has #defines to set for this. Incorrect tstset
    | functions can appear to work for a long time before they fail.
    |
    | These functions are called from the locking routines in
    | mt.c. They should not be used from anywhere else.
    | See mtctl.h for details of the latches and also for the
    | macros which are used to call these functions.
    | Note that on many machines tstclr() is not needed because it
    | is sufficient to store a zero in the lock to free it.
    |
    | Note that on some ratsh*t machines, there is no atomic
    | test-and-set instruction or its equivalent. On these systems,
    | os calls may be provided to perform the function in a slower way.
    | In the absence of os calls, Unix semaphores must be used.
    | mt.c has the required entry points. Dont put them here.
    |
    | If the OS provides system calls to manipulate the locks,
    | those calls should be put into the macros in mtctl.h, NOT HERE.
    | There is no need for the extra level of subroutine if the
    | os functions can be called from c. Stuff should be added here
    | only if assembler code is absolutely necessary.
    |
    |					ghb
    |___________________________________________________________
    |
    | To be called from c as you would call the following c function:
    | (see mtctl.h for macros which call these).
    |
    | tstset (ptr)
    |     spinLock_t  *ptr; pointer to a lock which is to be locked.
    |
    |The lock pointed by ptr is atomically tested and locked
    |Return: 0 if the lock was free before the call.
    |        1 if the lock was locked before the call.
    |	NOT NEEDED IF OS FUNCTION MUST BE CALLED
    |___________________________________________________________
    |
    | tstclr (ptr)
    |     spinLock_t  *ptr; pointer to a lock which is to be freed.
    |
    |The lock pointed to by ptr is freed.
    |Return: Nothing
    |	NOT NEEDED IF LOCK CAN BE FREED WITH A STORE
    |	NOT NEEDED IF OS FUNCTION MUST BE CALLED
    |___________________________________________________________
    | test_lock (ptr)
    |     spinLock_t  *ptr; pointer to a lock which is to be tested.
    |
    |The lock pointed to by ptr is examined.
    |Return: 0 if the lock is free.
    |        1 if the lock is locked.
    |	NOT NEEDED IF LOCK CAN BE EXAMINED WITH (*ptr == x)
    |	NOT NEEDED IF OS FUNCTION MUST BE CALLED
    |___________________________________________________________
    ********************************************************** */

#include "gem_config.h"
????????? THIS is a dummy line to be removed by sed later

#if HP825
;
; On HP PA-RISC machines, it is necessary for the locks to be
; aligned on 16 byte boundaries. Otherwise they wont work correctly
; It is up to the caller to ensure that this is the case.
; Note also that tstset returns 1 for free and 0 for locked.
; This is dealt with in mt.c and mtctl.h.
;	ghb
;
	.code
#define latch arg0
#define tmp r31

; tstset(latch)
;*****************************************************
; tstset is true if we succeeded in acquiring the latch
;****************************************************
        .proc
        .callinfo 
        .export tstset
tstset
; tstset+set, and return
        bv     (rp)
        ldcws  (latch), ret0
        .procend

; tstclr(latch)
;*****************************************************************
; STWSMA sets the value of a word used with LDCWS.  This is necessary
;   on certain PA-90 machines and should work on all PAxx.
;*********************************************************************
    .proc
    .callinfo
    .export tstclr
tstclr

; store a 1 and return
    ldi    1, tmp
    bv    (rp)
    stws  tmp, 0(latch)
    .procend

; test_lock(latch)
;*****************************************************************
; test_lock is true if the latch is presently unlocked
;*********************************************************************
    .proc
    .callinfo
    .export test_lock
test_lock
 
; get the value and return
    bv    (rp)
    ldws  0(latch), ret0
    .procend
 
;$THIS_DATA$
;
        ;.SUBSPA $SHORTDATA$,QUAD=1,ALIGN=8,ACCESS=31,SORT=24
;$THIS_SHORTDATA$
;
        ;.SUBSPA $BSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=82
;$THIS_BSS$
;
        ;.SUBSPA $SHORTBSS$,QUAD=1,ALIGN=8,ACCESS=31,ZERO,SORT=80
;$THIS_SHORTBSS$
;; 
        ;.SUBSPA $STATICDATA$,QUAD=1,ALIGN=8,ACCESS=31,SORT=16
;$STATIC_DATA$
; 
        ;.SUBSPA $SHORTSTATICDATA$,QUAD=1,ALIGN=8,ACCESS=31,SORT=24
;$SHORT_STATIC_DATA$
; 
        ;.SPACE  $TEXT$
        ;.SUBSPA $CODE$
        ;.EXPORT tstset,ENTRY,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
        ;.SUBSPA $CODE$
        ;.EXPORT tstclr,ENTRY,PRIV_LEV=3,ARGW0=GR
        ;.END

#endif /* hp825 */

#if SUN
	.data
	.text
LL0:
|#PROC# 04
	LF72	=	0
	LS72	=	0
	LFF72	=	0
	LSS72	=	0
	LP72	=	8
	.data
	.text
	.globl	_tstset
_tstset:
|#PROLOGUE# 0
	link	a6,#0
|#PROLOGUE# 1
	movl	a6@(8),a0
	tas	a0@
	jne	L74
	moveq	#0,d0
	jra	LE72
L74:
	moveq	#1,d0
LE72:
	unlk	a6
	rts
|#PROC# 04
	LF76	=	0
	LS76	=	0
	LFF76	=	0
	LSS76	=	0
	LP76	=	8
	.data
	.text
	.globl	_tstclr
_tstclr:
|#PROLOGUE# 0
	link	a6,#0
|#PROLOGUE# 1
	movl	a6@(8),a0
	clrb	a0@
	unlk	a6
	rts
#endif	/* end of SUN */

#if PYRAMID
	.data	0
	.data	1
	.text	0
	.globl	_tstset
_tstset:
	bitsw	$0x1,(pr0)
	bne	L13
	movw	$0x0,pr0
	ret	
	br	L14
L13:
	movw	$0x1,pr0
	ret	
L14:
	ret	
	.data	1
	.text	0
	.globl	_tstclr
_tstclr:
	movw	$0x0,(pr0)
	ret	
#endif /* end PYRAMID */

/* T32_800 is a multiprocessor machine.
   Shared memory test - set is done by system call */
#if TOWERXP && ( TOWER800 || TOWER700 )
int
tstset (ptr)
unsigned char    *ptr;
{
    if (shmtas(ptr) == 0)
        return 0;
    else
        return 1;
}
int
tstclr (ptr)
unsigned char    *ptr;
{
    *ptr = 0;
}
#else
#if TOWERXP && (TOWER32 || TOWER650)
       file    "tstset.c"
       global  tstset
       global  tstclr
       set     S%1,0
       set     T%1,0
       set     F%1,-4
       set     M%1,0x0000
tstset:
       tst.b   F%1-256(%sp)
       link    %fp,&-4
       tas     ([8.w,%fp])
       bne     L%73
       mov.l   &0,%d0
L%72:
       unlk    %fp
       rts
L%73:
       mov.l   &1,%d0
       br      L%72
       set     S%2,0
       set     T%2,0
       set     F%2,-4
       set     M%2,0x0000
tstclr:
       tst.b   F%2-256(%sp)
       link    %fp,&-4
       clr.b   ([8.w,%fp])
       unlk    %fp
       rts
#else
#if TOWERXP
        file    "tstset68000.c"
        global  tstset
        global  tstclr
        set     S%1,0
        set     T%1,0
        set     F%1,-4
        set     P%1,-4
        set     M%1,0x0000
tstset:
        tst.b   P%1-256(%sp)
        link    %fp,&-4
        mov.l   8(%fp),%a0
        tas     (%a0)
        bne.b   L%91
        mov.l   &0,%d0
L%90:
        unlk    %fp
        rts
L%91:
        mov.l   &1,%d0
        br.b    L%90
        set     S%2,0
        set     T%2,0
        set     F%2,-4
        set     P%2,-4
        set     M%2,0x0000
tstclr:
        tst.b   P%2-256(%sp)
        link    %fp,&-4
        mov.l   8(%fp),%a0
        clr.b   (%a0)
        unlk    %fp
        rts
#endif /* TOWERXP */
#endif /* TOWERXP && (TOWER32 || TOWER650 || TOWER700) */
#endif /* TOWERXP && TOWER800 */

#if XENIXAT
;	Static Name Aliases
;
	TITLE   tstset

	.287
TSTSET_TEXT	SEGMENT  BYTE PUBLIC 'CODE'
TSTSET_TEXT	ENDS
_DATA	SEGMENT  WORD PUBLIC 'DATA'
_DATA	ENDS
CONST	SEGMENT  WORD PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT  WORD PUBLIC 'BSS'
_BSS	ENDS
DGROUP	GROUP	CONST,	_BSS,	_DATA
	ASSUME  CS: TSTSET_TEXT, DS: DGROUP, SS: DGROUP, ES: DGROUP
EXTRN	__chkstk:FAR
TSTSET_TEXT      SEGMENT
	PUBLIC	_tstset
_tstset	PROC FAR
	push	bp
	mov	bp,sp
	xor	ax,ax
	call	FAR PTR __chkstk
	les	bx,[bp+6]	;ptr
	mov     ax, 1
	xchg    al, BYTE PTR es:[bx]
	or      ax, ax
	jnz	$I82
	sub	ax,ax
	jmp	SHORT $EX81
$I82:
	mov	ax,1
$EX81:
	mov	sp,bp
	pop	bp
	ret	

_tstset	ENDP
	PUBLIC	_tstclr
_tstclr	PROC FAR
	push	bp
	mov	bp,sp
	xor	ax,ax
	call	FAR PTR __chkstk
	les	bx,[bp+6]	;ptr
	mov     ax, 0
	xchg    al, BYTE PTR es:[bx]
	mov	sp,bp
	pop	bp
	ret	

_tstclr	ENDP
TSTSET_TEXT	ENDS
END
#endif /* end XENIXAT */

#if SEQUENT | SEQUENTPTX
#include <parallel/parallel.h>
int
tstset(pl)
	slock_t	*pl;
{

	if ((S_CLOCK(pl)) == L_FAILED) return 1;
	else return 0;
}
int
tstclr(pl)
        slock_t    *pl;
{
	S_UNLOCK (pl);
}
int
tstinit(pl)
        slock_t    *pl;
{
        S_INIT_LOCK (pl);
}
#endif /* end SEQUENT */

#if XSCO
;	Static Name Aliases
;
	TITLE	$tstset

	.386
DGROUP	GROUP	CONST, _BSS, _DATA
PUBLIC  _tstset
PUBLIC  _tstclr
_DATA	SEGMENT  DWORD USE32 PUBLIC 'DATA'
_DATA      ENDS
_BSS	SEGMENT  DWORD USE32 PUBLIC 'BSS'
_BSS      ENDS
CONST	SEGMENT  DWORD USE32 PUBLIC 'CONST'
CONST      ENDS
_TEXT	SEGMENT  DWORD USE32 PUBLIC 'CODE'
	ASSUME   CS: _TEXT, DS: DGROUP, SS: DGROUP, ES: DGROUP
; Line 4
_tstset	PROC NEAR
	push 	ebp
	mov  	ebp, esp
; Line 5
;	ptr = 8
; Line 6
	mov     edx, 1
	mov  	eax, DWORD PTR [ebp+8]
	xchg    dl, BYTE PTR [eax]
	or	edx, edx
	jne  	SHORT $I85
; Line 7
	xor  	eax, eax
	leave	
	ret  	
	nop  	
$I85:
; Line 9
	mov  	eax, 1
	leave	
	ret  	
	nop  	
_tstset	ENDP

; Line 13
_tstclr	PROC NEAR
	push 	ebp
	mov  	ebp, esp
; Line 14
;	ptr = 8
; Line 15
	mov  	eax, DWORD PTR [ebp+8]
	mov  	BYTE PTR [eax], 0
; Line 16
	leave	
	ret  	
	nop  	
_tstclr	ENDP

_TEXT	ENDS
END
#endif /* end SCO */

#if ARIX825 || ARIXS90
/* NOTE for S90 Systems ONLY: trap 2 will be invalidated.
   A future O/S will only support Compare & Set (cas) for
   68020 -->> 68040. A1000 (825) Systems will still use trap 2. 
   This code NEEDS to be split in two!!!! 
   The next port to S90 needs cas to be investigated!! 
   mjd 10/22/90
*/ 
	file	"tstset68000.c"
	data	1
	text
	global	tstset
tstset:
	link.l	%fp,&F%1
	movm.l	&M%1,(4,%sp)
	fmovm	&FPM%1,(FPO%1,%sp)
	mov.l	(8,%fp),%d0
	trap	&2
	bne	L%77
	mov.l	&0,%d0
	bra	L%76
L%77:
	mov.l	&1,%d0
	bra	L%76
L%76:
	fmovm	(FPO%1,%sp),&FPM%1
	movm.l	(4,%sp),&M%1
	unlk	%fp
	rts
	set	S%1,0
	set	T%1,0
	set	F%1,-4
	set	FPO%1,4
	set	FPM%1,0x0000
	set	M%1,0x0000
	data	1
	text
	global	tstclr
tstclr:
	link.l	%fp,&F%2
	movm.l	&M%2,(4,%sp)
	fmovm	&FPM%2,(FPO%2,%sp)
	mov.l	(8,%fp),%a1
	clr.b	(%a1)
L%79:
	fmovm	(FPO%2,%sp),&FPM%2
	movm.l	(4,%sp),&M%2
	unlk	%fp
	rts
	set	S%2,0
	set	T%2,0
	set	F%2,-4
	set	FPO%2,4
	set	FPM%2,0x0000
	set	M%2,0x0000
	data	1
#endif /* end ARIX */
#if OLIVETTI
	file	"tstset.c"


	text
	even
	global	tstset
tstset:
	mov.l	4(%sp),%a0

	tas.b	(%a0)
	bne.b	L%3
	mov.l	&0,%d0
	bra.b	L%1
L%3:
	mov.l	&1,%d0
L%1:

	rts
	data	1
	even

#4(%sp)	ptr
	text
	even
	global	tstclr
tstclr:
	mov.l	4(%sp),%a0

	clr.b	(%a0)

	rts
	data	1
	even

#4(%sp)	ptr
	text
	data	1
	even



	text
#endif /* OLIVETTI */
#if DG88K | DG5225
/*      Motorola 88000 RISC chip. Use xmem to swap reg. and memory */
/*      Use a general purpose register num 13 */
	file	"tstset.c"
; ccom  -O -X22 -X71 -X74 -X78 -X80 -X83 -X84 -X85 -X152 -X202 -X233 -X266
;	 -X301 -X325 -X326 -X332 -X334 -X335 -X350 -X357 -X358 -X361 -X383
;	 -X484 -X485
	text
	align	4
_tstset:
;	.bf:	;
	or	r13,r0,1
	xmem.bu r13,r2,r0
;	ld.bu	r9,r2,0
	bcnd	ne0,r13,@L4
	jmp.n	r1
	or	r2,r0,0
@L4:
;	.ef:	;
	jmp.n	r1
	or	r2,r0,1
	align	4
	data
@L1:
;_ptr	r2	local
	text
	align	4
_tstclr:
;	.bf:	;
;	.ef:	;
	jmp.n	r1
	st.b	r0,r2,0
	align	4
	data
@L8:
;_ptr	r2	local
	text
	data
;_trace	_trace	import
;__filesx	__filesx	import
	global	_tstclr
	global	_tstset
	text
#endif /* DG 88K */
#if SYS25ICL | ICLDRS
/*	Notice  similarity to Tower32 code */
file    "tstset.c"
        global  tstset
        global  tstclr
        set     S%1,0
        set     T%1,0
        set     F%1,-4
        set     M%1,0x0000
tstset:
        link.l  %fp,&-4
        tas     ([8.w,%fp])
        bne.b   L%77
        mov.l   &0,%d0
L%76:
        unlk    %fp
        rts
L%77:
        mov.l   &1,%d0
        bra.b   L%76
        set     S%2,0
        set     T%2,0
        set     F%2,-4
        set     M%2,0x0000
tstclr:
        link.l  %fp,&-4
        clr.b   ([8.w,%fp])
        unlk    %fp
        rts
#endif /* system 25 icl */
#if ALT386 | PS2AIX
	REMOVE USING SED - SEE CTSTSET FOR COMMENTS
	.file	"tstset.c"
	.version	"80386 Development System"
	.data
	.text
	.def	tstset;	.val	tstset;	.scl	2;	.type	044;	.endef
	.globl	tstset
tstset:
	pushl	%ebp
	movl	%esp,%ebp
	movl	8(%ebp),%eax
        movl    $1,%edx
	xchgl	(%eax),%edx
	testl	%edx,%edx
	jne	.L79
	xorl	%eax,%eax
.L76:
	leave	
	ret	
.L79:
	movl	$1,%eax
	jmp	.L76
	.def	tstset;	.val	.;	.scl	-1;	.endef
	.data
	.text
	.def	tstclr;	.val	tstclr;	.scl	2;	.type	044;	.endef
	.globl	tstclr
tstclr:
	pushl	%ebp
	movl	%esp,%ebp
	movl	8(%ebp),%eax
	movb	$0,(%eax)
	leave	
	ret	
	.def	tstclr;	.val	.;	.scl	-1;	.endef
	.data
	.text
#endif /* altos 386 */
/* 	
	For ATT3B2600 use a hook to detect the start of the assembler text
	and use sed to remove all lines from top to and including the hook
*/
#if ATT3B2600
	BEGIN ASSEMBLER
	.file	"tstset68000.c"
	.version	"02.01"
	.data
	.text
	.data
	.text
	.data
	.text
	.set	.F1,0
	.align	4
	.def	tstset;	.val	tstset;	.scl	2;	.type	044;	.endef
	.globl	tstset
tstset:
	SAVE	%fp
	MOVB    &1,%r0
        SWAPBI  *0(%ap)
        TSTB    %r0
	jne	.L77
	CLRW	%r0
	RESTORE	%fp
	RET	
.L77:
	MOVW	&1,%r0
	RESTORE	%fp
	RET	
	.def	tstset;	.val	.;	.scl	-1;	.endef
	.text
	.set	.F2,0
	.align	4
	.def	tstclr;	.val	tstclr;	.scl	2;	.type	044;	.endef
	.globl	tstclr
tstclr:
	SAVE	%fp
	CLRB	*0(%ap)
	RESTORE	%fp
	RET	
	.def	tstclr;	.val	.;	.scl	-1;	.endef
	.text
#endif /* ATT3B2600 */
#if DIAB
        .text
        .globl  _tstset
_tstset:
        link    a6,#-0
        movl    8(a6),a0
        tas     (a0)
        jne     .L2
        moveq   #0,d0
        jra     .L1
.L2:
        moveq   #1,d0
.L1:
	unlk    a6
        rts

| Allocations for _tstset
        .data
|       8(a6)   ptr

        .text
        .globl  _tstclr
_tstclr:
        link    a6,#-0
        movl    8(a6),a0
        clrb    (a0)
        unlk    a6
        rts

| Allocations for _tstclr
        .data
|       8(a6)   ptr

| Allocations for module
        .data
#endif  /* diab10, diab20       */
#if HARRIS
        .data
        .set    .R12,0x0
        .set    .R18,0x0
        .data
        .text
                .file   "tstset.c"
        .align  2
        .globl  _tstset
        .data
        .text
        _tstset:.word   .R12
        tstl    *4(fp)
        bbssi   $0,*4(fp),failed
        clrl    r0
        ret#1
        failed:movl     $1,r0
        ret#1
        .align  2
        .globl  _tstclr
        .data
	.text
        _tstclr:.word   .R18
        clrl    *4(fp)
        ret#2
#endif  /* harris */
#if ATT386 | CT386 | SCO
	.file	"tstset.c"
	.version	"02.01"
	.data
	.text
	.align	4
	.def	tstset;	.val	tstset;	.scl	2;	.type	044;	.endef
	.globl	tstset
tstset:
	pushl	%ebp
	movl	%esp,%ebp
	movl	8(%ebp),%eax
	movb	$1,%dl
	xchgb	(%eax),%dl
	testb	%dl,%dl
	jne	.L82
	xorl	%eax,%eax
.L79:
	leave	
	ret	
	.align	4
.L82:
	movl	$1,%eax
	leave	
	ret	
	.align	4
	.def	tstset;	.val	.;	.scl	-1;	.endef
	.data
	.text
	.align	4
	.def	tstclr;	.val	tstclr;	.scl	2;	.type	044;	.endef
	.globl	tstclr
tstclr:
	pushl	%ebp
	movl	%esp,%ebp
	movl	8(%ebp),%eax
	movb	$0,(%eax)
	leave	
	ret	
	.align	4
	.def	tstclr;	.val	.;	.scl	-1;	.endef
	.data
	.text
#endif	/* att386	*/

#if BULLQ700 
	.file	'tstset.c'
	section 15
	section 10
	xdef	_tstset
_tstset:
.L82:
	link	a6,#-0
	move.l	8(a6),a0
	bfset	(a0){7:1}
	bne	.L84
	moveq	#0,d0
	bra	.L81
.L84:
	moveq	#1,d0
.L81:
	unlk	a6
	rts
	section 15
	section 10
	xdef	_tstclr
_tstclr:
.L87:
	link	a6,#-0
	move.l	8(a6),a0
	clr.b	(a0)
	unlk	a6
	rts
	section 15
#endif /* BULLQ700 */
#if OLIVETTI
        file    "tstset.c"


        text
        even
        global  tstset
tstset:
        mov.l   4(%sp),%a0

        tas.b   (%a0)
        bne.b   L%3
        mov.l   &0,%d0
        bra.b   L%1
L%3:
        mov.l   &1,%d0
L%1:

        rts
        data    1
        even

#4(%sp) ptr
        text
        even
        global  tstclr
tstclr:
        mov.l   4(%sp),%a0

        clr.b   (%a0)

        rts
        data    1
        even

#4(%sp) ptr
        text
        data    1
        even



        text
#endif /* OLIVETTI */
#if BULLXPS
	file	"tstset.c"
	set	S%1,0
	set	T%1,0
	set	F%1,-8
	set	M%1,0x0000
	text
	global	tstset
tstset:	#  7 nodes, refc 0
	link	%fp,&F%1
	mov.l	8(%fp),%a0
	tas	(%a0)
	bne.b	L%82
	mov.l	&0,%d0
	br.b	L%81
L%82:	#  1 nodes, refc 1
	mov.l	&1,%d0
L%81:	#  2 nodes, refc 1
	unlk	%fp
	rts
	set	S%2,0
	set	T%2,0
	set	F%2,-8
	set	M%2,0x0000
	global	tstclr
tstclr:	#  7 nodes, refc 0
	link	%fp,&F%2
	mov.l	8(%fp),%a0
	mov.l	&0,%d1
	mov.b	%d1,(%a0)
	unlk	%fp
	rts
#endif	/* BULLXPS */
#if CTMIGHTY
	file	"tstset.c"
	global	tstset
	global	tstclr
	set	S%1,0
	set	T%1,0
	set	F%1,-4
	set	M%1,0x0000
tstset:
	link	%fp,&-4
	mov.l	8(%fp),%a0
	tas	(%a0)
	bne.b	L%77
	mov.l	&0,%d0
L%76:
	unlk	%fp
	rts
L%77:
	mov.l	&1,%d0
	br.b	L%76
	data
	fpused	0
	set	S%2,0
	set	T%2,0
	set	F%2,-4
	set	M%2,0x0000
	text
tstclr:
	link	%fp,&-4
	mov.l	8(%fp),%a0
	clr.b	(%a0)
	unlk	%fp
	rts
#endif /* CTMIGHTY */
#if MOTOROLA
	file	"tstset.c"
	global	tstset
	global	tstclr
	set	S%1,0
	set	T%1,0
	set	F%1,-4
	set	M%1,0x0000
tstset:
	link	%fp,&-4
	tas	([8.w,%fp])
	bne.b	L%82
	mov.l	&0,%d0
L%81:
	unlk	%fp
	rts
L%82:
	mov.l	&1,%d0
	bra.b	L%81
	set	S%2,0
	set	T%2,0
	set	F%2,-4
	set	M%2,0x0000
tstclr:
	link	%fp,&-4
	clr.b	([8.w,%fp])
	unlk	%fp
	rts
#endif	/* MOTOROLA */
#if SUN4
	.seg	"text"			! [internal]
	.proc	4
	.global	_tstset
_tstset:
	ldstub	[%o0],%o1		! atomic load/store unsigned byte
					! loads ubyte from [%o0] into %o1
					! stores 1s in [%o0] (atomically)
	tst	%o1			! tst if WE set [%o0] or if already set
	bne,a	L77004			! process delay slot if != 0 (br taken)
	mov	1,%o0			! rtc 1 - busy (on branch taken)
	mov	0,%o0			! rtc 0 - available (branch not taken)
L77004:
	retl				! return to caller
	nop				! [internal]
	.proc	4
	.global	_tstclr
_tstclr:
	stb	%g0,[%o0]		! clear lock unconditionally
	retl				! return to caller
	add	%g0,0,%o0		! rtc 0 (delay slot always executed)
	.seg	"data"			! [internal]
#endif /* SUN4 3-29-90 */
#if SUN45
        .seg    ".text"                 ! [internal]
        .proc   4
        .global tstset
tstset:
        ldstub  [%o0],%o1		! atomic load/store unsigned byte
					! loads ubyte from [%o0] into %o1
					! stores 1s in [%o0] (atomically)
        tst     %o1			! tst if WE set [%o0] or if already set
        bne,a   L77004			! process delay slot if != 0 (br taken)
        mov     1,%o0			! rtc 1 - busy (on branch taken)
        mov     0,%o0			! rtc 0 - available (branch not taken)
L77004:
        retl				! return to caller
        nop                             ! [internal]
        .proc   4
        .global tstclr
tstclr:
        stb     %g0,[%o0]		! clear lock unconditionally
        retl				! return to caller
        add     %g0,0,%o0		! rtc 0 (delay slot always executed)
        .seg    ".data"                 ! [internal]
#endif /* SUN45 1-10-92 */
#if TI1500
         XDEF       tstset,tstclr
         SECTION    0
tstset   LINK       A6,#0
         MOVE.L     8(A6),A0
         TAS        (A0)
         BNE.S      L10
         MOVEQ      #0,D0
         BRA.S      L12
L10      MOVEQ      #1,D0
L12      UNLK       A6
         RTS

tstclr   LINK       A6,#0
         MOVE.L     8(A6),A0
         CLR.L      (A0)
         UNLK       A6
         RTS

         END
#endif
#if DDE
        file    "tstset68000.c"

        text
        global  tstset
tstset:
        mov.l   (4,%a7),%a0
        tas     (%a0)
        bne     L%2
        mov.l   &0,%d0
        mov.l   %d0,%a0
        rts
L%2:
        mov.l   &1,%d0
        mov.l   %d0,%a0
L%1:
        rts

        text
        global  tstclr
tstclr:
        mov.l   (4,%a7),%a0
        clr.b   (%a0)
        rts
#endif  /* dde  */
#if BULLDPX2
        .file   'tstset68000.c'
* ccom  -O -X12 -X14 -X22 -X35 -X39 -X66 -X74 -X78 -X84 -X85 -X95 -X96 -X97
*        -X98 -X99 -X125 -X134 -X187 -X350 -X357 -X358 -X370 -X382 -X413 -X1032
*        -X1041 -X1046
        SECTION 10

_tstset:
        move.l  4(sp),a0


* line  6

        tas     (a0)
        bne     .L3

* line  7

        moveq   #0,d0
        bra     .L1
.L3:

* line  9

        moveq   #1,d0
.L1:

        rts

        SECTION 15

*       4(sp)   _ptr
        SECTION 10

_tstclr:
        move.l  4(sp),a0


* line  15

        clr.b   (a0)

* line  16


        rts

        SECTION 15

*       4(sp)   _ptr
        SECTION 10
        SECTION 15
*       import  _trace
*       import  __filesx
        XDEF    _tstclr
        XDEF    _tstset
        SECTION 10
        END
#endif /* BULLDPX2 */
#if ICL6000
        .section ".text"                        ! [marker node]
        .proc   4
        .global tstset
tstset:
        ldstub  [%o0],%o1		! atomic load/store unsigned byte
					! loads ubyte from [%o0] into %o1
					! stores 1s in [%o0] (atomically)
        tst     %o1			! tst if WE set [%o0] or if already set
        bne,a   .L77004			! process delay slot if != 0 (br taken)
        mov     1,%o0			! rtc 1 - busy (on branch taken)
        mov     0,%o0			! rtc 0 - available (branch not taken)
.L77004:
        retl				! return to caller
        nop                             ! [OPT_LEAF]
        .type   tstset,#function
        .size   tstset,(.-tstset)
        .proc   16
        .global tstclr
tstclr:
        stb     %g0,[%o0]		! clear lock unconditionally
        retl				! return to caller
        add     %g0,0,%o0		! rtc 0 (delay slot always executed)
        .type   tstclr,#function
        .size   tstclr,(.-tstclr)
        .file   "tstset68000.c"
        .version        "02.01"
        .ident  "acomp: (CDS) 5.0 (H6.1U) 01/03/90"
#endif /* ICL6000 5-11-90 */
#if NCR486 || ICL3000 || UNIX486V4 || SOLARISX86 || LINUXX86
	.file   "t.c"
	.version        "01.01"
        .ident  "@(#)head.sys:types.h   1.5.8.1"
	.ident  "@(#)head.sys:select.h  1.1.1.1"
	.ident  "@(#)head.sys:ipc.h     1.3.4.1"
	.ident  "@(#)head.sys:sem.h     1.3.5.1"
	.ident  "@(#)head.sys:shm.h     1.3.5.1"
	.type   tstset,@function
	.text
	.globl  tstset
	.align  4
tstset:
	movl    4(%esp),%eax
	movl    $1,%edx
	xchgl   %edx,(%eax)
	cmpl    $0,%edx
	jne     .L101
	xorl    %eax,%eax
.L102:
	ret
	.align  4
.L101:
	movl	$1,%eax
	ret
	.align  4
	.size   tstset,.-tstset
	.type   tstclr,@function
	.text
	.globl  tstclr
	.align  4
tstclr:
	movl    4(%esp),%eax
	movl    $0,%edx
	xchgl   %edx,(%eax)
       ret
	.align  4
	.size   tstclr,.-tstclr
	.ident  "acomp: (SCDE) 5.0 (i10) 02/02/90"
	.text
	.ident  "optim: (SCDE) 5.0 (i10) 02/02/90"
#endif /* ncr486 */
#if ROADRUNNER
	.file	"tstset68000.c"
	.version	"sun386-1.0"
	.optim
.LL0:
        .data
	.text
	.globl	tstset
tstset:
	pushl	%ebp
	movl	%esp,%ebp
	movl	8(%ebp),%eax
	movb	$1,%dl
	xchgb	(%eax),%dl
	testb	%dl,%dl
	jne	.L79
	xorl	%eax,%eax
.L76:
	leave	
	ret	
.L79:
	movl	$1,%eax
	jmp	.L76
/FUNCEND

        .data
	.text
	.globl	tstclr
tstclr:
	pushl	%ebp
	movl	%esp,%ebp
	movl	8(%ebp),%eax
	movb	$0,(%eax)
	leave	
	ret	
/FUNCEND

        .data
	.text
#endif /* ROAADRUNNER */
#if SIEMENS
;       National Semiconductor Corporation - GNX/CTP Version 3, Revision 4
;       -- tstset.s --   Thu Feb 14 11:58:33 1991
;

	.program
	.module tstset.c
	.globl  _tstset
	.align 4
_tstset:
	enter   [],0
	movd    8(fp),r0
	sbitib  0,0((r0))
	bfs     LL1
	movqd   0,r0
	exit    []
	ret     0
	.align 4
LL1:
	movqd   1,r0
	exit    []
	ret     0
	.globl  _tstclr
	.align 4
_tstclr:
	enter   [],0
	movd    8(fp),r0
	movqb   0,0((r0))
	exit    []
	ret     0
	.endseg
#endif /* SIEMENS */
#if IBMRIOS
        .file "tstset.s"
        .csect tstset[ds]
        .long  .tstset[PR]
        .long TOC[tc0] 
        .long 0
        .toc
T.tstset:
	.tc .tstset[tc], tstset[ds]
        .globl   .tstset[PR]
        .csect   .tstset[PR]
        .set   r0,0  
        .set   r2,2  
        .set   r3,3  
        .set   r4,4  
        .set   r5,5  
        .set   r12,12
         l     r12,T_csptr(r2)
         cal   r4,0(r0)      
         l     r12,0(r12)   
         l     r12,0(r12)  
         mtctr r12        
         cal   r5,1(r0)  
         bctr           
         .toc
T_csptr:
	 .tc csptr[tc], csptr[rw]
         .extern  csptr[rw]
         .file "tstclr.s"
         .csect tstclr[ds]
         .long  .tstclr[PR]
         .long TOC[tc0] 
         .long 0
         .toc
T.tstclr:
	 .tc  .tstclr[tc], tstclr[ds]
         .globl  .tstclr[PR]
         .csect   .tstclr[PR]
         l     r5,T_AIX_MP(r2) 
         cal   r4,0(r0)     
         cmpli 0,r5,1      
         beq   0,lab0     
         st    r4,0(r3)  
         blr            
lab0:
         ba    0x00003400 
                         
             .align 2
             .toc
T_AIX_MP:   
	     .tc AIX_MP[tc], AIX_MP[rw]
             .extern AIX_MP[rw] 
#endif

#if MIPS3000

#include <sys/types.h>
#include <mutex.h>

int
tstset(pLock)
int     *pLock;
{
    return (test_and_set(pLock,1));
}


int
tstclr(pLock)
int     *pLock;
{
    (test_and_set(pLock,0));
}

#endif /* MIPS3000 */
#if ALPHAOSF || WINNT_ALPHA

/*
 * extern int tstset(int *loc);
 *
 * Perform an atomic test-and-set of the longword pointed to by "loc".
 * This routine is written with the following considerations as
 * suggested by the Alpha SRM:
 *
 *      1. Only register-to-register operate instructions are used to
 *         construct the value to be stored.
 *
 *      2. Branches "fall-through" in the success case.
 *
 *      3. Both conditional branches are forward branches so they
 *         are properly predicted not to be taken to match the common
 *         case of no contention for the lock.
 */
	.ugen	
	.verstamp	3 11
	.text	
	.align	4
	.globl	tstset
	.ent	tstset 2
tstset:
	.option	O1
	ldgp	$gp, 0($27)
	lda	$sp, -96($sp)
	stq	$26, 0($sp)
	stq	$16, 48($sp)
	.mask	0x04000600, -96
	.frame	$sp, 96, $26, 48
	.prologue	1
TEST_FLAG:
 #   Test and set flag (or try to)
	.set	 volatile
	ldl_l	$0, 0($16)	# load flag locked
	.set	 novolatile
	bne	$0, WAIT	# WAIT if flag was set
	bis	$31, 1, $1	# set flag
	stl_c   $1, 0($16)	# try and store set flag
	beq	$1, RETEST_FLAG	# RETEST_FLAG if store failed
 # mb will ensure that no memory accesses are pre-fetched 
 # before the lock has been granted
	mb
WAIT:
 # return
	.livereg	0x007F0002,0x3FC00000
	ldq	$26, 0($sp)
	lda	$sp, 96($sp)
	ret	$31, ($26), 1
RETEST_FLAG:
 #  We wind up here if someone else set the flag, or an interrupt occurred
	br	TEST_FLAG	# now, branch backward to retry
	.end	tstset
	.text	
	.align	4
	.globl	tstclr
	.ent	tstclr 2
tstclr:
        .option O1
 # mb will flush memory updates before the lock is freed by stl
        mb
 # unconditionally store "zero" into lock
        stl     $31, 0($16)
 # no return value, so, $0 not set
        ret     $31, ($26), 1
        .end    tstclr
	.text	
	.align	4
	.globl	test_lock
	.ent	test_lock
test_lock:
 # return flag value and synch 
	ldl	$0,0($16)
	mb
        ret     $31, ($26), 1
        .end    test_lock
#endif /* ALPHAOSF || WINNT_ALPHA*/

#if WINNT_ALPHA
/*
   int rnrundll_axp(	custom procedure for rnrundll() 
	int (*lpfn)(),	poinmter to dll function to be called 
	int nargs,	number of arguments to pass 
	int nDynBytes,	size of dynargs array in bytes (mult of 16) 
	long *dynargs, 	alloca space for 7th thru nth args, if present 
	long *regargs,	space for args 1-6, always present, may be unused 
	double *pDret )	pointer used to set double return value 
*/
	.text
	.align	4
	.globl	rnrundll_axp
	.ent	rnrundll_axp
	.frame	$fp,64,$ra
	.set	noat

rnrundll_axp:

/* Instruction lines are kept in groups of four to make it easier
   to see how brach targets and such are kept aligned.

   The prolog always sets up the frame pointer, even if it doesn't
   extend the stack.
*/
	lda	$sp, -64($sp)
	stq	$ra, 0($sp)	/* save return address */
	stq	$fp, 8($sp)	/* save frame pointer */
	bis	$sp, $31, $fp	/* set frame ptr */
	.prologue 0

/* save the args to rnrundll_axp in the fixed portion of the stack
   where we can get at them should we ever need to debug this stuff
   in the field.  This also frees up the argument registers so we
   can start stuffing lpfn arguments into them.
*/
	bis	$a0, $31, $t0	/* lpfn */
	bis	$a1, $31, $t1	/* n args */
	bis	$a2, $31, $t2	/* nDynBytes */
	bis	$a3, $31, $t3	/* dynargs */

	bis	$a4, $31, $t4	/* regargs */
	stq	$t0, 16($sp)	/* save the rnrundll_axp args on the stack  */
	stq	$t1, 24($sp)	/*   so we can "debug" lpfn calls */
	stq	$t2, 32($sp)	

	stq	$t3, 40($sp)	
	stq	$t4, 48($sp)	
	stq	$a5, 56($sp)	
	beq	$t1, call	/* no lpfn args to pass? go make the call */

/* Since we don't know whether we are passing integer or floating point
   args, load all twelve argument registers, a0-a5 and f16-f21 from 
   regargs array.
*/
	ldl	$a0, 0($t4)	/* load all arg regs for simplicity */ 
	ldt	$f16, 0($t4)	
	ldl	$a1, 8($t4)
	ldt	$f17, 8($t4)

	ldl	$a2, 16($t4)
	ldt	$f18, 16($t4)
	ldl	$a3, 24($t4)
	ldt	$f19, 24($t4)

	ldl	$a4, 32($t4)
	ldt	$f20, 32($t4)
	ldl	$a5, 40($t4)
	ldt	$f21, 40($t4)

/* dynamically passed argument must be passed on the stack beginning
   at $sp of the caller (that's rnrundll_axp(), since we are the 
   surrogate caller), so we have to extend the stack and copy the
   dynargs array to the beginning of the stack.  We have to do this
   even thought the dynargs array was allocated using alloca().

   The dynargs array was allocated (in rnrundlll) as a multiple of 16 bytes
   to maintain the required "octoword" alignment of the stack pointer.

   The copy instructions below execute better if the loads and stores are
   in separate groups of four instructions.  The reg which  points to the
   target is decremented before it is used, requiring the stores to use
   negative displacements.
*/
	beq	$t2, call	/* args don't spill onto stack? go call lpfn */
	bis	$31, $31, $31	/* noop to align "copy:" branch target */
	subq	$sp, $t2, $sp	/* move stack pointer to the left */
	subq	$fp, $t2, $t4	/* use same address as dest (t4 is free) */

copy:	ldq	$t6, 0($t3)	/* load source octaword */
	ldq	$t7, 8($t3)
	lda	$t3, 16($t3)
	lda	$t4, 16($t4)	/* dest stored using negative disp */

	subq	$t4, $fp, $t5	/* stop at the old stack pointer */
	stq	$t6, -16($t4)	/* store dest octaword */
	stq	$t7, -8($t4)
	bne	$t5, copy

/* although not technically required, we use r28 as the jsr target
   instead of r27 so that rnrundll_axp looks like a transfer vector
   to a dll function, in case anyone who cares is watching.
*/
call:	bis	$t0, $31, $at	/* set transfer value */
	jsr	$ra, ($at), 1	/* call *lpfn */
	bis	$31, $31, $31   /* these pad the routine to a multiple */
	bis	$31, $31, $31	/*    of four. */

/* $r0 already contains the integer return value, if there was one,
   so we just need to store the floating point return value into
   Dret, get the return address, and go back to rnrundll().
*/
	bis	$fp, $31, $sp	/* reset stack ptr from frame ptr */
	ldq	$t0, 56($fp)	/* get &Dret */
	ldq	$ra, 0($sp)	/* restore return address */
	stt	$f0, 0($t0)	/* set Dret */

/* KEEP THE NEXT 3 INSTRUCTIONS IN THIS ORDER */
	ldq	$fp, 8($sp)	/* restore frame pointer */
	lda	$sp, 16($sp)	/* restore stack pointer */
	ret	$31, ($ra), 1	/* return to rnproc */
	bis	$31, $31, $31
	.end

#endif
