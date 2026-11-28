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
	movl	%eax, %ebx
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$4, %edi
	vmovdqa	%xmm2, %xmm0
	vmovd	%ebx, %xmm5
	movl	%eax, %esi
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$3, %edi
	vmovdqa	%xmm2, %xmm0
	movl	%eax, %r10d
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$2, %edi
	vmovdqa	%xmm2, %xmm0
	vmovd	%r10d, %xmm4
	movl	%eax, %r9d
	call	_ZN4dselL13PopcountDigitEDv2_xi
	movl	$1, %edi
	vmovdqa	%xmm2, %xmm0
	movl	%eax, %r11d
	call	_ZN4dselL13PopcountDigitEDv2_xi
	vmovdqa	%xmm2, %xmm0
	xorl	%edi, %edi
	movl	%eax, %ecx
	call	_ZN4dselL13PopcountDigitEDv2_xi
	vmovd	%eax, %xmm1
	movl	$-2, %eax
	vpinsrw	$1, %ecx, %xmm1, %xmm0
	vmovd	%r11d, %xmm1
	vpinsrw	$1, %r9d, %xmm1, %xmm3
	vpinsrw	$1, %esi, %xmm4, %xmm1
	vpinsrw	$1, %r8d, %xmm5, %xmm4
	vpunpckldq	%xmm4, %xmm1, %xmm1
	vpunpckldq	%xmm3, %xmm0, %xmm0
	vpunpcklqdq	%xmm1, %xmm0, %xmm0
	vpbroadcastw	%eax, %xmm1
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
	vmovdqa	%xmm0, %xmm4
	vpbroadcastq	%rax, %xmm6
	movabsq	$18014673391583296, %rax
	vpbroadcastq	%rax, %xmm3
	vpand	%xmm6, %xmm0, %xmm6
	movabsq	$9007336695791648, %rax
	vpand	%xmm3, %xmm4, %xmm3
	vpunpckhqdq	%xmm6, %xmm6, %xmm0
	vpbroadcastq	%rax, %xmm7
	movabsq	$4503668347895824, %rax
	vpunpckhqdq	%xmm3, %xmm3, %xmm8
	vpand	%xmm7, %xmm4, %xmm7
	vpunpcklqdq	%xmm6, %xmm3, %xmm3
	vpopcntq	%xmm3, %xmm3
	vpunpcklqdq	%xmm0, %xmm8, %xmm8
	vpbroadcastq	%rax, %xmm0
	movabsq	$2251834173947912, %rax
	vpopcntq	%xmm8, %xmm8
	vpbroadcastq	%rax, %xmm9
	movabsq	$1125917086973956, %rax
	vpand	%xmm0, %xmm4, %xmm0
	vpbroadcastq	%rax, %xmm2
	vpunpckhqdq	%xmm7, %xmm7, %xmm1
	vpunpckhqdq	%xmm0, %xmm0, %xmm10
	movabsq	$562958543486978, %rax
	vpand	%xmm9, %xmm4, %xmm9
	vpand	%xmm2, %xmm4, %xmm2
	vpunpcklqdq	%xmm1, %xmm10, %xmm10
	vpopcntq	%xmm10, %xmm10
	vpunpckhqdq	%xmm9, %xmm9, %xmm1
	vpunpckhqdq	%xmm2, %xmm2, %xmm11
	vpbroadcastq	%rax, %xmm12
	vpunpcklqdq	%xmm1, %xmm11, %xmm11
	vpand	%xmm12, %xmm4, %xmm12
	vpunpcklqdq	%xmm9, %xmm2, %xmm2
	movabsq	$281479271743489, %rax
	vpopcntq	%xmm2, %xmm2
	vpbroadcastq	%rax, %xmm1
	vpunpckhqdq	%xmm12, %xmm12, %xmm13
	movl	$-2, %eax
	vpopcntq	%xmm11, %xmm11
	vpand	%xmm1, %xmm4, %xmm1
	vpunpcklqdq	%xmm12, %xmm1, %xmm5
	vpunpckhqdq	%xmm1, %xmm1, %xmm1
	vpopcntq	%xmm5, %xmm5
	vpunpcklqdq	%xmm13, %xmm1, %xmm1
	vpopcntq	%xmm1, %xmm1
	vshufps	$136, %xmm8, %xmm10, %xmm10
	vshufps	$136, %xmm2, %xmm5, %xmm5
	vpunpcklqdq	%xmm7, %xmm0, %xmm2
	vmovdqa	.LC9(%rip), %xmm0
	vpopcntq	%xmm2, %xmm2
	vshufps	$136, %xmm11, %xmm1, %xmm1
	vshufps	$136, %xmm3, %xmm2, %xmm2
	vmovdqa	%xmm5, %xmm3
	vpermt2w	%xmm2, %xmm0, %xmm3
	vpermi2w	%xmm10, %xmm1, %xmm0
	vpbroadcastw	%eax, %xmm1
	vpaddw	%xmm3, %xmm0, %xmm0
	vpaddw	%xmm1, %xmm0, %xmm0
	vphminposuw	%xmm0, %xmm0
	vmovd	%xmm0, %eax
	movl	%eax, %ecx
	shrl	$16, %ecx
	andl	$65535, %eax
	jne	.L11
