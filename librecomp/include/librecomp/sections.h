#ifndef __SECTIONS_H__
#define __SECTIONS_H__

#include <stdint.h>
#include "recomp.h"

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))

typedef struct {
    recomp_func_t* func;
    uint32_t offset;
    uint32_t rom_size;
} FuncEntry;

typedef enum {
    R_MIPS_NONE = 0,
    R_MIPS_16,
    R_MIPS_32,
    R_MIPS_REL32,
    R_MIPS_26,
    R_MIPS_HI16,
    R_MIPS_LO16,
    R_MIPS_GPREL16,
} RelocEntryType;

typedef struct {
    // Offset into the section of the word to relocate.
    uint32_t offset;
    // Reloc addend from the target section's address.
    uint32_t target_section_offset;
    // Index of the target section (indexes into `section_addresses`).
    uint16_t target_section;
    // Relocation type.
    RelocEntryType type;
} RelocEntry;

typedef struct {
    uint32_t rom_addr;
    uint32_t ram_addr;
    uint32_t size;
    FuncEntry *funcs;
    size_t num_funcs;
    RelocEntry* relocs;
    size_t num_relocs;
    size_t index;
} SectionTableEntry;

typedef struct {
    const char* name;
    uint32_t ram_addr;
} FunctionExport;

typedef struct {
    uint32_t ram_addr;
    recomp_func_t* func;
} ManualPatchSymbol;

#endif
