#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <iostream>
#include <optional>
#include <mutex>
#include <array>
#include <cinttypes>
#include <cuchar>
#include <charconv>

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "librecomp/game.hpp"
#include "xxHash/xxh3.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/error_handling.hpp"
#include "librecomp/addresses.hpp"
#include "librecomp/mods.hpp"
#include "recompiler/live_recompiler.h"

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#else
#    include <sys/mman.h>
#endif

#if defined(_WIN32)
#define PATHFMT "%ls"
#else
#define PATHFMT "%s"
#endif

enum GameStatus {
    None,
    Running,
    Quit
};

// Mutexes
std::mutex game_roms_mutex;
std::mutex current_game_mutex;
std::mutex mod_context_mutex{};

// Global variables
std::filesystem::path config_path;
// Maps game_id to the game's entry.
std::unordered_map<std::u8string, recomp::GameEntry> game_roms {};
// The global mod context.
std::unique_ptr<recomp::mods::ModContext> mod_context = std::make_unique<recomp::mods::ModContext>();
// The project's version.
recomp::Version project_version;
// The current game's save type.
recomp::SaveType save_type = recomp::SaveType::None;

std::u8string recomp::GameEntry::stored_filename() const {
    return game_id + u8".z64";
}

void recomp::register_config_path(std::filesystem::path path) {
    config_path = path;
}

bool recomp::register_game(const recomp::GameEntry& entry) {
    // TODO verify that there's no game with this ID already.
    {
        std::lock_guard<std::mutex> lock(game_roms_mutex);
        game_roms.insert({ entry.game_id, entry });
    }
    if (!entry.mod_game_id.empty()) {
        std::lock_guard<std::mutex> lock(mod_context_mutex);
        mod_context->register_game(entry.mod_game_id);
    }

    return true;
}

void recomp::mods::initialize_mods() {
    N64Recomp::live_recompiler_init();
    std::filesystem::create_directories(config_path / mods_directory);
    std::filesystem::create_directories(config_path / mod_config_directory);
    mod_context->set_mods_config_path(config_path / "mods.json");
    mod_context->set_mod_config_directory(config_path / mod_config_directory);
}

void recomp::mods::scan_mods() {
    std::vector<recomp::mods::ModOpenErrorDetails> mod_open_errors;
    {
        std::lock_guard mod_lock{ mod_context_mutex };
        mod_open_errors = mod_context->scan_mod_folder(config_path / mods_directory);
    }
    for (const auto& cur_error : mod_open_errors) {
        printf("Error opening mod " PATHFMT ": %s (%s)\n", cur_error.mod_path.c_str(), recomp::mods::error_to_string(cur_error.error).c_str(), cur_error.error_param.c_str());
    }

    mod_context->load_mods_config();
}

recomp::mods::ModContentTypeId recomp::mods::register_mod_content_type(const ModContentType& type) {
    std::lock_guard mod_lock{ mod_context_mutex };
    return mod_context->register_content_type(type);
}

bool recomp::mods::register_mod_container_type(const std::string& extension, const std::vector<ModContentTypeId>& content_types, bool requires_manifest) {
    std::lock_guard mod_lock{ mod_context_mutex };
    return mod_context->register_container_type(extension, content_types, requires_manifest);
}

bool check_hash(const std::vector<uint8_t>& rom_data, uint64_t expected_hash) {
    uint64_t calculated_hash = XXH3_64bits(rom_data.data(), rom_data.size());
    return calculated_hash == expected_hash;
}

static std::vector<uint8_t> read_file(const std::filesystem::path& path) {
    std::vector<uint8_t> ret;

    std::ifstream file{ path, std::ios::binary};

    if (file.good()) {
        file.seekg(0, std::ios::end);
        ret.resize(file.tellg());
        file.seekg(0, std::ios::beg);

        file.read(reinterpret_cast<char*>(ret.data()), ret.size());
    }

    return ret;
}

bool write_file(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::ofstream out_file{ path, std::ios::binary };

    if (!out_file.good()) {
        return false;
    }

    out_file.write(reinterpret_cast<const char*>(data.data()), data.size());

    return true;
}

