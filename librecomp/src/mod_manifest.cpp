#include <unordered_map>

#include "json/json.hpp"

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
    file_handle = fopen(mod_path.c_str(), L"rb");
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
    Id,
    MajorVersion,
    MinorVersion,
    PatchVersion,
    BinaryPath,
    BinarySymsPath,
    RomPatchPath,
    RomPatchSymsPath,
    NativeLibraryPaths,
};

const std::string mod_id_key = "id";
const std::string major_version_key = "major_version";
const std::string minor_version_key = "minor_version";
const std::string patch_version_key = "patch_version";
const std::string binary_path_key = "binary";
const std::string binary_syms_path_key = "binary_syms";
const std::string rom_patch_path_key = "rom_patch";
const std::string rom_patch_syms_path_key = "rom_patch_syms";
const std::string native_library_paths_key = "native_libraries";

std::unordered_map<std::string, ManifestField> field_map {
    { mod_id_key,               ManifestField::Id                 },
    { major_version_key,        ManifestField::MajorVersion       },
    { minor_version_key,        ManifestField::MinorVersion       },
    { patch_version_key,        ManifestField::PatchVersion       },
    { binary_path_key,          ManifestField::BinaryPath         },
    { binary_syms_path_key,     ManifestField::BinarySymsPath     },
    { rom_patch_path_key,       ManifestField::RomPatchPath       },
    { rom_patch_syms_path_key,  ManifestField::RomPatchSymsPath   },
    { native_library_paths_key, ManifestField::NativeLibraryPaths },
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
            case ManifestField::Id:
                if (!get_to<json::string_t>(val, ret.mod_id)) {
                    error_param = key;
                    return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                }
                break;
            case ManifestField::MajorVersion:
                if (!get_to<json::number_unsigned_t>(val, ret.major_version)) {
                    error_param = key;
                    return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                }
                break;
            case ManifestField::MinorVersion:
                if (!get_to<json::number_unsigned_t>(val, ret.minor_version)) {
                    error_param = key;
                    return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                }
                break;
            case ManifestField::PatchVersion:
                if (!get_to<json::number_unsigned_t>(val, ret.patch_version)) {
                    error_param = key;
                    return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                }
                break;
            case ManifestField::BinaryPath:
                if (!get_to<json::string_t>(val, ret.binary_path)) {
                    error_param = key;
                    return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                }
                break;
            case ManifestField::BinarySymsPath:
                if (!get_to<json::string_t>(val, ret.binary_syms_path)) {
                    error_param = key;
                    return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                }
                break;
            case ManifestField::RomPatchPath:
                if (!get_to<json::string_t>(val, ret.rom_patch_path)) {
                    error_param = key;
                    return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                }
                break;
            case ManifestField::RomPatchSymsPath:
                if (!get_to<json::string_t>(val, ret.rom_patch_syms_path)) {
                    error_param = key;
                    return recomp::mods::ModOpenError::IncorrectManifestFieldType;
                }
                break;
            case ManifestField::NativeLibraryPaths:
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

recomp::mods::ModOpenError validate_file_exists(const recomp::mods::ModManifest& manifest, const std::string& filepath, std::string& error_param) {
    // No file provided, so nothing to check for.
    if (filepath.empty()) {
        return recomp::mods::ModOpenError::Good;
    }
    if (!manifest.file_handle->file_exists(filepath)) {
        error_param = filepath;
        return recomp::mods::ModOpenError::InnerFileDoesNotExist;
    }
    return recomp::mods::ModOpenError::Good;
}

recomp::mods::ModOpenError validate_manifest(const recomp::mods::ModManifest& manifest, std::string& error_param) {
    using namespace recomp::mods;

    // Check for required fields.
    if (manifest.mod_id.empty()) {
        error_param = mod_id_key;
        return ModOpenError::MissingManifestField;
    }
    if (manifest.major_version == -1) {
        error_param = major_version_key;
        return ModOpenError::MissingManifestField;
    }
    if (manifest.minor_version == -1) {
        error_param = minor_version_key;
        return ModOpenError::MissingManifestField;
    }
    if (manifest.patch_version == -1) {
        error_param = patch_version_key;
        return ModOpenError::MissingManifestField;
    }

    // If either a binary file or binary symbol file is provided, the other must be as well.
    if (manifest.binary_path.empty() != manifest.binary_syms_path.empty()) {
        if (manifest.binary_path.empty()) {
            error_param = binary_path_key;
        }
        else {
            error_param = binary_syms_path_key;
        }
        return ModOpenError::MissingManifestField;
    }

    // If a ROM patch symbol file is provided, a ROM patch file must be as well.
    if (!manifest.rom_patch_syms_path.empty() && manifest.rom_patch_path.empty()) {
        error_param = rom_patch_path_key;
        return ModOpenError::MissingManifestField;
    }

    // Validate that provided files exist.
    ModOpenError validate_error;
    if ((validate_error = validate_file_exists(manifest, manifest.binary_path, error_param)) != ModOpenError::Good) {
        return validate_error;
    }
    if ((validate_error = validate_file_exists(manifest, manifest.binary_syms_path, error_param)) != ModOpenError::Good) {
        return validate_error;
    }
    if ((validate_error = validate_file_exists(manifest, manifest.rom_patch_path, error_param)) != ModOpenError::Good) {
        return validate_error;
    }
    if ((validate_error = validate_file_exists(manifest, manifest.rom_patch_syms_path, error_param)) != ModOpenError::Good) {
        return validate_error;
    }

    return ModOpenError::Good;
}   

recomp::mods::ModOpenError recomp::mods::ModContext::open_mod(const std::filesystem::path& mod_path, std::string& error_param) {
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
            return ModOpenError::NoManifest;;
        }

        ModOpenError parse_error = parse_manifest(manifest, manifest_data, error_param);
        if (parse_error != ModOpenError::Good) {
            return parse_error;
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

    // Store the loaded mod manifest in a new mod handle.
    manifest.mod_root_path = mod_path;
    add_opened_mod(std::move(manifest));

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
        case ModOpenError::MissingManifestField:
            return "Missing required field in manifest";
        case ModOpenError::InnerFileDoesNotExist:
            return "File inside mod does not exist";
        case ModOpenError::DuplicateMod:
            return "Duplicate mod found";
    }
    return "Unknown mod opening error: " + std::to_string((int)error);
}

