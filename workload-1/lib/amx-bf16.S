	.intel_syntax noprefix

	.text
        //
        // function prototype:
        // unsigned long long test_amx_bf16(void* tilecfg, int loops)
	.globl	amx_bf16
amx_bf16:
        ldtilecfg [rdi]
        test    rsi, rsi
        tilezero tmm0
        tilezero tmm1
        tilezero tmm2
        jz      _amx_bf16_exit
_amx_bf16_loop:
        tdpbssd tmm0, tmm1, tmm2
        //tdpbf16ps tmm0, tmm1, tmm2
        sub     rsi, 0x1
        jnz     _amx_bf16_loop
_amx_bf16_exit:
	mov	rax, QWORD PTR[rdi]
        tilerelease
	ret
	.size	amx_bf16, .-amx_bf16

	.global get_amx_xcr_reg
	// int get_amx_xcr_reg();
get_amx_xcr_reg:
	push rcx
	push rdx
	xor  rcx, rcx
	xgetbv
	pop  rdx
	pop  rcx
	ret

	.globl	amx_bf16_mul
        //void * amx_bf16_mul(void* cfg, void *ma, void* mb, int64 a_stride, int64 b_stride, void* mc);
        // if cfg is 0, no TILE config will be load
        // args regs: rdi, rsi, rdx, rcx, r8, r9
amx_bf16_mul:
        test rdi, rdi
        jz _amx_bf16_mul_skip_cfg
        ldtilecfg [rdi]
_amx_bf16_mul_skip_cfg:
        tileloadd tmm0, [rsi+rcx*1]
        tileloadd tmm1, [rdx+r8*1]
        tdpbf16ps tmm2, tmm0, tmm1
        tilestored [r9+r8*1], tmm2
        mov       rax, r9
        tilerelease
	ret
	.size	amx_bf16_mul, .-amx_bf16_mul


