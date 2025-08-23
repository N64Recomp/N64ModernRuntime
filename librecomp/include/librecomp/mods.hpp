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
#include <mutex>
#include <optional>

#include "blockingconcurrentqueue.h"

#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#include "miniz.h"
#include "miniz_zip.h"

#include "recomp.h"
#include "librecomp/game.hpp"
#include "librecomp/sections.h"
#include "librecomp/overlays.hpp"

namespace N64Recomp {
    class Context;
    struct LiveGeneratorOutput;
    class ShimFunction;
};

namespace recomp {
    namespace mods {
        struct HookDefinition {
            uint32_t section_rom;
            uint32_t function_vram;
            bool at_return;
            bool operator==(const HookDefinition& rhs) const = default;
        };
    }
}

template <>
struct std::hash<recomp::mods::HookDefinition>
{
    std::size_t operator()(const recomp::mods::HookDefinition& def) const {
        // This hash packing only works if the resulting value is 64 bits.
        static_assert(sizeof(std::size_t) == 8);
        // Combine the three values into a single 64-bit value.
        // The lower 2 bits of a function address will always be zero, so pack
        // the value of at_return into the lowest bit.
        return (size_t(def.section_rom) << 32) | size_t(def.function_vram) | size_t(def.at_return ? 1 : 0);
    }
};

namespace recomp {
    namespace mods {
        static constexpr std::string_view mods_directory = "mods";
        static constexpr std::string_view mod_config_directory = "mod_config";

        enum class ModOpenError {
            Good,
            DoesNotExist,
            NotAFileOrFolder,
            FileError,
            InvalidZip,
            NoManifest,
            FailedToParseManifest,
            InvalidManifestSchema,
            IncorrectManifestFieldType,
            MissingConfigSchemaField,
            IncorrectConfigSchemaType,
            InvalidConfigSchemaDefault,
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
            MissingDependency,
            WrongDependencyVersion,
            FailedToLoadCode,
            RomPatchConflict,
            FailedToLoadPatch,
        };

        std::string error_to_string(ModLoadError);
        void unmet_dependency_handler(uint8_t* rdram, recomp_context* ctx, uintptr_t arg);

        enum class CodeModLoadError {
            Good,
            InternalError,
            HasSymsButNoBinary,
            HasBinaryButNoSyms,
            FailedToParseSyms,
            MissingDependencyInManifest,
            FailedToLoadNativeCode,
            FailedToLoadNativeLibrary,
            FailedToFindNativeExport,
            FailedToRecompile,
            InvalidReferenceSymbol,
            InvalidImport,
            InvalidCallbackEvent,
            InvalidFunctionReplacement,
            HooksUnavailable,
            InvalidHook,
            CannotBeHooked,
            FailedToFindReplacement,
            BaseRecompConflict,
            ModConflict,
            DuplicateExport,
            OfflineModHooked,
            NoSpecifiedApiVersion,
            UnsupportedApiVersion,
        };

        std::string error_to_string(CodeModLoadError);

        enum class ConfigOptionType {
            None,
            Enum,
            Number,
            String
        };

        enum class DependencyStatus {
            // Do not change these values as they're exposed in the mod API!

            // The dependency was found and the version requirement was met.
            Found = 0,
            // The ID given is not a dependency of the mod in question.
            InvalidDependency = 1,
            // The dependency was not found.
            NotFound = 2,
            // The dependency was found, but the version requirement was not met.
            WrongVersion = 3
        };

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
            ZipModFileHandle(std::span<const uint8_t> mod_bytes, ModOpenError& error);
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
            bool optional;
        };

        struct ConfigOptionEnum {
            std::vector<std::string> options;
            uint32_t default_value = 0;
        };

        struct ConfigOptionNumber {
            double min = 0.0;
            double max = 0.0;
            double step = 0.0;
            int precision = 0;
            bool percent = false;
            double default_value = 0.0;
        };

        struct ConfigOptionString {
            std::string default_value;
        };

