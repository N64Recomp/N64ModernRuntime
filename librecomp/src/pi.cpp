#include <memory>
#include <fstream>
#include <array>
#include <cstring>
#include <string>
#include <mutex>
#include "recomp.h"
#include "librecomp/addresses.hpp"
#include "librecomp/game.hpp"
#include "librecomp/files.hpp"
#include <ultramodern/ultra64.h>
#include <ultramodern/ultramodern.hpp>

static std::vector<uint8_t> rom;

bool recomp::is_rom_loaded() {
    return !rom.empty();
}

void recomp::set_rom_contents(std::vector<uint8_t>&& new_rom) {
    rom = std::move(new_rom);
}

std::span<const uint8_t> recomp::get_rom() {
    return rom;
}

constexpr uint32_t k1_to_phys(uint32_t addr) {
    return addr & 0x1FFFFFFF;
}

constexpr uint32_t phys_to_k1(uint32_t addr) {
    return addr | 0xA0000000;
}

extern "C" void __osPiGetAccess_recomp(uint8_t* rdram, recomp_context* ctx) {
}

extern "C" void __osPiRelAccess_recomp(uint8_t* rdram, recomp_context* ctx) {
}

extern "C" void osCartRomInit_recomp(uint8_t* rdram, recomp_context* ctx) {
    OSPiHandle* handle = TO_PTR(OSPiHandle, recomp::cart_handle);
    handle->type = 0; // cart
    handle->baseAddress = phys_to_k1(recomp::rom_base);
    handle->domain = 0;

    ctx->r2 = (gpr)recomp::cart_handle;
}

extern "C" void osDriveRomInit_recomp(uint8_t * rdram, recomp_context * ctx) {
    OSPiHandle* handle = TO_PTR(OSPiHandle, recomp::drive_handle);
    handle->type = 1; // bulk
    handle->baseAddress = phys_to_k1(recomp::drive_base);
    handle->domain = 0;

    ctx->r2 = (gpr)recomp::drive_handle;
}

extern "C" void osCreatePiManager_recomp(uint8_t* rdram, recomp_context* ctx) {
    ;
}

void recomp::do_rom_read(uint8_t* rdram, gpr ram_address, uint32_t physical_addr, size_t num_bytes) {
    // TODO use word copies when possible

    // TODO handle misaligned DMA
    assert((physical_addr & 0x1) == 0 && "Only PI DMA from aligned ROM addresses is currently supported");
    assert((ram_address & 0x7) == 0 && "Only PI DMA to aligned RDRAM addresses is currently supported");
    uint8_t* rom_addr = rom.data() + physical_addr - recomp::rom_base;
    for (size_t i = 0; i < num_bytes; i++) {
        MEM_B(i, ram_address) = *rom_addr;
        rom_addr++;
    }
}

void recomp::do_rom_pio(uint8_t* rdram, gpr ram_address, uint32_t physical_addr) {
    assert((physical_addr & 0x3) == 0 && "PIO not 4-byte aligned in device, currently unsupported");
    assert((ram_address & 0x3) == 0 && "PIO not 4-byte aligned in RDRAM, currently unsupported");
    uint8_t* rom_addr = rom.data() + physical_addr - recomp::rom_base;
    MEM_B(0, ram_address) = *rom_addr++;
    MEM_B(1, ram_address) = *rom_addr++;
    MEM_B(2, ram_address) = *rom_addr++;
    MEM_B(3, ram_address) = *rom_addr++;
}

struct {
    std::vector<char> save_buffer;
    std::thread saving_thread;
    std::filesystem::path save_file_path;
    moodycamel::LightweightSemaphore write_sempahore;
    // Used to tell the saving thread that a file swap is pending.
    moodycamel::LightweightSemaphore swap_file_pending_sempahore;
    // Used to tell the consumer thread that the saving thread is ready for a file swap.
    moodycamel::LightweightSemaphore swap_file_ready_sempahore;
    std::mutex save_buffer_mutex;
} save_context;

const std::u8string save_folder = u8"saves";

extern std::filesystem::path config_path;

std::filesystem::path ultramodern::get_save_file_path() {
    return save_context.save_file_path;
}

void set_save_file_path(const std::u8string& subfolder, const std::u8string& name) {
    std::filesystem::path save_folder_path = config_path / save_folder;
    if (!subfolder.empty()) {
        save_folder_path = save_folder_path / subfolder;
    }
    save_context.save_file_path = save_folder_path / (name + u8".bin");
}

void update_save_file() {
    bool saving_failed = false;
    {
        std::ofstream save_file = recomp::open_output_file_with_backup(ultramodern::get_save_file_path(), std::ios_base::binary);

        if (save_file.good()) {
            std::lock_guard lock{ save_context.save_buffer_mutex };
            save_file.write(save_context.save_buffer.data(), save_context.save_buffer.size());
        }
        else {
            saving_failed = true;
        }
    }
    if (!saving_failed) {
        saving_failed = !recomp::finalize_output_file_with_backup(ultramodern::get_save_file_path());
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

        if (save_context.swap_file_pending_sempahore.tryWait()) {
            save_context.swap_file_ready_sempahore.signal();
        }
    }
}

