#pragma once

#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/variant.hpp"

#include <memory>

namespace carbon {
class Carbon;
}

namespace common {
struct Config;
}

namespace godot {

class GDSoot : public RefCounted {
    GDCLASS(GDSoot, RefCounted)

protected:
    static void _bind_methods();

public:
    GDSoot();
    ~GDSoot() override;

    // Lifecycle
    bool initialize(const String &project_root = "");
    void shutdown();
    bool is_initialized() const;

    // Info
    String get_project_root() const;
    String get_sidbase_path() const;
    int get_search_path_count() const;

    // Modules
    bool load_module(const String &path);
    Variant run_top_level(const String &module_name);
    bool has_module(const String &module_name) const;

    // Calling
    Variant call(const String &name, const Array &args = {});

    // Tick (for Godot _process / _physics_process)
    void tick();
    bool should_exit() const;

private:
    bool load_config(const String &project_root);

    std::unique_ptr<common::Config>     m_config;
    std::unique_ptr<carbon::Carbon>     m_carbon;
    bool                                m_initialized = false;
};

} // namespace godot