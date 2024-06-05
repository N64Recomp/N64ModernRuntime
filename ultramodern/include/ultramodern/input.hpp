#ifndef __ULTRAMODERN_INPUT_HPP__
#define __ULTRAMODERN_INPUT_HPP__

#include <cstdint>

namespace ultramodern {
    namespace input {
        struct callbacks_t {
            using poll_input_t = void(void);
            using get_input_t = void(uint16_t*, float*, float*);
            using set_rumble_t = void(bool);

            poll_input_t* poll_input;
            get_input_t* get_input;
            set_rumble_t* set_rumble;
        };

        void set_callbacks(const callbacks_t& callbacks);


    }
}

#endif
