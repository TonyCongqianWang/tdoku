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
	vpand	(%rax,%rdi), %xmm0, %xmm0
	vmovq	%xmm0, %rdx
	vpsrldq	$8, %xmm0, %xmm0
	vmovq	%xmm0, %rax
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
	vmovdqa	%xmm0, %xmm2
	movl	$7, %edi
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$6, %edi
	vmovdqa	%xmm2, %xmm0
	movl	%eax, %r8d
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$5, %edi
	vmovdqa	%xmm2, %xmm0
	movl	%eax, %r10d
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$4, %edi
	vmovdqa	%xmm2, %xmm0
	movl	%eax, %esi
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$3, %edi
	vmovdqa	%xmm2, %xmm0
	movl	%eax, %r11d
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$2, %edi
	vmovdqa	%xmm2, %xmm0
	vmovd	%r11d, %xmm4
	movl	%eax, %r9d
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$1, %edi
	vmovdqa	%xmm2, %xmm0
	movl	%eax, %ebx
	call	_ZN4dselL13PopcountDigitEDv2_xi
	xorl	%edi, %edi
	vmovdqa	%xmm2, %xmm0
	movl	%eax, %ecx
	call	_ZN4dselL13PopcountDigitEDv2_xi
	vmovd	%eax, %xmm1
	movl	$-2, %eax
	vpinsrw	$1, %ecx, %xmm1, %xmm0
	vmovd	%ebx, %xmm1
	vpinsrw	$1, %r9d, %xmm1, %xmm3
	vpinsrw	$1, %esi, %xmm4, %xmm1
	vmovd	%r10d, %xmm4
	vpinsrw	$1, %r8d, %xmm4, %xmm5
	vpunpckldq	%xmm3, %xmm0, %xmm0
	vpunpckldq	%xmm5, %xmm1, %xmm1
	vpunpcklqdq	%xmm1, %xmm0, %xmm0
	vmovd	%eax, %xmm1
	vpbroadcastw	%xmm1, %xmm1
	vpaddw	%xmm0, %xmm1, %xmm1
	vphminposuw	%xmm1, %xmm1
	vmovd	%xmm1, %eax
	movl	%eax, %esi
	shrl	$16, %esi
	andl	$65535, %eax
	je	.L3
	movl	$8, %edi
	vmovdqa	%xmm2, %xmm0
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
	vmovdqa	(%rax,%rsi), %xmm0
	ret
	.cfi_endproc
.LFE6456:
	.size	_ZN4dselL16SelAColdFallbackEDv2_x, .-_ZN4dselL16SelAColdFallbackEDv2_x
	.text
	.p2align 4
	.globl	k_A
	.type	k_A, @function
