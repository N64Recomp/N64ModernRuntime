#include <algorithm>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include "recomp.h"
#include "overlays.hpp"
#include "sections.h"

static recomp::overlay_section_table_data_t sections_info {};
static recomp::overlays_by_index_t overlays_info {};

void recomp::register_overlays(const recomp::overlay_section_table_data_t& sections, const recomp::overlays_by_index_t& overlays) {
    sections_info = sections;
    overlays_info = overlays;
}

struct LoadedSection {
    int32_t loaded_ram_addr;
    size_t section_table_index;

    LoadedSection(int32_t loaded_ram_addr_, size_t section_table_index_) {
        loaded_ram_addr = loaded_ram_addr_;
        section_table_index = section_table_index_;
    }

    bool operator<(const LoadedSection& rhs) {
        return loaded_ram_addr < rhs.loaded_ram_addr;
    }
};

std::vector<LoadedSection> loaded_sections{};
std::unordered_map<int32_t, recomp_func_t*> func_map{};

void load_overlay(size_t section_table_index, int32_t ram) {
    const SectionTableEntry& section = sections_info.code_sections[section_table_index];

    for (size_t function_index = 0; function_index < section.num_funcs; function_index++) {
        const FuncEntry& func = section.funcs[function_index];
        func_map[ram + func.offset] = func.func;
    }

    loaded_sections.emplace_back(ram, section_table_index);
    section_addresses[section.index] = ram;
}

void load_special_overlay(const SectionTableEntry& section, int32_t ram) {
    for (size_t function_index = 0; function_index < section.num_funcs; function_index++) {
        const FuncEntry& func = section.funcs[function_index];
        func_map[ram + func.offset] = func.func;
    }
}


extern "C" {
int32_t* section_addresses = nullptr;
}

extern "C" void load_overlays(uint32_t rom, int32_t ram_addr, uint32_t size) {
    // Search for the first section that's included in the loaded rom range
    // Sections were sorted by `init_overlays` so we can use the bounds functions
    auto lower = std::lower_bound(&sections_info.code_sections[0], &sections_info.code_sections[sections_info.num_code_sections], rom,
        [](const SectionTableEntry& entry, uint32_t addr) {
            return entry.rom_addr < addr;
        }
    );
    auto upper = std::upper_bound(&sections_info.code_sections[0], &sections_info.code_sections[sections_info.num_code_sections], (uint32_t)(rom + size),
        [](uint32_t addr, const SectionTableEntry& entry) {
            return addr < entry.size + entry.rom_addr;
        }
    );
    // Load the overlays that were found
    for (auto it = lower; it != upper; ++it) {
        load_overlay(std::distance(&sections_info.code_sections[0], it), it->rom_addr - rom + ram_addr);
    }
}

extern "C" void unload_overlay_by_id(uint32_t id) {
    uint32_t section_table_index = overlays_info.table[id];
    const SectionTableEntry& section = sections_info.code_sections[section_table_index];

    auto find_it = std::find_if(loaded_sections.begin(), loaded_sections.end(), [section_table_index](const LoadedSection& s) { return s.section_table_index == section_table_index; });

    if (find_it != loaded_sections.end()) {
        // Determine where each function was loaded to and remove that entry from the function map
        for (size_t func_index = 0; func_index < section.num_funcs; func_index++) {
            const auto& func = section.funcs[func_index];
            uint32_t func_address = func.offset + find_it->loaded_ram_addr;
            func_map.erase(func_address);
        }
        // Reset the section's address in the address table
        section_addresses[section.index] = section.ram_addr;
        // Remove the section from the loaded section map
        loaded_sections.erase(find_it);
    }
}

extern "C" void load_overlay_by_id(uint32_t id, uint32_t ram_addr) {
    uint32_t section_table_index = overlays_info.table[id];
    const SectionTableEntry& section = sections_info.code_sections[section_table_index];
    int32_t prev_address = section_addresses[section.index];
    if (/*ram_addr >= 0x80000000 && ram_addr < 0x81000000) {*/ prev_address == section.ram_addr) {
        load_overlay(section_table_index, ram_addr);
    }
    else {
        int32_t new_address = prev_address + ram_addr;
        unload_overlay_by_id(id);
        load_overlay(section_table_index, new_address);
    }
}

extern "C" void unload_overlays(int32_t ram_addr, uint32_t size) {
    for (auto it = loaded_sections.begin(); it != loaded_sections.end();) {
        const auto& section = sections_info.code_sections[it->section_table_index];

        // Check if the unloaded region overlaps with the loaded section
        if (ram_addr < (it->loaded_ram_addr + section.size) && (ram_addr + size) >= it->loaded_ram_addr) {
            // Check if the section isn't entirely in the loaded region
            if (ram_addr > it->loaded_ram_addr || (ram_addr + size) < (it->loaded_ram_addr + section.size)) {
                fprintf(stderr,
                    "Cannot partially unload section\n"
                    "  rom: 0x%08X size: 0x%08X loaded_addr: 0x%08X\n"
                    "  unloaded_ram: 0x%08X unloaded_size : 0x%08X\n",
                        section.rom_addr, section.size, it->loaded_ram_addr, ram_addr, size);
                assert(false);
                std::exit(EXIT_FAILURE);
            }
            // Determine where each function was loaded to and remove that entry from the function map
            for (size_t func_index = 0; func_index < section.num_funcs; func_index++) {
                const auto& func = section.funcs[func_index];
                uint32_t func_address = func.offset + it->loaded_ram_addr;
                func_map.erase(func_address);
            }
            // Reset the section's address in the address table
            section_addresses[section.index] = section.ram_addr;
            // Remove the section from the loaded section map
            it = loaded_sections.erase(it);
            // Skip incrementing the iterator
            continue;
        }
        ++it;
    }
}

void init_overlays() {
    section_addresses = (int32_t *)malloc(sections_info.total_num_sections * sizeof(int32_t));

    for (size_t section_index = 0; section_index < sections_info.total_num_sections; section_index++) {
        section_addresses[sections_info.code_sections[section_index].index] = sections_info.code_sections[section_index].ram_addr;
    }

    // Sort the executable sections by rom address
    std::sort(&sections_info.code_sections[0], &sections_info.code_sections[sections_info.num_code_sections],
        [](const SectionTableEntry& a, const SectionTableEntry& b) {
            return a.rom_addr < b.rom_addr;
        }
    );

    recomp::load_patch_functions();
}

extern "C" recomp_func_t * get_function(int32_t addr) {
    auto func_find = func_map.find(addr);
    if (func_find == func_map.end()) {
        fprintf(stderr, "Failed to find function at 0x%08X\n", addr);
        assert(false);
        std::exit(EXIT_FAILURE);
    }
    return func_find->second;
}

