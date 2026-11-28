	.file	"mca_kernels.cc"
	.text
	.section	.text.unlikely,"ax",@progbits
	.type	_ZN4dselL13PopcountDigitEDv2_xi, @function
_ZN4dselL13PopcountDigitEDv2_xi:
.LFB6454:
	.cfi_startproc
	movslq	%edi, %rdi
	leaq	_ZN4dselL10kValueMaskE(%rip), %rax
	salq	$4, %rdi
	andps	(%rax,%rdi), %xmm0
	movq	%xmm0, %rdx
	psrldq	$8, %xmm0
	movq	%xmm0, %rax
	popcntq	%rdx, %rdx
	popcntq	%rax, %rax
	addl	%edx, %eax
	ret
	.cfi_endproc
.LFE6454:
	.size	_ZN4dselL13PopcountDigitEDv2_xi, .-_ZN4dselL13PopcountDigitEDv2_xi
	.type	_ZN4dselL16SelAColdFallbackEDv2_x, @function
_ZN4dselL16SelAColdFallbackEDv2_x:
.LFB6456:
	.cfi_startproc
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset 3, -16
	movaps	%xmm0, %xmm2
	movl	$7, %edi
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$6, %edi
	movaps	%xmm2, %xmm0
	movl	%eax, %r9d
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$5, %edi
	movaps	%xmm2, %xmm0
	movl	%eax, %esi
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$4, %edi
	movaps	%xmm2, %xmm0
	movd	%esi, %xmm3
	movl	%eax, %r10d
	pinsrw	$1, %r9d, %xmm3
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$3, %edi
	movaps	%xmm2, %xmm0
	movl	%eax, %ecx
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$2, %edi
	movaps	%xmm2, %xmm0
	movl	%eax, %r11d
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$1, %edi
	movaps	%xmm2, %xmm0
	movl	%eax, %r8d
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movaps	%xmm2, %xmm0
	xorl	%edi, %edi
	movd	%r8d, %xmm4
	movl	%eax, %ebx
	pinsrw	$1, %r11d, %xmm4
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movd	%ecx, %xmm0
	movd	%eax, %xmm1
	pinsrw	$1, %r10d, %xmm0
	pinsrw	$1, %ebx, %xmm1
	punpckldq	%xmm3, %xmm0
	punpckldq	%xmm4, %xmm1
	punpcklqdq	%xmm0, %xmm1
	paddw	.LC0(%rip), %xmm1
	phminposuw	%xmm1, %xmm1
	movd	%xmm1, %eax
	movl	%eax, %esi
	shrl	$16, %esi
	andl	$65535, %eax
	je	.L3
	movl	$8, %edi
	movaps	%xmm2, %xmm0
	movl	%eax, %ecx
	call	_ZN4dselL13PopcountDigitEDv2_xi
	cmpl	$1, %eax
	jbe	.L3
	addl	$2, %ecx
	cmpl	%ecx, %eax
	movl	$8, %eax
	cmovb	%rax, %rsi
.L3:
	salq	$4, %rsi
	leaq	_ZN4dselL10kValueMaskE(%rip), %rax
	popq	%rbx
	.cfi_def_cfa_offset 8
	movaps	(%rax,%rsi), %xmm0
	ret
	.cfi_endproc
.LFE6456:
	.size	_ZN4dselL16SelAColdFallbackEDv2_x, .-_ZN4dselL16SelAColdFallbackEDv2_x
	.text
	.p2align 4
	.globl	k_A
	.type	k_A, @function
