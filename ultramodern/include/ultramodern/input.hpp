#ifndef __ULTRAMODERN_INPUT_HPP__
#define __ULTRAMODERN_INPUT_HPP__

#include <cstdint>

namespace ultramodern {
    namespace input {
        struct callbacks_t {
            using poll_input_t = void(void);
            using get_input_t = bool(int controller_num, uint16_t* buttons, float* x, float* y);
            using set_rumble_t = void(int controller_num, bool rumble);

            poll_input_t* poll_input;

            /**
             * Requests the state of the pressed buttons and the analog stick for the given `controller_num`.
             *
             * Returns `true` if was able to fetch the specified data, `false` otherwise and the parameter arguments are left untouched.
             */
            get_input_t* get_input;

            /**
             * Turns on or off rumbling for the specified controller.
             */
            set_rumble_t* set_rumble;
        };

        void set_callbacks(const callbacks_t& callbacks);
    }
}

#endif
