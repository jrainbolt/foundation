#ifndef FOUNDATION_GODOT_REGISTER_TYPES_H
#define FOUNDATION_GODOT_REGISTER_TYPES_H

#include <godot_cpp/core/class_db.hpp>

void initialize_foundation_module(
    godot::ModuleInitializationLevel level
);
void uninitialize_foundation_module(
    godot::ModuleInitializationLevel level
);

#endif
