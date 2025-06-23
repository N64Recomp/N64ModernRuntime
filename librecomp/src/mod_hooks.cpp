#include <vector>
#include "librecomp/mods.hpp"
#include "librecomp/overlays.hpp"
#include "ultramodern/error_handling.hpp"

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

struct HookEntry {
    size_t mod_index;
    recomp::mods::GenericFunction func;
};

struct HookTableEntry {
    std::vector<HookEntry> hooks;
    bool is_return_hook;
};

// Vector of individual hooks for each hook slot.
std::vector<HookTableEntry> hook_table{};

// Holds the recomp context to restore after running each hook. This is a vector because a hook may end up calling another hooked function,
// so this acts as a stack of contexts to handle that recursion.
thread_local std::vector<recomp_context> hook_contexts = { recomp_context{} };

void recomp::mods::run_hook(uint8_t* rdram, recomp_context* ctx, size_t hook_slot_index) {
    // Sanity check the hook slot index.
    if (hook_slot_index >= hook_table.size()) {
        printf("Hook slot %zu triggered, but only %zu hook slots have been registered!\n", hook_slot_index, hook_table.size());
        assert(false);
        ultramodern::error_handling::message_box("Encountered an error with loaded mods: hook slot out of bounds");
        ULTRAMODERN_QUICK_EXIT();
    }

    // Copy the initial context state to restore it after running each callback.
    hook_contexts.emplace_back(*ctx);

    // Call every hook attached to the hook slot.
    const std::vector<HookEntry>& hooks = hook_table[hook_slot_index].hooks;
    for (HookEntry hook : hooks) {
        // Run the hook.
        std::visit(overloaded {
            [rdram, ctx](recomp_func_t* native_func) {
                native_func(rdram, ctx);
            },
        }, hook.func);

        // Restore the original context.
        *ctx = hook_contexts.back();
    }

    // Pop the context after the hook is done.
    hook_contexts.pop_back();
}

void recomp::mods::setup_hooks(size_t num_hook_slots) {
    hook_table.resize(num_hook_slots);
}

void recomp::mods::set_hook_type(size_t hook_slot_index, bool is_return) {
    hook_table[hook_slot_index].is_return_hook = is_return;
}

void recomp::mods::register_hook(size_t hook_slot_index, size_t mod_index, GenericFunction callback) {
    hook_table[hook_slot_index].hooks.emplace_back(HookEntry{ mod_index, callback });
}

void recomp::mods::finish_hook_setup(const ModContext& context) {
    // Sort hooks by mod order (and return hooks in reverse order).
    for (HookTableEntry& cur_entry : hook_table) {
        // Reverse sort if this slot is a return hook.
        if (cur_entry.is_return_hook) {
            std::sort(cur_entry.hooks.begin(), cur_entry.hooks.end(), 
                [&context](const HookEntry& lhs, const HookEntry& rhs) {
                    return context.get_mod_order_index(lhs.mod_index) > context.get_mod_order_index(rhs.mod_index);
                }
            );
        }
        // Otherwise sort normally.
        else {
            std::sort(cur_entry.hooks.begin(), cur_entry.hooks.end(), 
                [&context](const HookEntry& lhs, const HookEntry& rhs) {
                    return context.get_mod_order_index(lhs.mod_index) < context.get_mod_order_index(rhs.mod_index);
                }
            );
        }
    }
}

void recomp::mods::reset_hooks() {
    hook_table.clear();
}

void recomphook_get_return_s32(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = (gpr)(int32_t)hook_contexts.back().r2;
}

void recomphook_get_return_u32(uint8_t* rdram, recomp_context* ctx) {
    recomphook_get_return_s32(rdram, ctx);
}

void recomphook_get_return_ptr(uint8_t* rdram, recomp_context* ctx) {
    recomphook_get_return_s32(rdram, ctx);
}

void recomphook_get_return_s16(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = (gpr)(int16_t)hook_contexts.back().r2;
}

void recomphook_get_return_u16(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = (gpr)(uint16_t)hook_contexts.back().r2;
}

void recomphook_get_return_s8(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = (gpr)(int8_t)hook_contexts.back().r2;
}

void recomphook_get_return_u8(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = (gpr)(uint8_t)hook_contexts.back().r2;
}

void recomphook_get_return_s64(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = (gpr)(int32_t)hook_contexts.back().r2;
    ctx->r3 = (gpr)(int32_t)hook_contexts.back().r3;
}

void recomphook_get_return_u64(uint8_t* rdram, recomp_context* ctx) {
    recomphook_get_return_s64(rdram, ctx);
}

void recomphook_get_return_float(uint8_t* rdram, recomp_context* ctx) {
    ctx->f0.fl = hook_contexts.back().f0.fl;
}

void recomphook_get_return_double(uint8_t* rdram, recomp_context* ctx) {
    ctx->f0.fl = (gpr)(uint8_t)hook_contexts.back().f0.fl;
    ctx->f1.fl = (gpr)(uint8_t)hook_contexts.back().f1.fl;
}

#define REGISTER_FUNC(name) recomp::overlays::register_base_export(#name, name)

void recomp::mods::register_hook_exports() {
    REGISTER_FUNC(recomphook_get_return_s32);
    REGISTER_FUNC(recomphook_get_return_u32);
    REGISTER_FUNC(recomphook_get_return_ptr);
    REGISTER_FUNC(recomphook_get_return_s16);
    REGISTER_FUNC(recomphook_get_return_u16);
    REGISTER_FUNC(recomphook_get_return_s8);
    REGISTER_FUNC(recomphook_get_return_u8);
    REGISTER_FUNC(recomphook_get_return_s64);
    REGISTER_FUNC(recomphook_get_return_u64);
    REGISTER_FUNC(recomphook_get_return_float);
    REGISTER_FUNC(recomphook_get_return_double);
}
