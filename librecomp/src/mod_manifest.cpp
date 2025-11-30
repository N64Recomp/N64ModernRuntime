#include <unordered_map>

#include "json/json.hpp"

#include "recompiler/context.h"
#include "librecomp/files.hpp"
#include "librecomp/mods.hpp"

static bool read_json(std::ifstream input_file, nlohmann::json &json_out) {
    if (!input_file.good()) {
        return false;
    }

    try {
        input_file >> json_out;
    }
    catch (nlohmann::json::parse_error &) {
        return false;
    }
    return true;
}

static bool read_json_with_backups(const std::filesystem::path &path, nlohmann::json &json_out) {
    // Try reading and parsing the base file.
    if (read_json(std::ifstream{ path }, json_out)) {
        return true;
    }

    // Try reading and parsing the backup file.
    if (read_json(recomp::open_input_backup_file(path), json_out)) {
        return true;
    }

    // Both reads failed.
    return false;
}

recomp::mods::ZipModFileHandle::~ZipModFileHandle() {
    if (file_handle) {
        fclose(file_handle);
        file_handle = nullptr;
    }

    if (archive) {
        mz_zip_reader_end(archive.get());
    }
    archive = {};
}

recomp::mods::ZipModFileHandle::ZipModFileHandle(const std::filesystem::path& mod_path, ModOpenError& error) {
#ifdef _WIN32
    if (_wfopen_s(&file_handle, mod_path.c_str(), L"rb") != 0) {
        error = ModOpenError::FileError;
        return;
    }
#else
    file_handle = fopen(mod_path.c_str(), "rb");
    if (!file_handle) {
        error = ModOpenError::FileError;
        return;
    }
#endif
    archive = std::make_unique<mz_zip_archive>();
    if (!mz_zip_reader_init_cfile(archive.get(), file_handle, 0, 0)) {
        error = ModOpenError::InvalidZip;
        return;
    }

    error = ModOpenError::Good;
}

recomp::mods::ZipModFileHandle::ZipModFileHandle(std::span<const uint8_t> mod_bytes, ModOpenError& error) {
    archive = std::make_unique<mz_zip_archive>();
    if (!mz_zip_reader_init_mem(archive.get(), mod_bytes.data(), mod_bytes.size(), 0)) {
        error = ModOpenError::InvalidZip;
        return;
    }

    error = ModOpenError::Good;
}

std::vector<char> recomp::mods::ZipModFileHandle::read_file(const std::string& filepath, bool& exists) const {
    std::vector<char> ret{};

    mz_uint32 file_index;
    if (!mz_zip_reader_locate_file_v2(archive.get(), filepath.c_str(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE, &file_index)) {
        exists = false;
        return ret;
    }

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(archive.get(), file_index, &stat)) {
        exists = false;
        return ret;
    }

    ret.resize(stat.m_uncomp_size);
    if (!mz_zip_reader_extract_to_mem(archive.get(), file_index, ret.data(), ret.size(), 0)) {
        exists = false;
        return {};
    }

    exists = true;
    return ret;
}

