#include <array>
#include <filesystem>
#include <fstream>
#include <ultramodern/save.hpp>
#include <ultramodern/ultra64.h>

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))
#define DEF_DIR_PAGES 2
#define MAX_FILES 16

/* PFS Context */

struct pfs_header_t { // same layout as OSPfsState, but non-byteswapped
    uint32_t file_size;
    uint32_t game_code;
    uint16_t company_code;
    std::array<char, 4> ext_name;
    std::array<char, 16> game_name;
    uint16_t padding;

    pfs_header_t() = default;
    pfs_header_t(uint32_t fs, uint32_t gc, uint16_t cc, const char* en, const char* gn)
    : file_size{fs}, game_code{gc}, company_code{cc} {
        std::memcpy(ext_name.data(), en, 4);
        std::memcpy(game_name.data(), gn, 16);
    }
    inline bool valid() const {
        return game_code != 0 && company_code != 0;
    }
    inline bool compare(uint32_t gcode, uint16_t ccode, const char* ename, const char* gname) const {
        return game_code == gcode && company_code == ccode &&
            std::memcmp(ext_name.data(), ename, 4) == 0 &&
            std::memcmp(game_name.data(), gname, 14) == 0;
    }
};

inline std::filesystem::path pfs_header_path() {
    const auto filename = "controllerpak_header.bin";
    return ultramodern::get_save_base_path() / filename;
}

inline std::filesystem::path pfs_file_path(size_t file_no) {
    const auto filename = "controllerpak_file_" + std::to_string(file_no) + ".bin";
    return ultramodern::get_save_base_path() / filename;
}

inline bool pfs_header_alloc() {
    if (!std::filesystem::exists(pfs_header_path())) {
        std::ofstream out(pfs_header_path(), std::ios::binary | std::ios::out | std::ios::trunc);
        return out.good();
    }
    return true;
}

inline bool pfs_header_write(int file_no, const pfs_header_t& hdr) {
    std::ofstream out(pfs_header_path(), std::ios::binary | std::ios::in);
    if (out.is_open() && out.good()) {
        out.seekp(file_no * sizeof(pfs_header_t), std::ios::beg);
        out.write((const char*)&hdr.file_size, sizeof(hdr.file_size));
        out.write((const char*)&hdr.game_code, sizeof(hdr.game_code));
        out.write((const char*)&hdr.company_code, sizeof(hdr.company_code));
        out.write((const char*)&hdr.ext_name[0], hdr.ext_name.size());
        out.write((const char*)&hdr.game_name[0], hdr.game_name.size());
        out.write((const char*)&hdr.padding, sizeof(hdr.padding));
    }
    return out.good();
}

inline bool pfs_header_read(int file_no, pfs_header_t& hdr) {
    std::ifstream in(pfs_header_path(), std::ios::binary | std::ios::in);
    if (in.is_open() && in.good()) {
        in.seekg(file_no * sizeof(pfs_header_t), std::ios::beg);
        in.read((char*)&hdr.file_size, sizeof(hdr.file_size));
        in.read((char*)&hdr.game_code, sizeof(hdr.game_code));
        in.read((char*)&hdr.company_code, sizeof(hdr.company_code));
        in.read((char*)&hdr.ext_name[0], hdr.ext_name.size());
        in.read((char*)&hdr.game_name[0], hdr.game_name.size());
        in.read((char*)&hdr.padding, sizeof(hdr.padding));
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

    if (company_code == 0 || game_code == 0) {
        return PFS_ERR_INVALID;
    }

    pfs_header_t hdr{};
    u8 free_file_index = 0;
    for (size_t i = 0; i < MAX_FILES; i++) {
        pfs_header_read(i, hdr);
        if (!hdr.valid()) {
            free_file_index = i;
            break;
        }
    }

    if (free_file_index == MAX_FILES) {
        return PFS_DIR_FULL;
    }
    if (!pfs_header_write(free_file_index, pfs_header_t{(uint32_t)nbytes, game_code, company_code, (char*)ext_name, (char*)game_name})) {
        return PFS_ERR_INVALID;
    }
    if (!pfs_file_alloc(free_file_index, nbytes)) {
        return PFS_ERR_INVALID;
    }
    *file_no = free_file_index;
    return 0;
}

extern "C" s32 osPfsFindFile(RDRAM_ARG PTR(OSPfs) pfs_, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, PTR(s32) file_no_) {
    s32* file_no = TO_PTR(s32, file_no_);

    if (company_code == 0 || game_code == 0) {
        return PFS_ERR_INVALID;
    }

    pfs_header_t hdr{};
    for (size_t i = 0; i < MAX_FILES; i++) {
        pfs_header_read(i, hdr);
        if (hdr.compare(game_code, company_code, (char*)ext_name, (char*)game_name)) {
            *file_no = i;
            return 0;
        }
    }
    return PFS_ERR_INVALID;
}

extern "C" s32 osPfsDeleteFile(RDRAM_ARG PTR(OSPfs) pfs_, u16 company_code, u32 game_code, u8* game_name, u8* ext_name) {
    if (company_code == 0 || game_code == 0) {
        return PFS_ERR_INVALID;
    }

    pfs_header_t hdr{};
    for (int i = 0; i < MAX_FILES; i++) {
        pfs_header_read(i, hdr);
        if (hdr.compare(game_code, company_code, (char*)ext_name, (char*)game_name)) {
            pfs_header_write(i, pfs_header_t{});
            std::filesystem::remove(pfs_file_path(i));
            return 0;
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

inline void bswap_copy(char* dst, const char* src, int offset, int n) {
    for (int i = 0; i < n; i++) { dst[(i + offset) ^ 3] = src[i + offset]; }
}

extern "C" s32 osPfsFileState(RDRAM_ARG PTR(OSPfs) pfs_, s32 file_no, PTR(OSPfsState) state_) {
    OSPfsState *state = TO_PTR(OSPfsState, state_);

    if (!std::filesystem::exists(pfs_file_path(file_no))) {
        return PFS_ERR_INVALID;
    }

    pfs_header_t hdr{};
    pfs_header_read(file_no, hdr);

    state->file_size = hdr.file_size;
    state->company_code = hdr.company_code;
    state->game_code = hdr.game_code;

    // FIXME OSPfsState layout is an absoute mess. giving up and byte swapping
    bswap_copy((char*)state, (char*)&hdr, 10, 20);
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
    pfs_header_t hdr{};
    for (size_t i = 0; i < MAX_FILES; i++) {
        pfs_header_read(i, hdr);
        if (hdr.valid()) {
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
    pfs_header_t hdr{};
    for (size_t i = 0; i < MAX_FILES; i++) {
        pfs_header_read(i, hdr);
        if (hdr.valid()) {
            num_files++;
        }
    }

    *max_files = MAX_FILES;
    *files_used = num_files;
    return 0;
}