        typedef std::variant<ConfigOptionEnum, ConfigOptionNumber, ConfigOptionString> ConfigOptionVariant;

        struct ConfigOption {
            std::string id;
            std::string name;
            std::string description;
            ConfigOptionType type;
            ConfigOptionVariant variant;
        };

        struct ConfigSchema {
            std::vector<ConfigOption> options;
            std::unordered_map<std::string, size_t> options_by_id;
        };

        typedef std::variant<std::monostate, uint32_t, double, std::string> ConfigValueVariant;

        struct ConfigStorage {
            std::unordered_map<std::string, ConfigValueVariant> value_map;
        };

        struct ModDetails {
            std::string mod_id;
            std::string display_name;
            std::string description;
            std::string short_description;
            Version version;
            std::vector<std::string> authors;
            std::vector<Dependency> dependencies;
            bool runtime_toggleable;
            bool enabled_by_default;
        };

        struct ModManifest {
            std::filesystem::path mod_root_path;

            std::vector<std::string> mod_game_ids;
            std::string mod_id;
            std::string display_name;
            std::string description;
            std::string short_description;
            std::vector<std::string> authors;
            std::vector<Dependency> dependencies;
            std::unordered_map<std::string, size_t> dependencies_by_id;
            ConfigSchema config_schema;
            Version minimum_recomp_version;
            Version version;
            bool runtime_toggleable;
            bool enabled_by_default;

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

        void set_mod_index(const std::string &mod_game_id, const std::string &mod_id, size_t index);

        // Internal functions, TODO move to an internal header.
        struct PatchData {
            std::array<std::byte, 16> replaced_bytes;
            std::string mod_id;
        };

        using GenericFunction = std::variant<recomp_func_t*>;

        class ModContext;
        class ModHandle;
        using content_enabled_callback = void(ModContext&, const ModHandle&);
        using content_disabled_callback = void(ModContext&, const ModHandle&);
        using content_reordered_callback = void(ModContext&);

        struct ModContentType {
            // The file that's used to indicate that a mod contains this content type.
            // If a mod contains this file, then it has this content type.
            std::string content_filename;
            // Whether or not this type of content can be toggled at runtime.
            bool allow_runtime_toggle;
            // Function to call when an instance of this content type is enabled.
            content_enabled_callback* on_enabled;
            // Function to call when an instance of this content type is disabled.
            content_disabled_callback* on_disabled;
            // Function to call when an instance of this content type has been reordered.
            // No mod handle is provided as multiple instances may have been reordered at the same time.
            // Will not be called if an instance of this content type was incidentally reordered due
            // to the reordering of another mod, as the ordering of just instances of this content type will not have changed.
            content_reordered_callback* on_reordered;
        };

        // Holds IDs for mod content types, which get assigned as they're registered.
        // This is just a wrapper around a number for type safety purposes.
        struct ModContentTypeId {
            size_t value;
            bool operator==(const ModContentTypeId& rhs) const = default;
        };

        struct ModContainerType {
            // The types of content that this container is allowed to have.
            // Leaving this empty will allow the container to have any type of content.
            std::vector<ModContentTypeId> supported_content_types;
            // Whether or not this container requires a manifest to be treated as a valid mod.
            // If no manifest is present, a default one will be created.
            bool requires_manifest;
        };

        struct ModConfigQueueSaveMod {
            std::string mod_id;
        };

        struct ModConfigQueueSave {
            uint32_t pad;
        };

        struct ModConfigQueueEnd {
            uint32_t pad;
        };

        typedef std::variant<ModConfigQueueSaveMod, ModConfigQueueSave, ModConfigQueueEnd> ModConfigQueueVariant;

        class LiveRecompilerCodeHandle;
        class ModContext {
        public:
            ModContext();
            ~ModContext();

