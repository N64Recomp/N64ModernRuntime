#include "librecomp/mods.hpp"
#include "librecomp/helpers.hpp"
#include "librecomp/addresses.hpp"

void recomp_get_config_u32(uint8_t* rdram, recomp_context* ctx, size_t mod_index) {
    recomp::mods::ConfigValueVariant val = recomp::mods::get_mod_config_value(mod_index, _arg_string<0>(rdram, ctx));
    if (uint32_t* as_u32 = std::get_if<uint32_t>(&val)) {
        _return(ctx, *as_u32);
    }
    else if (double* as_double = std::get_if<double>(&val)) {
        _return(ctx, uint32_t(int32_t(*as_double)));
    }
    else {
        _return(ctx, uint32_t{0});
    }
}

void recomp_get_config_double(uint8_t* rdram, recomp_context* ctx, size_t mod_index) {
    recomp::mods::ConfigValueVariant val = recomp::mods::get_mod_config_value(mod_index, _arg_string<0>(rdram, ctx));
    if (uint32_t* as_u32 = std::get_if<uint32_t>(&val)) {
        ctx->f0.d = double(*as_u32);
    }
    else if (double* as_double = std::get_if<double>(&val)) {
        ctx->f0.d = *as_double;
    }
    else {
        ctx->f0.d = 0.0;
    }
}

void recomp_get_config_string(uint8_t* rdram, recomp_context* ctx, size_t mod_index) {
    recomp::mods::ConfigValueVariant val = recomp::mods::get_mod_config_value(mod_index, _arg_string<0>(rdram, ctx));
    if (std::string* as_string = std::get_if<std::string>(&val)) {
        const std::string& str = *as_string;
        // Allocate space in the recomp heap to hold the string, including the null terminator.
        size_t alloc_size = (str.size() + 1 + 15) & ~15;
        gpr offset = reinterpret_cast<uint8_t*>(recomp::alloc(rdram, alloc_size)) - rdram;
        gpr addr = offset + 0xFFFFFFFF80000000ULL;

        // Copy the string's data into the allocated memory and null terminate it.
        for (size_t i = 0; i < str.size(); i++) {
            MEM_B(i, addr) = str[i];
        }
        MEM_B(str.size(), addr) = 0;

        // Return the allocated memory.
        ctx->r2 = addr;
    }
    else {
        _return(ctx, NULLPTR);
    }
}

void recomp_free_config_string(uint8_t* rdram, recomp_context* ctx) {
    gpr str_rdram = (gpr)_arg<0, PTR(char)>(rdram, ctx);
    gpr offset = str_rdram - 0xFFFFFFFF80000000ULL;

    recomp::free(rdram, rdram + offset);
}

void recomp::mods::register_config_exports() {
    recomp::overlays::register_ext_base_export("recomp_get_config_u32", recomp_get_config_u32);
    recomp::overlays::register_ext_base_export("recomp_get_config_double", recomp_get_config_double);
    recomp::overlays::register_ext_base_export("recomp_get_config_string", recomp_get_config_string);
    recomp::overlays::register_base_export("recomp_free_config_string", recomp_free_config_string);
}
