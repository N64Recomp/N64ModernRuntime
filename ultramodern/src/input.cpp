#include <cassert>
#include <cstdlib>
#include <format>

#include "ultramodern/input.hpp"
#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "controller_pak.hpp"

#define PFS_ERR_NOPACK          1   // no device inserted
#define PFS_ERR_NEW_PACK        2   /* ram pack has been changed to a different one */
#define PFS_ERR_INCONSISTENT    3   /* need to run Pfschecker*/
#define PFS_ERR_CONTRFAIL       4   // data transmission failure
#define PFS_ERR_INVALID         5   // invalid parameter or invalid file
#define PFS_ERR_BAD_DATA        6   /* the data read from pack are bad*/
#define PFS_DATA_FULL           7   /* no free pages on ram pack*/
#define PFS_DIR_FULL            8   /* no free directories on ram pack*/
#define PFS_ERR_EXIST           9   /* file exists*/
#define PFS_ERR_ID_FATAL        10  /* dead ram pack */
#define PFS_ERR_DEVICE          11  // different type of device inserted
#define PFS_ERR_NO_GBCART       12  /* no gb cartridge (64GB-PAK) */
#define PFS_ERR_NEW_GBCART      13  /* gb cartridge may be changed */

#define PFS_INITIALIZED         1
#define PFS_CORRUPTED           2
#define PFS_ID_BROKEN           4
#define PFS_MOTOR_INITIALIZED   8
#define PFS_GBPAK_INITIALIZED   16

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))
#define MAX_FILES 16
#define TRACE_ENTRY()   fprintf(stderr, "PAK_ENTRY(%s)\n", __func__);

static ultramodern::input::callbacks_t input_callbacks {};

void ultramodern::input::set_callbacks(const callbacks_t& callbacks) {
    input_callbacks = callbacks;
}

static std::chrono::high_resolution_clock::time_point input_poll_time;

static void update_poll_time() {
    input_poll_time = std::chrono::high_resolution_clock::now();
}

void ultramodern::measure_input_latency() {
#if 0
    printf("Delta: %ld micros\n", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - input_poll_time));
#endif
}

#define MAXCONTROLLERS 4

#define CONT_NO_RESPONSE_ERROR 0x8

#define CONT_TYPE_NORMAL 0x0005
#define CONT_TYPE_MOUSE  0x0002
#define CONT_TYPE_VOICE  0x0100

static int max_controllers = 0;

/* Plain controller */

static u16 get_controller_type(ultramodern::input::Device device_type) {
    switch (device_type) {
    case ultramodern::input::Device::None:
        return 0;

    case ultramodern::input::Device::Controller:
        return CONT_TYPE_NORMAL;

#if 0
    case ultramodern::input::Device::Mouse:
        return CONT_TYPE_MOUSE;

    case ultramodern::input::Device::VRU:
        return CONT_TYPE_VOICE;
#endif
    }

    return 0;
}

static void __osContGetInitData(u8* pattern, OSContStatus *data) {
    *pattern = 0x00;

    for (int controller = 0; controller < max_controllers; controller++) {
        ultramodern::input::connected_device_info_t device_info{};

        if (input_callbacks.get_connected_device_info != nullptr) {
            device_info = input_callbacks.get_connected_device_info(controller);
        }

        if (device_info.connected_device != ultramodern::input::Device::None) {
            // Mark controller as present

            data[controller].type = get_controller_type(device_info.connected_device);
            data[controller].status = device_info.connected_pak != ultramodern::input::Pak::None;
            data[controller].err_no = 0x00;

            *pattern |= 1 << controller;
        }
        else {
            // Mark controller as not connected

            // Libultra doesn't write status or type for absent controllers
            data[controller].err_no = CONT_NO_RESPONSE_ERROR; // CHNL_ERR_NORESP >> 4
        }
    }
}

extern "C" s32 osContInit(RDRAM_ARG PTR(OSMesgQueue) mq, u8* bitpattern, PTR(OSContStatus) data_) {
    OSContStatus *data = TO_PTR(OSContStatus, data_);

    max_controllers = MAXCONTROLLERS;

    __osContGetInitData(bitpattern, data);

    return 0;
}