k_A:
.LFB6470:
	.cfi_startproc
	endbr64
	movdqa	%xmm0, %xmm1
	pand	.LC1(%rip), %xmm0
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	movq	%xmm0, %rax
	pextrq	$1, %xmm0, %rdi
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	movdqa	.LC2(%rip), %xmm0
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	popcntq	%rax, %rax
	popcntq	%rdi, %rdi
	pand	%xmm1, %xmm0
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	movq	%xmm0, %r11
	pextrq	$1, %xmm0, %rcx
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	movdqa	.LC3(%rip), %xmm0
	popcntq	%r11, %r11
	popcntq	%rcx, %rcx
	addl	%r11d, %ecx
	pand	%xmm1, %xmm0
	movd	%ecx, %xmm3
	movq	%xmm0, %rbx
	pextrq	$1, %xmm0, %r8
	movq	%rax, -16(%rsp)
	movdqa	.LC4(%rip), %xmm0
	popcntq	%rbx, %rbx
	popcntq	%r8, %r8
	addl	%ebx, %r8d
	movl	-16(%rsp), %ebx
	pand	%xmm1, %xmm0
	movq	%xmm0, %rbp
	pextrq	$1, %xmm0, %rdx
	addl	%ebx, %edi
	movdqa	.LC5(%rip), %xmm0
	popcntq	%rbp, %rbp
	popcntq	%rdx, %rdx
	addl	%ebp, %edx
	pinsrw	$1, %edi, %xmm3
	pand	%xmm1, %xmm0
	movd	%edx, %xmm2
	movq	%xmm0, %r12
	pextrq	$1, %xmm0, %r9
	movdqa	.LC6(%rip), %xmm0
	pinsrw	$1, %r8d, %xmm2
	popcntq	%r12, %r12
	popcntq	%r9, %r9
	addl	%r12d, %r9d
	punpckldq	%xmm3, %xmm2
	pand	%xmm1, %xmm0
	movq	%xmm0, %r13
	pextrq	$1, %xmm0, %rsi
	movdqa	.LC7(%rip), %xmm0
	popcntq	%r13, %r13
	popcntq	%rsi, %rsi
	addl	%r13d, %esi
	pand	%xmm1, %xmm0
	movd	%esi, %xmm4
	movq	%xmm0, %r14
	pextrq	$1, %xmm0, %r10
	movdqa	.LC8(%rip), %xmm0
	pinsrw	$1, %r9d, %xmm4
	popcntq	%r14, %r14
	popcntq	%r10, %r10
	addl	%r14d, %r10d
	pand	%xmm1, %xmm0
	movq	%xmm0, %r15
	pextrq	$1, %xmm0, %rax
	popcntq	%r15, %r15
	popcntq	%rax, %rax
	addl	%r15d, %eax
	movd	%eax, %xmm0
	pinsrw	$1, %r10d, %xmm0
	punpckldq	%xmm4, %xmm0
	punpcklqdq	%xmm2, %xmm0
	paddw	.LC0(%rip), %xmm0
	phminposuw	%xmm0, %xmm0
	movd	%xmm0, %eax
	movl	%eax, %ecx
	shrl	$16, %ecx
	andl	$65535, %eax
	jne	.L11
.L13:
	movl	%ecx, %eax
	salq	$4, %rax
.L12:
	leaq	_ZN4dselL10kValueMaskE(%rip), %rdx
	popq	%rbx
	.cfi_remember_state
	.cfi_def_cfa_offset 48
	popq	%rbp
	.cfi_def_cfa_offset 40
	movdqa	(%rdx,%rax), %xmm0
	popq	%r12
	.cfi_def_cfa_offset 32
	popq	%r13
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	ret
	.p2align 4,,10
	.p2align 3
.L11:
	.cfi_restore_state
	pand	.LC9(%rip), %xmm1
	movq	%xmm1, %rsi
	pextrq	$1, %xmm1, %rdx
	popcntq	%rsi, %rsi
	popcntq	%rdx, %rdx
	addl	%esi, %edx
	cmpl	$1, %edx
	jle	.L13
	addl	$2, %eax
	cmpl	%eax, %edx
	jnb	.L13
	movl	$128, %eax
	jmp	.L12
	.cfi_endproc
.LFE6470:
	.size	k_A, .-k_A
	.p2align 4
	.globl	k_B
	.type	k_B, @function
