#ifndef __EVENTS_HPP__
#define __EVENTS_HPP__

namespace ultramodern {
    namespace events {
        struct callbacks_t {
            using vi_callback_t = void();
            using gfx_init_callback_t = void();

            /**
             * Called in each VI.
             */
            vi_callback_t* vi_callback;

            /**
             * Called before entering the gfx main loop.
             */
            gfx_init_callback_t* gfx_init_callback;
        };

        void set_callbacks(const callbacks_t& callbacks);
    }
}

#endif
