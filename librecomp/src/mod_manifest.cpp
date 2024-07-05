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

recomp::mods::ZipModHandle::ZipModHandle(ZipModHandle&& rhs) {
    *this = std::move(rhs);
}

recomp::mods::ZipModHandle& recomp::mods::ZipModHandle::operator=(ZipModHandle&& rhs) {
    if (file_handle) {
        fclose(file_handle);
    }
    file_handle = rhs.file_handle;
    rhs.file_handle = nullptr;

    mz_zip_reader_end(archive.get());
    archive = std::move(rhs.archive);

    return *this;
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

std::vector<char> recomp::mods::ZipModHandle::read_file(const std::string& filename, bool& exists) {
    std::vector<char> ret{};
    
    mz_uint32 file_index;
    if (!mz_zip_reader_locate_file_v2(archive.get(), filename.c_str(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE, &file_index)) {
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

std::unordered_map<std::string, ManifestField> field_map {
    { "id",             ManifestField::Id               },
    { "major_version",  ManifestField::MajorVersion     },
    { "minor_version",  ManifestField::MinorVersion     },
    { "patch_version",  ManifestField::PatchVersion     },
    { "binary",         ManifestField::BinaryPath       },
    { "binary_syms",    ManifestField::BinarySymsPath   },
    { "rom_patch",      ManifestField::RomPatchPath     },
    { "rom_patch_syms", ManifestField::RomPatchSymsPath },
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
    json manifest_json = json::parse(manifest_data.begin(), manifest_data.end(), false);

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

recomp::mods::ModManifest recomp::mods::load_mod(const std::filesystem::path& mod_path, ModLoadError& error, std::string& error_param) {
    ModManifest ret{};
    std::error_code ec;
    error_param = "";

    if (!std::filesystem::exists(mod_path, ec) || ec) {
        error = ModLoadError::DoesNotExist;
        return {};
    }

    // TODO support symlinks?
    if (!std::filesystem::is_regular_file(mod_path, ec) || ec) {
        error = ModLoadError::NotAFile;
        return {};
    }

    // Load the zip file.
    ModLoadError zip_error;
    ret.mod_handle = recomp::mods::ZipModHandle(mod_path, zip_error);

    if (zip_error != ModLoadError::Good) {
        error = zip_error;
        return {};
    }

    {
        bool exists;
        std::vector<char> manifest_data = ret.mod_handle.read_file("manifest.json", exists);
        if (!exists) {
            error = ModLoadError::NoManifest;
            return {};
        }

        if (!parse_manifest(ret, manifest_data, error, error_param)) {
            return {};
        }
    }

    // Return the loaded mod manifest
    error = ModLoadError::Good;
    return ret;
}
