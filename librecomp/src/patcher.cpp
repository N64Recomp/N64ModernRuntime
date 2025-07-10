#include "patcher.hpp"

// BPS patcher based on the spec at https://github.com/blakesmith/rombp/blob/master/docs/bps_spec.md.

bool read_number(std::span<const uint8_t> patch_data, size_t& offset, uint64_t& number_out) {
    uint64_t data = 0;
    uint64_t shift = 1;
    while (true) {
        if (offset >= patch_data.size()) {
            return false;
        }
        uint8_t x = patch_data[offset++];
        data += (x & 0x7f) * shift;
        if (x & 0x80) {
            break;
        }
        shift <<= 7;
        data += shift;
    }
    number_out = data;
    return true;
}

bool read_signed_number(std::span<const uint8_t> patch_data, size_t& offset, int64_t& number_out) {
    uint64_t raw_number;
    if (!read_number(patch_data, offset, raw_number)) {
        return false;
    }

    int64_t ret = raw_number >> 1;
    if (raw_number & 1) {
        ret = -ret;
    }
    number_out = ret;
    return true;
}

bool read_u32(std::span<const uint8_t> patch_data, size_t& offset, uint32_t& number_out) {
    if (offset > patch_data.size() - sizeof(uint32_t)) {
        return false;
    }

    number_out =
        (uint32_t(patch_data[offset++]) <<  0) |
        (uint32_t(patch_data[offset++]) <<  8) |
        (uint32_t(patch_data[offset++]) << 16) |
        (uint32_t(patch_data[offset++]) << 24);
    return true;
}

enum class PatchActionType {
    SourceRead,
    TargetRead,
    SourceCopy,
    TargetCopy
};

bool do_source_read(std::span<const uint8_t> patch_data, std::span<const uint8_t> rom, std::vector<uint8_t>& ret, size_t& patch_offset, uint64_t action_length) {
    size_t output_offset = ret.size();
    size_t read_end = output_offset + action_length;
    if (read_end > rom.size()) {
        return false;
    }

    ret.insert(ret.end(), rom.begin() + output_offset, rom.begin() + output_offset + action_length);
    return true;
}

bool do_target_read(std::span<const uint8_t> patch_data, std::span<const uint8_t> rom, std::vector<uint8_t>& ret, size_t& patch_offset, uint64_t action_length) {
    size_t read_end = patch_offset + action_length;
    if (read_end > patch_data.size()) {
        return false;
    }

    ret.insert(ret.end(), patch_data.begin() + patch_offset, patch_data.begin() + patch_offset + action_length);
    patch_offset += action_length;
    return true;
}

bool do_source_copy(std::span<const uint8_t> patch_data, std::span<const uint8_t> rom, std::vector<uint8_t>& ret, size_t& patch_offset, size_t& source_offset, uint64_t action_length) {
    int64_t copy_offset;
    if (!read_signed_number(patch_data, patch_offset, copy_offset)) {
        return false;
    }

    source_offset += copy_offset;
    size_t read_end = source_offset + action_length;
    if (read_end > rom.size()) {
        return false;
    }

    ret.insert(ret.end(), rom.begin() + source_offset, rom.begin() + source_offset + action_length);
    source_offset += action_length;
    return true;
}

bool do_target_copy(std::span<const uint8_t> patch_data, std::span<const uint8_t> rom, std::vector<uint8_t>& ret, size_t& patch_offset, size_t& target_offset, uint64_t action_length) {
    int64_t copy_offset;
    if (!read_signed_number(patch_data, patch_offset, copy_offset)) {
        return false;
    }

    target_offset += copy_offset;
    if (target_offset >= ret.size()) {
        return false;
    }
    
    // Copy one byte at a time to allow for overlap.
    size_t output_start = ret.size();
    ret.resize(output_start + action_length);
    for (size_t i = 0; i < action_length; i++) {
        ret[output_start + i] = ret[target_offset++];
    }

    return true;
}

static const uint32_t crc_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32_t calculate_crc32(std::span<const uint8_t> data) {
    uint32_t crc;

    crc = 0xFFFFFFFF;
    for (size_t i = 0; i < data.size(); i++) {
        crc = (crc >> 8) ^ crc_table[ (crc ^ data[i]) & 0xFF ];
    }
    return crc ^ 0xFFFFFFFF;
}

