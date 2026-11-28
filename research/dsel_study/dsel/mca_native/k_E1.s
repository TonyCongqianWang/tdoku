endbr64
vmovdqa	.LC10(%rip), %xmm4
vmovdqa	%xmm0, %xmm5
vpshufb	%xmm4, %xmm0, %xmm2
vpand	%xmm2, %xmm0, %xmm3
vpor	%xmm2, %xmm0, %xmm1
vpshufb	%xmm4, %xmm2, %xmm2
vpshufb	%xmm4, %xmm2, %xmm9
vpand	%xmm2, %xmm1, %xmm6
vpor	%xmm2, %xmm1, %xmm1
vpshufb	%xmm4, %xmm9, %xmm8
vpor	%xmm3, %xmm6, %xmm6
vpand	%xmm9, %xmm1, %xmm0
vpor	%xmm9, %xmm1, %xmm1
vpor	%xmm6, %xmm0, %xmm0
vpshufb	%xmm4, %xmm8, %xmm4
vpand	%xmm8, %xmm1, %xmm7
vpand	%xmm2, %xmm3, %xmm3
vpand	%xmm9, %xmm6, %xmm6
vpor	%xmm0, %xmm7, %xmm7
vpor	%xmm8, %xmm1, %xmm1
vpand	%xmm8, %xmm0, %xmm0
vpand	%xmm4, %xmm7, %xmm10
vpor	%xmm6, %xmm3, %xmm3
vpand	%xmm4, %xmm1, %xmm1
vpor	%xmm10, %xmm0, %xmm0
vpor	%xmm7, %xmm1, %xmm1
vpor	%xmm3, %xmm0, %xmm0
vpandn	%xmm1, %xmm0, %xmm0
vptest	%xmm0, %xmm0
je	.L38
vpcmpeqd	%xmm1, %xmm1, %xmm1
vpsignw	%xmm1, %xmm0, %xmm1
vpand	%xmm1, %xmm0, %xmm0
