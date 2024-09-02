#include "CallbackRegistry.hpp"

namespace recomp::config {

callback_registry_map callback_registry = {};

static recomp_callback_internal nullcb;

void register_callback(const std::string& config_group, const std::string& key, recomp_callback& callback) {
    callback_registry[config_group + "/" + key] = callback;
};

const void *get_pointer_from_conf_value(config_store_value& val) {
    if (std::holds_alternative<std::string>(val)) {
        return &std::get<std::string>(val);
    }
    if (std::holds_alternative<int>(val)) {
        return &std::get<int>(val);
    }
    return nullptr;
}

void invoke_callback(const std::string& key) {
    auto find_it = callback_registry.find(key);
    if (find_it == callback_registry.end()) {
        printf("ERROR: Could not locate callback at '%s'.\n", key);
        return;
    }

    auto& cb = callback_registry.at(key);

    config_store_value& val = get_config_store_value<config_store_value>(key);

    if (const auto& func = *std::get_if<recomp_callback_internal>(&cb)) {
        func(key, val);
    } else if (const auto& func = *std::get_if<recomp_callback_external>(&cb)) {
        func(
            key.c_str(),
            get_pointer_from_conf_value(val),
            (ConfigStoreValueType)val.index()
        );
    }
};

}
