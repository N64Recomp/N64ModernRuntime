#include <unordered_map>
#include <algorithm>
#include <vector>
#include "recomp.h"
#include "sections.h"
#include "recomp_overlays.h"

void load_special_overlay(const SectionTableEntry& section, int32_t ram);

void load_patch_functions() {
    load_special_overlay(get_section_table()[0], get_section_table()[0].ram_addr);
}
