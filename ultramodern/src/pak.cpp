#include "ultramodern/ultra64.h"


extern "C" s32 osPfsInitPak(RDRAM_ARG PTR(OSMesgQueue) queue, PTR(OSPfs) pfs, int channel) {
    return PFS_ERR_NOPACK;
}

// osPfsRepairId
// osPfsInit
// osPfsReFormat

extern "C" s32 osPfsChecker(RDRAM_ARG PTR(OSPfs) pfs) {
    return PFS_ERR_NOPACK;
}

extern "C" s32 osPfsAllocateFile(RDRAM_ARG PTR(OSPfs) pfs, u16 company_code, u32 game_code, PTR(u8) game_name, PTR(u8) ext_name, int file_size_in_bytes, PTR(s32) file_no) {
    return PFS_ERR_NOPACK;
}

extern "C" s32 osPfsFindFile(RDRAM_ARG PTR(OSPfs) pfs, u16 company_code, u32 game_code, PTR(u8) game_name, PTR(u8) ext_name, PTR(s32) file_no) {
    return PFS_ERR_NOPACK;
}

extern "C" s32 osPfsDeleteFile(RDRAM_ARG PTR(OSPfs) pfs, u16 company_code, u32 game_code, PTR(u8) game_name, PTR(u8) ext_name) {
    return PFS_ERR_NOPACK;
}

extern "C" s32 osPfsReadWriteFile(RDRAM_ARG PTR(OSPfs) pfs, s32 file_no, u8 flag, int offset, int size_in_bytes, PTR(u8) data_buffer) {
    return PFS_ERR_NOPACK;
}

extern "C" s32 osPfsFileState(RDRAM_ARG PTR(OSPfs) pfs, s32 file_no, PTR(OSPfsState) state) {
    return PFS_ERR_NOPACK;
}

// osPfsGetLabel
// osPfsSetLabel
// osPfsIsPlug

extern "C" s32 osPfsFreeBlocks(RDRAM_ARG PTR(OSPfs) pfs, PTR(s32) bytes_not_used) {
    return PFS_ERR_NOPACK;
}

// osPfsNumFiles
