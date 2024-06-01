#ifndef __RSP_HPP__
#define __RSP_HPP__

#include <cstdint>

#include "ultra64.h"

namespace ultramodern {
    namespace rsp {
        struct callbacks_t {
            using init_t = void();
            using run_microcode_t = bool(RDRAM_ARG const OSTask* task);

            init_t* init;

            /**
             * Executes the given RSP task.
             *
             * Returns true if task was executed successfully.
             */
            run_microcode_t* run_task;
        };

        void set_callbacks(const callbacks_t& callbacks);

        void init();
        bool run_task(RDRAM_ARG const OSTask* task);
    };
} // namespace ultramodern

#endif
