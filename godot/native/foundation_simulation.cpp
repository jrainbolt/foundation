#include "foundation_simulation.h"

#include <climits>

#include <godot_cpp/core/class_db.hpp>

namespace godot {
namespace {

constexpr uint32_t DEMO_WIDTH = 12U;
constexpr uint32_t DEMO_HEIGHT = 8U;
constexpr FactoryConstructionMaterial DEMO_CONSTRUCTION_UNITS = 1000U;
constexpr int64_t MAX_STEP_MANY = 10000;

FactoryCommand place(
    FactoryCommandType type, int32_t x, int32_t y,
    FactoryDirection first = FACTORY_DIRECTION_NORTH,
    FactoryDirection second = FACTORY_DIRECTION_NORTH
)
{
    FactoryCommand command = {};
    command.type = type;
    switch (type) {
    case FACTORY_COMMAND_PLACE_EXTRACTOR:
        command.data.place_extractor = {x, y, first};
        break;
    case FACTORY_COMMAND_PLACE_BELT:
        command.data.place_belt = {x, y, first};
        break;
    case FACTORY_COMMAND_PLACE_STORAGE:
        command.data.place_storage = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_REFINERY:
        command.data.place_refinery = {x, y, first, second};
        break;
    case FACTORY_COMMAND_PLACE_ASSEMBLER:
        command.data.place_assembler = {x, y, first};
        break;
    case FACTORY_COMMAND_PLACE_SPLITTER:
        command.data.place_splitter = {x, y, first};
        break;
    case FACTORY_COMMAND_PLACE_INSERTER:
        command.data.place_inserter = {x, y, first};
        break;
    case FACTORY_COMMAND_PLACE_POWER_POLE:
        command.data.place_power_pole = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_POWER_GENERATOR:
        command.data.place_power_generator = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_FLUID_TANK:
        command.data.place_fluid_tank = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_PIPE:
        command.data.place_pipe = {x, y};
        break;
    default:
        break;
    }
    return command;
}

Dictionary item_slot(FactoryItemType item, uint32_t quantity)
{
    Dictionary value;
    value["item"] = (int64_t)item;
    value["quantity"] = (int64_t)quantity;
    return value;
}

const char *result_name_c(FactoryResult result)
{
    switch (result) {
    case FACTORY_RESULT_OK: return "ok";
    case FACTORY_RESULT_INVALID_ARGUMENT: return "invalid argument";
    case FACTORY_RESULT_OUT_OF_BOUNDS: return "out of bounds";
    case FACTORY_RESULT_TILE_OCCUPIED: return "tile occupied";
    case FACTORY_RESULT_OUT_OF_MEMORY: return "out of memory";
    case FACTORY_RESULT_NO_RESOURCE: return "no resource";
    case FACTORY_RESULT_UNSUPPORTED_RESOURCE: return "unsupported resource";
    case FACTORY_RESULT_QUEUE_FULL: return "queue full";
    case FACTORY_RESULT_INVALID_STATE: return "invalid state";
    case FACTORY_RESULT_ENTITY_NOT_FOUND: return "entity not found";
    case FACTORY_RESULT_ENTITY_BUSY: return "entity busy";
    case FACTORY_RESULT_ENTITY_HAS_MATERIAL: return "entity has material";
    case FACTORY_RESULT_UNSUPPORTED_ENTITY: return "unsupported entity";
    case FACTORY_RESULT_INTERNAL_STATE_MISMATCH:
        return "internal state mismatch";
    case FACTORY_RESULT_INSUFFICIENT_CONSTRUCTION_UNITS:
        return "insufficient construction units";
    case FACTORY_RESULT_CONSTRUCTION_INVENTORY_OVERFLOW:
        return "construction inventory overflow";
    case FACTORY_RESULT_ASSEMBLER_NOT_EMPTY: return "assembler not empty";
    case FACTORY_RESULT_STORAGE_OUTPUT_NOT_EMPTY:
        return "storage output not empty";
    case FACTORY_RESULT_SNAPSHOT_BUFFER_TOO_SMALL:
        return "snapshot buffer too small";
    case FACTORY_RESULT_SNAPSHOT_INVALID_MAGIC:
        return "snapshot invalid magic";
    case FACTORY_RESULT_SNAPSHOT_UNSUPPORTED_VERSION:
        return "snapshot unsupported version";
    case FACTORY_RESULT_SNAPSHOT_TRUNCATED: return "snapshot truncated";
    case FACTORY_RESULT_SNAPSHOT_CORRUPT: return "snapshot corrupt";
    case FACTORY_RESULT_SNAPSHOT_SIZE_OVERFLOW:
        return "snapshot size overflow";
    case FACTORY_RESULT_SNAPSHOT_IO_ERROR: return "snapshot I/O error";
    case FACTORY_RESULT_POWER_NOT_APPLICABLE:
        return "power not applicable";
    case FACTORY_RESULT_POWER_NETWORK_NOT_FOUND:
        return "power network not found";
    case FACTORY_RESULT_POWER_OVERFLOW: return "power overflow";
    case FACTORY_RESULT_FLUID_INCOMPATIBLE: return "fluid incompatible";
    case FACTORY_RESULT_FLUID_MISMATCH: return "fluid mismatch";
    case FACTORY_RESULT_FLUID_CAPACITY_EXCEEDED:
        return "fluid capacity exceeded";
    case FACTORY_RESULT_INSUFFICIENT_FLUID: return "insufficient fluid";
    case FACTORY_RESULT_FLUID_NETWORK_NOT_FOUND:
        return "fluid network not found";
    }
    return "unknown result";
}

}

void FoundationSimulation::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("reset_demo"), &FoundationSimulation::reset_demo);
    ClassDB::bind_method(D_METHOD("step"), &FoundationSimulation::step);
    ClassDB::bind_method(
        D_METHOD("step_many", "count"), &FoundationSimulation::step_many
    );
    ClassDB::bind_method(
        D_METHOD("place_fluid_tank", "x", "y"),
        &FoundationSimulation::place_fluid_tank
    );
    ClassDB::bind_method(
        D_METHOD("insert_fluid", "destination_entity_id", "fluid_type",
            "quantity"),
        &FoundationSimulation::insert_fluid
    );
    ClassDB::bind_method(
        D_METHOD("remove_fluid", "source_entity_id", "quantity"),
        &FoundationSimulation::remove_fluid
    );
    ClassDB::bind_method(
        D_METHOD("transfer_fluid", "source_entity_id",
            "destination_entity_id", "quantity"),
        &FoundationSimulation::transfer_fluid
    );
    ClassDB::bind_method(D_METHOD("get_tick"), &FoundationSimulation::get_tick);
    ClassDB::bind_method(
        D_METHOD("get_entities"), &FoundationSimulation::get_entities
    );
    ClassDB::bind_method(
        D_METHOD("get_resources"), &FoundationSimulation::get_resources
    );
    ClassDB::bind_method(
        D_METHOD("get_power_edges"), &FoundationSimulation::get_power_edges
    );
    ClassDB::bind_method(
        D_METHOD("get_events"), &FoundationSimulation::get_events
    );
    ClassDB::bind_method(
        D_METHOD("clear_events"), &FoundationSimulation::clear_events
    );
    ClassDB::bind_method(
        D_METHOD("has_error"), &FoundationSimulation::has_error
    );
    ClassDB::bind_method(
        D_METHOD("get_last_error"), &FoundationSimulation::get_last_error
    );
    ClassDB::bind_method(
        D_METHOD("clear_error"), &FoundationSimulation::clear_error
    );
    ClassDB::bind_method(
        D_METHOD("rebuild_presentation"),
        &FoundationSimulation::rebuild_presentation
    );
    ClassDB::bind_method(
        D_METHOD("result_name", "result"), &FoundationSimulation::result_name
    );
}