k_A:
.LFB6471:
	.cfi_startproc
	endbr64
	movabsq	$36029346783166592, %rax
	pushq	%r15
	.cfi_def_cfa_offset 16
	.cfi_offset 15, -16
	movabsq	$4503668347895824, %rsi
	movabsq	$562958543486978, %r15
	vmovq	%rax, %xmm1
	pushq	%r14
	.cfi_def_cfa_offset 24
	.cfi_offset 14, -24
	movabsq	$1125917086973956, %r14
	movabsq	$281479271743489, %r10
	vpunpcklqdq	%xmm1, %xmm1, %xmm1
	pushq	%r13
	.cfi_def_cfa_offset 32
	.cfi_offset 13, -32
	vpand	%xmm1, %xmm0, %xmm1
	pushq	%r12
	.cfi_def_cfa_offset 40
	.cfi_offset 12, -40
	pushq	%rbp
	.cfi_def_cfa_offset 48
	.cfi_offset 6, -48
	vmovq	%xmm1, %rax
	vpextrq	$1, %xmm1, %rcx
	pushq	%rbx
	.cfi_def_cfa_offset 56
	.cfi_offset 3, -56
	popcntq	%rax, %rax
	popcntq	%rcx, %rcx
	movabsq	$18014673391583296, %rbx
	vmovq	%rbx, %xmm1
	movabsq	$9007336695791648, %rbx
	vpunpcklqdq	%xmm1, %xmm1, %xmm1
	vpand	%xmm1, %xmm0, %xmm1
	movq	%rax, -16(%rsp)
	vmovq	%xmm1, %r11
	vpextrq	$1, %xmm1, %rdi
	vmovq	%rbx, %xmm1
	vpunpcklqdq	%xmm1, %xmm1, %xmm1
	popcntq	%r11, %r11
	popcntq	%rdi, %rdi
	addl	%r11d, %edi
	vpand	%xmm1, %xmm0, %xmm1
	vmovd	%edi, %xmm4
	vmovq	%xmm1, %rbx
	vpextrq	$1, %xmm1, %rdx
	vmovq	%rsi, %xmm1
	movabsq	$2251834173947912, %rsi
	vpunpcklqdq	%xmm1, %xmm1, %xmm1
	popcntq	%rbx, %rbx
	popcntq	%rdx, %rdx
	addl	%ebx, %edx
	vpand	%xmm1, %xmm0, %xmm1
	movl	-16(%rsp), %ebx
	vmovq	%xmm1, %rbp
	vpextrq	$1, %xmm1, %r8
	vmovq	%rsi, %xmm1
	vpunpcklqdq	%xmm1, %xmm1, %xmm1
	popcntq	%rbp, %rbp
	popcntq	%r8, %r8
	addl	%ebx, %ecx
	vpand	%xmm1, %xmm0, %xmm1
	addl	%ebp, %r8d
	vmovq	%xmm1, %r12
	vpextrq	$1, %xmm1, %rsi
	vmovq	%r14, %xmm1
	vpunpcklqdq	%xmm1, %xmm1, %xmm1
	vmovd	%r8d, %xmm3
	popcntq	%r12, %r12
	popcntq	%rsi, %rsi
	vpand	%xmm1, %xmm0, %xmm1
	addl	%r12d, %esi
	vmovq	%xmm1, %r13
	vpextrq	$1, %xmm1, %r9
	vmovq	%r15, %xmm1
	vpunpcklqdq	%xmm1, %xmm1, %xmm1
	popcntq	%r13, %r13
	popcntq	%r9, %r9
	addl	%r13d, %r9d
	vpand	%xmm1, %xmm0, %xmm1
	vmovq	%xmm1, %r14
	vpextrq	$1, %xmm1, %rax
	vmovq	%r10, %xmm1
	vpunpcklqdq	%xmm1, %xmm1, %xmm1
	popcntq	%r14, %r14
	popcntq	%rax, %rax
	addl	%r14d, %eax
	vpand	%xmm1, %xmm0, %xmm1
	vmovq	%xmm1, %r15
	vpextrq	$1, %xmm1, %r10
	popcntq	%r15, %r15
	popcntq	%r10, %r10
	addl	%r15d, %r10d
	vmovd	%r10d, %xmm1
	vpinsrw	$1, %eax, %xmm1, %xmm2
	vmovd	%r9d, %xmm1
	movl	$-2, %eax
	vpinsrw	$1, %esi, %xmm1, %xmm5
	vpinsrw	$1, %edx, %xmm3, %xmm1
	vpinsrw	$1, %ecx, %xmm4, %xmm3
	vpunpckldq	%xmm3, %xmm1, %xmm1
	vpunpckldq	%xmm5, %xmm2, %xmm2
	vpunpcklqdq	%xmm1, %xmm2, %xmm2
	vmovd	%eax, %xmm1
	vpbroadcastw	%xmm1, %xmm1
	vpaddw	%xmm2, %xmm1, %xmm1
	vphminposuw	%xmm1, %xmm1
	vmovd	%xmm1, %eax
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
	vmovdqa	(%rdx,%rax), %xmm0
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
	movabsq	$72058693566333184, %rdi
	vmovq	%rdi, %xmm1
	vpunpcklqdq	%xmm1, %xmm1, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
	vmovq	%xmm0, %rsi
	vpextrq	$1, %xmm0, %rdx
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
.LFE6471:
	.size	k_A, .-k_A
	.p2align 4
	.globl	k_B
	.type	k_B, @function
