#include <filesystem>
#include <fstream>
#include <ultramodern/save.hpp>
#include <ultramodern/ultra64.h>

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))
#define DEF_DIR_PAGES 2
#define MAX_FILES 16

/* PFS Context */

inline std::filesystem::path pfs_header_path() {
    const auto filename = "controllerpak_header.bin";
    return ultramodern::get_save_base_path() / filename;
}

inline std::filesystem::path pfs_file_path(size_t file_no) {
    const auto filename = "controllerpak_file_" + std::to_string(file_no) + ".bin", file_no);
    return ultramodern::get_save_base_path() / filename;
}

inline bool pfs_header_alloc() {
    if (!std::filesystem::exists(pfs_header_path())) {
        std::ofstream out(pfs_header_path(), std::ios::binary | std::ios::out | std::ios::trunc);
        return out.good();
    }
    return true;
}

inline bool pfs_header_write(int file_no, u32 file_size, u32 game_code, u16 company_code, u8* ext_name, u8* game_name) {
    std::ofstream out(pfs_header_path(), std::ios::binary | std::ios::in);
    if (out.is_open() && out.good()) {
        u16 padding = 0;
        out.seekp(file_no * sizeof(OSPfsState), std::ios::beg);
        out.write((const char*)&file_size, 4);
        out.write((const char*)&game_code, 4);
        out.write((const char*)&company_code, 2);
        out.write((const char*)&padding, 2);
        out.write((const char*)ext_name, 4);
        out.write((const char*)game_name, 16);
    }
    return out.good();
}

inline bool pfs_header_write(int file_no, const OSPfsState& hdr) {
    return pfs_header_write(file_no, hdr.file_size, hdr.game_code, hdr.company_code, (u8*)hdr.ext_name, (u8*)hdr.game_name);
}

inline bool pfs_header_read(int file_no, OSPfsState& hdr) {
    std::ifstream in(pfs_header_path(), std::ios::binary | std::ios::in);
    if (in.is_open() && in.good()) {
        in.seekg(file_no * sizeof(OSPfsState), std::ios::beg);
        in.read((char*)&hdr.file_size, 4);
        in.read((char*)&hdr.game_code, 4);
        in.read((char*)&hdr.company_code, 2);
        in.read((char*)&hdr.pad_0A, 2);
        in.read((char*)&hdr.ext_name[0], 4);
        in.read((char*)&hdr.game_name[0], 16);
    }
    return in.good();
}

inline bool pfs_file_alloc(int file_no, int nbytes) {
    std::vector<char> zero_block((nbytes + 31) & ~31, 0);
    std::ofstream out(pfs_file_path(file_no), std::ios::binary | std::ios::out | std::ios::trunc);
    if (out.is_open() && out.good()) {
        out.write((const char*)zero_block.data(), zero_block.size());
    }
    return out.good();
}

inline bool pfs_file_write(int file_no, int offset, const char* data_buffer, int nbytes) {
    std::ofstream out(pfs_file_path(file_no), std::ios::binary | std::ios::out);
    if (out.is_open() && out.good()) {
        out.seekp(offset, std::ios::beg);
        out.write((const char*)data_buffer, nbytes);
    }
    return out.good();
}

inline bool pfs_file_read(int file_no, int offset, char* data_buffer, int nbytes) {
    std::ifstream in(pfs_file_path(file_no), std::ios::binary | std::ios::in);
    if (in.is_open() && in.good()) {
        in.seekg(offset, std::ios::beg);
        in.read((char*)data_buffer, nbytes);
    }
    return in.good();
}

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