FoundationSimulation::FoundationSimulation()
{
    presentation_ = factory_presentation_snapshot_create();
}

FoundationSimulation::~FoundationSimulation()
{
    destroy_state();
    factory_presentation_snapshot_destroy(presentation_);
}

void FoundationSimulation::destroy_state()
{
    factory_simulation_destroy(simulation_);
    factory_world_destroy(world_);
    simulation_ = nullptr;
    world_ = nullptr;
    if (presentation_ != nullptr)
        factory_presentation_snapshot_clear(presentation_);
}

FactoryResult FoundationSimulation::build_demo()
{
    FactoryResult result;
    world_ = factory_world_create(DEMO_WIDTH, DEMO_HEIGHT);
    if (world_ == nullptr)
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result = factory_world_add_resource(
        world_, 0, 2, FACTORY_RESOURCE_IRON, 500U
    );
    if (result != FACTORY_RESULT_OK)
        return result;
    result = factory_world_add_resource(
        world_, 0, 4, FACTORY_RESOURCE_COPPER, 500U
    );
    if (result != FACTORY_RESULT_OK)
        return result;

    simulation_ = factory_simulation_create_with_construction_units(
        world_, DEMO_CONSTRUCTION_UNITS
    );
    if (simulation_ == nullptr)
        return FACTORY_RESULT_OUT_OF_MEMORY;

    auto submit = [this](const FactoryCommand &command) {
        return factory_simulation_submit_command(simulation_, &command);
    };
    const FactoryCommand placements[] = {
        place(FACTORY_COMMAND_PLACE_EXTRACTOR, 0, 2, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 1, 2, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_REFINERY, 2, 2, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 3, 2, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 4, 2, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 5, 2, FACTORY_DIRECTION_SOUTH),
        place(FACTORY_COMMAND_PLACE_EXTRACTOR, 0, 4, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 1, 4, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_REFINERY, 2, 4, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 3, 4, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 4, 4, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 5, 4, FACTORY_DIRECTION_NORTH),
        place(FACTORY_COMMAND_PLACE_ASSEMBLER, 5, 3, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 6, 3, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_SPLITTER, 7, 3, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 7, 2, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_BELT, 7, 4, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_STORAGE, 8, 2),
        place(FACTORY_COMMAND_PLACE_STORAGE, 8, 4),
        place(FACTORY_COMMAND_PLACE_INSERTER, 9, 2, FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_STORAGE, 10, 2),
        place(FACTORY_COMMAND_PLACE_POWER_POLE, 1, 0),
        place(FACTORY_COMMAND_PLACE_POWER_POLE, 1, 3),
        place(FACTORY_COMMAND_PLACE_POWER_POLE, 4, 6),
        place(FACTORY_COMMAND_PLACE_POWER_POLE, 9, 5),
        place(FACTORY_COMMAND_PLACE_POWER_GENERATOR, 0, 0),
    };
    for (const FactoryCommand &command : placements) {
        result = submit(command);
        if (result != FACTORY_RESULT_OK)
            return result;
    }
    result = factory_simulation_tick(simulation_);
    if (result != FACTORY_RESULT_OK)
        return result;
    for (size_t index = 0;
         index < sizeof(placements) / sizeof(placements[0]); ++index) {
        const FactoryCommandResult *placement =
            factory_simulation_get_command_result(simulation_, index);
        if (placement == nullptr)
            return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
        if (placement->result != FACTORY_RESULT_OK)
            return placement->result;
    }

    const FactoryCommandResult *iron =
        factory_simulation_get_command_result(simulation_, 2U);
    const FactoryCommandResult *copper =
        factory_simulation_get_command_result(simulation_, 8U);
    const FactoryCommandResult *assembler =
        factory_simulation_get_command_result(simulation_, 12U);
    const FactoryCommandResult *storage =
        factory_simulation_get_command_result(simulation_, 17U);
    if (iron == nullptr || copper == nullptr || assembler == nullptr
        || storage == nullptr
        || iron->result != FACTORY_RESULT_OK
        || copper->result != FACTORY_RESULT_OK
        || assembler->result != FACTORY_RESULT_OK
        || storage->result != FACTORY_RESULT_OK)
        return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;

    FactoryCommand configuration[4] = {};
    configuration[0].type = FACTORY_COMMAND_SET_REFINERY_RECIPE;
    configuration[0].data.set_refinery_recipe = {
        iron->entity_id, FACTORY_RECIPE_IRON_PLATE
    };
    configuration[1].type = FACTORY_COMMAND_SET_REFINERY_RECIPE;
    configuration[1].data.set_refinery_recipe = {
        copper->entity_id, FACTORY_RECIPE_COPPER_PLATE
    };
    configuration[2].type = FACTORY_COMMAND_SET_ASSEMBLER_RECIPE;
    configuration[2].data.set_assembler_recipe = {
        assembler->entity_id,
        FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT
    };
    configuration[3].type = FACTORY_COMMAND_SET_STORAGE_OUTPUT;
    configuration[3].data.set_storage_output = {
        storage->entity_id, FACTORY_ITEM_ELECTRONIC_COMPONENT
    };
    for (const FactoryCommand &command : configuration) {
        result = submit(command);
        if (result != FACTORY_RESULT_OK)
            return result;
    }
    result = submit(place(FACTORY_COMMAND_PLACE_FLUID_TANK, 11, 7));
    if (result != FACTORY_RESULT_OK)
        return result;
    result = submit(place(FACTORY_COMMAND_PLACE_PIPE, 10, 7));
    if (result != FACTORY_RESULT_OK)
        return result;
    result = factory_simulation_submit_fluid_insert(
        simulation_, 27U, FACTORY_FLUID_WATER, 2500U);
    if (result != FACTORY_RESULT_OK)
        return result;
    result = factory_simulation_tick(simulation_);
    if (result != FACTORY_RESULT_OK)
        return result;
    const FactoryCommandResult *tank_result =
        factory_simulation_get_command_result(simulation_, 4U);
    if (tank_result == nullptr)
        return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    if (tank_result->result != FACTORY_RESULT_OK)
        return tank_result->result;
    if (tank_result->entity_id != 27U)
        return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    FactoryFluidStorageInspection tank_storage = {};
    result = factory_simulation_get_fluid_storage(
        simulation_, tank_result->entity_id, &tank_storage);
    if (result != FACTORY_RESULT_OK)
        return result;
    return factory_presentation_snapshot_rebuild(presentation_, simulation_);
}