k_B:
.LFB6472:
	.cfi_startproc
	endbr64
	vmovdqa	.LC10(%rip), %xmm2
	vpshufb	%xmm2, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm4
	vpor	%xmm1, %xmm0, %xmm0
	vpshufb	%xmm2, %xmm1, %xmm1
	vpand	%xmm1, %xmm4, %xmm6
	vpand	%xmm1, %xmm0, %xmm3
	vpor	%xmm1, %xmm0, %xmm0
	vpshufb	%xmm2, %xmm1, %xmm1
	vpor	%xmm4, %xmm3, %xmm3
	vpand	%xmm1, %xmm0, %xmm5
	vpand	%xmm1, %xmm3, %xmm7
	vpor	%xmm1, %xmm0, %xmm0
	vpor	%xmm3, %xmm5, %xmm5
	vpshufb	%xmm2, %xmm1, %xmm3
	vpor	%xmm6, %xmm7, %xmm7
	vpand	%xmm3, %xmm0, %xmm4
	vpand	%xmm3, %xmm5, %xmm8
	vpshufb	%xmm2, %xmm3, %xmm2
	vpor	%xmm5, %xmm4, %xmm4
	vpor	%xmm3, %xmm0, %xmm0
	vpor	%xmm7, %xmm8, %xmm8
	vpand	%xmm2, %xmm4, %xmm5
	vpand	%xmm2, %xmm0, %xmm0
	vpor	%xmm8, %xmm5, %xmm5
	vpor	%xmm4, %xmm0, %xmm0
	vpandn	%xmm0, %xmm5, %xmm0
	vptest	%xmm0, %xmm0
	je	.L16
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsignw	%xmm1, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L16:
	vpand	%xmm1, %xmm6, %xmm6
	vpand	%xmm3, %xmm7, %xmm7
	vpand	%xmm2, %xmm8, %xmm8
	vpor	%xmm7, %xmm6, %xmm6
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpor	%xmm8, %xmm6, %xmm6
	vpandn	%xmm5, %xmm6, %xmm5
	vptest	%xmm5, %xmm5
	je	.L18
	vpsignw	%xmm0, %xmm5, %xmm0
	vpand	%xmm0, %xmm5, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L18:
	vpsignw	%xmm0, %xmm6, %xmm0
	vpand	%xmm0, %xmm6, %xmm0
	ret
	.cfi_endproc
.LFE6472:
	.size	k_B, .-k_B
	.p2align 4
	.globl	k_C
	.type	k_C, @function
