#include <fstream>
#include "librecomp/files.hpp"
#include "librecomp/config.hpp"
#include "librecomp/game.hpp"
#include "librecomp/mods.hpp"

static bool read_json(std::ifstream input_file, nlohmann::json& json_out) {
    if (!input_file.good()) {
        return false;
    }

    try {
        input_file >> json_out;
    }
    catch (nlohmann::json::parse_error&) {
        return false;
    }
    return true;
}

static bool read_json_with_backups(const std::filesystem::path& path, nlohmann::json& json_out) {
    // Try reading and parsing the base file.
    if (read_json(std::ifstream{path}, json_out)) {
        return true;
    }

    // Try reading and parsing the backup file.
    if (read_json(recomp::open_input_backup_file(path), json_out)) {
        return true;
    }

    // Both reads failed.
    return false;
}

static bool save_json_with_backups(const std::filesystem::path& path, const nlohmann::json& json_data) {
    {
        std::ofstream output_file = recomp::open_output_file_with_backup(path);
        if (!output_file.good()) {
            return false;
        }

        output_file << std::setw(4) << json_data;
    }
    return recomp::finalize_output_file_with_backup(path);
}

static std::filesystem::path get_path_to_config(bool is_mod_config) {
    if (is_mod_config) {
        return recomp::get_config_path() / recomp::mods::mod_config_directory;
    }
    return recomp::get_config_path();
}

