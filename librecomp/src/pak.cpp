#include <filesystem>
#include <fstream>
#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"

#include "recomp.h"
#include "helpers.hpp"

typedef struct ControllerPak {
    OSPfsState state;
    FILE* file;
} ControllerPak;

static ControllerPak sControllerPak[16];
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

    *bytes_not_used = 123 << 8;

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

    ControllerPak* pak = &sControllerPak[*file_no];

    printf("osPfsAllocateFile_recomp:\n");

    *file_no = sFileSelect++;

    char filename[100];
    sprintf(filename, "controllerPak_file_%d.sav", *file_no);
    pak->file = fopen(filename, "wb+");

    file_size_in_bytes = (file_size_in_bytes + 31) & ~31;

    u8* zero_block = (u8*) malloc(file_size_in_bytes);
    memset(zero_block, 0, file_size_in_bytes);
    fwrite(zero_block, file_size_in_bytes, 1, pak->file);

    pak->state.company_code = company_code;
    pak->state.game_code = game_code;
    strcpy(pak->state.game_name, (const char*) game_name);
    strcpy(pak->state.ext_name, (const char*) ext_name);

    ctx->r2 = 0; // PFS_NO_ERROR
}

// TODO: VALIDATE
// Can't really be validated without a valid controller pak file system header, which is only used
// by the controller pak manager present in games by holding START on boot and not necessary for recomp.
// s32 osPfsDeleteFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name);
extern "C" void osPfsDeleteFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    u8* game_name = _arg<3, u8*>(rdram, ctx);
    u8* ext_name = TO_PTR(u8, MEM_W(0x10, ctx->r29));
    s32 file_no;

    printf("osPfsDeleteFile_recomp:\n");

    for (size_t i = 0; i < ARRAYSIZE(sControllerPak); i++) {
        if ((sControllerPak[i].state.company_code = company_code) && (sControllerPak[i].state.game_code == game_code) &&
            (strcmp(sControllerPak[i].state.game_name, (const char*) game_name) == 0) &&
            strcmp(sControllerPak[i].state.ext_name, (const char*) ext_name) == 0) {
            file_no = i;

            char filename[100];
            sprintf(filename, "controllerPak_file_%d.sav", file_no);
            remove(filename);

            ctx->r2 = 0; // PFS_NO_ERROR
            printf("Returned 0: FILE DELETED\n\n");
            return;
        } else {
            ctx->r2 = 1; // PFS_ERR_NOPACK
            printf("Returned 1: FILE NOT FOUND\n\n");
        }
    }
}

bool IsFileEmpty(FILE* file, s32 file_no) {
    u8 data_buffer[256];

    fseek(file, 0, SEEK_SET);
    fread(data_buffer, 0x100, 1, file);

    long current = ftell(file);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, current, SEEK_SET);

    return size == 0;
}

// s32 osPfsFileState(OSPfs* pfs, s32 file_no, OSPfsState* state);
extern "C" void osPfsFileState_recomp(uint8_t* rdram, recomp_context* ctx) {
    s32 file_no = _arg<1, s32>(rdram, ctx);
    OSPfsState* state = _arg<2, OSPfsState*>(rdram, ctx);

    ControllerPak* pak = &sControllerPak[file_no];

    char filename[100];
    sprintf(filename, "controllerPak_file_%d.sav", file_no);
    pak->file = fopen(filename, "rb+");
    if (pak->file == NULL) {
        pak->file = fopen(filename, "wb+");
    }

    if (!IsFileEmpty(pak->file, file_no)) {
        state->file_size = pak->state.file_size;
        state->company_code = pak->state.company_code;
        state->game_code = pak->state.game_code;

        for (size_t j = 0; j < ARRAYSIZE(pak->state.game_name); j++) {
            state->game_name[j] = pak->state.game_name[j];
        }
        for (size_t j = 0; j < ARRAYSIZE(pak->state.ext_name); j++) {
            state->ext_name[j] = pak->state.ext_name[j];
        }

        fclose(pak->file);
        ctx->r2 = 0; // PFS_NO_ERROR
    } else {
        fclose(pak->file);
        remove(filename);
        ctx->r2 = 1; // PFS_ERR_NOPACK
    }
}