bool recomp::mods::ZipModFileHandle::file_exists(const std::string& filepath) const {
    mz_uint32 file_index;
    if (!mz_zip_reader_locate_file_v2(archive.get(), filepath.c_str(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE, &file_index)) {
        return false;
    }

    return true;
}

recomp::mods::LooseModFileHandle::~LooseModFileHandle() {
    // Nothing to do here, members will be destroyed automatically.
}

recomp::mods::LooseModFileHandle::LooseModFileHandle(const std::filesystem::path& mod_path, ModOpenError& error) {
    root_path = mod_path;

    std::error_code ec;
    if (!std::filesystem::is_directory(root_path, ec)) {
        error = ModOpenError::NotAFileOrFolder;
    }

    if (ec) {
        error = ModOpenError::FileError;
    }

    error = ModOpenError::Good;
}

std::vector<char> recomp::mods::LooseModFileHandle::read_file(const std::string& filepath, bool& exists) const {
    std::vector<char> ret{};
    std::filesystem::path full_path = root_path / filepath;

    std::error_code ec;
    if (!std::filesystem::is_regular_file(full_path, ec) || ec) {
        exists = false;
        return ret;
    }

    std::ifstream file{ full_path, std::ios::binary };

    if (!file.good()) {
        exists = false;
        return ret;
    }

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    ret.resize(file_size);
    file.read(ret.data(), ret.size());

    exists = true;
    return ret;
}

bool recomp::mods::LooseModFileHandle::file_exists(const std::string& filepath) const {
    std::filesystem::path full_path = root_path / filepath;

    std::error_code ec;
    if (!std::filesystem::is_regular_file(full_path, ec) || ec) {
        return false;
    }

    return true;
}

const std::string game_mod_id_key = "game_id";
const std::string mod_id_key = "id";
const std::string display_name_key = "display_name";
const std::string description_key = "description";
const std::string short_description_key = "short_description";
const std::string version_key = "version";
const std::string authors_key = "authors";
const std::string minimum_recomp_version_key = "minimum_recomp_version";
const std::string enabled_by_default_key = "enabled_by_default";
const std::string dependencies_key = "dependencies";
const std::string optional_dependencies_key = "optional_dependencies";
const std::string native_libraries_key = "native_libraries";
const std::string config_schema_key = "config_schema";

template <typename T1, typename T2>
bool get_to(const nlohmann::json& val, T2& out) {
    const T1* ptr = val.get_ptr<const T1*>();
    if (ptr == nullptr) {
        return false;
    }

    out = *ptr;
    return true;
}

template <typename T1, typename T2>
bool get_to_vec(const nlohmann::json& val, std::vector<T2>& out) {
    const nlohmann::json::array_t* ptr = val.get_ptr<const nlohmann::json::array_t*>();
    if (ptr == nullptr) {
        return false;
    }

    out.clear();

    for (const nlohmann::json& cur_val : *ptr) {
        const T1* temp_ptr = cur_val.get_ptr<const T1*>();
        if (temp_ptr == nullptr) {
            out.clear();
            return false;
        }

        out.emplace_back(*temp_ptr);
    }

    return true;
}

static bool parse_dependency(const std::string& val, recomp::mods::Dependency& out) {
    recomp::mods::Dependency ret;

    bool validated_name;
    bool validated_version;

    // Check if there's a version number specified.
    size_t colon_pos = val.find(':');
    if (colon_pos == std::string::npos) {
        // No version present, so just validate the dependency's id.
        validated_name = N64Recomp::validate_mod_id(std::string_view{val});
        ret.mod_id = val;
        validated_version = true;
        ret.version.minor = 0;
        ret.version.major = 0;
        ret.version.patch = 0;
    }
    else {
        // Version present, validate both the id and version.        
        ret.mod_id = val.substr(0, colon_pos);
        validated_name = N64Recomp::validate_mod_id(ret.mod_id);
        validated_version = recomp::Version::from_string(val.substr(colon_pos + 1), ret.version);
    }

    if (validated_name && validated_version) {
        out = std::move(ret);
        return true;
    }

    return false;
}

template <typename T1, typename T2>
recomp::mods::ModOpenError try_get(T2& out, const nlohmann::json& data, const std::string& key, bool required, std::string& error_param, T2 default_value = {}) {
    auto find_it = data.find(key);
    if (find_it == data.end()) {
        if (required) {
            error_param = key;
            return recomp::mods::ModOpenError::MissingManifestField;
        }
        out = default_value;
        return recomp::mods::ModOpenError::Good;
    }

    const T1* ptr = find_it->get_ptr<const T1*>();
    if (ptr == nullptr) {
        error_param = key;
        return recomp::mods::ModOpenError::IncorrectManifestFieldType;
    }

    out = *ptr;
    return recomp::mods::ModOpenError::Good;
}

recomp::mods::ModOpenError try_get_version(recomp::Version& out, const nlohmann::json& data, const std::string& key, std::string& error_param, recomp::mods::ModOpenError invalid_version_error) {
    std::string version_string{};

    recomp::mods::ModOpenError try_get_err = try_get<nlohmann::json::string_t>(version_string, data, key, true, error_param);
    if (try_get_err != recomp::mods::ModOpenError::Good) {
        return try_get_err;
    }

    if (!recomp::Version::from_string(version_string, out)) {
        error_param = version_string;
        return invalid_version_error;
    }

    return recomp::mods::ModOpenError::Good;
}

template <typename T1, typename T2>
recomp::mods::ModOpenError try_get_vec(std::vector<T2>& out, const nlohmann::json& data, const std::string& key, bool required, std::string& error_param) {
    auto find_it = data.find(key);
    if (find_it == data.end()) {
        if (required) {
            error_param = key;
            return recomp::mods::ModOpenError::MissingManifestField;
        }
        return recomp::mods::ModOpenError::Good;
    }

    const nlohmann::json::array_t* ptr = find_it->get_ptr<const nlohmann::json::array_t*>();
    if (ptr == nullptr) {
        error_param = key;
        return recomp::mods::ModOpenError::IncorrectManifestFieldType;
    }

    out.clear();

    for (const nlohmann::json& cur_val : *ptr) {
        const T1* temp_ptr = cur_val.get_ptr<const T1*>();
        if (temp_ptr == nullptr) {
            out.clear();
            error_param = key;
            return recomp::mods::ModOpenError::IncorrectManifestFieldType;
        }

        out.emplace_back(*temp_ptr);
    }

    return recomp::mods::ModOpenError::Good;
}

constexpr std::string_view config_schema_id_key = "id";
constexpr std::string_view config_schema_name_key = "name";
constexpr std::string_view config_schema_description_key = "description";
constexpr std::string_view config_schema_type_key = "type";
constexpr std::string_view config_schema_min_key = "min";
constexpr std::string_view config_schema_max_key = "max";
constexpr std::string_view config_schema_step_key = "step";
constexpr std::string_view config_schema_precision_key = "precision";
constexpr std::string_view config_schema_percent_key = "percent";
constexpr std::string_view config_schema_options_key = "options";
constexpr std::string_view config_schema_default_key = "default";

std::unordered_map<std::string, recomp::mods::ConfigOptionType> config_option_map{
    { "Enum",   recomp::mods::ConfigOptionType::Enum},
    { "Number", recomp::mods::ConfigOptionType::Number},
    { "String", recomp::mods::ConfigOptionType::String},
};

recomp::mods::ModOpenError parse_manifest_config_schema_option(const nlohmann::json &config_schema_json, recomp::mods::ModManifest &ret, std::string &error_param) {
    using json = nlohmann::json;
    recomp::mods::ConfigOption option;
    auto id = config_schema_json.find(config_schema_id_key);
    if (id != config_schema_json.end()) {
        if (!get_to<json::string_t>(*id, option.id)) {
            error_param = config_schema_id_key;
            return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
        }
    }
    else {
        error_param = config_schema_id_key;
        return recomp::mods::ModOpenError::MissingConfigSchemaField;
    }

    auto name = config_schema_json.find(config_schema_name_key);
    if (name != config_schema_json.end()) {
        if (!get_to<json::string_t>(*name, option.name)) {
            error_param = config_schema_name_key;
            return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
        }
    }
    else {
        error_param = config_schema_name_key;
        return recomp::mods::ModOpenError::MissingConfigSchemaField;
    }

    auto description = config_schema_json.find(config_schema_description_key);
    if (description != config_schema_json.end()) {
        if (!get_to<json::string_t>(*description, option.description)) {
            error_param = config_schema_description_key;
            return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
        }
    }

    auto type = config_schema_json.find(config_schema_type_key);
    if (type != config_schema_json.end()) {
        std::string type_string;
        if (!get_to<json::string_t>(*type, type_string)) {
            error_param = config_schema_type_key;
            return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
        }
        else {
            auto it = config_option_map.find(type_string);
            if (it != config_option_map.end()) {
                option.type = it->second;
            }
            else {
                error_param = config_schema_type_key;
                return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
            }
        }
    }
    else {
        error_param = config_schema_type_key;
        return recomp::mods::ModOpenError::MissingConfigSchemaField;
    }

    switch (option.type) {
    case recomp::mods::ConfigOptionType::Enum:
        {
            recomp::mods::ConfigOptionEnum option_enum;

            auto options = config_schema_json.find(config_schema_options_key);
            if (options != config_schema_json.end()) {
                if (!get_to_vec<std::string>(*options, option_enum.options)) {
                    error_param = config_schema_options_key;
                    return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
                }
            }

            auto default_value = config_schema_json.find(config_schema_default_key);
            if (default_value != config_schema_json.end()) {
                std::string default_value_string;
                if (get_to<json::string_t>(*default_value, default_value_string)) {
                    auto it = std::find(option_enum.options.begin(), option_enum.options.end(), default_value_string);
                    if (it != option_enum.options.end()) {
                        option_enum.default_value = uint32_t(it - option_enum.options.begin());
                    }
                    else {
                        error_param = config_schema_default_key;
                        return recomp::mods::ModOpenError::InvalidConfigSchemaDefault;
                    }
                }
                else {
                    error_param = config_schema_default_key;
                    return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
                }
            }

            option.variant = option_enum;

        }
        break;
    case recomp::mods::ConfigOptionType::Number:
        {
            recomp::mods::ConfigOptionNumber option_number;

            auto min = config_schema_json.find(config_schema_min_key);
            if (min != config_schema_json.end()) {
                if (!min->is_number()) {
                    error_param = config_schema_min_key;
                    return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
                }
                option_number.min = min->template get<double>();
            }

            auto max = config_schema_json.find(config_schema_max_key);
            if (max != config_schema_json.end()) {
                if (!max->is_number()) {
                    error_param = config_schema_max_key;
                    return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
                }
                option_number.max = max->template get<double>();
            }
            
            auto step = config_schema_json.find(config_schema_step_key);
            if (step != config_schema_json.end()) {
                if (!step->is_number()) {
                    error_param = config_schema_step_key;
                    return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
                }
                option_number.step = step->template get<double>();
            }

            auto precision = config_schema_json.find(config_schema_precision_key);
            if (precision != config_schema_json.end()) {
                int64_t precision_int64;
                if (get_to<int64_t>(*precision, precision_int64)) {
                    option_number.precision = precision_int64;
                }
                else {
                    error_param = config_schema_precision_key;
                    return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
                }
            }

            auto percent = config_schema_json.find(config_schema_percent_key);
            if (percent != config_schema_json.end()) {
                if (!get_to<bool>(*percent, option_number.percent)) {
                    error_param = config_schema_percent_key;
                    return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
                }
            }

            auto default_value = config_schema_json.find(config_schema_default_key);
            if (default_value != config_schema_json.end()) {
                if (!default_value->is_number()) {
                    error_param = config_schema_default_key;
                    return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
                }
                option_number.default_value = default_value->template get<double>();
            }

            option.variant = option_number;
        }
        break;
    case recomp::mods::ConfigOptionType::String:
        {
            recomp::mods::ConfigOptionString option_string;

            auto default_value = config_schema_json.find(config_schema_default_key);
            if (default_value != config_schema_json.end()) {
                if (!get_to<json::string_t>(*default_value, option_string.default_value)) {
                    error_param = config_schema_default_key;
                    return recomp::mods::ModOpenError::IncorrectConfigSchemaType;
                }
            }

            option.variant = option_string;
        }
        break;
    default:
        break;
    }

    ret.config_schema.options_by_id.emplace(option.id, ret.config_schema.options.size());
    ret.config_schema.options.emplace_back(option);

    return recomp::mods::ModOpenError::Good;
}

recomp::mods::ModOpenError recomp::mods::parse_manifest(ModManifest& ret, const std::vector<char>& manifest_data, std::string& error_param) {
    using json = nlohmann::json;
    json manifest_json = json::parse(manifest_data.begin(), manifest_data.end(), nullptr, false);

    if (manifest_json.is_discarded()) {
        return ModOpenError::FailedToParseManifest;
    }

    if (!manifest_json.is_object()) {
        return ModOpenError::InvalidManifestSchema;
    }

    ModOpenError current_error = ModOpenError::Good;

    // Mod Game ID
    std::string mod_game_id{};
    current_error = try_get<json::string_t>(mod_game_id, manifest_json, game_mod_id_key, true, error_param);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }
    ret.mod_game_ids.emplace_back(std::move(mod_game_id));

    // Mod ID
    current_error = try_get<json::string_t>(ret.mod_id, manifest_json, mod_id_key, true, error_param);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }

    // Display name
    current_error = try_get<json::string_t>(ret.display_name, manifest_json, display_name_key, true, error_param);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }

    // Description (optional)
    current_error = try_get<json::string_t>(ret.description, manifest_json, description_key, false, error_param);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }

    // Short Description (optional)
    current_error = try_get<json::string_t>(ret.short_description, manifest_json, short_description_key, false, error_param);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }

    // Version
    current_error = try_get_version(ret.version, manifest_json, version_key, error_param, ModOpenError::InvalidVersionString);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }

    // Authors
    current_error = try_get_vec<json::string_t>(ret.authors, manifest_json, authors_key, true, error_param);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }

    // Minimum recomp version
    current_error = try_get_version(ret.minimum_recomp_version, manifest_json, minimum_recomp_version_key, error_param, ModOpenError::InvalidMinimumRecompVersionString);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }

    // Enabled by default (optional, true if not present)
    current_error = try_get<json::boolean_t>(ret.enabled_by_default, manifest_json, enabled_by_default_key, false, error_param, true);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }

    // Dependencies (optional)
    std::vector<std::string> dep_strings{};
    current_error = try_get_vec<json::string_t>(dep_strings, manifest_json, dependencies_key, false, error_param);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }
    for (const std::string& dep_string : dep_strings) {
        Dependency cur_dep;
        if (!parse_dependency(dep_string, cur_dep)) {
            error_param = dep_string;
            return ModOpenError::InvalidDependencyString;
        }
        cur_dep.optional = false;

        size_t dependency_index = ret.dependencies.size();
        ret.dependencies_by_id.emplace(cur_dep.mod_id, dependency_index);
        ret.dependencies.emplace_back(std::move(cur_dep));
    }

    // Optional dependencies (optional)
    std::vector<std::string> optional_dep_strings{};
    current_error = try_get_vec<json::string_t>(optional_dep_strings, manifest_json, optional_dependencies_key, false, error_param);
    if (current_error != ModOpenError::Good) {
        return current_error;
    }
    for (const std::string& dep_string : optional_dep_strings) {
        Dependency cur_dep;
        if (!parse_dependency(dep_string, cur_dep)) {
            error_param = dep_string;
            return ModOpenError::InvalidDependencyString;
        }
        cur_dep.optional = true;

        size_t dependency_index = ret.dependencies.size();
        ret.dependencies_by_id.emplace(cur_dep.mod_id, dependency_index);
        ret.dependencies.emplace_back(std::move(cur_dep));
    }

    // Native libraries (optional)
    auto find_libs_it = manifest_json.find(native_libraries_key);
    if (find_libs_it != manifest_json.end()) {
        auto& val = *find_libs_it;
        if (!val.is_object()) {
            error_param = native_libraries_key;
            return ModOpenError::IncorrectManifestFieldType;
        }
        for (const auto& [lib_name, lib_exports] : val.items()) {
            NativeLibraryManifest& cur_lib = ret.native_libraries.emplace_back();

            cur_lib.name = lib_name;
            if (!get_to_vec<std::string>(lib_exports, cur_lib.exports)) {
                error_param = native_libraries_key;
                return ModOpenError::IncorrectManifestFieldType;
            }
        }
    }

    // Config schema (optional)
    auto find_config_schema_it = manifest_json.find(config_schema_key);
    if (find_config_schema_it != manifest_json.end()) {
        auto& val = *find_config_schema_it;
        if (!val.is_object()) {
            error_param = config_schema_key;
            return ModOpenError::IncorrectManifestFieldType;
        }

        auto options = val.find(config_schema_options_key);
        if (options != val.end()) {
            if (!options->is_array()) {
                error_param = config_schema_options_key;
                return ModOpenError::IncorrectManifestFieldType;
            }

            for (const json &option : *options) {
                ModOpenError open_error = parse_manifest_config_schema_option(option, ret, error_param);
                if (open_error != ModOpenError::Good) {
                    return open_error;
                }
            }
        }
        else {
            error_param = config_schema_options_key;
            return ModOpenError::MissingConfigSchemaField;
        }
    }

    return ModOpenError::Good;
}