int64_t FoundationSimulation::reset_demo()
{
    clear_error();
    destroy_state();
    return (int64_t)build_demo();
}

int64_t FoundationSimulation::step()
{
    clear_error();
    if (simulation_ == nullptr)
        return FACTORY_RESULT_INVALID_STATE;
    FactoryResult result = factory_simulation_tick(simulation_);
    if (result == FACTORY_RESULT_OK)
        result = factory_presentation_snapshot_rebuild(
            presentation_, simulation_
        );
    return (int64_t)result;
}

int64_t FoundationSimulation::step_many(int64_t count)
{
    if (count < 0 || count > MAX_STEP_MANY)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    for (int64_t index = 0; index < count; ++index) {
        int64_t result = step();
        if (result != FACTORY_RESULT_OK)
            return result;
    }
    return FACTORY_RESULT_OK;
}

int64_t FoundationSimulation::place_fluid_tank(int64_t x, int64_t y)
{
    if (simulation_ == nullptr || x < INT32_MIN || x > INT32_MAX
        || y < INT32_MIN || y > INT32_MAX)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command = place(
        FACTORY_COMMAND_PLACE_FLUID_TANK, (int32_t)x, (int32_t)y);
    FactoryResult result =
        factory_simulation_submit_command(simulation_, &command);
    return result == FACTORY_RESULT_OK ? step() : (int64_t)result;
}

