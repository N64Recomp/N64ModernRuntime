#include "librecomp/config.hpp"

static char make_char_upper(char c) {
    if (c >= 'a' && c <= 'z')  {
        c -= 'a' - 'A';
    }
    return c;
}

static bool case_insensitive_compare(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (make_char_upper(a[i]) != make_char_upper(b[i])) {
            return false;
        }
    }
    return true;
}

namespace recomp::config {
    bool check_config_option_bool_string(const std::string& str) {
        static const std::string false_strings[] = {
            "false",
            "off",
            "no",
        };
        static const std::string true_strings[] = {
            "true",
            "on",
            "yes",
        };

        for (const auto& false_str : false_strings) {
            if (case_insensitive_compare(str, false_str)) {
                return false;
            }
        }

        for (const auto& true_str : true_strings) {
            if (case_insensitive_compare(str, true_str)) {
                return true;
            }
        }

        return false;
    }

    // ConfigOptionEnum
    std::vector<ConfigOptionEnumOption>::const_iterator ConfigOptionEnum::find_option_from_string(const std::string& option_key) const {
        return std::find_if(options.begin(), options.end(), [option_key](const ConfigOptionEnumOption& opt) {
            return case_insensitive_compare(opt.key, option_key);
        });
    };

    std::vector<ConfigOptionEnumOption>::const_iterator ConfigOptionEnum::find_option_from_value(uint32_t value) const {
        return std::find_if(options.begin(), options.end(), [value](const ConfigOptionEnumOption& opt) {
            return opt.value == value;
        });
    }

    bool ConfigOptionEnum::can_add_option(const std::string& option_key, uint32_t option_value) const {
        return options.size() == 0 || (
            find_option_from_string(option_key) == options.end() &&
            find_option_from_value(option_value) == options.end());
    }

    // ConfigOptionDependency
    void ConfigOptionDependency::add_option_dependency(size_t dependent_option_index, size_t source_option_index, std::vector<ConfigValueVariant> &values) {
        if (!option_to_dependencies.contains(source_option_index)) {
            option_to_dependencies[source_option_index] = {};
        }
        option_to_dependencies[source_option_index].insert(dependent_option_index);
        dependency_to_values[dependent_option_index] = values;
    }

    std::unordered_map<size_t, bool> ConfigOptionDependency::check_option_dependencies(size_t source_option_index, ConfigValueVariant value) {
        std::unordered_map<size_t, bool> result{};
        if (!option_to_dependencies.contains(source_option_index)) {
            return result;
        }

        std::unordered_set<size_t> &dependencies = option_to_dependencies[source_option_index];
        for (auto &dep : dependencies) {
            bool is_match = false;
            for (auto &check_value : dependency_to_values[dep]) {
                if (value == check_value) {
                    is_match = true;
                    break;
                }
            }

            result[dep] = is_match;
        }

        return result;
    }
}
