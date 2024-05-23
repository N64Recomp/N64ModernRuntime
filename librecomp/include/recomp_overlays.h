#ifndef __RECOMP_OVERLAYS_H__
#define __RECOMP_OVERLAYS_H__

#include <cstdint>
#include "sections.h"

namespace recomp {
    namespace overlays {
        struct section_table_data_t {
            SectionTableEntry* code_sections;
            size_t num_code_sections;
            size_t total_num_sections;
        };

        struct overlays_by_index_t {
            int* table;
            size_t len;
        };

        void register_overlays(const section_table_data_t& sections, const overlays_by_index_t& overlays);

        extern section_table_data_t sections_info;
        extern overlays_by_index_t overlays_info;
    }
}

extern "C" void load_overlays(uint32_t rom, int32_t ram_addr, uint32_t size);
extern "C" void unload_overlays(int32_t ram_addr, uint32_t size);
void init_overlays();

#endif
