#include <unordered_map>

#include "json/json.hpp"

#include "librecomp/mods.hpp"

recomp::mods::ZipModHandle::~ZipModHandle() {
    if (file_handle) {
        fclose(file_handle);
        file_handle = nullptr;
    }

    if (archive) {
        mz_zip_reader_end(archive.get());
    }
    archive = {};
}

recomp::mods::ZipModHandle::ZipModHandle(const std::filesystem::path& mod_path, ModLoadError& error) {
#ifdef _WIN32
    if (_wfopen_s(&file_handle, mod_path.c_str(), L"rb") != 0) {
        error = ModLoadError::FileError;
        return;
    }
#else
    file_handle = fopen(mod_path.c_str(), L"rb");
    if (!file_handle) {
        error = ModLoadError::FileError;
        return;
    }
#endif
    archive = std::make_unique<mz_zip_archive>();
    if (!mz_zip_reader_init_cfile(archive.get(), file_handle, 0, 0)) {
        error = ModLoadError::InvalidZip;
        return;
    }

    error = ModLoadError::Good;
}

std::vector<char> recomp::mods::ZipModHandle::read_file(const std::string& filepath, bool& exists) const {
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

bool recomp::mods::ZipModHandle::file_exists(const std::string& filepath) const {
    mz_uint32 file_index;
    if (!mz_zip_reader_locate_file_v2(archive.get(), filepath.c_str(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE, &file_index)) {
        return false;
    }

    return true;
}

recomp::mods::LooseModHandle::~LooseModHandle() {
    // Nothing to do here, members will be destroyed automatically.
}

recomp::mods::LooseModHandle::LooseModHandle(const std::filesystem::path& mod_path, ModLoadError& error) {
    root_path = mod_path;

    std::error_code ec;
    if (!std::filesystem::is_directory(root_path, ec)) {
        error = ModLoadError::NotAFileOrFolder;
    }

    if (ec) {
        error = ModLoadError::FileError;
    }

    error = ModLoadError::Good;
}

std::vector<char> recomp::mods::LooseModHandle::read_file(const std::string& filepath, bool& exists) const {
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

bool recomp::mods::LooseModHandle::file_exists(const std::string& filepath) const {
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
    Invalid,
};

const std::string binary_path_key = "binary";
const std::string binary_syms_path_key = "binary_syms";
const std::string rom_patch_path_key = "rom_patch";
const std::string rom_patch_syms_path_key = "rom_patch_syms";

std::unordered_map<std::string, ManifestField> field_map {
    { "id",                    ManifestField::Id               },
    { "major_version",         ManifestField::MajorVersion     },
    { "minor_version",         ManifestField::MinorVersion     },
    { "patch_version",         ManifestField::PatchVersion     },
    { binary_path_key,         ManifestField::BinaryPath       },
    { binary_syms_path_key,    ManifestField::BinarySymsPath   },
    { rom_patch_path_key,      ManifestField::RomPatchPath     },
    { rom_patch_syms_path_key, ManifestField::RomPatchSymsPath },
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

bool parse_manifest(recomp::mods::ModManifest& ret, const std::vector<char>& manifest_data, recomp::mods::ModLoadError& error, std::string& error_param) {
    using json = nlohmann::json;
    json manifest_json = json::parse(manifest_data.begin(), manifest_data.end(), nullptr, false);

    if (manifest_json.is_discarded()) {
        error = recomp::mods::ModLoadError::FailedToParseManifest;
        return false;
    }

    if (!manifest_json.is_object()) {
        error = recomp::mods::ModLoadError::InvalidManifestSchema;
        return false;
    }

    for (const auto& [key, val] : manifest_json.items()) {
        const auto find_key_it = field_map.find(key);
        if (find_key_it == field_map.end()) {
            // Unrecognized field
            error = recomp::mods::ModLoadError::UnrecognizedManifestField;
            error_param = key;
            return false;
        }

        ManifestField field = find_key_it->second;
        switch (field) {
            case ManifestField::Id:
                if (!get_to<json::string_t>(val, ret.mod_id)) {
                    error = recomp::mods::ModLoadError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::MajorVersion:
                if (!get_to<json::number_unsigned_t>(val, ret.major_version)) {
                    error = recomp::mods::ModLoadError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::MinorVersion:
                if (!get_to<json::number_unsigned_t>(val, ret.minor_version)) {
                    error = recomp::mods::ModLoadError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::PatchVersion:
                if (!get_to<json::number_unsigned_t>(val, ret.patch_version)) {
                    error = recomp::mods::ModLoadError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::BinaryPath:
                if (!get_to<json::string_t>(val, ret.binary_path)) {
                    error = recomp::mods::ModLoadError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::BinarySymsPath:
                if (!get_to<json::string_t>(val, ret.binary_syms_path)) {
                    error = recomp::mods::ModLoadError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::RomPatchPath:
                if (!get_to<json::string_t>(val, ret.rom_patch_path)) {
                    error = recomp::mods::ModLoadError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::RomPatchSymsPath:
                if (!get_to<json::string_t>(val, ret.rom_patch_syms_path)) {
                    error = recomp::mods::ModLoadError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
        }
    }

    return true;
}

bool validate_file_exists(const recomp::mods::ModManifest& manifest, const std::string& filepath, recomp::mods::ModLoadError& error, std::string& error_param) {
    // No file provided, so nothing to check for.
    if (filepath.empty()) {
        return true;
    }
    if (!manifest.mod_handle->file_exists(filepath)) {
        error = recomp::mods::ModLoadError::InnerFileDoesNotExist;
        error_param = filepath;
        return false;
    }
    return true;
}

bool validate_manifest(const recomp::mods::ModManifest& manifest, recomp::mods::ModLoadError& error, std::string& error_param) {
    using namespace recomp::mods;
    // If either a binary file or binary symbol file is provided, the other must be as well.
    if (manifest.binary_path.empty() != manifest.binary_syms_path.empty()) {
        error = ModLoadError::MissingManifestField;
        if (manifest.binary_path.empty()) {
            error_param = binary_path_key;
        }
        else {
            error_param = binary_syms_path_key;
        }
        return false;
    }

    // If a ROM patch symbol file is provided, a ROM patch file must be as well.
    if (!manifest.rom_patch_syms_path.empty() && manifest.rom_patch_path.empty()) {
        error = ModLoadError::MissingManifestField;
        error_param = rom_patch_path_key;
        return false;
    }

    // Validate that provided files exist.
    if (!validate_file_exists(manifest, manifest.binary_path, error, error_param)) {
        return false;
    }
    if (!validate_file_exists(manifest, manifest.binary_syms_path, error, error_param)) {
        return false;
    }
    if (!validate_file_exists(manifest, manifest.rom_patch_path, error, error_param)) {
        return false;
    }
    if (!validate_file_exists(manifest, manifest.rom_patch_syms_path, error, error_param)) {
        return false;
    }

    return true;
}   

recomp::mods::ModManifest recomp::mods::load_mod(const std::filesystem::path& mod_path, ModLoadError& error, std::string& error_param) {
    ModManifest ret{};
    std::error_code ec;
    error_param = "";

    if (!std::filesystem::exists(mod_path, ec) || ec) {
        error = ModLoadError::DoesNotExist;
        return {};
    }

    // TODO support symlinks?
    bool is_file = std::filesystem::is_regular_file(mod_path, ec);
    if (ec) {
        error = ModLoadError::FileError;
        return {};
    }

    bool is_directory = std::filesystem::is_directory(mod_path, ec);
    if (ec) {
        error = ModLoadError::FileError;
        return {};
    }

    // Load the directory or zip file.
    ModLoadError handle_error;
    if (is_file) {
        ret.mod_handle = std::make_unique<recomp::mods::ZipModHandle>(mod_path, handle_error);
    }
    else if (is_directory) {
        ret.mod_handle = std::make_unique<recomp::mods::LooseModHandle>(mod_path, handle_error);
    }
    else {
        error = ModLoadError::NotAFileOrFolder;
        return {};
    }

    if (handle_error != ModLoadError::Good) {
        error = handle_error;
        return {};
    }

    {
        bool exists;
        std::vector<char> manifest_data = ret.mod_handle->read_file("manifest.json", exists);
        if (!exists) {
            error = ModLoadError::NoManifest;
            return {};
        }

        if (!parse_manifest(ret, manifest_data, error, error_param)) {
            return {};
        }
    }

    if (!validate_manifest(ret, error, error_param)) {
        return {};
    }

    // Return the loaded mod manifest
    error = ModLoadError::Good;
    return ret;
}

std::string recomp::mods::error_to_string(ModLoadError error) {
    switch (error) {
        case ModLoadError::Good:
            return "Good";
        case ModLoadError::DoesNotExist:
            return "Mod does not exist";
        case ModLoadError::NotAFileOrFolder:
            return "Mod is not a file or folder";
        case ModLoadError::FileError:
            return "Error reading mod file(s)";
        case ModLoadError::InvalidZip:
            return "Mod is an invalid zip file";
        case ModLoadError::NoManifest:
            return "Mod is missing a manifest.json";
        case ModLoadError::FailedToParseManifest:
            return "Failed to parse mod's manifest.json";
        case ModLoadError::InvalidManifestSchema:
            return "Mod's manifest.json has an invalid schema";
        case ModLoadError::UnrecognizedManifestField:
            return "Unrecognized field in manifest.json";
        case ModLoadError::IncorrectManifestFieldType:
            return "Incorrect type for field in manifest.json";
        case ModLoadError::MissingManifestField:
            return "Missing required field in manifest";
        case ModLoadError::InnerFileDoesNotExist:
            return "File inside mod does not exist";
    }
    return "Unknown error " + std::to_string((int)error);
}
