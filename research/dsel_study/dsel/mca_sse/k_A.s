endbr64
movdqa	%xmm0, %xmm1
pand	.LC1(%rip), %xmm0
pushq	%r15
pushq	%r14
movq	%xmm0, %rax
pextrq	$1, %xmm0, %rdi
pushq	%r13
movdqa	.LC2(%rip), %xmm0
pushq	%r12
popcntq	%rax, %rax
popcntq	%rdi, %rdi
pand	%xmm1, %xmm0
pushq	%rbp
movq	%xmm0, %r11
pextrq	$1, %xmm0, %rcx
pushq	%rbx
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
movl	%ecx, %eax
salq	$4, %rax
leaq	_ZN4dselL10kValueMaskE(%rip), %rdx
popq	%rbx
popq	%rbp
movdqa	(%rdx,%rax), %xmm0
popq	%r12
popq	%r13
popq	%r14
popq	%r15