.L13:
	movl	%ecx, %eax
	salq	$4, %rax
.L12:
	leaq	_ZN4dselL10kValueMaskE(%rip), %rdx
	vmovdqa	(%rdx,%rax), %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L11:
	movabsq	$72058693566333184, %rdx
	vpbroadcastq	%rdx, %xmm0
	vpand	%xmm0, %xmm4, %xmm0
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
	vmovdqa	.LC11(%rip), %xmm3
	vpshufb	%xmm3, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm2
	vpor	%xmm1, %xmm0, %xmm0
	vpshufb	%xmm3, %xmm1, %xmm1
	vpand	%xmm1, %xmm2, %xmm8
	vpternlogq	$248, %xmm0, %xmm1, %xmm2
	vpor	%xmm1, %xmm0, %xmm0
	vmovdqa	%xmm8, %xmm5
	vpshufb	%xmm3, %xmm1, %xmm1
	vpternlogq	$248, %xmm2, %xmm1, %xmm5
	vpshufb	%xmm3, %xmm1, %xmm4
	vpternlogq	$248, %xmm0, %xmm1, %xmm2
	vmovdqa	%xmm5, %xmm7
	vpor	%xmm1, %xmm0, %xmm0
	vpshufb	%xmm3, %xmm4, %xmm3
	vpternlogq	$248, %xmm2, %xmm4, %xmm7
	vpternlogq	$248, %xmm0, %xmm4, %xmm2
	vmovdqa	%xmm3, %xmm9
	vmovdqa	%xmm7, %xmm6
	vpternlogq	$224, %xmm0, %xmm4, %xmm9
	vpternlogq	$248, %xmm2, %xmm3, %xmm6
	vmovdqa	%xmm6, %xmm0
	vpternlogq	$14, %xmm9, %xmm2, %xmm0
	vptest	%xmm0, %xmm0
	je	.L15
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsignw	%xmm1, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L15:
	vpand	%xmm4, %xmm5, %xmm5
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpternlogq	$248, %xmm8, %xmm1, %xmm5
	vpternlogq	$248, %xmm7, %xmm3, %xmm5
	vpandn	%xmm6, %xmm5, %xmm6
	vptest	%xmm6, %xmm6
	je	.L17
	vpsignw	%xmm0, %xmm6, %xmm0
	vpand	%xmm0, %xmm6, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L17:
	vpsignw	%xmm0, %xmm5, %xmm0
	vpand	%xmm0, %xmm5, %xmm0
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
	vmovdqa	.LC11(%rip), %xmm3
	vpshufb	%xmm3, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm2
	vpor	%xmm1, %xmm0, %xmm0
	vpshufb	%xmm3, %xmm1, %xmm1
	vpand	%xmm1, %xmm2, %xmm5
	vpternlogq	$248, %xmm0, %xmm1, %xmm2
	vpor	%xmm1, %xmm0, %xmm0
	vmovdqa	%xmm5, %xmm8
	vpshufb	%xmm3, %xmm1, %xmm1
	vpternlogq	$248, %xmm2, %xmm1, %xmm8
	vpshufb	%xmm3, %xmm1, %xmm4
	vpternlogq	$248, %xmm0, %xmm1, %xmm2
	vmovdqa	%xmm8, %xmm7
	vpor	%xmm1, %xmm0, %xmm0
	vpshufb	%xmm3, %xmm4, %xmm3
	vpternlogq	$248, %xmm2, %xmm4, %xmm7
	vpternlogq	$248, %xmm0, %xmm4, %xmm2
	vmovdqa	%xmm3, %xmm9
	vmovdqa	%xmm7, %xmm6
	vpternlogq	$224, %xmm0, %xmm4, %xmm9
	vpternlogq	$248, %xmm2, %xmm3, %xmm6
	vmovdqa	%xmm6, %xmm0
	vpternlogq	$14, %xmm9, %xmm2, %xmm0
	vptest	%xmm0, %xmm0
	je	.L19
