#include "config_store.hpp"

namespace recomp {

ConfigStore config_store = {{}, {}};

void set_config_store_value(std::string key, config_store_value value) {
    std::lock_guard lock{ config_store.store_mutex };
    config_store.map[key] = value;
}

void set_config_store_default_value(std::string key, config_store_value value) {
    std::lock_guard lock{ config_store.default_store_mutex };
    config_store.default_map[key] = value;
}

void set_config_store_value_and_default(std::string key, config_store_value value, config_store_value default_value) {
    set_config_store_value(key, value);
    set_config_store_default_value(key, default_value);
}

}