bool parse_mod_config_storage(const std::filesystem::path &path, const std::string &expected_mod_id, recomp::mods::ConfigStorage &config_storage, const recomp::mods::ConfigSchema &config_schema) {
    using json = nlohmann::json;
    json config_json;
    if (!read_json_with_backups(path, config_json)) {
        return false;
    }

    auto mod_id = config_json.find("mod_id");
    if (mod_id != config_json.end()) {
        std::string mod_id_str;
        if (get_to<json::string_t>(*mod_id, mod_id_str)) {
            if (*mod_id != expected_mod_id) {
                // The mod's ID doesn't match.
                return false;
            }
        }
        else {
            // The mod ID is not a string.
            return false;
        }
    }
    else {
        // The configuration file doesn't have a mod ID.
        return false;
    }

    auto storage_json = config_json.find("storage");
    if (storage_json == config_json.end()) {
        // The configuration file doesn't have a storage object.
        return false;
    }

    if (!storage_json->is_object()) {
        // The storage key does not correspond to an object.
        return false;
    }

    // Only parse the object for known option types based on the schema.
    std::string value_str;
    for (const recomp::mods::ConfigOption &option : config_schema.options) {
        auto option_json = storage_json->find(option.id);
        if (option_json == storage_json->end()) {
            // Option doesn't exist in storage.
            continue;
        }

        switch (option.type) {
        case recomp::mods::ConfigOptionType::Enum:
            if (get_to<json::string_t>(*option_json, value_str)) {
                const recomp::mods::ConfigOptionEnum &option_enum = std::get<recomp::mods::ConfigOptionEnum>(option.variant);
                auto option_it = std::find(option_enum.options.begin(), option_enum.options.end(), value_str);
                if (option_it != option_enum.options.end()) {
                    config_storage.value_map[option.id] = uint32_t(option_it - option_enum.options.begin());
                }
            }

            break;
        case recomp::mods::ConfigOptionType::Number:
            if (option_json->is_number()) {
                config_storage.value_map[option.id] = option_json->template get<double>();
            }

            break;
        case recomp::mods::ConfigOptionType::String: {
            if (get_to<json::string_t>(*option_json, value_str)) {
                config_storage.value_map[option.id] = value_str;
            }

            break;
        }
        default:
            assert(false && "Unknown option type.");
            break;
        }
    }

    return true;
}

