#ifndef __RSP_STUFF_HPP__
#define __RSP_STUFF_HPP__

// TODO: rename

#include <cstdint>

#include "ultra64.h"

// TODO: Move these to ultramodern namespace?

namespace ultramodern {
    namespace rsp {
        struct callbacks_t {
            using init_t = void();
            using run_microcode_t = bool(RDRAM_ARG const OSTask *task);

            init_t *init;

            /**
             * Executes the given RSP task.
             *
             * Returns true if task was executed successfully.
             */
            run_microcode_t *run_task;
        };

        void set_callbacks(const callbacks_t& callbacks);

        void init();
        bool run_task(RDRAM_ARG const OSTask *task);
    }; // namespace rsp
} // namespace ultramodern

#endif
