#include <cassert>
#include <cstring>

#include "rsp_stuff.hpp"


static ultramodern::rsp::rsp_callbacks_t rsp_callbacks;

void ultramodern::rsp::set_callbacks(const ultramodern::rsp::rsp_callbacks_t& callbacks) {
    rsp_callbacks = callbacks;
}


uint8_t dmem[0x1000];
uint16_t rspReciprocals[512];
uint16_t rspInverseSquareRoots[512];

// From Ares emulator. For license details, see rsp_vu.h
void ultramodern::rsp::constants_init() {
    rspReciprocals[0] = u16(~0);
    for (u16 index = 1; index < 512; index++) {
        u64 a = index + 512;
        u64 b = (u64(1) << 34) / a;
        rspReciprocals[index] = u16((b + 1) >> 8);
    }

    for (u16 index = 0; index < 512; index++) {
        u64 a = (index + 512) >> ((index % 2 == 1) ? 1 : 0);
        u64 b = 1 << 17;
        //find the largest b where b < 1.0 / sqrt(a)
        while (a * (b + 1) * (b + 1) < (u64(1) << 44)) b++;
        rspInverseSquareRoots[index] = u16(b >> 1);
    }
}

RspUcodeFunc* ultramodern::rsp::get_microcode(uint32_t task_type, OSTask* task) {
    assert(rsp_callbacks.get_rsp_microcode != nullptr);

    return rsp_callbacks.get_rsp_microcode(task_type, task);
}

// Runs a recompiled RSP microcode
void ultramodern::rsp::run_microcode(uint8_t* rdram, const OSTask* task, RspUcodeFunc* ucode_func) {
    // Load the OSTask into DMEM
    memcpy(&dmem[0xFC0], task, sizeof(OSTask));

    assert(rsp_callbacks.dma_rdram_to_dmem != nullptr);

    // Load the ucode data into DMEM
    rsp_callbacks.dma_rdram_to_dmem(rdram, 0x0000, task->t.ucode_data, 0xF80 - 1);

    // Run the ucode
    RspExitReason exit_reason = ucode_func(rdram);
    // Ensure that the ucode exited correctly
    assert(exit_reason == RspExitReason::Broke);
}

