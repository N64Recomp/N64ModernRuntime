#ifndef __LIBRECOMP_SAVE_HPP__
#define __LIBRECOMP_SAVE_HPP__

#include <cstdint>

#include "ultramodern/ultra64.h"

namespace recomp {
    namespace save {
        void init(RDRAM_ARG1);
        void join_thread();

        void write_ptr(const void* in, uint32_t offset, uint32_t count);
        void write(RDRAM_ARG PTR(void) rdram_address, uint32_t offset, uint32_t count);
        void read(RDRAM_ARG PTR(void) rdram_address, uint32_t offset, uint32_t count);
        void clear(uint32_t start, uint32_t size, char value);
    }
}

#endif