recomp::mods::ModOpenError recomp::mods::ModContext::open_mod_from_manifest(ModManifest& manifest, std::string& error_param, const std::vector<ModContentTypeId>& supported_content_types, bool requires_manifest) {
    {
        bool exists;
        std::vector<char> manifest_data = manifest.file_handle->read_file("mod.json", exists);
        if (!exists) {
            // If this container type requires a manifest then return an error.
            if (requires_manifest) {
                return ModOpenError::NoManifest;
            }
            // Otherwise, create a default manifest.
            else {
                // Take the file handle from the manifest before clearing it so that it can be reassigned afterwards.
                std::unique_ptr<ModFileHandle> file_handle = std::move(manifest.file_handle);
                std::filesystem::path root_path = std::move(manifest.mod_root_path);
                manifest = {};
                manifest.file_handle = std::move(file_handle);
                manifest.mod_root_path = std::move(root_path);

                for (const auto &[key, val] : mod_game_ids) {
                    manifest.mod_game_ids.emplace_back(key);
                }

                manifest.mod_id = manifest.mod_root_path.stem().string();
                manifest.display_name = manifest.mod_id;
                manifest.description.clear();
                manifest.short_description.clear();
                manifest.authors = { "Unknown" };

                manifest.minimum_recomp_version.major = 0;
                manifest.minimum_recomp_version.minor = 0;
                manifest.minimum_recomp_version.patch = 0;
                manifest.version.major = 0;
                manifest.version.minor = 0;
                manifest.version.patch = 0;
                manifest.enabled_by_default = true;
            }
        }
        else {
            ModOpenError parse_error = parse_manifest(manifest, manifest_data, error_param);
            if (parse_error != ModOpenError::Good) {
                return parse_error;
            }
        }
    }

    // Check for this being a duplicate of another opened mod.
    if (mod_ids.contains(manifest.mod_id)) {
        error_param = manifest.mod_id;
        return ModOpenError::DuplicateMod;
    }
    mod_ids.emplace(manifest.mod_id);

    // Check for this mod's game ids being valid.
    std::vector<size_t> game_indices;
    for (const auto &mod_game_id : manifest.mod_game_ids) {
        auto find_id_it = mod_game_ids.find(mod_game_id);
        if (find_id_it == mod_game_ids.end()) {
            error_param = mod_game_id;
            return ModOpenError::WrongGame;
        }
        game_indices.emplace_back(find_id_it->second);
    }

    // Scan for content types present in this mod.
    std::vector<ModContentTypeId> detected_content_types;

    auto scan_for_content_type = [&detected_content_types, &manifest](ModContentTypeId type_id, std::vector<ModContentType> &content_types) {
        const ModContentType &content_type = content_types[type_id.value];
        if (manifest.file_handle->file_exists(content_type.content_filename)) {
            detected_content_types.emplace_back(type_id);
        }
        };

    // If the mod has a list of specific content types, scan for only those.
    if (!supported_content_types.empty()) {
        for (ModContentTypeId content_type_id : supported_content_types) {
            scan_for_content_type(content_type_id, content_types);
        }
    }
    // Otherwise, scan for all content types.
    else {
        for (size_t content_type_index = 0; content_type_index < content_types.size(); content_type_index++) {
            scan_for_content_type(ModContentTypeId{ .value = content_type_index }, content_types);
        }
    }

    // Read the mod config if it exists.
    ConfigStorage config_storage;
    std::filesystem::path config_path = mod_config_directory / (manifest.mod_id + ".json");
    parse_mod_config_storage(config_path, manifest.mod_id, config_storage, manifest.config_schema);

    // Read the mod thumbnail if it exists.
    static const std::string thumbnail_dds_name = "thumb.dds";
    static const std::string thumbnail_png_name = "thumb.png";
    bool exists = false;
    std::vector<char> thumbnail_data = manifest.file_handle->read_file(thumbnail_dds_name, exists);
    if (!exists) {
        thumbnail_data = manifest.file_handle->read_file(thumbnail_png_name, exists);
    }

    // Store the loaded mod manifest in a new mod handle.
    add_opened_mod(std::move(manifest), std::move(config_storage), std::move(game_indices), std::move(detected_content_types), std::move(thumbnail_data));

    return ModOpenError::Good;
}

