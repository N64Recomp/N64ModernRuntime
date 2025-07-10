#ifndef __RECOMP_PATCHER_HPP__
#define __RECOMP_PATCHER_HPP__

#include <cstdint>
#include <span>
#include <vector>

namespace recomp {
    namespace patcher {
        enum class PatcherResult {
            Success,
            InvalidPatchFile,
            WrongRom,
        };

        PatcherResult patch_rom(std::span<const uint8_t> rom, std::span<const uint8_t> patch_data, std::vector<uint8_t>& patched_rom_out);
    }
}

#endif