k_C:
.LFB6473:
	.cfi_startproc
	endbr64
	vmovdqa	.LC10(%rip), %xmm2
	vpshufb	%xmm2, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm4
	vpor	%xmm1, %xmm0, %xmm0
	vpshufb	%xmm2, %xmm1, %xmm1
	vpand	%xmm1, %xmm4, %xmm6
	vpand	%xmm1, %xmm0, %xmm3
	vpor	%xmm1, %xmm0, %xmm0
	vpshufb	%xmm2, %xmm1, %xmm1
	vpor	%xmm4, %xmm3, %xmm3
	vpand	%xmm1, %xmm0, %xmm5
	vpand	%xmm1, %xmm3, %xmm7
	vpor	%xmm1, %xmm0, %xmm0
	vpor	%xmm3, %xmm5, %xmm5
	vpshufb	%xmm2, %xmm1, %xmm3
	vpor	%xmm6, %xmm7, %xmm7
	vpand	%xmm3, %xmm0, %xmm4
	vpand	%xmm3, %xmm5, %xmm8
	vpshufb	%xmm2, %xmm3, %xmm2
	vpor	%xmm5, %xmm4, %xmm4
	vpor	%xmm3, %xmm0, %xmm0
	vpor	%xmm7, %xmm8, %xmm8
	vpand	%xmm2, %xmm4, %xmm5
	vpand	%xmm2, %xmm0, %xmm0
	vpor	%xmm8, %xmm5, %xmm5
	vpor	%xmm4, %xmm0, %xmm0
	vpandn	%xmm0, %xmm5, %xmm0
	vptest	%xmm0, %xmm0
	je	.L20
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsignw	%xmm1, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L20:
	vpand	%xmm1, %xmm6, %xmm6
	vpand	%xmm3, %xmm7, %xmm7
	vpand	%xmm2, %xmm8, %xmm8
	vpor	%xmm6, %xmm7, %xmm7
	vpor	%xmm7, %xmm8, %xmm8
	vpandn	%xmm5, %xmm8, %xmm5
	vptest	%xmm5, %xmm5
	je	.L22
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpsignw	%xmm0, %xmm5, %xmm0
	vpand	%xmm0, %xmm5, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L22:
	vpand	%xmm3, %xmm6, %xmm6
	vpand	%xmm2, %xmm7, %xmm7
	vpor	%xmm6, %xmm7, %xmm7
	vpandn	%xmm8, %xmm7, %xmm8
	vptest	%xmm8, %xmm8
	jne	.L25
	vpand	%xmm2, %xmm6, %xmm6
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpandn	%xmm7, %xmm6, %xmm7
	vptest	%xmm7, %xmm7
	je	.L24
	vpsignw	%xmm0, %xmm7, %xmm0
	vpand	%xmm0, %xmm7, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L25:
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpsignw	%xmm0, %xmm8, %xmm0
	vpand	%xmm0, %xmm8, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L24:
	vpsignw	%xmm0, %xmm6, %xmm0
	vpand	%xmm0, %xmm6, %xmm0
	ret
	.cfi_endproc
.LFE6473:
	.size	k_C, .-k_C
	.p2align 4
	.globl	k_D1
	.type	k_D1, @function
k_D1:
.LFB6474:
	.cfi_startproc
	endbr64
	vmovdqa	.LC10(%rip), %xmm3
	vpshufb	%xmm3, %xmm0, %xmm4
	vpshufb	%xmm3, %xmm4, %xmm3
	vpxor	%xmm4, %xmm0, %xmm2
	vpand	%xmm4, %xmm0, %xmm0
	vpxor	%xmm3, %xmm2, %xmm1
	vpand	%xmm3, %xmm2, %xmm2
	vpor	%xmm0, %xmm2, %xmm0
	vmovdqa	.LC11(%rip), %xmm2
	vpshufb	%xmm2, %xmm1, %xmm3
	vpshufb	%xmm2, %xmm0, %xmm2
	vpxor	%xmm3, %xmm1, %xmm4
	vpand	%xmm3, %xmm1, %xmm1
	vpxor	%xmm2, %xmm0, %xmm3
	vpxor	%xmm3, %xmm1, %xmm5
	vpand	%xmm2, %xmm0, %xmm0
	vpand	%xmm3, %xmm1, %xmm1
	vpor	%xmm0, %xmm1, %xmm1
	vpor	%xmm1, %xmm4, %xmm0
	vpandn	%xmm5, %xmm0, %xmm0
	vptest	%xmm0, %xmm0
	je	.L27
.L33:
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsignw	%xmm1, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L27:
	vpand	%xmm5, %xmm4, %xmm0
	vpandn	%xmm0, %xmm1, %xmm0
	vptest	%xmm0, %xmm0
	jne	.L33
	vpor	%xmm5, %xmm4, %xmm0
	vpandn	%xmm1, %xmm0, %xmm0
	vptest	%xmm0, %xmm0
	jne	.L33
	vpand	%xmm1, %xmm4, %xmm4
	vpandn	%xmm4, %xmm5, %xmm4
	vptest	%xmm4, %xmm4
	je	.L31
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpsignw	%xmm0, %xmm4, %xmm0
	vpand	%xmm0, %xmm4, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L31:
	vpand	%xmm1, %xmm5, %xmm5
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpsignw	%xmm0, %xmm5, %xmm0
	vpand	%xmm0, %xmm5, %xmm0
	ret
	.cfi_endproc
