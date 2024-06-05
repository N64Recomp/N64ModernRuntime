#include <unordered_map>
#include <algorithm>
#include <vector>
#include "recomp.h"
#include "sections.h"
#include "overlays.hpp"
#include "ultramodern/ultramodern.hpp"

static SectionTableEntry* code_sections = nullptr;

void load_special_overlay(const SectionTableEntry& section, int32_t ram);

void recomp::register_patch_section(SectionTableEntry* sections) {
    code_sections = sections;
}

void recomp::load_patch_functions() {
    if (code_sections == nullptr) {
        debug_printf("[Patch] No patch section was registered\n");
        return;
    }
    load_special_overlay(code_sections[0], code_sections[0].ram_addr);
}
