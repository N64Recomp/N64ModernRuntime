#ifndef __LIBRECOMP_GFX_HPP__
#define __LIBRECOMP_GFX_HPP__

#include "ultramodern/renderer_context.hpp"

namespace recomp {
    namespace gfx {
        struct callbacks_t {
            using gfx_data_t = void*;
            using create_gfx_t = gfx_data_t();
            using create_window_t = ultramodern::renderer::WindowHandle(gfx_data_t);
            using update_gfx_t = void(gfx_data_t);

            create_gfx_t* create_gfx;
            create_window_t* create_window;
            update_gfx_t* update_gfx;
        };
    }

}

#endif
