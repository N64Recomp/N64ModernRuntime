#ifndef __RECOMP_GAME__
#define __RECOMP_GAME__

#include <vector>
#include <filesystem>

#include "recomp.h"
#include "rsp.hpp"
#include <ultramodern/ultramodern.hpp>

namespace recomp {
    struct GameEntry {
        uint64_t rom_hash;
        std::string internal_name;
        std::u8string game_id;
		std::u8string mod_subdirectory;
        std::span<const char> cache_data;
        bool is_enabled;
		bool mods;

        gpr entrypoint_address;
        void (*entrypoint)(uint8_t* rdram, recomp_context* context);

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
	void register_config_path(std::filesystem::path path);
	bool register_game(const recomp::GameEntry& entry);
	void check_all_stored_roms();
	bool load_stored_rom(std::u8string& game_id);
	RomValidationError select_rom(const std::filesystem::path& rom_path, std::u8string& game_id);
	bool is_rom_valid(std::u8string& game_id);
	bool is_rom_loaded();
	void set_rom_contents(std::vector<uint8_t>&& new_rom);
	void do_rom_read(uint8_t* rdram, gpr ram_address, uint32_t physical_addr, size_t num_bytes);
	void do_rom_pio(uint8_t* rdram, gpr ram_address, uint32_t physical_addr);

	/**
	 * The following arguments contain mandatory callbacks that need to be registered (i.e., can't be `nullptr`):
	 * - `rsp_callbacks`
	 * - `renderer_callbacks`
	 *
	 * It must be called only once and it must be called before `ultramodern::preinit`.
	 */
    void start(
		uint32_t rdram_size,
        ultramodern::renderer::WindowHandle window_handle,
        const recomp::rsp::callbacks_t& rsp_callbacks,
        const ultramodern::renderer::callbacks_t& renderer_callbacks,
        const ultramodern::audio_callbacks_t& audio_callbacks,
        const ultramodern::input::callbacks_t& input_callbacks,
        const ultramodern::gfx_callbacks_t& gfx_callbacks,
        const ultramodern::events::callbacks_t& events_callbacks,
        const ultramodern::error_handling::callbacks_t& error_handling_callbacks,
        const ultramodern::threads::callbacks_t& threads_callbacks
    );

	void start_game(const std::u8string& game_id);
	std::u8string current_game_id();
}

#endif