int64_t FoundationSimulation::insert_fluid(
    int64_t destination_entity_id, int64_t fluid_type, int64_t quantity
)
{
    if (simulation_ == nullptr || destination_entity_id <= 0
        || destination_entity_id > UINT32_MAX || fluid_type <= 0
        || fluid_type > UINT32_MAX || quantity <= 0
        || quantity > UINT32_MAX)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryResult result = factory_simulation_submit_fluid_insert(
        simulation_, (FactoryEntityId)destination_entity_id,
        (FactoryFluidType)fluid_type, (FactoryFluidQuantity)quantity);
    return result == FACTORY_RESULT_OK ? step() : (int64_t)result;
}

int64_t FoundationSimulation::remove_fluid(
    int64_t source_entity_id, int64_t quantity
)
{
    if (simulation_ == nullptr || source_entity_id <= 0
        || source_entity_id > UINT32_MAX || quantity <= 0
        || quantity > UINT32_MAX)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryResult result = factory_simulation_submit_fluid_remove(
        simulation_, (FactoryEntityId)source_entity_id,
        (FactoryFluidQuantity)quantity);
    return result == FACTORY_RESULT_OK ? step() : (int64_t)result;
}

int64_t FoundationSimulation::transfer_fluid(
    int64_t source_entity_id, int64_t destination_entity_id, int64_t quantity
)
{
    if (simulation_ == nullptr || source_entity_id <= 0
        || source_entity_id > UINT32_MAX || destination_entity_id <= 0
        || destination_entity_id > UINT32_MAX || quantity <= 0
        || quantity > UINT32_MAX)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryResult result = factory_simulation_submit_fluid_transfer(
        simulation_, (FactoryEntityId)source_entity_id,
        (FactoryEntityId)destination_entity_id,
        (FactoryFluidQuantity)quantity);
    return result == FACTORY_RESULT_OK ? step() : (int64_t)result;
}

