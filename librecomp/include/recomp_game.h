#ifndef __RECOMP_GAME__
#define __RECOMP_GAME__

#include <vector>
#include <filesystem>

#include "recomp.h"
#include <ultramodern/ultramodern.hpp>

namespace recomp {
    struct GameHandle {
        uint64_t id;
    };
    struct GameEntry {
        uint64_t rom_hash;
        std::string internal_name;
        void (*entrypoint)();
        std::span<const char> cache_data;
        bool is_enabled;

        std::string stored_filename() const;
    };
	enum class RomValidationError {
		Good,
		FailedToOpen,
		NotARom,
		IncorrectRom,
		NotYet,
		IncorrectVersion,
		OtherError
	};
    GameHandle register_game(const recomp::GameEntry& entry);
    void register_patch(const char* patch, std::size_t size);
	void check_all_stored_roms();
	bool load_stored_rom(GameHandle game);
	RomValidationError select_rom(const std::filesystem::path& rom_path, GameHandle game);
	bool is_rom_valid(GameHandle game);
	bool is_rom_loaded();
	void set_rom_contents(std::vector<uint8_t>&& new_rom);
	void do_rom_read(uint8_t* rdram, gpr ram_address, uint32_t physical_addr, size_t num_bytes);
	void do_rom_pio(uint8_t* rdram, gpr ram_address, uint32_t physical_addr);
	void start(ultramodern::WindowHandle window_handle, const ultramodern::audio_callbacks_t& audio_callbacks, const ultramodern::input_callbacks_t& input_callbacks, const ultramodern::gfx_callbacks_t& gfx_callbacks);
	void start_game(GameHandle game);
	void message_box(const char* message);
    std::filesystem::path get_app_folder_path();
}

#endif
