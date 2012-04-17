	.file	"cachekill-readtest.c"
	.text
	.p2align 4,,15
.globl handler
	.type	handler, @function
handler:
.LFB23:
	.cfi_startproc
	movl	$1, doquit(%rip)
	ret
	.cfi_endproc
.LFE23:
	.size	handler, .-handler
	.section	.rodata.str1.8,"aMS",@progbits,1
	.align 8
.LC0:
	.string	"usage: cachekill-readtest [data file]\n"
	.align 8
.LC1:
	.string	"\tto generate an appropriate data file:\t\tdd if=dev/urandom of=DATAFILE bs=1048576 count=MEGABYTES\n"
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC2:
	.string	"error: unable to open %s\n"
.LC3:
	.string	"Opened file %s.\n"
	.section	.rodata.str1.8
	.align 8
.LC4:
	.string	"error: unable to stat data file\n"
	.section	.rodata.str1.1
.LC5:
	.string	"Statted file, size: %llu\n"
	.section	.rodata.str1.8
	.align 8
.LC6:
	.string	"error: file too small for block size %d\n"
	.section	.rodata.str1.1
.LC7:
	.string	"Beginning reads..\n"
.LC8:
	.string	"fatal error: seek error\n"
.LC9:
	.string	"fatal error: read error\n"
.LC11:
	.string	"%llu\t%llu\t%lf\t%lf\n"
.LC12:
	.string	"Interrupted, quitting.\n"
	.text
	.p2align 4,,15
.globl main
	.type	main, @function
main:
.LFB24:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
	movq	%rsi, %r12
	.cfi_offset 12, -48
	.cfi_offset 13, -40
	.cfi_offset 14, -32
	.cfi_offset 15, -24
	pushq	%rbx
	subq	$200, %rsp
	cmpl	$1, %edi
	jle	.L24
	.cfi_offset 3, -56
	movq	8(%r12), %rdi
	xorl	%esi, %esi
	xorl	%eax, %eax
	call	open
	cmpl	$-1, %eax
	movl	%eax, %ebx
	je	.L25
	movq	8(%r12), %rdx
	movq	stderr(%rip), %rdi
	movl	$.LC3, %esi
	xorl	%eax, %eax
	call	fprintf
	leaq	-240(%rbp), %rdx
	movl	%ebx, %esi
	movl	$1, %edi
	call	__fxstat
	testl	%eax, %eax
	jne	.L26
	movq	-192(%rbp), %rdx
	movq	stderr(%rip), %rdi
	xorl	%eax, %eax
	movl	$.LC5, %esi
	call	fprintf
	movq	-192(%rbp), %r15
	subq	$4096, %r15
	js	.L27
	movl	$handler, %esi
	movl	$2, %edi
	call	signal
	xorl	%edi, %edi
	xorl	%eax, %eax
	call	time
	movl	%eax, %edi
	xorl	%eax, %eax
	call	srand
	leaq	-80(%rbp), %rdi
	xorl	%esi, %esi
	xorl	%eax, %eax
	call	gettimeofday
	leaq	-64(%rbp), %rdi
	xorl	%esi, %esi
	xorl	%eax, %eax
	call	gettimeofday
	movq	stderr(%rip), %rcx
	movl	$.LC7, %edi
	movl	$18, %edx
	movl	$1, %esi
	call	fwrite
	movl	doquit(%rip), %edi
	testl	%edi, %edi
	jne	.L9
	addq	$1, %r15
	xorl	%r13d, %r13d
	jmp	.L20
	.p2align 4,,10
	.p2align 3
.L13:
	movl	doquit(%rip), %eax
	movq	%r12, %rsp
	testl	%eax, %eax
	jne	.L9
.L20:
	xorl	%eax, %eax
	movq	%rsp, %r12
	call	rand
	cltq
	subq	$4112, %rsp
	movl	%ebx, %edi
	movq	%rax, %rdx
	leaq	15(%rsp), %r14
	sarq	$63, %rdx
	idivq	%r15
	andq	$-16, %r14
	movq	%rdx, %rsi
	xorl	%edx, %edx
	call	lseek
	cmpq	$-1, %rax
	je	.L28
	movl	$4096, %edx
	movq	%r14, %rsi
	movl	%ebx, %edi
	call	read
	cmpq	$-1, %rax
	je	.L29
	addq	$1, %r13
	movabsq	$3777893186295716171, %rax
	mulq	%r13
	shrq	$11, %rdx
	imulq	$10000, %rdx, %rdx
	cmpq	%rdx, %r13
	jne	.L13
	leaq	-96(%rbp), %rdi
	xorl	%esi, %esi
	xorl	%eax, %eax
	call	gettimeofday
	movq	-96(%rbp), %rsi
	movq	-64(%rbp), %rcx
	imulq	$1000000, %rsi, %rsi
	imulq	$-1000000, %rcx, %rax
	addq	-88(%rbp), %rsi
	subq	-56(%rbp), %rax
	addq	%rsi, %rax
	js	.L14
	cvtsi2sdq	%rax, %xmm1
