#ifndef __RECOMP_CONFIG_HPP__
#define __RECOMP_CONFIG_HPP__

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <variant>
#include <functional>

#include <json/json.hpp>

#include "recomp.h"

namespace recomp {
    namespace config {

        bool check_config_option_bool_string(const std::string& str);

        enum class ConfigOptionType {
            None,
            Enum,
            Number,
            String,
            Bool
        };

        struct ConfigOptionEnumOption {
            uint32_t value;
            std::string key;
            std::string name;

            template <typename ENUM_TYPE = uint32_t>
            ConfigOptionEnumOption(ENUM_TYPE value, std::string key, std::string name)
                : value(static_cast<uint32_t>(value)), key(key), name(name) {}

            template <typename ENUM_TYPE = uint32_t>
            ConfigOptionEnumOption(ENUM_TYPE value, std::string key)
                : value(static_cast<uint32_t>(value)), key(key), name(key) {}
        };

        struct ConfigOptionEnum {
            std::vector<ConfigOptionEnumOption> options;
            uint32_t default_value = 0;

            // Case insensitive search for an option based on a key string. (Matches against options[n].key)
            std::vector<ConfigOptionEnumOption>::const_iterator find_option_from_string(const std::string& option_key) const;
            // Search for an option that has a specific value. (Matches against options[n].value)
            std::vector<ConfigOptionEnumOption>::const_iterator find_option_from_value(uint32_t value) const;
            // Verify an option has a unique key and a unique value
            bool can_add_option(const std::string& option_key, uint32_t option_value) const;

            ConfigOptionEnum() = default;
            ConfigOptionEnum(const std::vector<ConfigOptionEnumOption>& options, uint32_t default_value = 0)
                : options(options), default_value(default_value) {}
            template <typename ENUM_TYPE = uint32_t>
            ConfigOptionEnum(const std::vector<ConfigOptionEnumOption>& options, ENUM_TYPE default_value = 0)
                : options(options), default_value(static_cast<uint32_t>(default_value)) {}
        };

        struct ConfigOptionNumber {
            double min = 0.0;
            double max = 0.0;
            double step = 0.0;
            int precision = 0;
            bool percent = false;
            double default_value = 0.0;

            static ConfigOptionNumber create_percent_option(double default_value = 0.0) {
                return ConfigOptionNumber{
                    .min = 0.0,
                    .max = 100.0,
                    .step = 1.0,
                    .precision = 0,
                    .percent = true,
                    .default_value = default_value
                };
            }
        };

        struct ConfigOptionString {
            std::string default_value;
        };

        struct ConfigOptionBool {
            bool default_value;
        };

        typedef std::variant<ConfigOptionEnum, ConfigOptionNumber, ConfigOptionString, ConfigOptionBool> ConfigOptionVariant;

        struct ConfigOption {
            std::string id;
            std::string name;
            std::string description;
            bool hidden = false;
            ConfigOptionType type;
            ConfigOptionVariant variant;
        };

        typedef std::variant<std::monostate, uint32_t, double, std::string, bool> ConfigValueVariant;

        // Manages value dependencies between config options (e.g. option is hidden or disabled from other option being a certain value) .
        class ConfigOptionDependency {
        private:
            // Maps options to the options that are affected by their values
            std::unordered_map<size_t, std::unordered_set<size_t>> option_to_dependencies = {};
            // Maps dependent options to the values that the source option can be
            std::unordered_map<size_t, std::vector<ConfigValueVariant>> dependency_to_values = {};
        public:
            ConfigOptionDependency() = default;

            // Add dependency. When <source_option> is one of the <values>, <dependent_option> is affected.
            void add_option_dependency(size_t dependent_option_index, size_t source_option_index, std::vector<ConfigValueVariant> &values);

            // Check which dependent options are affected by the value of the source option.
            // Returns a map of dependent options and if they are a match
            std::unordered_map<size_t, bool> check_option_dependencies(size_t source_option_index, ConfigValueVariant value);
        };

        struct ConfigSchema {
            std::vector<ConfigOption> options;
            std::unordered_map<std::string, size_t> options_by_id;
            ConfigOptionDependency disable_dependencies;
            ConfigOptionDependency hidden_dependencies;
        };

        struct ConfigStorage {
            std::unordered_map<std::string, ConfigValueVariant> value_map;
        };

