#ifndef __RECOMP_OVERLAYS_H__
#define __RECOMP_OVERLAYS_H__

#include <cstdint>
#include "sections.h"

namespace recomp {
    struct overlay_section_table_data_t {
        SectionTableEntry* code_sections;
        size_t num_code_sections;
        size_t total_num_sections;
    };

    struct overlays_by_index_t {
        int* table;
        size_t len;
    };

    void register_overlays(const overlay_section_table_data_t& sections, const overlays_by_index_t& overlays);
    void register_patch_section(SectionTableEntry* code_sections);
    void load_patch_functions();
};

extern "C" void load_overlays(uint32_t rom, int32_t ram_addr, uint32_t size);
extern "C" void unload_overlays(int32_t ram_addr, uint32_t size);
void init_overlays();

#endif
