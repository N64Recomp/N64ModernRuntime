#ifndef __RECOMP_CALLBACK_REGISTRY_H__
#define __RECOMP_CALLBACK_REGISTRY_H__

#include <string>
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <map>
#include "json/json.hpp"
#include "ConfigStore.hpp"

namespace recomp::config {
    using recomp_callback_internal = std::function<void(
        const std::string& key,
        config_store_value& value
    )>;

    using recomp_callback_external = std::function<void(
        const char *key,
        const void *value,
        ConfigStoreValueType val_type
    )>; 

    using recomp_callback = std::variant<recomp_callback_internal, recomp_callback_external>; 

    using callback_registry_map = std::unordered_map<std::string, recomp_callback>;

    extern callback_registry_map callback_registry;

    void register_callback(const std::string& config_group, const std::string& key, recomp_callback& callback);
}
#endif
