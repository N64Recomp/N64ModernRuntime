#include <unordered_map>
#include <algorithm>
#include <vector>
#include "recomp.h"
#include "sections.h"
#include "overlays.hpp"

static SectionTableEntry* code_sections = nullptr;

void load_special_overlay(const SectionTableEntry& section, int32_t ram);

void recomp::register_patch_section(SectionTableEntry* sections) {
    code_sections = sections;
}

void recomp::load_patch_functions() {
    load_special_overlay(code_sections[0], code_sections[0].ram_addr);
}
