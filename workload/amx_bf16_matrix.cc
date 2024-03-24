#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iomanip>

#include "bfloat16.h"
#include <random>

#define u64 unsigned long long
#define u8  unsigned char

extern "C" u64* amx_bf16_mul(u64* cfg, u8* ma, u8* mb, u64 stridea, u64 strideb, u8* mc);
extern "C" int get_amx_xcr_reg();

#define XFEATURE_XTILECFG           17
#define XFEATURE_XTILEDATA          18
#define XFEATURE_MASK_XTILECFG      (1 << XFEATURE_XTILECFG)
#define XFEATURE_MASK_XTILEDATA     (1 << XFEATURE_XTILEDATA)
#define XFEATURE_MASK_XTILE         (XFEATURE_MASK_XTILECFG | XFEATURE_MASK_XTILEDATA)
#define ARCH_GET_XCOMP_PERM         0x1022
#define ARCH_REQ_XCOMP_PERM         0x1023

int enable_amx() {
    unsigned long bitmask = 0;
    long status = syscall(SYS_arch_prctl, ARCH_GET_XCOMP_PERM, &bitmask);
    if (0 != status) {
        std::cout << "SYS_arch_prctl(READ) error" << std::endl;
        return 0;
    }
    if (bitmask & XFEATURE_MASK_XTILEDATA) {
        return 1;
    }
    status = syscall(SYS_arch_prctl, ARCH_REQ_XCOMP_PERM, XFEATURE_XTILEDATA);
    if (0 != status) {
        std::cout << "SYS_arch_prctl(WRITE) error" << std::endl;
        return 0;
    }
    status = syscall(SYS_arch_prctl, ARCH_GET_XCOMP_PERM, &bitmask);
    if (0 != status || !(bitmask & XFEATURE_MASK_XTILEDATA)) {
        std::cout << "SYS_arch_prctl(READ) error" << std::endl;
        return 0;
    }
    return 1;
}

int main(){
    int DIM = 8;
    int MAX_LEN_BYTES = DIM * sizeof(float);
    int m = 1;
    int n = 1;
    int k = DIM * 4;    

    char* cfg = new char[64];
    float ma[DIM];
    float mb[DIM];
    u8 mc[DIM*4];    


    // if (k%4 || m%4) {
    //     std::cerr << "M and K 不是4的倍数，可能算法有问题。 " << std::endl;
    //     exit(1);
    // }

    int cr0 = get_amx_xcr_reg();
    //std::cout << "CR0: 0x" << std::hex << cr0 << std::endl;
    if (!enable_amx()){
        std::cerr << "Kernel doesn't support AMX! Exit..." << std::endl;
        exit(1);
    }

    std::cout << "Matrix(bf16) multiply" << std::endl;
    std::cout << "Matrix A: row: " << m << ", column: " << k << std::endl;
    std::cout << "Matrix B: row: " << k << ", column: " << n << std::endl;
    std::cout << "Expected Result Matrix: row: " << m << ", column: " << n << std::endl;

    unsigned long long loop = -1;

    std::srand(std::time(0));

    memset(cfg, 0, 64);
    memset(mc, 0, MAX_LEN_BYTES);    

    std::random_device rd; // 用于获得随机数种子
    std::mt19937 gen(rd()); // 以 rd() 作为种子的 Mersenne Twister 生成器
    std::uniform_real_distribution<> dis(0.0, 1.0); // 定义一个分布在 [0.0, 1.0] 的浮点数

    for (auto i = 0; i < DIM; i++) {
        ma[i] = (float)dis(gen);;
        mb[i] = (float)dis(gen);
    }

    // bfloat16_t *src1_f16 = new bfloat16_t[DIM];
    // bfloat16_t *src2_f16 = new bfloat16_t[DIM];

    u8 *ma_bf16 = new u8[DIM*2*8];
    u8 *mb_bf16 = new u8[DIM*2*8];     

    cvt_float_to_bfloat16(ma,(uint8_t*)ma_bf16, DIM*2*8);
    cvt_float_to_bfloat16(mb,(uint8_t*)mb_bf16, DIM*2*8);

    //
    // Calculating the result should be delivered:
    // Suppose Matrix A and B are storing the first row in first bytes of
    // 'ma' and 'mb' in a sequence.
    for (auto i = 0; i < m; i++){
        for (auto j = 0; j < n; j++) {
            int sum = 0;
            for (auto l = 0; l < k; l++) {
                sum += ma[i*k+l] * mb[j+l*n];
            }
            std::cout << std::setw(10) << std::hex << sum ;
        }
        std::cout << std::endl;
    }

    cfg[0] = 1;
    cfg[16] = k*2;  // col->K
    cfg[48] = m;  // row->M

    // matrix B need a layout rearragement
    cfg[16+1*2] = n*2*2;   // col = N*4
    cfg[48+1]   = k/2;   // row = K/4

    cfg[16+2*2] = (n*4); // N*sizeof(int32)
    cfg[48+2] = m;     // M

    //
    // Relayout matrix B
    u8 mb_temp[DIM*2*8];
    memset(mb_temp, 0, DIM*2*8);
    for (auto i = 0; i < k/4; i++) {
        for (auto j = 0; j < n; j++) {
            for (auto a = 0; a < 4; a++) {
                mb_temp[i*(n*4) + j*4 + a] = mb_bf16[(i*4+a)*n+j];
            }
        }
    }

    amx_bf16_mul((u64*)cfg, ma_bf16, mb_temp, k, n*4, mc);
    int*addr = (int*)mc;

    std::cout << std::endl << "Result Matrix: " << std::endl;
    std::cout << "------------------------------------------------------" << std::endl;
    for (auto i = 0; i < 1024/4; i++){
        std::cout << std::setw(10) << std::hex << *(addr+i);
        if ((1+i)%16 == 0)
            std::cout << std::endl;
    }
    std::cout << "------------------------------------------------------" << std::endl;

    delete []cfg;
    // delete []ma;
    // delete []mb;
    // delete []mb_temp;
    // delete []mc;
    return 0;
}