        enum class ConfigOptionUpdateType {
            Disabled,
            Hidden,
            EnumDetails,
            EnumDisabled,
            Value,
            Description
        };

        struct ConfigOptionUpdateContext {
            size_t option_index;
            std::vector<ConfigOptionUpdateType> updates = {};
        };

        enum class OptionChangeContext {
            Load,
            Temporary,
            Permanent
        };

        using on_option_change_callback = std::function<void(
            ConfigValueVariant cur_value,
            ConfigValueVariant prev_value,
            OptionChangeContext change_context
        )>;

        using parse_option_func = std::function<ConfigValueVariant(const nlohmann::json&)>;
        using serialize_option_func = std::function<nlohmann::json(const ConfigValueVariant&)>;

        class Config {
        public:
            std::string name;
            // id is used for the file name (e.g. general.json) and storing keys
            std::string id;
            // If true, any configuration changes are temporarily stored until Apply is pressed.
            // Changing the tab will prompt the user to either apply or cancel changes.
            bool requires_confirmation = false;
            
            std::unordered_set<size_t> modified_options = {};

            // For base game configs
            Config(std::string name, std::string id, bool requires_confirmation = false);
            // For mod configs
            Config();

            void set_id(const std::string &id);
            void set_mod_version(const std::string &mod_version);

            void add_option(const ConfigOption& option);

            void add_enum_option(
                const std::string &id,
                const std::string &name,
                const std::string &description,
                const std::vector<ConfigOptionEnumOption> &options,
                uint32_t default_value,
                bool hidden = false
            );

            // Allows you to add an enum option using an enum type instead of uint32_t.
            template <typename ENUM_TYPE>
            void add_enum_option(
                const std::string &id,
                const std::string &name,
                const std::string &description,
                const std::vector<ConfigOptionEnumOption> &options,
                ENUM_TYPE default_value,
                bool hidden = false
            ) {
                add_enum_option(id, name, description, options, static_cast<uint32_t>(default_value), hidden);
            };

            void add_number_option(
                const std::string &id,
                const std::string &name,
                const std::string &description,
                double min = 0,
                double max = 0,
                double step = 1,
                int precision = 0,
                bool percent = false,
                double default_value = 0,
                bool hidden = false
            );

            // Convenience function for adding a percent number option
            void add_percent_number_option(
                const std::string &id,
                const std::string &name,
                const std::string &description,
                double default_value,
                bool hidden = false
            );

            void add_string_option(
                const std::string &id,
                const std::string &name,
                const std::string &description,
                const std::string &default_value,
                bool hidden = false
            );

            void add_bool_option(
                const std::string &id,
                const std::string &name,
                const std::string &description,
                bool default_value = false,
                bool hidden = false
            );

            const ConfigOption &get_option(size_t option_index) const;
            const ConfigOption &get_option(const std::string& option_id) const;
            template <typename T = ConfigOptionVariant>
            const T &get_option_config(size_t option_index) const {
                return std::get<T>(get_option(option_index).variant);
            };

            const ConfigValueVariant get_option_value(const std::string& option_id) const;
            const ConfigValueVariant get_temp_option_value(const std::string& option_id) const;
            // This should only be used internally to recompui. Other changes to values should be done through update_option_value
            // so rendering can be updated with your new set value.
            void set_option_value(const std::string& option_id, ConfigValueVariant value);
            bool get_enum_option_disabled(size_t option_index, uint32_t enum_index) const;
            void add_option_change_callback(const std::string& option_id, on_option_change_callback callback);
            void set_load_callback(std::function<void()> callback) {
                load_callback = callback;
            }
            void set_save_callback(std::function<void()> callback) {
                save_callback = callback;
            }

            void report_config_option_update(size_t option_index, ConfigOptionUpdateType update_type);
            void update_option_disabled(size_t option_index, bool disabled);
            void update_option_disabled(const std::string& option_id, bool disabled);
            void update_option_hidden(size_t option_index, bool hidden);
            void update_option_hidden(const std::string& option_id, bool hidden);
            void update_option_enum_details(const std::string& option_id, const std::string& enum_details);
            void update_option_value(const std::string& option_id, ConfigValueVariant value);
            void update_option_description(const std::string& option_id, const std::string& new_description);
            void update_enum_option_disabled(const std::string& option_id, uint32_t enum_index, bool disabled);