void save_write_ptr(const void* in, uint32_t offset, uint32_t count) {
    assert(offset + count <= save_context.save_buffer.size());

    {
        std::lock_guard lock { save_context.save_buffer_mutex };
        memcpy(&save_context.save_buffer[offset], in, count);
    }
    
    save_context.write_sempahore.signal();
}

void save_write(RDRAM_ARG PTR(void) rdram_address, uint32_t offset, uint32_t count) {
    assert(offset + count <= save_context.save_buffer.size());

    {
        std::lock_guard lock { save_context.save_buffer_mutex };
        for (gpr i = 0; i < count; i++) {
            save_context.save_buffer[offset + i] = MEM_B(i, rdram_address);
        }
    }

    save_context.write_sempahore.signal();
}

void save_read(RDRAM_ARG PTR(void) rdram_address, uint32_t offset, uint32_t count) {
    assert(offset + count <= save_context.save_buffer.size());

    std::lock_guard lock { save_context.save_buffer_mutex };
    for (gpr i = 0; i < count; i++) {
        MEM_B(i, rdram_address) = save_context.save_buffer[offset + i];
    }
}

void save_clear(uint32_t start, uint32_t size, char value) {
    assert(start + size < save_context.save_buffer.size());

    {
        std::lock_guard lock { save_context.save_buffer_mutex };
        std::fill_n(save_context.save_buffer.begin() + start, size, value);
    }

    save_context.write_sempahore.signal();
}

size_t get_save_size(recomp::SaveType save_type) {
    switch (save_type) {
        case recomp::SaveType::AllowAll:
        case recomp::SaveType::Flashram:
            return 0x20000;
        case recomp::SaveType::Sram:
            return 0x8000;
        case recomp::SaveType::Eep16k:
            return 0x800;
        case recomp::SaveType::Eep4k:
            return 0x200;
        case recomp::SaveType::None:
            return 0;
    }
    return 0;
}

void read_save_file() {
    std::filesystem::path save_file_path = ultramodern::get_save_file_path();

    // Ensure the save file directory exists.
    std::filesystem::create_directories(save_file_path.parent_path());

    // Read the save file if it exists.
    std::ifstream save_file = recomp::open_input_file_with_backup(save_file_path, std::ios_base::binary);
    if (save_file.good()) {
        save_file.read(save_context.save_buffer.data(), save_context.save_buffer.size());
    }
    else {
        // Otherwise clear the save file to all zeroes.
        std::fill(save_context.save_buffer.begin(), save_context.save_buffer.end(), 0);
    }
}

void ultramodern::init_saving(RDRAM_ARG1) {
    set_save_file_path(u8"", recomp::current_game_id());

    save_context.save_buffer.resize(get_save_size(recomp::get_save_type()));

    read_save_file();

    save_context.saving_thread = std::thread{saving_thread_func, PASS_RDRAM};
}

void ultramodern::change_save_file(const std::u8string& subfolder, const std::u8string& name) {
    // Tell the saving thread that a file swap is pending.
    save_context.swap_file_pending_sempahore.signal();
    // Wait until the saving thread indicates it's ready to swap files.
    save_context.swap_file_ready_sempahore.wait();
    // Perform the save file swap.
    set_save_file_path(subfolder, name);
    read_save_file();
}

void ultramodern::join_saving_thread() {
    if (save_context.saving_thread.joinable()) {
        save_context.saving_thread.join();
    }
}

void do_dma(RDRAM_ARG PTR(OSMesgQueue) mq, gpr rdram_address, uint32_t physical_addr, uint32_t size, uint32_t direction) {
    // TODO asynchronous transfer
    // TODO implement unaligned DMA correctly
    if (direction == 0) {
        if (physical_addr >= recomp::rom_base) {
            // read cart rom
            recomp::do_rom_read(rdram, rdram_address, physical_addr, size);

            // Send a message to the mq to indicate that the transfer completed
            osSendMesg(rdram, mq, 0, OS_MESG_NOBLOCK);
        } else if (physical_addr >= recomp::sram_base) {
            if (!recomp::sram_allowed()) {
                ultramodern::error_handling::message_box("Attempted to use SRAM saving with other save type");
                ULTRAMODERN_QUICK_EXIT();
            }
            // read sram
            save_read(rdram, rdram_address, physical_addr - recomp::sram_base, size);

            // Send a message to the mq to indicate that the transfer completed
            osSendMesg(rdram, mq, 0, OS_MESG_NOBLOCK);
        } else {
            fprintf(stderr, "[WARN] PI DMA read from unknown region, phys address 0x%08X\n", physical_addr);
        }
    } else {
        if (physical_addr >= recomp::rom_base) {
            // write cart rom
            throw std::runtime_error("ROM DMA write unimplemented");
        } else if (physical_addr >= recomp::sram_base) {
            if (!recomp::sram_allowed()) {
                ultramodern::error_handling::message_box("Attempted to use SRAM saving with other save type");
                ULTRAMODERN_QUICK_EXIT();
            }
            // write sram
            save_write(rdram, rdram_address, physical_addr - recomp::sram_base, size);

            // Send a message to the mq to indicate that the transfer completed
            osSendMesg(rdram, mq, 0, OS_MESG_NOBLOCK);
        } else {
            fprintf(stderr, "[WARN] PI DMA write to unknown region, phys address 0x%08X\n", physical_addr);
        }
    }
}

