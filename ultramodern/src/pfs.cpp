#include <cassert>
#include <cstdlib>
#include <format>

#include <ultramodern/ultra64.h>
#include "pfs.hpp"

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))
#define MAX_FILES 16

/* ControllerPak */

static s32 __osPfsGetStatus(RDRAM_ARG PTR(OSMesgQueue) queue, int channel) {
    const auto device_info = ultramodern::get_connected_device_info(channel);
    if (device_info.connected_device != ultramodern::input::Device::Controller) {
        return PFS_ERR_CONTRFAIL;
    }
    if (device_info.connected_pak == ultramodern::input::Pak::None) {
        return PFS_ERR_NOPACK;
    }
    if (device_info.connected_pak != ultramodern::input::Pak::ControllerPak) {
        return PFS_ERR_DEVICE;
    }

    pfs_header_alloc();
    return 0;
}

extern "C" s32 osPfsInitPak(RDRAM_ARG PTR(OSMesgQueue) mq_, PTR(OSPfs) pfs_, int channel) {
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);

    const auto status = __osPfsGetStatus(PASS_RDRAM mq_, channel);
    if (status != 0) {
        return status;
    }

    pfs->queue = mq_;
    pfs->channel = channel;
    pfs->status = 0;

    const s32 ret = osPfsChecker(PASS_RDRAM pfs_);
    pfs->status |= PFS_INITIALIZED;
    return 0;
}

extern "C" s32 osPfsRepairId(RDRAM_ARG PTR(OSPfs) pfs) {
    return 0;
}

extern "C" s32 osPfsInit(RDRAM_ARG PTR(OSMesgQueue) mq_, PTR(OSPfs) pfs_, int channel) {
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);

    const auto status = __osPfsGetStatus(PASS_RDRAM mq_, channel);
    if (status != 0) {
        return status;
    }

    pfs->queue = mq_;
    pfs->channel = channel;
    pfs->status = 0;
    pfs->activebank = -1;

    const s32 ret = osPfsChecker(PASS_RDRAM pfs_);
    pfs->status |= PFS_INITIALIZED;
    return 0;
}

extern "C" s32 osPfsReFormat(RDRAM_ARG PTR(OSPfs) pfs, PTR(OSMesgQueue) mq_, int channel) {
    return 0;
}

extern "C" s32 osPfsChecker(RDRAM_ARG PTR(OSPfs) pfs) {
    return 0;
}

extern "C" s32 osPfsAllocateFile(RDRAM_ARG PTR(OSPfs) pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, int nbytes, PTR(s32) file_no_) {
    s32* file_no = TO_PTR(s32, file_no_);

    if ((company_code == 0) || (game_code == 0)) {
        return PFS_ERR_INVALID;
    }

    /* Search for a free slot */
    u8 free_file_index = 0;
    for (size_t i = 0; i < MAX_FILES; i++) {
        pfs_header_t hdr{};
        pfs_header_read(hdr, i);

        if ((hdr.company_code == 0) || (hdr.game_code == 0)) {
            free_file_index = i;
            break;
        }
    }

    if (free_file_index == MAX_FILES) {
        return PFS_DIR_FULL;
    }

    pfs_header_write(nbytes, game_code, company_code, ext_name, game_name, free_file_index);

    /* Create empty file */

    pfs_state_t pak;

    const auto filename = pfs_file_path(free_file_index);
    pak.file.open(filename, std::ios::binary | std::ios::out | std::ios::trunc);

    nbytes = (nbytes + 31) & ~31;

    std::vector<u8> zero_block(nbytes, 0);
    pak.file.seekp(0, std::ios::beg);
    pak.file.write((char*)zero_block.data(), nbytes);
    pak.file.close();

    *file_no = free_file_index;
    return 0;
}