bool check_stored_rom(const recomp::GameEntry& game_entry) {
    std::vector stored_rom_data = read_file(config_path / game_entry.stored_filename());

    if (!check_hash(stored_rom_data, game_entry.rom_hash)) {
        // Incorrect hash, remove the stored ROM file if it exists.
        std::filesystem::remove(config_path / game_entry.stored_filename());
        return false;
    }

    return true;
}

static std::unordered_set<std::u8string> valid_game_roms;

bool recomp::is_rom_valid(std::u8string& game_id) {
    return valid_game_roms.contains(game_id);
}

void recomp::check_all_stored_roms() {
    for (const auto& cur_rom_entry: game_roms) {
        if (check_stored_rom(cur_rom_entry.second)) {
            valid_game_roms.insert(cur_rom_entry.first);
        }
    }
}

bool recomp::load_stored_rom(std::u8string& game_id) {
    auto find_it = game_roms.find(game_id);

    if (find_it == game_roms.end()) {
        return false;
    }
    
    std::vector<uint8_t> stored_rom_data = read_file(config_path / find_it->second.stored_filename());

    if (!check_hash(stored_rom_data, find_it->second.rom_hash)) {
        // The ROM no longer has the right hash, delete it.
        std::filesystem::remove(config_path / find_it->second.stored_filename());
        return false;
    }

    recomp::set_rom_contents(std::move(stored_rom_data));
    return true;
}

const recomp::Version& recomp::get_project_version() {
    return project_version;
}

bool recomp::Version::from_string(const std::string& str, Version& out) {
    std::array<size_t, 2> period_indices;
    size_t cur_pos = 0;
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    std::string suffix;

    // Find the 2 required periods.
    cur_pos = str.find('.', cur_pos);
    period_indices[0] = cur_pos;
    cur_pos = str.find('.', cur_pos + 1);
    period_indices[1] = cur_pos;

    // Check that both were found.
    if (period_indices[0] == std::string::npos || period_indices[1] == std::string::npos) {
        return false;
    }

    // Parse the 3 numbers formed by splitting the string via the periods.
    std::array<std::from_chars_result, 3> parse_results; 
    std::array<size_t, 3> parse_starts { 0, period_indices[0] + 1, period_indices[1] + 1 };
    std::array<size_t, 3> parse_ends { period_indices[0], period_indices[1], str.size() };
    parse_results[0] = std::from_chars(str.data() + parse_starts[0], str.data() + parse_ends[0], major);
    parse_results[1] = std::from_chars(str.data() + parse_starts[1], str.data() + parse_ends[1], minor);
    parse_results[2] = std::from_chars(str.data() + parse_starts[2], str.data() + parse_ends[2], patch);

    // Check that the first two parsed correctly.
    auto did_parse = [&](size_t i) {
        return parse_results[i].ec == std::errc{} && parse_results[i].ptr == str.data() + parse_ends[i];
    };
    
    if (!did_parse(0) || !did_parse(1)) {
        return false;
    }

    // Check that the third had a successful parse, but not necessarily read all the characters.
    if (parse_results[2].ec != std::errc{}) {
        return false;
    }

    // Allow a plus or minus directly after the third number.
    if (parse_results[2].ptr != str.data() + parse_ends[2]) {
        if (*parse_results[2].ptr == '+' || *parse_results[2].ptr == '-') {
            suffix = str.substr(std::distance(str.data(), parse_results[2].ptr));
        }
        // Failed to parse, as nothing is allowed directly after the last number besides a plus or minus.
        else {
            return false;
        }
    }

    out.major = major;
    out.minor = minor;
    out.patch = patch;
    out.suffix = std::move(suffix);
    return true;
}

const std::array<uint8_t, 4> first_rom_bytes { 0x80, 0x37, 0x12, 0x40 };

enum class ByteswapType {
    NotByteswapped,
    Byteswapped4,
    Byteswapped2,
    Invalid
};