            void register_game(const std::string& mod_game_id);
            void register_embedded_mod(const std::string& mod_id, std::span<const uint8_t> mod_bytes);
            std::vector<ModOpenErrorDetails> scan_mod_folder(const std::filesystem::path& mod_folder);
            void close_mods();
            void load_mods_config();
            void enable_mod(const std::string& mod_id, bool enabled, bool trigger_save);
            bool is_mod_enabled(const std::string& mod_id);
            bool is_mod_auto_enabled(const std::string& mod_id);
            size_t num_opened_mods();
            std::vector<ModLoadErrorDetails> load_mods(const GameEntry& game_entry, uint8_t* rdram, int32_t load_address, uint32_t& ram_used);
            void unload_mods();
            std::string get_mod_id_from_filename(const std::filesystem::path& mod_filename) const;
            std::filesystem::path get_mod_filename(const std::string& mod_id) const;
            size_t get_mod_order_index(const std::string& mod_id) const;
            size_t get_mod_order_index(size_t mod_index) const;
            std::optional<ModDetails> get_details_for_mod(const std::string& mod_id) const;
            std::vector<ModDetails> get_all_mod_details(const std::string& mod_game_id);
            recomp::Version get_mod_version(size_t mod_index);
            std::string get_mod_id(size_t mod_index);
            void set_mod_index(const std::string &mod_game_id, const std::string &mod_id, size_t index);
            const ConfigSchema &get_mod_config_schema(const std::string &mod_id) const;
            const std::vector<char> &get_mod_thumbnail(const std::string &mod_id) const;
            void set_mod_config_value(size_t mod_index, const std::string &option_id, const ConfigValueVariant &value);
            void set_mod_config_value(const std::string &mod_id, const std::string &option_id, const ConfigValueVariant &value);
            ConfigValueVariant get_mod_config_value(size_t mod_index, const std::string &option_id) const;
            ConfigValueVariant get_mod_config_value(const std::string &mod_id, const std::string &option_id) const;
            void set_mods_config_path(const std::filesystem::path &path);
            void set_mod_config_directory(const std::filesystem::path &path);
            ModContentTypeId register_content_type(const ModContentType& type);
            bool register_container_type(const std::string& extension, const std::vector<ModContentTypeId>& content_types, bool requires_manifest);
            ModContentTypeId get_code_content_type() const { return code_content_type_id; }
            bool is_content_runtime_toggleable(ModContentTypeId content_type) const;
            std::string get_mod_display_name(size_t mod_index) const;
            std::filesystem::path get_mod_path(size_t mod_index) const;
            std::pair<std::string, std::string> get_mod_import_info(size_t mod_index, size_t import_index) const;
            DependencyStatus is_dependency_met(size_t mod_index, const std::string& dependency_id) const;
        private:
            ModOpenError open_mod_from_manifest(ModManifest &manifest, std::string &error_param, const std::vector<ModContentTypeId> &supported_content_types, bool requires_manifest);
            ModOpenError open_mod_from_path(const std::filesystem::path& mod_path, std::string& error_param, const std::vector<ModContentTypeId>& supported_content_types, bool requires_manifest);
            ModOpenError open_mod_from_memory(std::span<const uint8_t> mod_bytes, std::string &error_param, const std::vector<ModContentTypeId> &supported_content_types, bool requires_manifest);
            ModLoadError load_mod(ModHandle& mod, std::string& error_param);
            void check_dependencies(ModHandle& mod, std::vector<std::pair<ModLoadError, std::string>>& errors);
            CodeModLoadError init_mod_code(uint8_t* rdram, const std::unordered_map<uint32_t, uint16_t>& section_vrom_map, ModHandle& mod, int32_t load_address, bool hooks_available, uint32_t& ram_used, std::string& error_param);
            CodeModLoadError load_mod_code(uint8_t* rdram, ModHandle& mod, uint32_t base_event_index, std::string& error_param);
            CodeModLoadError resolve_code_dependencies(ModHandle& mod, size_t mod_index, const std::unordered_map<recomp_func_t*, recomp::overlays::BasePatchedFunction>& base_patched_funcs, std::string& error_param);
            void add_opened_mod(ModManifest&& manifest, ConfigStorage&& config_storage, std::vector<size_t>&& game_indices, std::vector<ModContentTypeId>&& detected_content_types, std::vector<char>&& thumbnail);
            std::vector<ModLoadErrorDetails> regenerate_with_hooks(
                const std::vector<std::pair<HookDefinition, size_t>>& sorted_unprocessed_hooks,
                const std::unordered_map<uint32_t, uint16_t>& section_vrom_map,
                const std::unordered_map<recomp_func_t*, overlays::BasePatchedFunction>& base_patched_funcs,
                std::span<const uint8_t> decompressed_rom);
            void dirty_mod_configuration_thread_process();
            void rebuild_mod_order_lookup();

