#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "ultramodern/user_callbacks.hpp"

static ultramodern::UserCallbacks s_user_callbacks {};
static bool s_callbacks_initialized = false;

void ultramodern::register_user_callbacks(UserCallbacks& callbacks) {
    s_user_callbacks = callbacks;

    s_callbacks_initialized = true;
}

const ultramodern::UserCallbacks& ultramodern::get_user_callbacks() {
    if (!s_callbacks_initialized) {
        fprintf(stderr, "%s: User callbacks have not been initialized.\n", __func__);
        assert(false);
        std::quick_exit(EXIT_FAILURE);
    }

    return s_user_callbacks;
}
