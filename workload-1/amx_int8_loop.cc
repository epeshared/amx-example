#include <string.h>
#include <iostream>
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

extern "C" unsigned long long amx_int8(void*cfg, unsigned long long );
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
    char * cfg = new char[64];
    unsigned long long loop = -1;

    int cr0 = get_amx_xcr_reg();
    std::cout << "CR0: 0x" << std::hex << cr0 << std::endl;
    enable_amx();

    memset(cfg, 0, 64);
    cfg[0] = 1;
    for (auto i = 0; i < 8; i++) {
        cfg[16+i*2] = 64;
        cfg[48+i] = 16;
    }

    auto r = amx_int8((void*) cfg, loop);
    std::cout << "RET: " << r << std::endl;

    delete []cfg;
    return 0;
}