.L24:
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsignw	%xmm1, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L19:
	vpand	%xmm1, %xmm5, %xmm5
	vmovdqa	%xmm5, %xmm1
	vpternlogq	$248, %xmm8, %xmm4, %xmm1
	vmovdqa	%xmm1, %xmm0
	vpternlogq	$248, %xmm7, %xmm3, %xmm0
	vpandn	%xmm6, %xmm0, %xmm6
	vptest	%xmm6, %xmm6
	je	.L21
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpsignw	%xmm0, %xmm6, %xmm0
	vpand	%xmm0, %xmm6, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L21:
	vpand	%xmm4, %xmm5, %xmm5
	vmovdqa	%xmm5, %xmm2
	vpternlogq	$248, %xmm1, %xmm3, %xmm2
	vpandn	%xmm0, %xmm2, %xmm0
	vptest	%xmm0, %xmm0
	jne	.L24
	vpand	%xmm3, %xmm5, %xmm5
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpandn	%xmm2, %xmm5, %xmm2
	vptest	%xmm2, %xmm2
	je	.L23
	vpsignw	%xmm0, %xmm2, %xmm0
	vpand	%xmm0, %xmm2, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L23:
	vpsignw	%xmm0, %xmm5, %xmm0
	vpand	%xmm0, %xmm5, %xmm0
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
	vmovdqa	.LC11(%rip), %xmm1
	vmovdqa	%xmm0, %xmm3
	vpshufb	%xmm1, %xmm0, %xmm2
	vpshufb	%xmm1, %xmm2, %xmm1
	vpternlogd	$150, %xmm1, %xmm2, %xmm3
	vpternlogd	$232, %xmm1, %xmm2, %xmm0
	vmovdqa	.LC12(%rip), %xmm1
	vpshufb	%xmm1, %xmm3, %xmm2
	vpshufb	%xmm1, %xmm0, %xmm1
	vpxor	%xmm3, %xmm2, %xmm4
	vpand	%xmm3, %xmm2, %xmm2
	vmovdqa	%xmm0, %xmm3
	vpternlogd	$232, %xmm2, %xmm1, %xmm0
	vpternlogd	$150, %xmm2, %xmm1, %xmm3
	vpor	%xmm0, %xmm4, %xmm1
	vpandn	%xmm3, %xmm1, %xmm1
	vptest	%xmm1, %xmm1
	je	.L26
.L32:
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpsignw	%xmm0, %xmm1, %xmm0
	vpand	%xmm0, %xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L26:
	vmovdqa	%xmm3, %xmm1
	vpternlogq	$64, %xmm0, %xmm4, %xmm1
	vptest	%xmm1, %xmm1
	jne	.L32
	vpor	%xmm3, %xmm4, %xmm1
	vpandn	%xmm0, %xmm1, %xmm1
	vptest	%xmm1, %xmm1
	jne	.L32
	vmovdqa	%xmm0, %xmm1
	vpternlogq	$64, %xmm3, %xmm4, %xmm1
	vptest	%xmm1, %xmm1
	jne	.L32
	vpand	%xmm3, %xmm0, %xmm0
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsignw	%xmm1, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
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
	vpunpckhqdq	%xmm0, %xmm0, %xmm3
	vpsrlq	$32, %xmm0, %xmm5
	vmovdqa	%xmm0, %xmm4
	vpsrlq	$16, %xmm0, %xmm1
	vmovdqa	%xmm0, %xmm2
	vpsrlq	$48, %xmm0, %xmm0
	vpternlogd	$150, %xmm5, %xmm1, %xmm4
	vpternlogd	$232, %xmm5, %xmm1, %xmm2
	vpsrlq	$16, %xmm3, %xmm5
	vmovdqa	%xmm0, %xmm1
	vpternlogd	$150, %xmm5, %xmm3, %xmm1
	vpternlogd	$232, %xmm5, %xmm3, %xmm0
	vmovdqa	%xmm2, %xmm5
	vpand	%xmm4, %xmm1, %xmm3
	vpxor	%xmm4, %xmm1, %xmm1
	vpternlogd	$150, %xmm3, %xmm0, %xmm5
	vpternlogd	$232, %xmm3, %xmm0, %xmm2
	vpunpcklwd	%xmm5, %xmm1, %xmm1
	vpunpckldq	%xmm2, %xmm1, %xmm1
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
	je	.L35
