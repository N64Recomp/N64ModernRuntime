#include <filesystem>
#include <fstream>
#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"

#include "recomp.h"
#include "helpers.hpp"

#define MAX_FILES 16
typedef struct ControllerPak {
    OSPfsState state;
    std::fstream file;
    std::fstream header;
} ControllerPak;

static int sFileSelect = 0;

void ByteSwapFile(u8* buffer, size_t size) {
    uint8_t c0, c1, c2, c3;

    for (size_t i = 0; i < size; i += 4) {
        c0 = buffer[i + 0];
        c1 = buffer[i + 1];
        c2 = buffer[i + 2];
        c3 = buffer[i + 3];

        buffer[i + 3] = c0;
        buffer[i + 2] = c1;
        buffer[i + 1] = c2;
        buffer[i + 0] = c3;
    }
}

void Pak_WriteHeader(OSPfsState state, s32 file_no) {
}

// s32 osPfsIsPlug(OSMesgQueue* mq, u8* pattern)
extern "C" void osPfsIsPlug_recomp(uint8_t* rdram, recomp_context* ctx) {
    MEM_B(0, ctx->r5) = 1; // *pattern = 1;
    ctx->r2 = 0;           // PFS_NO_ERROR
}

// s32 osPfsInit(OSMesgQueue* queue, OSPfs* pfs, int channel) {
extern "C" void osPfsInit_recomp(uint8_t* rdram, recomp_context* ctx) {
    OSMesgQueue* queue = _arg<0, OSMesgQueue*>(rdram, ctx);
    OSPfs* pfs = _arg<1, OSPfs*>(rdram, ctx);
    s32 channel = _arg<2, s32>(rdram, ctx);

    pfs->queue = (int32_t) queue;
    pfs->channel = channel;
    pfs->status = 0x1;

    ControllerPak pak;

    if (!std::filesystem::exists("controllerPak_header.sav")) {
        pak.header.open("controllerPak_header.sav", std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        pak.header.close();
    }

    ctx->r2 = 0; // PFS_NO_ERROR
}

// s32 osPfsInitPak(OSMesgQueue* queue, OSPfs* pfs, int channel);
extern "C" void osPfsInitPak_recomp(uint8_t* rdram, recomp_context* ctx) {
    OSMesgQueue* queue = _arg<0, OSMesgQueue*>(rdram, ctx);
    OSPfs* pfs = _arg<1, OSPfs*>(rdram, ctx);
    s32 channel = _arg<2, s32>(rdram, ctx);

    pfs->queue = (int32_t) queue;
    pfs->channel = channel;
    pfs->status = 0x1;

    ctx->r2 = 0; // PFS_NO_ERROR
}

// s32 osPfsFreeBlocks(OSPfs* pfs, s32* bytes_not_used);
extern "C" void osPfsFreeBlocks_recomp(uint8_t* rdram, recomp_context* ctx) {
    s32* bytes_not_used = _arg<1, s32*>(rdram, ctx);

    // locals:
    s32 usedSpace = 0;

    ControllerPak pak;

    // *bytes_not_used = 123 << 8;

    pak.header.open("controllerPak_header.sav", std::ios::binary | std::ios::in | std::ios::out);

    if (!pak.header.good()) {
        printf("file is not good!\n");
        assert(false);
    }
    if (!pak.header.is_open()) {
        printf("file isn't open!\n");
        assert(false);
    }

    for (size_t i = 0; i < 16; i++) {
        u32 seek = i * sizeof(OSPfsState);
        s32 file_size_in_bytes;
        u32 game_code;
        u16 company_code;

        // file_size
        pak.header.seekg(seek + 0x0, std::ios::beg);
        pak.header.read((char*) &file_size_in_bytes, sizeof(file_size_in_bytes));
        // game_code
        pak.header.seekg(seek + 0x4, std::ios::beg);
        pak.header.read((char*) &game_code, sizeof(game_code));
        // company_code
        pak.header.seekg(seek + 0x08, std::ios::beg);
        pak.header.read((char*) &company_code, sizeof(company_code));

        if ((company_code == 0) || (game_code == 0)) {
            continue;
        } else {
            usedSpace += file_size_in_bytes >> 8;
            file_size_in_bytes = game_code = company_code = 0;
        }
    }

    pak.header.close();

    *bytes_not_used = (123 - usedSpace) << 8;

    ctx->r2 = 0; // PFS_NO_ERROR
}

// TODO: VALIDATE
// Can't really be validated without a valid controller pak file system header, which is only used
// by the controller pak manager present in games by holding START on boot and not necessary for recomp.
// s32 osPfsAllocateFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name,
// int file_size_in_bytes, s32* file_no);
extern "C" void osPfsAllocateFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    u8* game_name = _arg<3, u8*>(rdram, ctx);
    u8* ext_name = TO_PTR(u8, MEM_W(0x10, ctx->r29));
    s32 file_size_in_bytes = (s32) MEM_W(0x14, ctx->r29);
    s32* file_no = TO_PTR(s32, MEM_W(0x18, ctx->r29));

    if ((company_code == 0) || (game_code == 0)) {
        ctx->r2 = 5; // PFS_ERR_INVALID
        return;
    }

    ControllerPak pak;

    pak.header.open("controllerPak_header.sav", std::ios::binary | std::ios::in | std::ios::out);

    if (!pak.header.good()) {
        printf("file is not good!\n");
        assert(false);
    }
    if (!pak.header.is_open()) {
        printf("file isn't open!\n");
        assert(false);
    }
    u8 freeFileIndex = 0;
#if 0
    /* Search for a free slot */
    for (size_t i = 0; i < 16; i++) {
        u32 seek = i * sizeof(OSPfsState);
        u32 game_code_;
        u16 company_code_;

        // game_code
        pak.header.seekg(seek + 0x4, std::ios::beg);
        pak.header.read((char*) &game_code_, sizeof(game_code_));
        // company_code
        pak.header.seekg(seek + 0x08, std::ios::beg);
        pak.header.read((char*) &company_code_, sizeof(company_code_));

        if ((company_code_ == 0) || (game_code_ == 0)) {
            freeFileIndex = i;
            break;
        }

        game_code_ = company_code_ = 0;
    }
#endif
    /* Set file parameters to header */
    freeFileIndex = 0;
    u32 seek = freeFileIndex * 0x20;

    // file_size
    pak.header.seekp(seek + 0x0, std::ios::beg);
    pak.header.write((char*) &file_size_in_bytes, sizeof(file_size_in_bytes));
    // game_code
    pak.header.seekp(seek + 0x4, std::ios::beg);
    pak.header.write((char*) &game_code, sizeof(game_code));
    // company_code
    pak.header.seekp(seek + 0x08, std::ios::beg);
    pak.header.write((char*) &company_code, sizeof(company_code));
    // ext_name
    pak.header.seekp(seek + 0x0C, std::ios::beg);
    pak.header.write((char*) ext_name, 4);
    // game_name
    pak.header.seekp(seek + 0x10, std::ios::beg);
    pak.header.write((char*) game_name, 16);

    pak.header.close();

    /* Create empty file */

    char filename[100];
    sprintf(filename, "controllerPak_file_%d.sav", freeFileIndex);
    pak.file.open(filename, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);

    file_size_in_bytes = (file_size_in_bytes + 31) & ~31;

    u8* zero_block = (u8*) malloc(file_size_in_bytes);
    memset(zero_block, 0, file_size_in_bytes);

    pak.file.seekp(0, std::ios::beg);
    pak.file.write((char*) zero_block, file_size_in_bytes);

    free(zero_block);

    pak.file.close();

    *file_no = freeFileIndex;

    ctx->r2 = 0; // PFS_NO_ERROR
}

