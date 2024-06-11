#ifndef __RENDERER_WRAPPER_HPP__
#define __RENDERER_WRAPPER_HPP__

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#elif defined(__ANDROID__)
#    include "android/native_window.h"
#elif defined(__linux__)
#    include "X11/Xlib.h"
#    undef None
#    undef Status
#    undef LockMask
#    undef Always
#    undef Success
#endif

#include "config.hpp"
#include "ultra64.h"

namespace ultramodern {
    namespace renderer {

#if defined(_WIN32)
        // Native HWND handle to the target window.
        struct WindowHandle {
            HWND window;
            DWORD thread_id = (DWORD)-1;
            auto operator<=>(const WindowHandle&) const = default;
        };
#elif defined(__ANDROID__)
        using WindowHandle = ANativeWindow *;
#elif defined(__linux__)
        struct WindowHandle {
            Display *display;
            Window window;
            auto operator<=>(const WindowHandle&) const = default;
        };
#elif defined(__APPLE__)
        struct WindowHandle {
            void *window;
            void *view;
            auto operator<=>(const WindowHandle&) const = default;
        };
#endif

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
            virtual SetupResult get_setup_result() const {
                return setup_result;
            }

            virtual bool update_config(const GraphicsConfig& old_config, const GraphicsConfig& new_config) = 0;

            virtual void enable_instant_present() = 0;
            virtual void send_dl(const OSTask *task) = 0;
            virtual void update_screen(uint32_t vi_origin) = 0;
            virtual void shutdown() = 0;
            virtual uint32_t get_display_framerate() const = 0;
            virtual float get_resolution_scale() const = 0;
            virtual void load_shader_cache(std::span<const char> cache_binary) = 0;

        protected:
            SetupResult setup_result;
        };

        struct callbacks_t {
            using create_render_context_t =
                std::unique_ptr<RendererContext>(uint8_t *rdram, WindowHandle window_handle, bool developer_mode);
            using get_graphics_api_name_t = std::string(const GraphicsConfig& config);

            /**
             * Instances a subclass of RendererContext that is used to render the game.
             *
             * This callback is mandatory for using the library.
             */
            create_render_context_t *create_render_context;

            /**
             * This callback is optional. If not provided a library default will be used.
             */
            get_graphics_api_name_t *get_graphics_api_name;
        };

        void set_callbacks(const callbacks_t& callbacks);

        std::unique_ptr<RendererContext> create_render_context(uint8_t *rdram, WindowHandle window_handle, bool developer_mode);

        std::string get_graphics_api_name(const GraphicsConfig& config);
    } // namespace renderer
} // namespace ultramodern

#endif
