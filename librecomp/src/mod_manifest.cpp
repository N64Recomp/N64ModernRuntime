#include <unordered_map>

#include "json/json.hpp"

#include "recompiler/context.h"
#include "librecomp/mods.hpp"

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

enum class ManifestField {
    GameModId,
    Id,
    Version,
    Authors,
    MinimumRecompVersion,
    Dependencies,
    NativeLibraries,
};

const std::string game_mod_id_key = "game_id";
const std::string mod_id_key = "id";
const std::string version_key = "version";
const std::string authors_key = "authors";
const std::string minimum_recomp_version_key = "minimum_recomp_version";
const std::string dependencies_key = "dependencies";
const std::string native_libraries_key = "native_libraries";

std::unordered_map<std::string, ManifestField> field_map {
    { game_mod_id_key,            ManifestField::GameModId            },
    { mod_id_key,                 ManifestField::Id                   },
    { version_key,                ManifestField::Version              },
    { authors_key,                ManifestField::Authors              },
    { minimum_recomp_version_key, ManifestField::MinimumRecompVersion },
    { dependencies_key,           ManifestField::Dependencies         },
    { native_libraries_key,       ManifestField::NativeLibraries      },
};

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

recomp::mods::ModOpenError parse_manifest(recomp::mods::ModManifest& ret, const std::vector<char>& manifest_data, std::string& error_param) {
    using json = nlohmann::json;
    json manifest_json = json::parse(manifest_data.begin(), manifest_data.end(), nullptr, false);

    if (manifest_json.is_discarded()) {
        return recomp::mods::ModOpenError::FailedToParseManifest;
    }

    if (!manifest_json.is_object()) {
        return recomp::mods::ModOpenError::InvalidManifestSchema;
    }

    for (const auto& [key, val] : manifest_json.items()) {
        const auto find_key_it = field_map.find(key);
        if (find_key_it == field_map.end()) {
            // Unrecognized field
            error_param = key;
            return recomp::mods::ModOpenError::UnrecognizedManifestField;
        }

        ManifestField field = find_key_it->second;
        switch (field) {
            case ManifestField::GameModId:
                {
                    std::string mod_game_id;
                    if (!get_to<json::string_t>(val, mod_game_id)) {
                        error_param = key;
                        return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    }
                    ret.mod_game_ids.resize(1);
                    ret.mod_game_ids[0] = std::move(mod_game_id);
                }
                break;
            case ManifestField::Id:
                if (!get_to<json::string_t>(val, ret.mod_id)) {
                    error_param = key;
                    return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                }
                break;
            case ManifestField::Version:
                {
                    const std::string* version_str = val.get_ptr<const std::string*>();
                    if (version_str == nullptr) {
                        error_param = key;
                        return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    }
                    if (!recomp::Version::from_string(*version_str, ret.version)) {
                        error_param = *version_str;
                        return recomp::mods::ModOpenError::InvalidVersionString;
                    }
                }
                break;
            case ManifestField::Authors:
                if (!get_to_vec<std::string>(val, ret.authors)) {
                    error_param = key;
                    return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                }
                break;
            case ManifestField::MinimumRecompVersion:
                {
                    const std::string* version_str = val.get_ptr<const std::string*>();
                    if (version_str == nullptr) {
                        error_param = key;
                        return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    }
                    if (!recomp::Version::from_string(*version_str, ret.minimum_recomp_version)) {
                        error_param = *version_str;
                        return recomp::mods::ModOpenError::InvalidMinimumRecompVersionString;
                    }
                    ret.minimum_recomp_version.suffix.clear();
                }
                break;
            case ManifestField::Dependencies:
                {
                    std::vector<std::string> dep_strings{};
                    if (!get_to_vec<std::string>(val, dep_strings)) {
                        error_param = key;
                        return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    }

                    for (const std::string& dep_string : dep_strings) {
                        recomp::mods::Dependency cur_dep;
                        if (!parse_dependency(dep_string, cur_dep)) {
                            error_param = dep_string;
                            return recomp::mods::ModOpenError::InvalidDependencyString;
                        }

                        size_t dependency_index = ret.dependencies.size();
                        ret.dependencies_by_id.emplace(cur_dep.mod_id, dependency_index);
                        ret.dependencies.emplace_back(std::move(cur_dep));
                    }
                }
                break;
            case ManifestField::NativeLibraries:
                {
                    if (!val.is_object()) {
                        error_param = key;
                        return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    }
                    for (const auto& [lib_name, lib_exports] : val.items()) {
                        recomp::mods::NativeLibraryManifest& cur_lib = ret.native_libraries.emplace_back();

                        cur_lib.name = lib_name;
                        if (!get_to_vec<std::string>(lib_exports, cur_lib.exports)) {
                            error_param = key;
                            return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                        }
                    }
                }
                break;
        }
    }

    return recomp::mods::ModOpenError::Good;
}