extern "C" s32 osPfsFindFile(RDRAM_ARG PTR(OSPfs) pfs_, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, PTR(s32) file_no_) {
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);
    s32* file_no = TO_PTR(s32, file_no_);

    for (size_t i = 0; i < MAX_FILES; i++) {
        pfs_header_t hdr{};
        pfs_header_read(hdr, i);

        if ((hdr.company_code == 0) || (hdr.game_code == 0)) {
            continue;
        } else {
            if ((game_code == hdr.game_code) && (company_code == hdr.company_code)) {
                for (size_t i = 0; i < ARRLEN(hdr.game_name); i++) {
                    if (game_name[i] != hdr.game_name[i]) {
                        break;
                    }
                }
                for (size_t i = 0; i < ARRLEN(hdr.ext_name); i++) {
                    if (ext_name[i] != hdr.ext_name[i]) {
                        break;
                    }
                }
                //  File found
                *file_no = i;
                return 0;
            }
        }
    }

    return PFS_ERR_INVALID;
}

extern "C" s32 osPfsDeleteFile(RDRAM_ARG PTR(OSPfs) pfs_, u16 company_code, u32 game_code, u8* game_name, u8* ext_name) {
    if (company_code == 0 || game_code == 0) {
        return PFS_ERR_INVALID;
    }

    pfs_state_t pak;
    for (int i = 0; i < MAX_FILES; i++) {
        pfs_header_t hdr{};
        pfs_header_read(hdr, i);

        if ((hdr.company_code == 0) || (hdr.game_code == 0)) {
            continue;
        } else {
            if ((game_code == hdr.game_code) && (company_code == hdr.company_code)) {
                int gncount = 0;
                int encount = 0;

                for (size_t i = 0; i < ARRLEN(hdr.game_name); i++) {
                    if (game_name[i] == hdr.game_name[i]) {
                        gncount++;
                    }
                }
                for (size_t i = 0; i < ARRLEN(hdr.ext_name); i++) {
                    if (ext_name[i] == hdr.ext_name[i]) {
                        encount++;
                    }
                }
                if ((gncount != 16) || encount != 4) {
                    continue;
                }

                // File found
                pak.header.open(pfs_header_path(), std::ios::binary | std::ios::in | std::ios::out);

                if (!pak.header.good()) {
                    assert(false);
                }
                if (!pak.header.is_open()) {
                    assert(false);
                }

                u32 seek = i * sizeof(OSPfsState);

                // Zero out the header for this file.
                std::vector<u8> zero_block(sizeof(OSPfsState), 0);
                pak.header.seekp(seek + 0x0, std::ios::beg);
                pak.header.write((char*)zero_block.data(), sizeof(OSPfsState));
                pak.header.close();

                std::filesystem::remove(pfs_file_path(i));
                return 0;
            }
        }
    }

    return PFS_ERR_INVALID;
}

extern "C" s32 osPfsReadWriteFile(RDRAM_ARG PTR(OSPfs) pfs_, s32 file_no, u8 flag, int offset, int size_in_bytes, u8* data_buffer) {
    pfs_state_t pak;

    const auto filename = pfs_file_path(file_no);
    pak.file.open(filename, std::ios::binary | std::ios::in | std::ios::out);

    if (!std::filesystem::exists(filename)) {
        return PFS_ERR_INVALID;
    }
    if (!pak.file.good()) {
        return PFS_ERR_INVALID;
    }
    if (!pak.file.is_open()) {
        return PFS_ERR_INVALID;
    }

    std::vector<u8> swap_buffer(size_in_bytes);
    if (flag == 0) {
        pak.file.seekg(offset, std::ios::beg);
        pak.file.read((char*)swap_buffer.data(), size_in_bytes);
        ByteSwapCopy(data_buffer, swap_buffer.data(), size_in_bytes);
    } else {
        ByteSwapCopy(swap_buffer.data(), data_buffer, size_in_bytes);
        pak.file.seekp(offset, std::ios::beg);
        pak.file.write((char*)swap_buffer.data(), size_in_bytes);
    }

    pak.file.close();
    return 0;
}

