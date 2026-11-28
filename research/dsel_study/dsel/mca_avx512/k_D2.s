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
tzcntl	%edx, %edx
leaq	_ZN4dselL10kValueMaskE(%rip), %rax
salq	$4, %rdx
vmovdqa	(%rax,%rdx), %xmm0
