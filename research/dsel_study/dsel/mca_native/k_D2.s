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
tzcntl	%edx, %edx
leaq	_ZN4dselL10kValueMaskE(%rip), %rax
salq	$4, %rdx
vmovdqa	(%rax,%rdx), %xmm0
