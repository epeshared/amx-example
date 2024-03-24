#include <string.h>
#include <unistd.h>

#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iomanip>

#define u64 unsigned long long
#define u8  unsigned char

extern "C" u64* amx_int8_mul(u64* cfg, u8* ma, u8* mb, u64 stridea, u64 strideb, u8* mc);
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
    char* cfg = new char[64];
    u8* ma = (u8*)new char[1024];
    u8* mb = (u8*)new char[1024];
    u8* mc = (u8*)new char[1024];
    u8* mb_temp = (u8*)new char[1024];

    int m = 8;
    int n = 8;
    int k = 64;

    if (k%4 || m%4) {
        std::cerr << "M and K 不是4的倍数，可能算法有问题。 " << std::endl;
        exit(1);
    }

    int cr0 = get_amx_xcr_reg();
    //std::cout << "CR0: 0x" << std::hex << cr0 << std::endl;
    if (!enable_amx()){
        std::cerr << "Kernel doesn't support AMX! Exit..." << std::endl;
        exit(1);
    }

    std::cout << "Matrix(int8) multiply" << std::endl;
    std::cout << "Matrix A: row: " << m << ", column: " << k << std::endl;
    std::cout << "Matrix B: row: " << k << ", column: " << n << std::endl;
    std::cout << "Expected Result Matrix: row: " << m << ", column: " << n << std::endl;

    unsigned long long loop = -1;

    std::srand(std::time(0));

    memset(cfg, 0, 64);
    memset(mc, 0, 1024);
    memset(mb_temp, 0, 1024);
    for (auto i = 0; i < 1024; i++) {
        ma[i] = (u8)std::rand();
        mb[i] = (u8)std::rand();
    }

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
    cfg[16] = k;  // col->K
    cfg[48] = m;  // row->M

    // matrix B need a layout rearragement
    cfg[16+1*2] = n*4;   // col = N*4
    cfg[48+1]   = k/4;   // row = K/4

    cfg[16+2*2] = n*4; // N*sizeof(int32)
    cfg[48+2] = m;     // M

    //
    // Relayout matrix B
    
    for (auto i = 0; i < k/4; i++) {
        for (auto j = 0; j < n; j++) {
            for (auto a = 0; a < 4; a++) {
                mb_temp[i*(n*4) + j*4 + a] = mb[(i*4+a)*n+j];
            }
        }
    }

    amx_int8_mul((u64*)cfg, ma, mb_temp, k, n*4, mc);
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
    delete []ma;
    delete []mb;
    delete []mb_temp;
    delete []mc;
    return 0;
}