k_B:
.LFB6471:
	.cfi_startproc
	endbr64
	movdqa	.LC10(%rip), %xmm1
	movdqa	%xmm0, %xmm2
	movdqa	%xmm0, %xmm4
	pshufb	%xmm1, %xmm2
	pand	%xmm2, %xmm4
	por	%xmm2, %xmm0
	pshufb	%xmm1, %xmm2
	movdqa	%xmm4, %xmm6
	movdqa	%xmm0, %xmm3
	por	%xmm2, %xmm0
	pand	%xmm2, %xmm6
	pand	%xmm2, %xmm3
	movdqa	%xmm0, %xmm5
	pshufb	%xmm1, %xmm2
	por	%xmm4, %xmm3
	pand	%xmm2, %xmm5
	por	%xmm2, %xmm0
	movdqa	%xmm3, %xmm7
	por	%xmm3, %xmm5
	movdqa	%xmm2, %xmm3
	movdqa	%xmm0, %xmm4
	pshufb	%xmm1, %xmm3
	movdqa	%xmm5, %xmm8
	pand	%xmm2, %xmm7
	pand	%xmm3, %xmm4
	movdqa	%xmm3, %xmm9
	por	%xmm6, %xmm7
	por	%xmm5, %xmm4
	pshufb	%xmm1, %xmm9
	pand	%xmm3, %xmm8
	movdqa	%xmm0, %xmm1
	movdqa	%xmm4, %xmm5
	por	%xmm7, %xmm8
	pand	%xmm9, %xmm5
	por	%xmm3, %xmm1
	por	%xmm8, %xmm5
	pand	%xmm9, %xmm1
	por	%xmm4, %xmm1
	movdqa	%xmm5, %xmm0
	pandn	%xmm1, %xmm0
	ptest	%xmm0, %xmm0
	je	.L16
.L19:
	pcmpeqd	%xmm2, %xmm2
	movdqa	%xmm0, %xmm1
	psignw	%xmm2, %xmm1
	pand	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L16:
	pand	%xmm2, %xmm6
	pand	%xmm3, %xmm7
	pand	%xmm9, %xmm8
	por	%xmm7, %xmm6
	por	%xmm6, %xmm8
	movdqa	%xmm8, %xmm0
	pandn	%xmm5, %xmm0
	ptest	%xmm0, %xmm0
	jne	.L19
	pcmpeqd	%xmm1, %xmm1
	movdqa	%xmm8, %xmm0
	psignw	%xmm1, %xmm0
	pand	%xmm8, %xmm0
	ret
	.cfi_endproc
.LFE6471:
	.size	k_B, .-k_B
	.p2align 4
	.globl	k_C
	.type	k_C, @function
k_C:
.LFB6472:
	.cfi_startproc
	endbr64
	movdqa	.LC10(%rip), %xmm1
	movdqa	%xmm0, %xmm2
	movdqa	%xmm0, %xmm4
	pshufb	%xmm1, %xmm2
	pand	%xmm2, %xmm4
	por	%xmm2, %xmm0
	pshufb	%xmm1, %xmm2
	movdqa	%xmm4, %xmm6
	movdqa	%xmm0, %xmm3
	por	%xmm2, %xmm0
	pand	%xmm2, %xmm6
	pand	%xmm2, %xmm3
	movdqa	%xmm0, %xmm5
	pshufb	%xmm1, %xmm2
	por	%xmm4, %xmm3
	pand	%xmm2, %xmm5
	por	%xmm2, %xmm0
	movdqa	%xmm3, %xmm7
	por	%xmm3, %xmm5
	movdqa	%xmm2, %xmm3
	movdqa	%xmm0, %xmm4
	pshufb	%xmm1, %xmm3
	movdqa	%xmm5, %xmm8
	pand	%xmm2, %xmm7
	pand	%xmm3, %xmm4
	movdqa	%xmm3, %xmm9
	por	%xmm6, %xmm7
	por	%xmm5, %xmm4
	pshufb	%xmm1, %xmm9
	pand	%xmm3, %xmm8
	movdqa	%xmm0, %xmm1
	movdqa	%xmm4, %xmm5
	por	%xmm7, %xmm8
	pand	%xmm9, %xmm5
	por	%xmm3, %xmm1
	por	%xmm8, %xmm5
	pand	%xmm9, %xmm1
	por	%xmm4, %xmm1
	movdqa	%xmm5, %xmm0
	pandn	%xmm1, %xmm0
	ptest	%xmm0, %xmm0
	je	.L21
