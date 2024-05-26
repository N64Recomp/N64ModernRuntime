#include "ultramodern/ultra64.h"


extern "C" s32 osPfsInitPak(RDRAM_ARG PTR(OSMesgQueue) queue, PTR(OSPfs) pfs, int channel) {
    return PFS_ERR_NOPACK;
}

#if 0
extern "C" void osPfsFreeBlocks_recomp(uint8_t * rdram, recomp_context * ctx) {
    ctx->r2 = 1; // PFS_ERR_NOPACK
}

extern "C" void osPfsAllocateFile_recomp(uint8_t * rdram, recomp_context * ctx) {
    ctx->r2 = 1; // PFS_ERR_NOPACK
}

extern "C" void osPfsDeleteFile_recomp(uint8_t * rdram, recomp_context * ctx) {
    ctx->r2 = 1; // PFS_ERR_NOPACK
}

extern "C" void osPfsFileState_recomp(uint8_t * rdram, recomp_context * ctx) {
    ctx->r2 = 1; // PFS_ERR_NOPACK
}

extern "C" void osPfsFindFile_recomp(uint8_t * rdram, recomp_context * ctx) {
    ctx->r2 = 1; // PFS_ERR_NOPACK
}

extern "C" void osPfsReadWriteFile_recomp(uint8_t * rdram, recomp_context * ctx) {
    ctx->r2 = 1; // PFS_ERR_NOPACK
}

extern "C" void osPfsChecker_recomp(uint8_t * rdram, recomp_context * ctx) {
    ctx->r2 = 1; // PFS_ERR_NOPACK
}
#endif
