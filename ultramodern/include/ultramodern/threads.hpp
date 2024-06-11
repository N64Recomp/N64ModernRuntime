#ifndef __THREADS_HPP__
#define __THREADS_HPP__

#include <string>

#include "ultra64.h"

namespace ultramodern {
    namespace threads {
        struct callbacks_t {
            using get_game_thread_name_t = std::string(OSThread* t);

            /**
             * Allows to specifyin a custom name for each thread. Mainly for debugging purposes.
             *
             * If this function is not provided then the thread id will be used as the name of the thread.
             */
            get_game_thread_name_t *get_game_thread_name;
        };

        void set_callbacks(const callbacks_t& callbacks);

        std::string get_game_thread_name(OSThread* t);
    }
}

#endif