extern "C" s32 osContReset(RDRAM_ARG PTR(OSMesgQueue) mq, PTR(OSContStatus) data) {
    assert(false);
    return 0;
}

extern "C" s32 osContStartQuery(RDRAM_ARG PTR(OSMesgQueue) mq) {
    ultramodern::send_si_message();

    return 0;
}

extern "C" s32 osContStartReadData(RDRAM_ARG PTR(OSMesgQueue) mq) {
    if (input_callbacks.poll_input != nullptr) {
        input_callbacks.poll_input();
    }
    update_poll_time();

    ultramodern::send_si_message();

    return 0;
}

extern "C" s32 osContSetCh(RDRAM_ARG u8 ch) {
    max_controllers = std::min(ch, u8(MAXCONTROLLERS));

    return 0;
}

extern "C" void osContGetQuery(RDRAM_ARG PTR(OSContStatus) data_) {
    OSContStatus *data = TO_PTR(OSContStatus, data_);
    u8 pattern;

    __osContGetInitData(&pattern, data);
}

extern "C" void osContGetReadData(OSContPad *data) {
    for (int controller = 0; controller < max_controllers; controller++) {
        uint16_t buttons = 0;
        float x = 0.0f;
        float y = 0.0f;
        bool got_response = false;

        if (input_callbacks.get_input != nullptr) {
            got_response = input_callbacks.get_input(controller, &buttons, &x, &y);
        }

        if (got_response) {
            data[controller].button = buttons;
            data[controller].stick_x = (int8_t)(127 * x);
            data[controller].stick_y = (int8_t)(127 * y);
            data[controller].err_no = 0;
        } else {
            data[controller].err_no =  CONT_NO_RESPONSE_ERROR; // CHNL_ERR_NORESP >> 4
        }
    }
}

/* RumblePak */

extern "C" s32 osMotorInit(RDRAM_ARG PTR(OSMesgQueue) mq_, PTR(OSPfs) pfs_, int channel) {
    OSPfs *pfs = TO_PTR(OSPfs, pfs_);

    // basic initialization performed regardless of connected/disconnected status
    pfs->queue = mq_;
    pfs->channel = channel;
    pfs->activebank = 0xFF;
    pfs->status = 0;

    ultramodern::input::connected_device_info_t device_info{};
    if (input_callbacks.get_connected_device_info != nullptr) {
        device_info = input_callbacks.get_connected_device_info(channel);
    }

    if (device_info.connected_device != ultramodern::input::Device::Controller) {
        return PFS_ERR_CONTRFAIL;
    }
    if (device_info.connected_pak == ultramodern::input::Pak::None) {
        return PFS_ERR_NOPACK;
    }
    if (device_info.connected_pak != ultramodern::input::Pak::RumblePak) {
        return PFS_ERR_DEVICE;
    }

    pfs->status = PFS_MOTOR_INITIALIZED;
    return 0;
}

extern "C" s32 osMotorStop(RDRAM_ARG PTR(OSPfs) pfs) {
    return __osMotorAccess(PASS_RDRAM pfs, false);
}

extern "C" s32 osMotorStart(RDRAM_ARG PTR(OSPfs) pfs) {
    return __osMotorAccess(PASS_RDRAM pfs, true);
}

extern "C" s32 __osMotorAccess(RDRAM_ARG PTR(OSPfs) pfs_, s32 flag) {
    OSPfs *pfs = TO_PTR(OSPfs, pfs_);

    if (!(pfs->status & PFS_MOTOR_INITIALIZED)) {
        return PFS_ERR_INVALID;
    }

    if (input_callbacks.set_rumble != nullptr) {
        input_callbacks.set_rumble(pfs->channel, flag);
    }

    return 0;
}

/* ControllerPak */