.L34:
	tzcntl	%edx, %edx
	leaq	_ZN4dselL10kValueMaskE(%rip), %rax
	salq	$4, %rdx
	vmovdqa	(%rax,%rdx), %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L35:
	movl	%esi, %ecx
	andl	$511, %edi
	movl	%eax, %r9d
	andl	%eax, %ecx
	andl	$511, %r9d
	andn	%ecx, %edi, %edx
	andn	%ecx, %edi, %ecx
	jne	.L34
	orl	%eax, %r8d
	andl	$511, %r8d
	andn	%edi, %r8d, %edx
	andn	%edi, %r8d, %r8d
	jne	.L34
	andl	%edi, %r9d
	andl	%esi, %edi
	andn	%r9d, %esi, %edx
	andn	%r9d, %esi, %r9d
	cmove	%edi, %edx
	jmp	.L34
	.cfi_endproc
.LFE6475:
	.size	k_D2, .-k_D2
	.section	.text.unlikely
.LCOLDB13:
	.text
.LHOTB13:
	.p2align 4
	.globl	k_E1
	.type	k_E1, @function
k_E1:
.LFB6476:
	.cfi_startproc
	endbr64
	vmovdqa	.LC11(%rip), %xmm1
	vpshufb	%xmm1, %xmm0, %xmm4
	vpand	%xmm4, %xmm0, %xmm9
	vpor	%xmm4, %xmm0, %xmm3
	vpshufb	%xmm1, %xmm4, %xmm4
	vmovdqa	%xmm9, %xmm5
	vpshufb	%xmm1, %xmm4, %xmm7
	vpternlogq	$248, %xmm3, %xmm4, %xmm5
	vpor	%xmm4, %xmm3, %xmm3
	vpshufb	%xmm1, %xmm7, %xmm6
	vmovdqa	%xmm5, %xmm10
	vpshufb	%xmm1, %xmm6, %xmm2
	vpand	%xmm7, %xmm5, %xmm5
	vpternlogq	$248, %xmm3, %xmm7, %xmm10
	vpor	%xmm7, %xmm3, %xmm3
	vpternlogq	$248, %xmm9, %xmm4, %xmm5
	vmovdqa	%xmm10, %xmm8
	vpternlogq	$248, %xmm3, %xmm6, %xmm8
	vpand	%xmm2, %xmm8, %xmm1
	vpternlogq	$224, %xmm3, %xmm6, %xmm2
	vpternlogq	$248, %xmm10, %xmm6, %xmm1
	vpor	%xmm5, %xmm1, %xmm1
	vpternlogq	$14, %xmm2, %xmm8, %xmm1
	vptest	%xmm1, %xmm1
	je	.L37
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpsignw	%xmm0, %xmm1, %xmm0
	vpand	%xmm0, %xmm1, %xmm0
	ret
	.cfi_endproc
	.section	.text.unlikely
	.cfi_startproc
	.type	k_E1.cold, @function
k_E1.cold:
.LFSB6476:
.L37:
	jmp	_ZN4dselL16SelAColdFallbackEDv2_x
	.cfi_endproc
.LFE6476:
	.text
	.size	k_E1, .-k_E1
	.section	.text.unlikely
	.size	k_E1.cold, .-k_E1.cold
.LCOLDE13:
	.text
.LHOTE13:
	.section	.text.unlikely
.LCOLDB14:
	.text
.LHOTB14:
	.p2align 4
	.globl	k_E2
	.type	k_E2, @function
k_E2:
.LFB6477:
	.cfi_startproc
	endbr64
	vmovdqa	.LC11(%rip), %xmm1
	vmovdqa	%xmm0, %xmm3
	vmovdqa	%xmm0, %xmm5
	vpshufb	%xmm1, %xmm0, %xmm2
	vpshufb	%xmm1, %xmm2, %xmm1
	vpternlogd	$150, %xmm1, %xmm2, %xmm3
	vpternlogd	$232, %xmm1, %xmm2, %xmm5
	vmovdqa	.LC12(%rip), %xmm2
	vmovdqa	%xmm3, %xmm4
	vpshufb	%xmm2, %xmm3, %xmm1
	vpshufb	%xmm2, %xmm5, %xmm2
	vpternlogq	$6, %xmm2, %xmm5, %xmm4
	vpor	%xmm5, %xmm2, %xmm2
	vpxor	%xmm1, %xmm3, %xmm1
	vpternlogq	$244, %xmm2, %xmm3, %xmm4
	vpandn	%xmm4, %xmm1, %xmm1
	vptest	%xmm1, %xmm1
	je	.L42
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpsignw	%xmm0, %xmm1, %xmm0
	vpand	%xmm0, %xmm1, %xmm0
	ret
	.cfi_endproc
	.section	.text.unlikely
	.cfi_startproc
	.type	k_E2.cold, @function
