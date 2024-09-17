#include <span>
#include <fstream>
#include <sstream>

#include "librecomp/mods.hpp"
#include "librecomp/overlays.hpp"
#include "librecomp/game.hpp"
#include "n64recomp.h"

// Architecture detection.

// MSVC x86_64
#if defined (_M_AMD64) && (_M_AMD64 == 100) && !defined (_M_ARM64EC)
#   define IS_X86_64
// GCC/Clang x86_64
#elif defined(__x86_64__)
#   define IS_X86_64
// MSVC/GCC/Clang ARM64
#elif defined(__ARM_ARCH_ISA_A64)
#   define IS_ARM64
#else
#   error "Unsupported architecture!"
#endif


#if defined(_WIN32)
#define PATHFMT "%ls"
#else
#define PATHFMT "%s"
#endif

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include "Windows.h"

class recomp::mods::DynamicLibrary {
public:
    static constexpr std::string_view PlatformExtension = ".dll";
    DynamicLibrary() = default;
    DynamicLibrary(const std::filesystem::path& path) {
        native_handle = LoadLibraryW(path.c_str());

        if (good()) {
            uint32_t* recomp_api_version;
            if (get_dll_symbol(recomp_api_version, "recomp_api_version")) {
                api_version = *recomp_api_version;
            }
            else {
                api_version = (uint32_t)-1;
            }
        }
    }
    ~DynamicLibrary() {
        unload();
    }
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    DynamicLibrary(DynamicLibrary&&) = delete;
    DynamicLibrary& operator=(DynamicLibrary&&) = delete;

    void unload() {
        if (native_handle != nullptr) {
            FreeLibrary(native_handle);
        }
        native_handle = nullptr;
    }

    bool good() const {
        return native_handle != nullptr;
    }

    template <typename T>
    bool get_dll_symbol(T& out, const char* name) const {
        out = (T)GetProcAddress(native_handle, name);
        if (out == nullptr) {
            return false;
        }
        return true;
    };

    uint32_t get_api_version() {
        return api_version;
    }
private:
    HMODULE native_handle;
    uint32_t api_version;
};

void unprotect(void* target_func, uint64_t* old_flags) {
    DWORD old_flags_dword;
    BOOL result = VirtualProtect(target_func,
        16,
        PAGE_READWRITE,
        &old_flags_dword);
    *old_flags = old_flags_dword;
    (void)result;
}

void protect(void* target_func, uint64_t old_flags) {
    DWORD dummy_old_flags;
    BOOL result = VirtualProtect(target_func,
        16,
        static_cast<DWORD>(old_flags),
        &dummy_old_flags);
    (void)result;
}
#else
#  include <unistd.h>
#  include <dlfcn.h>
#  include <sys/mman.h>

class recomp::mods::DynamicLibrary {
public:
    #if defined(__APPLE__)
    static constexpr std::string_view PlatformExtension = ".dylib";
    #else
    static constexpr std::string_view PlatformExtension = ".so";
    #endif
    DynamicLibrary() = default;
    DynamicLibrary(const std::filesystem::path& path) {
        native_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);

        if (good()) {
            uint32_t* recomp_api_version;
            if (get_dll_symbol(recomp_api_version, "recomp_api_version")) {
                api_version = *recomp_api_version;
            }
            else {
                api_version = (uint32_t)-1;
            }
        }
    }
    ~DynamicLibrary() {
        unload();
    }
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    DynamicLibrary(DynamicLibrary&&) = delete;
    DynamicLibrary& operator=(DynamicLibrary&&) = delete;

    void unload() {
        if (native_handle != nullptr) {
            dlclose(native_handle);
        }
        native_handle = nullptr;
    }

    bool good() const {
        return native_handle != nullptr;
    }

    template <typename T>
    bool get_dll_symbol(T& out, const char* name) const {
        out = (T)dlsym(native_handle, name);
        if (out == nullptr) {
            return false;
        }
        return true;
    };

    uint32_t get_api_version() {
        return api_version;
    }
private:
    void* native_handle;
    uint32_t api_version;
};

void unprotect(void* target_func, uint64_t* old_flags) {
    // Align the address to a page boundary.
    uintptr_t page_start = (uintptr_t)target_func;
    int page_size = getpagesize();
    page_start = (page_start / page_size) * page_size;

    int result = mprotect((void*)page_start, page_size, PROT_READ | PROT_WRITE);
    *old_flags = 0;
    (void)result;
}

