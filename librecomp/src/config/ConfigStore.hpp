#ifndef __RECOMP_CONFIG_STORE_H__
#define __RECOMP_CONFIG_STORE_H__

#include <stdlib.h>
#include <unordered_map>
#include <string>
#include <variant>
#include <mutex>

// Use a custom hash class to enable hetereogenous lookup 
struct string_hash {
    using is_transparent = void;
    [[nodiscard]] size_t operator()(const char *txt) const {
        return std::hash<std::string_view>{}(txt);
    }
    [[nodiscard]] size_t operator()(std::string_view txt) const {
        return std::hash<std::string_view>{}(txt);
    }
    [[nodiscard]] size_t operator()(const std::string &txt) const {
        return std::hash<std::string>{}(txt);
    }
};

namespace recomp::config {    
    typedef std::variant<std::string, int> config_store_value;
    typedef std::unordered_map<std::string, config_store_value, string_hash, std::equal_to<>> config_store_map;

    struct ConfigStore {
        config_store_map map;
        config_store_map default_map;
        std::mutex store_mutex;
        std::mutex default_store_mutex;
    };

    extern ConfigStore config_store;

    void set_config_store_value(std::string_view key, config_store_value value);
    void set_config_store_default_value(std::string_view key, config_store_value value);
    void set_config_store_value_and_default(std::string_view key, config_store_value value, config_store_value default_value);

    template<typename T>
    T get_config_store_default_value(std::string_view key) {
        std::lock_guard lock{ config_store.default_store_mutex };
        auto it = config_store.default_map.find(key);
        if (it != config_store.default_map.end()) {
            if (std::holds_alternative<T>(it->second)) {
                return std::get<T>(it->second);
            } else {
                throw std::runtime_error("Stored value is not of requested type");
            }
        } else {
            throw std::runtime_error("Key not found");
        }
    };

    // Get a value from the config store, if it doesn't exist then return the default value
    template<typename T>
    T get_config_store_value(std::string_view key) {
        std::lock_guard lock{ config_store.store_mutex };
        auto it = config_store.map.find(key);
        if (it != config_store.map.end()) {
            if (std::holds_alternative<T>(it->second)) {
                return std::get<T>(it->second);
            } else {
                throw std::runtime_error("Stored value is not of requested type");
            }
        } else {
            return get_config_store_default_value<T>(key);
        }
    };
};

#endif
