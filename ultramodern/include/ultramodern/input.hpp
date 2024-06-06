#ifndef __ULTRAMODERN_INPUT_HPP__
#define __ULTRAMODERN_INPUT_HPP__

#include <cstdint>

namespace ultramodern {
    namespace input {
        enum class Device {
            None,
            Controller,
            // Mouse,
            // VRU,
        };

        enum class Pak {
            None,
            RumblePak,
            // ControllerPak,
            // TransferPak
        };

        struct connected_device_info_t {
            Device connected_device;
            Pak connected_pak;
        };

        struct callbacks_t {
            using poll_input_t = void(void);
            using get_input_t = bool(int controller_num, uint16_t* buttons, float* x, float* y);
            using set_rumble_t = void(int controller_num, bool rumble);
            using get_connected_device_info_t = connected_device_info_t(int controller_num);

            poll_input_t* poll_input;

            /**
             * Requests the state of the pressed buttons and the analog stick for the given `controller_num`.
             *
             * `controller_num` is zero-indexed, meaning 0 corresponds to the first controller.
             *
             * Returns `true` if was able to fetch the specified data, `false` otherwise and the parameter arguments are left untouched.
             */
            get_input_t* get_input;

            /**
             * Turns on or off rumbling for the specified controller.
             *
             * `controller_num` is zero-indexed, meaning 0 corresponds to the first controller.
             */
            set_rumble_t* set_rumble;

            /**
             * Returns the connected device info for the given `controller_num` (as in, the controller port of the console).
             *
             * `controller_num` is zero-indexed, meaning 0 corresponds to the first controller.
             */
            get_connected_device_info_t* get_connected_device_info;
        };

        void set_callbacks(const callbacks_t& callbacks);
    }
}

#endif
