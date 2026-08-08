#ifndef FOUNDATION_GODOT_SIMULATION_H
#define FOUNDATION_GODOT_SIMULATION_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <foundation/foundation.h>

#include "integer_conversion.h"

namespace godot {

class FoundationSimulation : public RefCounted {
    GDCLASS(FoundationSimulation, RefCounted)

private:
    FactoryWorld *world_ = nullptr;
    FactorySimulation *simulation_ = nullptr;
    FactoryPresentationSnapshot *presentation_ = nullptr;
    mutable String last_error_;

    void destroy_state();
    FactoryResult build_demo();
    bool entity_to_dictionary(
        const FactoryPresentationEntity &entity, Dictionary *out_value
    ) const;
    bool set_unsigned(
        Dictionary *dictionary, const char *key, uint64_t value,
        const char *field
    ) const;
    void set_conversion_error(const char *field, uint64_t value) const;

protected:
    static void _bind_methods();

public:
    FoundationSimulation();
    ~FoundationSimulation();

    int64_t reset_demo();
    int64_t step();
    int64_t step_many(int64_t count);
    int64_t queue_place_entity(
        int64_t entity_type,int64_t x,int64_t y,int64_t direction);
    int64_t queue_demolish_entity(int64_t entity_id);
    Array get_command_results() const;
    Array get_build_catalog() const;
    int64_t get_construction_units() const;
    int64_t place_fluid_tank(int64_t x, int64_t y);
    int64_t insert_fluid(
        int64_t destination_entity_id, int64_t fluid_type, int64_t quantity
    );
    int64_t remove_fluid(int64_t source_entity_id, int64_t quantity);
    int64_t transfer_fluid(
        int64_t source_entity_id, int64_t destination_entity_id,
        int64_t quantity
    );
    int64_t get_tick() const;
    int64_t get_day() const;
    int64_t get_time_of_day() const;
    Dictionary get_research() const;
    Array get_entities() const;
    Array get_resources() const;
    Array get_power_edges() const;
    Array get_events() const;
    void clear_events();
    bool has_error() const;
    String get_last_error() const;
    void clear_error();
    int64_t rebuild_presentation();
    String result_name(int64_t result) const;
};

}

#endif