recomp::mods::ModOpenError recomp::mods::ModContext::open_mod_from_path(const std::filesystem::path& mod_path, std::string& error_param, const std::vector<ModContentTypeId>& supported_content_types, bool requires_manifest) {
    ModManifest manifest{};
    manifest.mod_root_path = mod_path;

    std::error_code ec;
    error_param = "";

    if (!std::filesystem::exists(mod_path, ec) || ec) {
        return ModOpenError::DoesNotExist;
    }

    // TODO support symlinks?
    bool is_file = std::filesystem::is_regular_file(mod_path, ec);
    if (ec) {
        return ModOpenError::FileError;
    }

    bool is_directory = std::filesystem::is_directory(mod_path, ec);
    if (ec) {
        return ModOpenError::FileError;
    }

    // Load the directory or zip file.
    ModOpenError handle_error;
    if (is_file) {
        manifest.file_handle = std::make_unique<recomp::mods::ZipModFileHandle>(mod_path, handle_error);
    }
    else if (is_directory) {
        manifest.file_handle = std::make_unique<recomp::mods::LooseModFileHandle>(mod_path, handle_error);
    }
    else {
        return ModOpenError::NotAFileOrFolder;
    }

    if (handle_error != ModOpenError::Good) {
        return handle_error;
    }

    return open_mod_from_manifest(manifest, error_param, supported_content_types, requires_manifest);
}