.L27:
	pcmpeqd	%xmm2, %xmm2
	movdqa	%xmm0, %xmm1
	psignw	%xmm2, %xmm1
	pand	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L21:
	pand	%xmm2, %xmm6
	pand	%xmm3, %xmm7
	pand	%xmm9, %xmm8
	por	%xmm6, %xmm7
	por	%xmm7, %xmm8
	movdqa	%xmm8, %xmm0
	pandn	%xmm5, %xmm0
	ptest	%xmm0, %xmm0
	jne	.L27
	pand	%xmm3, %xmm6
	pand	%xmm9, %xmm7
	por	%xmm6, %xmm7
	movdqa	%xmm7, %xmm0
	pandn	%xmm8, %xmm0
	ptest	%xmm0, %xmm0
	jne	.L27
	pand	%xmm9, %xmm6
	movdqa	%xmm6, %xmm0
	pandn	%xmm7, %xmm0
	ptest	%xmm0, %xmm0
	jne	.L27
	pcmpeqd	%xmm1, %xmm1
	movdqa	%xmm6, %xmm0
	psignw	%xmm1, %xmm0
	pand	%xmm6, %xmm0
	ret
	.cfi_endproc
.LFE6472:
	.size	k_C, .-k_C
	.p2align 4
	.globl	k_D1
	.type	k_D1, @function
k_D1:
.LFB6473:
	.cfi_startproc
	endbr64
	movdqa	.LC10(%rip), %xmm1
	movdqa	%xmm0, %xmm3
	movdqa	%xmm0, %xmm2
	pshufb	%xmm1, %xmm3
	movdqa	%xmm3, %xmm4
	pxor	%xmm3, %xmm2
	pand	%xmm3, %xmm0
	pshufb	%xmm1, %xmm4
	movdqa	%xmm2, %xmm1
	pand	%xmm4, %xmm2
	pxor	%xmm4, %xmm1
	movdqa	.LC11(%rip), %xmm4
	por	%xmm2, %xmm0
	movdqa	%xmm1, %xmm3
	movdqa	%xmm0, %xmm2
	pshufb	%xmm4, %xmm3
	pshufb	%xmm4, %xmm2
	movdqa	%xmm1, %xmm4
	pand	%xmm3, %xmm1
	pxor	%xmm3, %xmm4
	movdqa	%xmm0, %xmm3
	movdqa	%xmm1, %xmm5
	pxor	%xmm2, %xmm3
	pand	%xmm2, %xmm0
	pand	%xmm3, %xmm1
	pxor	%xmm3, %xmm5
	por	%xmm0, %xmm1
	movdqa	%xmm4, %xmm0
	por	%xmm1, %xmm0
	pandn	%xmm5, %xmm0
	ptest	%xmm0, %xmm0
	je	.L29
.L35:
	pcmpeqd	%xmm2, %xmm2
	movdqa	%xmm0, %xmm1
	psignw	%xmm2, %xmm1
	pand	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L29:
	movdqa	%xmm4, %xmm2
	movdqa	%xmm1, %xmm0
	pand	%xmm5, %xmm2
	pandn	%xmm2, %xmm0
	ptest	%xmm0, %xmm0
	jne	.L35
	movdqa	%xmm4, %xmm0
	por	%xmm5, %xmm0
	pandn	%xmm1, %xmm0
	ptest	%xmm0, %xmm0
	jne	.L35
	pand	%xmm1, %xmm4
	movdqa	%xmm5, %xmm0
	pandn	%xmm4, %xmm0
	ptest	%xmm0, %xmm0
	jne	.L35
	pand	%xmm1, %xmm5
	pcmpeqd	%xmm1, %xmm1
	movdqa	%xmm5, %xmm0
	psignw	%xmm1, %xmm0
	pand	%xmm5, %xmm0
	ret
	.cfi_endproc
.LFE6473:
	.size	k_D1, .-k_D1
	.p2align 4
	.globl	k_D2
	.type	k_D2, @function
