#include "ultramodern/user_callbacks.hpp"

static ultramodern::UserCallbacks s_user_callbacks {};

void ultramodern::register_user_callbacks(UserCallbacks& callbacks) {
    s_user_callbacks = callbacks;
}

const ultramodern::UserCallbacks& ultramodern::get_user_callbacks() {
    return s_user_callbacks;
}
