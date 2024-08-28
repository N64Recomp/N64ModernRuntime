#include <span>
#include <fstream>
#include <variant>
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

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace recomp {
    namespace mods {
        using ModFunction = std::variant<recomp_func_t*>;

        class ModCodeHandle {
        public:
            virtual ~ModCodeHandle() {}
            virtual bool good() = 0;
            virtual void set_imported_function_pointer(size_t import_index, recomp_func_t* ptr) = 0;
            virtual void set_reference_symbol_pointer(size_t symbol_index, recomp_func_t* ptr) = 0;
            virtual void set_event_index(size_t local_event_index, uint32_t global_event_index) = 0;
            virtual void set_recomp_trigger_event_pointer(void (*ptr)(uint8_t* rdram, recomp_context* ctx, uint32_t index)) = 0;
            virtual void set_get_function_pointer(recomp_func_t* (*ptr)(int32_t)) = 0;
            virtual void set_reference_section_addresses_pointer(int32_t* ptr) = 0;
            virtual void set_local_section_address(size_t section_index, int32_t address) = 0;
            virtual ModFunction get_function_handle(size_t func_index) = 0;
        };

        class NativeCodeHandle : public ModCodeHandle {
        public:
            NativeCodeHandle(const std::filesystem::path& dll_path, const N64Recomp::Context& context) {
                // Load the DLL.
                mod_dll = LoadLibraryW(dll_path.c_str());
                if (mod_dll == nullptr) {
                    set_bad();
                    return;
                }

                // Fill out the list of function pointers.
                functions.resize(context.functions.size());
                for (size_t i = 0; i < functions.size(); i++) {
                    std::string func_name = "mod_func_" + std::to_string(i);
                    functions[i] = (recomp_func_t*)GetProcAddress(mod_dll, func_name.c_str());
                    if (functions[i] == nullptr) {
                        set_bad();
                        return;
                    }
                }

                // Get the standard exported symbols.
                get_dll_func(imported_funcs, "imported_funcs");
                get_dll_func(reference_symbol_funcs, "reference_symbol_funcs");
                get_dll_func(event_indices, "event_indices");
                get_dll_func(recomp_trigger_event, "recomp_trigger_event");
                get_dll_func(get_function, "get_function");
                get_dll_func(reference_section_addresses, "reference_section_addresses");
                get_dll_func(section_addresses, "section_addresses");
            }
            ~NativeCodeHandle() = default;
            bool good() final {
                return mod_dll != nullptr;
            }
            void set_imported_function_pointer(size_t import_index, recomp_func_t* ptr) final {
                imported_funcs[import_index] = ptr;
            }
            void set_reference_symbol_pointer(size_t symbol_index, recomp_func_t* ptr) final {
                reference_symbol_funcs[symbol_index] = ptr;
            };
            void set_event_index(size_t local_event_index, uint32_t global_event_index) final {
                event_indices[local_event_index] = global_event_index;
            };
            void set_recomp_trigger_event_pointer(void (*ptr)(uint8_t* rdram, recomp_context* ctx, uint32_t index)) final {
                *recomp_trigger_event = ptr;
            };
            void set_get_function_pointer(recomp_func_t* (*ptr)(int32_t)) final {
                *get_function = ptr;
            };
            void set_reference_section_addresses_pointer(int32_t* ptr) final {
                *reference_section_addresses = ptr;
            };
            void set_local_section_address(size_t section_index, int32_t address) final {
                section_addresses[section_index] = address;
            };
            ModFunction get_function_handle(size_t func_index) final {
                return ModFunction{ functions[func_index] };
            }
        private:
            template <typename T>
            void get_dll_func(T& out, const char* name) {
                out = (T)GetProcAddress(mod_dll, name);
                if (out == nullptr) {
                    set_bad();
                }
            };
            void set_bad() {
                if (mod_dll) {
                    FreeLibrary(mod_dll);
                }
                mod_dll = nullptr;
            }
            HMODULE mod_dll;
            std::vector<recomp_func_t*> functions;
            recomp_func_t** imported_funcs;
            recomp_func_t** reference_symbol_funcs;
            uint32_t* event_indices;
            void (**recomp_trigger_event)(uint8_t* rdram, recomp_context* ctx, uint32_t index);
            recomp_func_t* (**get_function)(int32_t vram);
            int32_t** reference_section_addresses;
            int32_t* section_addresses;
        };

        struct ModHandle {
            ModManifest manifest;
            std::unique_ptr<ModCodeHandle> code_handle;
            N64Recomp::Context recompiler_context;

            ModHandle(ModManifest&& manifest) :
                manifest(std::move(manifest)), code_handle(), recompiler_context{} {
            }
        };
    }
}

void unprotect(void* target_func, DWORD* old_flags) {
    BOOL result = VirtualProtect(target_func,
        16,
        PAGE_READWRITE,
        old_flags);
    (void)result;
}

void protect(void* target_func, DWORD old_flags) {
    DWORD dummy_old_flags;
    BOOL result = VirtualProtect(target_func,
        16,
        old_flags,
        &dummy_old_flags);
    (void)result;
}