extern "C" s32 osPfsFileState(RDRAM_ARG PTR(OSPfs) pfs_, s32 file_no, PTR(OSPfsState) state_) {
    OSPfsState *state = TO_PTR(OSPfsState, state_);

    // should pass the state of the requested file_no to the incoming state pointer,
    // games call this function 16 times, once per file
    // fills the incoming state with the information inside the header of the pak.

    const auto filename = pfs_file_path(file_no);
    if (!std::filesystem::exists(filename)) {
        return PFS_ERR_INVALID;
    }

    /* Read game info from pak */
    pfs_header_t hdr{};
    pfs_header_read(hdr, file_no);

    state->file_size = hdr.file_size;
    state->company_code = hdr.company_code;
    state->game_code = hdr.game_code;

    for (size_t i = 0; i < ARRLEN(hdr.game_name); i++) {
        state->game_name[i] = hdr.game_name[i];
    }
    for (size_t i = 0; i < ARRLEN(hdr.ext_name); i++) {
        state->ext_name[i] = hdr.ext_name[i];
    }

    return 0;
}

extern "C" s32 osPfsGetLabel(RDRAM_ARG PTR(OSPfs) pfs_, u8* label, PTR(int) len_) {
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);
    int* len = TO_PTR(int, len_);

    if (label == NULL) {
        return PFS_ERR_INVALID;
    }
//  if (__osCheckId(pfs) == PFS_ERR_NEW_PACK) {
//      return PFS_ERR_NEW_PACK;
//  }

    int i;
    for (i = 0; i < ARRLEN(pfs->label); i++) {
        if (pfs->label[i] == 0) {
            break;
        }
        *label++ = pfs->label[i];
    }
    *len = i;
    return 0;
}

extern "C" s32 osPfsSetLabel(RDRAM_ARG PTR(OSPfs) pfs_, u8* label) {
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);

    if (label != NULL) {
        for (int i = 0; i < ARRLEN(pfs->label); i++) {
            if (*label == 0) {
                break;
            }
            pfs->label[i] = *label++;
        }
    }
    return 0;
}

extern "C" s32 osPfsIsPlug(RDRAM_ARG PTR(OSMesgQueue) mq_, u8* pattern) {
    u8 bits = 0;

    for (int channel = 0; channel < ultramodern::get_max_controllers(); channel++) {
        if (__osPfsGetStatus(PASS_RDRAM mq_, channel) == 0) {
            bits |= (1 << channel);
        }
    }
    *pattern = bits;
    return 0;
}

extern "C" s32 osPfsFreeBlocks(RDRAM_ARG PTR(OSPfs) pfs_, PTR(s32) bytes_not_used_) {
    OSPfs *pfs = TO_PTR(OSPfs, pfs_);
    s32 *bytes_not_used = TO_PTR(s32, bytes_not_used_);

    s32 bytes_used = 0;
    for (size_t i = 0; i < MAX_FILES; i++) {
        pfs_header_t hdr{};
        pfs_header_read(hdr, i);

        if ((hdr.company_code != 0) && (hdr.game_code != 0)) {
            bytes_used += hdr.file_size >> 8;
        }
    }

    *bytes_not_used = (123 - bytes_used) << 8;
    return 0;
}

extern "C" s32 osPfsNumFiles(RDRAM_ARG PTR(OSPfs) pfs_, PTR(s32) max_files_, PTR(s32) files_used_) {
    OSPfs *pfs = TO_PTR(OSPfs, pfs_);
    s32 *max_files = TO_PTR(s32, max_files_);
    s32 *files_used = TO_PTR(s32, files_used_);

    u8 num_files = 0;
    for (size_t i = 0; i < MAX_FILES; i++) {
        pfs_header_t hdr{};
        pfs_header_read(hdr, i);

        if ((hdr.company_code != 0) && (hdr.game_code != 0)) {
            num_files++;
        }
    }

    *max_files = MAX_FILES;
    *files_used = num_files;
    return 0;
}

