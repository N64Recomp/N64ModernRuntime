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
#include <variant>

#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#include "miniz.h"
#include "miniz_zip.h"

#include "librecomp/recomp.h"
#include "librecomp/sections.h"

namespace N64Recomp {
    class Context;
};

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

        using GenericFunction = std::variant<recomp_func_t*>;

        class ModHandle;
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
            // // Maps (mod id, export name) to (mod index, function index).
            // std::unordered_map<std::pair<std::string, std::string>, std::pair<size_t, size_t>> mod_exports;
            // // Maps (mod id, event name) to a vector of callback functions attached to that event.
            // std::unordered_map<std::pair<std::string, std::string>, std::vector<GenericFunction>> callbacks;
        };

        class ModCodeHandle {
        public:
            virtual ~ModCodeHandle() {}
            virtual bool good() = 0;
            virtual void set_imported_function(size_t import_index, GenericFunction func) = 0;
            virtual void set_reference_symbol_pointer(size_t symbol_index, recomp_func_t* ptr) = 0;
            virtual void set_event_index(size_t local_event_index, uint32_t global_event_index) = 0;
            virtual void set_recomp_trigger_event_pointer(void (*ptr)(uint8_t* rdram, recomp_context* ctx, uint32_t index)) = 0;
            virtual void set_get_function_pointer(recomp_func_t* (*ptr)(int32_t)) = 0;
            virtual void set_reference_section_addresses_pointer(int32_t* ptr) = 0;
            virtual void set_local_section_address(size_t section_index, int32_t address) = 0;
            virtual GenericFunction get_function_handle(size_t func_index) = 0;
        };

        class ModHandle {
        public:
            // TODO make these private and expose methods for the functionality they're currently used in.
            ModManifest manifest;
            std::unique_ptr<ModCodeHandle> code_handle;
            std::unique_ptr<N64Recomp::Context> recompiler_context;
            std::vector<uint32_t> section_load_addresses;

            ModHandle(ModManifest&& manifest);
            ModHandle(const ModHandle& rhs) = delete;
            ModHandle& operator=(const ModHandle& rhs) = delete;
            ModHandle(ModHandle&& rhs);
            ModHandle& operator=(ModHandle&& rhs);
            ~ModHandle();

            size_t num_exports() const;
            size_t num_events() const;

            ModLoadError populate_exports(std::string& error_param);
            bool get_export_function(const std::string& export_name, GenericFunction& out) const;
        private:
            // Mapping of export name to function index.
            std::unordered_map<std::string, size_t> exports_by_name;
            // List of global event indices ordered by the event's local index.
            std::vector<size_t> global_event_indices;
        };

        class DynamicLibrary;
        class NativeCodeHandle : public ModCodeHandle {
        public:
            NativeCodeHandle(const std::filesystem::path& dll_path, const N64Recomp::Context& context);
            ~NativeCodeHandle() = default;
            bool good() final;
            void set_imported_function(size_t import_index, GenericFunction func) final;
            void set_reference_symbol_pointer(size_t symbol_index, recomp_func_t* ptr) final {
                reference_symbol_funcs[symbol_index] = ptr;
            };
            void set_event_index(size_t local_event_index, uint32_t global_event_index) final {
                event_indices[local_event_index] = global_event_index;
            };
            void set_recomp_trigger_event_pointer(void (*ptr)(uint8_t* rdram, recomp_context* ctx, uint32_t index)) final {
                *recomp_trigger_event = ptr;
            };
            void set_get_function_pointer(recomp_func_t* (*ptr)(int32_t)) final {
                *get_function = ptr;
            };
            void set_reference_section_addresses_pointer(int32_t* ptr) final {
                *reference_section_addresses = ptr;
            };
            void set_local_section_address(size_t section_index, int32_t address) final {
                section_addresses[section_index] = address;
            };
            GenericFunction get_function_handle(size_t func_index) final {
                return GenericFunction{ functions[func_index] };
            }
        private:
            void set_bad();
            bool is_good;
            std::unique_ptr<DynamicLibrary> dynamic_lib;
            std::vector<recomp_func_t*> functions;
            recomp_func_t** imported_funcs;
            recomp_func_t** reference_symbol_funcs;
            uint32_t* event_indices;
            void (**recomp_trigger_event)(uint8_t* rdram, recomp_context* ctx, uint32_t index);
            recomp_func_t* (**get_function)(int32_t vram);
            int32_t** reference_section_addresses;
            int32_t* section_addresses;
        };

    }
};

#endif