extern "C" void osPiStartDma_recomp(RDRAM_ARG recomp_context* ctx) {
    uint32_t mb = ctx->r4;
    uint32_t pri = ctx->r5;
    uint32_t direction = ctx->r6;
    uint32_t devAddr = ctx->r7 | recomp::rom_base;
    gpr dramAddr = MEM_W(0x10, ctx->r29);
    uint32_t size = MEM_W(0x14, ctx->r29);
    PTR(OSMesgQueue) mq = MEM_W(0x18, ctx->r29);
    uint32_t physical_addr = k1_to_phys(devAddr);

    debug_printf("[pi] DMA from 0x%08X into 0x%08X of size 0x%08X\n", devAddr, dramAddr, size);

    do_dma(PASS_RDRAM mq, dramAddr, physical_addr, size, direction);

    ctx->r2 = 0;
}

extern "C" void osEPiStartDma_recomp(RDRAM_ARG recomp_context* ctx) {
    OSPiHandle* handle = TO_PTR(OSPiHandle, ctx->r4);
    OSIoMesg* mb = TO_PTR(OSIoMesg, ctx->r5);
    uint32_t direction = ctx->r6;
    uint32_t devAddr = handle->baseAddress | mb->devAddr;
    gpr dramAddr = mb->dramAddr;
    uint32_t size = mb->size;
    PTR(OSMesgQueue) mq = mb->hdr.retQueue;
    uint32_t physical_addr = k1_to_phys(devAddr);

    debug_printf("[pi] DMA from 0x%08X into 0x%08X of size 0x%08X\n", devAddr, dramAddr, size);

    do_dma(PASS_RDRAM mq, dramAddr, physical_addr, size, direction);

    ctx->r2 = 0;
}

extern "C" void osEPiReadIo_recomp(RDRAM_ARG recomp_context * ctx) {
    OSPiHandle* handle = TO_PTR(OSPiHandle, ctx->r4);
    uint32_t devAddr = handle->baseAddress | ctx->r5;
    gpr dramAddr = ctx->r6;
    uint32_t physical_addr = k1_to_phys(devAddr);

    if (physical_addr > recomp::rom_base) {
        // cart rom
        recomp::do_rom_pio(PASS_RDRAM dramAddr, physical_addr);
    } else {
        // sram
        assert(false && "SRAM ReadIo unimplemented");
    }

    ctx->r2 = 0;
}

extern "C" void osPiGetStatus_recomp(RDRAM_ARG recomp_context * ctx) {
    ctx->r2 = 0;
}

extern "C" void osPiRawStartDma_recomp(RDRAM_ARG recomp_context * ctx) {
    ultramodern::error_handling::message_box(
        "Stub `osPiRawStartDma_recomp` function called!\n"
        "Most games do not call this function directly, which means the libultra function\n"
        "that uses this function was not properly named.\n"
        "\n"
        "If you triggered this message, please make sure you have properly identified\n"
        "every libultra function on your recompiled game. If you are sure every libultra\n"
        "function has been identified and you still get this problem then open an issue on\n"
        "the N64ModernRuntime Github repository mentioning the game you are trying to\n"
        "recompile and steps to reproduce the issue.\n"
        "\n"
        "The application will close now, bye and good luck!"
    );
    ULTRAMODERN_QUICK_EXIT();
}

extern "C" void osEPiRawStartDma_recomp(RDRAM_ARG recomp_context * ctx) {
    ultramodern::error_handling::message_box(
        "Stub `osEPiRawStartDma_recomp` function called!\n"
        "Most games do not call this function directly, which means the libultra function\n"
        "that uses this function was not properly named.\n"
        "\n"
        "If you triggered this message, please make sure you have properly identified\n"
        "every libultra function on your recompiled game. If you are sure every libultra\n"
        "function has been identified and you still get this problem then open an issue on\n"
        "the N64ModernRuntime Github repository mentioning the game you are trying to\n"
        "recompile and steps to reproduce the issue.\n"
        "\n"
        "The application will close now, bye and good luck!"
    );
    ULTRAMODERN_QUICK_EXIT();
}
