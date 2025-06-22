#include <vector>
#include "librecomp/mods.hpp"
#include "librecomp/overlays.hpp"
#include "ultramodern/error_handling.hpp"

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

struct EventCallback {
    size_t mod_index;
    recomp::mods::GenericFunction func;
};

// Vector of callbacks for each registered event.
std::vector<std::vector<EventCallback>> event_callbacks{};

extern "C" {
    // This can stay at 0 since the base events are always first in the list.
    uint32_t builtin_base_event_index = 0;
}

extern "C" void recomp_trigger_event(uint8_t* rdram, recomp_context* ctx, uint32_t event_index) {
    // Sanity check the event index.
    if (event_index >= event_callbacks.size()) {
        printf("Event %u triggered, but only %zu events have been registered!\n", event_index, event_callbacks.size());
        assert(false);
        ultramodern::error_handling::message_box("Encountered an error with loaded mods: event index out of bounds");
        ULTRAMODERN_QUICK_EXIT();
    }

    // Copy the initial context state to restore it after running each callback.
    recomp_context initial_context = *ctx;

    // Call every callback attached to the event.
    const std::vector<EventCallback>& callbacks = event_callbacks[event_index];
    for (const EventCallback& callback : callbacks) {
        // Run the callback.
        std::visit(overloaded {
            [rdram, ctx](recomp_func_t* native_func) {
                native_func(rdram, ctx);
            },
        }, callback.func);

        // Restore the original context.
        *ctx = initial_context;
    }
}

void recomp::mods::setup_events(size_t num_events) {
    event_callbacks.resize(num_events);
}

void recomp::mods::register_event_callback(size_t event_index, size_t mod_index, GenericFunction callback) {
    event_callbacks[event_index].emplace_back(EventCallback{ mod_index, callback });
}

void recomp::mods::finish_event_setup(const ModContext& context) {
    // Sort callbacks by mod order.
    for (std::vector<EventCallback>& cur_entry : event_callbacks) {
        std::sort(cur_entry.begin(), cur_entry.end(), 
            [&context](const EventCallback& lhs, const EventCallback& rhs) {
                return context.get_mod_order_index(lhs.mod_index) < context.get_mod_order_index(rhs.mod_index);
            }
        );
    }
}

void recomp::mods::reset_events() {
    event_callbacks.clear();
}