static s32 __osPfsGetStatus(RDRAM_ARG PTR(OSMesgQueue) queue, int channel) {
    ultramodern::input::connected_device_info_t device_info{};
    if (input_callbacks.get_connected_device_info != nullptr) {
        device_info = input_callbacks.get_connected_device_info(channel);
    }

    if (device_info.connected_device != ultramodern::input::Device::Controller) {
        return PFS_ERR_CONTRFAIL;
    }
    if (device_info.connected_pak == ultramodern::input::Pak::None) {
        return PFS_ERR_NOPACK;
    }
    if (device_info.connected_pak != ultramodern::input::Pak::ControllerPak) {
        return PFS_ERR_DEVICE;
    }

    // If a header file doesn't exist, create it.
    ControllerPak pak;
    if (!std::filesystem::exists("controllerPak_header.sav")) {
        pak.header.open("controllerPak_header.sav", std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        pak.header.close();
    }

    fprintf(stderr, "__osPfsGetStatus OK\n");
    return 0;
}

extern "C" s32 osPfsInitPak(RDRAM_ARG PTR(OSMesgQueue) mq_, PTR(OSPfs) pfs_, int channel) {
    TRACE_ENTRY()
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);

    const auto status = __osPfsGetStatus(PASS_RDRAM mq_, channel);
    if (status != 0) {
        return status;
    }

    pfs->queue = mq_;
    pfs->channel = channel;
    pfs->status = 0;

    const s32 ret = osPfsChecker(PASS_RDRAM pfs_);
    pfs->status |= PFS_INITIALIZED;
    return 0;
}

extern "C" s32 osPfsRepairId(RDRAM_ARG PTR(OSPfs) pfs) {
    TRACE_ENTRY()
    return 0;
}

extern "C" s32 osPfsInit(RDRAM_ARG PTR(OSMesgQueue) mq_, PTR(OSPfs) pfs_, int channel) {
    TRACE_ENTRY()
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);

    const auto status = __osPfsGetStatus(PASS_RDRAM mq_, channel);
    if (status != 0) {
        return status;
    }

    pfs->queue = mq_;
    pfs->channel = channel;
    pfs->status = 0;
    pfs->activebank = -1;

    const s32 ret = osPfsChecker(PASS_RDRAM pfs_);
    pfs->status |= PFS_INITIALIZED;
    return 0;
}

extern "C" s32 osPfsReFormat(RDRAM_ARG PTR(OSPfs) pfs, PTR(OSMesgQueue) mq_, int channel) {
    TRACE_ENTRY()
    return 0;
}

extern "C" s32 osPfsChecker(RDRAM_ARG PTR(OSPfs) pfs) {
    TRACE_ENTRY()
    return 0;
}

extern "C" s32 osPfsAllocateFile(RDRAM_ARG PTR(OSPfs) pfs, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, int nbytes, PTR(s32) file_no_) {
    TRACE_ENTRY()
    s32* file_no = TO_PTR(s32, file_no_);

    if ((company_code == 0) || (game_code == 0)) {
        return PFS_ERR_INVALID;
    }

    /* Search for a free slot */
    u8 free_file_index = 0;
    for (size_t i = 0; i < MAX_FILES; i++) {
        ControllerPakHdr hdr{};
        Pfs_PakHeader_Read(hdr, i);

        if ((hdr.company_code == 0) || (hdr.game_code == 0)) {
            free_file_index = i;
            break;
        }
    }

    if (free_file_index == MAX_FILES) {
        return PFS_DIR_FULL;
    }

    Pfs_PakHeader_Write(nbytes, game_code, company_code, ext_name, game_name, free_file_index);

    /* Create empty file */

    ControllerPak pak;

    const auto filename = std::format("controllerPak_file_{}.sav", free_file_index);
    pak.file.open(filename, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);

    nbytes = (nbytes + 31) & ~31;

    std::vector<u8> zero_block(nbytes, 0);
    pak.file.seekp(0, std::ios::beg);
    pak.file.write((char*)zero_block.data(), nbytes);
    pak.file.close();

    *file_no = free_file_index;
    return 0;
}

