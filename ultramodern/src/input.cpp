#include <cassert>

#include "ultramodern/input.hpp"
#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"

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

static int max_controllers = 0;

/* Plain controller */

static void __osContGetInitData(u8* pattern, OSContStatus *data) {
    // Set bit 0 to indicate that controller 0 is present
    *pattern = 0x01;

    // Mark controller 0 as present
    data[0].type = 0x0005; // type: CONT_TYPE_NORMAL (from joybus)
    data[0].status = 0x01; // status: 0x01 (from joybus, indicates that a pak is plugged into the controller)
    data[0].err_no = 0x00; // errno: 0 (from libultra)

    // Mark controllers 1-3 as not connected
    for (int controller = 1; controller < max_controllers; controller++) {
        // Libultra doesn't write status or type for absent controllers
        data[controller].err_no = 0x80 >> 4; // errno: CONT_NO_RESPONSE_ERROR >> 4
    }
}

extern "C" s32 osContInit(RDRAM_ARG PTR(OSMesgQueue) mq, PTR(u8) bitpattern_, PTR(OSContStatus) data_) {
    u8 *bitpattern = TO_PTR(u8, bitpattern_);
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
    ultramodern::send_si_message(PASS_RDRAM1);

    return 0;
}

extern "C" s32 osContStartReadData(RDRAM_ARG PTR(OSMesgQueue) mq) {
    if (input_callbacks.poll_input != nullptr) {
        input_callbacks.poll_input();
    }
    update_poll_time();

    ultramodern::send_si_message(rdram);

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

extern "C" void osContGetReadData(RDRAM_ARG PTR(OSContPad) data_) {
    OSContPad *data = TO_PTR(OSContPad, data_);

    for (int controller = 0; controller < max_controllers; controller++) {
        uint16_t buttons = 0;
        float x = 0.0f;
        float y = 0.0f;
        bool got_response = false;

        if (input_callbacks.get_input != nullptr) {
            got_response = input_callbacks.get_input(controller, &buttons, &x, &y);
        }

        if (got_response) {
            data[0].button = buttons;
            data[0].stick_x = (int8_t)(127 * x);
            data[0].stick_y = (int8_t)(127 * y);
            data[0].err_no = 0;
        } else {
            data[controller].err_no = 0x80 >> 4; // errno: CONT_NO_RESPONSE_ERROR >> 4
        }
    }
}

/* Rumble */

s32 osMotorInit(RDRAM_ARG PTR(OSMesgQueue) mq, PTR(OSPfs) pfs_, int channel) {
    OSPfs *pfs = TO_PTR(OSPfs, pfs_);

    pfs->channel = channel;

    return 0;
}

s32 osMotorStop(RDRAM_ARG PTR(OSPfs) pfs) {
    return __osMotorAccess(PASS_RDRAM pfs, false);
}

s32 osMotorStart(RDRAM_ARG PTR(OSPfs) pfs) {
    return __osMotorAccess(PASS_RDRAM pfs, true);
}

s32 __osMotorAccess(RDRAM_ARG PTR(OSPfs) pfs_, s32 flag) {
    OSPfs *pfs = TO_PTR(OSPfs, pfs_);

    if (input_callbacks.set_rumble != nullptr) {
        input_callbacks.set_rumble(pfs->channel, flag);
    }

    return 0;
}
