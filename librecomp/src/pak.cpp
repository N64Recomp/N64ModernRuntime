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
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    PTR(s32) bytes_not_used = _arg<1, PTR(s32)>(rdram, ctx);

    s32 ret = osPfsFreeBlocks(rdram, pfs, bytes_not_used);

    _return<s32>(ctx, ret);
}

extern "C" void osPfsAllocateFile_recomp(uint8_t * rdram, recomp_context * ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    PTR(u8) game_name = _arg<3, PTR(u8)>(rdram, ctx);
    // TODO
    PTR(u8) ext_name = 0; // _arg<4, PTR(u8)>(rdram, ctx);
    int file_size_in_bytes = 0; // _arg<5, int>(rdram, ctx);
    PTR(s32) file_no = 0; // _arg<6, s32>(rdram, ctx);

    s32 ret = osPfsAllocateFile(rdram, pfs, company_code, game_code, game_name, ext_name, file_size_in_bytes, file_no);

    _return<s32>(ctx, ret);
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
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);

    s32 ret = osPfsChecker(rdram, pfs);

    _return<s32>(ctx, ret);
}