ByteswapType check_rom_start(const std::vector<uint8_t>& rom_data) {
    if (rom_data.size() < 4) {
        return ByteswapType::Invalid;
    }

    auto check_match = [&](uint8_t index0, uint8_t index1, uint8_t index2, uint8_t index3) {
        return
            rom_data[0] == first_rom_bytes[index0] &&
            rom_data[1] == first_rom_bytes[index1] &&
            rom_data[2] == first_rom_bytes[index2] &&
            rom_data[3] == first_rom_bytes[index3];
    };

    // Check if the ROM is already in the correct byte order.
    if (check_match(0,1,2,3)) {
        return ByteswapType::NotByteswapped;
    }

    // Check if the ROM has been byteswapped in groups of 4 bytes.
    if (check_match(3,2,1,0)) {
        return ByteswapType::Byteswapped4;
    }

    // Check if the ROM has been byteswapped in groups of 2 bytes.
    if (check_match(1,0,3,2)) {
        return ByteswapType::Byteswapped2;
    }

    // No match found.
    return ByteswapType::Invalid;
}

void byteswap_data(std::vector<uint8_t>& rom_data, size_t index_xor) {
    for (size_t rom_pos = 0; rom_pos < rom_data.size(); rom_pos += 4) {
        uint8_t temp0 = rom_data[rom_pos + 0];
        uint8_t temp1 = rom_data[rom_pos + 1];
        uint8_t temp2 = rom_data[rom_pos + 2];
        uint8_t temp3 = rom_data[rom_pos + 3];

        rom_data[rom_pos + (0 ^ index_xor)] = temp0;
        rom_data[rom_pos + (1 ^ index_xor)] = temp1;
        rom_data[rom_pos + (2 ^ index_xor)] = temp2;
        rom_data[rom_pos + (3 ^ index_xor)] = temp3;
    }
}

recomp::RomValidationError recomp::select_rom(const std::filesystem::path& rom_path, std::u8string& game_id) {
    auto find_it = game_roms.find(game_id);

    if (find_it == game_roms.end()) {
        return recomp::RomValidationError::OtherError;
    }

    const recomp::GameEntry& game_entry = find_it->second;

    std::vector<uint8_t> rom_data = read_file(rom_path);

    if (rom_data.empty()) {
        return recomp::RomValidationError::FailedToOpen;
    }

    // Pad the rom to the nearest multiple of 4 bytes.
    rom_data.resize((rom_data.size() + 3) & ~3);

    ByteswapType byteswap_type = check_rom_start(rom_data);

    switch (byteswap_type) {
        case ByteswapType::Invalid:
            return recomp::RomValidationError::NotARom;
        case ByteswapType::Byteswapped2:
            byteswap_data(rom_data, 1);
            break;
        case ByteswapType::Byteswapped4:
            byteswap_data(rom_data, 3);
            break;
        case ByteswapType::NotByteswapped:
            break;
    }

    if (!check_hash(rom_data, game_entry.rom_hash)) {
        const std::string_view name{ reinterpret_cast<const char*>(rom_data.data()) + 0x20, game_entry.internal_name.size()};
        if (name == game_entry.internal_name) {
            return recomp::RomValidationError::IncorrectVersion;
        }
        else {
            if (game_entry.is_enabled && std::string_view{ reinterpret_cast<const char*>(rom_data.data()) + 0x20, 19 } == game_entry.internal_name) {
                return recomp::RomValidationError::NotYet;
            }
            else {
                return recomp::RomValidationError::IncorrectRom;
            }
        }
    }

    write_file(config_path / game_entry.stored_filename(), rom_data);
    
    return recomp::RomValidationError::Good;
}

extern "C" void osGetMemSize_recomp(uint8_t * rdram, recomp_context * ctx) {
    ctx->r2 = 8 * 1024 * 1024;
}

enum class StatusReg {
    FR = 0x04000000,
};

extern "C" void cop0_status_write(recomp_context* ctx, gpr value) {
    uint32_t old_sr = ctx->status_reg;
    uint32_t new_sr = (uint32_t)value;
    uint32_t changed = old_sr ^ new_sr;

    // Check if the FR bit changed
    if (changed & (uint32_t)StatusReg::FR) {
        // Check if the FR bit was set
        if (new_sr & (uint32_t)StatusReg::FR) {
            // FR = 1, odd single floats point to their own registers
            ctx->f_odd = &ctx->f1.u32l;
            ctx->mips3_float_mode = true;
        }
        // Otherwise, it was cleared
        else {
            // FR = 0, odd single floats point to the upper half of the previous register
            ctx->f_odd = &ctx->f0.u32h;
            ctx->mips3_float_mode = false;
        }

        // Remove the FR bit from the changed bits as it's been handled
        changed &= ~(uint32_t)StatusReg::FR;
    }

    // If any other bits were changed, assert false as they're not handled currently
    if (changed) {
        printf("Unhandled status register bits changed: 0x%08X\n", changed);
        assert(false);
        exit(EXIT_FAILURE);
    }
    
    // Update the status register in the context
    ctx->status_reg = new_sr;
}