k_D2:
.LFB6474:
	.cfi_startproc
	endbr64
	movdqa	%xmm0, %xmm3
	movdqa	%xmm0, %xmm5
	movdqa	%xmm0, %xmm2
	psrlq	$16, %xmm3
	psrlq	$32, %xmm5
	movdqa	%xmm0, %xmm4
	pxor	%xmm3, %xmm2
	punpckhqdq	%xmm0, %xmm4
	pand	%xmm0, %xmm3
	movdqa	%xmm2, %xmm1
	psrlq	$48, %xmm0
	pand	%xmm5, %xmm2
	por	%xmm3, %xmm2
	movdqa	%xmm4, %xmm6
	movdqa	%xmm0, %xmm3
	pxor	%xmm4, %xmm3
	psrlq	$16, %xmm6
	pxor	%xmm5, %xmm1
	pand	%xmm4, %xmm0
	movdqa	%xmm3, %xmm5
	pand	%xmm6, %xmm3
	pxor	%xmm6, %xmm5
	por	%xmm3, %xmm0
	movdqa	%xmm1, %xmm3
	pand	%xmm5, %xmm3
	movdqa	%xmm2, %xmm4
	pxor	%xmm5, %xmm1
	pxor	%xmm0, %xmm4
	movdqa	%xmm3, %xmm5
	pand	%xmm0, %xmm2
	pxor	%xmm4, %xmm5
	pand	%xmm4, %xmm3
	punpcklwd	%xmm5, %xmm1
	por	%xmm2, %xmm3
	punpckldq	%xmm3, %xmm1
	movq	%xmm1, %rdx
	movq	%rdx, %rsi
	movq	%rdx, %rdi
	shrq	$32, %rsi
	shrq	$16, %rdi
	movl	%esi, %eax
	movl	%edi, %ecx
	orl	%edx, %eax
	andl	$511, %ecx
	andl	$511, %eax
	notl	%eax
	andl	%ecx, %eax
	je	.L38
.L37:
	rep bsfl	%eax, %eax
	leaq	_ZN4dselL10kValueMaskE(%rip), %rdx
	salq	$4, %rax
	movdqa	(%rdx,%rax), %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L38:
	andl	$511, %esi
	movl	%ecx, %eax
	movl	%edx, %r9d
	movl	%esi, %r8d
	andl	%edx, %eax
	andl	$511, %r9d
	notl	%r8d
	andl	%r8d, %eax
	jne	.L37
	orl	%edx, %edi
	andl	$511, %edi
	notl	%edi
	movl	%edi, %eax
	andl	%esi, %eax
	jne	.L37
	movl	%ecx, %eax
	andl	%esi, %r9d
	andl	%esi, %ecx
	notl	%eax
	andl	%r9d, %eax
	cmove	%ecx, %eax
	jmp	.L37
	.cfi_endproc
.LFE6474:
	.size	k_D2, .-k_D2
	.section	.text.unlikely
.LCOLDB12:
	.text
.LHOTB12:
	.p2align 4
	.globl	k_E1
	.type	k_E1, @function
k_E1:
.LFB6475:
	.cfi_startproc
	endbr64
	movdqa	.LC10(%rip), %xmm5
	movdqa	%xmm0, %xmm2
	movdqa	%xmm0, %xmm3
	movdqa	%xmm0, %xmm1
	movdqa	%xmm0, %xmm4
	pshufb	%xmm5, %xmm2
	pand	%xmm2, %xmm3
	por	%xmm2, %xmm1
	pshufb	%xmm5, %xmm2
	movdqa	%xmm2, %xmm10
	movdqa	%xmm1, %xmm6
	por	%xmm2, %xmm1
	pshufb	%xmm5, %xmm10
	movdqa	%xmm1, %xmm0
	pand	%xmm2, %xmm6
	por	%xmm10, %xmm1
	movdqa	%xmm10, %xmm8
	por	%xmm3, %xmm6
	pshufb	%xmm5, %xmm8
	movdqa	%xmm1, %xmm7
	pand	%xmm10, %xmm0
	por	%xmm6, %xmm0
	pand	%xmm8, %xmm7
	movdqa	%xmm8, %xmm9
	por	%xmm0, %xmm7
	pshufb	%xmm5, %xmm9
	pand	%xmm8, %xmm0
	movdqa	%xmm7, %xmm5
	pand	%xmm2, %xmm3
	pand	%xmm10, %xmm6
	pand	%xmm9, %xmm5
	por	%xmm8, %xmm1
	por	%xmm6, %xmm3
	por	%xmm5, %xmm0
	pand	%xmm9, %xmm1
	por	%xmm3, %xmm0
	por	%xmm7, %xmm1
	pandn	%xmm1, %xmm0
	ptest	%xmm0, %xmm0
	je	.L40
	pcmpeqd	%xmm2, %xmm2
	movdqa	%xmm0, %xmm1
	psignw	%xmm2, %xmm1
	pand	%xmm1, %xmm0
	ret
	.cfi_endproc
	.section	.text.unlikely
	.cfi_startproc
	.type	k_E1.cold, @function