            // Makes the dependent option disabled when the source option is set to any of the specified values.
            void add_option_disable_dependency(const std::string& dependent_option_id, const std::string& source_option_id, std::vector<ConfigValueVariant> &values);
            template <typename... ENUM_TYPE>
            void add_option_disable_dependency(const std::string& dependent_option_id, const std::string& source_option_id, ENUM_TYPE... enum_values) {
                std::vector<ConfigValueVariant> values;
                for (const auto& value : {enum_values...}) {
                    values.push_back(static_cast<uint32_t>(value));
                }
                add_option_disable_dependency(dependent_option_id, source_option_id, values);
            };
            // Makes the dependent option hidden when the source option is set to any of the specified values.
            // Does not override the option's inherent hidden property if set.
            void add_option_hidden_dependency(const std::string& dependent_option_id, const std::string& source_option_id, std::vector<ConfigValueVariant> &values);
            template <typename... ENUM_TYPE>
            void add_option_hidden_dependency(const std::string& dependent_option_id, const std::string& source_option_id, ENUM_TYPE... enum_values) {
                std::vector<ConfigValueVariant> values;
                for (const auto& value : {enum_values...}) {
                    values.push_back(static_cast<uint32_t>(value));
                }
                add_option_hidden_dependency(dependent_option_id, source_option_id, values);
            };
            void add_option_hidden_dependency(const std::string& dependent_option_id, const std::string& source_option_id, bool bool_val) {
                std::vector<ConfigValueVariant> values = { bool_val };
                add_option_hidden_dependency(dependent_option_id, source_option_id, values);
            };

            bool load_config(std::function<bool(nlohmann::json &)> validate_callback = nullptr);
            bool save_config();
            bool save_config_json(nlohmann::json config_json) const;
            nlohmann::json get_json_config() const;

            void revert_temp_config();

            bool is_dirty() const;

            std::vector<ConfigOptionUpdateContext> get_config_option_updates() { return config_option_updates; }
            bool is_config_option_disabled(size_t option_index) const { return disabled_options.contains(option_index); }
            bool is_config_option_hidden(size_t option_index) const;
            void clear_config_option_updates() {
                config_option_updates.clear();
            }
            std::string get_enum_option_details(size_t option_index) const;
            void on_json_parse_option(const std::string& option_id, parse_option_func callback) {
                json_parse_option_map[option_id] = callback;
            }
            void on_json_serialize_option(const std::string& option_id, serialize_option_func callback) {
                json_serialize_option_map[option_id] = callback;
            }

            const ConfigStorage& get_config_storage() const;
            const ConfigSchema& get_config_schema() const;

        private:
            bool loaded_config = false;
            bool is_mod_config = false;

            std::string config_file_name;
            std::string mod_version; // only used if mod

            ConfigSchema schema;
            ConfigStorage storage;
            ConfigStorage temp_storage;

            std::unordered_map<size_t, on_option_change_callback> option_change_callbacks = {};
            std::function<void()> load_callback = nullptr;
            std::function<void()> save_callback = nullptr;
            std::vector<ConfigOptionUpdateContext> config_option_updates = {};
            std::unordered_set<size_t> disabled_options = {};
            std::unordered_set<size_t> hidden_options = {};
            std::unordered_map<size_t, std::string> enum_option_details = {};
            std::unordered_map<size_t, std::unordered_set<size_t>> enum_options_disabled = {};

            std::unordered_map<std::string, parse_option_func> json_parse_option_map = {};
            std::unordered_map<std::string, serialize_option_func> json_serialize_option_map = {};

            const ConfigValueVariant get_option_value_from_storage(const std::string& option_id, const ConfigStorage& src) const;

            void derive_all_config_option_dependencies();
            void derive_option_dependencies(size_t option_index);
            void try_call_option_change_callback(const std::string& option_id, ConfigValueVariant value, ConfigValueVariant prev_value, OptionChangeContext change_context);
            const ConfigValueVariant get_option_default_value(const std::string& option_id) const;
            void determine_changed_option(const std::string& option_id);
            ConfigValueVariant parse_config_option_json_value(const nlohmann::json& json_value, const ConfigOption &option);

            // Return pointer to the root of where the config values should be stored in the json.
            nlohmann::json *get_config_storage_root(nlohmann::json* json);
            nlohmann::json get_storage_json() const;
        };
    }
}

#endif // __RECOMP_CONFIG_HPP__
