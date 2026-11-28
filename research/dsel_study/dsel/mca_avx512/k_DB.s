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