.LFE6474:
	.size	k_D1, .-k_D1
	.p2align 4
	.globl	k_D2
	.type	k_D2, @function
k_D2:
.LFB6475:
	.cfi_startproc
	endbr64
	vpsrlq	$16, %xmm0, %xmm3
	vpsrlq	$32, %xmm0, %xmm5
	vpunpckhqdq	%xmm0, %xmm0, %xmm4
	vpxor	%xmm3, %xmm0, %xmm2
	vpsrlq	$16, %xmm4, %xmm6
	vpand	%xmm3, %xmm0, %xmm3
	vpxor	%xmm5, %xmm2, %xmm1
	vpsrlq	$48, %xmm0, %xmm0
	vpand	%xmm5, %xmm2, %xmm2
	vpor	%xmm3, %xmm2, %xmm2
	vpxor	%xmm4, %xmm0, %xmm3
	vpand	%xmm4, %xmm0, %xmm0
	vpxor	%xmm6, %xmm3, %xmm5
	vpand	%xmm6, %xmm3, %xmm3
	vpor	%xmm0, %xmm3, %xmm0
	vpand	%xmm5, %xmm1, %xmm3
	vpxor	%xmm5, %xmm1, %xmm1
	vpxor	%xmm0, %xmm2, %xmm4
	vpand	%xmm0, %xmm2, %xmm2
	vpxor	%xmm4, %xmm3, %xmm5
	vpand	%xmm4, %xmm3, %xmm3
	vpunpcklwd	%xmm5, %xmm1, %xmm1
	vpor	%xmm2, %xmm3, %xmm3
	vpunpckldq	%xmm3, %xmm1, %xmm1
	vmovq	%xmm1, %rax
	movq	%rax, %rdi
	movq	%rax, %r8
	shrq	$32, %rdi
	shrq	$16, %r8
	movl	%edi, %ecx
	movl	%r8d, %esi
	orl	%eax, %ecx
	andl	$511, %esi
	andl	$511, %ecx
	andn	%esi, %ecx, %edx
	andn	%esi, %ecx, %ecx
	je	.L36
.L35:
	tzcntl	%edx, %edx
	leaq	_ZN4dselL10kValueMaskE(%rip), %rax
	salq	$4, %rdx
	vmovdqa	(%rax,%rdx), %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L36:
	movl	%esi, %ecx
	andl	$511, %edi
	movl	%eax, %r9d
	andl	%eax, %ecx
	andl	$511, %r9d
	andn	%ecx, %edi, %edx
	andn	%ecx, %edi, %ecx
	jne	.L35
	orl	%eax, %r8d
	andl	$511, %r8d
	andn	%edi, %r8d, %edx
	andn	%edi, %r8d, %r8d
	jne	.L35
	andl	%edi, %r9d
	andl	%esi, %edi
	andn	%r9d, %esi, %edx
	andn	%r9d, %esi, %r9d
	cmove	%edi, %edx
	jmp	.L35
	.cfi_endproc
.LFE6475:
	.size	k_D2, .-k_D2
	.section	.text.unlikely
.LCOLDB12:
	.text
.LHOTB12:
	.p2align 4
	.globl	k_E1
	.type	k_E1, @function
