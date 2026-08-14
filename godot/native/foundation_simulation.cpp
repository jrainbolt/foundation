#include "foundation_simulation.h"
extern "C" {
#include "logistics_endpoint_internal.h"
}

#include <climits>

#include <godot_cpp/core/class_db.hpp>

namespace godot {
namespace {

constexpr uint32_t DEMO_WIDTH = 48U;
constexpr uint32_t DEMO_HEIGHT = 32U;
constexpr int32_t DEMO_OFFSET_X = 18;
constexpr int32_t DEMO_OFFSET_Y = 12;
constexpr FactoryConstructionMaterial DEMO_CONSTRUCTION_UNITS = 1000U;
constexpr int64_t MAX_STEP_MANY = 10000;

FactoryCommand place(
    FactoryCommandType type, int32_t x, int32_t y,
    FactoryDirection first = FACTORY_DIRECTION_NORTH,
    FactoryDirection second = FACTORY_DIRECTION_NORTH
)
{
    FactoryCommand command = {};
    x += DEMO_OFFSET_X;
    y += DEMO_OFFSET_Y;
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
    case FACTORY_COMMAND_PLACE_WATER_EXTRACTOR:
        command.data.place_water_extractor = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_BOILER:
        command.data.place_boiler = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_STEAM_ENGINE:
        command.data.place_steam_engine = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_SOLAR_GENERATOR:
        command.data.place_solar_generator = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_ACCUMULATOR:
        command.data.place_accumulator = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_REACTOR_CORE:
        command.data.place_reactor_core = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR:
        command.data.place_heat_conductor = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_HEAT_EXCHANGER:
        command.data.place_heat_exchanger = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_STEAM_TURBINE:
        command.data.place_steam_turbine = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_STEAM_CONDENSER:
        command.data.place_steam_condenser = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_RESEARCH_LAB:
        command.data.place_research_lab = {x, y};
        break;
    case FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT:
        command.data.place_construction_depot={x,y};
        break;
    case FACTORY_COMMAND_PLACE_RAIL:
        command.data.place_rail={x,y,(uint32_t)first};break;
    case FACTORY_COMMAND_PLACE_RAIL_STATION:
        command.data.place_rail_station={x,y,first};break;
    case FACTORY_COMMAND_PLACE_RAIL_SWITCH:
        command.data.place_rail_switch={x,y,(uint32_t)first};break;
    case FACTORY_COMMAND_PLACE_RAIL_SIGNAL:
        command.data.place_rail_signal={x,y,first};break;
    case FACTORY_COMMAND_PLACE_RAIL_CHAIN_SIGNAL:
        command.data.place_rail_chain_signal={x,y,first};break;
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
    case FACTORY_RESULT_FUEL_INCOMPATIBLE:
        return "fuel incompatible";
    case FACTORY_RESULT_FUEL_INVENTORY_FULL:
        return "fuel inventory full";
    case FACTORY_RESULT_HEAT_NETWORK_NOT_FOUND:
        return "heat network not found";
    case FACTORY_RESULT_TECHNOLOGY_INVALID: return "technology invalid";
    case FACTORY_RESULT_TECHNOLOGY_ALREADY_COMPLETED:
        return "technology already completed";
    case FACTORY_RESULT_TECHNOLOGY_PREREQUISITES_MISSING:
        return "technology prerequisites missing";
    case FACTORY_RESULT_TECHNOLOGY_LOCKED:
        return "technology locked";
    case FACTORY_RESULT_TERRAIN_BLOCKED:
        return "terrain blocked";
    case FACTORY_RESULT_WORLD_GENERATION_FAILED:
        return "world generation failed";
    case FACTORY_RESULT_NO_CONSTRUCTION_SUPPLY:
        return "no construction supply";
    case FACTORY_RESULT_CONSTRUCTION_SUPPLY_INSUFFICIENT:
        return "construction supply insufficient";
    case FACTORY_RESULT_CONSTRUCTION_DEPOT_NOT_EMPTY:
        return "construction depot not empty";
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
        D_METHOD("queue_place_entity","entity_type","x","y","direction"),
        &FoundationSimulation::queue_place_entity);
    ClassDB::bind_method(
        D_METHOD("queue_demolish_entity","entity_id"),
        &FoundationSimulation::queue_demolish_entity);
    ClassDB::bind_method(
        D_METHOD("queue_set_assembler_recipe","entity_id","recipe_id"),
        &FoundationSimulation::queue_set_assembler_recipe);
    ClassDB::bind_method(
        D_METHOD("queue_set_storage_output","entity_id","item_type"),
        &FoundationSimulation::queue_set_storage_output);
    ClassDB::bind_method(
        D_METHOD("queue_set_rail_switch_branch","entity_id","branch"),
        &FoundationSimulation::queue_set_rail_switch_branch);
    ClassDB::bind_method(
        D_METHOD("queue_set_rail_station_freight_mode","entity_id","mode"),
        &FoundationSimulation::queue_set_rail_station_freight_mode);
    ClassDB::bind_method(
        D_METHOD("queue_set_rail_station_freight_item","entity_id","item"),
        &FoundationSimulation::queue_set_rail_station_freight_item);
    ClassDB::bind_method(
        D_METHOD("queue_place_locomotive","rail_entity_id","direction"),
        &FoundationSimulation::queue_place_locomotive);
    ClassDB::bind_method(D_METHOD("queue_place_cargo_wagon","rail_entity_id","direction"),
        &FoundationSimulation::queue_place_cargo_wagon);
    ClassDB::bind_method(D_METHOD("queue_couple_rear_wagon","locomotive_id","wagon_id"),
        &FoundationSimulation::queue_couple_rear_wagon);
    ClassDB::bind_method(D_METHOD("queue_decouple_rear_wagon","locomotive_id"),
        &FoundationSimulation::queue_decouple_rear_wagon);
    ClassDB::bind_method(D_METHOD("queue_set_train_destination","train_id","station_id"),
        &FoundationSimulation::queue_set_train_destination);
    ClassDB::bind_method(D_METHOD("queue_clear_train_destination","train_id"),
        &FoundationSimulation::queue_clear_train_destination);
    ClassDB::bind_method(D_METHOD("queue_replan_train_route","train_id"),
        &FoundationSimulation::queue_replan_train_route);
    ClassDB::bind_method(D_METHOD("get_train_route","train_id"),
        &FoundationSimulation::get_train_route);
    ClassDB::bind_method(D_METHOD("queue_train_schedule_add_stop","train_id",
        "station_id","wait_condition","wait_value"),
        &FoundationSimulation::queue_train_schedule_add_stop);
    ClassDB::bind_method(D_METHOD("queue_train_schedule_remove_stop","train_id",
        "index"),&FoundationSimulation::queue_train_schedule_remove_stop);
    ClassDB::bind_method(D_METHOD("queue_train_schedule_clear","train_id"),
        &FoundationSimulation::queue_train_schedule_clear);
    ClassDB::bind_method(D_METHOD("queue_train_schedule_set_enabled","train_id",
        "enabled"),&FoundationSimulation::queue_train_schedule_set_enabled);
    ClassDB::bind_method(D_METHOD("get_train_schedule","train_id"),
        &FoundationSimulation::get_train_schedule);
    ClassDB::bind_method(D_METHOD("get_command_results"),
        &FoundationSimulation::get_command_results);
    ClassDB::bind_method(D_METHOD("get_build_catalog"),
        &FoundationSimulation::get_build_catalog);
    ClassDB::bind_method(D_METHOD("get_assembler_recipe_catalog"),
        &FoundationSimulation::get_assembler_recipe_catalog);
    ClassDB::bind_method(D_METHOD("get_item_catalog"),
        &FoundationSimulation::get_item_catalog);
    ClassDB::bind_method(D_METHOD("get_construction_units"),
        &FoundationSimulation::get_construction_units);
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
    ClassDB::bind_method(D_METHOD("get_day"), &FoundationSimulation::get_day);
    ClassDB::bind_method(
        D_METHOD("get_time_of_day"), &FoundationSimulation::get_time_of_day
    );
    ClassDB::bind_method(
        D_METHOD("get_research"), &FoundationSimulation::get_research
    );
    ClassDB::bind_method(
        D_METHOD("get_entities"), &FoundationSimulation::get_entities
    );
    ClassDB::bind_method(
        D_METHOD("get_resources"), &FoundationSimulation::get_resources
    );
    ClassDB::bind_method(
        D_METHOD("get_terrain"), &FoundationSimulation::get_terrain
    );
    ClassDB::bind_method(D_METHOD("get_entity_telemetry","entity_id"),
        &FoundationSimulation::get_entity_telemetry);
    ClassDB::bind_method(D_METHOD("get_start_x"),&FoundationSimulation::get_start_x);
    ClassDB::bind_method(D_METHOD("get_start_y"),&FoundationSimulation::get_start_y);
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
    FactoryTelemetryConfig config;
    factory_telemetry_default_config(&config);
    telemetry_=factory_telemetry_create(&config);
}

FoundationSimulation::~FoundationSimulation()
{
    destroy_state();
    factory_telemetry_destroy(telemetry_);
    factory_presentation_snapshot_destroy(presentation_);
}

void FoundationSimulation::destroy_state()
{
    factory_simulation_destroy(simulation_);
    factory_world_destroy(world_);
    simulation_ = nullptr;
    world_ = nullptr;
    factory_telemetry_clear(telemetry_);
    if (presentation_ != nullptr)
        factory_presentation_snapshot_clear(presentation_);
}

FactoryResult FoundationSimulation::build_demo()
{
    FactoryResult result;
    world_ = factory_world_create_with_seed(DEMO_WIDTH,DEMO_HEIGHT,UINT64_C(42));
    if (world_ == nullptr)
        return FACTORY_RESULT_OUT_OF_MEMORY;
    FactoryWorldGenerationConfig generation;
    factory_world_generation_default_config(&generation);
    generation.starting_area_radius=8U;
    generation.starter_distance=9U;
    result=factory_world_generate(world_,&generation);
    if(result!=FACTORY_RESULT_OK)return result;
    result = factory_world_add_resource(
        world_, DEMO_OFFSET_X, DEMO_OFFSET_Y+2, FACTORY_RESOURCE_IRON, 500U
    );
    if (result != FACTORY_RESULT_OK)
        return result;
    result = factory_world_add_resource(
        world_, DEMO_OFFSET_X, DEMO_OFFSET_Y+4, FACTORY_RESOURCE_COPPER, 500U
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
        place(FACTORY_COMMAND_PLACE_STORAGE, 11, 3),
        place(FACTORY_COMMAND_PLACE_INSERTER, 11, 4, FACTORY_DIRECTION_SOUTH),
        place(FACTORY_COMMAND_PLACE_RESEARCH_LAB, 11, 5),
        place(FACTORY_COMMAND_PLACE_POWER_POLE, 12, 6),
        place(FACTORY_COMMAND_PLACE_POWER_GENERATOR, 12, 7),
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

    /* Seed a visible logistics source, then let the ordinary inserter and
     * powered research lab perform all research work. This setup-only insert
     * does not bypass the lab or mutate the research controller. */
    if (factory_logistics_endpoint_insert(simulation_,
            {26U, FACTORY_LOGISTICS_SLOT_BURNER_INPUT},
            FACTORY_ITEM_BIOMASS_PELLET) != FACTORY_LOGISTICS_RESULT_OK
        || factory_logistics_endpoint_insert(simulation_,
            {31U, FACTORY_LOGISTICS_SLOT_BURNER_INPUT},
            FACTORY_ITEM_BIOMASS_PELLET) != FACTORY_LOGISTICS_RESULT_OK)
        return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    for (int science = 0; science < 6; ++science) {
        if (factory_logistics_endpoint_insert(simulation_,
                {27U, FACTORY_LOGISTICS_SLOT_STORAGE_INPUT},
                FACTORY_ITEM_BASIC_SCIENCE) != FACTORY_LOGISTICS_RESULT_OK)
            return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    }
    FactoryCommand science_output = {};
    science_output.type = FACTORY_COMMAND_SET_STORAGE_OUTPUT;
    science_output.data.set_storage_output = {
        27U, FACTORY_ITEM_BASIC_SCIENCE};
    result = submit(science_output);
    if (result != FACTORY_RESULT_OK) return result;
    FactoryCommand select_research = {};
    select_research.type = FACTORY_COMMAND_SELECT_RESEARCH;
    select_research.data.select_research.technology_id =
        FACTORY_TECHNOLOGY_BASIC_AUTOMATION;
    result = submit(select_research);
    if (result != FACTORY_RESULT_OK) return result;
    for (int tick = 0; tick < 32; ++tick) {
        result = factory_simulation_tick(simulation_);
        if (result != FACTORY_RESULT_OK) return result;
    }
    select_research.data.select_research.technology_id =
        FACTORY_TECHNOLOGY_FLUID_HANDLING;
    result = submit(select_research);
    if (result != FACTORY_RESULT_OK) return result;
    for (int tick = 0; tick < 20; ++tick) {
        result = factory_simulation_tick(simulation_);
        if (result != FACTORY_RESULT_OK) return result;
    }

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
        FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE
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
    result = submit(place(FACTORY_COMMAND_PLACE_WATER_EXTRACTOR, 7, 6));
    if (result != FACTORY_RESULT_OK)
        return result;
    result = submit(place(FACTORY_COMMAND_PLACE_PIPE, 8, 6));
    if (result != FACTORY_RESULT_OK)
        return result;
    result = submit(place(FACTORY_COMMAND_PLACE_BOILER, 9, 6));
    if (result != FACTORY_RESULT_OK)
        return result;
    result = submit(place(FACTORY_COMMAND_PLACE_PIPE, 10, 6));
    if (result != FACTORY_RESULT_OK)
        return result;
    result = submit(place(FACTORY_COMMAND_PLACE_STEAM_ENGINE, 11, 6));
    if (result != FACTORY_RESULT_OK)
        return result;
    /* The turbine's east tile (10, 0) is reserved for its exhaust pipe (see
     * below), so the solar generator and accumulator that used to sit at
     * (11, 0) and (10, 0) move to (12, 2) and (12, 1); both stay within
     * this pole's radius-3 reach. */
    result = submit(place(FACTORY_COMMAND_PLACE_SOLAR_GENERATOR, 12, 2));
    if (result != FACTORY_RESULT_OK)
        return result;
    result = submit(place(FACTORY_COMMAND_PLACE_POWER_POLE, 11, 1));
    if (result != FACTORY_RESULT_OK)
        return result;
    result = submit(place(FACTORY_COMMAND_PLACE_ACCUMULATOR, 12, 1));
    if (result != FACTORY_RESULT_OK)
        return result;
    result = submit(place(
        FACTORY_COMMAND_PLACE_INSERTER, 10, 1, FACTORY_DIRECTION_EAST));
    if (result != FACTORY_RESULT_OK)
        return result;
    result = submit(place(FACTORY_COMMAND_PLACE_REACTOR_CORE, 6, 1));
    if (result != FACTORY_RESULT_OK)
        return result;
    FactoryCommand reactor_fuel = {};
    reactor_fuel.type = FACTORY_COMMAND_INSERT_REACTOR_FUEL;
    reactor_fuel.data.insert_reactor_fuel = {
        43U, FACTORY_NUCLEAR_FUEL_BASIC_ROD};
    result = submit(reactor_fuel);
    if (result != FACTORY_RESULT_OK)
        return result;
    result = submit(place(FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR, 9, 1));
    if (result != FACTORY_RESULT_OK) return result;
    result = submit(place(FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR, 8, 1));
    if (result != FACTORY_RESULT_OK) return result;
    result = submit(place(FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR, 7, 1));
    if (result != FACTORY_RESULT_OK) return result;
    result = submit(place(FACTORY_COMMAND_PLACE_HEAT_EXCHANGER, 7, 0));
    if (result != FACTORY_RESULT_OK) return result;
    result = submit(place(FACTORY_COMMAND_PLACE_PIPE, 6, 0));
    if (result != FACTORY_RESULT_OK) return result;
    result = submit(place(FACTORY_COMMAND_PLACE_FLUID_TANK, 5, 0));
    if (result != FACTORY_RESULT_OK) return result;
    result = submit(place(FACTORY_COMMAND_PLACE_PIPE, 8, 0));
    if (result != FACTORY_RESULT_OK) return result;
    result = submit(place(FACTORY_COMMAND_PLACE_STEAM_TURBINE, 9, 0));
    if (result != FACTORY_RESULT_OK) return result;
    /* Closes the real thermal loop: the turbine's east port carries its own
     * exhaust steam (never live steam), one pipe tile bridges it directly
     * to the Steam Condenser's west input port, and the condenser recovers
     * that exhaust into water in its own storage. The condenser does not
     * tap the boiler/heat-exchanger side of the network at all, so it never
     * competes with the turbine for fresh steam. */
    result = submit(place(FACTORY_COMMAND_PLACE_PIPE, 10, 0));
    if (result != FACTORY_RESULT_OK) return result;
    result = submit(place(FACTORY_COMMAND_PLACE_STEAM_CONDENSER, 11, 0));
    if (result != FACTORY_RESULT_OK) return result;
    result = factory_simulation_submit_fluid_insert(
        simulation_, 49U, FACTORY_FLUID_WATER, 200U);
    if (result != FACTORY_RESULT_OK) return result;
    result = factory_simulation_submit_fluid_insert(
        simulation_, 32U, FACTORY_FLUID_WATER, 2500U);
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
    if (tank_result->entity_id != 32U)
        return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    FactoryFluidStorageInspection tank_storage = {};
    result = factory_simulation_get_fluid_storage(
        simulation_, tank_result->entity_id, &tank_storage);
    if (result != FACTORY_RESULT_OK)
        return result;
    const FactoryCommand rail_demo[] = {
        place(FACTORY_COMMAND_PLACE_RAIL,0,-2,
            (FactoryDirection)FACTORY_RAIL_CURVE_SE),
        place(FACTORY_COMMAND_PLACE_RAIL,0,-1,
            (FactoryDirection)FACTORY_RAIL_VERTICAL),
        place(FACTORY_COMMAND_PLACE_RAIL,1,-2,
            (FactoryDirection)FACTORY_RAIL_HORIZONTAL),
        place(FACTORY_COMMAND_PLACE_RAIL,2,-2,
            (FactoryDirection)FACTORY_RAIL_HORIZONTAL),
        place(FACTORY_COMMAND_PLACE_RAIL,3,-2,
            (FactoryDirection)FACTORY_RAIL_HORIZONTAL),
        place(FACTORY_COMMAND_PLACE_RAIL_SWITCH,4,-2,
            (FactoryDirection)FACTORY_RAIL_SWITCH_STEM_WEST),
        place(FACTORY_COMMAND_PLACE_RAIL,4,-3,
            (FactoryDirection)FACTORY_RAIL_VERTICAL),
        place(FACTORY_COMMAND_PLACE_RAIL,4,-1,
            (FactoryDirection)FACTORY_RAIL_VERTICAL),
        place(FACTORY_COMMAND_PLACE_RAIL_STATION,3,-3,
            FACTORY_DIRECTION_SOUTH)
    };
    for(const FactoryCommand &command:rail_demo){
        result=submit(command);if(result!=FACTORY_RESULT_OK)return result;
    }
    result=factory_simulation_tick(simulation_);
    if(result!=FACTORY_RESULT_OK)return result;
    for(size_t i=0U;i<sizeof(rail_demo)/sizeof(rail_demo[0]);++i){
        const FactoryCommandResult *rail_result=
            factory_simulation_get_command_result(simulation_,i);
        if(rail_result==nullptr||rail_result->result!=FACTORY_RESULT_OK)
            return rail_result==nullptr?FACTORY_RESULT_INTERNAL_STATE_MISMATCH
                :rail_result->result;
    }
    const FactoryCommand signal_demo[] = {
        place(FACTORY_COMMAND_PLACE_RAIL_CHAIN_SIGNAL,2,-1,
            FACTORY_DIRECTION_EAST),
        place(FACTORY_COMMAND_PLACE_RAIL_SIGNAL,3,-1,
            FACTORY_DIRECTION_EAST)
    };
    for(const FactoryCommand &command:signal_demo){
        result=submit(command);if(result!=FACTORY_RESULT_OK)return result;
    }
    FactoryCommand locomotive={};locomotive.type=FACTORY_COMMAND_PLACE_LOCOMOTIVE;
    locomotive.data.place_locomotive={57U,FACTORY_DIRECTION_EAST};
    result=submit(locomotive);if(result!=FACTORY_RESULT_OK)return result;
    FactoryCommand wagon1={};wagon1.type=FACTORY_COMMAND_PLACE_CARGO_WAGON;
    wagon1.data.place_cargo_wagon={56U,FACTORY_DIRECTION_EAST};
    FactoryCommand wagon2={};wagon2.type=FACTORY_COMMAND_PLACE_CARGO_WAGON;
    wagon2.data.place_cargo_wagon={54U,FACTORY_DIRECTION_EAST};
    result=submit(wagon1);if(result!=FACTORY_RESULT_OK)return result;
    result=submit(wagon2);if(result!=FACTORY_RESULT_OK)return result;
    FactoryCommand couple1={};couple1.type=FACTORY_COMMAND_COUPLE_REAR_WAGON;
    couple1.data.couple_rear_wagon={65U,66U};
    FactoryCommand couple2={};couple2.type=FACTORY_COMMAND_COUPLE_REAR_WAGON;
    couple2.data.couple_rear_wagon={65U,67U};
    result=submit(couple1);if(result!=FACTORY_RESULT_OK)return result;
    result=submit(couple2);if(result!=FACTORY_RESULT_OK)return result;
    FactoryCommand schedule_stop={};
    schedule_stop.type=FACTORY_COMMAND_TRAIN_SCHEDULE_ADD_STOP;
    schedule_stop.data.train_schedule_add_stop={
        65U,62U,FACTORY_TRAIN_WAIT_TIME,120U};
    result=submit(schedule_stop);if(result!=FACTORY_RESULT_OK)return result;
    FactoryCommand schedule_enable={};
    schedule_enable.type=FACTORY_COMMAND_TRAIN_SCHEDULE_SET_ENABLED;
    schedule_enable.data.train_schedule_set_enabled={65U,true};
    result=submit(schedule_enable);if(result!=FACTORY_RESULT_OK)return result;
    result=factory_simulation_tick(simulation_);if(result!=FACTORY_RESULT_OK)return result;
    for(size_t i=0U;i<9U;++i){const FactoryCommandResult*r=
        factory_simulation_get_command_result(simulation_,i);
        if(r==nullptr||r->result!=FACTORY_RESULT_OK)
            return r==nullptr?FACTORY_RESULT_INTERNAL_STATE_MISMATCH:r->result;}
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
    if (result == FACTORY_RESULT_OK && telemetry_ != nullptr) {
        FactoryTelemetryResult telemetry_result=
            factory_telemetry_observe_step(telemetry_,simulation_);
        if(telemetry_result!=FACTORY_TELEMETRY_RESULT_OK)
            last_error_="telemetry observation unavailable";
    }
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

int64_t FoundationSimulation::queue_place_entity(
    int64_t entity_type,int64_t x,int64_t y,int64_t direction)
{
    if (simulation_==nullptr || entity_type<=FACTORY_ENTITY_TYPE_NONE
        || (entity_type>FACTORY_ENTITY_TYPE_RAIL_SWITCH
            &&entity_type!=FACTORY_ENTITY_TYPE_RAIL_SIGNAL
            &&entity_type!=FACTORY_ENTITY_TYPE_RAIL_CHAIN_SIGNAL)
        || x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX
        || direction<0
        || (entity_type==FACTORY_ENTITY_TYPE_RAIL
            ?direction>=FACTORY_RAIL_GEOMETRY_COUNT
            :entity_type==FACTORY_ENTITY_TYPE_RAIL_SWITCH
                ?direction>=FACTORY_RAIL_SWITCH_GEOMETRY_COUNT
                :direction>FACTORY_DIRECTION_WEST))
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommandType command_type;
    switch ((FactoryEntityType)entity_type) {
    case FACTORY_ENTITY_TYPE_EXTRACTOR: command_type=FACTORY_COMMAND_PLACE_EXTRACTOR;break;
    case FACTORY_ENTITY_TYPE_BELT: command_type=FACTORY_COMMAND_PLACE_BELT;break;
    case FACTORY_ENTITY_TYPE_REFINERY: command_type=FACTORY_COMMAND_PLACE_REFINERY;break;
    case FACTORY_ENTITY_TYPE_ASSEMBLER: command_type=FACTORY_COMMAND_PLACE_ASSEMBLER;break;
    case FACTORY_ENTITY_TYPE_STORAGE: command_type=FACTORY_COMMAND_PLACE_STORAGE;break;
    case FACTORY_ENTITY_TYPE_SPLITTER: command_type=FACTORY_COMMAND_PLACE_SPLITTER;break;
    case FACTORY_ENTITY_TYPE_INSERTER: command_type=FACTORY_COMMAND_PLACE_INSERTER;break;
    case FACTORY_ENTITY_TYPE_POWER_POLE: command_type=FACTORY_COMMAND_PLACE_POWER_POLE;break;
    case FACTORY_ENTITY_TYPE_POWER_GENERATOR: command_type=FACTORY_COMMAND_PLACE_POWER_GENERATOR;break;
    case FACTORY_ENTITY_TYPE_FLUID_TANK: command_type=FACTORY_COMMAND_PLACE_FLUID_TANK;break;
    case FACTORY_ENTITY_TYPE_PIPE: command_type=FACTORY_COMMAND_PLACE_PIPE;break;
    case FACTORY_ENTITY_TYPE_WATER_EXTRACTOR: command_type=FACTORY_COMMAND_PLACE_WATER_EXTRACTOR;break;
    case FACTORY_ENTITY_TYPE_BOILER: command_type=FACTORY_COMMAND_PLACE_BOILER;break;
    case FACTORY_ENTITY_TYPE_STEAM_ENGINE: command_type=FACTORY_COMMAND_PLACE_STEAM_ENGINE;break;
    case FACTORY_ENTITY_TYPE_SOLAR_GENERATOR: command_type=FACTORY_COMMAND_PLACE_SOLAR_GENERATOR;break;
    case FACTORY_ENTITY_TYPE_ACCUMULATOR: command_type=FACTORY_COMMAND_PLACE_ACCUMULATOR;break;
    case FACTORY_ENTITY_TYPE_REACTOR_CORE: command_type=FACTORY_COMMAND_PLACE_REACTOR_CORE;break;
    case FACTORY_ENTITY_TYPE_HEAT_CONDUCTOR: command_type=FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR;break;
    case FACTORY_ENTITY_TYPE_HEAT_EXCHANGER: command_type=FACTORY_COMMAND_PLACE_HEAT_EXCHANGER;break;
    case FACTORY_ENTITY_TYPE_STEAM_TURBINE: command_type=FACTORY_COMMAND_PLACE_STEAM_TURBINE;break;
    case FACTORY_ENTITY_TYPE_STEAM_CONDENSER: command_type=FACTORY_COMMAND_PLACE_STEAM_CONDENSER;break;
    case FACTORY_ENTITY_TYPE_RESEARCH_LAB: command_type=FACTORY_COMMAND_PLACE_RESEARCH_LAB;break;
    case FACTORY_ENTITY_TYPE_CONSTRUCTION_DEPOT: command_type=FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT;break;
    case FACTORY_ENTITY_TYPE_RAIL: command_type=FACTORY_COMMAND_PLACE_RAIL;break;
    case FACTORY_ENTITY_TYPE_RAIL_STATION: command_type=FACTORY_COMMAND_PLACE_RAIL_STATION;break;
    case FACTORY_ENTITY_TYPE_RAIL_SWITCH: command_type=FACTORY_COMMAND_PLACE_RAIL_SWITCH;break;
    case FACTORY_ENTITY_TYPE_RAIL_SIGNAL: command_type=FACTORY_COMMAND_PLACE_RAIL_SIGNAL;break;
    case FACTORY_ENTITY_TYPE_RAIL_CHAIN_SIGNAL:
        command_type=FACTORY_COMMAND_PLACE_RAIL_CHAIN_SIGNAL;break;
    default:return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    FactoryCommand command=place(command_type,(int32_t)x,(int32_t)y,
        (FactoryDirection)direction,(FactoryDirection)direction);
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_demolish_entity(int64_t entity_id)
{
    if (simulation_==nullptr || entity_id<=0 || entity_id>UINT32_MAX)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};
    command.type=FACTORY_COMMAND_DEMOLISH_ENTITY;
    command.data.demolish_entity.entity_id=(FactoryEntityId)entity_id;
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_set_assembler_recipe(
    int64_t entity_id,int64_t recipe_id)
{
    if (simulation_==nullptr || entity_id<=0 || entity_id>UINT32_MAX
        || recipe_id<FACTORY_ASSEMBLER_RECIPE_NONE
        || recipe_id>=FACTORY_ASSEMBLER_RECIPE_COUNT)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};
    command.type=FACTORY_COMMAND_SET_ASSEMBLER_RECIPE;
    command.data.set_assembler_recipe.assembler_entity=
        (FactoryEntityId)entity_id;
    command.data.set_assembler_recipe.recipe_id=
        (FactoryAssemblerRecipeId)recipe_id;
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_set_storage_output(
    int64_t entity_id,int64_t item_type)
{
    if (simulation_==nullptr || entity_id<=0 || entity_id>UINT32_MAX
        || item_type<FACTORY_ITEM_NONE
        || item_type>FACTORY_ITEM_CONSTRUCTION_MATERIAL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};
    command.type=FACTORY_COMMAND_SET_STORAGE_OUTPUT;
    command.data.set_storage_output.storage_entity=(FactoryEntityId)entity_id;
    command.data.set_storage_output.item=(FactoryItemType)item_type;
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_set_rail_switch_branch(
    int64_t entity_id,int64_t branch)
{
    if(simulation_==nullptr||entity_id<=0||entity_id>UINT32_MAX
        ||branch<FACTORY_RAIL_SWITCH_BRANCH_A
        ||branch>=FACTORY_RAIL_SWITCH_BRANCH_COUNT)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};
    command.type=FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH;
    command.data.set_rail_switch_branch={
        (FactoryEntityId)entity_id,(uint32_t)branch};
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_set_rail_station_freight_mode(
    int64_t entity_id,int64_t mode)
{
    if(simulation_==nullptr||entity_id<=0||entity_id>UINT32_MAX
        ||mode<FACTORY_RAIL_STATION_FREIGHT_DISABLED
        ||mode>FACTORY_RAIL_STATION_FREIGHT_UNLOAD)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};
    command.type=FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_MODE;
    command.data.set_rail_station_freight_mode={
        (FactoryEntityId)entity_id,(uint32_t)mode};
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_set_rail_station_freight_item(
    int64_t entity_id,int64_t item)
{
    if(simulation_==nullptr||entity_id<=0||entity_id>UINT32_MAX
        ||item<FACTORY_ITEM_NONE||item>FACTORY_ITEM_CONSTRUCTION_MATERIAL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};
    command.type=FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_ITEM;
    command.data.set_rail_station_freight_item={
        (FactoryEntityId)entity_id,(FactoryItemType)item};
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_place_locomotive(
    int64_t rail_entity_id,int64_t direction)
{
    if(simulation_==nullptr||rail_entity_id<=0||rail_entity_id>UINT32_MAX
        ||direction<FACTORY_DIRECTION_NORTH||direction>FACTORY_DIRECTION_WEST)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};command.type=FACTORY_COMMAND_PLACE_LOCOMOTIVE;
    command.data.place_locomotive={(FactoryEntityId)rail_entity_id,
        (FactoryDirection)direction};
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_set_train_destination(
    int64_t train_id,int64_t station_id)
{
    if(simulation_==nullptr||train_id<=0||train_id>UINT32_MAX
        ||station_id<=0||station_id>UINT32_MAX)return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};command.type=FACTORY_COMMAND_SET_TRAIN_DESTINATION;
    command.data.set_train_destination={(FactoryEntityId)train_id,
        (FactoryEntityId)station_id};
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_train_schedule_add_stop(int64_t train_id,
    int64_t station_id,int64_t wait_condition,int64_t wait_value)
{
    if(simulation_==nullptr||train_id<=0||train_id>UINT32_MAX
        ||station_id<=0||station_id>UINT32_MAX
        ||wait_condition<FACTORY_TRAIN_WAIT_NONE
        ||wait_condition>FACTORY_TRAIN_WAIT_CARGO_FULL||wait_value<0
        ||wait_value>UINT32_MAX)return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};
    command.type=FACTORY_COMMAND_TRAIN_SCHEDULE_ADD_STOP;
    command.data.train_schedule_add_stop={(FactoryEntityId)train_id,
        (FactoryEntityId)station_id,(uint32_t)wait_condition,
        (uint32_t)wait_value};
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_train_schedule_remove_stop(
    int64_t train_id,int64_t index)
{
    if(simulation_==nullptr||train_id<=0||train_id>UINT32_MAX
        ||index<0||index>=FACTORY_TRAIN_SCHEDULE_MAX_STOPS)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};
    command.type=FACTORY_COMMAND_TRAIN_SCHEDULE_REMOVE_STOP;
    command.data.train_schedule_remove_stop={(FactoryEntityId)train_id,
        (uint32_t)index};
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_train_schedule_clear(int64_t train_id)
{
    if(simulation_==nullptr||train_id<=0||train_id>UINT32_MAX)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};command.type=FACTORY_COMMAND_TRAIN_SCHEDULE_CLEAR;
    command.data.train_schedule_clear={(FactoryEntityId)train_id};
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_train_schedule_set_enabled(
    int64_t train_id,bool enabled)
{
    if(simulation_==nullptr||train_id<=0||train_id>UINT32_MAX)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};
    command.type=FACTORY_COMMAND_TRAIN_SCHEDULE_SET_ENABLED;
    command.data.train_schedule_set_enabled={(FactoryEntityId)train_id,enabled};
    return factory_simulation_submit_command(simulation_,&command);
}

Array FoundationSimulation::get_train_schedule(int64_t train_id) const
{
    Array result;
    if(simulation_==nullptr||train_id<=0||train_id>UINT32_MAX)return result;
    for(size_t index=0U;index<FACTORY_TRAIN_SCHEDULE_MAX_STOPS;++index){
        FactoryTrainScheduleStop stop;
        if(!factory_simulation_get_train_schedule_stop(simulation_,
                (FactoryTrainId)train_id,index,&stop))break;
        Dictionary value;value["index"]=(int64_t)index;
        value["station_id"]=(int64_t)stop.station_entity_id;
        value["wait_condition"]=(int64_t)stop.wait_condition;
        value["wait_value"]=(int64_t)stop.wait_value;result.append(value);
    }
    return result;
}

int64_t FoundationSimulation::queue_clear_train_destination(int64_t train_id)
{
    if(simulation_==nullptr||train_id<=0||train_id>UINT32_MAX)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};command.type=FACTORY_COMMAND_CLEAR_TRAIN_DESTINATION;
    command.data.clear_train_destination={(FactoryEntityId)train_id};
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_replan_train_route(int64_t train_id)
{
    if(simulation_==nullptr||train_id<=0||train_id>UINT32_MAX)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};command.type=FACTORY_COMMAND_REPLAN_TRAIN_ROUTE;
    command.data.replan_train_route={(FactoryEntityId)train_id};
    return factory_simulation_submit_command(simulation_,&command);
}

Array FoundationSimulation::get_train_route(int64_t train_id) const
{
    Array route;if(simulation_==nullptr||train_id<=0||train_id>UINT32_MAX)return route;
    FactoryLocomotiveInspection locomotive;
    if(!factory_simulation_get_locomotive(simulation_,(FactoryEntityId)train_id,
            &locomotive))return route;
    for(size_t i=0U;i<locomotive.route_length;++i){FactoryTrainRouteStep step;
        if(!factory_simulation_get_train_route_step(simulation_,
                (FactoryTrainId)train_id,i,&step))break;
        Dictionary value;value["rail_entity_id"]=(int64_t)step.rail_entity_id;
        value["entry_direction"]=(int64_t)step.entry_direction;route.append(value);
    }
    return route;
}

int64_t FoundationSimulation::queue_place_cargo_wagon(
    int64_t rail_entity_id,int64_t direction)
{
    if(simulation_==nullptr||rail_entity_id<=0||rail_entity_id>UINT32_MAX
        ||direction<FACTORY_DIRECTION_NORTH||direction>FACTORY_DIRECTION_WEST)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    FactoryCommand command={};command.type=FACTORY_COMMAND_PLACE_CARGO_WAGON;
    command.data.place_cargo_wagon={(FactoryEntityId)rail_entity_id,
        (FactoryDirection)direction};
    return factory_simulation_submit_command(simulation_,&command);
}

int64_t FoundationSimulation::queue_couple_rear_wagon(
    int64_t locomotive_id,int64_t wagon_id)
{if(simulation_==nullptr||locomotive_id<=0||locomotive_id>UINT32_MAX
    ||wagon_id<=0||wagon_id>UINT32_MAX)return FACTORY_RESULT_INVALID_ARGUMENT;
 FactoryCommand command={};command.type=FACTORY_COMMAND_COUPLE_REAR_WAGON;
 command.data.couple_rear_wagon={(FactoryEntityId)locomotive_id,
    (FactoryEntityId)wagon_id};return factory_simulation_submit_command(simulation_,&command);}

int64_t FoundationSimulation::queue_decouple_rear_wagon(int64_t locomotive_id)
{if(simulation_==nullptr||locomotive_id<=0||locomotive_id>UINT32_MAX)
    return FACTORY_RESULT_INVALID_ARGUMENT;FactoryCommand command={};
 command.type=FACTORY_COMMAND_DECOUPLE_REAR_WAGON;
 command.data.decouple_rear_wagon={(FactoryEntityId)locomotive_id};
 return factory_simulation_submit_command(simulation_,&command);}

Array FoundationSimulation::get_command_results() const
{
    Array values;
    if (simulation_==nullptr) return values;
    size_t count=factory_simulation_get_command_result_count(simulation_);
    for (size_t i=0;i<count;++i) {
        const FactoryCommandResult *r=factory_simulation_get_command_result(simulation_,i);
        if (r==nullptr) continue;
        Dictionary value;
        value["result"]=(int64_t)r->result;
        value["command_type"]=(int64_t)r->command.type;
        if (!set_unsigned(&value,"entity_id",r->entity_id,
                "command_result.entity_id")
            || !set_unsigned(&value,"construction_units_changed",
                r->construction_units_changed,
                "command_result.construction_units_changed")
            || !set_unsigned(&value,"construction_units_remaining",
                r->construction_units_remaining,
                "command_result.construction_units_remaining")
            || !set_unsigned(&value,"construction_depot_id",
                r->construction_depot_id,
                "command_result.construction_depot_id"))
            return Array();
        value["entity_type"]=(int64_t)r->entity_type;
        value["x"]=(int64_t)r->x; value["y"]=(int64_t)r->y;
        value["previous_assembler_recipe"]=
            (int64_t)r->previous_assembler_recipe;
        value["new_assembler_recipe"]=(int64_t)r->new_assembler_recipe;
        value["previous_storage_output"]=
            (int64_t)r->previous_storage_output;
        value["new_storage_output"]=(int64_t)r->new_storage_output;
        values.append(value);
    }
    return values;
}

Array FoundationSimulation::get_assembler_recipe_catalog() const
{
    Array values;
    Dictionary none;
    none["recipe_id"]=(int64_t)FACTORY_ASSEMBLER_RECIPE_NONE;
    none["name"]="None";
    none["required_unlock"]=(int64_t)0;
    none["unlocked"]=true;
    values.append(none);
    for (size_t i=0;i<factory_content_assembler_recipe_count();++i) {
        const FactoryAssemblerRecipe *recipe=
            factory_content_assembler_recipe_at(i);
        if (recipe==nullptr) continue;
        Dictionary value;
        value["recipe_id"]=(int64_t)recipe->recipe_id;
        value["name"]=String(factory_item_name(recipe->output_item));
        value["output_item"]=(int64_t)recipe->output_item;
        value["output_amount"]=(int64_t)recipe->output_amount;
        value["required_unlock"]=(int64_t)recipe->required_unlock;
        value["unlocked"]=simulation_!=nullptr
            && factory_simulation_is_assembler_recipe_unlocked(
                simulation_,recipe->recipe_id);
        values.append(value);
    }
    return values;
}

Array FoundationSimulation::get_item_catalog() const
{
    Array values;
    for(int item=FACTORY_ITEM_NONE;
        item<=FACTORY_ITEM_CONSTRUCTION_MATERIAL;++item){
        Dictionary value;
        value["item_type"]=(int64_t)item;
        value["name"]=item==FACTORY_ITEM_NONE
            ? String("None") : String(factory_item_name((FactoryItemType)item));
        values.append(value);
    }
    return values;
}

Array FoundationSimulation::get_build_catalog() const
{
    Array values;
    for (size_t i=0;i<factory_content_entity_definition_count();++i) {
        const FactoryEntityDefinition *d=factory_content_entity_definition_at(i);
        if (d==nullptr) continue;
        Dictionary value;
        value["entity_type"]=(int64_t)d->entity_type;
        if (!set_unsigned(&value,"construction_cost",d->construction_cost,
                "content.construction_cost")
            || !set_unsigned(&value,"required_unlock",d->required_unlock,
                "content.required_unlock")
            || !set_unsigned(&value,"footprint_width",d->footprint_width,
                "content.footprint_width")
            || !set_unsigned(&value,"footprint_height",d->footprint_height,
                "content.footprint_height"))
            return Array();
        value["unlocked"]=simulation_!=nullptr
            && factory_simulation_is_entity_unlocked(simulation_,d->entity_type);
        values.append(value);
    }
    return values;
}

int64_t FoundationSimulation::get_construction_units() const
{
    return simulation_==nullptr?0:(int64_t)factory_simulation_construction_units(simulation_);
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

int64_t FoundationSimulation::get_day() const
{
    if (simulation_ == nullptr) return 0;
    const uint64_t day = factory_simulation_clock_get_day(simulation_);
    int64_t converted = 0;
    if (!foundation_godot::checked_uint64_to_godot_int(day, &converted)) {
        set_conversion_error("simulation.day", day);
        return -1;
    }
    return converted;
}

int64_t FoundationSimulation::get_time_of_day() const
{
    return simulation_ == nullptr
        ? 0 : (int64_t)factory_simulation_clock_get_time_of_day(simulation_);
}

Dictionary FoundationSimulation::get_research() const
{
    Dictionary value;
    if (presentation_ == nullptr) return value;
    value["active_technology_id"]=(int64_t)
        factory_presentation_snapshot_get_active_research(presentation_);
    value["completed_technology_count"]=(int64_t)
        factory_presentation_snapshot_get_completed_technology_count(presentation_);
    FactoryTechnologyProgressInspection progress={};
    if (factory_presentation_snapshot_get_active_research_progress(
            presentation_,&progress)==FACTORY_RESULT_OK) {
        value["completed_units"]=(int64_t)progress.completed_units;
        value["required_units"]=(int64_t)progress.required_units;
        value["work_ticks"]=(int64_t)progress.work_ticks_in_current_unit;
        value["work_ticks_per_unit"]=(int64_t)progress.work_ticks_per_unit;
        value["science_committed"]=progress.science_committed_for_current_unit;
    }
    return value;
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
    case FACTORY_ENTITY_TYPE_WATER_EXTRACTOR:
        value["stored_water"] =
            (int64_t)entity.data.water_extractor.stored_water;
        value["output_capacity"] =
            (int64_t)entity.data.water_extractor.output_capacity;
        value["progress"] =
            (int64_t)entity.data.water_extractor.progress;
        value["duration"] =
            (int64_t)entity.data.water_extractor.duration;
        value["output_network_id"] =
            (int64_t)entity.data.water_extractor.output_network_id;
        break;
    case FACTORY_ENTITY_TYPE_BOILER:
        value["stored_water"] =
            (int64_t)entity.data.boiler.stored_water;
        value["water_capacity"] =
            (int64_t)entity.data.boiler.water_capacity;
        value["stored_steam"] =
            (int64_t)entity.data.boiler.stored_steam;
        value["steam_capacity"] =
            (int64_t)entity.data.boiler.steam_capacity;
        value["input_network_id"] =
            (int64_t)entity.data.boiler.input_network_id;
        value["output_network_id"] =
            (int64_t)entity.data.boiler.output_network_id;
        value["fuel_active"] = entity.data.boiler.burner.active;
        value["fuel_ticks"] =
            (int64_t)entity.data.boiler.burner.remaining_burn_ticks;
        if (!set_unsigned(
                &value, "energy_available",
                entity.data.boiler.burner.released_energy,
                "entity.boiler.burner.released_energy"))
            return false;
        value["conversion_active"] =
            entity.data.boiler.conversion_active;
        break;
    case FACTORY_ENTITY_TYPE_STEAM_ENGINE:
        value["stored_steam"] =
            (int64_t)entity.data.steam_engine.stored_steam;
        value["steam_capacity"] =
            (int64_t)entity.data.steam_engine.steam_capacity;
        value["steam_network_id"] =
            (int64_t)entity.data.steam_engine.steam_network_id;
        value["power_network_id"] =
            (int64_t)entity.data.steam_engine.power_network_id;
        value["maximum_output"] =
            (int64_t)entity.data.steam_engine.maximum_output_per_tick;
        value["available_generation"] =
            (int64_t)entity.data.steam_engine.available_generation;
        value["generated_last_tick"] =
            (int64_t)entity.data.steam_engine.generated_last_tick;
        value["generation_active"] = entity.data.steam_engine.active;
        break;
    case FACTORY_ENTITY_TYPE_SOLAR_GENERATOR:
        value["power_network_id"] =
            (int64_t)entity.data.solar_generator.power_network_id;
        value["maximum_output"] =
            (int64_t)entity.data.solar_generator.maximum_output;
        value["available_generation"] =
            (int64_t)entity.data.solar_generator.available_output;
        value["generated_last_tick"] =
            (int64_t)entity.data.solar_generator.actual_output;
        value["generation_active"] = entity.data.solar_generator.active;
        break;
    case FACTORY_ENTITY_TYPE_ACCUMULATOR:
        if (!set_unsigned(
                &value, "stored_energy",
                entity.data.accumulator.stored_energy,
                "entity.accumulator.stored_energy"
            ) || !set_unsigned(
                &value, "capacity",
                entity.data.accumulator.capacity,
                "entity.accumulator.capacity"
            ))
            return false;
        value["maximum_charge_rate"] =
            (int64_t)entity.data.accumulator.maximum_charge_rate;
        value["maximum_discharge_rate"] =
            (int64_t)entity.data.accumulator.maximum_discharge_rate;
        value["charged_last_tick"] =
            (int64_t)entity.data.accumulator.charged_last_tick;
        value["discharged_last_tick"] =
            (int64_t)entity.data.accumulator.discharged_last_tick;
        value["accumulator_activity"] =
            (int64_t)entity.data.accumulator.activity;
        value["power_network_id"] =
            (int64_t)entity.data.accumulator.power_network_id;
        value["connected"] = entity.data.accumulator.connected;
        break;
    case FACTORY_ENTITY_TYPE_REACTOR_CORE:
        if (!set_unsigned(
                &value, "stored_heat",
                entity.data.reactor.stored_heat,
                "entity.reactor.stored_heat"
            ) || !set_unsigned(
                &value, "heat_capacity",
                entity.data.reactor.heat_capacity,
                "entity.reactor.heat_capacity"
            ) || !set_unsigned(
                &value, "remaining_heat_yield",
                entity.data.reactor.remaining_heat_yield,
                "entity.reactor.remaining_heat_yield"
            ) || !set_unsigned(
                &value, "generated_last_tick",
                entity.data.reactor.generated_last_tick,
                "entity.reactor.generated_last_tick"
            ))
            return false;
        value["inventory_fuel_id"] =
            (int64_t)entity.data.reactor.inventory_fuel_id;
        value["inventory_quantity"] =
            (int64_t)entity.data.reactor.inventory_quantity;
        value["active_fuel_id"] =
            (int64_t)entity.data.reactor.active_fuel_id;
        value["remaining_burn_ticks"] =
            (int64_t)entity.data.reactor.remaining_burn_ticks;
        value["reactor_activity"] =
            (int64_t)entity.data.reactor.activity;
        value["heat_network_id"] =
            (int64_t)entity.data.reactor.heat_network_id;
        value["heat_connected"] = entity.data.reactor.heat_connected;
        break;
    case FACTORY_ENTITY_TYPE_HEAT_CONDUCTOR:
        value["connection_mask"] =
            (int64_t)entity.data.heat_conductor.connection_mask;
        value["heat_network_id"] =
            (int64_t)entity.data.heat_conductor.heat_network_id;
        value["connected"] = entity.data.heat_conductor.connected;
        break;
    case FACTORY_ENTITY_TYPE_HEAT_EXCHANGER:
        value["heat_network_id"] =
            (int64_t)entity.data.heat_exchanger.heat_network_id;
        value["water_network_id"] =
            (int64_t)entity.data.heat_exchanger.water_network_id;
        value["steam_network_id"] =
            (int64_t)entity.data.heat_exchanger.steam_network_id;
        value["stored_water"] =
            (int64_t)entity.data.heat_exchanger.stored_water;
        value["water_capacity"] =
            (int64_t)entity.data.heat_exchanger.water_capacity;
        value["stored_steam"] =
            (int64_t)entity.data.heat_exchanger.stored_steam;
        value["steam_capacity"] =
            (int64_t)entity.data.heat_exchanger.steam_capacity;
        if (!set_unsigned(&value,"consumed_heat_last_tick",
                entity.data.heat_exchanger.consumed_heat_last_tick,
                "entity.heat_exchanger.consumed_heat_last_tick"))
            return false;
        value["consumed_water_last_tick"] =
            (int64_t)entity.data.heat_exchanger.consumed_water_last_tick;
        value["produced_steam_last_tick"] =
            (int64_t)entity.data.heat_exchanger.produced_steam_last_tick;
        value["heat_exchanger_activity"] =
            (int64_t)entity.data.heat_exchanger.activity;
        break;
    case FACTORY_ENTITY_TYPE_STEAM_TURBINE:
        value["steam_fluid"] =
            (int64_t)entity.data.steam_turbine.steam_fluid;
        value["stored_steam"] =
            (int64_t)entity.data.steam_turbine.stored_steam;
        value["steam_capacity"] =
            (int64_t)entity.data.steam_turbine.steam_capacity;
        value["exhaust_fluid"] =
            (int64_t)entity.data.steam_turbine.exhaust_fluid;
        value["stored_exhaust"] =
            (int64_t)entity.data.steam_turbine.stored_exhaust;
        value["exhaust_capacity"] =
            (int64_t)entity.data.steam_turbine.exhaust_capacity;
        value["fluid_network_id"] =
            (int64_t)entity.data.steam_turbine.fluid_network_id;
        value["exhaust_network_id"] =
            (int64_t)entity.data.steam_turbine.exhaust_network_id;
        value["power_network_id"] =
            (int64_t)entity.data.steam_turbine.power_network_id;
        value["fluid_connected"] =
            entity.data.steam_turbine.fluid_connected;
        value["exhaust_connected"] =
            entity.data.steam_turbine.exhaust_connected;
        value["power_connected"] =
            entity.data.steam_turbine.power_connected;
        value["turbine_definition_id"] =
            (int64_t)entity.data.steam_turbine.definition_id;
        value["maximum_output"] =
            (int64_t)entity.data.steam_turbine.maximum_output;
        value["available_generation"] =
            (int64_t)entity.data.steam_turbine.available_output;
        value["generated_last_tick"] =
            (int64_t)entity.data.steam_turbine.actual_output;
        value["steam_consumed_last_tick"] =
            (int64_t)entity.data.steam_turbine.steam_consumed_last_tick;
        value["exhaust_produced_last_tick"] =
            (int64_t)entity.data.steam_turbine.exhaust_produced_last_tick;
        value["completed_cycles_last_tick"] =
            (int64_t)entity.data.steam_turbine.completed_cycles_last_tick;
        value["turbine_activity"] =
            (int64_t)entity.data.steam_turbine.activity;
        break;
    case FACTORY_ENTITY_TYPE_STEAM_CONDENSER:
        value["condenser_definition_id"] =
            (int64_t)entity.data.steam_condenser.definition_id;
        value["steam_fluid"] =
            (int64_t)entity.data.steam_condenser.steam_fluid;
        value["stored_steam"] =
            (int64_t)entity.data.steam_condenser.stored_steam;
        value["steam_capacity"] =
            (int64_t)entity.data.steam_condenser.steam_capacity;
        value["water_fluid"] =
            (int64_t)entity.data.steam_condenser.water_fluid;
        value["stored_water"] =
            (int64_t)entity.data.steam_condenser.stored_water;
        value["water_capacity"] =
            (int64_t)entity.data.steam_condenser.water_capacity;
        value["steam_network_id"] =
            (int64_t)entity.data.steam_condenser.steam_network_id;
        value["water_network_id"] =
            (int64_t)entity.data.steam_condenser.water_network_id;
        value["power_network_id"] =
            (int64_t)entity.data.steam_condenser.power_network_id;
        value["fluid_connected"] =
            entity.data.steam_condenser.fluid_connected;
        value["power_per_cycle"] =
            (int64_t)entity.data.steam_condenser.power_per_cycle;
        value["steam_consumed_last_tick"] =
            (int64_t)entity.data.steam_condenser.steam_consumed_last_tick;
        value["water_produced_last_tick"] =
            (int64_t)entity.data.steam_condenser.water_produced_last_tick;
        value["completed_cycles_last_tick"] =
            (int64_t)entity.data.steam_condenser.completed_cycles_last_tick;
        value["condenser_activity"] =
            (int64_t)entity.data.steam_condenser.activity;
        break;
    case FACTORY_ENTITY_TYPE_RESEARCH_LAB:
        value["science_quantity"] =
            (int64_t)entity.data.research_lab.science_quantity;
        value["science_capacity"] =
            (int64_t)entity.data.research_lab.science_capacity;
        value["power_network_id"] =
            (int64_t)entity.data.research_lab.power_network_id;
        value["power_connected"] = entity.data.research_lab.connected;
        value["lab_activity"] =
            (int64_t)entity.data.research_lab.activity;
        value["science_consumed_last_tick"] =
            (int64_t)entity.data.research_lab.science_consumed_last_tick;
        value["work_contributed_last_tick"] =
            (int64_t)entity.data.research_lab.work_contributed_last_tick;
        break;
    case FACTORY_ENTITY_TYPE_CONSTRUCTION_DEPOT:
        value["material_quantity"]=(int64_t)
            entity.data.construction_depot.material_quantity;
        value["capacity"]=(int64_t)entity.data.construction_depot.capacity;
        value["supply_radius"]=(int64_t)
            entity.data.construction_depot.supply_radius;
        break;
    case FACTORY_ENTITY_TYPE_RAIL: {
        value["rail_geometry"]=(int64_t)entity.data.rail.geometry;
        value["port_mask"]=(int64_t)entity.data.rail.port_mask;
        value["connection_mask"]=(int64_t)entity.data.rail.connection_mask;
        value["rail_network_id"]=(int64_t)entity.data.rail.network_id;
        value["block_id"]=(int64_t)entity.data.rail.block_id;
        value["reserved_train_id"]=(int64_t)entity.data.rail.reserved_train_id;
        value["occupied_train_count"]=(int64_t)entity.data.rail.occupied_train_count;
        Array neighbors;for(size_t i=0U;i<FACTORY_RAIL_NEIGHBOR_COUNT;++i)
            neighbors.append((int64_t)entity.data.rail.neighbors[i]);
        value["rail_neighbors"]=neighbors;break;
    }
    case FACTORY_ENTITY_TYPE_RAIL_STATION:
        value["attached_rail_id"]=(int64_t)
            entity.data.rail_station.attached_rail_id;
        value["rail_network_id"]=(int64_t)entity.data.rail_station.network_id;
        value["rail_connected"]=entity.data.rail_station.connected;
        value["freight_mode"]=(int64_t)entity.data.rail_station.freight_mode;
        value["freight_item"]=(int64_t)entity.data.rail_station.configured_item;
        value["freight_quantity"]=(int64_t)entity.data.rail_station.freight_quantity;
        value["freight_capacity"]=(int64_t)entity.data.rail_station.freight_capacity;
        value["eligible_train_id"]=(int64_t)entity.data.rail_station.eligible_train_id;
        value["freight_transfer_possible"]=
            entity.data.rail_station.freight_transfer_possible;
        value["freight_transferred_last_tick"]=(int64_t)
            entity.data.rail_station.latest_transfer_quantity;
        value["freight_activity"]=(int64_t)
            entity.data.rail_station.latest_transfer_activity;break;
    case FACTORY_ENTITY_TYPE_RAIL_SWITCH: {
        value["switch_geometry"]=(int64_t)entity.data.rail_switch.geometry;
        value["selected_branch"]=(int64_t)entity.data.rail_switch.selected_branch;
        value["stem_direction"]=(int64_t)entity.data.rail_switch.stem_direction;
        value["branch_a_direction"]=(int64_t)entity.data.rail_switch.branch_a_direction;
        value["branch_b_direction"]=(int64_t)entity.data.rail_switch.branch_b_direction;
        value["port_mask"]=(int64_t)entity.data.rail_switch.port_mask;
        value["connection_mask"]=(int64_t)entity.data.rail_switch.connection_mask;
        value["rail_network_id"]=(int64_t)entity.data.rail_switch.network_id;
        value["block_id"]=(int64_t)entity.data.rail_switch.block_id;
        value["reserved_train_id"]=(int64_t)entity.data.rail_switch.reserved_train_id;
        value["occupied_train_count"]=(int64_t)entity.data.rail_switch.occupied_train_count;
        Array neighbors;for(size_t i=0U;i<FACTORY_RAIL_NEIGHBOR_COUNT;++i)
            neighbors.append((int64_t)entity.data.rail_switch.neighbors[i]);
        value["rail_neighbors"]=neighbors;break;
    }
    case FACTORY_ENTITY_TYPE_LOCOMOTIVE:
        value["rail_entity_id"]=(int64_t)entity.data.locomotive.rail_entity_id;
        value["entry_direction"]=(int64_t)entity.data.locomotive.entry_direction;
        value["travel_direction"]=(int64_t)entity.data.locomotive.travel_direction;
        value["movement_progress"]=(int64_t)entity.data.locomotive.movement_progress;
        value["movement_interval"]=(int64_t)entity.data.locomotive.movement_interval;
        value["next_rail_id"]=(int64_t)entity.data.locomotive.next_rail_id;
        value["rail_network_id"]=(int64_t)entity.data.locomotive.network_id;
        value["locomotive_activity"]=(int64_t)entity.data.locomotive.activity;
        value["train_id"]=(int64_t)entity.data.locomotive.train_id;
        value["vehicle_count"]=(int64_t)entity.data.locomotive.vehicle_count;
        value["destination_station_id"]=(int64_t)entity.data.locomotive.destination_station_id;
        value["route_status"]=(int64_t)entity.data.locomotive.route_status;
        value["route_length"]=(int64_t)entity.data.locomotive.route_length;
        value["route_index"]=(int64_t)entity.data.locomotive.route_index;
        value["next_planned_rail_id"]=(int64_t)entity.data.locomotive.next_planned_rail_id;
        value["current_block_id"]=(int64_t)entity.data.locomotive.current_block_id;
        value["next_route_block_id"]=(int64_t)entity.data.locomotive.next_route_block_id;
        value["reserved_block_id"]=(int64_t)entity.data.locomotive.reserved_block_id;
        value["reservation_status"]=(int64_t)entity.data.locomotive.reservation_status;
        value["blocking_train_id"]=(int64_t)entity.data.locomotive.blocking_train_id;
        value["reserved_block_count"]=(int64_t)entity.data.locomotive.reserved_block_count;
        value["chain_required_block_count"]=(int64_t)entity.data.locomotive.chain_required_block_count;
        value["blocking_block_id"]=(int64_t)entity.data.locomotive.blocking_block_id;
        value["chain_status"]=(int64_t)entity.data.locomotive.chain_status;
        value["schedule_enabled"]=entity.data.locomotive.schedule_enabled;
        value["schedule_count"]=(int64_t)entity.data.locomotive.schedule_count;
        value["current_stop_index"]=(int64_t)
            entity.data.locomotive.current_stop_index;
        value["current_scheduled_station_id"]=(int64_t)
            entity.data.locomotive.current_scheduled_station_id;
        value["wait_condition"]=(int64_t)entity.data.locomotive.wait_condition;
        value["wait_value"]=(int64_t)entity.data.locomotive.wait_value;
        value["wait_progress"]=(int64_t)entity.data.locomotive.wait_progress;
        value["schedule_status"]=(int64_t)entity.data.locomotive.schedule_status;
        {
            Array reserved_blocks;
            const size_t count=factory_simulation_get_train_reserved_block_count(
                simulation_,entity.entity_id);
            for(size_t index=0U;index<count;++index){
                FactoryRailBlockId block=0U;
                if(factory_simulation_get_train_reserved_block_at(simulation_,
                        entity.entity_id,index,&block))
                    reserved_blocks.append((int64_t)block);
            }
            value["reserved_blocks"]=reserved_blocks;
        }
        break;
    case FACTORY_ENTITY_TYPE_CARGO_WAGON:
        value["rail_entity_id"]=(int64_t)entity.data.cargo_wagon.rail_entity_id;
        value["entry_direction"]=(int64_t)entity.data.cargo_wagon.entry_direction;
        value["rail_network_id"]=(int64_t)entity.data.cargo_wagon.network_id;
        value["train_id"]=(int64_t)entity.data.cargo_wagon.train_id;
        value["consist_index"]=(int64_t)entity.data.cargo_wagon.consist_index;
        value["coupled"]=entity.data.cargo_wagon.coupled;
        value["cargo_item"]=(int64_t)entity.data.cargo_wagon.cargo_item;
        value["cargo_quantity"]=(int64_t)entity.data.cargo_wagon.cargo_quantity;
        value["cargo_capacity"]=(int64_t)entity.data.cargo_wagon.cargo_capacity;
        break;
    case FACTORY_ENTITY_TYPE_RAIL_SIGNAL:
        value["signal_orientation"]=(int64_t)entity.data.rail_signal.orientation;
        value["attached_rail_id"]=(int64_t)entity.data.rail_signal.attached_rail_id;
        value["upstream_rail_id"]=(int64_t)entity.data.rail_signal.upstream_rail_id;
        value["upstream_block_id"]=(int64_t)entity.data.rail_signal.upstream_block_id;
        value["downstream_block_id"]=(int64_t)entity.data.rail_signal.downstream_block_id;
        value["signal_aspect"]=(int64_t)entity.data.rail_signal.aspect;
        value["reserved_train_id"]=(int64_t)entity.data.rail_signal.reserved_train_id;
        value["occupied_train_count"]=(int64_t)entity.data.rail_signal.occupied_train_count;
        value["rail_connected"]=entity.data.rail_signal.connected;
        break;
    case FACTORY_ENTITY_TYPE_RAIL_CHAIN_SIGNAL:
        value["signal_orientation"]=(int64_t)entity.data.rail_chain_signal.orientation;
        value["attached_rail_id"]=(int64_t)entity.data.rail_chain_signal.attached_rail_id;
        value["upstream_rail_id"]=(int64_t)entity.data.rail_chain_signal.upstream_rail_id;
        value["upstream_block_id"]=(int64_t)entity.data.rail_chain_signal.upstream_block_id;
        value["downstream_block_id"]=(int64_t)entity.data.rail_chain_signal.downstream_block_id;
        value["signal_aspect"]=(int64_t)entity.data.rail_chain_signal.aspect;
        value["reserved_train_id"]=(int64_t)entity.data.rail_chain_signal.reserved_train_id;
        value["occupied_train_count"]=(int64_t)entity.data.rail_chain_signal.occupied_train_count;
        value["rail_connected"]=entity.data.rail_chain_signal.connected;
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
        value["depleted"] = resource->depleted;
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

Array FoundationSimulation::get_terrain() const
{
    Array values;
    if(presentation_==nullptr)return values;
    const size_t count=factory_presentation_snapshot_get_terrain_count(presentation_);
    for(size_t index=0;index<count;++index){
        const FactoryPresentationTerrain *terrain=
            factory_presentation_snapshot_get_terrain(presentation_,index);
        if(terrain==nullptr)continue;
        Dictionary value;
        value["x"]=(int64_t)terrain->x;
        value["y"]=(int64_t)terrain->y;
        value["type"]=(int64_t)terrain->terrain_type;
        value["buildable"]=terrain->buildable;
        values.append(value);
    }
    return values;
}

Dictionary FoundationSimulation::get_entity_telemetry(int64_t entity_id) const
{
    Dictionary value;FactoryTelemetryEntityMetrics metrics;
    if(telemetry_==nullptr||entity_id<=0||entity_id>UINT32_MAX
        ||!factory_telemetry_get_entity_metrics(telemetry_,
            (FactoryEntityId)entity_id,FACTORY_TELEMETRY_WINDOW_SHORT,&metrics))
        return value;
    value["window_ticks"]=(int64_t)metrics.observed_ticks;
    if(!set_unsigned(&value,"received",metrics.received_quantity,
            "telemetry.received")
        ||!set_unsigned(&value,"sent",metrics.sent_quantity,"telemetry.sent")
        ||!set_unsigned(&value,"cycles",metrics.completed_cycles,
            "telemetry.cycles")
        ||!set_unsigned(&value,"pickups",metrics.pickups,"telemetry.pickups")
        ||!set_unsigned(&value,"drops",metrics.drops,"telemetry.drops"))
        return Dictionary();
    value["working"]=(int64_t)metrics.working_ticks;
    value["net_flow"]=metrics.net_flow;
    value["blocked_input"]=(int64_t)metrics.blocked_input_ticks;
    value["blocked_output"]=(int64_t)metrics.blocked_output_ticks;
    value["unpowered"]=(int64_t)metrics.unpowered_ticks;
    value["idle"]=(int64_t)metrics.idle_ticks;
    value["depleted"]=(int64_t)metrics.depleted_ticks;
    value["occupied"]=(int64_t)metrics.occupied_ticks;
    value["blocked"]=(int64_t)metrics.blocked_ticks;
    value["holding"]=(int64_t)metrics.holding_ticks;
    value["saturated"]=metrics.saturated;
    return value;
}

int64_t FoundationSimulation::get_start_x() const
{return world_==nullptr?0:(int64_t)factory_world_get_start_x(world_);}
int64_t FoundationSimulation::get_start_y() const
{return world_==nullptr?0:(int64_t)factory_world_get_start_y(world_);}

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
        value["related_fluid_type"] =
            (int64_t)event->related_fluid_type;
        value["nuclear_fuel_id"] =
            (int64_t)event->nuclear_fuel_id;
        value["technology_id"]=(int64_t)event->technology_id;
        value["quantity"] = (int64_t)event->quantity;
        value["related_quantity"] =
            (int64_t)event->related_quantity;
        value["third_quantity"] = (int64_t)event->third_quantity;
        value["resource_type"] = (int64_t)event->resource_type;
        value["x"] = (int64_t)event->x;
        value["y"] = (int64_t)event->y;
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
