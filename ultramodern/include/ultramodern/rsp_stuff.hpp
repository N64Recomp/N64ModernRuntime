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
            using run_microcode_t = bool(RDRAM_ARG const OSTask* task);

            init_t* init;
            run_microcode_t* run_microcode;
        };

        void set_callbacks(const callbacks_t& callbacks);

        void init();
        bool run_microcode(RDRAM_ARG const OSTask* task);
    };
} // namespace ultramodern

#endif
