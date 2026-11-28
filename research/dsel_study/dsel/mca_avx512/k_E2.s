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
