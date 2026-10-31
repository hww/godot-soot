#include "gd_soot.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/project_settings.hpp>

#include "carbon/Carbon.hpp"
#include "carbon/lib/Variant.hpp"
#include "util/ProjectConfig.hpp"
#include "util/Config.hpp"

#include <filesystem>
#include <string>

using namespace godot;
using namespace common;

namespace {

// Godot String -> std::string
std::string to_std(const String &s) {
    return std::string(s.utf8().get_data());
}

// std::string -> Godot String
String from_std(const std::string &s) {
    return String::utf8(s.c_str());
}

// std::filesystem::path -> Godot String
String from_path(const std::filesystem::path &p) {
    return from_std(p.string());
}

// Resolve the Godot project directory (where project.godot lives).
// `res://` maps to that directory in the editor and in exported games.
std::filesystem::path godot_project_dir() {
    // ProjectSettings.globalize_path("res://") returns the real OS path
    // of the project root.
    String res = ProjectSettings::get_singleton()->globalize_path("res://");
    return std::filesystem::path(to_std(res));
}

} // namespace

// ================================================================
// Binding
// ================================================================

void GDSoot::_bind_methods() {
    ClassDB::bind_method(D_METHOD("initialize", "project_root"), &GDSoot::initialize,
                         DEFVAL(String()));
    ClassDB::bind_method(D_METHOD("shutdown"), &GDSoot::shutdown);
    ClassDB::bind_method(D_METHOD("is_initialized"), &GDSoot::is_initialized);

    ClassDB::bind_method(D_METHOD("get_project_root"), &GDSoot::get_project_root);
    ClassDB::bind_method(D_METHOD("get_sidbase_path"), &GDSoot::get_sidbase_path);
    ClassDB::bind_method(D_METHOD("get_search_path_count"), &GDSoot::get_search_path_count);

    ClassDB::bind_method(D_METHOD("load_module", "path"), &GDSoot::load_module);
    ClassDB::bind_method(D_METHOD("run_top_level", "module_name"), &GDSoot::run_top_level);
    ClassDB::bind_method(D_METHOD("has_module", "module_name"), &GDSoot::has_module);

    ClassDB::bind_method(D_METHOD("call", "name", "args"), &GDSoot::call, DEFVAL(Array()));

    ClassDB::bind_method(D_METHOD("tick"), &GDSoot::tick);
    ClassDB::bind_method(D_METHOD("should_exit"), &GDSoot::should_exit);
}

// ================================================================
// Lifecycle
// ================================================================

GDSoot::GDSoot() = default;

GDSoot::~GDSoot() {
    shutdown();
}

bool GDSoot::initialize(const String &project_root) {
    if (m_initialized) return true;

    if (!load_config(project_root)) return false;

    m_carbon = std::make_unique<carbon::Carbon>();

    if (!m_carbon->initialize(*m_config)) {            // ← было: initialize(cfg)
        UtilityFunctions::push_error("GDSoot: Carbon::initialize failed");
        m_carbon.reset();
        return false;
    }

    m_initialized = true;
    UtilityFunctions::print("GDSoot: initialized, project_root=" +
                            from_path(m_config->common.project_root));
    return true;
}

void GDSoot::shutdown() {
    if (m_carbon) {
        m_carbon->shutdown();
        m_carbon.reset();
    }
    m_initialized = false;
}

bool GDSoot::is_initialized() const {
    return m_initialized && m_carbon && m_carbon->is_initialized();
}

// ================================================================
// Info
// ================================================================

String GDSoot::get_project_root() const {
    if (!m_config) return String();
    return from_path(m_config->common.project_root);
}

String GDSoot::get_sidbase_path() const {
    if (!m_config) return String();
    return from_path(m_config->common.sidbase);
}

int GDSoot::get_search_path_count() const {
    if (!m_config) return 0;
    return static_cast<int>(m_config->common.search_paths.size());
}

// ================================================================
// Modules
// ================================================================

bool GDSoot::load_module(const String &path) {
    if (!is_initialized()) {
        UtilityFunctions::push_error("GDSoot::load_module: not initialized");
        return false;
    }
    std::filesystem::path p = to_std(path);
    if (p.is_relative()) p = m_config->common.project_root / p;

    if (!m_carbon->load_module(p)) {
        UtilityFunctions::push_error("GDSoot::load_module: failed for " + from_path(p));
        return false;
    }
    return true;
}

