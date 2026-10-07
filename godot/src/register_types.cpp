// GDExtension entry point: registers ProtodishWorld with Godot.

#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "protodish_world.hpp"

using namespace godot;

namespace {

void initialize_protodish(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    GDREGISTER_CLASS(ProtodishWorld);
}

void uninitialize_protodish(ModuleInitializationLevel level) { (void)level; }

}  // namespace

extern "C" {

GDExtensionBool GDE_EXPORT protodish_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                                                  GDExtensionClassLibraryPtr library,
                                                  GDExtensionInitialization* initialization) {
    GDExtensionBinding::InitObject init_obj(get_proc_address, library, initialization);
    init_obj.register_initializer(initialize_protodish);
    init_obj.register_terminator(uninitialize_protodish);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}