void protect(void* target_func, uint64_t old_flags) {
    // Align the address to a page boundary.
    uintptr_t page_start = (uintptr_t)target_func;
    int page_size = getpagesize();
    page_start = (page_start / page_size) * page_size;

    int result = mprotect((void*)page_start, page_size, PROT_READ | PROT_EXEC);
    (void)result;
}
#endif

namespace modpaths {
    const std::string binary_path = "mod_binary.bin";
    const std::string binary_syms_path = "mod_syms.bin";
};

recomp::mods::ModLoadError recomp::mods::validate_api_version(uint32_t api_version, std::string& error_param) {
    switch (api_version) {
        case 1:
            return ModLoadError::Good;
        case (uint32_t)-1:
            return ModLoadError::NoSpecifiedApiVersion;
        default:
            error_param = std::to_string(api_version);
            return ModLoadError::UnsupportedApiVersion;
    }
}

recomp::mods::ModHandle::ModHandle(ModManifest&& manifest, std::vector<size_t>&& game_indices) :
    manifest(std::move(manifest)),
    code_handle(),
    recompiler_context{std::make_unique<N64Recomp::Context>()},
    game_indices{std::move(game_indices)}
{

}

recomp::mods::ModHandle::ModHandle(ModHandle&& rhs) = default;
recomp::mods::ModHandle& recomp::mods::ModHandle::operator=(ModHandle&& rhs) = default;
recomp::mods::ModHandle::~ModHandle() = default;

size_t recomp::mods::ModHandle::num_exports() const {
    return recompiler_context->exported_funcs.size();
}

size_t recomp::mods::ModHandle::num_events() const {
    return recompiler_context->event_symbols.size();
}

recomp::mods::ModLoadError recomp::mods::ModHandle::populate_exports(std::string& error_param) {
    for (size_t func_index : recompiler_context->exported_funcs) {
        const auto& func_handle = recompiler_context->functions[func_index];
        exports_by_name.emplace(func_handle.name, func_index);
    }

    return ModLoadError::Good;
}

recomp::mods::ModLoadError recomp::mods::ModHandle::load_native_library(const recomp::mods::NativeLibraryManifest& lib_manifest, std::string& error_param) {
    std::string lib_filename = lib_manifest.name + std::string{DynamicLibrary::PlatformExtension};
    std::filesystem::path lib_path = manifest.mod_root_path.parent_path() / lib_filename;

    std::unique_ptr<DynamicLibrary>& lib = native_libraries.emplace_back(std::make_unique<DynamicLibrary>(lib_path));

    if (!lib->good()) {
        error_param = lib_filename;
        return ModLoadError::FailedToLoadNativeLibrary;
    }
    
    std::string api_error_param;
    ModLoadError api_error = validate_api_version(lib->get_api_version(), api_error_param);

    if (api_error != ModLoadError::Good) {
        if (api_error_param.empty()) {
            error_param = lib_filename;
        }
        else {
            error_param = lib_filename + ":" + api_error_param;
        }
        return api_error;
    }

    for (const std::string& export_name : lib_manifest.exports) {
        recomp_func_t* cur_func;
        if (native_library_exports.contains(export_name)) {
            error_param = export_name;
            return ModLoadError::DuplicateExport;
        }
        if (!lib->get_dll_symbol(cur_func, export_name.c_str())) {
            error_param = lib_manifest.name + ":" + export_name;
            return ModLoadError::FailedToFindNativeExport;
        }
        native_library_exports.emplace(export_name, cur_func);
    }

    return ModLoadError::Good;
}

bool recomp::mods::ModHandle::get_export_function(const std::string& export_name, GenericFunction& out) const {
    // First, check the code exports.
    auto code_find_it = exports_by_name.find(export_name);
    if (code_find_it != exports_by_name.end()) {
        out = code_handle->get_function_handle(code_find_it->second);
        return true;
    }

    // Next, check the native library exports.
    auto native_find_it = native_library_exports.find(export_name);
    if (native_find_it != native_library_exports.end()) {
        out = native_find_it->second;
        return true;
    }


    // Nothing found.
    return false;
}