int64_t FoundationSimulation::get_tick() const
{
    if (simulation_ == nullptr)
        return 0;
    const uint64_t tick = factory_simulation_get_tick(simulation_);
    int64_t converted = 0;
    if (!foundation_godot::checked_uint64_to_godot_int(tick, &converted)) {
        set_conversion_error("simulation.tick", tick);
        return -1;
    }
    return converted;
}

void FoundationSimulation::set_conversion_error(
    const char *field, uint64_t value
) const
{
    last_error_ = String("unsigned integer field '") + field
        + "' exceeds Godot int range: " + String::num_uint64(value);
}

bool FoundationSimulation::set_unsigned(
    Dictionary *dictionary, const char *key, uint64_t value,
    const char *field
) const
{
    int64_t converted = 0;
    if (dictionary == nullptr
        || !foundation_godot::checked_uint64_to_godot_int(
            value, &converted
        )) {
        set_conversion_error(field, value);
        return false;
    }
    (*dictionary)[key] = converted;
    return true;
}

bool FoundationSimulation::entity_to_dictionary(
    const FactoryPresentationEntity &entity, Dictionary *out_value
) const
{
    if (out_value == nullptr)
        return false;
    Dictionary value;
    if (!set_unsigned(&value, "id", entity.entity_id, "entity.id"))
        return false;
    value["type"] = (int64_t)entity.entity_type;
    value["x"] = (int64_t)entity.x;
    value["y"] = (int64_t)entity.y;
    value["direction"] = (int64_t)entity.direction;
    value["status"] = (int64_t)entity.status;
    value["powered"] = entity.powered;
    switch (entity.entity_type) {
    case FACTORY_ENTITY_TYPE_EXTRACTOR:
        value["progress"] = (int64_t)entity.data.extractor.progress;
        value["duration"] = (int64_t)entity.data.extractor.duration;
        value["item"] = (int64_t)entity.data.extractor.output_item;
        value["quantity"] = (int64_t)entity.data.extractor.output_quantity;
        break;
    case FACTORY_ENTITY_TYPE_BELT:
        value["item"] = (int64_t)entity.data.belt.item;
        value["quantity"] = (int64_t)entity.data.belt.quantity;
        value["progress"] =
            (int64_t)entity.data.belt.movement_progress;
        value["duration"] =
            (int64_t)entity.data.belt.movement_duration;
        break;
    case FACTORY_ENTITY_TYPE_STORAGE: {
        Array inventory;
        for (uint32_t index = 0;
             index < FACTORY_PRESENTATION_STORAGE_ITEM_COUNT; ++index)
            inventory.append((int64_t)entity.data.storage.item_quantities[index]);
        value["inventory"] = inventory;
        value["capacity"] = (int64_t)entity.data.storage.total_capacity;
        value["configured_output"] =
            (int64_t)entity.data.storage.configured_output_item;
        value["item"] = (int64_t)entity.data.storage.output_item;
        value["quantity"] = (int64_t)entity.data.storage.output_quantity;
        break;
    }
    case FACTORY_ENTITY_TYPE_REFINERY:
        value["recipe"] = (int64_t)entity.data.refinery.recipe_id;
        value["progress"] = (int64_t)entity.data.refinery.progress;
        value["duration"] = (int64_t)entity.data.refinery.duration;
        value["input"] = item_slot(
            entity.data.refinery.input_item,
            entity.data.refinery.input_quantity
        );
        value["output"] = item_slot(
            entity.data.refinery.output_item,
            entity.data.refinery.output_quantity
        );
        break;
    case FACTORY_ENTITY_TYPE_ASSEMBLER: {
        Array inputs;
        for (uint32_t index = 0; index < FACTORY_ASSEMBLER_MAX_INPUT_TYPES;
             ++index)
            inputs.append(item_slot(
                entity.data.assembler.input_slots[index].item,
                entity.data.assembler.input_slots[index].count
            ));
        value["recipe"] = (int64_t)entity.data.assembler.recipe_id;
        value["progress"] = (int64_t)entity.data.assembler.progress;
        value["duration"] = (int64_t)entity.data.assembler.duration;
        value["inputs"] = inputs;
        value["output"] = item_slot(
            entity.data.assembler.output_item,
            entity.data.assembler.output_quantity
        );
        break;
    }
    case FACTORY_ENTITY_TYPE_SPLITTER:
        value["item"] = (int64_t)entity.data.splitter.item;
        value["quantity"] = (int64_t)entity.data.splitter.quantity;
        value["next_output"] =
            (int64_t)entity.data.splitter.next_output;
        break;
    case FACTORY_ENTITY_TYPE_INSERTER:
        value["item"] = (int64_t)entity.data.inserter.held_item;
        value["quantity"] = (int64_t)entity.data.inserter.held_quantity;
        value["progress"] = (int64_t)entity.data.inserter.progress;
        value["inserter_state"] = (int64_t)entity.data.inserter.state;
        break;
    case FACTORY_ENTITY_TYPE_POWER_POLE:
        if (!set_unsigned(
                &value, "network", entity.data.power_pole.network_id,
                "entity.power_pole.network_id"
            ))
            return false;
        value["connected_poles"] =
            (int64_t)entity.data.power_pole.connected_pole_count;
        break;
    case FACTORY_ENTITY_TYPE_POWER_GENERATOR:
        if (!set_unsigned(
                &value, "network", entity.data.power_source.network_id,
                "entity.power_source.network_id"
            ) || !set_unsigned(
                &value, "attached_pole_id",
                entity.data.power_source.attached_pole_id,
                "entity.power_source.attached_pole_id"
            ) || !set_unsigned(
                &value, "maximum_output",
                entity.data.power_source.maximum_output_per_tick,
                "entity.power_source.maximum_output"
            ) || !set_unsigned(
                &value, "allocated",
                entity.data.power_source.network_allocated_power,
                "entity.power_source.allocated"
            ))
            return false;
        value["connected"] = entity.data.power_source.connected;
        value["fuel_item"] =
            (int64_t)entity.data.power_source.burner.current_fuel_item;
        value["fuel_ticks"] =
            (int64_t)entity.data.power_source.burner.remaining_burn_ticks;
        value["fuel_active"] = entity.data.power_source.burner.active;
        if (!set_unsigned(
                &value, "energy_available",
                entity.data.power_source.burner.released_energy,
                "entity.power_source.burner.released_energy"
            ))
            return false;
        break;
    case FACTORY_ENTITY_TYPE_FLUID_TANK:
        value["fluid_type"] =
            (int64_t)entity.data.fluid_storage.fluid_type;
        value["fluid_quantity"] =
            (int64_t)entity.data.fluid_storage.quantity;
        value["fluid_capacity"] =
            (int64_t)entity.data.fluid_storage.capacity;
        value["network_id"] =
            (int64_t)entity.data.fluid_storage.network_id;
        break;
    case FACTORY_ENTITY_TYPE_PIPE:
        value["connection_mask"] =
            (int64_t)entity.data.pipe.connection_mask;
        value["network_id"] = (int64_t)entity.data.pipe.network_id;
        break;
    default:
        break;
    }
    *out_value = value;
    return true;
}