extern "C" s32 osPfsFindFile(RDRAM_ARG PTR(OSPfs) pfs_, u16 company_code, u32 game_code, u8* game_name, u8* ext_name, PTR(s32) file_no_) {
    TRACE_ENTRY()
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);
    s32* file_no = TO_PTR(s32, file_no_);

    for (size_t i = 0; i < MAX_FILES; i++) {
        ControllerPakHdr hdr{};
        Pfs_PakHeader_Read(hdr, i);

        if ((hdr.company_code == 0) || (hdr.game_code == 0)) {
            continue;
        } else {
            if ((game_code == hdr.game_code) && (company_code == hdr.company_code)) {
                for (size_t i = 0; i < ARRLEN(hdr.game_name); i++) {
                    if (game_name[i] != hdr.game_name[i]) {
                        break;
                    }
                }
                for (size_t i = 0; i < ARRLEN(hdr.ext_name); i++) {
                    if (ext_name[i] != hdr.ext_name[i]) {
                        break;
                    }
                }
                //  File found
                *file_no = i;
                return 0;
            }
        }
    }

    return PFS_ERR_INVALID;
}

extern "C" s32 osPfsDeleteFile(RDRAM_ARG PTR(OSPfs) pfs_, u16 company_code, u32 game_code, u8* game_name, u8* ext_name) {
    TRACE_ENTRY()

    if (company_code == 0 || game_code == 0) {
        return PFS_ERR_INVALID;
    }

    ControllerPak pak;
    for (int i = 0; i < MAX_FILES; i++) {
        ControllerPakHdr hdr{};
        Pfs_PakHeader_Read(hdr, i);

        if ((hdr.company_code == 0) || (hdr.game_code == 0)) {
            continue;
        } else {
            if ((game_code == hdr.game_code) && (company_code == hdr.company_code)) {
                int gncount = 0;
                int encount = 0;

                for (size_t i = 0; i < ARRLEN(hdr.game_name); i++) {
                    if (game_name[i] == hdr.game_name[i]) {
                        gncount++;
                    }
                }
                for (size_t i = 0; i < ARRLEN(hdr.ext_name); i++) {
                    if (ext_name[i] == hdr.ext_name[i]) {
                        encount++;
                    }
                }
                if ((gncount != 16) || encount != 4) {
                    continue;
                }
                // File found

                pak.header.open("controllerPak_header.sav", std::ios::binary | std::ios::in | std::ios::out);

                if (!pak.header.good()) {
                    assert(false);
                }
                if (!pak.header.is_open()) {
                    assert(false);
                }

                u32 seek = i * sizeof(OSPfsState);

                // Zero out the header for this file.
                std::vector<u8> zero_block(sizeof(OSPfsState), 0);
                pak.header.seekp(seek + 0x0, std::ios::beg);
                pak.header.write((char*)zero_block.data(), sizeof(OSPfsState));
                pak.header.close();

                std::filesystem::remove(std::format("controllerPak_file_{}.sav", i));
                return 0;
            }
        }
    }

    return PFS_ERR_INVALID;
}

extern "C" s32 osPfsReadWriteFile(RDRAM_ARG PTR(OSPfs) pfs_, s32 file_no, u8 flag, int offset, int size_in_bytes, u8* data_buffer) {
    TRACE_ENTRY()

    ControllerPak pak;

    const auto filename = std::format("controllerPak_file_{}.sav", file_no);
    pak.file.open(filename, std::ios::binary | std::ios::in | std::ios::out);

    if (!std::filesystem::exists(filename)) {
        return PFS_ERR_INVALID;
    }
    if (!pak.file.good()) {
        return PFS_ERR_INVALID;
    }
    if (!pak.file.is_open()) {
        return PFS_ERR_INVALID;
    }

    std::vector<u8> swap_buffer(size_in_bytes);
    if (flag == 0) {
        pak.file.seekg(offset, std::ios::beg);
        pak.file.read((char*)swap_buffer.data(), size_in_bytes);
        ByteSwapCopy(data_buffer, swap_buffer.data(), size_in_bytes);
    } else {
        ByteSwapCopy(swap_buffer.data(), data_buffer, size_in_bytes);
        pak.file.seekp(offset, std::ios::beg);
        pak.file.write((char*)swap_buffer.data(), size_in_bytes);
    }

    pak.file.close();
    return 0;
}

