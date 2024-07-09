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

recomp::mods::ZipModHandle::ZipModHandle(const std::filesystem::path& mod_path, ModOpenError& error) {
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

recomp::mods::LooseModHandle::LooseModHandle(const std::filesystem::path& mod_path, ModOpenError& error) {
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

const std::string mod_id_key = "id";
const std::string major_version_key = "major_version";
const std::string minor_version_key = "minor_version";
const std::string patch_version_key = "patch_version";
const std::string binary_path_key = "binary";
const std::string binary_syms_path_key = "binary_syms";
const std::string rom_patch_path_key = "rom_patch";
const std::string rom_patch_syms_path_key = "rom_patch_syms";

std::unordered_map<std::string, ManifestField> field_map {
    { mod_id_key,              ManifestField::Id               },
    { major_version_key,       ManifestField::MajorVersion     },
    { minor_version_key,       ManifestField::MinorVersion     },
    { patch_version_key,       ManifestField::PatchVersion     },
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

bool parse_manifest(recomp::mods::ModManifest& ret, const std::vector<char>& manifest_data, recomp::mods::ModOpenError& error, std::string& error_param) {
    using json = nlohmann::json;
    json manifest_json = json::parse(manifest_data.begin(), manifest_data.end(), nullptr, false);

    if (manifest_json.is_discarded()) {
        error = recomp::mods::ModOpenError::FailedToParseManifest;
        return false;
    }

    if (!manifest_json.is_object()) {
        error = recomp::mods::ModOpenError::InvalidManifestSchema;
        return false;
    }

    for (const auto& [key, val] : manifest_json.items()) {
        const auto find_key_it = field_map.find(key);
        if (find_key_it == field_map.end()) {
            // Unrecognized field
            error = recomp::mods::ModOpenError::UnrecognizedManifestField;
            error_param = key;
            return false;
        }

        ManifestField field = find_key_it->second;
        switch (field) {
            case ManifestField::Id:
                if (!get_to<json::string_t>(val, ret.mod_id)) {
                    error = recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::MajorVersion:
                if (!get_to<json::number_unsigned_t>(val, ret.major_version)) {
                    error = recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::MinorVersion:
                if (!get_to<json::number_unsigned_t>(val, ret.minor_version)) {
                    error = recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::PatchVersion:
                if (!get_to<json::number_unsigned_t>(val, ret.patch_version)) {
                    error = recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::BinaryPath:
                if (!get_to<json::string_t>(val, ret.binary_path)) {
                    error = recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::BinarySymsPath:
                if (!get_to<json::string_t>(val, ret.binary_syms_path)) {
                    error = recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::RomPatchPath:
                if (!get_to<json::string_t>(val, ret.rom_patch_path)) {
                    error = recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
            case ManifestField::RomPatchSymsPath:
                if (!get_to<json::string_t>(val, ret.rom_patch_syms_path)) {
                    error = recomp::mods::ModOpenError::IncorrectManifestFieldType;
                    error_param = key;
                    return false;
                }
                break;
        }
    }

    return true;
}

bool validate_file_exists(const recomp::mods::ModManifest& manifest, const std::string& filepath, recomp::mods::ModOpenError& error, std::string& error_param) {
    // No file provided, so nothing to check for.
    if (filepath.empty()) {
        return true;
    }
    if (!manifest.mod_handle->file_exists(filepath)) {
        error = recomp::mods::ModOpenError::InnerFileDoesNotExist;
        error_param = filepath;
        return false;
    }
    return true;
}

bool validate_manifest(const recomp::mods::ModManifest& manifest, recomp::mods::ModOpenError& error, std::string& error_param) {
    using namespace recomp::mods;

    // Check for required fields.
    if (manifest.mod_id.empty()) {
        error = ModOpenError::MissingManifestField;
        error_param = mod_id_key;
        return false;
    }
    if (manifest.major_version == -1) {
        error = ModOpenError::MissingManifestField;
        error_param = major_version_key;
        return false;
    }
    if (manifest.minor_version == -1) {
        error = ModOpenError::MissingManifestField;
        error_param = minor_version_key;
        return false;
    }
    if (manifest.patch_version == -1) {
        error = ModOpenError::MissingManifestField;
        error_param = patch_version_key;
        return false;
    }

    // If either a binary file or binary symbol file is provided, the other must be as well.
    if (manifest.binary_path.empty() != manifest.binary_syms_path.empty()) {
        error = ModOpenError::MissingManifestField;
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
        error = ModOpenError::MissingManifestField;
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

recomp::mods::ModManifest recomp::mods::open_mod(const std::filesystem::path& mod_path, ModOpenError& error, std::string& error_param) {
    ModManifest ret{};
    std::error_code ec;
    error_param = "";

    if (!std::filesystem::exists(mod_path, ec) || ec) {
        error = ModOpenError::DoesNotExist;
        return {};
    }

    // TODO support symlinks?
    bool is_file = std::filesystem::is_regular_file(mod_path, ec);
    if (ec) {
        error = ModOpenError::FileError;
        return {};
    }

    bool is_directory = std::filesystem::is_directory(mod_path, ec);
    if (ec) {
        error = ModOpenError::FileError;
        return {};
    }

    // Load the directory or zip file.
    ModOpenError handle_error;
    if (is_file) {
        ret.mod_handle = std::make_unique<recomp::mods::ZipModHandle>(mod_path, handle_error);
    }
    else if (is_directory) {
        ret.mod_handle = std::make_unique<recomp::mods::LooseModHandle>(mod_path, handle_error);
    }
    else {
        error = ModOpenError::NotAFileOrFolder;
        return {};
    }

    if (handle_error != ModOpenError::Good) {
        error = handle_error;
        return {};
    }

    {
        bool exists;
        std::vector<char> manifest_data = ret.mod_handle->read_file("manifest.json", exists);
        if (!exists) {
            error = ModOpenError::NoManifest;
            return {};
        }

        if (!parse_manifest(ret, manifest_data, error, error_param)) {
            return {};
        }
    }

    if (!validate_manifest(ret, error, error_param)) {
        return {};
    }

    ret.mod_root_path = mod_path;

    // Return the loaded mod manifest
    error = ModOpenError::Good;
    return ret;
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
    }
    return "Unknown error " + std::to_string((int)error);
}
