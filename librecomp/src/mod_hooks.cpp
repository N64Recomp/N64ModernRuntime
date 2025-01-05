#include <vector>
#include "librecomp/mods.hpp"
#include "librecomp/overlays.hpp"
#include "ultramodern/error_handling.hpp"

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// Vector of individual hooks for each hook slot.
std::vector<std::vector<recomp::mods::GenericFunction>> hook_table{};

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
    const std::vector<recomp::mods::GenericFunction>& hooks = hook_table[hook_slot_index];
    for (recomp::mods::GenericFunction func : hooks) {
        // Run the hook.
        std::visit(overloaded {
            [rdram, ctx](recomp_func_t* native_func) {
                native_func(rdram, ctx);
            },
        }, func);

        // Restore the original context.
        *ctx = initial_context;
    }
}

void recomp::mods::setup_hooks(size_t num_hook_slots) {
    hook_table.resize(num_hook_slots);
}

void recomp::mods::register_hook(size_t hook_slot_index, GenericFunction callback) {
    hook_table[hook_slot_index].emplace_back(callback);
}

void recomp::mods::reset_hooks() {
    hook_table.clear();
}
