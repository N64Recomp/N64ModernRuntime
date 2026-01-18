#ifndef __ULTRAMODERN_SAVE_HPP__
#define __ULTRAMODERN_SAVE_HPP__

#include <ultramodern/ultramodern.hpp>

namespace ultramodern {
    enum class SaveType {
        None,
        Eep4k,
        Eep16k,
        Sram,
        Flashram,
        AllowAll, // Allows all save types to work and reports eeprom size as 16kbit.
    };

    SaveType get_save_type();
}

#endif // __ULTRAMODERN_SAVE_HPP__

