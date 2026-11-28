endbr64
vmovdqa	.LC10(%rip), %xmm2
vpshufb	%xmm2, %xmm0, %xmm1
vpand	%xmm1, %xmm0, %xmm4
vpor	%xmm1, %xmm0, %xmm0
vpshufb	%xmm2, %xmm1, %xmm1
vpand	%xmm1, %xmm4, %xmm6
vpand	%xmm1, %xmm0, %xmm3
vpor	%xmm1, %xmm0, %xmm0
vpshufb	%xmm2, %xmm1, %xmm1
vpor	%xmm4, %xmm3, %xmm3
vpand	%xmm1, %xmm0, %xmm5
vpand	%xmm1, %xmm3, %xmm7
vpor	%xmm1, %xmm0, %xmm0
vpor	%xmm3, %xmm5, %xmm5
vpshufb	%xmm2, %xmm1, %xmm3
vpor	%xmm6, %xmm7, %xmm7
vpand	%xmm3, %xmm0, %xmm4
vpand	%xmm3, %xmm5, %xmm8
vpshufb	%xmm2, %xmm3, %xmm2
vpor	%xmm5, %xmm4, %xmm4
vpor	%xmm3, %xmm0, %xmm0
vpor	%xmm7, %xmm8, %xmm8
vpand	%xmm2, %xmm4, %xmm5
vpand	%xmm2, %xmm0, %xmm0
vpor	%xmm8, %xmm5, %xmm5
vpor	%xmm4, %xmm0, %xmm0
vpandn	%xmm0, %xmm5, %xmm0
vptest	%xmm0, %xmm0
je	.L20
vpcmpeqd	%xmm1, %xmm1, %xmm1
vpsignw	%xmm1, %xmm0, %xmm1
vpand	%xmm1, %xmm0, %xmm0
