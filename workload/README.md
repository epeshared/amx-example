# AMX Workload Set

- AMX 

- Matrix multipy


matrix_C is mxn metrix

interface:

```C
/**
 * Matrix Multiply
 *
 *  matrix_A x matrix_B = matrix_C
 * @param chgcfg: not 0, call ldtilecfg instruction. otherwize don't call it.
 */
 //void * amx_int8_mul(void* cfg, void *ma, void* mb, int64 a_stride, int64 b_stride);
```
