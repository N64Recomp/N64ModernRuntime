#ifndef __USER_CALLBACKS_HPP__
#define __USER_CALLBACKS_HPP__

#include "rsp_stuff.hpp"
#include "ultra64.h"

namespace ultramodern {
    struct UserCallbacks {
        // TODO: Do we want those functions to take a generic `void *arg` for user data?

        // TODO: Consider renaming some functions to something more general,
        // like `update_rumble` -> `update_controller`

        void (*update_rumble)();
        void (*update_supported_options)();

        /**
         * Show an OS dialog with the given `msg`.
         * `msg` is non-`nullptr`.
         */
        void (*message_box)(const char* msg);
    };

    /**
     *
     */
    void register_user_callbacks(UserCallbacks& callbacks);

    /**
     *
     */
    const UserCallbacks& get_user_callbacks();
};

#endif