k_E2.cold:
.LFSB6477:
.L42:
	jmp	_ZN4dselL16SelAColdFallbackEDv2_x
	.cfi_endproc
.LFE6477:
	.text
	.size	k_E2, .-k_E2
	.section	.text.unlikely
	.size	k_E2.cold, .-k_E2.cold
.LCOLDE14:
	.text
.LHOTE14:
	.p2align 4
	.globl	k_F
	.type	k_F, @function
k_F:
.LFB6478:
	.cfi_startproc
	endbr64
	vpshufb	.LC15(%rip), %xmm0, %xmm1
	vmovdqa	.LC16(%rip), %xmm0
	movabsq	$1085102592571150095, %rax
	vpbroadcastq	%rax, %xmm3
	movl	$-2, %eax
	vgf2p8affineqb	$0, %xmm1, %xmm0, %xmm0
	vmovdqa	.LC18(%rip), %xmm1
	vpsrlw	$4, %xmm0, %xmm2
	vpand	%xmm3, %xmm0, %xmm0
	vpand	%xmm3, %xmm2, %xmm2
	vpshufb	%xmm2, %xmm1, %xmm2
	vpshufb	%xmm0, %xmm1, %xmm1
	vpbroadcastb	%eax, %xmm0
	vpaddb	%xmm2, %xmm1, %xmm1
	vpaddb	%xmm0, %xmm1, %xmm1
	vpmovzxbw	%xmm1, %xmm0
	vpextrb	$8, %xmm1, %ecx
	vphminposuw	%xmm0, %xmm0
	vmovd	%xmm0, %eax
	movzwl	%ax, %edx
	cmpl	%edx, %ecx
	jb	.L48
	shrl	$16, %eax
	leaq	_ZN4dselL10kValueMaskE(%rip), %rdx
	salq	$4, %rax
	vmovdqa	(%rdx,%rax), %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L48:
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
	vmovdqa	.LC11(%rip), %xmm1
	vmovdqa	%xmm0, %xmm3
	vpshufb	%xmm1, %xmm0, %xmm2
	vpshufb	%xmm1, %xmm2, %xmm1
	vpternlogd	$150, %xmm1, %xmm2, %xmm3
	vpternlogd	$232, %xmm1, %xmm2, %xmm0
	vmovdqa	.LC12(%rip), %xmm1
	vpshufb	%xmm1, %xmm3, %xmm2
	vpshufb	%xmm1, %xmm0, %xmm1
	vpxor	%xmm3, %xmm2, %xmm4
	vpand	%xmm3, %xmm2, %xmm2
	vmovdqa	%xmm0, %xmm3
	vpternlogd	$232, %xmm2, %xmm1, %xmm0
	vpternlogd	$150, %xmm2, %xmm1, %xmm3
	vpor	%xmm0, %xmm4, %xmm1
	vpandn	%xmm3, %xmm1, %xmm1
	vptest	%xmm1, %xmm1
	je	.L50
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpsignw	%xmm0, %xmm1, %xmm0
	vpand	%xmm0, %xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L50:
	vpternlogq	$64, %xmm0, %xmm4, %xmm3
	vptest	%xmm3, %xmm3
	je	.L52
	vpcmpeqd	%xmm0, %xmm0, %xmm0
	vpsignw	%xmm0, %xmm3, %xmm0
	vpand	%xmm0, %xmm3, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L52:
	vpcmpeqd	%xmm1, %xmm1, %xmm1
	vpsignw	%xmm1, %xmm0, %xmm1
	vpand	%xmm1, %xmm0, %xmm0
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
.LC9:
	.value	0
	.value	2
	.value	4
	.value	6
	.value	8
	.value	10
	.value	12
	.value	14
	.align 16
.LC11:
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
.LC12:
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
.LC15:
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
.LC16:
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
.LC18:
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
