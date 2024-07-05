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
        enum class ModOpenError {
            Good,
            DoesNotExist,
            NotAFileOrFolder,
            FileError,
            InvalidZip,
            NoManifest,
            FailedToParseManifest,
            InvalidManifestSchema,
            UnrecognizedManifestField,
            IncorrectManifestFieldType,
            MissingManifestField,
            InnerFileDoesNotExist
        };

        struct ModHandle {
            virtual ~ModHandle() = default;
            virtual std::vector<char> read_file(const std::string& filepath, bool& exists) const = 0;
            virtual bool file_exists(const std::string& filepath) const = 0;
        };

        struct ZipModHandle : public ModHandle {
            FILE* file_handle = nullptr;
            std::unique_ptr<mz_zip_archive> archive;

            ZipModHandle() = default;
            ZipModHandle(const std::filesystem::path& mod_path, ModOpenError& error);
            ~ZipModHandle() final;

            std::vector<char> read_file(const std::string& filepath, bool& exists) const final;
            bool file_exists(const std::string& filepath) const final;
        };

        struct LooseModHandle : public ModHandle {
            std::filesystem::path root_path;

            LooseModHandle() = default;
            LooseModHandle(const std::filesystem::path& mod_path, ModOpenError& error);
            ~LooseModHandle() final;

            std::vector<char> read_file(const std::string& filepath, bool& exists) const final;
            bool file_exists(const std::string& filepath) const final;
        };

        struct ModManifest {
            std::filesystem::path mod_root_path;

            std::string mod_id;

            int major_version = -1;
            int minor_version = -1;
            int patch_version = -1;

            // These are all relative to the base path for loose mods or inside the zip for zipped mods.
            std::string binary_path;
            std::string binary_syms_path;
            std::string rom_patch_path;
            std::string rom_patch_syms_path;

            std::unique_ptr<ModHandle> mod_handle;
        };

        ModManifest open_mod(const std::filesystem::path& mod_path, ModOpenError& error, std::string& error_string);
        bool load_mod_(uint8_t* rdram, int32_t target_vram, const std::filesystem::path& symbol_file, const std::filesystem::path& binary_file);

        std::string error_to_string(ModOpenError);
    }
};

#endif
