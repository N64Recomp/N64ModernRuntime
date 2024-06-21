#ifndef __LIBRECOMP_SAVE_HPP__
#define __LIBRECOMP_SAVE_HPP__

#include <cstdint>

namespace recomp {
    namespace save {
        void init(uint8_t *rdram);
        void join_thread();
    }
}

#endif
