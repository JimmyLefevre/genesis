global _f_round_s

section .text

_f_round_s:
cvtps2dq xmm0, xmm0
movd     eax, xmm0
ret
