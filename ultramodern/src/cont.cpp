#include <cassert>

#include "ultramodern/input.hpp"
#include "ultramodern/ultra64.h"

static ultramodern::input::callbacks_t input_callbacks {};

void ultramodern::input::set_callbacks(const callbacks_t& callbacks) {
    input_callbacks = callbacks;
}

static int max_controllers = 0;

static void __osContGetInitData(u8* pattern, OSContStatus *data) {
    // Set bit 0 to indicate that controller 0 is present
    *pattern = 0x01;

    // Mark controller 0 as present
    data[0].type = 0x0005; // type: CONT_TYPE_NORMAL (from joybus)
    data[0].status = 0x00; // status: 0 (from joybus)
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

    max_controllers = 4;

    __osContGetInitData(bitpattern, data);

    return 0;
}

extern "C" s32 osContResetRDRAM_ARG (PTR(OSMesgQueue) , PTR(OSContStatus) ) {
    assert(false);
}

extern "C" s32 osContStartQuery(RDRAM_ARG PTR(OSMesgQueue) ) {
    assert(false);
}

extern "C" s32 osContStartReadData(RDRAM_ARG PTR(OSMesgQueue) ) {
    assert(false);
}

extern "C" s32 osContSetCh(RDRAM_ARG u8) {
    assert(false);
}

extern "C" void osContGetQuery(RDRAM_ARG PTR(OSContStatus) data_) {
    u8 pattern;
    OSContStatus *data = TO_PTR(OSContStatus, data_);

    __osContGetInitData(&pattern, data);
}

extern "C" void osContGetReadData(RDRAM_ARG PTR(OSContPad) ) {
    assert(false);
}
