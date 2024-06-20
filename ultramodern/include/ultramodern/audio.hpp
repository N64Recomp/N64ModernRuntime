#ifndef __ULTRAMODERN_AUDIO_HPP__
#define __ULTRAMODERN_AUDIO_HPP__

#include <cstddef>
#include <cstdint>

#include "ultra64.h"

namespace ultramodern {
    namespace audio {
        struct callbacks_t {
            using queue_samples_t = void(int16_t*, size_t);
            using get_samples_remaining_t = size_t();
            using set_frequency_t = void(uint32_t);

            queue_samples_t* queue_samples;
            get_samples_remaining_t* get_frames_remaining;
            set_frequency_t* set_frequency;
        };

        void set_callbacks(const callbacks_t& callbacks);

        void init();

        void set_frequency(uint32_t freq);
        void queue_buffer(RDRAM_ARG PTR(s16) audio_data, uint32_t byte_count);
        uint32_t get_remaining_bytes();
    }
}

#endif
