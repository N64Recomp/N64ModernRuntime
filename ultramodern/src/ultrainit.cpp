#include "ultra64.h"
#include "ultramodern.hpp"

void ultramodern::set_callbacks(
    const rsp::callbacks_t& rsp_callbacks,
    const audio_callbacks_t& audio_callbacks,
    const input_callbacks_t& input_callbacks,
    const gfx_callbacks_t& gfx_callbacks,
    const events::callbacks_t& thread_callbacks,
    const error_handling::callbacks_t& error_handling_callbacks
) {
    ultramodern::rsp::set_callbacks(rsp_callbacks);
    ultramodern::set_audio_callbacks(audio_callbacks);
    (void)input_callbacks; // nothing yet
    (void)gfx_callbacks; // nothing yet
    ultramodern::events::set_callbacks(thread_callbacks);
    ultramodern::error_handling::set_callbacks(error_handling_callbacks);
}

void ultramodern::preinit(RDRAM_ARG ultramodern::WindowHandle window_handle) {
    ultramodern::set_main_thread();
    ultramodern::init_events(PASS_RDRAM window_handle);
    ultramodern::init_timers(PASS_RDRAM1);
    ultramodern::init_audio();
    ultramodern::init_thread_cleanup();
}

extern "C" void osInitialize() {
}
