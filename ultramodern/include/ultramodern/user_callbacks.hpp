#ifndef __USER_CALLBACKS_HPP__
#define __USER_CALLBACKS_HPP__

#include "rsp_stuff.hpp"
#include "ultra64.h"

namespace ultramodern {
    struct UserCallbacks {
        // TODO: Do we want those functions to take a generic `void *arg` for user data?

        // TODO: Consider renaming some functions to something more general,
        // like `update_rumble` -> `update_controller`

        void (*update_rumble)();
        void (*update_supported_options)();

        // TODO: Since we have a destroy_ui we could provide an init_ui?
        // void (*init_ui)();
        void (*destroy_ui)();

        /**
         * Show an OS dialog with the given `msg`.
         * `msg` is non-`nullptr`.
         */
        void (*message_box)(const char* msg);

        // RSP

        /**
         * Simulate a DMA copy from RDRAM (CPU) to DMEM (RSP).
         *
         * This function should fill the ultramodern's `dmem` by reading from the `rdram` parameter.
         *
         * IMPORTANTE: This callback is required and must be non-`nullptr` when initializing the user callbacks.
         */
        void (*dma_rdram_to_dmem)(uint8_t* rdram, uint32_t dmem_addr, uint32_t dram_addr, uint32_t rd_len);

        /**
         * Return a function pointer to the corresponding RSP microcode function for the given `task_type`.
         * 
         * The full OSTask (`task` parameter) is passed in case the `task_type` number is not enough information to distinguish out the exact microcode function.
         * 
         * This function is allowed to return `nullptr` if no microcode matches the specified task. In this case a message will be printed to stderr and the program will exit.
         * 
         * IMPORTANTE: This callback is required and must be non-`nullptr` when initializing the user callbacks.
         */
        RspUcodeFunc* (*get_rsp_microcode)(uint32_t task_type, OSTask* task);
    };

    /**
     * 
     */
    void register_user_callbacks(UserCallbacks& callbacks);

    /**
     * 
     */
    const UserCallbacks& get_user_callbacks();
};

#endif