Variant GDSoot::run_top_level(const String &module_name) {
    if (!is_initialized()) return Variant();
    auto res = m_carbon->run_top_level(to_std(module_name));
    if (!res.ok) {
        UtilityFunctions::push_error("GDSoot::run_top_level: " + from_std(res.error));
        return Variant();
    }
    return Variant(); // call result is not converted here; use call() for values
}

bool GDSoot::has_module(const String &module_name) const {
    if (!is_initialized()) return false;
    return m_carbon->has_module(to_std(module_name));
}

// ================================================================
// Calling
// ================================================================

Variant GDSoot::call(const String &name, const Array &args) {
    if (!is_initialized()) {
        UtilityFunctions::push_error("GDSoot::call: not initialized");
        return Variant();
    }

    // Convert godot::Array -> std::vector<carbon::Variant>
    std::vector<carbon::Variant> vm_args;
    vm_args.reserve(args.size());
    for (int i = 0; i < args.size(); ++i) {
        Variant v = args[i];
        switch (v.get_type()) {
            case Variant::INT:    vm_args.emplace_back(static_cast<int64_t>(v)); break;
            case Variant::FLOAT:  vm_args.emplace_back(static_cast<double>(v)); break;
            case Variant::BOOL:   vm_args.emplace_back(static_cast<bool>(v)); break;
            case Variant::STRING: vm_args.emplace_back(carbon::Variant(to_std(v))); break;
            default:
                UtilityFunctions::push_error("GDSoot::call: unsupported argument type");
                return Variant();
        }
    }

    auto res = m_carbon->call(to_std(name), vm_args);
    if (!res.ok) {
        UtilityFunctions::push_error("GDSoot::call: " + from_std(res.error));
        return Variant();
    }

    // Convert carbon::Variant -> godot::Variant
    switch (res.value.get_type()) {
        case carbon::RuntimeType::Null:
            return Variant();
        case carbon::RuntimeType::Int:
            return Variant(static_cast<int64_t>(res.value.get_i64()));
        case carbon::RuntimeType::Float:
            return Variant(res.value.get_f64());
        case carbon::RuntimeType::Pointer:
            // Строка или указатель — зависит от того, что лежит в Pointer.
            // Если get_string()/to_string() доступны — используйте их.
            // Иначе верните указатель как int64_t.
            return Variant(static_cast<int64_t>(reinterpret_cast<intptr_t>(res.value.get_ptr())));
    }
    return Variant();
}

// ================================================================
// Tick
// ================================================================

void GDSoot::tick() {
    if (is_initialized()) m_carbon->tick();
}

bool GDSoot::should_exit() const {
    return is_initialized() && m_carbon->should_exit();
}

// ================================================================
// Config
// ================================================================

bool GDSoot::load_config(const String &project_root) {
    namespace fs = std::filesystem;

    // 1. Determine the project root.
    //    If the caller passed one, use it (relative paths are resolved
    //    against the Godot project dir).
    //    Otherwise use the Godot project directory.
    fs::path root;
    if (project_root.is_empty()) {
        root = godot_project_dir();
    } else {
        fs::path given = to_std(project_root);
        root = given.is_absolute() ? given : godot_project_dir() / given;
    }

    // 2. Read soot.json from that root (if present).
    m_config = std::make_unique<common::Config>();
    if (auto err = common::load_json(std::string{}, root.string(), *m_config)) {
        UtilityFunctions::push_warning("GDSoot: " + from_std(*err));
        // keep defaults
    }

    // 3. Apply environment (SOOT_*).
    if (auto env_err = common::apply_env(*m_config)) {
        UtilityFunctions::push_warning("GDSoot: env: " + from_std(*env_err));
    }

    // 4. Make sure project_root is absolute and non-empty.
    if (m_config->common.project_root.empty()) {
        m_config->common.project_root = root;
    } else if (m_config->common.project_root.is_relative()) {
        m_config->common.project_root = fs::absolute(m_config->common.project_root);
    }

    // 5. Tell the soot path layer about the project root. This is what
    //    common::get_path(PathType::PROJECT) and find_project_file use.
    common::set_project_path(m_config->common.project_root);

    UtilityFunctions::print("GDSoot: config loaded from " +
                            from_path(m_config->common.project_root));
    return true;
}