recomp::mods::ModOpenError recomp::mods::ModContext::open_mod_from_memory(std::span<const uint8_t> mod_bytes, std::string &error_param, const std::vector<ModContentTypeId> &supported_content_types, bool requires_manifest) {
    ModManifest manifest{};
    ModOpenError handle_error;
    manifest.file_handle = std::make_unique<recomp::mods::ZipModFileHandle>(mod_bytes, handle_error);
    if (handle_error != ModOpenError::Good) {
        return handle_error;
    }

    return open_mod_from_manifest(manifest, error_param, supported_content_types, requires_manifest);
}

std::string recomp::mods::error_to_string(ModOpenError error) {
    switch (error) {
        case ModOpenError::Good:
            return "Good";
        case ModOpenError::DoesNotExist:
            return "Mod does not exist";
        case ModOpenError::NotAFileOrFolder:
            return "Mod is not a file or folder";
        case ModOpenError::FileError:
            return "Error reading mod file(s)";
        case ModOpenError::InvalidZip:
            return "Mod is an invalid zip file";
        case ModOpenError::NoManifest:
            return "Mod is missing a mod.json";
        case ModOpenError::FailedToParseManifest:
            return "Failed to parse mod's mod.json";
        case ModOpenError::InvalidManifestSchema:
            return "Mod's mod.json has an invalid schema";
        case ModOpenError::IncorrectManifestFieldType:
            return "Incorrect type for field in mod.json";
        case ModOpenError::MissingConfigSchemaField:
            return "Missing required field in config schema in mod.json";
        case ModOpenError::IncorrectConfigSchemaType:
            return "Incorrect type for field in config schema in mod.json";
        case ModOpenError::InvalidConfigSchemaDefault:
            return "Invalid default for option in config schema in mod.json";
        case ModOpenError::InvalidVersionString:
            return "Invalid version string in mod.json";
        case ModOpenError::InvalidMinimumRecompVersionString:
            return "Invalid minimum recomp version string in mod.json";
        case ModOpenError::InvalidDependencyString:
            return "Invalid dependency string in mod.json";
        case ModOpenError::MissingManifestField:
            return "Missing required field in mod.json";
        case ModOpenError::DuplicateMod:
            return "Duplicate mod found";
        case ModOpenError::WrongGame:
            return "Mod is for a different game";
    }
    return "Unknown mod opening error: " + std::to_string((int)error);
}

