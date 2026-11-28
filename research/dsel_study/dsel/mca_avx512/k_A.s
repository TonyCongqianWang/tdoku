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
movl	%ecx, %eax
salq	$4, %rax
leaq	_ZN4dselL10kValueMaskE(%rip), %rdx
vmovdqa	(%rdx,%rax), %xmm0
