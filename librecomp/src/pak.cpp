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
    PTR(u8) ext_name = _arg<4, PTR(u8)>(rdram, ctx);
    int file_size_in_bytes = _arg<5, int>(rdram, ctx);
    PTR(s32) file_no = _arg<6, PTR(s32)>(rdram, ctx);

    s32 ret = osPfsAllocateFile(rdram, pfs, company_code, game_code, game_name, ext_name, file_size_in_bytes, file_no);

    _return<s32>(ctx, ret);
}

extern "C" void osPfsDeleteFile_recomp(uint8_t * rdram, recomp_context * ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    PTR(u8) game_name = _arg<3, PTR(u8)>(rdram, ctx);
    PTR(u8) ext_name = _arg<4, PTR(u8)>(rdram, ctx);

    s32 ret = osPfsDeleteFile(rdram, pfs, company_code, game_code, game_name, ext_name);

    _return<s32>(ctx, ret);
}

extern "C" void osPfsFileState_recomp(uint8_t * rdram, recomp_context * ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    s32 file_no = _arg<1, s32>(rdram, ctx);
    PTR(OSPfsState) state = _arg<2, PTR(OSPfsState)>(rdram, ctx);

    s32 ret = osPfsFileState(rdram, pfs, file_no, state);

    _return<s32>(ctx, ret);
}

extern "C" void osPfsFindFile_recomp(uint8_t * rdram, recomp_context * ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    PTR(u8) game_name = _arg<3, PTR(u8)>(rdram, ctx);
    PTR(u8) ext_name = _arg<4, PTR(u8)>(rdram, ctx);
    PTR(s32) file_no = _arg<5, PTR(s32)>(rdram, ctx);

    s32 ret = osPfsFindFile(rdram, pfs, company_code, game_code, game_name, ext_name, file_no);

    _return<s32>(ctx, ret);
}

extern "C" void osPfsReadWriteFile_recomp(uint8_t * rdram, recomp_context * ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    s32 file_no = _arg<1, s32>(rdram, ctx);
    u8 flag = _arg<2, u8>(rdram, ctx);
    int offset = _arg<3, int>(rdram, ctx);
    int size_in_bytes = _arg<4, int>(rdram, ctx);
    PTR(u8) data_buffer = _arg<5, PTR(u8)>(rdram, ctx);

    s32 ret = osPfsReadWriteFile(rdram, pfs, file_no, flag, offset, size_in_bytes, data_buffer);

    _return<s32>(ctx, ret);
}

extern "C" void osPfsChecker_recomp(uint8_t * rdram, recomp_context * ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);

    s32 ret = osPfsChecker(rdram, pfs);

    _return<s32>(ctx, ret);
}
