#include "recomp.h"
#include "librecomp/game.hpp"

#include <ultramodern/save.hpp>
#include <ultramodern/ultra64.h>

constexpr int eeprom_block_size = 8;

extern "C" void osEepromProbe_recomp(uint8_t* rdram, recomp_context* ctx) {
    switch (ultramodern::get_save_type()) {
        case ultramodern::SaveType::AllowAll:
        case ultramodern::SaveType::Eep16k:
            ctx->r2 = 0x02; // EEPROM_TYPE_16K
            break;
        case ultramodern::SaveType::Eep4k:
            ctx->r2 = 0x01; // EEPROM_TYPE_4K
            break;
        default:
            ctx->r2 = 0x00;
            break;
    }
}

extern "C" void osEepromWrite_recomp(uint8_t* rdram, recomp_context* ctx) {
    if (!ultramodern::eeprom_allowed()) {
        ultramodern::error_handling::message_box("Attempted to use EEPROM saving with other save type");
        ULTRAMODERN_QUICK_EXIT();
    }

    uint8_t eep_address = ctx->r5;
    gpr buffer = ctx->r6;
    int32_t nbytes = eeprom_block_size;

    ultramodern::save_write(rdram, buffer, eep_address * eeprom_block_size, nbytes);

    ctx->r2 = 0;
}

extern "C" void osEepromLongWrite_recomp(uint8_t* rdram, recomp_context* ctx) {
    if (!ultramodern::eeprom_allowed()) {
        ultramodern::error_handling::message_box("Attempted to use EEPROM saving with other save type");
        ULTRAMODERN_QUICK_EXIT();
    }

    uint8_t eep_address = ctx->r5;
    gpr buffer = ctx->r6;
    int32_t nbytes = ctx->r7;

    assert((nbytes % eeprom_block_size) == 0);

    ultramodern::save_write(rdram, buffer, eep_address * eeprom_block_size, nbytes);

    ctx->r2 = 0;
}

extern "C" void osEepromRead_recomp(uint8_t* rdram, recomp_context* ctx) {
    if (!ultramodern::eeprom_allowed()) {
        ultramodern::error_handling::message_box("Attempted to use EEPROM saving with other save type");
        ULTRAMODERN_QUICK_EXIT();
    }

    uint8_t eep_address = ctx->r5;
    gpr buffer = ctx->r6;
    int32_t nbytes = eeprom_block_size;

    ultramodern::save_read(rdram, buffer, eep_address * eeprom_block_size, nbytes);

    ctx->r2 = 0;
}

extern "C" void osEepromLongRead_recomp(uint8_t* rdram, recomp_context* ctx) {
    if (!ultramodern::eeprom_allowed()) {
        ultramodern::error_handling::message_box("Attempted to use EEPROM saving with other save type");
        ULTRAMODERN_QUICK_EXIT();
    }

    uint8_t eep_address = ctx->r5;
    gpr buffer = ctx->r6;
    int32_t nbytes = ctx->r7;

    assert((nbytes % eeprom_block_size) == 0);

    ultramodern::save_read(rdram, buffer, eep_address * eeprom_block_size, nbytes);

    ctx->r2 = 0;
}
