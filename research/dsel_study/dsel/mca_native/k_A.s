endbr64
movabsq	$36029346783166592, %rax
pushq	%r15
movabsq	$4503668347895824, %rsi
movabsq	$562958543486978, %r15
vmovq	%rax, %xmm1
pushq	%r14
movabsq	$1125917086973956, %r14
movabsq	$281479271743489, %r10
vpunpcklqdq	%xmm1, %xmm1, %xmm1
pushq	%r13
vpand	%xmm1, %xmm0, %xmm1
pushq	%r12
pushq	%rbp
vmovq	%xmm1, %rax
vpextrq	$1, %xmm1, %rcx
pushq	%rbx
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
movl	%ecx, %eax
salq	$4, %rax
leaq	_ZN4dselL10kValueMaskE(%rip), %rdx
popq	%rbx
popq	%rbp
vmovdqa	(%rdx,%rax), %xmm0
popq	%r12
popq	%r13
popq	%r14
popq	%r15