bool IsFileEmpty(std::fstream& file) {

    if (!file.good()) {
        return 1;
    }

    file.seekg(0, std::ios::end);
    long size = file.tellg();
    file.seekg(0, std::ios::beg);

    return size == 0;
}

// s32 osPfsFileState(OSPfs* pfs, s32 file_no, OSPfsState* state);
extern "C" void osPfsFileState_recomp(uint8_t* rdram, recomp_context* ctx) {
    s32 file_no = _arg<1, s32>(rdram, ctx);
    OSPfsState* state = _arg<2, OSPfsState*>(rdram, ctx);

    // locals:
    s32 file_size_in_bytes;
    u32 game_code;
    u16 company_code;
    char ext_name[4];
    char game_name[16];

    // should pass the state of the requested file_no to the incoming state pointer,
    // games call this function 16 times, once per bank
    // fills the incoming state with the information inside the header of the pak.

    /* Read game info from pak */
    ControllerPak pak;

    char filename[100];
    sprintf(filename, "controllerPak_file_%d.sav", file_no);
    pak.file.open(filename, std::ios::binary | std::ios::in | std::ios::out);

    if (IsFileEmpty(pak.file)) {
        pak.file.close();
        ctx->r2 = 1; // PFS_NO_ERROR
        return;
    }
    pak.file.close();

    pak.header.open("controllerPak_header.sav", std::ios::binary | std::ios::in | std::ios::out);

    if (!pak.header.good()) {
        printf("file is not good!\n");
        assert(false);
    }
    if (!pak.header.is_open()) {
        printf("file isn't open!\n");
        assert(false);
    }

    u32 seek = file_no * sizeof(OSPfsState);

    // file_size
    pak.header.seekg(seek + 0x0, std::ios::beg);
    pak.header.read((char*) &file_size_in_bytes, sizeof(file_size_in_bytes));
    // game_code
    pak.header.seekg(seek + 0x4, std::ios::beg);
    pak.header.read((char*) &game_code, sizeof(game_code));
    // company_code
    pak.header.seekg(seek + 0x08, std::ios::beg);
    pak.header.read((char*) &company_code, sizeof(company_code));
    // ext_name
    pak.header.seekg(seek + 0x0C, std::ios::beg);
    pak.header.read((char*) ext_name, ARRAYSIZE(ext_name));
    // game_name
    pak.header.seekg(seek + 0x10, std::ios::beg);
    pak.header.read((char*) game_name, ARRAYSIZE(game_name));

    pak.header.close();

    state->file_size = file_size_in_bytes;
    state->company_code = game_code;
    state->game_code = game_code;

    for (size_t j = 0; j < ARRAYSIZE(game_name); j++) {
        state->game_name[j] = game_name[j];
    }
    for (size_t j = 0; j < ARRAYSIZE(ext_name); j++) {
        state->ext_name[j] = ext_name[j];
    }

    ctx->r2 = 0; // PFS_NO_ERROR
}