std::string recomp::mods::error_to_string(ModLoadError error) {
    switch (error) {
        case ModLoadError::Good:
            return "Good";
        case ModLoadError::FailedToLoadSyms:
            return "Failed to load mod symbol file";
        case ModLoadError::FailedToLoadBinary:
            return "Failed to load mod binary file";
        case ModLoadError::FailedToLoadNativeCode:
            return "Failed to load mod code DLL";
        case ModLoadError::FailedToLoadNativeLibrary:
            return "Failed to load mod library DLL";
        case ModLoadError::FailedToFindNativeExport:
            return "Failed to find native export";
        case ModLoadError::InvalidReferenceSymbol:
            return "Reference symbol does not exist";
        case ModLoadError::InvalidImport:
            return "Imported function not found";
        case ModLoadError::InvalidCallbackEvent:
            return "Event for callback not found";
        case ModLoadError::InvalidFunctionReplacement:
            return "Function to be replaced does not exist";
        case ModLoadError::FailedToFindReplacement:
            return "Failed to find replacement function";
        case ModLoadError::ReplacementConflict:
            return "Attempted to replace a function that cannot be replaced";
        case ModLoadError::MissingDependency:
            return "Missing dependency";
        case ModLoadError::WrongDependencyVersion:
            return "Wrong dependency version";
        case ModLoadError::ModConflict:
            return "Conflicts with other mod";
        case ModLoadError::DuplicateExport:
            return "Duplicate exports in mod";
        case ModLoadError::NoSpecifiedApiVersion:
            return "Mod DLL does not specify an API version";
        case ModLoadError::UnsupportedApiVersion:
            return "Mod DLL has an unsupported API version";
    }
    return "Unknown mod loading error " + std::to_string((int)error);
}