static s32 __osGetId(RDRAM_ARG PTR(OSPfs) pfs_) {
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);

    // we don't implement the real filesystem, so just mimic initialization
    pfs->version = 0;
    pfs->banks = 1;
    pfs->activebank = 0;
    pfs->inode_start_page = 1 + DEF_DIR_PAGES + (2 * pfs->banks);
    pfs->dir_size = DEF_DIR_PAGES * PFS_ONE_PAGE;
    pfs->inode_table = 1 * PFS_ONE_PAGE;
    pfs->minode_table = (1 + pfs->banks) * PFS_ONE_PAGE;
    pfs->dir_table = pfs->minode_table + (pfs->banks * PFS_ONE_PAGE);

    std::memset(pfs->id, 0, ARRLEN(pfs->id));
    std::memset(pfs->label, 0, ARRLEN(pfs->label));
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
    __osGetId(PASS_RDRAM pfs_);

    const s32 ret = osPfsChecker(PASS_RDRAM pfs_);
    pfs->status |= PFS_INITIALIZED;
    return ret;
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
    __osGetId(PASS_RDRAM pfs_);

    const s32 ret = osPfsChecker(PASS_RDRAM pfs_);
    pfs->status |= PFS_INITIALIZED;
    return ret;
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

    u8 free_file_index = 0;
    for (size_t i = 0; i < MAX_FILES; i++) {
        OSPfsState hdr{};
        pfs_header_read(i, hdr);

        if ((hdr.company_code == 0) || (hdr.game_code == 0)) {
            free_file_index = i;
            break;
        }
    }

    if (free_file_index == MAX_FILES) {
        return PFS_DIR_FULL;
    }
    if (!pfs_header_write(free_file_index, nbytes, game_code, company_code, ext_name, game_name)) {
        return PFS_ERR_INVALID;
    }
    if (!pfs_file_alloc(free_file_index, nbytes)) {
        return PFS_ERR_INVALID;
    }
    *file_no = free_file_index;
    return 0;
}

extern "C" s32 osPfsFindFile(RDRAM_ARG PTR(OSPfs) pfs_, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, PTR(s32) file_no_) {
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);
    s32* file_no = TO_PTR(s32, file_no_);

    if (company_code == 0 || game_code == 0) {
        return PFS_ERR_INVALID;
    }

    for (size_t i = 0; i < MAX_FILES; i++) {
        OSPfsState hdr{};
        pfs_header_read(i, hdr);

        if ((game_code == hdr.game_code) && (company_code == hdr.company_code)) {
            const auto gn_match = !std::memcmp(game_name, hdr.game_name, sizeof(hdr.game_name));
            const auto en_match = !std::memcmp(ext_name, hdr.ext_name, sizeof(hdr.ext_name));
            if (gn_match && en_match) {
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

    for (int i = 0; i < MAX_FILES; i++) {
        OSPfsState hdr{};
        pfs_header_read(i, hdr);

        if ((game_code == hdr.game_code) && (company_code == hdr.company_code)) {
            const auto gn_match = !std::memcmp(game_name, hdr.game_name, sizeof(hdr.game_name));
            const auto en_match = !std::memcmp(ext_name, hdr.ext_name, sizeof(hdr.ext_name));
            if (gn_match && en_match) {
                pfs_header_write(i, OSPfsState{});
                std::filesystem::remove(pfs_file_path(i));
                return 0;
            }
        }
    }
    return PFS_ERR_INVALID;
}

extern "C" s32 osPfsReadWriteFile(RDRAM_ARG PTR(OSPfs) pfs_, s32 file_no, u8 flag, int offset, int nbytes, u8* data_buffer) {
    if (!std::filesystem::exists(pfs_file_path(file_no))) {
        return PFS_ERR_INVALID;
    }
    else if ((flag == PFS_READ) && !pfs_file_read(file_no, offset, (char*)data_buffer, nbytes)) {
        return PFS_ERR_INVALID;
    }
    else if ((flag == PFS_WRITE) && !pfs_file_write(file_no, offset, (const char*)data_buffer, nbytes)) {
        return PFS_ERR_INVALID;
    }
    return 0;
}

extern "C" s32 osPfsFileState(RDRAM_ARG PTR(OSPfs) pfs_, s32 file_no, PTR(OSPfsState) state_) {
    OSPfsState *state = TO_PTR(OSPfsState, state_);

    if (!std::filesystem::exists(pfs_file_path(file_no))) {
        return PFS_ERR_INVALID;
    }

    OSPfsState hdr{};
    pfs_header_read(file_no, hdr);

    state->file_size = hdr.file_size;
    state->company_code = hdr.company_code;
    state->game_code = hdr.game_code;
    std::memcpy(state->game_name, hdr.game_name, sizeof(hdr.game_name));
    std::memcpy(state->ext_name, hdr.ext_name, sizeof(hdr.ext_name));
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
        OSPfsState hdr{};
        pfs_header_read(i, hdr);

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
        OSPfsState hdr{};
        pfs_header_read(i, hdr);

        if ((hdr.company_code != 0) && (hdr.game_code != 0)) {
            num_files++;
        }
    }

    *max_files = MAX_FILES;
    *files_used = num_files;
    return 0;
}

