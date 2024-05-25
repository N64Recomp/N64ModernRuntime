#ifndef __ERROR_HANDLING_HPP__
#define __ERROR_HANDLING_HPP__

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
    }
}

#endif