extern "C" gpr cop0_status_read(recomp_context* ctx) {
    return (gpr)(int32_t)ctx->status_reg;
}

extern "C" void switch_error(const char* func, uint32_t vram, uint32_t jtbl) {
    printf("Switch-case out of bounds in %s at 0x%08X for jump table at 0x%08X\n", func, vram, jtbl);
    assert(false);
    exit(EXIT_FAILURE);
}

extern "C" void do_break(uint32_t vram) {
    printf("Encountered break at original vram 0x%08X\n", vram);
    assert(false);
    exit(EXIT_FAILURE);
}

std::optional<std::u8string> current_game = std::nullopt;
std::atomic<GameStatus> game_status = GameStatus::None;

void run_thread_function(uint8_t* rdram, uint64_t addr, uint64_t sp, uint64_t arg) {
    auto find_it = game_roms.find(current_game.value());
    const recomp::GameEntry& game_entry = find_it->second;
    
    recomp_context ctx{};
    ctx.r29 = sp;
    ctx.r4 = arg;
    ctx.mips3_float_mode = 0;
    ctx.f_odd = &ctx.f0.u32h;

    if (game_entry.thread_create_callback != nullptr) {
        game_entry.thread_create_callback(rdram, &ctx);
    }

    recomp_func_t* func = get_function(addr);
    func(rdram, &ctx);
}

void init(uint8_t* rdram, recomp_context* ctx, gpr entrypoint) {
    // Initialize the overlays
    recomp::overlays::init_overlays();

    // Load overlays in the first 1MB
    load_overlays(0x1000, (int32_t)entrypoint, 1024 * 1024);

    // Initial 1MB DMA (rom address 0x1000 = physical address 0x10001000)
    recomp::do_rom_read(rdram, entrypoint, 0x10001000, 0x100000);

    // Read in any extra data from patches
    recomp::overlays::read_patch_data(rdram, (gpr)recomp::patch_rdram_start);

    // Set up context floats
    ctx->f_odd = &ctx->f0.u32h;
    ctx->mips3_float_mode = false;

    // Initialize variables normally set by IPL3
    constexpr int32_t osTvType = 0x80000300;
    //constexpr int32_t osRomType = 0x80000304;
    constexpr int32_t osRomBase = 0x80000308;
    constexpr int32_t osResetType = 0x8000030c;
    //constexpr int32_t osCicId = 0x80000310;
    //constexpr int32_t osVersion = 0x80000314;
    constexpr int32_t osMemSize = 0x80000318;
    //constexpr int32_t osAppNMIBuffer = 0x8000031c;
    MEM_W(osTvType, 0) = 1; // NTSC
    MEM_W(osRomBase, 0) = 0xB0000000u; // standard rom base
    MEM_W(osResetType, 0) = 0; // cold reset
    MEM_W(osMemSize, 0) = 8 * 1024 * 1024; // 8MB
}

std::u8string recomp::current_game_id() {
    std::lock_guard<std::mutex> lock(current_game_mutex);
    return current_game.value();
};

void recomp::start_game(const std::u8string& game_id) {
    std::lock_guard<std::mutex> lock(current_game_mutex);
    current_game = game_id;
    game_status.store(GameStatus::Running);
    game_status.notify_all();
}

bool ultramodern::is_game_started() {
    return game_status.load() != GameStatus::None;
}

std::atomic_bool exited = false;

void ultramodern::quit() {
    exited.store(true);
    GameStatus desired = GameStatus::None;
    game_status.compare_exchange_strong(desired, GameStatus::Quit);
    game_status.notify_all();
    std::lock_guard<std::mutex> lock(current_game_mutex);
    current_game.reset();
}

void recomp::mods::enable_mod(const std::string& mod_id, bool enabled) {
    std::lock_guard lock { mod_context_mutex };
    return mod_context->enable_mod(mod_id, enabled, true);
}

bool recomp::mods::is_mod_enabled(const std::string& mod_id) {
    std::lock_guard lock { mod_context_mutex };
    return mod_context->is_mod_enabled(mod_id);
}

