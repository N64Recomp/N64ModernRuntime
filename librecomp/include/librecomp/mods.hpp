#ifndef __RECOMP_MODS_HPP__
#define __RECOMP_MODS_HPP__

#include <filesystem>
#include <string>
#include <fstream>
#include <cstdio>
#include <vector>
#include <memory>

#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#include "miniz.h"
#include "miniz_zip.h"

namespace recomp {
    namespace mods {
        enum class ModLoadError {
            Good,
            DoesNotExist,
            NotAFile,
            FileError,
            InvalidZip,
            NoManifest,
            InvalidManifest,
        };

        struct ZipModHandle {
            FILE* file_handle = nullptr;
            std::unique_ptr<mz_zip_archive> archive;

            ZipModHandle() = default;
            ZipModHandle(const std::filesystem::path& mod_path, ModLoadError& error);
            ZipModHandle(const ZipModHandle& rhs) = delete;
            ZipModHandle& operator=(const ZipModHandle& rhs) = delete;
            ZipModHandle(ZipModHandle&& rhs);
            ZipModHandle& operator=(ZipModHandle&& rhs);
            ~ZipModHandle();

            std::vector<char> read_file(const std::string& filename, bool& exists);
        };

        struct ModManifest {
            std::filesystem::path mod_root_path;

            std::string mod_id;

            int major_version;
            int minor_version;
            int patch_version;

            // These are all relative to the base path for loose mods or inside the zip for zipped mods.
            std::string binary_path;
            std::string binary_syms_path;
            std::string rom_patch_path;
            std::string rom_patch_syms_path;

            ZipModHandle mod_handle;
        };

        ModManifest load_mod(const std::filesystem::path& mod_path, ModLoadError& error);
    }
};

#endif