// s32 osPfsFindFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, s32* file_no);
extern "C" void osPfsFindFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    u8* game_name = _arg<3, u8*>(rdram, ctx);
    u8* ext_name = TO_PTR(u8, MEM_W(0x10, ctx->r29));
    s32* file_no = TO_PTR(s32, MEM_W(0x14, ctx->r29)); // we should return the index of the file found here

    ControllerPak pak;

    char filename[100];
    sprintf(filename, "controllerPak_file_%d.sav", *file_no);
    pak.file.open(filename, std::ios::binary | std::ios::in | std::ios::out);

    if (!std::filesystem::exists(filename) || IsFileEmpty(pak.file)) {
        pak.file.close();
        ctx->r2 = 5; // PFS_ERR_INVALID
        return;
    }
    pak.file.close();

    pak.header.open("controllerPak_header.sav", std::ios::binary | std::ios::in | std::ios::out);

    if (!pak.header.good()) {
        printf("file is not good!\n");
        assert(false);
    }
    if (!pak.header.is_open()) {
        printf("file isn't open!\n");
        assert(false);
    }

    for (size_t i = 0; i < 16; i++) {
        // locals:
        u32 game_code_;
        u16 company_code_;
        char ext_name_[4];
        char game_name_[16];
        u32 seek = i * sizeof(OSPfsState);

        // game_code
        pak.header.seekg(seek + 0x4, std::ios::beg);
        pak.header.read((char*) &game_code_, sizeof(game_code_));
        // company_code
        pak.header.seekg(seek + 0x08, std::ios::beg);
        pak.header.read((char*) &company_code_, sizeof(company_code_));
        // ext_name
        pak.header.seekg(seek + 0x0C, std::ios::beg);
        pak.header.read((char*) ext_name_, 4);
        // game_name
        pak.header.seekg(seek + 0x10, std::ios::beg);
        pak.header.read((char*) game_name_, 16);

        

        if ((company_code_ == 0) || (game_code_ == 0)) {
            continue;
        } else {
            if ((game_code == game_code_) && (company_code == company_code_) &&
                (strcmp((const char*) game_name, (const char*) game_name_) == 0) &&
                strcmp((const char*) ext_name, (const char*) ext_name_) == 0) {
                // File found
                *file_no = i;
                pak.header.close();
                ctx->r2 = 0; // PFS_NO_ERROR
                return;
            }
        }
    }

    pak.header.close();
    // File not found
    ctx->r2 = 5; // PFS_ERR_INVALID
}

// s32 osPfsReadWriteFile(OSPfs* pfs, s32 file_no, u8 flag, int offset, int size_in_bytes, u8* data_buffer);
extern "C" void osPfsReadWriteFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    s32 file_no = _arg<1, s32>(rdram, ctx);
    u8 flag = _arg<2, u8>(rdram, ctx);
    s32 offset = _arg<3, s32>(rdram, ctx);
    s32 size_in_bytes = (s32) MEM_W(0x10, ctx->r29);
    u8* data_buffer = TO_PTR(u8, MEM_W(0x14, ctx->r29));

    ControllerPak pak;

    char filename[100];
    sprintf(filename, "controllerPak_file_%d.sav", file_no);
    pak.file.open(filename, std::ios::binary | std::ios::in | std::ios::out);

    if (!pak.file.good()) {
        printf("file is not good!\n");
        ctx->r2 = 5; // PFS_ERR_INVALID // VALIDATE
        return;
        // assert(false);
    }
    if (!pak.file.is_open()) {
        printf("file isn't open!\n");
        ctx->r2 = 5; // PFS_ERR_INVALID // VALIDATE
        return;
        // assert(false);
    }

    if (flag == 0) {
        pak.file.seekg(offset, std::ios::beg);
        pak.file.read((char*) data_buffer, size_in_bytes);
        // TODO: use a separate buffer for holding the swapped memory
        ByteSwapFile(data_buffer, size_in_bytes);
    } else {
        // TODO: use a separate buffer for holding the swapped memory
        ByteSwapFile(data_buffer, size_in_bytes);
        pak.file.seekp(offset, std::ios::beg);
        pak.file.write((char*) data_buffer, size_in_bytes);
    }

    pak.file.close();

    ctx->r2 = 0; // PFS_NO_ERROR
}