bool recomp::mods::is_mod_auto_enabled(const std::string& mod_id) {
    std::lock_guard lock{ mod_context_mutex };
    return false; // TODO
}

const recomp::mods::ConfigSchema &recomp::mods::get_mod_config_schema(const std::string &mod_id) {
    std::lock_guard lock{ mod_context_mutex };
    return mod_context->get_mod_config_schema(mod_id);
}

void recomp::mods::set_mod_config_value(const std::string &mod_id, const std::string &option_id, const ConfigValueVariant &value) {
    std::lock_guard lock{ mod_context_mutex };
    return mod_context->set_mod_config_value(mod_id, option_id, value);
}

recomp::mods::ConfigValueVariant recomp::mods::get_mod_config_value(const std::string &mod_id, const std::string &option_id) {
    std::lock_guard lock{ mod_context_mutex };
    return mod_context->get_mod_config_value(mod_id, option_id);
}

std::vector<recomp::mods::ModDetails> recomp::mods::get_mod_details(const std::string& mod_game_id) {
    std::lock_guard lock { mod_context_mutex };
    return mod_context->get_mod_details(mod_game_id);
}

void recomp::mods::set_mod_index(const std::string &mod_game_id, const std::string &mod_id, size_t index) {
    std::lock_guard lock{ mod_context_mutex };
    return mod_context->set_mod_index(mod_game_id, mod_id, index);
}

bool wait_for_game_started(uint8_t* rdram, recomp_context* context) {
    game_status.wait(GameStatus::None);

    switch (game_status.load()) {
        // TODO refactor this to allow a project to specify what entrypoint function to run for a give game.
        case GameStatus::Running:
            {
                if (!recomp::load_stored_rom(current_game.value())) {
                    ultramodern::error_handling::message_box("Error opening stored ROM! Please restart this program.");
                }

                auto find_it = game_roms.find(current_game.value());
                const recomp::GameEntry& game_entry = find_it->second;

                init(rdram, context, game_entry.entrypoint_address);
                if (game_entry.on_init_callback) {
                    game_entry.on_init_callback(rdram, context);
                }

                uint32_t mod_ram_used = 0;
                if (!game_entry.mod_game_id.empty()) {
                    std::vector<recomp::mods::ModLoadErrorDetails> mod_load_errors;
                    {
                        std::lock_guard lock { mod_context_mutex };
                        mod_load_errors = mod_context->load_mods(game_entry, rdram, recomp::mod_rdram_start, mod_ram_used);
                    }

                    if (!mod_load_errors.empty()) {
                        std::ostringstream mod_error_stream;
                        mod_error_stream << "Error loading mods:\n\n";
                        for (const auto& cur_error : mod_load_errors) {
                            mod_error_stream << cur_error.mod_id.c_str() << ": " << recomp::mods::error_to_string(cur_error.error);
                            if (!cur_error.error_param.empty()) {
                                mod_error_stream << " (" << cur_error.error_param.c_str() << ")";
                            }
                            mod_error_stream << "\n";                                
                        }
                        ultramodern::error_handling::message_box(mod_error_stream.str().c_str());
                        game_status.store(GameStatus::None);
                        return false;
                    }
                }

                recomp::init_heap(rdram, recomp::mod_rdram_start + mod_ram_used);

                save_type = game_entry.save_type;
                ultramodern::init_saving(rdram);

                try {
                    game_entry.entrypoint(rdram, context);
                } catch (ultramodern::thread_terminated& terminated) {

                }
            }
            return true;

        case GameStatus::Quit:
            return true;

        case GameStatus::None:
            return true;
    }
}

recomp::SaveType recomp::get_save_type() {
    return save_type;
}

bool recomp::eeprom_allowed() {
    return
        save_type == SaveType::Eep4k || 
        save_type == SaveType::Eep16k ||
        save_type == SaveType::AllowAll;
}

bool recomp::sram_allowed() {
    return
        save_type == SaveType::Sram || 
        save_type == SaveType::AllowAll;
}

bool recomp::flashram_allowed() {
    return
        save_type == SaveType::Flashram || 
        save_type == SaveType::AllowAll;
}

