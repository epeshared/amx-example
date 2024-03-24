#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include <iostream>

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

int main(int argc, char* argv[]) {
    char cfg[64];
    unsigned long loops = 0;
    unsigned long i = 0;
    struct timeval begin;
    struct timeval end;
    unsigned long duration = 0;

    #define INIT_LOOPS (1L * 1024L * 1024L)
    loops = INIT_LOOPS;

    std::cout << "AMX Tile Config Release Bench" << std::endl;

    if (!enable_amx()) {
        return -1;
    }

    memset(cfg, 0, 64);
    cfg[0] = 1;
    //TODO: will the maxtrix shape affect the configuration speed
    // cfg -- column and row number tests...

    gettimeofday(&begin, NULL);
    for (i = 0; i < loops/4; i++) {
        // un-rolling?
        asm("ldtilecfg (%0)" : : "r"(cfg));
        asm("tilerelease");
        asm("ldtilecfg (%0)" : : "r"(cfg));
        asm("tilerelease");
        asm("ldtilecfg (%0)" : : "r"(cfg));
        asm("tilerelease");
        asm("ldtilecfg (%0)" : : "r"(cfg));
        asm("tilerelease");
    }
    gettimeofday(&end, NULL);
    duration =
        (end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec;

    loops = (loops * 5 * 1000000) / duration;
    gettimeofday(&begin, NULL);
    for (i = 0; i < loops/4; i++) {
        // un-rolling?
        asm("ldtilecfg (%0)" : : "r"(cfg));
        asm("tilerelease");
        asm("ldtilecfg (%0)" : : "r"(cfg));
        asm("tilerelease");
        asm("ldtilecfg (%0)" : : "r"(cfg));
        asm("tilerelease");
        asm("ldtilecfg (%0)" : : "r"(cfg));
        asm("tilerelease");
    }
    gettimeofday(&end, NULL);
    duration =
        (end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec;

    std::cout << "loops" << loops << " - duration " << duration << std::endl;
    std::cout << "Tile cfg+release pair throughput: "
              << loops / duration << " per us" << std::endl;
    return 0;
}