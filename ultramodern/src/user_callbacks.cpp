#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "ultramodern/user_callbacks.hpp"

static ultramodern::UserCallbacks s_user_callbacks {};
static bool s_callbacks_initialized = false;

void ultramodern::register_user_callbacks(UserCallbacks& callbacks) {
    s_user_callbacks = callbacks;

    if (s_user_callbacks.dma_rdram_to_dmem == nullptr) {
        fprintf(stderr, "%s: `dma_rdram_to_dmem` is a required callback, it can't be `nullptr`\n", __func__);
        assert(false);
        std::quick_exit(EXIT_FAILURE);
    }
    if (s_user_callbacks.get_rsp_microcode == nullptr) {
        fprintf(stderr, "%s: `get_rsp_microcode` is a required callback, it can't be `nullptr`\n", __func__);
        assert(false);
        std::quick_exit(EXIT_FAILURE);
    }

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
