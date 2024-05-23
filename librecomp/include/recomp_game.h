#ifndef __RECOMP_GAME__
#define __RECOMP_GAME__

#include <vector>
#include <filesystem>

#include "recomp.h"
#include <ultramodern/ultramodern.hpp>

namespace recomp {
    struct GameEntry {
        uint64_t rom_hash;
        std::string internal_name;
        std::u8string game_id;
        std::span<const char> cache_data;
        bool is_enabled;

        void (*entrypoint)();

        std::u8string stored_filename() const;
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
	bool register_game(const recomp::GameEntry& entry);
	void register_patch(const char* patch, std::size_t size);
	void check_all_stored_roms();
	bool load_stored_rom(std::u8string& game_id);
	RomValidationError select_rom(const std::filesystem::path& rom_path, std::u8string& game_id);
	bool is_rom_valid(std::u8string& game_id);
	bool is_rom_loaded();
	void set_rom_contents(std::vector<uint8_t>&& new_rom);
	void do_rom_read(uint8_t* rdram, gpr ram_address, uint32_t physical_addr, size_t num_bytes);
	void do_rom_pio(uint8_t* rdram, gpr ram_address, uint32_t physical_addr);
	void start(ultramodern::WindowHandle window_handle, const ultramodern::audio_callbacks_t& audio_callbacks, const ultramodern::input_callbacks_t& input_callbacks, const ultramodern::gfx_callbacks_t& gfx_callbacks);
	void start_game(std::u8string game_id);
	void message_box(const char* message);
	std::filesystem::path get_app_folder_path();
	std::u8string current_game_id();

	// TODO: implement both
	const std::u8string& get_program_id();
	void set_program_id(const std::u8string& program_id);
}

#endif