k_E1.cold:
.LFSB6475:
.L40:
	movdqa	%xmm4, %xmm0
	jmp	_ZN4dselL16SelAColdFallbackEDv2_x
	.cfi_endproc
.LFE6475:
	.text
	.size	k_E1, .-k_E1
	.section	.text.unlikely
	.size	k_E1.cold, .-k_E1.cold
.LCOLDE12:
	.text
.LHOTE12:
	.section	.text.unlikely
.LCOLDB13:
	.text
.LHOTB13:
	.p2align 4
	.globl	k_E2
	.type	k_E2, @function
k_E2:
.LFB6476:
	.cfi_startproc
	endbr64
	movdqa	%xmm0, %xmm2
	movdqa	.LC10(%rip), %xmm0
	movdqa	%xmm2, %xmm3
	movdqa	%xmm2, %xmm1
	pshufb	%xmm0, %xmm3
	movdqa	%xmm3, %xmm4
	pxor	%xmm3, %xmm1
	pand	%xmm2, %xmm3
	pshufb	%xmm0, %xmm4
	movdqa	%xmm1, %xmm0
	pand	%xmm4, %xmm1
	pxor	%xmm4, %xmm0
	movdqa	.LC11(%rip), %xmm4
	por	%xmm3, %xmm1
	movdqa	%xmm0, %xmm5
	movdqa	%xmm1, %xmm3
	movdqa	%xmm1, %xmm6
	pshufb	%xmm4, %xmm5
	pshufb	%xmm4, %xmm3
	movdqa	%xmm0, %xmm4
	pxor	%xmm3, %xmm6
	por	%xmm3, %xmm1
	pandn	%xmm0, %xmm1
	pandn	%xmm6, %xmm4
	pxor	%xmm5, %xmm0
	por	%xmm4, %xmm1
	pandn	%xmm1, %xmm0
	ptest	%xmm0, %xmm0
	je	.L45
	pcmpeqd	%xmm2, %xmm2
	movdqa	%xmm0, %xmm1
	psignw	%xmm2, %xmm1
	pand	%xmm1, %xmm0
	ret
	.cfi_endproc
	.section	.text.unlikely
	.cfi_startproc
	.type	k_E2.cold, @function
k_E2.cold:
.LFSB6476:
.L45:
	movdqa	%xmm2, %xmm0
	jmp	_ZN4dselL16SelAColdFallbackEDv2_x
	.cfi_endproc
.LFE6476:
	.text
	.size	k_E2, .-k_E2
	.section	.text.unlikely
	.size	k_E2.cold, .-k_E2.cold
.LCOLDE13:
	.text
.LHOTE13:
	.p2align 4
	.globl	k_DB
	.type	k_DB, @function
