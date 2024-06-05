#include <ultramodern/ultramodern.hpp>
#include "helpers.hpp"

static ultramodern::input_callbacks_t input_callbacks;

std::chrono::high_resolution_clock::time_point input_poll_time;

void update_poll_time() {
    input_poll_time = std::chrono::high_resolution_clock::now();
}

extern "C" void recomp_set_current_frame_poll_id(uint8_t* rdram, recomp_context* ctx) {
    // TODO reimplement the system for tagging polls with IDs to handle games with multithreaded input polling.
}

extern "C" void recomp_measure_latency(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::measure_input_latency();
}

void ultramodern::measure_input_latency() {
    // printf("Delta: %ld micros\n", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - input_poll_time));    
}

void set_input_callbacks(const ultramodern::input_callbacks_t& callbacks) {
    input_callbacks = callbacks;
}

static int max_controllers = 0;

extern "C" void osContInit_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSMesgQueue) mq = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);
    PTR(u8) bitpattern = _arg<1, PTR(u8)>(rdram, ctx);
    PTR(OSContStatus) data = _arg<2, PTR(OSContStatus)>(rdram, ctx);

    s32 ret = osContInit(PASS_RDRAM mq, bitpattern, data);

    _return<s32>(ctx, ret);
}

extern "C" void osContReset_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(OSMesgQueue) mq = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);
    PTR(OSContStatus) data = _arg<1, PTR(OSContStatus)>(rdram, ctx);

    s32 ret = osContReset(PASS_RDRAM mq, data);

    _return<s32>(ctx, ret);
}

extern "C" void osContStartReadData_recomp(uint8_t* rdram, recomp_context* ctx) {
    if (input_callbacks.poll_input) {
        input_callbacks.poll_input();
    }
    update_poll_time();

    ultramodern::send_si_message(rdram);
}

extern "C" void osContGetReadData_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) pad = _arg<0, PTR(void)>(rdram, ctx);

    uint16_t buttons = 0;
    float x = 0.0f;
    float y = 0.0f;

    if (input_callbacks.get_input) {
        input_callbacks.get_input(&buttons, &x, &y);
    }

    if (max_controllers > 0) {
        // button
        MEM_H(0, pad) = buttons;
        // stick_x
        MEM_B(2, pad) = (int8_t)(127 * x);
        // stick_y
        MEM_B(3, pad) = (int8_t)(127 * y);
        // errno
        MEM_B(4, pad) = 0;
    }
    for (int controller = 1; controller < max_controllers; controller++) {
        MEM_B(6 * controller + 4, pad) = 0x80 >> 4; // errno: CONT_NO_RESPONSE_ERROR >> 4
    }
}

extern "C" void osContStartQuery_recomp(uint8_t * rdram, recomp_context * ctx) {
    PTR(OSMesgQueue) mq = _arg<0, PTR(OSMesgQueue)>(rdram, ctx);

    s32 ret = osContStartQuery(PASS_RDRAM mq);

    _return<s32>(ctx, ret);
}

extern "C" void osContGetQuery_recomp(uint8_t * rdram, recomp_context * ctx) {
    PTR(OSContStatus) data = _arg<0, PTR(OSContStatus)>(rdram, ctx);

    osContGetQuery(PASS_RDRAM data);
}

extern "C" void osContSetCh_recomp(uint8_t* rdram, recomp_context* ctx) {
    u8 ch = _arg<0, u8>(rdram, ctx);

    s32 ret = osContSetCh(PASS_RDRAM ch);

    _return<s32>(ctx, ret);
}

extern "C" void __osMotorAccess_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) pfs = _arg<0, PTR(void)>(rdram, ctx);
    s32 flag = _arg<1, s32>(rdram, ctx);
    s32 channel = MEM_W(8, pfs);

    // Only respect accesses to controller 0.
    if (channel == 0) {
        input_callbacks.set_rumble(flag);
    }

    _return<s32>(ctx, 0);
}

extern "C" void osMotorInit_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) pfs = _arg<1, PTR(void)>(rdram, ctx);
    s32 channel = _arg<2, s32>(rdram, ctx);
    MEM_W(8, pfs) = channel;

    _return<s32>(ctx, 0);
}

extern "C" void osMotorStart_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) pfs = _arg<0, PTR(void)>(rdram, ctx);
    s32 channel = MEM_W(8, pfs);

    // Only respect accesses to controller 0.
    if (channel == 0) {
        input_callbacks.set_rumble(true);
    }

    _return<s32>(ctx, 0);
}

extern "C" void osMotorStop_recomp(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) pfs = _arg<0, PTR(void)>(rdram, ctx);
    s32 channel = MEM_W(8, pfs);

    // Only respect accesses to controller 0.
    if (channel == 0) {
        input_callbacks.set_rumble(false);
    }

    _return<s32>(ctx, 0);
}