recomp::mods::ModLoadError recomp::mods::ModHandle::populate_events(size_t base_event_index, std::string& error_param) {
    for (size_t event_index = 0; event_index < recompiler_context->event_symbols.size(); event_index++) {
        const N64Recomp::EventSymbol& event = recompiler_context->event_symbols[event_index];
        events_by_name.emplace(event.base.name, event_index);
    }

    code_handle->set_base_event_index(base_event_index);
    return ModLoadError::Good;
}

bool recomp::mods::ModHandle::get_global_event_index(const std::string& event_name, size_t& event_index_out) const {
    auto find_it = events_by_name.find(event_name);
    if (find_it == events_by_name.end()) {
        return false;
    }

    event_index_out = code_handle->get_base_event_index() + find_it->second;
    return true;
}

recomp::mods::NativeCodeHandle::NativeCodeHandle(const std::filesystem::path& dll_path, const N64Recomp::Context& context) {
    is_good = true;
    // Load the DLL.
    dynamic_lib = std::make_unique<DynamicLibrary>(dll_path);
    if (!dynamic_lib->good()) {
        is_good = false;
        return;
    }

    // Fill out the list of function pointers.
    functions.resize(context.functions.size());
    for (size_t i = 0; i < functions.size(); i++) {
        if(!context.functions[i].name.empty()) {
            is_good &= dynamic_lib->get_dll_symbol(functions[i], context.functions[i].name.c_str());
        }
        else {
            std::string func_name = "mod_func_" + std::to_string(i);
            is_good &= dynamic_lib->get_dll_symbol(functions[i], func_name.c_str());
        }
        if (!is_good) {
            return;
        }
    }

    // Get the standard exported symbols.
    is_good = true;
    is_good &= dynamic_lib->get_dll_symbol(imported_funcs, "imported_funcs");
    is_good &= dynamic_lib->get_dll_symbol(reference_symbol_funcs, "reference_symbol_funcs");
    is_good &= dynamic_lib->get_dll_symbol(base_event_index, "base_event_index");
    is_good &= dynamic_lib->get_dll_symbol(recomp_trigger_event, "recomp_trigger_event");
    is_good &= dynamic_lib->get_dll_symbol(get_function, "get_function");
    is_good &= dynamic_lib->get_dll_symbol(cop0_status_write, "cop0_status_write");
    is_good &= dynamic_lib->get_dll_symbol(cop0_status_read, "cop0_status_read");
    is_good &= dynamic_lib->get_dll_symbol(switch_error, "switch_error");
    is_good &= dynamic_lib->get_dll_symbol(do_break, "do_break");
    is_good &= dynamic_lib->get_dll_symbol(reference_section_addresses, "reference_section_addresses");
    is_good &= dynamic_lib->get_dll_symbol(section_addresses, "section_addresses");
}

bool recomp::mods::NativeCodeHandle::good()  {
    return dynamic_lib->good() && is_good;
}

uint32_t recomp::mods::NativeCodeHandle::get_api_version() {
    return dynamic_lib->get_api_version();
}

void recomp::mods::NativeCodeHandle::set_bad() {
    dynamic_lib.reset();
    is_good = false;
}

void recomp::mods::NativeCodeHandle::set_imported_function(size_t import_index, GenericFunction func) {
    std::visit(overloaded {
        [this, import_index](recomp_func_t* native_func) {
            imported_funcs[import_index] = native_func;
        }
    }, func);
}

void patch_func(recomp_func_t* target_func, recomp::mods::GenericFunction replacement_func) {
    uint8_t* target_func_u8 = reinterpret_cast<uint8_t*>(target_func);
    size_t offset = 0;

    auto write_bytes = [&](const void* bytes, size_t count) {
        memcpy(target_func_u8 + offset, bytes, count);
        offset += count;
    };

    uint64_t old_flags;
    unprotect(target_func_u8, &old_flags);

#if defined(IS_X86_64)
    static const uint8_t movabs_rax[] = {0x48, 0xB8};
    static const uint8_t jmp_rax[] = {0xFF, 0xE0};
    std::visit(overloaded {
        [&write_bytes](recomp_func_t* native_func) {
           write_bytes(movabs_rax, sizeof(movabs_rax));
           write_bytes(&native_func, sizeof(&native_func));
           write_bytes(jmp_rax, sizeof(jmp_rax));
        }
    }, replacement_func);
#elif defined(IS_ARM64)
    static const uint8_t ldr_x2_8__br_x2[] = {0x42, 0x00, 0x00, 0x58, 0x40, 0x00, 0x1F, 0xD6};
    std::visit(overloaded {
        [&write_bytes](recomp_func_t* native_func) {
           write_bytes(ldr_x2_8__br_x2, sizeof(ldr_x2_8__br_x2));
           write_bytes(&native_func, sizeof(&native_func));
        }
    }, replacement_func);
#else
#   error "Unsupported architecture"
#endif

    protect(target_func_u8, old_flags);
}