std::string recomp::mods::error_to_string(ModLoadError error) {
    switch (error) {
        case ModLoadError::Good:
            return "Good";
        case ModLoadError::InvalidGame:
            return "Invalid game";
        case ModLoadError::MinimumRecompVersionNotMet:
            return "Mod requires a newer version of this project";
        case ModLoadError::MissingDependency:
            return "Missing dependency";
        case ModLoadError::WrongDependencyVersion:
            return "Wrong dependency version";
        case ModLoadError::FailedToLoadCode:
            return "Failed to load mod code";
        case ModLoadError::RomPatchConflict:
            return "ROM patch mod conflict";
        case ModLoadError::FailedToLoadPatch:
            return "Invalid ROM patch";
    }
    return "Unknown mod loading error " + std::to_string((int)error);
}

std::string recomp::mods::error_to_string(CodeModLoadError error) {
    switch (error) {
        case CodeModLoadError::Good:
            return "Good";
        case CodeModLoadError::InternalError:
            return "Code mod loading internal error";
        case CodeModLoadError::HasSymsButNoBinary:
            return "Mod has a symbol file but no binary file";
        case CodeModLoadError::HasBinaryButNoSyms:
            return "Mod has a binary file but no symbol file";
        case CodeModLoadError::FailedToParseSyms:
            return "Failed to parse mod symbol file";
        case CodeModLoadError::MissingDependencyInManifest:
            return "Dependency is present in mod symbols but not in the manifest";
        case CodeModLoadError::FailedToLoadNativeCode:
            return "Failed to load offline mod library";
        case CodeModLoadError::FailedToLoadNativeLibrary:
            return "Failed to load mod library";
        case CodeModLoadError::FailedToFindNativeExport:
            return "Failed to find native export";
        case CodeModLoadError::FailedToRecompile:
            return "Failed to recompile mod";
        case CodeModLoadError::InvalidReferenceSymbol:
            return "Reference symbol does not exist";
        case CodeModLoadError::InvalidImport:
            return "Imported function not found";
        case CodeModLoadError::InvalidCallbackEvent:
            return "Event for callback not found";
        case CodeModLoadError::InvalidFunctionReplacement:
            return "Function to be replaced does not exist";
        case CodeModLoadError::HooksUnavailable:
            // This error will occur if the ROM's GameEntry is set as having compressed code, but no
            // ROM decompression routine has been provided. 
            return "Function hooks are currently unavailable in this project";
        case CodeModLoadError::InvalidHook:
            return "Function to be hooked does not exist";
        case CodeModLoadError::CannotBeHooked:
            return "Function is not hookable";
        case CodeModLoadError::FailedToFindReplacement:
            return "Failed to find replacement function";
        case CodeModLoadError::BaseRecompConflict:
            return "Attempted to replace a function that's been patched by the base recomp";
        case CodeModLoadError::ModConflict:
            return "Conflicts with other mod";
        case CodeModLoadError::DuplicateExport:
            return "Duplicate exports in mod";
        case CodeModLoadError::OfflineModHooked:
            return "Offline recompiled mod has a function hooked by another mod";
        case CodeModLoadError::NoSpecifiedApiVersion:
            return "Mod DLL does not specify an API version";
        case CodeModLoadError::UnsupportedApiVersion:
            return "Mod DLL has an unsupported API version";
    }
}
