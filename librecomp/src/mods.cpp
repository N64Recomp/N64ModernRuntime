#include <span>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "librecomp/mods.hpp"
#include "librecomp/overlays.hpp"
#include "n64recomp.h"

void unprotect(void* target_func, DWORD* old_flags) {
    BOOL result = VirtualProtect(target_func,
        16,
        PAGE_READWRITE,
        old_flags);
}

void protect(void* target_func, DWORD old_flags) {
    DWORD dummy_old_flags;
    BOOL result = VirtualProtect(target_func,
        16,
        old_flags,
        &dummy_old_flags);
}

void patch_func(void* target_func, void* replacement_func) {
    static uint8_t movabs_rax[] = {0x48, 0xB8};
    static uint8_t jmp_rax[] = {0xFF, 0xE0};
    uint8_t* target_func_u8 = reinterpret_cast<uint8_t*>(target_func);
    size_t offset = 0;

    auto write_bytes = [&](void* bytes, size_t count) {
        memcpy(target_func_u8 + offset, bytes, count);
        offset += count;
    };

    DWORD old_flags;
    unprotect(target_func, &old_flags);
    write_bytes(movabs_rax, sizeof(movabs_rax));
    write_bytes(&replacement_func, sizeof(&replacement_func));
    write_bytes(jmp_rax, sizeof(jmp_rax));
    protect(target_func, old_flags);
}

recomp::mods::ModLoadError recomp::mods::load_mod(uint8_t* rdram, const ModManifest& manifest, int32_t load_address, uint32_t& ram_used, std::string& error_param) {
    N64Recomp::Context context_out{};
    N64Recomp::ModContext mod_context_out{};
    std::vector<int32_t> section_load_addresses{};

    {
        // Load the mod symbol data from the file provided in the manifest.
        bool binary_syms_exists = false;
        std::vector<char> syms_data = manifest.mod_handle->read_file(manifest.binary_syms_path, binary_syms_exists);

        if (!binary_syms_exists) {
            return recomp::mods::ModLoadError::FailedToLoadSyms;
        }
        
        // Load the binary data from the file provided in the manifest.
        bool binary_exists = false;
        std::vector<char> binary_data = manifest.mod_handle->read_file(manifest.binary_path, binary_exists);

        if (!binary_exists) {
            return recomp::mods::ModLoadError::FailedToLoadBinary;
        }

        std::span<uint8_t> binary_span {reinterpret_cast<uint8_t*>(binary_data.data()), binary_data.size() };

        // Parse the symbol file into the recompiler contexts.
        N64Recomp::ModSymbolsError symbol_load_error = N64Recomp::parse_mod_symbols(syms_data, binary_span, {}, context_out, mod_context_out);
        if (symbol_load_error != N64Recomp::ModSymbolsError::Good) {
            return ModLoadError::FailedToLoadSyms;
        }
        
        section_load_addresses.resize(context_out.sections.size());
        
        // Copy each section's binary into rdram, leaving room for the section's bss before the next one.
        int32_t cur_section_addr = load_address;
        for (size_t section_index = 0; section_index < context_out.sections.size(); section_index++) {
            const auto& section = context_out.sections[section_index];
            for (size_t i = 0; i < section.size; i++) {
                MEM_B(i, (gpr)cur_section_addr) = binary_data[section.rom_addr + i];
            }
            section_load_addresses[section_index] = cur_section_addr;
            cur_section_addr += section.size + section.bss_size;
        }

        ram_used = cur_section_addr - load_address;
    }

    // TODO temporary solution for loading mod DLLs, replace with LuaJIT recompilation (including patching LO16/HI16 relocs).
    // N64Recomp::recompile_function(...);
    static HMODULE mod_dll;
    std::filesystem::path dll_path = manifest.mod_root_path;
    dll_path.replace_extension(".dll");
    mod_dll = LoadLibraryW(dll_path.c_str());

    if (!mod_dll) {
        printf("Failed to open mod dll: %ls\n", dll_path.c_str());
        return ModLoadError::Good;
    }

    // TODO track replacements by mod to find conflicts
    uint32_t total_func_count = 0;
    for (size_t section_index = 0; section_index < context_out.sections.size(); section_index++) {
        const auto& section = context_out.sections[section_index];
        const auto& mod_section = mod_context_out.section_info[section_index];
        // TODO check that section original_vrom is nonzero if it has replacements.
        for (const auto& replacement : mod_section.replacements) {
            recomp_func_t* to_replace = recomp::overlays::get_func_by_section_ram(mod_section.original_rom_addr, replacement.original_vram);

            if (to_replace == nullptr) {
                std::stringstream error_param_stream{};
                error_param_stream << std::hex <<
                    "section: 0x" << mod_section.original_rom_addr <<
                    " func: 0x" << std::setfill('0') << std::setw(8) << replacement.original_vram;
                error_param = error_param_stream.str();
                return ModLoadError::InvalidFunctionReplacement;
            }

            uint32_t section_func_index = replacement.func_index;

            // TODO temporary solution for loading mod DLLs, replace with LuaJIT recompilation.
            std::string section_func_name = "mod_func_" + std::to_string(total_func_count + section_func_index);
            void* replacement_func = GetProcAddress(mod_dll, section_func_name.c_str());

            if (!replacement_func) {
                printf("Failed to find func in dll: %s\n", section_func_name.c_str());
                return ModLoadError::Good;
            }

            printf("found replacement func: 0x%016llX\n", (uintptr_t)to_replace);

            patch_func(to_replace, replacement_func);
        }
        total_func_count += mod_section.replacements.size();
    }

    // TODO perform mips32 relocations

    return ModLoadError::Good;
}