extern "C" s32 osPfsFileState(RDRAM_ARG PTR(OSPfs) pfs_, s32 file_no, PTR(OSPfsState) state_) {
    TRACE_ENTRY()
    OSPfsState *state = TO_PTR(OSPfsState, state_);

    // should pass the state of the requested file_no to the incoming state pointer,
    // games call this function 16 times, once per file
    // fills the incoming state with the information inside the header of the pak.

    const auto filename = std::format("controllerPak_file_{}.sav", file_no);
    if (!std::filesystem::exists(filename)) {
        return PFS_ERR_INVALID;
    }

    /* Read game info from pak */
    ControllerPakHdr hdr{};
    Pfs_PakHeader_Read(hdr, file_no);

    state->file_size = hdr.file_size;
    state->company_code = hdr.company_code;
    state->game_code = hdr.game_code;

    for (size_t i = 0; i < ARRLEN(hdr.game_name); i++) {
        state->game_name[i] = hdr.game_name[i];
    }
    for (size_t i = 0; i < ARRLEN(hdr.ext_name); i++) {
        state->ext_name[i] = hdr.ext_name[i];
    }

    return 0;
}

extern "C" s32 osPfsGetLabel(RDRAM_ARG PTR(OSPfs) pfs_, u8* label, PTR(int) len_) {
    TRACE_ENTRY()
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);
    int* len = TO_PTR(int, len_);

    if (label == NULL) {
        return PFS_ERR_INVALID;
    }
//  if (__osCheckId(pfs) == PFS_ERR_NEW_PACK) {
//      return PFS_ERR_NEW_PACK;
//  }

    int i;
    for (i = 0; i < ARRLEN(pfs->label); i++) {
        if (pfs->label[i] == 0) {
            break;
        }
        *label++ = pfs->label[i];
    }
    *len = i;
    return 0;
}

extern "C" s32 osPfsSetLabel(RDRAM_ARG PTR(OSPfs) pfs_, u8* label) {
    TRACE_ENTRY()
    OSPfs* pfs = TO_PTR(OSPfs, pfs_);

    if (label != NULL) {
        for (int i = 0; i < ARRLEN(pfs->label); i++) {
            if (*label == 0) {
                break;
            }
            pfs->label[i] = *label++;
        }
    }
    return 0;
}

extern "C" s32 osPfsIsPlug(RDRAM_ARG PTR(OSMesgQueue) mq_, u8* pattern) {
    TRACE_ENTRY()
    u8 bits = 0;

    for (int channel = 0; channel < max_controllers; channel++) {
        if (__osPfsGetStatus(PASS_RDRAM mq_, channel) == 0) {
            bits |= (1 << channel);
        }
    }
    *pattern = bits;
    return 0;
}

extern "C" s32 osPfsFreeBlocks(RDRAM_ARG PTR(OSPfs) pfs_, PTR(s32) bytes_not_used_) {
    TRACE_ENTRY()
    OSPfs *pfs = TO_PTR(OSPfs, pfs_);
    s32 *bytes_not_used = TO_PTR(s32, bytes_not_used_);

    s32 bytes_used = 0;
    for (size_t i = 0; i < MAX_FILES; i++) {
        ControllerPakHdr hdr{};
        Pfs_PakHeader_Read(hdr, i);

        if ((hdr.company_code != 0) && (hdr.game_code != 0)) {
            bytes_used += hdr.file_size >> 8;
        }
    }

    *bytes_not_used = (123 - bytes_used) << 8;
    return 0;
}

extern "C" s32 osPfsNumFiles(RDRAM_ARG PTR(OSPfs) pfs_, PTR(s32) max_files_, PTR(s32) files_used_) {
    TRACE_ENTRY()
    OSPfs *pfs = TO_PTR(OSPfs, pfs_);
    s32 *max_files = TO_PTR(s32, max_files_);
    s32 *files_used = TO_PTR(s32, files_used_);

    u8 num_files = 0;
    for (size_t i = 0; i < MAX_FILES; i++) {
        ControllerPakHdr hdr{};
        Pfs_PakHeader_Read(hdr, i);

        if ((hdr.company_code != 0) && (hdr.game_code != 0)) {
            num_files++;
        }
    }

    *max_files = MAX_FILES;
    *files_used = num_files;
    return 0;
}

