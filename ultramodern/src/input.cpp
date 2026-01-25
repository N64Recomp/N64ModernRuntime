#include <cassert>

#include "ultramodern/input.hpp"
#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"

#define MAXCONTROLLERS 4

static int max_controllers = 0;

static ultramodern::input::callbacks_t input_callbacks {};

int ultramodern::get_max_controllers() {
    return max_controllers;
}

void ultramodern::input::set_callbacks(const callbacks_t& callbacks) {
    input_callbacks = callbacks;
}

ultramodern::input::connected_device_info_t ultramodern::get_connected_device_info(int channel) {
    ultramodern::input::connected_device_info_t device_info{};
    if (input_callbacks.get_connected_device_info != nullptr) {
        device_info = input_callbacks.get_connected_device_info(channel);
    }
    return device_info;
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
        const auto device_info = ultramodern::get_connected_device_info(controller);
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

    const auto device_info = ultramodern::get_connected_device_info(channel);
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