Array FoundationSimulation::get_entities() const
{
    Array values;
    if (presentation_ == nullptr)
        return values;
    const size_t count =
        factory_presentation_snapshot_get_entity_count(presentation_);
    for (size_t index = 0; index < count; ++index) {
        const FactoryPresentationEntity *entity =
            factory_presentation_snapshot_get_entity(presentation_, index);
        if (entity != nullptr) {
            Dictionary value;
            if (!entity_to_dictionary(*entity, &value))
                return Array();
            values.append(value);
        }
    }
    return values;
}

Array FoundationSimulation::get_resources() const
{
    Array values;
    if (presentation_ == nullptr)
        return values;
    const size_t count =
        factory_presentation_snapshot_get_resource_count(presentation_);
    for (size_t index = 0; index < count; ++index) {
        const FactoryPresentationResource *resource =
            factory_presentation_snapshot_get_resource(presentation_, index);
        if (resource == nullptr)
            continue;
        Dictionary value;
        value["x"] = (int64_t)resource->x;
        value["y"] = (int64_t)resource->y;
        value["type"] = (int64_t)resource->resource_type;
        value["remaining"] = (int64_t)resource->remaining_quantity;
        if (!set_unsigned(
                &value, "occupying_entity_id",
                resource->occupying_entity_id,
                "resource.occupying_entity_id"
            ))
            return Array();
        values.append(value);
    }
    return values;
}