namespace recomp::config {
Config::Config(std::string name, std::string id, bool requires_confirmation) {
    this->name = name;
    this->id = id;
    this->requires_confirmation = requires_confirmation;
    schema.options.clear();
    schema.options_by_id.clear();
    storage.value_map.clear();
    temp_storage.value_map.clear();

    config_file_name = this->id + ".json";
}

Config::Config() {
    is_mod_config = true;
    requires_confirmation = false;
    name = "Mod Config";

    schema.options.clear();
    schema.options_by_id.clear();
    storage.value_map.clear();
    temp_storage.value_map.clear();
}

void Config::set_id(const std::string &id) {
    this->id = id;
    config_file_name = this->id + ".json";
}
void Config::set_mod_version(const std::string &mod_version) {
    this->mod_version = mod_version;
}

const ConfigStorage& Config::get_config_storage() const {
    return storage;
}

const ConfigSchema& Config::get_config_schema() const {
    return schema;
}

nlohmann::json *Config::get_config_storage_root(nlohmann::json* json) {
    if (is_mod_config) {
        return &(*json)["storage"];
    }
    return json;
}

void Config::add_option(const ConfigOption& option) {
    if (loaded_config) {
        assert(false && "Cannot add options after config has been loaded.");
    }
    schema.options.push_back(option);
    schema.options_by_id[option.id] = schema.options.size() - 1;

    ConfigValueVariant default_value = std::monostate();
    switch (option.type) {
        case ConfigOptionType::None:
            assert(false && "Cannot add option with type None.");
            break;
        case ConfigOptionType::Enum:
            default_value = std::get<ConfigOptionEnum>(option.variant).default_value;
            break;
        case ConfigOptionType::Number:
            default_value = std::get<ConfigOptionNumber>(option.variant).default_value;
            break;
        case ConfigOptionType::String:
            default_value = std::get<ConfigOptionString>(option.variant).default_value;
            break;
        case ConfigOptionType::Bool:
            default_value = std::get<ConfigOptionBool>(option.variant).default_value;
            break;
    }

    storage.value_map[option.id] = default_value;
    if (requires_confirmation) {
        temp_storage.value_map[option.id] = default_value;
    }
}

void Config::add_enum_option(
    const std::string &id,
    const std::string &name,
    const std::string &description,
    const std::vector<ConfigOptionEnumOption> &options,
    uint32_t default_value,
    bool hidden
) {
    ConfigOption option;
    option.id = id;
    option.name = name;
    option.description = description;
    option.type = ConfigOptionType::Enum;
    option.hidden = hidden;

    ConfigOptionEnum option_enum = {{}, default_value};

    // Note: this is a bit too predictive since this calls add_option
    size_t option_index = schema.options.size();

    for (const auto &option : options) {
        assert(option_enum.can_add_option(option.key, option.value) && "Duplicate enum option key or value.");
        option_enum.options.push_back(option);
    }

    if (option_enum.find_option_from_value(default_value) == option_enum.options.end()) {
        assert(false && "Default value must match to an option.");
    }

    option.variant = option_enum;

    add_option(option);
}

void Config::add_number_option(
    const std::string &id,
    const std::string &name,
    const std::string &description,
    double min,
    double max,
    double step,
    int precision,
    bool percent,
    double default_value,
    bool hidden
) {
    ConfigOption option;
    option.id = id;
    option.name = name;
    option.description = description;
    option.type = ConfigOptionType::Number;
    option.variant = ConfigOptionNumber{min, max, step, precision, percent, default_value};
    option.hidden = hidden;

    add_option(option);
}

void Config::add_string_option(
    const std::string &id,
    const std::string &name,
    const std::string &description,
    const std::string &default_value,
    bool hidden
) {
    ConfigOption option;
    option.id = id;
    option.name = name;
    option.description = description;
    option.type = ConfigOptionType::String;
    option.variant = ConfigOptionString{default_value};
    option.hidden = hidden;

    add_option(option);
}

void Config::add_bool_option(
    const std::string &id,
    const std::string &name,
    const std::string &description,
    bool default_value,
    bool hidden
) {
    ConfigOption option;
    option.id = id;
    option.name = name;
    option.description = description;
    option.type = ConfigOptionType::Bool;
    option.variant = ConfigOptionBool{default_value};
    option.hidden = hidden;

    add_option(option);
}

const ConfigValueVariant Config::get_option_default_value(const std::string& option_id) const {
    auto option_by_id_it = schema.options_by_id.find(option_id);
    if (option_by_id_it == schema.options_by_id.end()) {
        assert(false && "Option not found.");
        return std::monostate();
    }

    const ConfigOption &option = schema.options[option_by_id_it->second];
    switch (option.type) {
    case ConfigOptionType::Enum:
        return std::get<ConfigOptionEnum>(option.variant).default_value;
    case ConfigOptionType::Number:
        return std::get<ConfigOptionNumber>(option.variant).default_value;
    case ConfigOptionType::String:
        return std::get<ConfigOptionString>(option.variant).default_value;
    case ConfigOptionType::Bool:
        return std::get<ConfigOptionBool>(option.variant).default_value;
    default:
        assert(false && "Unknown config option type.");
        return std::monostate();
    }
}

const ConfigValueVariant Config::get_option_value_from_storage(const std::string& option_id, const ConfigStorage& src) const {
    auto it = src.value_map.find(option_id);
    if (it != src.value_map.end()) {
        return it->second;
    }
    return get_option_default_value(option_id);
}

const ConfigValueVariant Config::get_option_value(const std::string& option_id) const {
    return get_option_value_from_storage(option_id, storage);
}

const ConfigValueVariant Config::get_temp_option_value(const std::string& option_id) const {
    return get_option_value_from_storage(option_id, temp_storage);
}

void Config::determine_changed_option(const std::string& option_id) {
    if (get_option_value(option_id) != get_temp_option_value(option_id)) {
        modified_options.insert(schema.options_by_id[option_id]);
    } else {
        modified_options.erase(schema.options_by_id[option_id]);
    }
}

void Config::try_call_option_change_callback(const std::string& option_id, ConfigValueVariant value, ConfigValueVariant prev_value, OptionChangeContext change_context) {
    size_t option_index = schema.options_by_id[option_id];
    auto callback_it = option_change_callbacks.find(option_index);
    bool is_load = (change_context == OptionChangeContext::Load);
    bool value_changed = (value != prev_value);
    if (callback_it != option_change_callbacks.end() && (is_load || value_changed)) {
        callback_it->second(value, prev_value, change_context);
    }
}

void Config::set_option_value(const std::string& option_id, ConfigValueVariant value) {
    ConfigStorage &storage = requires_confirmation ? temp_storage : storage;

    auto it = storage.value_map.find(option_id);
    if (it != storage.value_map.end()) {
        ConfigValueVariant prev_value = it->second;
        it->second = value;

        if (requires_confirmation) {
            determine_changed_option(option_id);
            try_call_option_change_callback(option_id, value, prev_value, OptionChangeContext::Temporary);
        } else {
            try_call_option_change_callback(option_id, value, prev_value, OptionChangeContext::Permanent);
        }

        derive_option_dependencies(schema.options_by_id[option_id]);
    }
}

bool Config::get_enum_option_disabled(size_t option_index, uint32_t enum_index) {
    auto enum_it = enum_options_disabled.find(option_index);
    if (enum_it != enum_options_disabled.end()) {
        return enum_it->second.contains(enum_index);
    }
    return false;
}

nlohmann::json Config::get_json_config() const {
    if (is_mod_config) {
        nlohmann::json config_json;
        if (id.empty()) {
            assert(false && "Mod ID does not exist for this config.");
        }
        if (mod_version.empty()) {
            assert(false && "Mod version does not exist for this config.");
        }
        config_json["mod_id"] = id;
        config_json["mod_version"] = mod_version;
        config_json["recomp_version"] = recomp::get_project_version().to_string();
        config_json["storage"] = get_storage_json();
        return config_json;
    }
    return get_storage_json();
}

nlohmann::json Config::get_storage_json() const {
    nlohmann::json json;
    for (const auto& option : schema.options) {
        const ConfigValueVariant value = get_option_value(option.id);

        if (json_serialize_option_map.contains(option.id)) {
            auto cb = json_serialize_option_map.at(option.id);
            json[option.id] = cb(value);
            continue;
        }

        switch (option.type) {
        case ConfigOptionType::Enum: {
            auto &option_enum = std::get<ConfigOptionEnum>(option.variant);
            auto found_opt = option_enum.find_option_from_value(std::get<uint32_t>(value));
            if (found_opt != option_enum.options.end()) {
                json[option.id] = found_opt->key;
            }
            break;
        }
        case ConfigOptionType::Number: {
            auto &option_number = std::get<ConfigOptionNumber>(option.variant);
            if (option_number.precision == 0) {
                json[option.id] = static_cast<int>(std::get<double>(value));
            } else {
                json[option.id] = std::get<double>(value);
            }
            break;
        }
        case ConfigOptionType::String:
            json[option.id] = std::get<std::string>(value);
            break;
        case ConfigOptionType::Bool:
            json[option.id] = std::get<bool>(value);
            break;
        }
    }
    return json;
}

bool Config::save_config_json(nlohmann::json config_json) const {
    std::filesystem::path file_path = get_path_to_config(is_mod_config) / config_file_name;

    bool result = save_json_with_backups(file_path, config_json);
    if (save_callback) {
        save_callback();
    }

    return result;
}

bool Config::save_config() {
    if (requires_confirmation) {
        for (const auto& option : schema.options) {
            ConfigValueVariant prev_value = get_option_value(option.id);
            ConfigValueVariant cur_value = get_temp_option_value(option.id);
            storage.value_map[option.id] = cur_value;
            try_call_option_change_callback(option.id, cur_value, prev_value, OptionChangeContext::Permanent);
        }

        if (apply_callback && is_dirty()) {
            apply_callback();
        }

        modified_options.clear();
    }

    return save_config_json(get_json_config());
}

void Config::derive_option_dependencies(size_t option_index) {
    auto &option_id = schema.options[option_index].id;
    auto value = requires_confirmation ? get_temp_option_value(option_id) :  get_option_value(option_id);

    auto disable_result = schema.disable_dependencies.check_option_dependencies(option_index, value);
    for (auto &option_res : disable_result) {
        update_option_disabled(option_res.first, option_res.second);
    }

    auto hidden_result = schema.hidden_dependencies.check_option_dependencies(option_index, value);
    for (auto &option_res : hidden_result) {
        update_option_hidden(option_res.first, option_res.second);
    }
}

void Config::derive_all_config_option_dependencies() {
    for (size_t option_index = 0; option_index < schema.options.size(); option_index++) {
        derive_option_dependencies(option_index);
    }
}

ConfigValueVariant Config::parse_config_option_json_value(const nlohmann::json& json_value, const ConfigOption &option) {
    if (json_parse_option_map.contains(option.id)) {
        return json_parse_option_map[option.id](json_value);
    }

    bool is_null = json_value.is_null();

    switch (option.type) {
        case ConfigOptionType::None:
        default: {
            return {};
        }
        case ConfigOptionType::Enum: {
            if (is_null) {
                return std::get<ConfigOptionEnum>(option.variant).default_value;
            }
            std::string enum_string_value = json_value.get<std::string>();
            auto option_variant = std::get<ConfigOptionEnum>(option.variant);
            auto found_opt = option_variant.find_option_from_string(enum_string_value);
            if (found_opt != option_variant.options.end()) {
                return found_opt->value;
            } else {
                return std::get<ConfigOptionEnum>(option.variant).default_value;
            }
        }
        case ConfigOptionType::Number:
            if (is_null) {
                return std::get<ConfigOptionNumber>(option.variant).default_value;
            }
            return json_value.get<double>();
        case ConfigOptionType::String:
            if (is_null) {
                return std::get<ConfigOptionString>(option.variant).default_value;
            }
            return json_value.get<std::string>();
        case ConfigOptionType::Bool:
            if (is_null) {
                return std::get<ConfigOptionBool>(option.variant).default_value;
                break;
            }
            return json_value.get<bool>();
    }
}

bool Config::load_config(std::function<bool(nlohmann::json &)> validate_callback) {
    std::filesystem::path file_path = get_path_to_config(is_mod_config) / config_file_name;
    nlohmann::json config_json{};

    if (!read_json_with_backups(file_path, config_json)) {
        if (requires_confirmation) {
            revert_temp_config();
        }
        save_config();
        derive_all_config_option_dependencies();
        clear_config_option_updates();
        loaded_config = true;
        return true;
    }

    if (validate_callback != nullptr && !validate_callback(config_json)) {
        return false;
    }

    nlohmann::json *json_config_root = get_config_storage_root(&config_json);

    for (const auto& option : schema.options) {
        auto json_value = (*json_config_root)[option.id];

        auto value = parse_config_option_json_value(json_value, option);
        storage.value_map[option.id] = value;

        if (requires_confirmation) {
            temp_storage.value_map[option.id] = value;
        }
        try_call_option_change_callback(option.id, value, value, OptionChangeContext::Load);
    }

    derive_all_config_option_dependencies();
    clear_config_option_updates();

    loaded_config = true;
    return true;
}

void Config::revert_temp_config() {
    if (!requires_confirmation) {
        return;
    }

    modified_options.clear();

    for (const auto& option : schema.options) {
        temp_storage.value_map[option.id] = get_option_value(option.id);
    }
    derive_all_config_option_dependencies();
}

bool Config::is_dirty() {
    return !modified_options.empty();
}

void Config::add_option_change_callback(const std::string& option_id, on_option_change_callback callback) {
    size_t option_index = schema.options_by_id[option_id];
    option_change_callbacks[option_index] = callback;
}

void Config::report_config_option_update(size_t option_index, ConfigOptionUpdateType update_type) {
    ConfigOptionUpdateContext *update_context = nullptr;
    for (auto &context : config_option_updates) {
        if (context.option_index == option_index) {
            update_context = &context;
            break;
        }
    }

    if (update_context == nullptr) {
        config_option_updates.push_back({option_index, {}});
        update_context = &config_option_updates.back();
    }

    update_context->updates.push_back(update_type);
}

void Config::update_option_disabled(size_t option_index, bool disabled) {
    bool was_disabled = is_config_option_disabled(option_index);
    if (was_disabled == disabled) return;

    if (disabled) {
        disabled_options.insert(option_index);
    } else {
        disabled_options.erase(option_index);
    }
    report_config_option_update(option_index, ConfigOptionUpdateType::Disabled);
};

void Config::update_option_disabled(const std::string& option_id, bool disabled) {
    size_t option_index = schema.options_by_id[option_id];
    update_option_disabled(option_index, disabled);
};

void Config::update_option_hidden(size_t option_index, bool hidden) {
    if (schema.options[option_index].hidden) {
        // unchangeable - always hidden
        return;
    }
    bool was_hidden = is_config_option_hidden(option_index);
    if (was_hidden == hidden) {
        return;
    }
    if (hidden) {
        hidden_options.insert(option_index);
    } else {
        hidden_options.erase(option_index);
    }
    report_config_option_update(option_index, ConfigOptionUpdateType::Hidden);
};

void Config::update_option_hidden(const std::string& option_id, bool hidden) {
    size_t option_index = schema.options_by_id[option_id];
    update_option_hidden(option_index, hidden);
};

void Config::update_option_enum_details(const std::string& option_id, const std::string& enum_details) {
    size_t option_index = schema.options_by_id[option_id];
    enum_option_details[option_index] = enum_details;
    report_config_option_update(option_index, ConfigOptionUpdateType::EnumDetails);
};

void Config::update_option_value(const std::string& option_id, ConfigValueVariant value) {
    size_t option_index = schema.options_by_id[option_id];
    // This could potentially cause an update loop due to set_option_value calling change callbacks, which could call this function.
    // It seems more important to call change callbacks AND respect requires_confirmation
    set_option_value(option_id, value);
    report_config_option_update(option_index, ConfigOptionUpdateType::Value);
};

void Config::update_option_description(const std::string& option_id, const std::string& new_description) {
    size_t option_index = schema.options_by_id[option_id];
    schema.options[option_index].description = new_description;
    report_config_option_update(option_index, ConfigOptionUpdateType::Description);
}

void Config::update_enum_option_disabled(const std::string& option_id, uint32_t enum_index, bool disabled) {
    size_t option_index = schema.options_by_id[option_id];
    if (!enum_options_disabled.contains(option_index)) {
        enum_options_disabled[option_index] = {};
    }
    if (disabled) {
        enum_options_disabled[option_index].insert(enum_index);
    } else {
        enum_options_disabled[option_index].erase(enum_index);
    }
    report_config_option_update(option_index, ConfigOptionUpdateType::EnumDisabled);
}

void Config::add_option_disable_dependency(const std::string& dependent_option_id, const std::string& source_option_id, std::vector<ConfigValueVariant> &values) {
    size_t dependent_index = schema.options_by_id[dependent_option_id];
    size_t source_index = schema.options_by_id[source_option_id];
    schema.disable_dependencies.add_option_dependency(dependent_index, source_index, values);
}

void Config::add_option_hidden_dependency(const std::string& dependent_option_id, const std::string& source_option_id, std::vector<ConfigValueVariant> &values) {
    size_t dependent_index = schema.options_by_id[dependent_option_id];
    size_t source_index = schema.options_by_id[source_option_id];
    schema.hidden_dependencies.add_option_dependency(dependent_index, source_index, values);
}

std::string Config::get_enum_option_details(size_t option_index) {
    if (!enum_option_details.contains(option_index)) {
        return std::string();
    }
    return enum_option_details[option_index];
}

bool Config::is_config_option_hidden(size_t option_index) {
    return schema.options[option_index].hidden || hidden_options.contains(option_index);
}

}
