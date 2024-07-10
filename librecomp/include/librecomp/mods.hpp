#ifndef __RECOMP_MODS_HPP__
#define __RECOMP_MODS_HPP__

#include <filesystem>
#include <string>
#include <fstream>
#include <cstdio>
#include <vector>
#include <memory>
#include <tuple>
#include <unordered_set>

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
            InnerFileDoesNotExist,
            DuplicateMod
        };

        std::string error_to_string(ModOpenError);

        enum class ModLoadError {
            Good,
            FailedToLoadSyms,
            FailedToLoadBinary,
            InvalidFunctionReplacement,
            FailedToFindReplacement,
        };

        struct ModFileHandle {
            virtual ~ModFileHandle() = default;
            virtual std::vector<char> read_file(const std::string& filepath, bool& exists) const = 0;
            virtual bool file_exists(const std::string& filepath) const = 0;
        };

        struct ZipModFileHandle : public ModFileHandle {
            FILE* file_handle = nullptr;
            std::unique_ptr<mz_zip_archive> archive;

            ZipModFileHandle() = default;
            ZipModFileHandle(const std::filesystem::path& mod_path, ModOpenError& error);
            ~ZipModFileHandle() final;

            std::vector<char> read_file(const std::string& filepath, bool& exists) const final;
            bool file_exists(const std::string& filepath) const final;
        };

        struct LooseModFileHandle : public ModFileHandle {
            std::filesystem::path root_path;

            LooseModFileHandle() = default;
            LooseModFileHandle(const std::filesystem::path& mod_path, ModOpenError& error);
            ~LooseModFileHandle() final;

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

            std::unique_ptr<ModFileHandle> file_handle;
        };

        struct ModOpenErrorDetails {
            std::filesystem::path mod_path;
            ModOpenError error;
            std::string error_param;
        };

        struct ModLoadErrorDetails {
            std::string mod_id;
            ModLoadError error;
            std::string error_param;
        };
        
        std::vector<ModOpenErrorDetails> scan_mod_folder(const std::u8string& game_id, const std::filesystem::path& mod_folder);
        void enable_mod(const std::u8string& game_id, const std::string& mod_id, bool enabled);
        bool is_mod_enabled(const std::u8string& game_id, const std::string& mod_id);
        size_t num_opened_mods(const std::u8string& game_id);

        // Internal functions, TODO move to an internal header.
        struct ModHandle;
        class ModContext {
        public:
            ModContext();
            ~ModContext();

            std::vector<ModOpenErrorDetails> scan_mod_folder(const std::filesystem::path& mod_folder);
            void enable_mod(const std::string& mod_id, bool enabled);
            bool is_mod_enabled(const std::string& mod_id);
            size_t num_opened_mods();
            std::vector<ModLoadErrorDetails> load_mods(uint8_t* rdram, int32_t load_address, uint32_t& ram_used);
            // const ModManifest& get_mod_manifest(size_t mod_index);
        private:
            ModOpenError open_mod(const std::filesystem::path& mod_path, std::string& error_param);
            ModLoadError load_mod(uint8_t* rdram, ModHandle& mod, int32_t load_address, uint32_t& ram_used, std::string& error_param);
            void add_opened_mod(ModManifest&& manifest);

            std::vector<ModHandle> opened_mods;
            std::unordered_set<std::string> mod_ids;
            std::unordered_set<std::string> enabled_mods;
        };
    }
};

#endif
