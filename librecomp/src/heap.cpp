#include "o1heap/o1heap.h"

#include "librecomp/addresses.hpp"
#include "librecomp/overlays.hpp"
#include "librecomp/helpers.hpp"

static uint32_t heap_offset;

static inline O1HeapInstance* get_heap(uint8_t* rdram) {
    return reinterpret_cast<O1HeapInstance*>(&rdram[heap_offset]);
}

extern "C" void recomp_alloc(uint8_t* rdram, recomp_context* ctx) {
    uint32_t offset = reinterpret_cast<uint8_t*>(recomp::alloc(rdram, ctx->r4)) - rdram;
    ctx->r2 = offset + 0xFFFFFFFF80000000ULL;
}

extern "C" void recomp_free(uint8_t* rdram, recomp_context* ctx) {
    PTR(void) to_free = _arg<0, PTR(void)>(rdram, ctx);
    // Handle NULL frees.
    if (to_free == NULLPTR) {
        return;
    }
    recomp::free(rdram, TO_PTR(void, to_free));
}

void recomp::register_heap_exports() {
    recomp::overlays::register_base_export("recomp_alloc", recomp_alloc);
    recomp::overlays::register_base_export("recomp_free", recomp_free);
}

void recomp::init_heap(uint8_t* rdram, uint32_t address) {
    // Align the heap start to 16 bytes.
    address = (address + 15U) & ~15U;

    // Calculate the offset of the heap from the start of rdram and the size of the heap.
    heap_offset = address - 0x80000000U;
    size_t heap_size = recomp::mem_size - heap_offset;

    printf("Initializing recomp heap at offset 0x%08X with size 0x%08X\n", heap_offset, static_cast<uint32_t>(heap_size));

    o1heapInit(&rdram[heap_offset], heap_size);
}

void* recomp::alloc(uint8_t* rdram, size_t size) {
    return o1heapAllocate(get_heap(rdram), size);
}

void recomp::free(uint8_t* rdram, void* mem) {
    o1heapFree(get_heap(rdram), mem);
}
