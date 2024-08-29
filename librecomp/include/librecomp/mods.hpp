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
#include <unordered_map>
#include <array>
#include <cstddef>

#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#include "miniz.h"
#include "miniz_zip.h"

#include "librecomp/recomp.h"
#include "librecomp/sections.h"

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
            FailedToLoadNativeCode,
            InvalidReferenceSymbol,
            InvalidImport,
            InvalidFunctionReplacement,
            FailedToFindReplacement,
            ReplacementConflict,
            MissingDependency,
            WrongDependencyVersion,
            ModConflict,
        };

        std::string error_to_string(ModLoadError);

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
        struct PatchData {
            std::array<std::byte, 16> replaced_bytes;
            std::string mod_id;
        };

        struct ModHandle;
        class ModContext {
        public:
            ModContext();
            ~ModContext();

            void setup_sections();
            std::vector<ModOpenErrorDetails> scan_mod_folder(const std::filesystem::path& mod_folder);
            void enable_mod(const std::string& mod_id, bool enabled);
            bool is_mod_enabled(const std::string& mod_id);
            size_t num_opened_mods();
            std::vector<ModLoadErrorDetails> load_mods(uint8_t* rdram, int32_t load_address, uint32_t& ram_used);
            void unload_mods();
            // const ModManifest& get_mod_manifest(size_t mod_index);
        private:
            ModOpenError open_mod(const std::filesystem::path& mod_path, std::string& error_param);
            ModLoadError load_mod(uint8_t* rdram, const std::unordered_map<uint32_t, uint16_t>& section_map, recomp::mods::ModHandle& handle, int32_t load_address, uint32_t& ram_used, std::string& error_param);
            void check_dependencies(recomp::mods::ModHandle& mod, std::vector<std::pair<recomp::mods::ModLoadError, std::string>>& errors);
            ModLoadError load_mod_code(recomp::mods::ModHandle& mod, std::string& error_param);
            ModLoadError resolve_dependencies(recomp::mods::ModHandle& mod, std::string& error_param);
            void add_opened_mod(ModManifest&& manifest);

            std::vector<ModHandle> opened_mods;
            std::unordered_set<std::string> mod_ids;
            std::unordered_set<std::string> enabled_mods;
            std::unordered_map<recomp_func_t*, PatchData> patched_funcs;
            std::unordered_map<std::string, size_t> loaded_mods_by_id;
        };
    }
};

#endif