            static void on_code_mod_enabled(ModContext& context, const ModHandle& mod);

            std::vector<ModContentType> content_types;
            std::unordered_map<std::string, ModContainerType> container_types;
            // Maps game mod ID to the mod's internal integer ID. 
            std::unordered_map<std::string, size_t> mod_game_ids;
            std::unordered_map<std::string, std::span<const uint8_t>> embedded_mod_bytes;
            std::vector<ModHandle> opened_mods;
            std::unordered_map<std::string, size_t> opened_mods_by_id;
            std::unordered_map<std::filesystem::path::string_type, size_t> opened_mods_by_filename;
            std::vector<size_t> opened_mods_order; // order index -> mod index
            std::vector<size_t> mod_order_lookup; // mod index -> order index
            std::mutex opened_mods_mutex;
            std::unordered_set<std::string> mod_ids;
            std::unordered_set<std::string> enabled_mods;
            std::unordered_set<std::string> auto_enabled_mods;
            std::unordered_map<recomp_func_t*, PatchData> patched_funcs;
            std::unordered_map<std::string, size_t> loaded_mods_by_id;
            std::unique_ptr<std::thread> mod_configuration_thread;
            moodycamel::BlockingConcurrentQueue<ModConfigQueueVariant> mod_configuration_thread_queue;
            std::filesystem::path mods_config_path;
            std::filesystem::path mod_config_directory;
            mutable std::mutex mod_config_storage_mutex;
            std::vector<size_t> loaded_code_mods;
            // Code handle for vanilla code that was regenerated to add hooks.
            std::unique_ptr<LiveRecompilerCodeHandle> regenerated_code_handle;
            // Code handle for base patched code that was regenerated to add hooks.
            std::unique_ptr<LiveRecompilerCodeHandle> base_patched_code_handle;
            // Map of hook definition to the entry hook slot's index.
            std::unordered_map<HookDefinition, size_t> hook_slots;
            // Tracks which hook slots have already been processed. Used to regenerate vanilla functions as needed
            // to add hooks to any functions that weren't already replaced by a mod.
            std::vector<bool> processed_hook_slots;
            // Generated shim functions to use for implementing shim exports.
            std::vector<std::unique_ptr<N64Recomp::ShimFunction>> shim_functions;
            ConfigSchema empty_schema;
            std::vector<char> empty_bytes;
            size_t num_events = 0;
            ModContentTypeId code_content_type_id;
            ModContentTypeId rom_patch_content_type_id;
            size_t active_game = (size_t)-1;
        };

        class ModCodeHandle {
        public:
            virtual ~ModCodeHandle() {}
            virtual bool good() = 0;
            virtual uint32_t get_api_version() = 0;
            virtual void set_imported_function(size_t import_index, GenericFunction func) = 0;
            virtual CodeModLoadError populate_reference_symbols(const N64Recomp::Context& recompiler_context, std::string& error_param) = 0;
            virtual uint32_t get_base_event_index() = 0;
            virtual void set_local_section_address(size_t section_index, int32_t address) = 0;
            virtual GenericFunction get_function_handle(size_t func_index) = 0;
        };