k_DB:
.LFB6477:
	.cfi_startproc
	endbr64
	movdqa	.LC10(%rip), %xmm1
	movdqa	%xmm0, %xmm3
	movdqa	%xmm0, %xmm2
	pshufb	%xmm1, %xmm3
	movdqa	%xmm3, %xmm4
	pxor	%xmm3, %xmm2
	pand	%xmm3, %xmm0
	pshufb	%xmm1, %xmm4
	movdqa	%xmm2, %xmm1
	pand	%xmm4, %xmm2
	pxor	%xmm4, %xmm1
	movdqa	.LC11(%rip), %xmm4
	por	%xmm2, %xmm0
	movdqa	%xmm1, %xmm3
	movdqa	%xmm0, %xmm2
	pshufb	%xmm4, %xmm3
	pshufb	%xmm4, %xmm2
	movdqa	%xmm1, %xmm4
	pand	%xmm3, %xmm1
	pxor	%xmm3, %xmm4
	movdqa	%xmm0, %xmm3
	movdqa	%xmm1, %xmm5
	pxor	%xmm2, %xmm3
	pand	%xmm2, %xmm0
	pand	%xmm3, %xmm1
	pxor	%xmm3, %xmm5
	por	%xmm0, %xmm1
	movdqa	%xmm4, %xmm0
	por	%xmm1, %xmm0
	pandn	%xmm5, %xmm0
	ptest	%xmm0, %xmm0
	je	.L50
	pcmpeqd	%xmm2, %xmm2
	movdqa	%xmm0, %xmm1
	psignw	%xmm2, %xmm1
	pand	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L50:
	pand	%xmm5, %xmm4
	movdqa	%xmm1, %xmm0
	pcmpeqd	%xmm2, %xmm2
	pandn	%xmm4, %xmm0
	ptest	%xmm0, %xmm0
	je	.L52
	movdqa	%xmm0, %xmm1
	psignw	%xmm2, %xmm1
	pand	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L52:
	movdqa	%xmm1, %xmm0
	psignw	%xmm2, %xmm0
	pand	%xmm1, %xmm0
	ret
	.cfi_endproc
.LFE6477:
	.size	k_DB, .-k_DB
	.section	.rodata
	.align 16
	.type	_ZN4dselL10kValueMaskE, @object
	.size	_ZN4dselL10kValueMaskE, 144
_ZN4dselL10kValueMaskE:
	.value	1
	.value	1
	.value	1
	.value	1
	.value	1
	.value	1
	.value	1
	.value	1
	.value	2
	.value	2
	.value	2
	.value	2
	.value	2
	.value	2
	.value	2
	.value	2
	.value	4
	.value	4
	.value	4
	.value	4
	.value	4
	.value	4
	.value	4
	.value	4
	.value	8
	.value	8
	.value	8
	.value	8
	.value	8
	.value	8
	.value	8
	.value	8
	.value	16
	.value	16
	.value	16
	.value	16
	.value	16
	.value	16
	.value	16
	.value	16
	.value	32
	.value	32
	.value	32
	.value	32
	.value	32
	.value	32
	.value	32
	.value	32
	.value	64
	.value	64
	.value	64
	.value	64
	.value	64
	.value	64
	.value	64
	.value	64
	.value	128
	.value	128
	.value	128
	.value	128
	.value	128
	.value	128
	.value	128
	.value	128
	.value	256
	.value	256
	.value	256
	.value	256
	.value	256
	.value	256
	.value	256
	.value	256
	.section	.rodata.cst16,"aM",@progbits,16
	.align 16
.LC0:
	.value	-2
	.value	-2
	.value	-2
	.value	-2
	.value	-2
	.value	-2
	.value	-2
	.value	-2
	.align 16
.LC1:
	.quad	36029346783166592
	.quad	36029346783166592
	.align 16
.LC2:
	.quad	18014673391583296
	.quad	18014673391583296
	.align 16
.LC3:
	.quad	9007336695791648
	.quad	9007336695791648
	.align 16
.LC4:
	.quad	4503668347895824
	.quad	4503668347895824
	.align 16
.LC5:
	.quad	2251834173947912
	.quad	2251834173947912
	.align 16
.LC6:
	.quad	1125917086973956
	.quad	1125917086973956
	.align 16
.LC7:
	.quad	562958543486978
	.quad	562958543486978
	.align 16
.LC8:
	.quad	281479271743489
	.quad	281479271743489
	.align 16
.LC9:
	.quad	72058693566333184
	.quad	72058693566333184
	.align 16
.LC10:
	.byte	2
	.byte	3
	.byte	4
	.byte	5
	.byte	6
	.byte	7
	.byte	8
	.byte	9
	.byte	10
	.byte	11
	.byte	0
	.byte	1
	.byte	-128
	.byte	-128
	.byte	-128
	.byte	-128
	.align 16
.LC11:
	.byte	6
	.byte	7
	.byte	8
	.byte	9
	.byte	10
	.byte	11
	.byte	0
	.byte	1
	.byte	2
	.byte	3
	.byte	4
	.byte	5
	.byte	-128
	.byte	-128
	.byte	-128
	.byte	-128
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