void unpatch_func(void* target_func, const recomp::mods::PatchData& data) {
    uint64_t old_flags;
    unprotect(target_func, &old_flags);
    memcpy(target_func, data.replaced_bytes.data(), data.replaced_bytes.size());
    protect(target_func, old_flags);
}

void recomp::mods::ModContext::add_opened_mod(ModManifest&& manifest, std::vector<size_t>&& game_indices) {
    opened_mods.emplace_back(std::move(manifest), std::move(game_indices));
}

recomp::mods::ModLoadError recomp::mods::ModContext::load_mod(uint8_t* rdram, const std::unordered_map<uint32_t, uint16_t>& section_vrom_map, recomp::mods::ModHandle& handle, int32_t load_address, uint32_t& ram_used, std::string& error_param) {
    using namespace recomp::mods;
    handle.section_load_addresses.clear();

    // Check that the mod's minimum recomp version is met.
    if (get_project_version() < handle.manifest.minimum_recomp_version) {
        error_param = handle.manifest.minimum_recomp_version.to_string();
        return recomp::mods::ModLoadError::MinimumRecompVersionNotMet;
    }
    
    // Load the mod symbol data from the file provided in the manifest.
    bool binary_syms_exists = false;
    std::vector<char> syms_data = handle.manifest.file_handle->read_file(modpaths::binary_syms_path, binary_syms_exists);
    
    // Load the binary data from the file provided in the manifest.
    bool binary_exists = false;
    std::vector<char> binary_data = handle.manifest.file_handle->read_file(modpaths::binary_path, binary_exists);

    if (binary_syms_exists && !binary_exists) {
        return recomp::mods::ModLoadError::HasSymsButNoBinary;
    }

    if (binary_exists && !binary_syms_exists) {
        return recomp::mods::ModLoadError::HasBinaryButNoSyms;
    }

    std::span<uint8_t> binary_span {reinterpret_cast<uint8_t*>(binary_data.data()), binary_data.size() };

    // Parse the symbol file into the recompiler context.
    N64Recomp::ModSymbolsError symbol_load_error = N64Recomp::parse_mod_symbols(syms_data, binary_span, section_vrom_map, *handle.recompiler_context);
    if (symbol_load_error != N64Recomp::ModSymbolsError::Good) {
        return ModLoadError::FailedToParseSyms;
    }
    
    const std::vector<N64Recomp::Section>& mod_sections = handle.recompiler_context->sections;
    handle.section_load_addresses.resize(mod_sections.size());
    
    // Copy each section's binary into rdram, leaving room for the section's bss before the next one.
    int32_t cur_section_addr = load_address;
    for (size_t section_index = 0; section_index < mod_sections.size(); section_index++) {
        const auto& section = mod_sections[section_index];
        for (size_t i = 0; i < section.size; i++) {
            MEM_B(i, (gpr)cur_section_addr) = binary_data[section.rom_addr + i];
        }
        handle.section_load_addresses[section_index] = cur_section_addr;
        cur_section_addr += section.size + section.bss_size;

    }

    // Iterate over each section again after loading them to perform R_MIPS_32 relocations.
    for (size_t section_index = 0; section_index < mod_sections.size(); section_index++) {
        const auto& section = mod_sections[section_index];
        uint32_t cur_section_original_vram = section.ram_addr;
        uint32_t cur_section_loaded_vram = handle.section_load_addresses[section_index]; 

        // Perform mips32 relocations for this section.
        for (const auto& reloc : section.relocs) {
            if (reloc.type == N64Recomp::RelocType::R_MIPS_32 && !reloc.reference_symbol) {
                if (reloc.target_section >= mod_sections.size()) {
                    return ModLoadError::FailedToParseSyms;
                }
                // Get the ram address of the word that's being relocated and read its original value.
                int32_t reloc_word_addr = reloc.address - cur_section_original_vram + cur_section_loaded_vram;
                uint32_t reloc_word = MEM_W(0, reloc_word_addr);

                // Determine the original and loaded addresses of the section that the relocation points to.
                uint32_t target_section_original_vram = mod_sections[reloc.target_section].ram_addr;
                uint32_t target_section_loaded_vram = handle.section_load_addresses[reloc.target_section];

                // Recalculate the word and write it back into ram.
                reloc_word += (target_section_loaded_vram - target_section_original_vram);
                MEM_W(0, reloc_word_addr) = reloc_word;           
            }
        }
    }

    ram_used = cur_section_addr - load_address;

    return ModLoadError::Good;
}

