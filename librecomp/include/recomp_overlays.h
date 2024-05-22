#ifndef __RECOMP_OVERLAYS_H__
#define __RECOMP_OVERLAYS_H__

#include <cstdint>
#include "sections.h"

extern "C" SectionTableEntry* get_section_table();
extern "C" size_t get_num_sections();
extern "C" int* get_overlay_sections_by_index();

extern "C" void load_overlays(uint32_t rom, int32_t ram_addr, uint32_t size);
extern "C" void unload_overlays(int32_t ram_addr, uint32_t size);
void init_overlays();

#endif