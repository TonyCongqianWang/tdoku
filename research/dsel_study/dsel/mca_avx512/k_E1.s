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
