#ifndef __THREADS_HPP__
#define __THREADS_HPP__

#include <string>

#include "ultra64.h"

namespace ultramodern {
    namespace threads {
        enum class GameThreadType {
            Normal,
            Temporary,
            Permanent
        };

        struct callbacks_t {
            using get_game_thread_type_t = GameThreadType(OSThread* t);
            using get_game_thread_name_t = std::string(OSThread* t);

            /**
             * TODO: document, I don't understand what this is used for.
             */
            get_game_thread_type_t *get_game_thread_type;

            /**
             * Allows specifying a custom name for each thread.
             *
             * If this function is not provided then the thread id will be used as the name.
             */
            get_game_thread_name_t *get_game_thread_name;
        };

        void set_callbacks(const callbacks_t& callbacks);

        GameThreadType get_game_thread_type(OSThread* t);
        std::string get_game_thread_name(OSThread* t);
    }
}

#endif
