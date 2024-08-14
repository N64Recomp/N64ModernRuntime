#include <array>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

#include "ultramodern/error_handling.hpp"

#include "librecomp/files.hpp"
#include "librecomp/game.hpp"
#include "librecomp/recomp.h"
#include "librecomp/save.hpp"


struct {
    std::array<char, 0x20000> save_buffer;
    std::thread saving_thread;
    moodycamel::LightweightSemaphore write_sempahore;
    std::mutex save_buffer_mutex;
} save_context;

const std::u8string save_folder = u8"saves";

extern std::filesystem::path config_path;

std::filesystem::path get_save_file_path() {
    return config_path / save_folder / (std::u8string{recomp::current_game_id()} + u8".bin");
}

void update_save_file() {
    bool saving_failed = false;
    {
        std::ofstream save_file = recomp::open_output_file_with_backup(get_save_file_path(), std::ios_base::binary);

        if (save_file.good()) {
            std::lock_guard lock{ save_context.save_buffer_mutex };
            save_file.write(save_context.save_buffer.data(), save_context.save_buffer.size());
        }
        else {
            saving_failed = true;
        }
    }
    if (!saving_failed) {
        saving_failed = !recomp::finalize_output_file_with_backup(get_save_file_path());
    }
    if (saving_failed) {
        ultramodern::error_handling::message_box("Failed to write to the save file. Check your file permissions and whether the save folder has been moved to Dropbox or similar, as this can cause issues.");
    }
}

extern std::atomic_bool exited;

void saving_thread_func(RDRAM_ARG1) {
    while (!exited) {
        bool save_buffer_updated = false;
        // Repeatedly wait for a new action to be sent.
        constexpr int64_t wait_time_microseconds = 10000;
        constexpr int max_actions = 128;
        int num_actions = 0;

        // Wait up to the given timeout for a write to come in. Allow multiple writes to coalesce together into a single save.
        // Cap the number of coalesced writes to guarantee that the save buffer eventually gets written out to the file even if the game
        // is constantly sending writes.
        while (save_context.write_sempahore.wait(wait_time_microseconds) && num_actions < max_actions) {
            save_buffer_updated = true;
            num_actions++;
        }

        // If an action came through that affected the save file, save the updated contents.
        if (save_buffer_updated) {
            update_save_file();
        }
    }
}

void recomp::save::write_ptr(const void* in, uint32_t offset, uint32_t count) {
    assert(offset + count < save_context.save_buffer.size());

    {
        std::lock_guard lock { save_context.save_buffer_mutex };
        memcpy(&save_context.save_buffer[offset], in, count);
    }

    save_context.write_sempahore.signal();
}

void recomp::save::write(RDRAM_ARG PTR(void) rdram_address, uint32_t offset, uint32_t count) {
    assert(offset + count < save_context.save_buffer.size());

    {
        std::lock_guard lock { save_context.save_buffer_mutex };
        for (uint32_t i = 0; i < count; i++) {
            save_context.save_buffer[offset + i] = MEM_B(i, rdram_address);
        }
    }

    save_context.write_sempahore.signal();
}

void recomp::save::read(RDRAM_ARG PTR(void) rdram_address, uint32_t offset, uint32_t count) {
    assert(offset + count < save_context.save_buffer.size());

    std::lock_guard lock { save_context.save_buffer_mutex };
    for (size_t i = 0; i < count; i++) {
        MEM_B(i, rdram_address) = save_context.save_buffer[offset + i];
    }
}

void recomp::save::clear(uint32_t start, uint32_t size, char value) {
    assert(start + size < save_context.save_buffer.size());

    {
        std::lock_guard lock { save_context.save_buffer_mutex };
        std::fill_n(save_context.save_buffer.begin() + start, size, value);
    }

    save_context.write_sempahore.signal();
}

void recomp::save::init(uint8_t *rdram) {
    std::filesystem::path save_file_path = get_save_file_path();

    // Ensure the save file directory exists.
    std::filesystem::create_directories(save_file_path.parent_path());

    // Read the save file if it exists.
    std::ifstream save_file = recomp::open_input_file_with_backup(save_file_path, std::ios_base::binary);
    if (save_file.good()) {
        save_file.read(save_context.save_buffer.data(), save_context.save_buffer.size());
    }
    else {
        // Otherwise clear the save file to all zeroes.
        save_context.save_buffer.fill(0);
    }

    save_context.saving_thread = std::thread{saving_thread_func, PASS_RDRAM};
}

void recomp::save::join_thread() {
    if (save_context.saving_thread.joinable()) {
        save_context.saving_thread.join();
    }
}
