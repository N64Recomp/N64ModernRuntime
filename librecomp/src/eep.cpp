#include "recomp.h"
#include "librecomp/game.hpp"

#include "ultramodern/ultra64.h"

void save_write(RDRAM_ARG PTR(void) rdram_address, uint32_t offset, uint32_t count);
void save_read(RDRAM_ARG PTR(void) rdram_address, uint32_t offset, uint32_t count);

constexpr int eeprom_block_size = 8;

extern "C" void osEepromProbe_recomp(uint8_t* rdram, recomp_context* ctx) {
    switch (recomp::get_save_type()) {
        case recomp::SaveType::AllowAll:
        case recomp::SaveType::Eep16k:
            ctx->r2 = 0x02; // EEPROM_TYPE_16K
            break;
        case recomp::SaveType::Eep4k:
            ctx->r2 = 0x01; // EEPROM_TYPE_4K
            break;
        default:
            ctx->r2 = 0x00;
            break;
    }
}

extern "C" void osEepromWrite_recomp(uint8_t* rdram, recomp_context* ctx) {
    if (!recomp::eeprom_allowed()) {
        ultramodern::error_handling::message_box("Attempted to use EEPROM saving with other save type");
        ULTRAMODERN_QUICK_EXIT();
    }

    uint8_t eep_address = ctx->r5;
    gpr buffer = ctx->r6;
    int32_t nbytes = eeprom_block_size;

    save_write(rdram, buffer, eep_address * eeprom_block_size, nbytes);

    ctx->r2 = 0;
}

extern "C" void osEepromLongWrite_recomp(uint8_t* rdram, recomp_context* ctx) {
    if (!recomp::eeprom_allowed()) {
        ultramodern::error_handling::message_box("Attempted to use EEPROM saving with other save type");
        ULTRAMODERN_QUICK_EXIT();
    }

    uint8_t eep_address = ctx->r5;
    gpr buffer = ctx->r6;
    int32_t nbytes = ctx->r7;

    assert((nbytes % eeprom_block_size) == 0);

    save_write(rdram, buffer, eep_address * eeprom_block_size, nbytes);

    ctx->r2 = 0;
}

extern "C" void osEepromRead_recomp(uint8_t* rdram, recomp_context* ctx) {
    if (!recomp::eeprom_allowed()) {
        ultramodern::error_handling::message_box("Attempted to use EEPROM saving with other save type");
        ULTRAMODERN_QUICK_EXIT();
    }

    uint8_t eep_address = ctx->r5;
    gpr buffer = ctx->r6;
    int32_t nbytes = eeprom_block_size;

    save_read(rdram, buffer, eep_address * eeprom_block_size, nbytes);

    ctx->r2 = 0;
}

extern "C" void osEepromLongRead_recomp(uint8_t* rdram, recomp_context* ctx) {
    if (!recomp::eeprom_allowed()) {
        ultramodern::error_handling::message_box("Attempted to use EEPROM saving with other save type");
        ULTRAMODERN_QUICK_EXIT();
    }

    uint8_t eep_address = ctx->r5;
    gpr buffer = ctx->r6;
    int32_t nbytes = ctx->r7;

    assert((nbytes % eeprom_block_size) == 0);

    save_read(rdram, buffer, eep_address * eeprom_block_size, nbytes);

    ctx->r2 = 0;
}
