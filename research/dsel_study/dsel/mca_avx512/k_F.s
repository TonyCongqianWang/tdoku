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
