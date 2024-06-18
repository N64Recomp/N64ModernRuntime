#ifndef __THREADS_HPP__
#define __THREADS_HPP__

#include <string>

#include "ultra64.h"

namespace ultramodern {
    namespace threads {
        struct callbacks_t {
            using get_game_thread_name_t = std::string(const OSThread* t);

            /**
             * Allows to specifying a custom name for each thread. Mainly for debugging purposes.
             *
             * For maximum cross-platform compatibility the returned name should be at most 15 bytes long (16 bytes including the null terminator).
             *
             * If this function is not provided then the thread id will be used as the name of the thread.
             */
            get_game_thread_name_t *get_game_thread_name;
        };

        void set_callbacks(const callbacks_t& callbacks);

        std::string get_game_thread_name(const OSThread* t);
    }
}

#endif
