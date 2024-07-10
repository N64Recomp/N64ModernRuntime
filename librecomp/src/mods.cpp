#include <span>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "librecomp/mods.hpp"
#include "librecomp/overlays.hpp"
#include "librecomp/game.hpp"
#include "n64recomp.h"

#if defined(_WIN32)
#define PATHFMT "%ls"
#else
#define PATHFMT "%s"
#endif

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

void unpatch_func(void* target_func, const recomp::mods::PatchData& data) {
    DWORD old_flags;
    unprotect(target_func, &old_flags);
    memcpy(target_func, data.replaced_bytes.data(), data.replaced_bytes.size());
    protect(target_func, old_flags);
}

namespace recomp {
    namespace mods {
        struct ModHandle {
            ModManifest manifest;
            N64Recomp::Context recompiler_context;
            N64Recomp::ModContext recompiler_mod_context;
            // TODO temporary solution for loading mod DLLs, replace with LuaJIT recompilation (including patching LO16/HI16 relocs).
            HMODULE mod_dll;

            ModHandle(ModManifest&& manifest) : manifest(std::move(manifest)), recompiler_context{}, recompiler_mod_context{} {}
        };
    }
}

void recomp::mods::ModContext::add_opened_mod(ModManifest&& manifest) {
    opened_mods.emplace_back(std::move(manifest));
}