// s32 osPfsFindFile(OSPfs* pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, s32* file_no);
extern "C" void osPfsFindFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    u16 company_code = _arg<1, u16>(rdram, ctx);
    u32 game_code = _arg<2, u32>(rdram, ctx);
    u8* game_name = _arg<3, u8*>(rdram, ctx);
    u8* ext_name = TO_PTR(u8, MEM_W(0x10, ctx->r29));
    s32* file_no = TO_PTR(s32, MEM_W(0x14, ctx->r29));

    for (size_t i = 0; i < ARRAYSIZE(sControllerPak); i++) {
        if ((sControllerPak[i].state.game_code == game_code) &&
            (sControllerPak[i].state.company_code == company_code) &&
            (strcmp(sControllerPak[i].state.game_name, (const char*) game_name) == 0) &&
            strcmp(sControllerPak[i].state.ext_name, (const char*) ext_name) == 0) {
            // File found
            *file_no = i;
            ctx->r2 = 0; // PFS_NO_ERROR
            return;
        }
    }

    // Open file
    *file_no = sFileSelect++;

    char filename[100];
    sprintf(filename, "controllerPak_file_%d.sav", *file_no);
    sControllerPak[*file_no].file = fopen(filename, "rb+");
    if (sControllerPak[*file_no].file == NULL) {
        sControllerPak[*file_no].file = fopen(filename, "wb+");
    }

    sControllerPak[*file_no].state.company_code = company_code;
    sControllerPak[*file_no].state.game_code = game_code;
    strcpy(sControllerPak[*file_no].state.game_name, (const char*) game_name);
    strcpy(sControllerPak[*file_no].state.ext_name, (const char*) ext_name);

    ctx->r2 = 0; // PFS_NO_ERROR
}

// s32 osPfsReadWriteFile(OSPfs* pfs, s32 file_no, u8 flag, int offset, int size_in_bytes, u8* data_buffer);
extern "C" void osPfsReadWriteFile_recomp(uint8_t* rdram, recomp_context* ctx) {
    s32 file_no = _arg<1, s32>(rdram, ctx);
    u8 flag = _arg<2, u8>(rdram, ctx);
    s32 offset = _arg<3, s32>(rdram, ctx);
    s32 size_in_bytes = (s32) MEM_W(0x10, ctx->r29);
    u8* data_buffer = TO_PTR(u8, MEM_W(0x14, ctx->r29));

    ControllerPak* pak = &sControllerPak[file_no];

    if (flag == 0) {
        fseek(pak->file, offset, SEEK_SET);
        fread(data_buffer, size_in_bytes, 1, pak->file);
        ByteSwapFile(data_buffer, size_in_bytes);
    } else {
        ByteSwapFile(data_buffer, size_in_bytes);
        fseek(pak->file, offset, SEEK_SET);
        fwrite(data_buffer, size_in_bytes, 1, pak->file);
    }

    ctx->r2 = 0; // PFS_NO_ERROR
}

extern "C" void osPfsChecker_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = 0; // PFS_NO_ERROR
}

// s32 osPfsNumFiles(OSPfs* pfs, s32* max_files, s32* files_used);
extern "C" void osPfsNumFiles_recomp(uint8_t* rdram, recomp_context* ctx) {
    s32* max_files = _arg<1, s32*>(rdram, ctx);
    s32* files_used = _arg<2, s32*>(rdram, ctx);

    u8 files = 0;
    for (size_t i = 0; i < ARRAYSIZE(sControllerPak); i++) {
        if ((sControllerPak[i].state.company_code != 0) && (sControllerPak[i].state.game_code != 0)) {
            files++;
        }
    }

    *files_used = files;
    *max_files = ARRAYSIZE(sControllerPak);

    ctx->r2 = 0; // PFS_NO_ERROR
}

extern "C" void osPfsRepairId_recomp(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, 1); // PFS_ERR_NOPACK
}
