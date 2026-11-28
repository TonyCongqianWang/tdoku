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
vpcmpeqd	%xmm1, %xmm1, %xmm1
vpsignw	%xmm1, %xmm0, %xmm1
vpand	%xmm1, %xmm0, %xmm0
