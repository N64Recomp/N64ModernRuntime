#include "ultramodern/ultramodern.hpp"

#include "helpers.hpp"

extern "C" void osPfsInitPak_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSMesgQueue) mq = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);
    PTR(OSPfs) pfs = _arg<1, PTR(OSPfs)>(rdram, ctx);
    int channel = _arg<2, int>(rdram, ctx);

    s32 ret = osPfsInitPak(PASS_RDRAM mq, pfs, channel);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsRepairId_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);

    s32 ret = osPfsRepairId(PASS_RDRAM pfs);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsInit_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSMesgQueue) mq = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);
    PTR(OSPfs) pfs = _arg<1, PTR(OSPfs)>(rdram, ctx);
    int channel = _arg<2, int>(rdram, ctx);

    s32 ret = osPfsInit(PASS_RDRAM mq, pfs, channel);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsReFormat_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    PTR(OSMesgQueue) mq = _arg<1, PTR(OSMesgQueue)>(rdram, ctx);
    int channel = _arg<2, int>(rdram, ctx);

    s32 ret = osPfsReFormat(PASS_RDRAM pfs, mq, channel);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsChecker_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);

    s32 ret = osPfsChecker(PASS_RDRAM pfs);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsAllocateFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    PTR(u8) game_name = _arg<3, PTR(u8)>(rdram, ctx);
    PTR(u8) ext_name = _arg<4, PTR(u8)>(rdram, ctx);
    int nbytes = _arg<5, int>(rdram, ctx);
    PTR(s32) file_no = _arg<6, PTR(s32)>(rdram, ctx);
    u8 game_name_proxy[PFS_FILE_NAME_LEN];
    u8 ext_name_proxy[PFS_FILE_EXT_LEN];

    for (uint32_t i = 0; i < PFS_FILE_NAME_LEN; i++) {
        game_name_proxy[i] = MEM_B(i, (gpr)game_name);
    }
    for (uint32_t i = 0; i < PFS_FILE_EXT_LEN; i++) {
        ext_name_proxy[i] = MEM_B(i, (gpr)ext_name);
    }
    s32 ret = osPfsAllocateFile(PASS_RDRAM pfs, company_code, game_code, game_name_proxy, ext_name_proxy, nbytes, file_no);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsFindFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    PTR(u8) game_name = _arg<3, PTR(u8)>(rdram, ctx);
    PTR(u8) ext_name = _arg<4, PTR(u8)>(rdram, ctx);
    PTR(s32) file_no = _arg<5, PTR(s32)>(rdram, ctx);
    u8 game_name_proxy[PFS_FILE_NAME_LEN];
    u8 ext_name_proxy[PFS_FILE_EXT_LEN];

    for (uint32_t i = 0; i < PFS_FILE_NAME_LEN; i++) {
        game_name_proxy[i] = MEM_B(i, (gpr)game_name);
    }
    for (uint32_t i = 0; i < PFS_FILE_EXT_LEN; i++) {
        ext_name_proxy[i] = MEM_B(i, (gpr)ext_name);
    }
    s32 ret = osPfsFindFile(PASS_RDRAM pfs, company_code, game_code, game_name_proxy, ext_name_proxy, file_no);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsDeleteFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    PTR(u8) game_name = _arg<3, PTR(u8)>(rdram, ctx);
    PTR(u8) ext_name = _arg<4, PTR(u8)>(rdram, ctx);
    u8 game_name_proxy[PFS_FILE_NAME_LEN];
    u8 ext_name_proxy[PFS_FILE_EXT_LEN];

    for (uint32_t i = 0; i < PFS_FILE_NAME_LEN; i++) {
        game_name_proxy[i] = MEM_B(i, (gpr)game_name);
    }
    for (uint32_t i = 0; i < PFS_FILE_EXT_LEN; i++) {
        ext_name_proxy[i] = MEM_B(i, (gpr)ext_name);
    }
    s32 ret = osPfsDeleteFile(PASS_RDRAM pfs, company_code, game_code, game_name_proxy, ext_name_proxy);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsReadWriteFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    s32 file_no = _arg<1, s32>(rdram, ctx);
    u8 flag = _arg<2, u8>(rdram, ctx);
    int offset = _arg<3, int>(rdram, ctx);
    int nbytes = _arg<4, int>(rdram, ctx);
    PTR(u8) data_buffer = _arg<5, PTR(u8)>(rdram, ctx);
    std::vector<u8> data_buffer_proxy(nbytes);

    if (flag == PFS_WRITE) {
        for (uint32_t i = 0; i < nbytes; i++) {
            data_buffer_proxy[i] = MEM_B(i, (gpr)data_buffer);
        }
    }
    s32 ret = osPfsReadWriteFile(PASS_RDRAM pfs, file_no, flag, offset, nbytes, data_buffer_proxy.data());
    if (flag == PFS_READ) {
        for (uint32_t i = 0; i < nbytes; i++) {
            MEM_B(i, (gpr)data_buffer) = data_buffer_proxy[i];
        }
    }
    _return<s32>(ctx, ret);
}

extern "C" void osPfsFileState_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    s32 file_no = _arg<1, s32>(rdram, ctx);
    PTR(OSPfsState) state = _arg<2, PTR(OSPfsState)>(rdram, ctx);

    s32 ret = osPfsFileState(PASS_RDRAM pfs, file_no, state);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsGetLabel_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    PTR(u8) label = _arg<1, PTR(u8)>(rdram, ctx);
    PTR(int) len = _arg<2, PTR(int)>(rdram, ctx);
    u8 label_proxy[32];

    s32 ret = osPfsGetLabel(PASS_RDRAM pfs, label_proxy, len);
    for (uint32_t i = 0; i < 32; i++) {
        MEM_B(i, (gpr)label) = label_proxy[i];
    }
    _return<s32>(ctx, ret);
}

extern "C" void osPfsSetLabel_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    PTR(u8) label = _arg<1, PTR(u8)>(rdram, ctx);
    u8 label_proxy[32];

    for (uint32_t i = 0; i < 32; i++) {
        label_proxy[i] = MEM_B(i, (gpr)label);
    }
    s32 ret = osPfsSetLabel(PASS_RDRAM pfs, label_proxy);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsIsPlug_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSMesgQueue) mq = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);
    PTR(u8) pattern = _arg<1, PTR(u8)>(rdram, ctx);
    u8 pattern_proxy = 0;

    s32 ret = osPfsIsPlug(PASS_RDRAM mq, &pattern_proxy);
    MEM_B(0, (gpr)pattern) = pattern_proxy;
    _return<s32>(ctx, ret);
}

extern "C" void osPfsFreeBlocks_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    PTR(s32) bytes_not_used = _arg<1, PTR(s32)>(rdram, ctx);

    s32 ret = osPfsFreeBlocks(PASS_RDRAM pfs, bytes_not_used);
    _return<s32>(ctx, ret);
}

extern "C" void osPfsNumFiles_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSPfs) pfs = _arg<0, PTR(OSPfs)>(rdram, ctx);
    PTR(s32) max_files = _arg<1, PTR(s32)>(rdram, ctx);
    PTR(s32) files_used = _arg<2, PTR(s32)>(rdram, ctx);

    s32 ret = osPfsNumFiles(PASS_RDRAM pfs, max_files, files_used);
    _return<s32>(ctx, ret);
}

