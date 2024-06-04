#ifndef __RENDERER_WRAPPER_HPP__
#define __RENDERER_WRAPPER_HPP__

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "ultra64.h"
#include "config.hpp"

namespace ultramodern {

    // TODO: should we move the WindowHandle definition here?
    struct WindowHandle;

    namespace renderer {
        enum class SetupResult {
            Success,
            DynamicLibrariesNotFound,
            InvalidGraphicsAPI,
            GraphicsAPINotFound,
            GraphicsDeviceNotFound
        };

        class RendererContext {
            public:
                virtual ~RendererContext() = default;

                virtual bool valid() = 0;
                virtual SetupResult get_setup_result() const { return setup_result; }

                virtual void update_config(const GraphicsConfig& old_config, const GraphicsConfig& new_config) = 0;

                virtual void enable_instant_present() = 0;
                virtual void send_dl(const OSTask* task) = 0;
                virtual void update_screen(uint32_t vi_origin) = 0;
                virtual void shutdown() = 0;
                virtual uint32_t get_display_framerate() const = 0;
                virtual float get_resolution_scale() const = 0;
                virtual void load_shader_cache(std::span<const char> cache_binary) = 0;

            protected:
                SetupResult setup_result;
        };

        struct callbacks_t {
            using create_render_context_t = std::unique_ptr<RendererContext>(uint8_t* rdram, WindowHandle window_handle, bool developer_mode);

            create_render_context_t *create_render_context;
        };

        void set_callbacks(const callbacks_t& callbacks);

        std::unique_ptr<RendererContext> create_render_context(uint8_t* rdram, WindowHandle window_handle, bool developer_mode);
    }
}


#endif