k_E1:
.LFB6476:
	.cfi_startproc
	endbr64
	vmovdqa	.LC10(%rip), %xmm4
	vmovdqa	%xmm0, %xmm5
	vpshufb	%xmm4, %xmm0, %xmm2
	vpand	%xmm2, %xmm0, %xmm3
	vpor	%xmm2, %xmm0, %xmm1
	vpshufb	%xmm4, %xmm2, %xmm2
	vpshufb	%xmm4, %xmm2, %xmm9
	vpand	%xmm2, %xmm1, %xmm6
	vpor	%xmm2, %xmm1, %xmm1
	vpshufb	%xmm4, %xmm9, %xmm8
	vpor	%xmm3, %xmm6, %xmm6
	vpand	%xmm9, %xmm1, %xmm0
	vpor	%xmm9, %xmm1, %xmm1
	vpor	%xmm6, %xmm0, %xmm0
	vpshufb	%xmm4, %xmm8, %xmm4
	vpand	%xmm8, %xmm1, %xmm7
	vpand	%xmm2, %xmm3, %xmm3
	vpand	%xmm9, %xmm6, %xmm6
	vpor	%xmm0, %xmm7, %xmm7
	vpor	%xmm8, %xmm1, %xmm1
	vpand	%xmm8, %xmm0, %xmm0
	vpand	%xmm4, %xmm7, %xmm10
	vpor	%xmm6, %xmm3, %xmm3
	vpand	%xmm4, %xmm1, %xmm1
	vpor	%xmm10, %xmm0, %xmm0
	vpor	%xmm7, %xmm1, %xmm1
	vpor	%xmm3, %xmm0, %xmm0
	vpandn	%xmm1, %xmm0, %xmm0
	vptest	%xmm0, %xmm0
	je	.L38
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsignw	%xmm1, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
	ret
	.cfi_endproc
	.section	.text.unlikely
	.cfi_startproc
	.type	k_E1.cold, @function
k_E1.cold:
.LFSB6476:
.L38:
	vmovdqa	%xmm5, %xmm0
	jmp	_ZN4dselL16SelAColdFallbackEDv2_x
	.cfi_endproc
.LFE6476:
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
.LFB6477:
	.cfi_startproc
	endbr64
	vmovdqa	.LC10(%rip), %xmm4
	vmovdqa	%xmm0, %xmm2
	vpshufb	%xmm4, %xmm0, %xmm3
	vpshufb	%xmm4, %xmm3, %xmm4
	vpxor	%xmm3, %xmm0, %xmm1
	vpand	%xmm3, %xmm2, %xmm3
	vpxor	%xmm4, %xmm1, %xmm0
	vpand	%xmm4, %xmm1, %xmm1
	vpor	%xmm3, %xmm1, %xmm1
	vmovdqa	.LC11(%rip), %xmm3
	vpshufb	%xmm3, %xmm0, %xmm5
	vpshufb	%xmm3, %xmm1, %xmm3
	vpxor	%xmm3, %xmm1, %xmm4
	vpor	%xmm3, %xmm1, %xmm1
	vpandn	%xmm4, %xmm0, %xmm4
	vpandn	%xmm0, %xmm1, %xmm1
	vpxor	%xmm5, %xmm0, %xmm0
	vpor	%xmm4, %xmm1, %xmm1
	vpandn	%xmm1, %xmm0, %xmm0
	vptest	%xmm0, %xmm0
	je	.L43
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsignw	%xmm1, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
	ret
	.cfi_endproc
	.section	.text.unlikely
	.cfi_startproc
	.type	k_E2.cold, @function
k_E2.cold:
.LFSB6477:
.L43:
	vmovdqa	%xmm2, %xmm0
	jmp	_ZN4dselL16SelAColdFallbackEDv2_x
	.cfi_endproc
.LFE6477:
	.text
	.size	k_E2, .-k_E2
	.section	.text.unlikely
	.size	k_E2.cold, .-k_E2.cold
.LCOLDE13:
	.text
.LHOTE13:
	.p2align 4
	.globl	k_F
	.type	k_F, @function