.L15:
	testq	%r13, %r13
	js	.L16
	cvtsi2sdq	%r13, %xmm0
.L17:
	movq	-80(%rbp), %rdx
	divsd	%xmm0, %xmm1
	imulq	$-1000000, %rdx, %rax
	subq	-72(%rbp), %rax
	addq	%rsi, %rax
	js	.L18
	cvtsi2sdq	%rax, %xmm0
.L19:
	divsd	.LC10(%rip), %xmm0
	movq	%r13, %rdx
	movl	$.LC11, %edi
	movl	$2, %eax
	call	printf
	movq	stdout(%rip), %rdi
	call	fflush
	leaq	-80(%rbp), %rdi
	xorl	%esi, %esi
	xorl	%eax, %eax
	call	gettimeofday
	movl	doquit(%rip), %eax
	movq	%r12, %rsp
	testl	%eax, %eax
	je	.L20
	.p2align 4,,10
	.p2align 3
.L9:
	movq	stderr(%rip), %rcx
	movl	$23, %edx
	movl	$1, %esi
	movl	$.LC12, %edi
	call	fwrite
	movl	%ebx, %edi
	call	close
	jmp	.L3
	.p2align 4,,10
	.p2align 3
.L26:
	movl	%ebx, %edi
	call	close
	movq	stderr(%rip), %rcx
	movl	$32, %edx
	movl	$1, %esi
	movl	$.LC4, %edi
	call	fwrite
.L3:
	leaq	-40(%rbp), %rsp
	movl	$1, %eax
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	leave
	.cfi_remember_state
	.cfi_def_cfa 7, 8
	ret
	.p2align 4,,10
	.p2align 3
.L18:
	.cfi_restore_state
	movq	%rax, %rdx
	andl	$1, %eax
	shrq	%rdx
	orq	%rax, %rdx
	cvtsi2sdq	%rdx, %xmm0
	addsd	%xmm0, %xmm0
	jmp	.L19
	.p2align 4,,10
	.p2align 3
.L16:
	movq	%r13, %rax
	movq	%r13, %rdx
	shrq	%rax
	andl	$1, %edx
	orq	%rdx, %rax
	cvtsi2sdq	%rax, %xmm0
	addsd	%xmm0, %xmm0
	jmp	.L17
	.p2align 4,,10
	.p2align 3
.L14:
	movq	%rax, %rdx
	andl	$1, %eax
	shrq	%rdx
	orq	%rax, %rdx
	cvtsi2sdq	%rdx, %xmm1
	addsd	%xmm1, %xmm1
	jmp	.L15
	.p2align 4,,10
	.p2align 3
.L24:
	movq	stderr(%rip), %rcx
	movl	$38, %edx
	movl	$1, %esi
	movl	$.LC0, %edi
	call	fwrite
	movq	stderr(%rip), %rcx
	movl	$97, %edx
	movl	$1, %esi
	movl	$.LC1, %edi
	call	fwrite
	jmp	.L3
	.p2align 4,,10
	.p2align 3
.L25:
	movq	8(%r12), %rdx
	movq	stderr(%rip), %rdi
	movl	$.LC2, %esi
	xorl	%eax, %eax
	call	fprintf
	jmp	.L3
	.p2align 4,,10
	.p2align 3
.L28:
	movq	stderr(%rip), %rcx
	movl	$24, %edx
	movl	$1, %esi
	movl	$.LC8, %edi
	call	fwrite
.L11:
	movq	%r12, %rsp
	jmp	.L9
	.p2align 4,,10
	.p2align 3
.L29:
	movq	stderr(%rip), %rcx
	movl	$24, %edx
	movl	$1, %esi
	movl	$.LC9, %edi
	call	fwrite
	jmp	.L11
	.p2align 4,,10
	.p2align 3
.L27:
	movl	%ebx, %edi
	call	close
	movq	stderr(%rip), %rdi
	movl	$4096, %edx
	movl	$.LC6, %esi
	xorl	%eax, %eax
	call	fprintf
	jmp	.L3
	.cfi_endproc
.LFE24:
	.size	main, .-main
	.local	doquit
	.comm	doquit,4,4
	.section	.rodata.cst8,"aM",@progbits,8
	.align 8
.LC10:
	.long	0
	.long	1086556160
	.ident	"GCC: (GNU) 4.4.5 20110214 (Red Hat 4.4.5-6)"
	.section	.note.GNU-stack,"",@progbits
