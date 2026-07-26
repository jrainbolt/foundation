#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/godot.hpp>

#include "foundation_simulation.h"

using namespace godot;

void initialize_foundation_module(ModuleInitializationLevel level)
{
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE)
        GDREGISTER_CLASS(FoundationSimulation);
}

void uninitialize_foundation_module(ModuleInitializationLevel level)
{
    (void)level;
}

extern "C" {
GDExtensionBool GDE_EXPORT foundation_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    GDExtensionClassLibraryPtr library,
    GDExtensionInitialization *initialization
)
{
    GDExtensionBinding::InitObject init(
        get_proc_address, library, initialization
    );
    init.register_initializer(initialize_foundation_module);
    init.register_terminator(uninitialize_foundation_module);
    init.set_minimum_library_initialization_level(
        MODULE_INITIALIZATION_LEVEL_SCENE
    );
    return init.init();
}
}
