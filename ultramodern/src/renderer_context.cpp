#include <cassert>
#include <mutex>

#include "ultramodern/renderer_context.hpp"
#include "ultramodern/ultramodern.hpp"

static ultramodern::renderer::callbacks_t render_callbacks{};

void ultramodern::renderer::set_callbacks(const callbacks_t& callbacks) {
    render_callbacks = callbacks;
}


std::unique_ptr<ultramodern::renderer::RendererContext> ultramodern::renderer::create_render_context(uint8_t* rdram, WindowHandle window_handle, bool developer_mode) {
    if (render_callbacks.create_render_context == nullptr) {
        error_handling::message_box("[Error] The mandatory render callback `create_render_context` was not registered");
        ULTRAMODERN_QUICK_EXIT();
    }

    return render_callbacks.create_render_context(rdram, window_handle, developer_mode);
}

std::string ultramodern::renderer::get_graphics_api_name(GraphicsApi api) {
    if (render_callbacks.get_graphics_api_name != nullptr) {
        return render_callbacks.get_graphics_api_name(api);
    }
    switch (api) {
    case ultramodern::renderer::GraphicsApi::Auto:
        return "Auto";
    case ultramodern::renderer::GraphicsApi::D3D12:
        return "D3D12";
    case ultramodern::renderer::GraphicsApi::Vulkan:
        return "Vulkan";
    case ultramodern::renderer::GraphicsApi::Metal:
        return "Metal";
    default:
        return "[Unknown graphics API]";
    }
}


static ultramodern::renderer::GraphicsConfig graphic_config{};
static std::mutex graphic_config_mutex;

void ultramodern::renderer::set_graphics_config(const GraphicsConfig& config) {
    std::lock_guard<std::mutex> lock(graphic_config_mutex);
    graphic_config = config;
    ultramodern::trigger_config_action();
}

const ultramodern::renderer::GraphicsConfig& ultramodern::renderer::get_graphics_config() {
    std::lock_guard<std::mutex> lock(graphic_config_mutex);
    return graphic_config;
}