        class DynamicLibrary;
        class ModHandle {
        public:
            // TODO make these private and expose methods for the functionality they're currently used in.
            ModManifest manifest;
            ConfigStorage config_storage;
            std::unique_ptr<ModCodeHandle> code_handle;
            std::unique_ptr<N64Recomp::Context> recompiler_context;
            std::vector<uint32_t> section_load_addresses;
            // Content types present in this mod.
            std::vector<ModContentTypeId> content_types;
            std::vector<char> thumbnail;

            ModHandle(const ModContext& context, ModManifest&& manifest, ConfigStorage&& config_storage, std::vector<size_t>&& game_indices, std::vector<ModContentTypeId>&& content_types, std::vector<char>&& thumbnail);
            ModHandle(const ModHandle& rhs) = delete;
            ModHandle& operator=(const ModHandle& rhs) = delete;
            ModHandle(ModHandle&& rhs);
            ModHandle& operator=(ModHandle&& rhs);
            ~ModHandle();

            size_t num_exports() const;
            size_t num_events() const;

            void populate_exports();
            bool get_export_function(const std::string& export_name, GenericFunction& out) const;
            void populate_events();
            bool get_global_event_index(const std::string& event_name, size_t& event_index_out) const;
            CodeModLoadError load_native_library(const NativeLibraryManifest& lib_manifest, std::string& error_param);

            bool is_for_game(size_t game_index) const {
                auto find_it = std::find(game_indices.begin(), game_indices.end(), game_index);
                return find_it != game_indices.end();
            }

            bool is_runtime_toggleable() const {
                return runtime_toggleable;
            }

            void disable_runtime_toggle() {
                runtime_toggleable = false;
            }
            
            ModDetails get_details() const {
                return ModDetails {
                    .mod_id = manifest.mod_id,
                    .display_name = manifest.display_name,
                    .description = manifest.description,
                    .short_description = manifest.short_description,
                    .version = manifest.version,
                    .authors = manifest.authors,
                    .dependencies = manifest.dependencies,
                    .runtime_toggleable = is_runtime_toggleable(),
                    .enabled_by_default = manifest.enabled_by_default
                };
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
            // Whether this mod can be toggled at runtime.
            bool runtime_toggleable;
        };
        
        struct ModCodeHandleInputs {
            uint32_t base_event_index;
            void (*recomp_trigger_event)(uint8_t* rdram, recomp_context* ctx, uint32_t index);
            recomp_func_t* (*get_function)(int32_t vram);
            void (*cop0_status_write)(recomp_context* ctx, gpr value);
            gpr (*cop0_status_read)(recomp_context* ctx);
            void (*switch_error)(const char* func, uint32_t vram, uint32_t jtbl);
            void (*do_break)(uint32_t vram);
            int32_t* reference_section_addresses;
        };

        class DynamicLibraryCodeHandle : public ModCodeHandle {
        public:
            DynamicLibraryCodeHandle(const std::filesystem::path& dll_path, const N64Recomp::Context& context, const ModCodeHandleInputs& inputs);
            ~DynamicLibraryCodeHandle() = default;
            bool good() final;
            uint32_t get_api_version() final;
            void set_imported_function(size_t import_index, GenericFunction func) final;
            CodeModLoadError populate_reference_symbols(const N64Recomp::Context& context, std::string& error_param) final;
            uint32_t get_base_event_index() final {
                return *base_event_index;
            }
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

        class LiveRecompilerCodeHandle : public ModCodeHandle {
        public:
            LiveRecompilerCodeHandle(const N64Recomp::Context& context, const ModCodeHandleInputs& inputs,
                std::unordered_map<size_t, size_t>&& entry_func_hooks, std::unordered_map<size_t, size_t>&& return_func_hooks, std::vector<size_t>&& original_section_indices, bool regenerated);

            ~LiveRecompilerCodeHandle() = default;
            
