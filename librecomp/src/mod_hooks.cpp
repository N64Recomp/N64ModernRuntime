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

void recomp::mods::run_hook(uint8_t* rdram, recomp_context* ctx, size_t hook_slot_index) {
    // Sanity check the hook slot index.
    if (hook_slot_index >= hook_table.size()) {
        printf("Hook slot %zu triggered, but only %zu hook slots have been registered!\n", hook_slot_index, hook_table.size());
        assert(false);
        ultramodern::error_handling::message_box("Encountered an error with loaded mods: hook slot out of bounds");
        ULTRAMODERN_QUICK_EXIT();
    }

    // Copy the initial context state to restore it after running each callback.
    recomp_context initial_context = *ctx;

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
        *ctx = initial_context;
    }
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
