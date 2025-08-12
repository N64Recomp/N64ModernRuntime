#ifndef __RENDERER_WRAPPER_HPP__
#define __RENDERER_WRAPPER_HPP__

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#elif defined(__ANDROID__)
#   include "android/native_window.h"
#elif defined(__linux__)
#   undef None
#   undef Status
#   undef LockMask
#   undef Always
#   undef Success
#   undef False
#   undef True
#endif

#include "ultra64.h"
#include "config.hpp"

struct SDL_Window;

namespace ultramodern {
    namespace renderer {
        struct ViRegs {
            unsigned int VI_STATUS_REG;
            unsigned int VI_ORIGIN_REG;
            unsigned int VI_WIDTH_REG;
            unsigned int VI_INTR_REG;
            unsigned int VI_V_CURRENT_LINE_REG;
            unsigned int VI_TIMING_REG;
            unsigned int VI_V_SYNC_REG;
            unsigned int VI_H_SYNC_REG;
            unsigned int VI_LEAP_REG;
            unsigned int VI_H_START_REG;
            unsigned int VI_V_START_REG;
            unsigned int VI_V_BURST_REG;
            unsigned int VI_X_SCALE_REG;
            unsigned int VI_Y_SCALE_REG;
        };
        ViRegs* get_vi_regs();

#if defined(_WIN32)
        // Native HWND handle to the target window.
        struct WindowHandle {
            HWND window;
            DWORD thread_id = (DWORD)-1;
            auto operator<=>(const WindowHandle&) const = default;
        };
// TODO add a native window handle option here (Display/Window for x11 and ANativeWindow for Android) as a compile-time option.
#elif defined(__linux__) || defined(__ANDROID__)
        using WindowHandle = SDL_Window*;
#elif defined(__APPLE__)
        struct WindowHandle {
            void* window;
            void* view;
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
                virtual SetupResult get_setup_result() const { return setup_result; }
                virtual GraphicsApi get_chosen_api() const { return chosen_api; }

                virtual bool update_config(const GraphicsConfig& old_config, const GraphicsConfig& new_config) = 0;

                virtual void enable_instant_present() = 0;
                virtual void send_dl(const OSTask* task) = 0;
                virtual void update_screen() = 0;
                virtual void shutdown() = 0;
                virtual uint32_t get_display_framerate() const = 0;
                virtual float get_resolution_scale() const = 0;

            protected:
                SetupResult setup_result;
                GraphicsApi chosen_api;
        };

        struct callbacks_t {
            using create_render_context_t = std::unique_ptr<RendererContext>(uint8_t* rdram, WindowHandle window_handle, bool developer_mode);
            using get_graphics_api_name_t = std::string(GraphicsApi api);

            /**
             * Instances a subclass of RendererContext that is used to render the game.
             *
             * This callback is mandatory for using the library.
             */
            create_render_context_t *create_render_context;

            /**
             * This callback is optional. If not provided a library default will be used.
             */
            get_graphics_api_name_t *get_graphics_api_name = nullptr;
        };

        void set_callbacks(const callbacks_t& callbacks);

        std::unique_ptr<RendererContext> create_render_context(uint8_t* rdram, WindowHandle window_handle, bool developer_mode);

        std::string get_graphics_api_name(GraphicsApi api);
    }
}

#endif