            // Disable copying.
            LiveRecompilerCodeHandle(const LiveRecompilerCodeHandle& rhs) = delete;
            LiveRecompilerCodeHandle& operator=(const LiveRecompilerCodeHandle& rhs) = delete;

            bool good() final { return is_good; }
            uint32_t get_api_version() final { return 1; }
            void set_imported_function(size_t import_index, GenericFunction func) final;
            CodeModLoadError populate_reference_symbols(const N64Recomp::Context& context, std::string& error_param) final;
            uint32_t get_base_event_index() final { 
                return base_event_index;
            }
            void set_local_section_address(size_t section_index, int32_t address) final {
                section_addresses[section_index] = address;
            }
            GenericFunction get_function_handle(size_t func_index) final;
        private:
            uint32_t base_event_index;
            std::unique_ptr<N64Recomp::LiveGeneratorOutput> recompiler_output;
            void set_bad();
            bool is_good = false;
            std::unique_ptr<int32_t[]> section_addresses;
        };

        void setup_events(size_t num_events);
        void register_event_callback(size_t event_index, size_t mod_index, GenericFunction callback);
        void reset_events();
        
        void setup_hooks(size_t num_hook_slots);
        void set_hook_type(size_t hook_slot_index, bool is_return_hook);
        void register_hook(size_t hook_slot_index, size_t mod_index, GenericFunction callback);
        void finish_event_setup(const ModContext& context);
        void finish_hook_setup(const ModContext& context);
        void reset_hooks();
        void register_hook_exports();
        void run_hook(uint8_t* rdram, recomp_context* ctx, size_t hook_slot_index);

        ModOpenError parse_manifest(ModManifest &ret, const std::vector<char> &manifest_data, std::string &error_param);
        CodeModLoadError validate_api_version(uint32_t api_version, std::string& error_param);

        void initialize_mods();
        void register_embedded_mod(const std::string &mod_id, std::span<const uint8_t> mod_bytes);
        void scan_mods();
        void close_mods();
        std::filesystem::path get_mods_directory();
        std::optional<ModDetails> get_details_for_mod(const std::string& mod_id);
        std::vector<ModDetails> get_all_mod_details(const std::string& mod_game_id);
        recomp::Version get_mod_version(size_t mod_index);
        std::string get_mod_id(size_t mod_index);
        void enable_mod(const std::string& mod_id, bool enabled);
        bool is_mod_enabled(const std::string& mod_id);
        bool is_mod_auto_enabled(const std::string& mod_id);
        const ConfigSchema &get_mod_config_schema(const std::string &mod_id);
        const std::vector<char> &get_mod_thumbnail(const std::string &mod_id);
        void set_mod_config_value(size_t mod_index, const std::string &option_id, const ConfigValueVariant &value);
        void set_mod_config_value(const std::string &mod_id, const std::string &option_id, const ConfigValueVariant &value);
        ConfigValueVariant get_mod_config_value(size_t mod_index, const std::string &option_id);
        ConfigValueVariant get_mod_config_value(const std::string &mod_id, const std::string &option_id);
        std::string get_mod_id_from_filename(const std::filesystem::path& mod_filename);
        std::filesystem::path get_mod_filename(const std::string& mod_id);
        size_t get_mod_order_index(const std::string& mod_id);
        size_t get_mod_order_index(size_t mod_index);
        ModContentTypeId register_mod_content_type(const ModContentType& type);
        bool register_mod_container_type(const std::string& extension, const std::vector<ModContentTypeId>& content_types, bool requires_manifest);
        std::string get_mod_display_name(size_t mod_index);
        std::filesystem::path get_mod_path(size_t mod_index);
        std::pair<std::string, std::string> get_mod_import_info(size_t mod_index, size_t import_index);
        DependencyStatus is_dependency_met(size_t mod_index, const std::string& dependency_id);

        void register_config_exports();
    }
};

extern "C" void recomp_trigger_event(uint8_t* rdram, recomp_context* ctx, uint32_t event_index);

#endif