extern "C" void osPfsChecker_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0; // PFS_NO_ERROR
}

// s32 osPfsNumFiles(OSPfs* pfs, s32* max_files, s32* files_used);
extern "C" void osPfsNumFiles_recomp(uint8_t* rdram, recomp_context* ctx) {
    s32* max_files = _arg<1, s32*>(rdram, ctx);
    s32* files_used = _arg<2, s32*>(rdram, ctx);

    ControllerPak pak;

    pak.header.open("controllerPak_header.sav", std::ios::binary | std::ios::in | std::ios::out);

    if (!pak.header.good()) {
        printf("file is not good!\n");
        assert(false);
    }
    if (!pak.header.is_open()) {
        printf("file isn't open!\n");
        assert(false);
    }

    u8 files = 0;
    for (size_t i = 0; i < 16; i++) {
        u32 seek = i * sizeof(OSPfsState);
        u32 game_code = 0;
        u16 company_code = 0;

        // game_code
        pak.header.seekg(seek + 0x4, std::ios::beg);
        pak.header.read((char*) &game_code, sizeof(game_code));
        // company_code
        pak.header.seekg(seek + 0x08, std::ios::beg);
        pak.header.read((char*) &company_code, sizeof(company_code));

        if ((company_code != 0) || (game_code != 0)) {
            files++;
        }
    }

    pak.header.close();

    *files_used = files;
    *max_files = MAX_FILES;

    ctx->r2 = 0; // PFS_NO_ERROR
}

// TODO: Doesn't work, the game is sending NULL game_name, ext_name and company_code for some reason?
// s32 osPfsDeleteFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name);
extern "C" void osPfsDeleteFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    u8* game_name = _arg<3, u8*>(rdram, ctx);
    u8* ext_name = TO_PTR(u8, MEM_W(0x10, ctx->r29));

    printf("osPfsDeleteFile_recomp:\n");

    if (company_code == 0 || game_code == 0) {
        ctx->r2 = 5; // PFS_ERR_INVALID
        return;
    }

    ControllerPak pak;

    pak.header.open("controllerPak_header.sav", std::ios::binary | std::ios::in | std::ios::out);

    if (!pak.header.good()) {
        printf("file is not good!\n");
        assert(false);
    }
    if (!pak.header.is_open()) {
        printf("file isn't open!\n");
        assert(false);
    }

    for (int i = 0; i < MAX_FILES; i++) {
        // locals:
        u32 game_code_;
        u16 company_code_;
        char ext_name_[4];
        char game_name_[16];
        u32 seek = i * sizeof(OSPfsState);

        // game_code
        pak.header.seekg(seek + 0x4, std::ios::beg);
        pak.header.read((char*) &game_code_, sizeof(game_code_));
        // company_code
        pak.header.seekg(seek + 0x08, std::ios::beg);
        pak.header.read((char*) &company_code_, sizeof(company_code_));
        // ext_name
        pak.header.seekg(seek + 0x0C, std::ios::beg);
        pak.header.read((char*) ext_name_, 4);
        // game_name
        pak.header.seekg(seek + 0x10, std::ios::beg);
        pak.header.read((char*) game_name_, 16);

        if ((company_code_ == 0) || (game_code_ == 0)) {
            continue;
        } else {
            if ((game_code == game_code_) && (company_code == company_code_) &&
                (strcmp((const char*) game_name, (const char*) game_name_) == 0) &&
                strcmp((const char*) ext_name, (const char*) ext_name_) == 0) {
                // File found

                // Zero out the header for this file.
                u8* zero_block = (u8*) malloc(sizeof(OSPfsState));
                memset(zero_block, 0, sizeof(OSPfsState));
                pak.header.seekp(seek + 0x0, std::ios::beg);
                pak.header.write((char*) &zero_block, sizeof(OSPfsState));
                free(zero_block);

                pak.header.close();

                char filename[100];
                sprintf(filename, "controllerPak_file_%d.sav", i);
                remove(filename);

                ctx->r2 = 0; // PFS_NO_ERROR
                return;
            }
        }
    }

    pak.header.close();

    // File not found
    ctx->r2 = 5; // PFS_ERR_INVALID
    return;
}

extern "C" void osPfsRepairId_recomp(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, 1); // PFS_ERR_NOPACK
}
