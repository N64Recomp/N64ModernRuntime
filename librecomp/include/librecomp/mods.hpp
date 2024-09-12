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

#include "librecomp/game.hpp"
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
            InvalidVersionString,
            InvalidMinimumRecompVersionString,
            InvalidDependencyString,
            MissingManifestField,
            DuplicateMod,
            WrongGame
        };

        std::string error_to_string(ModOpenError);

        enum class ModLoadError {
            Good,
            InvalidGame,
            MinimumRecompVersionNotMet,
            HasSymsButNoBinary,
            HasBinaryButNoSyms,
            FailedToParseSyms,
            FailedToLoadNativeCode,
            FailedToLoadNativeLibrary,
            FailedToFindNativeExport,
            InvalidReferenceSymbol,
            InvalidImport,
            InvalidCallbackEvent,
            InvalidFunctionReplacement,
            FailedToFindReplacement,
            ReplacementConflict,
            MissingDependencyInManifest,
            MissingDependency,
            WrongDependencyVersion,
            ModConflict,
            DuplicateExport,
            NoSpecifiedApiVersion,
            UnsupportedApiVersion,
        };

        std::string error_to_string(ModLoadError);

        struct ModFileHandle {
            virtual ~ModFileHandle() = default;
            virtual std::vector<char> read_file(const std::string& filepath, bool& exists) const = 0;
            virtual bool file_exists(const std::string& filepath) const = 0;
        };

        struct ZipModFileHandle final : public ModFileHandle {
            FILE* file_handle = nullptr;
            std::unique_ptr<mz_zip_archive> archive;

            ZipModFileHandle() = default;
            ZipModFileHandle(const std::filesystem::path& mod_path, ModOpenError& error);
            ~ZipModFileHandle() final;

            std::vector<char> read_file(const std::string& filepath, bool& exists) const final;
            bool file_exists(const std::string& filepath) const final;
        };

        struct LooseModFileHandle final : public ModFileHandle {
            std::filesystem::path root_path;

            LooseModFileHandle() = default;
            LooseModFileHandle(const std::filesystem::path& mod_path, ModOpenError& error);
            ~LooseModFileHandle() final;

            std::vector<char> read_file(const std::string& filepath, bool& exists) const final;
            bool file_exists(const std::string& filepath) const final;
        };

        struct NativeLibraryManifest {
            std::string name;
            std::vector<std::string> exports;
        };

        struct Dependency {
            std::string mod_id;
            Version version;
        };

        struct ModDetails {
            std::string mod_id;
            Version version;
            std::vector<std::string> authors;
            std::vector<Dependency> dependencies;
        };

        struct ModManifest {
            std::filesystem::path mod_root_path;

            std::vector<std::string> mod_game_ids;
            std::string mod_id;
            std::vector<std::string> authors;
            std::vector<Dependency> dependencies;
            std::unordered_map<std::string, size_t> dependencies_by_id;
            Version minimum_recomp_version;
            Version version;

            std::vector<NativeLibraryManifest> native_libraries;
            std::unique_ptr<ModFileHandle> file_handle;
        };

        struct ModOpenErrorDetails {
            std::filesystem::path mod_path;
            ModOpenError error;
            std::string error_param;
            ModOpenErrorDetails() = default;
            ModOpenErrorDetails(const std::filesystem::path& mod_path_, ModOpenError error_, const std::string& error_param_) :
                mod_path(mod_path_), error(error_), error_param(error_param_) {}
        };

        struct ModLoadErrorDetails {
            std::string mod_id;
            ModLoadError error;
            std::string error_param;
            ModLoadErrorDetails() = default;
            ModLoadErrorDetails(const std::string& mod_id_, ModLoadError error_, const std::string& error_param_) :
                mod_id(mod_id_), error(error_), error_param(error_param_) {}
        };
        
        void scan_mods();
        void enable_mod(const std::string& mod_id, bool enabled);
        bool is_mod_enabled(const std::string& mod_id);
        std::vector<ModDetails> get_mod_details(const std::string& mod_game_id);

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

            void register_game(const std::string& mod_game_id);
            std::vector<ModOpenErrorDetails> scan_mod_folder(const std::filesystem::path& mod_folder);
            void enable_mod(const std::string& mod_id, bool enabled);
            bool is_mod_enabled(const std::string& mod_id);
            size_t num_opened_mods();
            std::vector<ModLoadErrorDetails> load_mods(const std::string& mod_game_id, uint8_t* rdram, int32_t load_address, uint32_t& ram_used);
            void unload_mods();
            std::vector<ModDetails> get_mod_details(const std::string& mod_game_id);
        private:
            ModOpenError open_mod(const std::filesystem::path& mod_path, std::string& error_param);
            ModLoadError load_mod(uint8_t* rdram, const std::unordered_map<uint32_t, uint16_t>& section_map, recomp::mods::ModHandle& handle, int32_t load_address, uint32_t& ram_used, std::string& error_param);
            void check_dependencies(recomp::mods::ModHandle& mod, std::vector<std::pair<recomp::mods::ModLoadError, std::string>>& errors);
            ModLoadError load_mod_code(recomp::mods::ModHandle& mod, std::string& error_param);
            ModLoadError resolve_dependencies(recomp::mods::ModHandle& mod, std::string& error_param);
            void add_opened_mod(ModManifest&& manifest, std::vector<size_t>&& game_indices);

            // Maps game mod ID to the mod's internal integer ID. 
            std::unordered_map<std::string, size_t> mod_game_ids;
            std::vector<ModHandle> opened_mods;
            std::unordered_set<std::string> mod_ids;
            std::unordered_set<std::string> enabled_mods;
            std::unordered_map<recomp_func_t*, PatchData> patched_funcs;
            std::unordered_map<std::string, size_t> loaded_mods_by_id;
            size_t num_events = 0;
        };

        class ModCodeHandle {
        public:
            virtual ~ModCodeHandle() {}
            virtual bool good() = 0;
            virtual uint32_t get_api_version() = 0;
            virtual void set_imported_function(size_t import_index, GenericFunction func) = 0;
            virtual void set_reference_symbol_pointer(size_t symbol_index, recomp_func_t* ptr) = 0;
            virtual void set_base_event_index(uint32_t global_event_index) = 0;
            virtual uint32_t get_base_event_index() = 0;
            virtual void set_recomp_trigger_event_pointer(void (*ptr)(uint8_t* rdram, recomp_context* ctx, uint32_t index)) = 0;
            virtual void set_get_function_pointer(recomp_func_t* (*ptr)(int32_t)) = 0;
            virtual void set_cop0_status_write_pointer(void (*ptr)(recomp_context* ctx, gpr value)) = 0;
            virtual void set_cop0_status_read_pointer(gpr (*ptr)(recomp_context* ctx)) = 0;
            virtual void set_switch_error_pointer(void (*ptr)(const char* func, uint32_t vram, uint32_t jtbl)) = 0;
            virtual void set_do_break_pointer(void (*ptr)(uint32_t vram)) = 0;
            virtual void set_reference_section_addresses_pointer(int32_t* ptr) = 0;
            virtual void set_local_section_address(size_t section_index, int32_t address) = 0;
            virtual GenericFunction get_function_handle(size_t func_index) = 0;
        };

        class DynamicLibrary;
        class ModHandle {
        public:
            // TODO make these private and expose methods for the functionality they're currently used in.
            ModManifest manifest;
            std::unique_ptr<ModCodeHandle> code_handle;
            std::unique_ptr<N64Recomp::Context> recompiler_context;
            std::vector<uint32_t> section_load_addresses;

            ModHandle(ModManifest&& manifest, std::vector<size_t>&& game_indices);
            ModHandle(const ModHandle& rhs) = delete;
            ModHandle& operator=(const ModHandle& rhs) = delete;
            ModHandle(ModHandle&& rhs);
            ModHandle& operator=(ModHandle&& rhs);
            ~ModHandle();

            size_t num_exports() const;
            size_t num_events() const;

            ModLoadError populate_exports(std::string& error_param);
            bool get_export_function(const std::string& export_name, GenericFunction& out) const;
            ModLoadError populate_events(size_t base_event_index, std::string& error_param);
            bool get_global_event_index(const std::string& event_name, size_t& event_index_out) const;
            ModLoadError load_native_library(const NativeLibraryManifest& lib_manifest, std::string& error_param);

            bool is_for_game(size_t game_index) const {
                auto find_it = std::find(game_indices.begin(), game_indices.end(), game_index);
                return find_it != game_indices.end();
            }
        private:
            // Mapping of export name to function index.
            std::unordered_map<std::string, size_t> exports_by_name;
            // Mapping of export name to native library function pointer.
            std::unordered_map<std::string, recomp_func_t*> native_library_exports;
            // Mapping of event name to local index.
            std::unordered_map<std::string, size_t> events_by_name;
            // Loaded dynamic libraries.
            std::vector<std::unique_ptr<DynamicLibrary>> native_libraries; // Vector of pointers so that implementation can be elsewhere.
            // Games that this mod supports.
            std::vector<size_t> game_indices;
        };

        class NativeCodeHandle : public ModCodeHandle {
        public:
            NativeCodeHandle(const std::filesystem::path& dll_path, const N64Recomp::Context& context);
            ~NativeCodeHandle() = default;
            bool good() final;
            uint32_t get_api_version() final;
            void set_imported_function(size_t import_index, GenericFunction func) final;
            void set_reference_symbol_pointer(size_t symbol_index, recomp_func_t* ptr) final {
                reference_symbol_funcs[symbol_index] = ptr;
            };
            void set_base_event_index(uint32_t global_event_index) final {
                *base_event_index = global_event_index;
            };
            uint32_t get_base_event_index() final {
                return *base_event_index;
            }
            void set_recomp_trigger_event_pointer(void (*ptr)(uint8_t* rdram, recomp_context* ctx, uint32_t index)) final {
                *recomp_trigger_event = ptr;
            };
            void set_get_function_pointer(recomp_func_t* (*ptr)(int32_t)) final {
                *get_function = ptr;
            };
            void set_cop0_status_write_pointer(void (*ptr)(recomp_context* ctx, gpr value)) final {
                *cop0_status_write = ptr;
            }
            void set_cop0_status_read_pointer(gpr (*ptr)(recomp_context* ctx)) final {
                *cop0_status_read = ptr;
            }
            void set_switch_error_pointer(void (*ptr)(const char* func, uint32_t vram, uint32_t jtbl)) final {
                *switch_error = ptr;
            }
            void set_do_break_pointer(void (*ptr)(uint32_t vram)) final {
                *do_break = ptr;
            }
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
            bool is_good = false;
            std::unique_ptr<DynamicLibrary> dynamic_lib;
            std::vector<recomp_func_t*> functions;
            recomp_func_t** imported_funcs;
            recomp_func_t** reference_symbol_funcs;
            uint32_t* base_event_index;
            void (**recomp_trigger_event)(uint8_t* rdram, recomp_context* ctx, uint32_t index);
            recomp_func_t* (**get_function)(int32_t vram);
            void (**cop0_status_write)(recomp_context* ctx, gpr value);
            gpr (**cop0_status_read)(recomp_context* ctx);
            void (**switch_error)(const char* func, uint32_t vram, uint32_t jtbl);
            void (**do_break)(uint32_t vram);
            int32_t** reference_section_addresses;
            int32_t* section_addresses;
        };

        void setup_events(size_t num_events);
        void register_event_callback(size_t event_index, GenericFunction callback);
        void reset_events();
        ModLoadError validate_api_version(uint32_t api_version, std::string& error_param);
    }
};

extern "C" void recomp_trigger_event(uint8_t* rdram, recomp_context* ctx, uint32_t event_index);

#endif
