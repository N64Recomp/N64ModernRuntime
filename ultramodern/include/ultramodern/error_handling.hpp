#ifndef __ERROR_HANDLING_HPP__
#define __ERROR_HANDLING_HPP__

#include <cstdlib>

#define ULTRAMODERN_QUICK_EXIT() ultramodern::error_handling::quick_exit(__FILE__, __LINE__, __func__)

namespace ultramodern {
    namespace error_handling {
        struct callbacks_t {
            using message_box_t = void(const char* msg);

            /**
             * Show an OS dialog with the given `msg`.
             *
             * The `msg` parameter is always non-`nullptr`.
             */
            message_box_t *message_box;
        };

        void set_callbacks(const callbacks_t& callbacks);

        void message_box(const char* msg);

        [[noreturn]] void quick_exit(const char* filename, int line, const char *func, int exit_status = EXIT_FAILURE);
    }
}

#endif
