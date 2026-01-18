#ifndef __ULTRAMODERN_SAVE_HPP__
#define __ULTRAMODERN_SAVE_HPP__

#include <string>
#include <ultramodern/ultramodern.hpp>

namespace ultramodern {
    enum class SaveType {
        None,
        Eep4k,
        Eep16k,
        Sram,
        Flashram,
        AllowAll, // Allows all save types to work and reports eeprom size as 16kbit.
    };

    void set_save_type(SaveType type);

    void set_save_file_path(const std::u8string& subfolder, const std::u8string& name);

    void init_saving(RDRAM_ARG const std::u8string& name);

    void change_save_file(const std::u8string& subfolder, const std::u8string& name);

    void join_saving_thread();

    void save_write_ptr(const void* in, uint32_t offset, uint32_t count);

    void save_write(RDRAM_ARG PTR(void) rdram_address, uint32_t offset, uint32_t count);

    void save_read(RDRAM_ARG PTR(void) rdram_address, uint32_t offset, uint32_t count);

    void save_clear(uint32_t start, uint32_t size, char value);

    SaveType get_save_type();

    size_t get_save_size(SaveType save_type);

    std::filesystem::path get_save_file_path();

    bool eeprom_allowed();

    bool sram_allowed();

    bool flashram_allowed();
}

#endif // __ULTRAMODERN_SAVE_HPP__

