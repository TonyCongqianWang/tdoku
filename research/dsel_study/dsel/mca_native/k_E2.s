endbr64
vmovdqa	.LC10(%rip), %xmm4
vmovdqa	%xmm0, %xmm2
vpshufb	%xmm4, %xmm0, %xmm3
vpshufb	%xmm4, %xmm3, %xmm4
vpxor	%xmm3, %xmm0, %xmm1
vpand	%xmm3, %xmm2, %xmm3
vpxor	%xmm4, %xmm1, %xmm0
vpand	%xmm4, %xmm1, %xmm1
vpor	%xmm3, %xmm1, %xmm1
vmovdqa	.LC11(%rip), %xmm3
vpshufb	%xmm3, %xmm0, %xmm5
vpshufb	%xmm3, %xmm1, %xmm3
vpxor	%xmm3, %xmm1, %xmm4
vpor	%xmm3, %xmm1, %xmm1
vpandn	%xmm4, %xmm0, %xmm4
vpandn	%xmm0, %xmm1, %xmm1
vpxor	%xmm5, %xmm0, %xmm0
vpor	%xmm4, %xmm1, %xmm1
vpandn	%xmm1, %xmm0, %xmm0
vptest	%xmm0, %xmm0
je	.L43
vpcmpeqd	%xmm1, %xmm1, %xmm1
vpsignw	%xmm1, %xmm0, %xmm1
vpand	%xmm1, %xmm0, %xmm0
