#include "ultramodern/renderer_wrapper.hpp"
#include "ultramodern/ultramodern.hpp"

static ultramodern::renderer::callbacks_t render_callbacks{};

void set_callbacks(const ultramodern::renderer::callbacks_t& callbacks) {
    render_callbacks = callbacks;
}


std::unique_ptr<ultramodern::renderer::RendererContext> ultramodern::renderer::create_render_context(uint8_t* rdram, WindowHandle window_handle, bool developer_mode) {
    if (render_callbacks.create_render_context == nullptr) {
        error_handling::message_box("[Error] The render callback `create_render_context` was not registered");
        // TODO: should we make a macro for this?
        error_handling::quick_exit(__FILE__, __LINE__, __func__);
    }

    return render_callbacks.create_render_context(rdram, window_handle, developer_mode);
}
