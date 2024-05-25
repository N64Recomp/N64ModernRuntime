#include <cassert>
#include <cstring>

#include "rsp_stuff.hpp"

static ultramodern::rsp::callbacks_t rsp_callbacks {};

void ultramodern::rsp::set_callbacks(const callbacks_t& callbacks) {
    rsp_callbacks = callbacks;
}

void ultramodern::rsp::init() {
    if (rsp_callbacks.init != nullptr) {
        rsp_callbacks.init();
    }
}

bool ultramodern::rsp::run_microcode(RDRAM_ARG const OSTask* task) {
    assert(rsp_callbacks.run_microcode != nullptr);

    return rsp_callbacks.run_microcode(PASS_RDRAM task);
}
