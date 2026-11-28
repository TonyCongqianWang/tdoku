endbr64
vmovdqa	.LC10(%rip), %xmm3
vpshufb	%xmm3, %xmm0, %xmm4
vpshufb	%xmm3, %xmm4, %xmm3
vpxor	%xmm4, %xmm0, %xmm2
vpand	%xmm4, %xmm0, %xmm0
vpxor	%xmm3, %xmm2, %xmm1
vpand	%xmm3, %xmm2, %xmm2
vpor	%xmm0, %xmm2, %xmm0
vmovdqa	.LC11(%rip), %xmm2
vpshufb	%xmm2, %xmm1, %xmm3
vpshufb	%xmm2, %xmm0, %xmm2
vpxor	%xmm3, %xmm1, %xmm4
vpand	%xmm3, %xmm1, %xmm1
vpxor	%xmm2, %xmm0, %xmm3
vpxor	%xmm3, %xmm1, %xmm5
vpand	%xmm2, %xmm0, %xmm0
vpand	%xmm3, %xmm1, %xmm1
vpor	%xmm0, %xmm1, %xmm1
vpor	%xmm1, %xmm4, %xmm0
vpandn	%xmm5, %xmm0, %xmm0
vptest	%xmm0, %xmm0
je	.L51
vpcmpeqd	%xmm1, %xmm1, %xmm1
vpsignw	%xmm1, %xmm0, %xmm1
vpand	%xmm1, %xmm0, %xmm0