void recomp::start(
    const recomp::Version& version,
    ultramodern::renderer::WindowHandle window_handle,
    const recomp::rsp::callbacks_t& rsp_callbacks,
    const ultramodern::renderer::callbacks_t& renderer_callbacks,
    const ultramodern::audio_callbacks_t& audio_callbacks,
    const ultramodern::input::callbacks_t& input_callbacks,
    const ultramodern::gfx_callbacks_t& gfx_callbacks_,
    const ultramodern::events::callbacks_t& events_callbacks,
    const ultramodern::error_handling::callbacks_t& error_handling_callbacks,
    const ultramodern::threads::callbacks_t& threads_callbacks
) {
    project_version = version;
    recomp::check_all_stored_roms();

    recomp::rsp::set_callbacks(rsp_callbacks);

    static const ultramodern::rsp::callbacks_t ultramodern_rsp_callbacks {
        .init = recomp::rsp::constants_init,
        .run_task = recomp::rsp::run_task,
    };

    ultramodern::set_callbacks(ultramodern_rsp_callbacks, renderer_callbacks, audio_callbacks, input_callbacks, gfx_callbacks_, events_callbacks, error_handling_callbacks, threads_callbacks);

    ultramodern::gfx_callbacks_t gfx_callbacks = gfx_callbacks_;

    ultramodern::gfx_callbacks_t::gfx_data_t gfx_data{};

    if (gfx_callbacks.create_gfx) {
        gfx_data = gfx_callbacks.create_gfx();
    }

    if (window_handle == ultramodern::renderer::WindowHandle{}) {
        if (gfx_callbacks.create_window) {
            window_handle = gfx_callbacks.create_window(gfx_data);
        }
        else {
            assert(false && "No create_window callback provided");
        }
    }

    recomp::mods::initialize_mods();
    recomp::mods::scan_mods();

    // Allocate rdram without comitting it. Use a platform-specific virtual allocation function
    // that initializes to zero. Protect the region above the memory size to catch accesses to invalid addresses.
    uint8_t* rdram;
    bool alloc_failed;
#ifdef _WIN32
    rdram = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, allocation_size, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS));
    DWORD old_protect = 0;
    alloc_failed = (rdram == nullptr);
    if (!alloc_failed) {
        // VirtualProtect returns 0 on failure.
        alloc_failed = (VirtualProtect(rdram, mem_size, PAGE_READWRITE, &old_protect) == 0);
        if (alloc_failed) {
            VirtualFree(rdram, 0, MEM_RELEASE);
        }
    }
#else
    rdram = (uint8_t*)mmap(NULL, allocation_size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    alloc_failed = rdram == reinterpret_cast<uint8_t*>(MAP_FAILED);
    if (!alloc_failed) {
        // mprotect returns -1 on failure.
        alloc_failed = (mprotect(rdram, mem_size, PROT_READ | PROT_WRITE) == -1);
        if (alloc_failed) {
            munmap(rdram, allocation_size);
        }
    }
#endif

    if (alloc_failed) {
        ultramodern::error_handling::message_box("Failed to allocate memory!");
        return;
    }

    recomp::register_heap_exports();

    std::thread game_thread{[](ultramodern::renderer::WindowHandle window_handle, uint8_t* rdram) {
        debug_printf("[Recomp] Starting\n");

        ultramodern::set_native_thread_name("Game Start Thread");

        ultramodern::preinit(rdram, window_handle);

        recomp_context context{};

        // Loop until the game starts.
        while (!wait_for_game_started(rdram, &context)) {}
    }, window_handle, rdram};

    while (!exited) {
        ultramodern::sleep_milliseconds(1);
        if (gfx_callbacks.update_gfx != nullptr) {
            gfx_callbacks.update_gfx(gfx_data);
        }
    }

    game_thread.join();
    ultramodern::join_event_threads();
    ultramodern::join_thread_cleaner_thread();
    ultramodern::join_saving_thread();
    
    // Free rdram.
    bool free_failed;
#ifdef _WIN32
    // VirtualFree returns zero on failure.
    free_failed = (VirtualFree(rdram, 0, MEM_RELEASE) == 0);
#else
    // munmap returns -1 on failure.
    free_failed = (munmap(rdram, allocation_size) == -1);
#endif

    if (free_failed) {
        printf("Failed to free rdram\n");
    }
}
