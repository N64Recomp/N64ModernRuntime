#ifndef __ULTRAMODERN_PFS_HPP__
#define __ULTRAMODERN_PFS_HPP__

#include <fstream>
#include <ultramodern/save.hpp>

typedef struct pfs_state {
    std::fstream header;
    std::fstream file;
} pfs_state_t;

typedef struct pfs_header {
    int file_size;
    u32 game_code;
    u16 company_code;
    u8 ext_name[4];
    u8 game_name[16];
} pfs_header_t;

inline std::filesystem::path pfs_header_path() {
    const auto filename = "controllerpak_header.bin";
    return ultramodern::get_save_base_path() / filename;
}

inline std::filesystem::path pfs_file_path(size_t fileno) {
    const auto filename = std::format("controllerpak_file_{}.bin", fileno);
    return ultramodern::get_save_base_path() / filename;
}

void pfs_header_alloc() {
    pfs_state_t pak;
    if (!std::filesystem::exists(pfs_header_path())) {
        pak.header.open(pfs_header_path(), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        pak.header.close();
    }
}

void pfs_header_write(int file_size, u32 game_code, u16 company_code, const u8* ext_name, const u8* game_name, u8 file_index) {
    pfs_state_t pak;
    pak.header.open(pfs_header_path(), std::ios::binary | std::ios::out);

    if (!pak.header.good()) {
        assert(false);
    }
    if (!pak.header.is_open()) {
        assert(false);
    }

    /* Set file parameters to header */
    u32 seek = file_index * 0x20;

    // file_size
    pak.header.seekp(seek + 0x0, std::ios::beg);
    pak.header.write((const char*)&file_size, 4);
    // game_code
    pak.header.seekp(seek + 0x4, std::ios::beg);
    pak.header.write((const char*)&game_code, 4);
    // company_code
    pak.header.seekp(seek + 0x08, std::ios::beg);
    pak.header.write((const char*)&company_code, 2);
    // ext_name
    pak.header.seekp(seek + 0xC, std::ios::beg);
    pak.header.write((const char*)ext_name, 4);
    // game_name
    pak.header.seekp(seek + 0x10, std::ios::beg);
    pak.header.write((const char*)game_name, 16);

    pak.header.close();
}

void pfs_header_write(const pfs_header_t& hdr, u8 file_index) {
    pfs_header_write(hdr.file_size, hdr.game_code, hdr.company_code, hdr.ext_name, hdr.game_name, file_index);
}

void pfs_header_read(pfs_header_t& hdr, u8 file_index) {
    pfs_state_t pak;
    pak.header.open(pfs_header_path(), std::ios::binary | std::ios::in);

    if (!pak.header.good()) {
        assert(false);
    }
    if (!pak.header.is_open()) {
        assert(false);
    }

    /* Set file parameters to header */
    u32 seek = file_index * sizeof(OSPfsState);

    // file_size
    pak.header.seekg(seek + 0x0, std::ios::beg);
    pak.header.read((char*)&hdr.file_size, 4);
    // game_code
    pak.header.seekg(seek + 0x4, std::ios::beg);
    pak.header.read((char*)&hdr.game_code, 4);
    // company_code
    pak.header.seekg(seek + 0x08, std::ios::beg);
    pak.header.read((char*)&hdr.company_code, 2);
    // ext_name
    pak.header.seekg(seek + 0xC, std::ios::beg);
    pak.header.read((char*)hdr.ext_name, 4);
    // game_name
    pak.header.seekg(seek + 0x10, std::ios::beg);
    pak.header.read((char*)hdr.game_name, 16);

    pak.header.close();
}

void Pfs_ByteSwapFile(u8* buffer, size_t size) {
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

void ByteSwapCopy(uint8_t* dst, const uint8_t* src, size_t nbytes) {
    for (size_t i = 0; i < nbytes; i++) {
        dst[i] = src[i ^ 3];
    }
}

#endif // __ULTRAMODERN_PFS_HPP__