recomp::mods::ModLoadError load_mod(uint8_t* rdram, recomp::mods::ModHandle& handle, int32_t load_address, uint32_t& ram_used, std::string& error_param, std::unordered_map<recomp_func_t*, recomp::mods::PatchData>& patched_funcs) {
    using namespace recomp::mods;
    std::vector<int32_t> section_load_addresses{};

    {
        // Load the mod symbol data from the file provided in the manifest.
        bool binary_syms_exists = false;
        std::vector<char> syms_data = handle.manifest.file_handle->read_file(handle.manifest.binary_syms_path, binary_syms_exists);

        if (!binary_syms_exists) {
            return recomp::mods::ModLoadError::FailedToLoadSyms;
        }
        
        // Load the binary data from the file provided in the manifest.
        bool binary_exists = false;
        std::vector<char> binary_data = handle.manifest.file_handle->read_file(handle.manifest.binary_path, binary_exists);

        if (!binary_exists) {
            return recomp::mods::ModLoadError::FailedToLoadBinary;
        }

        std::span<uint8_t> binary_span {reinterpret_cast<uint8_t*>(binary_data.data()), binary_data.size() };

        // Parse the symbol file into the recompiler contexts.
        N64Recomp::ModSymbolsError symbol_load_error = N64Recomp::parse_mod_symbols(syms_data, binary_span, {}, handle.recompiler_context, handle.recompiler_mod_context);
        if (symbol_load_error != N64Recomp::ModSymbolsError::Good) {
            return ModLoadError::FailedToLoadSyms;
        }
        
        section_load_addresses.resize(handle.recompiler_context.sections.size());
        
        // Copy each section's binary into rdram, leaving room for the section's bss before the next one.
        int32_t cur_section_addr = load_address;
        for (size_t section_index = 0; section_index < handle.recompiler_context.sections.size(); section_index++) {
            const auto& section = handle.recompiler_context.sections[section_index];
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
    std::filesystem::path dll_path = handle.manifest.mod_root_path;
    dll_path.replace_extension(".dll");
    handle.mod_dll = LoadLibraryW(dll_path.c_str());

    if (!handle.mod_dll) {
        printf("Failed to open mod dll: %ls\n", dll_path.c_str());
        return ModLoadError::Good;
    }

    // TODO track replacements by mod to find conflicts
    uint32_t total_func_count = 0;
    for (size_t section_index = 0; section_index < handle.recompiler_context.sections.size(); section_index++) {
        const auto& section = handle.recompiler_context.sections[section_index];
        const auto& mod_section = handle.recompiler_mod_context.section_info[section_index];
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
            void* replacement_func = GetProcAddress(handle.mod_dll, section_func_name.c_str());

            if (!replacement_func) {
                printf("Failed to find func in dll: %s\n", section_func_name.c_str());
                return ModLoadError::FailedToFindReplacement;
            }

            printf("found replacement func: 0x%016llX\n", (uintptr_t)to_replace);

            // Check if this function has already been replaced.
            auto find_patch_it = patched_funcs.find(to_replace);
            if (find_patch_it != patched_funcs.end()) {
                error_param = find_patch_it->second.mod_id;
                return ModLoadError::ModConflict;
            }

            // Copy the original bytes so they can be restored later after the mod is unloaded.
            PatchData& cur_replacement_data = patched_funcs[to_replace];
            memcpy(cur_replacement_data.replaced_bytes.data(), to_replace, cur_replacement_data.replaced_bytes.size());
            cur_replacement_data.mod_id = handle.manifest.mod_id;

            // Patch the function to redirect it to the replacement.
            patch_func(to_replace, replacement_func);
        }
        total_func_count += mod_section.replacements.size();
    }

    // TODO perform mips32 relocations

    return ModLoadError::Good;
}

std::vector<recomp::mods::ModOpenErrorDetails> recomp::mods::ModContext::scan_mod_folder(const std::filesystem::path& mod_folder) {
    std::vector<recomp::mods::ModOpenErrorDetails> ret{};
    std::error_code ec;
    for (const auto& mod_path : std::filesystem::directory_iterator{mod_folder, std::filesystem::directory_options::skip_permission_denied, ec}) {
        if ((mod_path.is_regular_file() && mod_path.path().extension() == ".zip") || mod_path.is_directory()) {
            printf("Opening mod " PATHFMT "\n", mod_path.path().stem().c_str());
            std::string open_error_param;
            ModOpenError open_error = open_mod(mod_path, open_error_param);

            if (open_error != ModOpenError::Good) {
                ret.emplace_back(mod_path.path(), open_error, open_error_param);
            }
        }
        else {
            printf("Skipping non-mod " PATHFMT PATHFMT "\n", mod_path.path().stem().c_str(), mod_path.path().extension().c_str());
        }
    }

    return ret;
}

// Nothing needed for these two, they just need to be explicitly declared outside the header to allow forward declaration of ModHandle.
recomp::mods::ModContext::ModContext() {}
recomp::mods::ModContext::~ModContext() {}

void recomp::mods::ModContext::enable_mod(const std::string& mod_id, bool enabled) {
    if (enabled) {
        enabled_mods.emplace(mod_id);
    }
    else {
        enabled_mods.erase(mod_id);
    }
}

bool recomp::mods::ModContext::is_mod_enabled(const std::string& mod_id) {
    return enabled_mods.contains(mod_id);
}

size_t recomp::mods::ModContext::num_opened_mods() {
    return opened_mods.size();
}

std::vector<recomp::mods::ModLoadErrorDetails> recomp::mods::ModContext::load_mods(uint8_t* rdram, int32_t load_address, uint32_t& ram_used) {
    std::vector<recomp::mods::ModLoadErrorDetails> ret{};
    ram_used = 0;

    if (!patched_funcs.empty()) {
        printf("Mods already loaded!\n");
        return {};
    }

    for (auto& mod : opened_mods) {
        if (enabled_mods.contains(mod.manifest.mod_id)) {
            printf("Loading mod %s\n", mod.manifest.mod_id.c_str());
            uint32_t cur_ram_used = 0;
            std::string load_error_param;
            ModLoadError load_error = load_mod(rdram, mod, load_address, cur_ram_used, load_error_param, patched_funcs);

            if (load_error != ModLoadError::Good) {
                ret.emplace_back(mod.manifest.mod_id, load_error, load_error_param);
            }
            else {
                load_address += cur_ram_used;
                ram_used += cur_ram_used;
            }
        }
    }

    if (!ret.empty()) {
        printf("Mod loading failed, unpatching funcs\n");
        unload_mods();
    }

    return ret;
}

void recomp::mods::ModContext::unload_mods() {
    for (auto& [replacement_func, replacement_data] : patched_funcs) {
        unpatch_func(replacement_func, replacement_data);
    }
    patched_funcs.clear();
}
