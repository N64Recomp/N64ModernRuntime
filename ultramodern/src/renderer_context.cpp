#include <cassert>
#include <mutex>

#include "ultramodern/renderer_context.hpp"
#include "ultramodern/ultramodern.hpp"

static ultramodern::renderer::callbacks_t render_callbacks{};

void ultramodern::renderer::set_callbacks(const callbacks_t& callbacks) {
    render_callbacks = callbacks;
}

std::unique_ptr<ultramodern::renderer::RendererContext>
ultramodern::renderer::create_render_context(uint8_t *rdram, WindowHandle window_handle, bool developer_mode) {
    if (render_callbacks.create_render_context == nullptr) {
        error_handling::message_box("[Error] The mandatory render callback `create_render_context` was not registered");
        ULTRAMODERN_QUICK_EXIT();
    }

    return render_callbacks.create_render_context(rdram, window_handle, developer_mode);
}

std::string ultramodern::renderer::get_graphics_api_name(const GraphicsConfig& config) {
    if (render_callbacks.get_graphics_api_name != nullptr) {
        return render_callbacks.get_graphics_api_name(config);
    }
    return config.get_graphics_api_name();
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

std::string ultramodern::renderer::GraphicsConfig::get_graphics_api_name() const {
    ultramodern::renderer::GraphicsApi api = api_option;

    if (api == ultramodern::renderer::GraphicsApi::Auto) {
#if defined(_WIN32)
        api = ultramodern::renderer::GraphicsApi::D3D12;
#elif defined(__gnu_linux__)
        api = ultramodern::renderer::GraphicsApi::Vulkan;
#elif defined(__APPLE__)
        // TODO: Add MoltenVK option for Mac?
        api = ultramodern::renderer::GraphicsApi::Vulkan;
#else
        static_assert(false && "Unimplemented")
#endif
    }

    switch (api) {
        case ultramodern::renderer::GraphicsApi::D3D12:
            return "D3D12";
        case ultramodern::renderer::GraphicsApi::Vulkan:
            return "Vulkan";
        default:
            return "[Unknown graphics API]";
    }
}