void recomp::mods::ModContext::register_game(const std::string& mod_game_id) {
    mod_game_ids.emplace(mod_game_id, mod_game_ids.size());
}

std::vector<recomp::mods::ModOpenErrorDetails> recomp::mods::ModContext::scan_mod_folder(const std::filesystem::path& mod_folder) {
    std::vector<recomp::mods::ModOpenErrorDetails> ret{};
    std::error_code ec;
    for (const auto& mod_path : std::filesystem::directory_iterator{mod_folder, std::filesystem::directory_options::skip_permission_denied, ec}) {
        if ((mod_path.is_regular_file() && mod_path.path().extension() == ".nrm") || mod_path.is_directory()) {
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
recomp::mods::ModContext::ModContext() = default;
recomp::mods::ModContext::~ModContext() = default;

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

std::vector<recomp::mods::ModDetails> recomp::mods::ModContext::get_mod_details(const std::string& mod_game_id) {
    std::vector<ModDetails> ret{};
    bool all_games = mod_game_id.empty();
    size_t game_index = (size_t)-1;

    auto find_game_it = mod_game_ids.find(mod_game_id);
    if (find_game_it != mod_game_ids.end()) {
        game_index = find_game_it->second;
    }

    for (const ModHandle& mod : opened_mods) {
        if (all_games || mod.is_for_game(game_index)) {
            std::vector<Dependency> cur_dependencies{};

            ret.emplace_back(ModDetails{
                .mod_id = mod.manifest.mod_id,
                .version = mod.manifest.version,
                .authors = mod.manifest.authors,
                .dependencies = mod.manifest.dependencies
            });
        }
    }

    return ret;
}

std::vector<recomp::mods::ModLoadErrorDetails> recomp::mods::ModContext::load_mods(const std::string& mod_game_id, uint8_t* rdram, int32_t load_address, uint32_t& ram_used) {
    std::vector<recomp::mods::ModLoadErrorDetails> ret{};
    ram_used = 0;
    num_events = recomp::overlays::num_base_events();

    auto find_index_it = mod_game_ids.find(mod_game_id);
    if (find_index_it == mod_game_ids.end()) {
        ret.emplace_back(mod_game_id, ModLoadError::InvalidGame, std::string{});
        return ret;
    }

    size_t mod_game_index = find_index_it->second;

    if (!patched_funcs.empty()) {
        printf("Mods already loaded!\n");
        return {};
    }

    const std::unordered_map<uint32_t, uint16_t>& section_vrom_map = recomp::overlays::get_vrom_to_section_map();

    std::vector<size_t> active_mods{};

    // Find and load active mods.
    for (size_t mod_index = 0; mod_index < opened_mods.size(); mod_index++) {
        auto& mod = opened_mods[mod_index];
        if (mod.is_for_game(mod_game_index) && enabled_mods.contains(mod.manifest.mod_id)) {
            active_mods.push_back(mod_index);
            loaded_mods_by_id.emplace(mod.manifest.mod_id, mod_index);

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

    // Exit early if errors were found.
    if (!ret.empty()) {
        unload_mods();
        return ret;
    }

    // Check that mod dependencies are met.
    for (size_t mod_index : active_mods) {
        auto& mod = opened_mods[mod_index];
        std::vector<std::pair<ModLoadError, std::string>> cur_errors;
        check_dependencies(mod, cur_errors);

        if (!cur_errors.empty()) {
            for (auto const& [cur_error, cur_error_param] : cur_errors) {
                ret.emplace_back(mod.manifest.mod_id, cur_error, cur_error_param);
            }
        }
    }

    // Exit early if errors were found.
    if (!ret.empty()) {
        unload_mods();
        return ret;
    }

    // Load the code and exports from all mods.
    for (size_t mod_index : active_mods) {
        auto& mod = opened_mods[mod_index];
        std::string cur_error_param;
        ModLoadError cur_error = load_mod_code(mod, cur_error_param);
        if (cur_error != ModLoadError::Good) {
            ret.emplace_back(mod.manifest.mod_id, cur_error, cur_error_param);
        }
    }

    // Exit early if errors were found.
    if (!ret.empty()) {
        unload_mods();
        return ret;
    }

    // Set up the event callbacks based on the number of events allocated.
    recomp::mods::setup_events(num_events);
    
    // Resolve dependencies for all mods.
    for (size_t mod_index : active_mods) {
        auto& mod = opened_mods[mod_index];
        std::string cur_error_param;
        ModLoadError cur_error = resolve_dependencies(mod, cur_error_param);
        if (cur_error != ModLoadError::Good) {
            ret.emplace_back(mod.manifest.mod_id, cur_error, cur_error_param);
        }
    }

    // Exit early if errors were found.
    if (!ret.empty()) {
        unload_mods();
        return ret;
    }

    return ret;
}

void recomp::mods::ModContext::check_dependencies(recomp::mods::ModHandle& mod, std::vector<std::pair<recomp::mods::ModLoadError, std::string>>& errors) {
    errors.clear();
    for (const auto& [cur_dep_id, cur_dep_index] : mod.recompiler_context->dependencies_by_name) {
        // Handle special dependency names.
        if (cur_dep_id == N64Recomp::DependencyBaseRecomp || cur_dep_id == N64Recomp::DependencySelf) {
            continue;
        }

        // Find the dependency in the mod manifest to get its version.
        auto find_manifest_dep_it = mod.manifest.dependencies_by_id.find(cur_dep_id);
        if (find_manifest_dep_it == mod.manifest.dependencies_by_id.end()) {
            errors.emplace_back(ModLoadError::MissingDependencyInManifest, cur_dep_id);
            continue;
        }

        const auto& cur_dep = mod.manifest.dependencies[find_manifest_dep_it->second];

        // Look for the dependency in the loaded mod mapping.
        auto find_loaded_dep_it = loaded_mods_by_id.find(cur_dep_id);
        if (find_loaded_dep_it == loaded_mods_by_id.end()) {
            errors.emplace_back(ModLoadError::MissingDependency, cur_dep_id);
            continue;
        }

        const ModHandle& dep_mod = opened_mods[find_loaded_dep_it->second];
        if (cur_dep.version > dep_mod.manifest.version)
        {
            std::stringstream error_param_stream{};
            error_param_stream << "requires mod \"" << cur_dep.mod_id << "\" " <<
                (int)cur_dep.version.major << "." << (int)cur_dep.version.minor << "." << (int)cur_dep.version.patch << ", got " <<
                (int)dep_mod.manifest.version.major << "." << (int)dep_mod.manifest.version.minor << "." << (int)dep_mod.manifest.version.patch << "";
            errors.emplace_back(ModLoadError::WrongDependencyVersion, error_param_stream.str());
        }
    }
}

recomp::mods::ModLoadError recomp::mods::ModContext::load_mod_code(recomp::mods::ModHandle& mod, std::string& error_param) {
    // TODO implement LuaJIT recompilation and allow it instead of native code loading via a mod manifest flag.
    std::filesystem::path dll_path = mod.manifest.mod_root_path;
    dll_path.replace_extension(DynamicLibrary::PlatformExtension);
    mod.code_handle = std::make_unique<NativeCodeHandle>(dll_path, *mod.recompiler_context);
    if (!mod.code_handle->good()) {
        mod.code_handle.reset();
        error_param = dll_path.string();
        return ModLoadError::FailedToLoadNativeCode;
    }

    std::string cur_error_param;
    ModLoadError cur_error = validate_api_version(mod.code_handle->get_api_version(), cur_error_param);

    if (cur_error != ModLoadError::Good) {
        if (cur_error_param.empty()) {
            error_param = dll_path.filename().string();
        }
        else {
            error_param = dll_path.filename().string() + ":" + std::move(cur_error_param);
        }
        return cur_error;
    }

    // Populate the mod's export map.
    cur_error = mod.populate_exports(cur_error_param);

    if (cur_error != ModLoadError::Good) {
        error_param = std::move(cur_error_param);
        return cur_error;
    }

    // Load any native libraries specified by the mod and validate/register the expors.
    std::filesystem::path parent_path = mod.manifest.mod_root_path.parent_path();
    for (const recomp::mods::NativeLibraryManifest& cur_lib_manifest: mod.manifest.native_libraries) {
        cur_error = mod.load_native_library(cur_lib_manifest, cur_error_param);
        if (cur_error != ModLoadError::Good) {
            error_param = std::move(cur_error_param);
            return cur_error;
        }
    }

    // Populate the mod's event map and set its base event index.
    cur_error = mod.populate_events(num_events, cur_error_param);

    if (cur_error != ModLoadError::Good) {
        error_param = std::move(cur_error_param);
        return cur_error;
    }

    // Allocate the event indices used by the mod.
    num_events += mod.num_events();

    // Add each function from the mod into the function lookup table.
    const std::vector<N64Recomp::Section>& mod_sections = mod.recompiler_context->sections;
    for (size_t func_index = 0; func_index < mod.recompiler_context->functions.size(); func_index++) {
        const auto& func = mod.recompiler_context->functions[func_index];
        if (func.section_index >= mod_sections.size()) {
            return ModLoadError::FailedToParseSyms;
        }
        // Calculate the loaded address of this function.
        int32_t func_address = func.vram - mod_sections[func.section_index].ram_addr + mod.section_load_addresses[func.section_index];

        // Get the handle to the function and add it to the lookup table based on its type.
        recomp::mods::GenericFunction func_handle = mod.code_handle->get_function_handle(func_index);
        std::visit(overloaded{
            [func_address](recomp_func_t* native_func) {
                recomp::overlays::add_loaded_function(func_address, native_func);
            }
            }, func_handle);
    }

    return ModLoadError::Good;
}

recomp::mods::ModLoadError recomp::mods::ModContext::resolve_dependencies(recomp::mods::ModHandle& mod, std::string& error_param) {
    // Reference symbols from the base recomp.1:1 with relocs for offline mods.
    // TODO this won't be needed for LuaJIT recompilation, so move this logic into the code handle.
    size_t reference_symbol_index = 0;
    for (const auto& section : mod.recompiler_context->sections) {
        for (const auto& reloc : section.relocs) {
            if (reloc.type == N64Recomp::RelocType::R_MIPS_26 && reloc.reference_symbol && mod.recompiler_context->is_regular_reference_section(reloc.target_section)) {
                recomp_func_t* cur_func = recomp::overlays::get_func_by_section_index_function_offset(reloc.target_section, reloc.target_section_offset);
                if (cur_func == nullptr) {
                    std::stringstream error_param_stream{};
                    error_param_stream << std::hex <<
                        "section: " << reloc.target_section <<
                        " func offset: 0x" << reloc.target_section_offset;
                    error_param = error_param_stream.str();
                    return ModLoadError::InvalidReferenceSymbol;
                }
                mod.code_handle->set_reference_symbol_pointer(reference_symbol_index, cur_func);
                reference_symbol_index++;
            }
        }
    }

    // Create a list of dependencies ordered by their index in the recompiler context.
    std::vector<std::string> dependencies_ordered{};
    dependencies_ordered.resize(mod.recompiler_context->dependencies_by_name.size());

    for (const auto& [dependency, dependency_index] : mod.recompiler_context->dependencies_by_name) {
        dependencies_ordered[dependency_index] = dependency;
    }

    // Imported symbols.
    for (size_t import_index = 0; import_index < mod.recompiler_context->import_symbols.size(); import_index++) {
        const N64Recomp::ImportSymbol& imported_func = mod.recompiler_context->import_symbols[import_index];
        const std::string& dependency_id = dependencies_ordered[imported_func.dependency_index];

        GenericFunction func_handle{};
        bool did_find_func = false;

        if (dependency_id == N64Recomp::DependencyBaseRecomp) {
            recomp_func_t* func_ptr = recomp::overlays::get_base_export(imported_func.base.name);
            did_find_func = func_ptr != nullptr;
            func_handle = func_ptr;
        }
        else if (dependency_id == N64Recomp::DependencySelf) {
            did_find_func = mod.get_export_function(imported_func.base.name, func_handle);
        }
        else {
            auto find_mod_it = loaded_mods_by_id.find(dependency_id);
            if (find_mod_it == loaded_mods_by_id.end()) {
                error_param = dependency_id;
                return ModLoadError::MissingDependency;
            }
            const auto& dependency = opened_mods[find_mod_it->second];
            did_find_func = dependency.get_export_function(imported_func.base.name, func_handle);
        }

        if (!did_find_func) {
            error_param = dependency_id + ":" + imported_func.base.name;
            return ModLoadError::InvalidImport;
        }

        mod.code_handle->set_imported_function(import_index, func_handle);
    }

    // Register callbacks.
    for (const N64Recomp::Callback& callback : mod.recompiler_context->callbacks) {
        const N64Recomp::DependencyEvent& dependency_event = mod.recompiler_context->dependency_events[callback.dependency_event_index];
        const std::string& dependency_id = dependencies_ordered[dependency_event.dependency_index];
        GenericFunction func = mod.code_handle->get_function_handle(callback.function_index);
        size_t event_index = 0;
        bool did_find_event = false;

        if (dependency_id == N64Recomp::DependencyBaseRecomp) {
            event_index = recomp::overlays::get_base_event_index(dependency_event.event_name);
            if (event_index != (size_t)-1) {
                did_find_event = true;
            }
        }
        else if (dependency_id == N64Recomp::DependencySelf) {
            did_find_event = mod.get_global_event_index(dependency_event.event_name, event_index);
        }
        else {
            auto find_mod_it = loaded_mods_by_id.find(dependency_id);
            if (find_mod_it == loaded_mods_by_id.end()) {
                error_param = dependency_id;
                return ModLoadError::MissingDependency;
            }
            const auto& dependency_mod = opened_mods[find_mod_it->second];
            did_find_event = dependency_mod.get_global_event_index(dependency_event.event_name, event_index);
        }

        if (!did_find_event) {
            error_param = dependency_id + ":" + dependency_event.event_name;
            return ModLoadError::InvalidCallbackEvent;
        }

        recomp::mods::register_event_callback(event_index, func);
    }

    // Populate the mod's state fields.
    mod.code_handle->set_recomp_trigger_event_pointer(recomp_trigger_event);
    mod.code_handle->set_get_function_pointer(get_function);
    mod.code_handle->set_cop0_status_write_pointer(cop0_status_write);
    mod.code_handle->set_cop0_status_read_pointer(cop0_status_read);
    mod.code_handle->set_switch_error_pointer(switch_error);
    mod.code_handle->set_do_break_pointer(do_break);
    mod.code_handle->set_reference_section_addresses_pointer(section_addresses);
    for (size_t section_index = 0; section_index < mod.section_load_addresses.size(); section_index++) {
        mod.code_handle->set_local_section_address(section_index, mod.section_load_addresses[section_index]);
    }

    // Apply all the function replacements in the mod.
    for (const auto& replacement : mod.recompiler_context->replacements) {
        recomp_func_t* to_replace = recomp::overlays::get_func_by_section_rom_function_vram(replacement.original_section_vrom, replacement.original_vram);

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
        memcpy(cur_replacement_data.replaced_bytes.data(), reinterpret_cast<void*>(to_replace), cur_replacement_data.replaced_bytes.size());
        cur_replacement_data.mod_id = mod.manifest.mod_id;

        // Patch the function to redirect it to the replacement.
        patch_func(to_replace, mod.code_handle->get_function_handle(replacement.func_index));
    }

    return ModLoadError::Good;
}

void recomp::mods::ModContext::unload_mods() {
    for (auto& [replacement_func, replacement_data] : patched_funcs) {
        unpatch_func(reinterpret_cast<void*>(replacement_func), replacement_data);
    }
    patched_funcs.clear();
    loaded_mods_by_id.clear();
    recomp::mods::reset_events();
    num_events = recomp::overlays::num_base_events();
}
