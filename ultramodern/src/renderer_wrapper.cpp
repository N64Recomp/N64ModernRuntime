#include <cassert>

#include "ultramodern/renderer_wrapper.hpp"
#include "ultramodern/ultramodern.hpp"

static ultramodern::renderer::callbacks_t render_callbacks{};

void ultramodern::renderer::set_callbacks(const callbacks_t& callbacks) {
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


static std::unique_ptr<const ultramodern::renderer::GraphicsConfig> graphic_config{};
static std::mutex graphic_config_mutex;

void ultramodern::renderer::set_graphics_config(std::unique_ptr<const GraphicsConfig>&& config) {
    std::lock_guard<std::mutex> lock(graphic_config_mutex);
    graphic_config.swap(config);
}

const ultramodern::renderer::GraphicsConfig* ultramodern::renderer::get_graphics_config() {
    std::lock_guard<std::mutex> lock(graphic_config_mutex);
    auto ptr = graphic_config.get();
    if (ptr == nullptr) {
        error_handling::message_box("[Error] The graphic configuration was not registered");
        // TODO: should we make a macro for this?
        error_handling::quick_exit(__FILE__, __LINE__, __func__);
    }
    return ptr;
}
