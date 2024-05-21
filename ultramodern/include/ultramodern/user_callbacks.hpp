#ifndef __USER_CALLBACKS_HPP__
#define __USER_CALLBACKS_HPP__

namespace ultramodern {
    struct UserCallbacks {
        // TODO: Do we want those functions to take a generic `void *arg` for user data?

        // TODO: Consider renaming some functions to something more general,
        // like `update_rumble` -> `update_controller`

        void (*update_rumble)();
        void (*update_supported_options)();

        // TODO: Since we have a destroy_ui we could provide an init_ui?
        // void (*init_ui)();
        void (*destroy_ui)();

        /**
         * Show an OS dialog with the given `msg`.
         * `msg` is non-NULL.
         */
        void (*message_box)(const char* msg);
    };

    void register_user_callbacks(UserCallbacks& callbacks);
    const UserCallbacks& get_user_callbacks();
};

#endif