recomp::patcher::PatcherResult recomp::patcher::patch_rom(std::span<const uint8_t> rom, std::span<const uint8_t> patch_data, std::vector<uint8_t>& patched_rom_out) {
    constexpr size_t footer_size = 3 * sizeof(uint32_t);
    if (patch_data.size() < 4) {
        return PatcherResult::InvalidPatchFile;
    }

    if (patch_data[0] != 'B' || patch_data[1] != 'P' || patch_data[2] != 'S' || patch_data[3] != '1') {
        return PatcherResult::InvalidPatchFile;
    }

    size_t patch_offset = 4;
    size_t source_offset = 0;
    size_t target_offset = 0;

    // Read the header fields.
    uint64_t source_size;
    uint64_t target_size;
    uint64_t metadata_size;
    if (!read_number(patch_data, patch_offset, source_size)) {
        return PatcherResult::InvalidPatchFile;
    }
    if (!read_number(patch_data, patch_offset, target_size)) {
        return PatcherResult::InvalidPatchFile;
    }
    if (!read_number(patch_data, patch_offset, metadata_size)) {
        return PatcherResult::InvalidPatchFile;
    }

    // Skip the metadata.
    patch_offset += metadata_size;
    if (patch_offset >= patch_data.size()) {
        return PatcherResult::InvalidPatchFile;
    }

    // Early phase validation to rule out very incorrect patch files.
    if (source_size != rom.size()) {
        return PatcherResult::WrongRom;
    }

    std::vector<uint8_t> ret;
    ret.reserve(target_size);

    // Read and apply actions.
    size_t actions_end = patch_data.size() - footer_size;
    while (patch_offset < actions_end) {
        uint64_t cur_action_number;
        if (!read_number(patch_data, patch_offset, cur_action_number)) {
            return PatcherResult::InvalidPatchFile;
        }
        uint64_t action_length = (cur_action_number >> 2) + 1;
        PatchActionType action_type = static_cast<PatchActionType>(cur_action_number & 0b11); 
        switch (action_type) {
            case PatchActionType::SourceRead:
                if (!do_source_read(patch_data, rom, ret, patch_offset, action_length)) {
                    return PatcherResult::InvalidPatchFile;
                }
                break;
            case PatchActionType::TargetRead:
                if (!do_target_read(patch_data, rom, ret, patch_offset, action_length)) {
                    return PatcherResult::InvalidPatchFile;
                }
                break;
            case PatchActionType::SourceCopy:
                if (!do_source_copy(patch_data, rom, ret, patch_offset, source_offset, action_length)) {
                    return PatcherResult::InvalidPatchFile;
                }
                break;
            case PatchActionType::TargetCopy:
                if (!do_target_copy(patch_data, rom, ret, patch_offset, target_offset, action_length)) {
                    return PatcherResult::InvalidPatchFile;
                }
                break;
        }
    }

    // Read the checksums from the patch file.
    uint32_t good_source_checksum;
    uint32_t good_target_checksum;
    uint32_t good_patch_checksum;
    if (!read_u32(patch_data, patch_offset, good_source_checksum)) {
        return PatcherResult::InvalidPatchFile;
    }
    if (!read_u32(patch_data, patch_offset, good_target_checksum)) {
        return PatcherResult::InvalidPatchFile;
    }
    if (!read_u32(patch_data, patch_offset, good_patch_checksum)) {
        return PatcherResult::InvalidPatchFile;
    }

    // Make sure all the patch data was consumed.
    if (patch_offset != patch_data.size()) {
        return PatcherResult::InvalidPatchFile;
    }

    // Checksums are skipped as these patches aren't applied by end users. Any issues with a patch included in a mod would be caught immediately in mod testing.

    // uint32_t source_checksum = calculate_crc32(rom);
    // if (source_checksum != good_source_checksum) {
    //     return PatcherResult::WrongRom;
    // }
    
    // uint32_t target_checksum = calculate_crc32(std::span{ ret });
    // if (target_checksum != good_target_checksum) {
    //     return PatcherResult::InvalidPatchFile;
    // }

    // uint32_t patch_checksum = calculate_crc32(patch_data.subspan(0, patch_data.size() - 4));
    // if (patch_checksum != good_patch_checksum) {
    //     return PatcherResult::InvalidPatchFile;
    // }

    patched_rom_out = std::move(ret);
    return PatcherResult::Success;
}
