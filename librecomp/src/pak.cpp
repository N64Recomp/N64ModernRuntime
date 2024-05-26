#include "recomp.h"
#include <ultramodern/ultra64.h>
#include <ultramodern/ultramodern.hpp>
#include "recomp_helpers.h"

extern "C" void osPfsInitPak_recomp(uint8_t * rdram, recomp_context* ctx) {
    PTR(OSMesgQueue) queue = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);
    PTR(OSPfs) pfs = _arg<1, PTR(OSPfs)>(rdram, ctx);
    int channel = _arg<2, int>(rdram, ctx);

    s32 ret = osPfsInitPak(rdram, queue, pfs, channel);

    _return<s32>(ctx, ret);
}

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