recomp::mods::ModOpenError validate_manifest(const recomp::mods::ModManifest& manifest, std::string& error_param) {
    using namespace recomp::mods;

    // Check for required fields.
    if (manifest.mod_game_ids.empty()) {
        error_param = game_mod_id_key;
        return ModOpenError::MissingManifestField;
    }    
    if (manifest.mod_id.empty()) {
        error_param = mod_id_key;
        return ModOpenError::MissingManifestField;
    }
    if (manifest.version.major == -1 || manifest.version.major == -1 || manifest.version.major == -1) {
        error_param = version_key;
        return ModOpenError::MissingManifestField;
    }
    if (manifest.authors.empty()) {
        error_param = authors_key;
        return ModOpenError::MissingManifestField;
    }
    if (manifest.minimum_recomp_version.major == -1 || manifest.minimum_recomp_version.major == -1 || manifest.minimum_recomp_version.major == -1) {
        error_param = minimum_recomp_version_key;
        return ModOpenError::MissingManifestField;
    }

    return ModOpenError::Good;
}   

recomp::mods::ModOpenError recomp::mods::ModContext::open_mod(const std::filesystem::path& mod_path, std::string& error_param, const std::vector<ModContentTypeId>& supported_content_types, bool requires_manifest) {
    ModManifest manifest{};
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

    {
        bool exists;
        std::vector<char> manifest_data = manifest.file_handle->read_file("manifest.json", exists);
        if (!exists) {
            // If this container type requires a manifest then return an error.
            if (requires_manifest) {
                return ModOpenError::NoManifest;
            }
            // Otherwise, create a default manifest.
            else {
                // Take the file handle from the manifest before clearing it so that it can be reassigned afterwards.
                std::unique_ptr<ModFileHandle> file_handle = std::move(manifest.file_handle);
                manifest = {};
                manifest.file_handle = std::move(file_handle);
                
                for (const auto& [key, val] : mod_game_ids) {
                    manifest.mod_game_ids.emplace_back(key);
                }

                manifest.mod_id = mod_path.stem().string();
                manifest.authors = { "Unknown" };

                manifest.minimum_recomp_version.major = 0;
                manifest.minimum_recomp_version.minor = 0;
                manifest.minimum_recomp_version.patch = 0;
                manifest.version.major = 0;
                manifest.version.minor = 0;
                manifest.version.patch = 0;
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

    ModOpenError validate_error = validate_manifest(manifest, error_param);
    if (validate_error != ModOpenError::Good) {
        return validate_error;
    }

    // Check for this mod's game ids being valid.
    std::vector<size_t> game_indices;
    for (const auto& mod_game_id : manifest.mod_game_ids) {
        auto find_id_it = mod_game_ids.find(mod_game_id);
        if (find_id_it == mod_game_ids.end()) {
            error_param = mod_game_id;
            return ModOpenError::WrongGame;
        }
        game_indices.emplace_back(find_id_it->second);
    }
    
    // Scan for content types present in this mod.
    std::vector<ModContentTypeId> detected_content_types;

    auto scan_for_content_type = [&detected_content_types, &manifest](ModContentTypeId type_id, std::vector<ModContentType>& content_types) {
        const ModContentType& content_type = content_types[type_id.value];
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
            scan_for_content_type(ModContentTypeId{.value = content_type_index}, content_types);
        }
    }

    // Store the loaded mod manifest in a new mod handle.
    manifest.mod_root_path = mod_path;
    add_opened_mod(std::move(manifest), std::move(game_indices), std::move(detected_content_types));

    return ModOpenError::Good;
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
            return "Mod is missing a manifest.json";
        case ModOpenError::FailedToParseManifest:
            return "Failed to parse mod's manifest.json";
        case ModOpenError::InvalidManifestSchema:
            return "Mod's manifest.json has an invalid schema";
        case ModOpenError::UnrecognizedManifestField:
            return "Unrecognized field in manifest.json";
        case ModOpenError::IncorrectManifestFieldType:
            return "Incorrect type for field in manifest.json";
        case ModOpenError::InvalidVersionString:
            return "Invalid version string in manifest.json";
        case ModOpenError::InvalidMinimumRecompVersionString:
            return "Invalid minimum recomp version string in manifest.json";
        case ModOpenError::InvalidDependencyString:
            return "Invalid dependency string in manifest.json";
        case ModOpenError::MissingManifestField:
            return "Missing required field in manifest";
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
        case CodeModLoadError::FailedToFindReplacement:
            return "Failed to find replacement function";
        case CodeModLoadError::ReplacementConflict:
            return "Attempted to replace a function that cannot be replaced";
        case CodeModLoadError::ModConflict:
            return "Conflicts with other mod";
        case CodeModLoadError::DuplicateExport:
            return "Duplicate exports in mod";
        case CodeModLoadError::NoSpecifiedApiVersion:
            return "Mod DLL does not specify an API version";
        case CodeModLoadError::UnsupportedApiVersion:
            return "Mod DLL has an unsupported API version";
    }
}