void patch_func(recomp_func_t* target_func, recomp::mods::ModFunction replacement_func) {
    static const uint8_t movabs_rax[] = {0x48, 0xB8};
    static const uint8_t jmp_rax[] = {0xFF, 0xE0};
    uint8_t* target_func_u8 = reinterpret_cast<uint8_t*>(target_func);
    size_t offset = 0;

    auto write_bytes = [&](const void* bytes, size_t count) {
        memcpy(target_func_u8 + offset, bytes, count);
        offset += count;
    };

    DWORD old_flags;
    unprotect(target_func_u8, &old_flags);

    std::visit(overloaded {
        [&write_bytes](recomp_func_t* native_func) {
           write_bytes(movabs_rax, sizeof(movabs_rax));
           write_bytes(&native_func, sizeof(&native_func));
           write_bytes(jmp_rax, sizeof(jmp_rax));
        }
    }, replacement_func);

    protect(target_func_u8, old_flags);
}

void unpatch_func(void* target_func, const recomp::mods::PatchData& data) {
    DWORD old_flags;
    unprotect(target_func, &old_flags);
    memcpy(target_func, data.replaced_bytes.data(), data.replaced_bytes.size());
    protect(target_func, old_flags);
}

void recomp::mods::ModContext::add_opened_mod(ModManifest&& manifest) {
    opened_mods.emplace_back(std::move(manifest));
}

recomp::mods::ModLoadError recomp::mods::ModContext::load_mod(uint8_t* rdram, const std::unordered_map<uint32_t, uint16_t>& section_vrom_map, recomp::mods::ModHandle& handle, int32_t load_address, uint32_t& ram_used, std::string& error_param) {
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
        N64Recomp::ModSymbolsError symbol_load_error = N64Recomp::parse_mod_symbols(syms_data, binary_span, section_vrom_map, handle.recompiler_context);
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

    // TODO implement LuaJIT recompilation and allow it instead of native code loading via a mod manifest flag.
    std::filesystem::path dll_path = handle.manifest.mod_root_path;
    dll_path.replace_extension(".dll");
    handle.code_handle = std::make_unique<NativeCodeHandle>(dll_path, handle.recompiler_context);
    if (!handle.code_handle->good()) {
        printf("Failed to open mod dll: %ls\n", dll_path.c_str());
        handle.code_handle.reset();
        return ModLoadError::FailedToLoadNativeCode;
    }

    // TODO dependency resolution

    // TODO imported_funcs from other mods
    for (size_t import_index = 0; import_index < handle.recompiler_context.import_symbols.size(); import_index++) {
        const N64Recomp::ImportSymbol& imported_func = handle.recompiler_context.import_symbols[import_index];
        const N64Recomp::Dependency& dependency = handle.recompiler_context.dependencies[imported_func.dependency_index];

        recomp_func_t* found_func = nullptr;

        if (dependency.mod_id == "*") {
            found_func = recomp::overlays::get_base_export(imported_func.base.name);
        }

        if (found_func == nullptr) {
            error_param = dependency.mod_id + ":" + imported_func.base.name;
            return ModLoadError::InvalidImport;
        }

        handle.code_handle->set_imported_function_pointer(import_index, found_func);
    }

    for (size_t reference_sym_index = 0; reference_sym_index < handle.recompiler_context.num_regular_reference_symbols(); reference_sym_index++) {
        const N64Recomp::ReferenceSymbol& reference_sym = handle.recompiler_context.get_regular_reference_symbol(reference_sym_index);
        uint32_t reference_section_vrom = handle.recompiler_context.get_reference_section_rom(reference_sym.section_index);
        uint32_t reference_section_vram = handle.recompiler_context.get_reference_section_vram(reference_sym.section_index);
        uint32_t reference_symbol_vram = reference_section_vram + reference_sym.section_offset;

        recomp_func_t* found_func = recomp::overlays::get_func_by_section_ram(reference_section_vrom, reference_symbol_vram);

        if (found_func == nullptr) {
            std::stringstream error_param_stream{};
            error_param_stream << std::hex <<
                "section: 0x" << reference_section_vrom <<
                " func: 0x" << std::setfill('0') << std::setw(8) << reference_symbol_vram;
            error_param = error_param_stream.str();
            return ModLoadError::InvalidReferenceSymbol;
        }

        handle.code_handle->set_reference_symbol_pointer(reference_sym_index, found_func);
    }

    // TODO event_indices
    // TODO recomp_trigger_event

    handle.code_handle->set_get_function_pointer(get_function);
    handle.code_handle->set_reference_section_addresses_pointer(section_addresses);

    for (size_t section_index = 0; section_index < section_load_addresses.size(); section_index++) {
        handle.code_handle->set_local_section_address(section_index, section_load_addresses[section_index]);
    }

    // Apply all the function replacements in the mod.
    for (const auto& replacement : handle.recompiler_context.replacements) {
        recomp_func_t* to_replace = recomp::overlays::get_func_by_section_ram(replacement.original_section_vrom, replacement.original_vram);

        if (to_replace == nullptr) {
            std::stringstream error_param_stream{};
            error_param_stream << std::hex <<
                "section: 0x" << replacement.original_section_vrom <<
                " func: 0x" << std::setfill('0') << std::setw(8) << replacement.original_vram;
            error_param = error_param_stream.str();
            return ModLoadError::InvalidFunctionReplacement;
        }

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
        patch_func(to_replace, handle.code_handle->get_function_handle(replacement.func_index));
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

    const std::unordered_map<uint32_t, uint16_t>& section_vrom_map = recomp::overlays::get_vrom_to_section_map();

    for (auto& mod : opened_mods) {
        if (enabled_mods.contains(mod.manifest.mod_id)) {
            printf("Loading mod %s\n", mod.manifest.mod_id.c_str());
            uint32_t cur_ram_used = 0;
            std::string load_error_param;
            ModLoadError load_error = load_mod(rdram, section_vrom_map, mod, load_address, cur_ram_used, load_error_param);

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