Array FoundationSimulation::get_power_edges() const
{
    Array values;
    if (presentation_ == nullptr)
        return values;
    const size_t count =
        factory_presentation_snapshot_get_power_edge_count(presentation_);
    for (size_t index = 0; index < count; ++index) {
        const FactoryPresentationPowerEdge *edge =
            factory_presentation_snapshot_get_power_edge(
                presentation_, index
            );
        if (edge == nullptr)
            continue;
        Dictionary value;
        if (!set_unsigned(
                &value, "a", edge->pole_a, "power_edge.a"
            ) || !set_unsigned(
                &value, "b", edge->pole_b, "power_edge.b"
            ))
            return Array();
        values.append(value);
    }
    return values;
}

Array FoundationSimulation::get_events() const
{
    Array values;
    if (simulation_ == nullptr)
        return values;
    const size_t count = factory_simulation_get_event_count(simulation_);
    for (size_t index = 0; index < count; ++index) {
        const FactoryEvent *event =
            factory_simulation_get_event(simulation_, index);
        if (event == nullptr)
            continue;
        Dictionary value;
        value["type"] = (int64_t)event->type;
        if (!set_unsigned(
                &value, "tick", event->tick, "event.tick"
            ) || !set_unsigned(
                &value, "entity_id", event->entity_id,
                "event.entity_id"
            ) || !set_unsigned(
                &value, "related_entity_id",
                event->related_entity_id,
                "event.related_entity_id"
            ))
            return Array();
        value["entity_type"] = (int64_t)event->entity_type;
        value["item_type"] = (int64_t)event->item_type;
        value["fluid_type"] = (int64_t)event->fluid_type;
        value["quantity"] = (int64_t)event->quantity;
        values.append(value);
    }
    return values;
}

void FoundationSimulation::clear_events()
{
    factory_simulation_clear_events(simulation_);
}

bool FoundationSimulation::has_error() const
{
    return !last_error_.is_empty();
}

String FoundationSimulation::get_last_error() const
{
    return last_error_;
}

void FoundationSimulation::clear_error()
{
    last_error_ = String();
}

int64_t FoundationSimulation::rebuild_presentation()
{
    if (presentation_ == nullptr || simulation_ == nullptr)
        return FACTORY_RESULT_INVALID_STATE;
    return (int64_t)factory_presentation_snapshot_rebuild(
        presentation_, simulation_
    );
}

String FoundationSimulation::result_name(int64_t result) const
{
    return String(result_name_c((FactoryResult)result));
}

}
