#ifndef __RSP_STUFF_HPP__
#define __RSP_STUFF_HPP__

// TODO: rename

#include <cstdint>

#include "ultra64.h"

// TODO: Move these to ultramodern namespace?

enum class RspExitReason {
    Invalid,
    Broke,
    ImemOverrun,
    UnhandledJumpTarget,
    Unsupported
};

using RspUcodeFunc = RspExitReason(uint8_t* rdram);

extern uint8_t dmem[];
extern uint16_t rspReciprocals[512];
extern uint16_t rspInverseSquareRoots[512];

namespace ultramodern {
    namespace rsp {
        struct rsp_callbacks_t {
            /**
             * Simulate a DMA copy from RDRAM (CPU) to DMEM (RSP).
             *
             * This function should fill the ultramodern's `dmem` by reading from the `rdram` parameter.
             */
            void (*dma_rdram_to_dmem)(uint8_t* rdram, uint32_t dmem_addr, uint32_t dram_addr, uint32_t rd_len);

            /**
             * Return a function pointer to the corresponding RSP microcode function for the given `task_type`.
             *
             * The full OSTask (`task` parameter) is passed in case the `task_type` number is not enough information to distinguish out the exact microcode function.
             *
             * This function is allowed to return `nullptr` if no microcode matches the specified task. In this case a message will be printed to stderr and the program will exit.
             */
            RspUcodeFunc* (*get_rsp_microcode)(uint32_t task_type, OSTask* task);
        };

        void set_callbacks(const rsp_callbacks_t& callbacks);

        void constants_init();

        RspUcodeFunc* get_microcode(uint32_t task_type, OSTask* task);
        void run_microcode(uint8_t* rdram, const OSTask* task, RspUcodeFunc* ucode_func);
    };
} // namespace ultramodern

#endif