k_F:
.LFB6478:
	.cfi_startproc
	endbr64
	vpshufb	.LC14(%rip), %xmm0, %xmm1
	vmovdqa	.LC15(%rip), %xmm0
	movabsq	$1085102592571150095, %rax
	vmovq	%rax, %xmm3
	movl	$-2, %eax
	vgf2p8affineqb	$0, %xmm1, %xmm0, %xmm0
	vpunpcklqdq	%xmm3, %xmm3, %xmm3
	vmovdqa	.LC17(%rip), %xmm1
	vpsrlw	$4, %xmm0, %xmm2
	vpand	%xmm3, %xmm0, %xmm0
	vpand	%xmm3, %xmm2, %xmm2
	vpshufb	%xmm2, %xmm1, %xmm2
	vpshufb	%xmm0, %xmm1, %xmm1
	vmovd	%eax, %xmm0
	vpbroadcastb	%xmm0, %xmm0
	vpaddb	%xmm2, %xmm1, %xmm1
	vpaddb	%xmm0, %xmm1, %xmm1
	vpmovzxbw	%xmm1, %xmm0
	vpextrb	$8, %xmm1, %ecx
	vphminposuw	%xmm0, %xmm0
	vmovd	%xmm0, %eax
	movzwl	%ax, %edx
	cmpl	%edx, %ecx
	jb	.L49
	shrl	$16, %eax
	leaq	_ZN4dselL10kValueMaskE(%rip), %rdx
	salq	$4, %rax
	vmovdqa	(%rdx,%rax), %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L49:
	movl	$128, %eax
	leaq	_ZN4dselL10kValueMaskE(%rip), %rdx
	vmovdqa	(%rdx,%rax), %xmm0
	ret
	.cfi_endproc
.LFE6478:
	.size	k_F, .-k_F
	.p2align 4
	.globl	k_DB
	.type	k_DB, @function
k_DB:
.LFB6479:
	.cfi_startproc
	endbr64
	vmovdqa	.LC10(%rip), %xmm3
	vpshufb	%xmm3, %xmm0, %xmm4
	vpshufb	%xmm3, %xmm4, %xmm3
	vpxor	%xmm4, %xmm0, %xmm2
	vpand	%xmm4, %xmm0, %xmm0
	vpxor	%xmm3, %xmm2, %xmm1
	vpand	%xmm3, %xmm2, %xmm2
	vpor	%xmm0, %xmm2, %xmm0
	vmovdqa	.LC11(%rip), %xmm2
	vpshufb	%xmm2, %xmm1, %xmm3
	vpshufb	%xmm2, %xmm0, %xmm2
	vpxor	%xmm3, %xmm1, %xmm4
	vpand	%xmm3, %xmm1, %xmm1
	vpxor	%xmm2, %xmm0, %xmm3
	vpxor	%xmm3, %xmm1, %xmm5
	vpand	%xmm2, %xmm0, %xmm0
	vpand	%xmm3, %xmm1, %xmm1
	vpor	%xmm0, %xmm1, %xmm1
	vpor	%xmm1, %xmm4, %xmm0
	vpandn	%xmm5, %xmm0, %xmm0
	vptest	%xmm0, %xmm0
	je	.L51
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsignw	%xmm1, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L51:
	vpand	%xmm5, %xmm4, %xmm4
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpandn	%xmm4, %xmm1, %xmm4
	vptest	%xmm4, %xmm4
	je	.L53
	vpsignw	%xmm0, %xmm4, %xmm0
	vpand	%xmm0, %xmm4, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L53:
	vpsignw	%xmm0, %xmm1, %xmm0
	vpand	%xmm0, %xmm1, %xmm0
	ret
	.cfi_endproc
.LFE6479:
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
	.align 16
.LC14:
	.byte	-128
	.byte	-128
	.byte	10
	.byte	8
	.byte	6
	.byte	4
	.byte	2
	.byte	0
	.byte	-128
	.byte	-128
	.byte	11
	.byte	9
	.byte	7
	.byte	5
	.byte	3
	.byte	1
	.align 16
.LC15:
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	-128
	.byte	1
	.byte	2
	.byte	4
	.byte	8
	.byte	16
	.byte	32
	.byte	64
	.byte	-128
	.align 16
.LC17:
	.byte	0
	.byte	1
	.byte	1
	.byte	2
	.byte	1
	.byte	2
	.byte	2
	.byte	3
	.byte	1
	.byte	2
	.byte	2
	.byte	3
	.byte	2
	.byte	3
	.byte	3
	.byte	4
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
