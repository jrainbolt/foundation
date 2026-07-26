#include "foundation/simulation.h"

#include <stdlib.h>

#include "assembler_internal.h"
#include "belt_internal.h"
#include "extractor_internal.h"
#include "inserter_internal.h"
#include "logistics_endpoint_internal.h"
#include "refinery_internal.h"
#include "splitter_internal.h"
#include "storage_internal.h"
#include "simulation_internal.h"
#include "world_internal.h"

typedef struct {
    FactoryLogisticsEndpoint source;
    FactoryLogisticsEndpoint destination;
    FactoryItemType item;
    FactorySplitterOutput splitter_output;
    bool wins;
} TransferIntent;

static void adjacent_coordinate(
    int32_t x,
    int32_t y,
    FactoryDirection direction,
    int32_t *out_x,
    int32_t *out_y
)
{
    *out_x = x;
    *out_y = y;
    switch (direction) {
        case FACTORY_DIRECTION_NORTH:
            --*out_y;
            break;
        case FACTORY_DIRECTION_EAST:
            ++*out_x;
            break;
        case FACTORY_DIRECTION_SOUTH:
            ++*out_y;
            break;
        case FACTORY_DIRECTION_WEST:
            --*out_x;
            break;
    }
}

static FactoryDirection opposite_direction(FactoryDirection direction)
{
    return (FactoryDirection)(((int)direction + 2) % 4);
}

static FactoryDirection splitter_output_direction(
    FactoryDirection facing,
    FactorySplitterOutput output
)
{
    int offset = output == FACTORY_SPLITTER_OUTPUT_LEFT ? 3 : 1;

    return (FactoryDirection)(((int)facing + offset) % 4);
}

FactorySimulation *factory_simulation_create(FactoryWorld *world)
{
    return factory_simulation_create_with_construction_units(world, 0U);
}

FactorySimulation *factory_simulation_create_with_construction_units(
    FactoryWorld *world,
    FactoryConstructionMaterial construction_units
)
{
    FactorySimulation *simulation;

    if (world == NULL) {
        return NULL;
    }
    simulation = calloc(1U, sizeof(*simulation));
    if (simulation == NULL) {
        return NULL;
    }
    simulation->entities = factory_entity_manager_create();
    if (simulation->entities == NULL) {
        free(simulation);
        return NULL;
    }
    simulation->world = world;
    simulation->owns_world = false;
    simulation->construction_inventory.units = construction_units;
    return simulation;
}

void factory_simulation_destroy(FactorySimulation *simulation)
{
    if (simulation == NULL) {
        return;
    }
    factory_event_batch_destroy(&simulation->events);
    factory_fluid_storage_store_destroy(&simulation->fluid_storages);
    factory_pipe_store_destroy(&simulation->pipes);
    factory_fluid_port_store_destroy(&simulation->fluid_ports);
    factory_fluid_network_state_destroy(&simulation->fluid_networks);
    factory_water_extractor_store_destroy(&simulation->water_extractors);
    factory_boiler_store_destroy(&simulation->boilers);
    factory_steam_engine_store_destroy(&simulation->steam_engines);
    factory_solar_generator_store_destroy(&simulation->solar_generators);
    factory_accumulator_store_destroy(&simulation->accumulators);
    factory_reactor_store_destroy(&simulation->reactors);
    factory_heat_conductor_store_destroy(&simulation->heat_conductors);
    factory_heat_port_store_destroy(&simulation->heat_ports);
    factory_heat_exchanger_store_destroy(&simulation->heat_exchangers);
    factory_heat_network_state_destroy(&simulation->heat_networks);
    factory_burner_store_destroy(&simulation->burners);
    factory_power_state_destroy(&simulation->power);
    factory_power_generator_store_destroy(&simulation->power_generators);
    factory_power_pole_store_destroy(&simulation->power_poles);
    factory_storage_store_destroy(&simulation->storages);
    factory_belt_store_destroy(&simulation->belts);
    factory_inserter_store_destroy(&simulation->inserters);
    factory_splitter_store_destroy(&simulation->splitters);
    factory_assembler_store_destroy(&simulation->assemblers);
    factory_refinery_store_destroy(&simulation->refineries);
    factory_extractor_store_destroy(&simulation->extractors);
    factory_entity_manager_destroy(simulation->entities);
    if (simulation->owns_world) {
        factory_world_destroy(simulation->world);
    }
    free(simulation);
}

static FactoryResult validate_empty_tile(
    FactorySimulation *simulation, int32_t x, int32_t y
);
static FactoryResult occupy_with_entity(
    FactorySimulation *simulation,
    int32_t x,
    int32_t y,
    FactoryEntityId *out_id
);

static FactoryResult place_power_entity(
    FactorySimulation *simulation,
    int32_t x,
    int32_t y,
    bool generator,
    FactoryEntityId *out_id
)
{
    FactoryResult result = validate_empty_tile(simulation, x, y);
    if (result != FACTORY_RESULT_OK) return result;
    if (generator) {
        if (!factory_power_generator_store_reserve_one(
                &simulation->power_generators)
            || !factory_burner_store_reserve_one(&simulation->burners)) {
            return FACTORY_RESULT_OUT_OF_MEMORY;
        }
    } else if (!factory_power_pole_store_reserve_one(
            &simulation->power_poles)) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    result = occupy_with_entity(simulation, x, y, out_id);
    if (result != FACTORY_RESULT_OK) return result;
    if (generator) {
        factory_power_generator_store_add(
            &simulation->power_generators, *out_id, x, y
        );
        factory_burner_store_add(
            &simulation->burners, *out_id, FACTORY_FUEL_CLASS_SOLID
        );
        if (simulation->fixture_initial_generator_fuel != 0U) {
            FactoryBurner *burner = factory_burner_store_find_mutable(
                &simulation->burners, *out_id);
            burner->inventory_item = FACTORY_ITEM_BIOMASS_PELLET;
            burner->inventory_quantity =
                simulation->fixture_initial_generator_fuel;
        }
    } else {
        factory_power_pole_store_add(
            &simulation->power_poles, *out_id, x, y
        );
    }
    return FACTORY_RESULT_OK;
}

FactoryResult factory_simulation_submit_command(
    FactorySimulation *simulation,
    const FactoryCommand *command
)
{
    if (simulation == NULL || !factory_command_is_well_formed(command)) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    if (simulation->command_count == FACTORY_COMMAND_QUEUE_CAPACITY) {
        return FACTORY_RESULT_QUEUE_FULL;
    }
    simulation->commands[simulation->command_count++] = *command;
    return FACTORY_RESULT_OK;
}

static FactoryResult validate_empty_tile(
    FactorySimulation *simulation,
    int32_t x,
    int32_t y
)
{
    const FactoryTile *tile = factory_world_get_tile(simulation->world, x, y);

    if (tile == NULL) {
        return FACTORY_RESULT_OUT_OF_BOUNDS;
    }
    if (tile->occupying_entity != 0U) {
        return FACTORY_RESULT_TILE_OCCUPIED;
    }
    return FACTORY_RESULT_OK;
}

static FactoryResult occupy_with_entity(
    FactorySimulation *simulation,
    int32_t x,
    int32_t y,
    FactoryEntityId *out_id
)
{
    FactoryEntityId id = factory_entity_create(simulation->entities);
    FactoryResult result;

    if (id == 0U) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    result = factory_world_set_occupying_entity(simulation->world, x, y, id);
    if (result != FACTORY_RESULT_OK) {
        factory_entity_destroy(simulation->entities, id);
        return result;
    }
    *out_id = id;
    return FACTORY_RESULT_OK;
}

static FactoryResult place_extractor(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_extractor.x;
    int32_t y = command->data.place_extractor.y;
    const FactoryTile *tile = factory_world_get_tile(simulation->world, x, y);
    FactoryItemType produced_item;
    FactoryResult result;

    if (tile == NULL) {
        return FACTORY_RESULT_OUT_OF_BOUNDS;
    }
    if (tile->resource == FACTORY_RESOURCE_NONE) {
        return FACTORY_RESULT_NO_RESOURCE;
    }
    if (tile->resource == FACTORY_RESOURCE_IRON) {
        produced_item = FACTORY_ITEM_IRON_ORE;
    } else if (tile->resource == FACTORY_RESOURCE_COPPER) {
        produced_item = FACTORY_ITEM_COPPER_ORE;
    } else {
        return FACTORY_RESULT_UNSUPPORTED_RESOURCE;
    }
    if (tile->occupying_entity != 0U) {
        return FACTORY_RESULT_TILE_OCCUPIED;
    }
    if (!factory_extractor_store_reserve_one(&simulation->extractors)) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    result = occupy_with_entity(simulation, x, y, out_id);
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    factory_extractor_store_add(
        &simulation->extractors,
        *out_id,
        x,
        y,
        tile->resource,
        produced_item,
        command->data.place_extractor.output_direction
    );
    return FACTORY_RESULT_OK;
}

static FactoryResult place_belt(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_belt.x;
    int32_t y = command->data.place_belt.y;
    FactoryResult result = validate_empty_tile(simulation, x, y);

    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    if (!factory_belt_store_reserve_one(&simulation->belts)) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    result = occupy_with_entity(simulation, x, y, out_id);
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    factory_belt_store_add(
        &simulation->belts,
        *out_id,
        x,
        y,
        command->data.place_belt.direction
    );
    return FACTORY_RESULT_OK;
}

static FactoryResult place_storage(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_storage.x;
    int32_t y = command->data.place_storage.y;
    FactoryResult result = validate_empty_tile(simulation, x, y);

    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    if (!factory_storage_store_reserve_one(&simulation->storages)) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    result = occupy_with_entity(simulation, x, y, out_id);
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    factory_storage_store_add(&simulation->storages, *out_id, x, y);
    return FACTORY_RESULT_OK;
}

static FactoryResult place_fluid_tank(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_fluid_tank.x;
    int32_t y = command->data.place_fluid_tank.y;
    FactoryResult result = validate_empty_tile(simulation, x, y);
    if (result != FACTORY_RESULT_OK) return result;
    if (!factory_fluid_storage_store_reserve_one(
            &simulation->fluid_storages)
        || !factory_fluid_port_store_reserve_one(&simulation->fluid_ports))
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result = occupy_with_entity(simulation, x, y, out_id);
    if (result != FACTORY_RESULT_OK) return result;
    factory_fluid_storage_store_add(
        &simulation->fluid_storages, *out_id,
        FACTORY_FLUID_STORAGE_DEFAULT, x, y,
        FACTORY_FLUID_CLASS_AQUEOUS | FACTORY_FLUID_CLASS_VAPOR,
        FACTORY_FLUID_TANK_CAPACITY);
    factory_fluid_port_store_add(
        &simulation->fluid_ports, *out_id,
        FACTORY_FLUID_STORAGE_DEFAULT, x, y,
        FACTORY_FLUID_CONNECTION_ALL,
        FACTORY_FLUID_CLASS_AQUEOUS | FACTORY_FLUID_CLASS_VAPOR);
    simulation->fluid_networks.dirty = true;
    return FACTORY_RESULT_OK;
}

static FactoryResult place_pipe(
    FactorySimulation *simulation, const FactoryCommand *command,
    FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_pipe.x;
    int32_t y = command->data.place_pipe.y;
    FactoryResult result = validate_empty_tile(simulation, x, y);
    if (result != FACTORY_RESULT_OK) return result;
    if (!factory_pipe_store_reserve_one(&simulation->pipes))
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result = occupy_with_entity(simulation, x, y, out_id);
    if (result != FACTORY_RESULT_OK) return result;
    factory_pipe_store_add(&simulation->pipes, *out_id, x, y);
    simulation->fluid_networks.dirty = true;
    return FACTORY_RESULT_OK;
}

static bool reserve_two_fluid_storages(FactoryFluidStorageStore *store)
{
    if (!factory_fluid_storage_store_reserve_one(store)) return false;
    if (store->capacity - store->count >= 2U) return true;
    ++store->count;
    bool ok = factory_fluid_storage_store_reserve_one(store);
    --store->count;
    return ok;
}

static bool reserve_two_fluid_ports(FactoryFluidPortStore *store)
{
    if (!factory_fluid_port_store_reserve_one(store)) return false;
    if (store->capacity - store->count >= 2U) return true;
    ++store->count;
    bool ok = factory_fluid_port_store_reserve_one(store);
    --store->count;
    return ok;
}

static FactoryResult place_water_extractor(
    FactorySimulation *s, const FactoryCommand *command, FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_water_extractor.x;
    int32_t y = command->data.place_water_extractor.y;
    FactoryResult result = validate_empty_tile(s, x, y);
    if (result != FACTORY_RESULT_OK) return result;
    if (!factory_water_extractor_store_reserve_one(&s->water_extractors)
        || !factory_fluid_storage_store_reserve_one(&s->fluid_storages)
        || !factory_fluid_port_store_reserve_one(&s->fluid_ports))
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result = occupy_with_entity(s, x, y, out_id);
    if (result != FACTORY_RESULT_OK) return result;
    factory_water_extractor_store_add(&s->water_extractors, *out_id, x, y);
    factory_fluid_storage_store_add(
        &s->fluid_storages, *out_id, FACTORY_FLUID_STORAGE_DEFAULT, x, y,
        FACTORY_FLUID_CLASS_AQUEOUS, FACTORY_WATER_EXTRACTOR_CAPACITY);
    factory_fluid_port_store_add(
        &s->fluid_ports, *out_id, FACTORY_FLUID_STORAGE_DEFAULT, x, y,
        FACTORY_FLUID_CONNECTION_ALL, FACTORY_FLUID_CLASS_AQUEOUS);
    s->fluid_networks.dirty = true;
    return FACTORY_RESULT_OK;
}

static FactoryResult place_boiler(
    FactorySimulation *s, const FactoryCommand *command, FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_boiler.x;
    int32_t y = command->data.place_boiler.y;
    FactoryResult result = validate_empty_tile(s, x, y);
    if (result != FACTORY_RESULT_OK) return result;
    if (!factory_boiler_store_reserve_one(&s->boilers)
        || !factory_burner_store_reserve_one(&s->burners)
        || !reserve_two_fluid_storages(&s->fluid_storages)
        || !reserve_two_fluid_ports(&s->fluid_ports))
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result = occupy_with_entity(s, x, y, out_id);
    if (result != FACTORY_RESULT_OK) return result;
    factory_boiler_store_add(&s->boilers, *out_id, x, y);
    factory_burner_store_add(&s->burners, *out_id, FACTORY_FUEL_CLASS_SOLID);
    factory_fluid_storage_store_add(
        &s->fluid_storages, *out_id, FACTORY_FLUID_STORAGE_BOILER_INPUT, x, y,
        FACTORY_FLUID_CLASS_AQUEOUS, FACTORY_BOILER_STORAGE_CAPACITY);
    factory_fluid_storage_store_add(
        &s->fluid_storages, *out_id, FACTORY_FLUID_STORAGE_BOILER_OUTPUT, x, y,
        FACTORY_FLUID_CLASS_VAPOR, FACTORY_BOILER_STORAGE_CAPACITY);
    factory_fluid_port_store_add(
        &s->fluid_ports, *out_id, FACTORY_FLUID_STORAGE_BOILER_INPUT, x, y,
        FACTORY_FLUID_CONNECTION_WEST, FACTORY_FLUID_CLASS_AQUEOUS);
    factory_fluid_port_store_add(
        &s->fluid_ports, *out_id, FACTORY_FLUID_STORAGE_BOILER_OUTPUT, x, y,
        FACTORY_FLUID_CONNECTION_EAST, FACTORY_FLUID_CLASS_VAPOR);
    s->fluid_networks.dirty = true;
    return FACTORY_RESULT_OK;
}

static FactoryResult place_steam_engine(
    FactorySimulation *s, const FactoryCommand *command, FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_steam_engine.x;
    int32_t y = command->data.place_steam_engine.y;
    FactoryResult result = validate_empty_tile(s, x, y);
    if (result != FACTORY_RESULT_OK) return result;
    if (!factory_steam_engine_store_reserve_one(&s->steam_engines)
        || !factory_power_generator_store_reserve_one(&s->power_generators)
        || !factory_fluid_storage_store_reserve_one(&s->fluid_storages)
        || !factory_fluid_port_store_reserve_one(&s->fluid_ports))
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result = occupy_with_entity(s, x, y, out_id);
    if (result != FACTORY_RESULT_OK) return result;
    factory_steam_engine_store_add(&s->steam_engines, *out_id, x, y);
    factory_power_generator_store_add(&s->power_generators, *out_id, x, y);
    factory_fluid_storage_store_add(
        &s->fluid_storages, *out_id,
        FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT, x, y,
        FACTORY_FLUID_CLASS_VAPOR, FACTORY_STEAM_ENGINE_STORAGE_CAPACITY);
    factory_fluid_port_store_add(
        &s->fluid_ports, *out_id,
        FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT, x, y,
        FACTORY_FLUID_CONNECTION_WEST, FACTORY_FLUID_CLASS_VAPOR);
    s->fluid_networks.dirty = true;
    return FACTORY_RESULT_OK;
}

static FactoryResult place_solar_generator(
    FactorySimulation *s, const FactoryCommand *command, FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_solar_generator.x;
    int32_t y = command->data.place_solar_generator.y;
    FactoryResult result = validate_empty_tile(s, x, y);
    if (result != FACTORY_RESULT_OK) return result;
    if (!factory_solar_generator_store_reserve_one(&s->solar_generators)
        || !factory_power_generator_store_reserve_one(&s->power_generators))
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result = occupy_with_entity(s, x, y, out_id);
    if (result != FACTORY_RESULT_OK) return result;
    factory_solar_generator_store_add(&s->solar_generators, *out_id, x, y);
    factory_power_generator_store_add(&s->power_generators, *out_id, x, y);
    return FACTORY_RESULT_OK;
}

static FactoryResult place_accumulator(
    FactorySimulation *s, const FactoryCommand *command, FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_accumulator.x;
    int32_t y = command->data.place_accumulator.y;
    FactoryResult result = validate_empty_tile(s, x, y);
    if (result != FACTORY_RESULT_OK) return result;
    if (!factory_accumulator_store_reserve_one(&s->accumulators))
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result = occupy_with_entity(s, x, y, out_id);
    if (result != FACTORY_RESULT_OK) return result;
    factory_accumulator_store_add(&s->accumulators, *out_id, x, y);
    return FACTORY_RESULT_OK;
}

static FactoryResult place_reactor_core(
    FactorySimulation *s, const FactoryCommand *command, FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_reactor_core.x;
    int32_t y = command->data.place_reactor_core.y;
    FactoryResult result = validate_empty_tile(s, x, y);
    if (result != FACTORY_RESULT_OK) return result;
    if (!factory_reactor_store_reserve_one(&s->reactors)
        || !factory_heat_port_store_reserve_one(&s->heat_ports))
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result = occupy_with_entity(s, x, y, out_id);
    if (result != FACTORY_RESULT_OK) return result;
    factory_reactor_store_add(&s->reactors, *out_id, x, y);
    factory_heat_port_store_add(&s->heat_ports, *out_id,
        FACTORY_HEAT_PORT_REACTOR_OUTPUT, x, y);
    s->heat_networks.dirty = true;
    return FACTORY_RESULT_OK;
}

static FactoryResult place_heat_conductor(
    FactorySimulation *s, const FactoryCommand *command, FactoryEntityId *out_id)
{
    int32_t x=command->data.place_heat_conductor.x;
    int32_t y=command->data.place_heat_conductor.y;
    FactoryResult result=validate_empty_tile(s,x,y);
    if (result!=FACTORY_RESULT_OK) return result;
    if (!factory_heat_conductor_store_reserve_one(&s->heat_conductors))
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result=occupy_with_entity(s,x,y,out_id);
    if (result!=FACTORY_RESULT_OK) return result;
    factory_heat_conductor_store_add(&s->heat_conductors,*out_id,x,y);
    s->heat_networks.dirty=true;
    return FACTORY_RESULT_OK;
}

static FactoryResult place_heat_exchanger(
    FactorySimulation *s, const FactoryCommand *command, FactoryEntityId *out_id)
{
    int32_t x=command->data.place_heat_exchanger.x;
    int32_t y=command->data.place_heat_exchanger.y;
    FactoryResult result=validate_empty_tile(s,x,y);
    if (result!=FACTORY_RESULT_OK) return result;
    if (!factory_heat_exchanger_store_reserve_one(&s->heat_exchangers)
        || !factory_heat_port_store_reserve_one(&s->heat_ports)
        || !reserve_two_fluid_storages(&s->fluid_storages)
        || !reserve_two_fluid_ports(&s->fluid_ports))
        return FACTORY_RESULT_OUT_OF_MEMORY;
    result=occupy_with_entity(s,x,y,out_id);
    if (result!=FACTORY_RESULT_OK) return result;
    factory_heat_exchanger_store_add(&s->heat_exchangers,*out_id,x,y);
    factory_heat_port_store_add(&s->heat_ports,*out_id,
        FACTORY_HEAT_PORT_EXCHANGER_INPUT,x,y);
    factory_fluid_storage_store_add(&s->fluid_storages,*out_id,
        FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT,x,y,
        FACTORY_FLUID_CLASS_AQUEOUS,FACTORY_HEAT_EXCHANGER_WATER_CAPACITY);
    factory_fluid_storage_store_add(&s->fluid_storages,*out_id,
        FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_OUTPUT,x,y,
        FACTORY_FLUID_CLASS_VAPOR,FACTORY_HEAT_EXCHANGER_STEAM_CAPACITY);
    factory_fluid_port_store_add(&s->fluid_ports,*out_id,
        FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT,x,y,
        FACTORY_FLUID_CONNECTION_WEST,FACTORY_FLUID_CLASS_AQUEOUS);
    factory_fluid_port_store_add(&s->fluid_ports,*out_id,
        FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_OUTPUT,x,y,
        FACTORY_FLUID_CONNECTION_EAST,FACTORY_FLUID_CLASS_VAPOR);
    s->heat_networks.dirty=true; s->fluid_networks.dirty=true;
    return FACTORY_RESULT_OK;
}

static FactoryResult insert_reactor_fuel(
    FactorySimulation *s, const FactoryCommand *command,
    FactoryEntityId *out_id)
{
    FactoryReactor *reactor = factory_reactor_store_find_mutable(
        &s->reactors, command->data.insert_reactor_fuel.reactor_entity_id);
    FactoryResult result;
    if (reactor == NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    result = factory_reactor_insert_fuel(
        reactor, command->data.insert_reactor_fuel.fuel_id);
    if (result != FACTORY_RESULT_OK) return result;
    *out_id = reactor->entity_id;
    factory_simulation_emit_event(s, (FactoryEvent){
        .type = FACTORY_EVENT_REACTOR_FUELED,
        .entity_id = reactor->entity_id,
        .nuclear_fuel_id = command->data.insert_reactor_fuel.fuel_id,
        .quantity = 1U
    });
    return FACTORY_RESULT_OK;
}

static FactoryResult place_refinery(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_refinery.x;
    int32_t y = command->data.place_refinery.y;
    FactoryResult result = validate_empty_tile(simulation, x, y);

    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    if (!factory_refinery_store_reserve_one(&simulation->refineries)) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    result = occupy_with_entity(simulation, x, y, out_id);
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    factory_refinery_store_add(
        &simulation->refineries,
        *out_id,
        x,
        y,
        command->data.place_refinery.input_direction,
        command->data.place_refinery.output_direction
    );
    return FACTORY_RESULT_OK;
}

static FactoryResult set_refinery_recipe(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id
)
{
    FactoryEntityId id = command->data.set_refinery_recipe.refinery_entity;
    FactoryRecipeId recipe_id = command->data.set_refinery_recipe.recipe_id;
    FactoryRefinery *refinery;

    if (factory_recipe_get(recipe_id) == NULL
        || !factory_entity_is_valid(simulation->entities, id)) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    refinery = factory_refinery_store_find_mutable(
        &simulation->refineries, id
    );
    if (refinery == NULL) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    if (refinery->recipe_id == recipe_id) {
        *out_id = id;
        return FACTORY_RESULT_OK;
    }
    if (refinery->processing
        || refinery->processing_progress != 0U
        || refinery->input_item != FACTORY_ITEM_NONE
        || refinery->input_amount != 0U
        || refinery->output_item != FACTORY_ITEM_NONE
        || refinery->output_amount != 0U) {
        return FACTORY_RESULT_INVALID_STATE;
    }
    refinery->recipe_id = recipe_id;
    *out_id = id;
    return FACTORY_RESULT_OK;
}

static FactoryResult place_assembler(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_assembler.x;
    int32_t y = command->data.place_assembler.y;
    FactoryResult result = validate_empty_tile(simulation, x, y);

    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    if (!factory_assembler_store_reserve_one(&simulation->assemblers)) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    result = occupy_with_entity(simulation, x, y, out_id);
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    factory_assembler_store_add(
        &simulation->assemblers,
        *out_id,
        x,
        y,
        command->data.place_assembler.output_direction
    );
    return FACTORY_RESULT_OK;
}

static FactoryResult place_splitter(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_splitter.x;
    int32_t y = command->data.place_splitter.y;
    FactoryResult result = validate_empty_tile(simulation, x, y);

    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    if (!factory_splitter_store_reserve_one(&simulation->splitters)) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    result = occupy_with_entity(simulation, x, y, out_id);
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    factory_splitter_store_add(
        &simulation->splitters,
        *out_id,
        x,
        y,
        command->data.place_splitter.facing
    );
    return FACTORY_RESULT_OK;
}

static FactoryResult place_inserter(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id
)
{
    int32_t x = command->data.place_inserter.x;
    int32_t y = command->data.place_inserter.y;
    FactoryResult result = validate_empty_tile(simulation, x, y);

    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    if (!factory_inserter_store_reserve_one(&simulation->inserters)) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    result = occupy_with_entity(simulation, x, y, out_id);
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    factory_inserter_store_add(
        &simulation->inserters,
        *out_id,
        x,
        y,
        command->data.place_inserter.facing
    );
    return FACTORY_RESULT_OK;
}

static FactoryResult validate_demolition(
    FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryEntityType *out_type,
    int32_t *out_x,
    int32_t *out_y
)
{
    const FactoryExtractor *extractor =
        factory_extractor_store_find(&simulation->extractors, id);
    const FactoryBelt *belt =
        factory_belt_store_find(&simulation->belts, id);
    const FactoryRefinery *refinery =
        factory_refinery_store_find(&simulation->refineries, id);
    const FactoryAssembler *assembler =
        factory_assembler_store_find(&simulation->assemblers, id);
    const FactoryStorage *storage =
        factory_storage_store_find(&simulation->storages, id);
    const FactorySplitter *splitter =
        factory_splitter_store_find(&simulation->splitters, id);
    const FactoryInserter *inserter =
        factory_inserter_store_find(&simulation->inserters, id);
    const FactoryPowerPole *power_pole =
        factory_power_pole_store_find(&simulation->power_poles, id);
    const FactoryPowerGenerator *power_generator =
        factory_power_generator_store_find(&simulation->power_generators, id);
    const FactoryFluidStorage *fluid_storage =
        factory_fluid_storage_store_find(&simulation->fluid_storages, id);
    const FactoryPipe *pipe =
        factory_pipe_store_find(&simulation->pipes, id);
    const FactoryWaterExtractor *water_extractor =
        factory_water_extractor_store_find(&simulation->water_extractors, id);
    const FactoryBoiler *boiler =
        factory_boiler_store_find(&simulation->boilers, id);
    const FactorySteamEngine *steam_engine =
        factory_steam_engine_store_find(&simulation->steam_engines, id);
    const FactorySolarGenerator *solar_generator =
        factory_solar_generator_store_find(&simulation->solar_generators, id);
    const FactoryAccumulator *accumulator =
        factory_accumulator_store_find(&simulation->accumulators, id);
    const FactoryReactor *reactor =
        factory_reactor_store_find(&simulation->reactors, id);
    const FactoryHeatConductor *heat_conductor =
        factory_heat_conductor_store_find(&simulation->heat_conductors, id);
    const FactoryHeatExchanger *heat_exchanger =
        factory_heat_exchanger_store_find(&simulation->heat_exchangers, id);
    const FactoryTile *tile;

    if (id == 0U) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    if (!factory_entity_is_valid(simulation->entities, id)) {
        return FACTORY_RESULT_ENTITY_NOT_FOUND;
    }
    if (extractor != NULL) {
        if (extractor->output_item != FACTORY_ITEM_NONE
            || extractor->output_amount != 0U) {
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        }
        *out_type = FACTORY_ENTITY_TYPE_EXTRACTOR;
        *out_x = extractor->x;
        *out_y = extractor->y;
    } else if (belt != NULL) {
        if (belt->item != FACTORY_ITEM_NONE) {
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        }
        if (belt->movement_progress != 0U) {
            return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
        }
        *out_type = FACTORY_ENTITY_TYPE_BELT;
        *out_x = belt->x;
        *out_y = belt->y;
    } else if (refinery != NULL) {
        if (refinery->processing || refinery->processing_progress != 0U) {
            return FACTORY_RESULT_ENTITY_BUSY;
        }
        if (refinery->input_item != FACTORY_ITEM_NONE
            || refinery->input_amount != 0U
            || refinery->output_item != FACTORY_ITEM_NONE
            || refinery->output_amount != 0U) {
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        }
        *out_type = FACTORY_ENTITY_TYPE_REFINERY;
        *out_x = refinery->x;
        *out_y = refinery->y;
    } else if (assembler != NULL) {
        if (assembler->processing || assembler->processing_progress != 0U) {
            return FACTORY_RESULT_ENTITY_BUSY;
        }
        if (assembler->input_slots[0].count != 0U
            || assembler->input_slots[1].count != 0U
            || assembler->output_item != FACTORY_ITEM_NONE
            || assembler->output_amount != 0U) {
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        }
        *out_type = FACTORY_ENTITY_TYPE_ASSEMBLER;
        *out_x = assembler->x;
        *out_y = assembler->y;
    } else if (storage != NULL) {
        if (factory_storage_get_total_amount(storage) != 0U
            || storage->output_occupied
            || storage->output_item != FACTORY_ITEM_NONE) {
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        }
        *out_type = FACTORY_ENTITY_TYPE_STORAGE;
        *out_x = storage->x;
        *out_y = storage->y;
    } else if (splitter != NULL) {
        if (splitter->item != FACTORY_ITEM_NONE) {
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        }
        *out_type = FACTORY_ENTITY_TYPE_SPLITTER;
        *out_x = splitter->x;
        *out_y = splitter->y;
    } else if (inserter != NULL) {
        if (inserter->held_item != FACTORY_ITEM_NONE
            || inserter->held_amount != 0U) {
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        }
        if (inserter->state != FACTORY_INSERTER_STATE_IDLE
            || inserter->progress != 0U) {
            return FACTORY_RESULT_ENTITY_BUSY;
        }
        *out_type = FACTORY_ENTITY_TYPE_INSERTER;
        *out_x = inserter->x;
        *out_y = inserter->y;
    } else if (power_pole != NULL) {
        *out_type = FACTORY_ENTITY_TYPE_POWER_POLE;
        *out_x = power_pole->x;
        *out_y = power_pole->y;
    } else if (accumulator != NULL) {
        *out_type = FACTORY_ENTITY_TYPE_ACCUMULATOR;
        *out_x = accumulator->x;
        *out_y = accumulator->y;
    } else if (reactor != NULL) {
        *out_type = FACTORY_ENTITY_TYPE_REACTOR_CORE;
        *out_x = reactor->x;
        *out_y = reactor->y;
    } else if (heat_conductor != NULL) {
        *out_type=FACTORY_ENTITY_TYPE_HEAT_CONDUCTOR;
        *out_x=heat_conductor->x; *out_y=heat_conductor->y;
    } else if (heat_exchanger != NULL) {
        *out_type=FACTORY_ENTITY_TYPE_HEAT_EXCHANGER;
        *out_x=heat_exchanger->x; *out_y=heat_exchanger->y;
    } else if (solar_generator != NULL) {
        if (power_generator == NULL)
            return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
        *out_type = FACTORY_ENTITY_TYPE_SOLAR_GENERATOR;
        *out_x = solar_generator->x;
        *out_y = solar_generator->y;
    } else if (steam_engine != NULL) {
        const FactoryFluidStorage *steam =
            factory_fluid_storage_store_find_slot(
                &simulation->fluid_storages, id,
                FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT);
        if (power_generator == NULL || steam == NULL)
            return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
        if (steam->quantity != 0U)
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        *out_type = FACTORY_ENTITY_TYPE_STEAM_ENGINE;
        *out_x = steam_engine->x;
        *out_y = steam_engine->y;
    } else if (power_generator != NULL) {
        const FactoryBurner *burner =
            factory_burner_store_find(&simulation->burners, id);
        if (burner == NULL)
            return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
        if (burner->inventory_quantity != 0U
            || burner->remaining_burn_ticks != 0U
            || burner->released_energy != 0U)
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        *out_type = FACTORY_ENTITY_TYPE_POWER_GENERATOR;
        *out_x = power_generator->x;
        *out_y = power_generator->y;
    } else if (water_extractor != NULL) {
        if (fluid_storage == NULL)
            return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
        if (water_extractor->progress != 0U)
            return FACTORY_RESULT_ENTITY_BUSY;
        if (fluid_storage->quantity != 0U)
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        *out_type = FACTORY_ENTITY_TYPE_WATER_EXTRACTOR;
        *out_x = water_extractor->x;
        *out_y = water_extractor->y;
    } else if (boiler != NULL) {
        const FactoryBurner *burner =
            factory_burner_store_find(&simulation->burners, id);
        const FactoryFluidStorage *input =
            factory_fluid_storage_store_find_slot(
                &simulation->fluid_storages, id,
                FACTORY_FLUID_STORAGE_BOILER_INPUT);
        const FactoryFluidStorage *output =
            factory_fluid_storage_store_find_slot(
                &simulation->fluid_storages, id,
                FACTORY_FLUID_STORAGE_BOILER_OUTPUT);
        if (burner == NULL || input == NULL || output == NULL)
            return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
        if (boiler->conversion_active)
            return FACTORY_RESULT_ENTITY_BUSY;
        if (burner->inventory_quantity != 0U
            || burner->remaining_burn_ticks != 0U
            || burner->released_energy != 0U
            || input->quantity != 0U || output->quantity != 0U)
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        *out_type = FACTORY_ENTITY_TYPE_BOILER;
        *out_x = boiler->x;
        *out_y = boiler->y;
    } else if (fluid_storage != NULL) {
        if (fluid_storage->quantity != 0U)
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        *out_type = FACTORY_ENTITY_TYPE_FLUID_TANK;
        *out_x = fluid_storage->x;
        *out_y = fluid_storage->y;
    } else if (pipe != NULL) {
        *out_type = FACTORY_ENTITY_TYPE_PIPE;
        *out_x = pipe->x;
        *out_y = pipe->y;
    } else {
        return FACTORY_RESULT_UNSUPPORTED_ENTITY;
    }
    tile = factory_world_get_tile(simulation->world, *out_x, *out_y);
    if (tile == NULL || tile->occupying_entity != id) {
        return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    }
    return FACTORY_RESULT_OK;
}

static bool remove_subsystem_record(
    FactorySimulation *simulation,
    FactoryEntityType type,
    FactoryEntityId id
)
{
    switch (type) {
        case FACTORY_ENTITY_TYPE_EXTRACTOR:
            return factory_extractor_store_remove(&simulation->extractors, id);
        case FACTORY_ENTITY_TYPE_BELT:
            return factory_belt_store_remove(&simulation->belts, id);
        case FACTORY_ENTITY_TYPE_REFINERY:
            return factory_refinery_store_remove(&simulation->refineries, id);
        case FACTORY_ENTITY_TYPE_ASSEMBLER:
            return factory_assembler_store_remove(&simulation->assemblers, id);
        case FACTORY_ENTITY_TYPE_STORAGE:
            return factory_storage_store_remove(&simulation->storages, id);
        case FACTORY_ENTITY_TYPE_SPLITTER:
            return factory_splitter_store_remove(&simulation->splitters, id);
        case FACTORY_ENTITY_TYPE_INSERTER:
            return factory_inserter_store_remove(&simulation->inserters, id);
        case FACTORY_ENTITY_TYPE_POWER_POLE:
            return factory_power_pole_store_remove(
                &simulation->power_poles, id
            );
        case FACTORY_ENTITY_TYPE_POWER_GENERATOR:
            if (!factory_burner_store_remove(&simulation->burners, id))
                return false;
            return factory_power_generator_store_remove(
                &simulation->power_generators, id);
        case FACTORY_ENTITY_TYPE_FLUID_TANK:
            if (!factory_fluid_port_store_remove(
                    &simulation->fluid_ports, id)) return false;
            simulation->fluid_networks.dirty = true;
            return factory_fluid_storage_store_remove(
                &simulation->fluid_storages, id);
        case FACTORY_ENTITY_TYPE_PIPE:
            simulation->fluid_networks.dirty = true;
            return factory_pipe_store_remove(&simulation->pipes, id);
        case FACTORY_ENTITY_TYPE_WATER_EXTRACTOR:
            if (!factory_fluid_port_store_remove(
                    &simulation->fluid_ports, id)
                || !factory_fluid_storage_store_remove(
                    &simulation->fluid_storages, id)) return false;
            simulation->fluid_networks.dirty = true;
            return factory_water_extractor_store_remove(
                &simulation->water_extractors, id);
        case FACTORY_ENTITY_TYPE_BOILER:
            if (!factory_fluid_port_store_remove(
                    &simulation->fluid_ports, id)
                || !factory_fluid_port_store_remove(
                    &simulation->fluid_ports, id)
                || !factory_fluid_storage_store_remove(
                    &simulation->fluid_storages, id)
                || !factory_fluid_storage_store_remove(
                    &simulation->fluid_storages, id)
                || !factory_burner_store_remove(&simulation->burners, id))
                return false;
            simulation->fluid_networks.dirty = true;
            return factory_boiler_store_remove(&simulation->boilers, id);
        case FACTORY_ENTITY_TYPE_STEAM_ENGINE:
            if (!factory_fluid_port_store_remove(
                    &simulation->fluid_ports, id)
                || !factory_fluid_storage_store_remove(
                    &simulation->fluid_storages, id)
                || !factory_power_generator_store_remove(
                    &simulation->power_generators, id))
                return false;
            simulation->fluid_networks.dirty = true;
            return factory_steam_engine_store_remove(
                &simulation->steam_engines, id);
        case FACTORY_ENTITY_TYPE_SOLAR_GENERATOR:
            if (!factory_power_generator_store_remove(
                    &simulation->power_generators, id))
                return false;
            return factory_solar_generator_store_remove(
                &simulation->solar_generators, id);
        case FACTORY_ENTITY_TYPE_ACCUMULATOR:
            return factory_accumulator_store_remove(
                &simulation->accumulators, id);
        case FACTORY_ENTITY_TYPE_REACTOR_CORE:
            if (!factory_heat_port_store_remove(&simulation->heat_ports,id))
                return false;
            simulation->heat_networks.dirty=true;
            return factory_reactor_store_remove(&simulation->reactors, id);
        case FACTORY_ENTITY_TYPE_HEAT_CONDUCTOR:
            simulation->heat_networks.dirty=true;
            return factory_heat_conductor_store_remove(
                &simulation->heat_conductors,id);
        case FACTORY_ENTITY_TYPE_HEAT_EXCHANGER:
            if (!factory_heat_port_store_remove(&simulation->heat_ports,id)
                || !factory_fluid_port_store_remove(&simulation->fluid_ports,id)
                || !factory_fluid_port_store_remove(&simulation->fluid_ports,id)
                || !factory_fluid_storage_store_remove(
                    &simulation->fluid_storages,id)
                || !factory_fluid_storage_store_remove(
                    &simulation->fluid_storages,id))
                return false;
            simulation->heat_networks.dirty=true;
            simulation->fluid_networks.dirty=true;
            return factory_heat_exchanger_store_remove(
                &simulation->heat_exchangers,id);
        case FACTORY_ENTITY_TYPE_NONE:
        default:
            return false;
    }
}

static FactoryResult demolish_entity(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityType *out_type,
    int32_t *out_x,
    int32_t *out_y
)
{
    FactoryEntityId id = command->data.demolish_entity.entity_id;
    FactoryConstructionMaterial refund;
    FactoryResult result = validate_demolition(
        simulation, id, out_type, out_x, out_y
    );

    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    if (!factory_entity_construction_cost(*out_type, &refund)) {
        return FACTORY_RESULT_UNSUPPORTED_ENTITY;
    }
    if (!factory_construction_inventory_can_credit(
            &simulation->construction_inventory, refund)) {
        return FACTORY_RESULT_CONSTRUCTION_INVENTORY_OVERFLOW;
    }
    result = factory_world_clear_occupying_entity(
        simulation->world, *out_x, *out_y, id
    );
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    if (!remove_subsystem_record(simulation, *out_type, id)) {
        return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    }
    factory_entity_destroy(simulation->entities, id);
    factory_construction_inventory_credit_validated(
        &simulation->construction_inventory, refund
    );
    return FACTORY_RESULT_OK;
}

static bool placement_type(
    FactoryCommandType command_type,
    FactoryEntityType *out_type
)
{
    switch (command_type) {
        case FACTORY_COMMAND_PLACE_EXTRACTOR:
            *out_type = FACTORY_ENTITY_TYPE_EXTRACTOR;
            return true;
        case FACTORY_COMMAND_PLACE_BELT:
            *out_type = FACTORY_ENTITY_TYPE_BELT;
            return true;
        case FACTORY_COMMAND_PLACE_STORAGE:
            *out_type = FACTORY_ENTITY_TYPE_STORAGE;
            return true;
        case FACTORY_COMMAND_PLACE_REFINERY:
            *out_type = FACTORY_ENTITY_TYPE_REFINERY;
            return true;
        case FACTORY_COMMAND_PLACE_ASSEMBLER:
            *out_type = FACTORY_ENTITY_TYPE_ASSEMBLER;
            return true;
        case FACTORY_COMMAND_PLACE_SPLITTER:
            *out_type = FACTORY_ENTITY_TYPE_SPLITTER;
            return true;
        case FACTORY_COMMAND_PLACE_INSERTER:
            *out_type = FACTORY_ENTITY_TYPE_INSERTER;
            return true;
        case FACTORY_COMMAND_PLACE_POWER_POLE:
            *out_type = FACTORY_ENTITY_TYPE_POWER_POLE;
            return true;
        case FACTORY_COMMAND_PLACE_POWER_GENERATOR:
            *out_type = FACTORY_ENTITY_TYPE_POWER_GENERATOR;
            return true;
        case FACTORY_COMMAND_PLACE_FLUID_TANK:
            *out_type = FACTORY_ENTITY_TYPE_FLUID_TANK;
            return true;
        case FACTORY_COMMAND_PLACE_PIPE:
            *out_type = FACTORY_ENTITY_TYPE_PIPE;
            return true;
        case FACTORY_COMMAND_PLACE_WATER_EXTRACTOR:
            *out_type = FACTORY_ENTITY_TYPE_WATER_EXTRACTOR;
            return true;
        case FACTORY_COMMAND_PLACE_BOILER:
            *out_type = FACTORY_ENTITY_TYPE_BOILER;
            return true;
        case FACTORY_COMMAND_PLACE_STEAM_ENGINE:
            *out_type = FACTORY_ENTITY_TYPE_STEAM_ENGINE;
            return true;
        case FACTORY_COMMAND_PLACE_SOLAR_GENERATOR:
            *out_type = FACTORY_ENTITY_TYPE_SOLAR_GENERATOR;
            return true;
        case FACTORY_COMMAND_PLACE_ACCUMULATOR:
            *out_type = FACTORY_ENTITY_TYPE_ACCUMULATOR;
            return true;
        case FACTORY_COMMAND_PLACE_REACTOR_CORE:
            *out_type = FACTORY_ENTITY_TYPE_REACTOR_CORE;
            return true;
        case FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR:
            *out_type=FACTORY_ENTITY_TYPE_HEAT_CONDUCTOR; return true;
        case FACTORY_COMMAND_PLACE_HEAT_EXCHANGER:
            *out_type=FACTORY_ENTITY_TYPE_HEAT_EXCHANGER; return true;
        default:
            return false;
    }
}

static FactoryResult grant_construction_units(
    FactorySimulation *simulation,
    FactoryConstructionMaterial amount
)
{
    if (!factory_construction_inventory_can_credit(
            &simulation->construction_inventory, amount)) {
        return FACTORY_RESULT_CONSTRUCTION_INVENTORY_OVERFLOW;
    }
    factory_construction_inventory_credit_validated(
        &simulation->construction_inventory, amount
    );
    return FACTORY_RESULT_OK;
}

static FactoryResult set_assembler_recipe(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id,
    FactoryAssemblerRecipeId *out_previous,
    FactoryAssemblerRecipeId *out_new
)
{
    FactoryEntityId id =
        command->data.set_assembler_recipe.assembler_entity;
    FactoryAssemblerRecipeId recipe_id =
        command->data.set_assembler_recipe.recipe_id;
    FactoryAssembler *assembler;

    if (recipe_id < FACTORY_ASSEMBLER_RECIPE_NONE
        || recipe_id >= FACTORY_ASSEMBLER_RECIPE_COUNT
        || (recipe_id != FACTORY_ASSEMBLER_RECIPE_NONE
            && !factory_assembler_recipe_get(recipe_id,
                &(FactoryAssemblerRecipe){0}))) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    if (!factory_entity_is_valid(simulation->entities, id)) {
        return FACTORY_RESULT_ENTITY_NOT_FOUND;
    }
    assembler = factory_assembler_store_find_mutable(
        &simulation->assemblers, id
    );
    if (assembler == NULL) {
        return FACTORY_RESULT_UNSUPPORTED_ENTITY;
    }
    *out_previous = assembler->recipe_id;
    *out_new = recipe_id;
    *out_id = id;
    if (assembler->processing
        || assembler->processing_progress != 0U
        || assembler->input_slots[0].count != 0U
        || assembler->input_slots[1].count != 0U
        || assembler->output_item != FACTORY_ITEM_NONE
        || assembler->output_amount != 0U) {
        return FACTORY_RESULT_ASSEMBLER_NOT_EMPTY;
    }
    if (assembler->recipe_id == recipe_id) {
        return FACTORY_RESULT_OK;
    }
    return factory_assembler_configure_recipe(assembler, recipe_id)
        ? FACTORY_RESULT_OK : FACTORY_RESULT_INVALID_ARGUMENT;
}

static FactoryResult apply_fluid_command(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_entity_id
)
{
    FactoryFluidStorage *source;
    FactoryFluidStorage *destination;
    FactoryFluidType fluid_type = FACTORY_FLUID_NONE;
    FactoryFluidQuantity quantity;
    FactoryResult result;
    if (command->type == FACTORY_COMMAND_FLUID_INSERT) {
        destination = factory_fluid_storage_store_find_mutable(
            &simulation->fluid_storages,
            command->data.fluid_insert.destination_entity_id);
        if (destination == NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
        result = factory_fluid_storage_insert(
            destination, command->data.fluid_insert.fluid_type,
            command->data.fluid_insert.quantity);
        if (result != FACTORY_RESULT_OK) return result;
        *out_entity_id = destination->owner_entity_id;
        factory_simulation_emit_event(simulation, (FactoryEvent){
            .type = FACTORY_EVENT_FLUID_INSERTED,
            .entity_id = destination->owner_entity_id,
            .fluid_type = command->data.fluid_insert.fluid_type,
            .quantity = command->data.fluid_insert.quantity
        });
        return FACTORY_RESULT_OK;
    }
    if (command->type == FACTORY_COMMAND_FLUID_REMOVE) {
        source = factory_fluid_storage_store_find_mutable(
            &simulation->fluid_storages,
            command->data.fluid_remove.source_entity_id);
        if (source == NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
        quantity = command->data.fluid_remove.quantity;
        result = factory_fluid_storage_remove(
            source, quantity, &fluid_type);
        if (result != FACTORY_RESULT_OK) return result;
        *out_entity_id = source->owner_entity_id;
        factory_simulation_emit_event(simulation, (FactoryEvent){
            .type = FACTORY_EVENT_FLUID_REMOVED,
            .entity_id = source->owner_entity_id,
            .fluid_type = fluid_type,
            .quantity = quantity
        });
        return FACTORY_RESULT_OK;
    }
    source = factory_fluid_storage_store_find_mutable(
        &simulation->fluid_storages,
        command->data.fluid_transfer.source_entity_id);
    destination = factory_fluid_storage_store_find_mutable(
        &simulation->fluid_storages,
        command->data.fluid_transfer.destination_entity_id);
    if (source == NULL || destination == NULL)
        return FACTORY_RESULT_ENTITY_NOT_FOUND;
    quantity = command->data.fluid_transfer.quantity;
    result = factory_fluid_storage_transfer(
        source, destination, quantity, &fluid_type);
    if (result != FACTORY_RESULT_OK) return result;
    *out_entity_id = source->owner_entity_id;
    factory_simulation_emit_event(simulation, (FactoryEvent){
        .type = FACTORY_EVENT_FLUID_TRANSFERRED,
        .entity_id = source->owner_entity_id,
        .related_entity_id = destination->owner_entity_id,
        .fluid_type = fluid_type,
        .quantity = quantity
    });
    return FACTORY_RESULT_OK;
}

static FactoryResult set_storage_output(
    FactorySimulation *simulation,
    const FactoryCommand *command,
    FactoryEntityId *out_id,
    FactoryItemType *out_previous,
    FactoryItemType *out_new
)
{
    FactoryEntityId id = command->data.set_storage_output.storage_entity;
    FactoryItemType item = command->data.set_storage_output.item;
    FactoryStorage *storage;

    if (item < FACTORY_ITEM_NONE
        || item > FACTORY_ITEM_BIOMASS_PELLET) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    if (!factory_entity_is_valid(simulation->entities, id)) {
        return FACTORY_RESULT_ENTITY_NOT_FOUND;
    }
    storage = factory_storage_store_find_mutable(&simulation->storages, id);
    if (storage == NULL) {
        return FACTORY_RESULT_UNSUPPORTED_ENTITY;
    }
    *out_id = id;
    *out_previous = storage->configured_output_item;
    *out_new = item;
    if (storage->configured_output_item == item) {
        return FACTORY_RESULT_OK;
    }
    if (storage->output_occupied
        || storage->output_item != FACTORY_ITEM_NONE) {
        return FACTORY_RESULT_STORAGE_OUTPUT_NOT_EMPTY;
    }
    storage->configured_output_item = item;
    return FACTORY_RESULT_OK;
}

static void apply_commands(FactorySimulation *simulation)
{
    size_t index;

    simulation->result_count = simulation->command_count;
    for (index = 0U; index < simulation->command_count; ++index) {
        FactoryCommandResult *result = &simulation->results[index];

        result->command = simulation->commands[index];
        result->entity_id = 0U;
        result->entity_type = FACTORY_ENTITY_TYPE_NONE;
        result->x = 0;
        result->y = 0;
        result->construction_units_changed = 0U;
        result->construction_units_remaining =
            simulation->construction_inventory.units;
        result->previous_assembler_recipe = FACTORY_ASSEMBLER_RECIPE_NONE;
        result->new_assembler_recipe = FACTORY_ASSEMBLER_RECIPE_NONE;
        result->previous_storage_output = FACTORY_ITEM_NONE;
        result->new_storage_output = FACTORY_ITEM_NONE;
        if (placement_type(
                result->command.type, &result->entity_type)) {
            FactoryConstructionMaterial cost;

            if (!factory_entity_construction_cost(
                    result->entity_type, &cost)) {
                result->result = FACTORY_RESULT_UNSUPPORTED_ENTITY;
                continue;
            }
            if (!factory_construction_inventory_can_spend(
                    &simulation->construction_inventory, cost)) {
                result->result =
                    FACTORY_RESULT_INSUFFICIENT_CONSTRUCTION_UNITS;
                continue;
            }
        }
        switch (result->command.type) {
            case FACTORY_COMMAND_PLACE_EXTRACTOR:
                result->result = place_extractor(
                    simulation, &result->command, &result->entity_id
                );
                break;
            case FACTORY_COMMAND_PLACE_BELT:
                result->result = place_belt(
                    simulation, &result->command, &result->entity_id
                );
                break;
            case FACTORY_COMMAND_PLACE_STORAGE:
                result->result = place_storage(
                    simulation, &result->command, &result->entity_id
                );
                break;
            case FACTORY_COMMAND_PLACE_REFINERY:
                result->result = place_refinery(
                    simulation, &result->command, &result->entity_id
                );
                break;
            case FACTORY_COMMAND_SET_REFINERY_RECIPE:
                result->result = set_refinery_recipe(
                    simulation, &result->command, &result->entity_id
                );
                break;
            case FACTORY_COMMAND_PLACE_ASSEMBLER:
                result->result = place_assembler(
                    simulation, &result->command, &result->entity_id
                );
                break;
            case FACTORY_COMMAND_PLACE_SPLITTER:
                result->result = place_splitter(
                    simulation, &result->command, &result->entity_id
                );
                break;
            case FACTORY_COMMAND_PLACE_INSERTER:
                result->result = place_inserter(
                    simulation, &result->command, &result->entity_id
                );
                break;
            case FACTORY_COMMAND_DEMOLISH_ENTITY:
                result->entity_id =
                    result->command.data.demolish_entity.entity_id;
                result->result = demolish_entity(
                    simulation,
                    &result->command,
                    &result->entity_type,
                    &result->x,
                    &result->y
                );
                break;
            case FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS:
                result->result = grant_construction_units(
                    simulation,
                    result->command.data.grant_construction_units.amount
                );
                break;
            case FACTORY_COMMAND_SET_ASSEMBLER_RECIPE:
                result->result = set_assembler_recipe(
                    simulation,
                    &result->command,
                    &result->entity_id,
                    &result->previous_assembler_recipe,
                    &result->new_assembler_recipe
                );
                break;
            case FACTORY_COMMAND_SET_STORAGE_OUTPUT:
                result->result = set_storage_output(
                    simulation,
                    &result->command,
                    &result->entity_id,
                    &result->previous_storage_output,
                    &result->new_storage_output
                );
                break;
            case FACTORY_COMMAND_PLACE_POWER_POLE:
                result->result = place_power_entity(
                    simulation,
                    result->command.data.place_power_pole.x,
                    result->command.data.place_power_pole.y,
                    false,
                    &result->entity_id
                );
                break;
            case FACTORY_COMMAND_PLACE_POWER_GENERATOR:
                result->result = place_power_entity(
                    simulation,
                    result->command.data.place_power_generator.x,
                    result->command.data.place_power_generator.y,
                    true,
                    &result->entity_id
                );
                break;
            case FACTORY_COMMAND_PLACE_FLUID_TANK:
                result->result = place_fluid_tank(
                    simulation, &result->command, &result->entity_id);
                break;
            case FACTORY_COMMAND_PLACE_PIPE:
                result->result = place_pipe(
                    simulation, &result->command, &result->entity_id);
                break;
            case FACTORY_COMMAND_PLACE_WATER_EXTRACTOR:
                result->result = place_water_extractor(
                    simulation, &result->command, &result->entity_id);
                break;
            case FACTORY_COMMAND_PLACE_BOILER:
                result->result = place_boiler(
                    simulation, &result->command, &result->entity_id);
                break;
            case FACTORY_COMMAND_PLACE_STEAM_ENGINE:
                result->result = place_steam_engine(
                    simulation, &result->command, &result->entity_id);
                break;
            case FACTORY_COMMAND_PLACE_SOLAR_GENERATOR:
                result->result = place_solar_generator(
                    simulation, &result->command, &result->entity_id);
                break;
            case FACTORY_COMMAND_PLACE_ACCUMULATOR:
                result->result = place_accumulator(
                    simulation, &result->command, &result->entity_id);
                break;
            case FACTORY_COMMAND_PLACE_REACTOR_CORE:
                result->result = place_reactor_core(
                    simulation, &result->command, &result->entity_id);
                break;
            case FACTORY_COMMAND_INSERT_REACTOR_FUEL:
                result->result = insert_reactor_fuel(
                    simulation, &result->command, &result->entity_id);
                break;
            case FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR:
                result->result=place_heat_conductor(
                    simulation,&result->command,&result->entity_id);
                break;
            case FACTORY_COMMAND_PLACE_HEAT_EXCHANGER:
                result->result=place_heat_exchanger(
                    simulation,&result->command,&result->entity_id);
                break;
            case FACTORY_COMMAND_FLUID_INSERT:
            case FACTORY_COMMAND_FLUID_REMOVE:
            case FACTORY_COMMAND_FLUID_TRANSFER:
                result->result = apply_fluid_command(
                    simulation, &result->command, &result->entity_id);
                break;
        }
        if (result->result == FACTORY_RESULT_OK) {
            FactoryConstructionMaterial amount;

            if (placement_type(
                    result->command.type, &result->entity_type)
                && factory_entity_construction_cost(
                    result->entity_type, &amount)) {
                factory_construction_inventory_spend_validated(
                    &simulation->construction_inventory, amount
                );
                result->construction_units_changed = amount;
                factory_simulation_emit_event(simulation, (FactoryEvent){
                    .type = FACTORY_EVENT_ENTITY_CONSTRUCTED,
                    .entity_id = result->entity_id,
                    .entity_type = result->entity_type
                });
            } else if (result->command.type
                == FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS) {
                result->construction_units_changed =
                    result->command.data.grant_construction_units.amount;
            } else if (result->command.type
                == FACTORY_COMMAND_DEMOLISH_ENTITY
                && factory_entity_construction_cost(
                    result->entity_type, &amount)) {
                result->construction_units_changed = amount;
                factory_simulation_emit_event(simulation, (FactoryEvent){
                    .type = FACTORY_EVENT_ENTITY_DEMOLISHED,
                    .entity_id = result->entity_id,
                    .entity_type = result->entity_type
                });
            }
        }
        result->construction_units_remaining =
            simulation->construction_inventory.units;
    }
    simulation->command_count = 0U;
    simulation->fixture_initial_generator_fuel = 0U;
}

FactoryConstructionMaterial factory_simulation_construction_units(
    const FactorySimulation *simulation
)
{
    return simulation == NULL
        ? 0U
        : simulation->construction_inventory.units;
}

static void add_producer_intent(
    FactorySimulation *simulation,
    TransferIntent *intents,
    size_t *count,
    FactoryEntityId source_id,
    int32_t x,
    int32_t y,
    FactoryDirection direction,
    FactoryItemType item
)
{
    const FactoryTile *tile;
    const FactoryBelt *belt;
    int32_t target_x;
    int32_t target_y;

    adjacent_coordinate(x, y, direction, &target_x, &target_y);
    tile = factory_world_get_tile(simulation->world, target_x, target_y);
    if (tile == NULL) {
        return;
    }
    belt = factory_belt_store_find(&simulation->belts, tile->occupying_entity);
    if (belt == NULL) {
        return;
    }
    intents[*count].source = (FactoryLogisticsEndpoint){
        source_id, FACTORY_LOGISTICS_SLOT_OUTPUT
    };
    intents[*count].destination = (FactoryLogisticsEndpoint){
        belt->entity_id, FACTORY_LOGISTICS_SLOT_MAIN
    };
    if (factory_logistics_endpoint_can_accept(
            simulation, intents[*count].destination, item)
        != FACTORY_LOGISTICS_RESULT_OK) {
        return;
    }
    intents[*count].item = item;
    intents[*count].wins = true;
    ++*count;
}

static bool splitter_output_is_available(
    FactorySimulation *simulation,
    const FactorySplitter *splitter,
    FactorySplitterOutput output,
    FactoryEntityId *out_belt_id
)
{
    FactoryDirection direction = splitter_output_direction(
        splitter->facing, output
    );
    const FactoryTile *tile;
    const FactoryBelt *belt;
    int32_t x;
    int32_t y;

    adjacent_coordinate(splitter->x, splitter->y, direction, &x, &y);
    tile = factory_world_get_tile(simulation->world, x, y);
    if (tile == NULL) {
        return false;
    }
    belt = factory_belt_store_find(&simulation->belts, tile->occupying_entity);
    if (belt == NULL
        || factory_logistics_endpoint_can_accept(
            simulation,
            (FactoryLogisticsEndpoint){
                belt->entity_id, FACTORY_LOGISTICS_SLOT_MAIN
            },
            splitter->item
        ) != FACTORY_LOGISTICS_RESULT_OK) {
        return false;
    }
    *out_belt_id = belt->entity_id;
    return true;
}

static void add_splitter_intent(
    FactorySimulation *simulation,
    TransferIntent *intents,
    size_t *count,
    const FactorySplitter *splitter
)
{
    FactorySplitterOutput selected = splitter->next_output;
    FactoryEntityId belt_id = 0U;

    if (!splitter_output_is_available(
            simulation, splitter, selected, &belt_id)) {
        selected = selected == FACTORY_SPLITTER_OUTPUT_LEFT
            ? FACTORY_SPLITTER_OUTPUT_RIGHT
            : FACTORY_SPLITTER_OUTPUT_LEFT;
        if (!splitter_output_is_available(
                simulation, splitter, selected, &belt_id)) {
            return;
        }
    }
    intents[*count].source = (FactoryLogisticsEndpoint){
        splitter->entity_id,
        selected == FACTORY_SPLITTER_OUTPUT_LEFT
            ? FACTORY_LOGISTICS_SLOT_SPLITTER_LEFT_OUTPUT
            : FACTORY_LOGISTICS_SLOT_SPLITTER_RIGHT_OUTPUT
    };
    intents[*count].destination = (FactoryLogisticsEndpoint){
        belt_id, FACTORY_LOGISTICS_SLOT_MAIN
    };
    intents[*count].item = splitter->item;
    intents[*count].splitter_output = selected;
    intents[*count].wins = true;
    ++*count;
}

static void resolve_transfers(TransferIntent *intents, size_t count)
{
    size_t first;
    size_t second;

    for (first = 0U; first < count; ++first) {
        for (second = first + 1U; second < count; ++second) {
            if (factory_logistics_endpoint_equal(
                    intents[first].destination,
                    intents[second].destination)) {
                if (intents[first].source.entity_id
                    < intents[second].source.entity_id) {
                    intents[second].wins = false;
                } else {
                    intents[first].wins = false;
                }
            }
        }
    }
}

static void update_producer_transfers(FactorySimulation *simulation)
{
    size_t capacity = simulation->extractors.count
        + simulation->refineries.count
        + simulation->assemblers.count
        + simulation->splitters.count;
    TransferIntent *intents;
    size_t count = 0U;
    size_t index;

    if (capacity == 0U) {
        return;
    }
    intents = malloc(capacity * sizeof(*intents));
    if (intents == NULL) {
        return;
    }
    for (index = 0U; index < simulation->extractors.count; ++index) {
        const FactoryExtractor *source = &simulation->extractors.items[index];

        if (source->output_item != FACTORY_ITEM_NONE) {
            add_producer_intent(
                simulation, intents, &count, source->entity_id,
                source->x, source->y,
                source->output_direction, source->output_item
            );
        }
    }
    for (index = 0U; index < simulation->refineries.count; ++index) {
        const FactoryRefinery *source = &simulation->refineries.items[index];

        if (source->output_item != FACTORY_ITEM_NONE) {
            add_producer_intent(
                simulation, intents, &count, source->entity_id,
                source->x, source->y,
                source->output_direction, source->output_item
            );
        }
    }
    for (index = 0U; index < simulation->assemblers.count; ++index) {
        const FactoryAssembler *source = &simulation->assemblers.items[index];

        if (source->output_item != FACTORY_ITEM_NONE) {
            add_producer_intent(
                simulation, intents, &count, source->entity_id,
                source->x, source->y,
                source->output_direction, source->output_item
            );
        }
    }
    for (index = 0U; index < simulation->splitters.count; ++index) {
        const FactorySplitter *source = &simulation->splitters.items[index];

        if (source->item != FACTORY_ITEM_NONE) {
            add_splitter_intent(simulation, intents, &count, source);
        }
    }
    resolve_transfers(intents, count);
    for (index = 0U; index < count; ++index) {
        if (!intents[index].wins) {
            continue;
        }
        if (factory_logistics_endpoint_transfer(
                simulation,
                intents[index].source,
                intents[index].destination,
                intents[index].item) != FACTORY_LOGISTICS_RESULT_OK) {
            continue;
        }
        if (intents[index].source.slot
            == FACTORY_LOGISTICS_SLOT_SPLITTER_LEFT_OUTPUT
            || intents[index].source.slot
                == FACTORY_LOGISTICS_SLOT_SPLITTER_RIGHT_OUTPUT) {
            FactorySplitter *source = factory_splitter_store_find_mutable(
                &simulation->splitters, intents[index].source.entity_id
            );
            source->next_output =
                intents[index].splitter_output
                    == FACTORY_SPLITTER_OUTPUT_LEFT
                ? FACTORY_SPLITTER_OUTPUT_RIGHT
                : FACTORY_SPLITTER_OUTPUT_LEFT;
        }
    }
    free(intents);
}

static size_t plan_belt_transfers(
    FactorySimulation *simulation,
    TransferIntent *intents
)
{
    size_t belt_index;
    size_t intent_count = 0U;

    for (belt_index = 0U; belt_index < simulation->belts.count; ++belt_index) {
        const FactoryBelt *source = &simulation->belts.items[belt_index];
        const FactoryTile *tile;
        const FactoryBelt *destination_belt;
        const FactoryRefinery *destination_refinery;
        const FactoryAssembler *destination_assembler;
        const FactorySplitter *destination_splitter;
        const FactoryStorage *destination_storage;
        const FactoryPowerGenerator *destination_generator;
        FactoryLogisticsEndpoint destination = {0};
        int32_t target_x;
        int32_t target_y;
        int32_t input_x;
        int32_t input_y;

        if (source->item == FACTORY_ITEM_NONE
            || source->movement_progress < FACTORY_BELT_TRANSFER_TICKS) {
            continue;
        }
        adjacent_coordinate(
            source->x, source->y, source->direction, &target_x, &target_y
        );
        tile = factory_world_get_tile(simulation->world, target_x, target_y);
        if (tile == NULL) {
            continue;
        }
        destination_belt = factory_belt_store_find(
            &simulation->belts, tile->occupying_entity
        );
        destination_storage = factory_storage_store_find(
            &simulation->storages, tile->occupying_entity
        );
        destination_generator = factory_power_generator_store_find(
            &simulation->power_generators, tile->occupying_entity
        );
        destination_refinery = factory_refinery_store_find(
            &simulation->refineries, tile->occupying_entity
        );
        destination_assembler = factory_assembler_store_find(
            &simulation->assemblers, tile->occupying_entity
        );
        destination_splitter = factory_splitter_store_find(
            &simulation->splitters, tile->occupying_entity
        );
        if (destination_belt != NULL) {
            destination = (FactoryLogisticsEndpoint){
                destination_belt->entity_id, FACTORY_LOGISTICS_SLOT_MAIN
            };
        } else if (destination_storage != NULL) {
            destination = (FactoryLogisticsEndpoint){
                destination_storage->entity_id,
                FACTORY_LOGISTICS_SLOT_STORAGE_INPUT
            };
        } else if (destination_generator != NULL) {
            destination = (FactoryLogisticsEndpoint){
                destination_generator->entity_id,
                FACTORY_LOGISTICS_SLOT_BURNER_INPUT
            };
        } else if (destination_refinery != NULL) {
            adjacent_coordinate(
                destination_refinery->x,
                destination_refinery->y,
                destination_refinery->input_direction,
                &input_x,
                &input_y
            );
            if (input_x != source->x || input_y != source->y) {
                continue;
            }
            destination = (FactoryLogisticsEndpoint){
                destination_refinery->entity_id,
                FACTORY_LOGISTICS_SLOT_INPUT
            };
        } else if (destination_assembler != NULL) {
            for (size_t slot = 0U;
                slot < FACTORY_ASSEMBLER_MAX_INPUT_TYPES;
                ++slot) {
                FactoryLogisticsEndpoint candidate = {
                    destination_assembler->entity_id,
                    slot == 0U
                        ? FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0
                        : FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1
                };

                if (factory_logistics_endpoint_can_accept(
                        simulation, candidate, source->item)
                    == FACTORY_LOGISTICS_RESULT_OK) {
                    destination = candidate;
                    break;
                }
            }
            if (destination.entity_id == 0U) {
                continue;
            }
        } else if (destination_splitter != NULL) {
            adjacent_coordinate(
                destination_splitter->x,
                destination_splitter->y,
                opposite_direction(destination_splitter->facing),
                &input_x,
                &input_y
            );
            if (input_x != source->x || input_y != source->y) {
                continue;
            }
            destination = (FactoryLogisticsEndpoint){
                destination_splitter->entity_id,
                FACTORY_LOGISTICS_SLOT_SPLITTER_INPUT
            };
        } else {
            continue;
        }
        if (factory_logistics_endpoint_can_accept(
                simulation, destination, source->item)
            != FACTORY_LOGISTICS_RESULT_OK) {
            continue;
        }
        intents[intent_count].source = (FactoryLogisticsEndpoint){
            source->entity_id, FACTORY_LOGISTICS_SLOT_MAIN
        };
        intents[intent_count].destination = destination;
        intents[intent_count].item = source->item;
        intents[intent_count].wins = true;
        ++intent_count;
    }
    return intent_count;
}

static void commit_belt_transfers(
    FactorySimulation *simulation,
    const TransferIntent *intents,
    size_t count
)
{
    size_t index;

    for (index = 0U; index < count; ++index) {
        if (!intents[index].wins) {
            continue;
        }
        (void)factory_logistics_endpoint_transfer(
            simulation,
            intents[index].source,
            intents[index].destination,
            intents[index].item
        );
    }
}

static void update_belt_transfers(FactorySimulation *simulation)
{
    TransferIntent *intents;
    size_t count;

    if (simulation->belts.count == 0U) {
        return;
    }
    intents = malloc(simulation->belts.count * sizeof(*intents));
    if (intents == NULL) {
        return;
    }
    count = plan_belt_transfers(simulation, intents);
    resolve_transfers(intents, count);
    commit_belt_transfers(simulation, intents, count);
    free(intents);
}

typedef struct {
    FactoryEntityId inserter_id;
    FactoryLogisticsEndpoint endpoint;
    FactoryItemType item;
    bool wins;
} InserterIntent;

static bool coordinate_matches_direction(
    int32_t origin_x,
    int32_t origin_y,
    FactoryDirection direction,
    int32_t expected_x,
    int32_t expected_y
)
{
    int32_t x;
    int32_t y;

    adjacent_coordinate(origin_x, origin_y, direction, &x, &y);
    return x == expected_x && y == expected_y;
}

static bool splitter_can_output_to_inserter(
    const FactorySplitter *splitter,
    const FactoryInserter *inserter
)
{
    return coordinate_matches_direction(
            splitter->x,
            splitter->y,
            splitter_output_direction(
                splitter->facing, FACTORY_SPLITTER_OUTPUT_LEFT
            ),
            inserter->x,
            inserter->y
        )
        || coordinate_matches_direction(
            splitter->x,
            splitter->y,
            splitter_output_direction(
                splitter->facing, FACTORY_SPLITTER_OUTPUT_RIGHT
            ),
            inserter->x,
            inserter->y
        );
}

static bool inspect_inserter_source(
    FactorySimulation *simulation,
    const FactoryInserter *inserter,
    FactoryLogisticsEndpoint *out_endpoint,
    FactoryItemType *out_item
)
{
    const FactoryTile *tile = factory_world_get_tile(
        simulation->world, inserter->source_x, inserter->source_y
    );
    const FactoryBelt *belt;
    const FactorySplitter *splitter;
    const FactoryRefinery *refinery;
    const FactoryAssembler *assembler;
    const FactoryStorage *storage;

    if (tile == NULL || tile->occupying_entity == 0U) {
        return false;
    }
    belt = factory_belt_store_find(&simulation->belts, tile->occupying_entity);
    if (belt != NULL) {
        *out_endpoint = (FactoryLogisticsEndpoint){
            belt->entity_id, FACTORY_LOGISTICS_SLOT_MAIN
        };
        return factory_logistics_endpoint_peek(
            simulation, *out_endpoint, out_item
        ) == FACTORY_LOGISTICS_RESULT_OK;
    }
    splitter = factory_splitter_store_find(
        &simulation->splitters, tile->occupying_entity
    );
    if (splitter != NULL
        && splitter_can_output_to_inserter(splitter, inserter)) {
        *out_endpoint = (FactoryLogisticsEndpoint){
            splitter->entity_id,
            coordinate_matches_direction(
                splitter->x,
                splitter->y,
                splitter_output_direction(
                    splitter->facing, FACTORY_SPLITTER_OUTPUT_LEFT
                ),
                inserter->x,
                inserter->y
            )
                ? FACTORY_LOGISTICS_SLOT_SPLITTER_LEFT_OUTPUT
                : FACTORY_LOGISTICS_SLOT_SPLITTER_RIGHT_OUTPUT
        };
        return factory_logistics_endpoint_peek(
            simulation, *out_endpoint, out_item
        ) == FACTORY_LOGISTICS_RESULT_OK;
    }
    refinery = factory_refinery_store_find(
        &simulation->refineries, tile->occupying_entity
    );
    if (refinery != NULL
        && coordinate_matches_direction(
            refinery->x,
            refinery->y,
            refinery->output_direction,
            inserter->x,
            inserter->y
        )) {
        *out_endpoint = (FactoryLogisticsEndpoint){
            refinery->entity_id, FACTORY_LOGISTICS_SLOT_OUTPUT
        };
        return factory_logistics_endpoint_peek(
            simulation, *out_endpoint, out_item
        ) == FACTORY_LOGISTICS_RESULT_OK;
    }
    assembler = factory_assembler_store_find(
        &simulation->assemblers, tile->occupying_entity
    );
    if (assembler != NULL
        && coordinate_matches_direction(
            assembler->x,
            assembler->y,
            assembler->output_direction,
            inserter->x,
            inserter->y
        )) {
        *out_endpoint = (FactoryLogisticsEndpoint){
            assembler->entity_id, FACTORY_LOGISTICS_SLOT_OUTPUT
        };
        return factory_logistics_endpoint_peek(
            simulation, *out_endpoint, out_item
        ) == FACTORY_LOGISTICS_RESULT_OK;
    }
    storage = factory_storage_store_find(
        &simulation->storages, tile->occupying_entity
    );
    if (storage != NULL) {
        *out_endpoint = (FactoryLogisticsEndpoint){
            storage->entity_id, FACTORY_LOGISTICS_SLOT_STORAGE_OUTPUT
        };
        return factory_logistics_endpoint_peek(
            simulation, *out_endpoint, out_item
        ) == FACTORY_LOGISTICS_RESULT_OK;
    }
    return false;
}

static void resolve_inserter_intents(
    InserterIntent *intents,
    size_t count
)
{
    size_t first;
    size_t second;

    for (first = 0U; first < count; ++first) {
        for (second = first + 1U; second < count; ++second) {
            if (factory_logistics_endpoint_equal(
                    intents[first].endpoint, intents[second].endpoint)) {
                if (intents[first].inserter_id
                    < intents[second].inserter_id) {
                    intents[second].wins = false;
                } else {
                    intents[first].wins = false;
                }
            }
        }
    }
}

static void update_inserter_pickups(FactorySimulation *simulation)
{
    InserterIntent *intents;
    size_t count = 0U;
    size_t index;

    if (simulation->inserters.count == 0U) {
        return;
    }
    intents = malloc(simulation->inserters.count * sizeof(*intents));
    if (intents == NULL) {
        return;
    }
    for (index = 0U; index < simulation->inserters.count; ++index) {
        FactoryInserter *inserter = &simulation->inserters.items[index];

        if (!factory_power_is_entity_powered(
                simulation, inserter->entity_id)) {
            continue;
        }
        if (inserter->state == FACTORY_INSERTER_STATE_IDLE) {
            FactoryLogisticsEndpoint endpoint;
            FactoryItemType item;

            if (inspect_inserter_source(
                    simulation, inserter, &endpoint, &item)) {
                inserter->state = FACTORY_INSERTER_STATE_PICKING_UP;
                inserter->progress = 0U;
            }
        } else if (inserter->state
            == FACTORY_INSERTER_STATE_PICKING_UP) {
            ++inserter->progress;
            if (inserter->progress >= FACTORY_INSERTER_ACTION_TICKS) {
                InserterIntent *intent = &intents[count];

                if (!inspect_inserter_source(
                        simulation,
                        inserter,
                        &intent->endpoint,
                        &intent->item)) {
                    inserter->state = FACTORY_INSERTER_STATE_IDLE;
                    inserter->progress = 0U;
                    continue;
                }
                intent->inserter_id = inserter->entity_id;
                intent->wins = true;
                ++count;
            }
        }
    }
    resolve_inserter_intents(intents, count);
    for (index = 0U; index < count; ++index) {
        FactoryInserter *inserter = factory_inserter_store_find_mutable(
            &simulation->inserters, intents[index].inserter_id
        );

        if (!intents[index].wins) {
            inserter->state = FACTORY_INSERTER_STATE_IDLE;
            inserter->progress = 0U;
            continue;
        }
        if (factory_logistics_endpoint_transfer(
                simulation,
                intents[index].endpoint,
                (FactoryLogisticsEndpoint){
                    inserter->entity_id,
                    FACTORY_LOGISTICS_SLOT_INSERTER_HELD
                },
                intents[index].item) != FACTORY_LOGISTICS_RESULT_OK) {
            inserter->state = FACTORY_INSERTER_STATE_IDLE;
            inserter->progress = 0U;
            continue;
        }
        inserter->state = FACTORY_INSERTER_STATE_HOLDING;
        inserter->progress = 0U;
    }
    free(intents);
}

static bool inspect_inserter_destination(
    FactorySimulation *simulation,
    const FactoryInserter *inserter,
    FactoryLogisticsEndpoint *out_endpoint
)
{
    const FactoryTile *tile = factory_world_get_tile(
        simulation->world,
        inserter->destination_x,
        inserter->destination_y
    );
    const FactoryBelt *belt;
    const FactorySplitter *splitter;
    const FactoryStorage *storage;
    const FactoryRefinery *refinery;
    const FactoryAssembler *assembler;
    const FactoryPowerGenerator *generator;

    *out_endpoint = (FactoryLogisticsEndpoint){
        0U, FACTORY_LOGISTICS_SLOT_NONE
    };

    if (tile == NULL || tile->occupying_entity == 0U) {
        return false;
    }
    belt = factory_belt_store_find(&simulation->belts, tile->occupying_entity);
    if (belt != NULL) {
        *out_endpoint = (FactoryLogisticsEndpoint){
            belt->entity_id, FACTORY_LOGISTICS_SLOT_MAIN
        };
    }
    splitter = factory_splitter_store_find(
        &simulation->splitters, tile->occupying_entity
    );
    if (splitter != NULL
        && coordinate_matches_direction(
            splitter->x,
            splitter->y,
            opposite_direction(splitter->facing),
            inserter->x,
            inserter->y
        )) {
        *out_endpoint = (FactoryLogisticsEndpoint){
            splitter->entity_id, FACTORY_LOGISTICS_SLOT_SPLITTER_INPUT
        };
    } else {
        storage = factory_storage_store_find(
            &simulation->storages, tile->occupying_entity
        );
        if (storage != NULL) {
            *out_endpoint = (FactoryLogisticsEndpoint){
                storage->entity_id, FACTORY_LOGISTICS_SLOT_STORAGE_INPUT
            };
        }
    }
    refinery = factory_refinery_store_find(
        &simulation->refineries, tile->occupying_entity
    );
    if (refinery != NULL
        && coordinate_matches_direction(
            refinery->x,
            refinery->y,
            refinery->input_direction,
            inserter->x,
            inserter->y
        )) {
        *out_endpoint = (FactoryLogisticsEndpoint){
            refinery->entity_id, FACTORY_LOGISTICS_SLOT_INPUT
        };
    }
    assembler = factory_assembler_store_find(
        &simulation->assemblers, tile->occupying_entity
    );
    if (assembler != NULL) {
        for (size_t slot = 0U;
            slot < FACTORY_ASSEMBLER_MAX_INPUT_TYPES;
            ++slot) {
            FactoryLogisticsEndpoint candidate = {
                assembler->entity_id,
                slot == 0U
                    ? FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0
                    : FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1
            };

            if (factory_logistics_endpoint_can_accept(
                    simulation, candidate, inserter->held_item)
                == FACTORY_LOGISTICS_RESULT_OK) {
                *out_endpoint = candidate;
                break;
            }
        }
    }
    generator = factory_power_generator_store_find(
        &simulation->power_generators, tile->occupying_entity
    );
    if (generator != NULL) {
        *out_endpoint = (FactoryLogisticsEndpoint){
            generator->entity_id, FACTORY_LOGISTICS_SLOT_BURNER_INPUT
        };
    }
    return out_endpoint->entity_id != 0U
        && factory_logistics_endpoint_can_accept(
            simulation, *out_endpoint, inserter->held_item
        ) == FACTORY_LOGISTICS_RESULT_OK;
}

static void update_inserter_drops(FactorySimulation *simulation)
{
    InserterIntent *intents;
    size_t count = 0U;
    size_t index;

    if (simulation->inserters.count == 0U) {
        return;
    }
    intents = malloc(simulation->inserters.count * sizeof(*intents));
    if (intents == NULL) {
        return;
    }
    for (index = 0U; index < simulation->inserters.count; ++index) {
        FactoryInserter *inserter = &simulation->inserters.items[index];

        if (!factory_power_is_entity_powered(
                simulation, inserter->entity_id)) {
            continue;
        }
        if (inserter->state == FACTORY_INSERTER_STATE_HOLDING) {
            inserter->state = FACTORY_INSERTER_STATE_DROPPING;
            inserter->progress = 0U;
        } else if (inserter->state == FACTORY_INSERTER_STATE_DROPPING) {
            InserterIntent *intent;

            if (inserter->progress < FACTORY_INSERTER_ACTION_TICKS) {
                ++inserter->progress;
            }
            if (inserter->progress < FACTORY_INSERTER_ACTION_TICKS) {
                continue;
            }
            intent = &intents[count];
            if (!inspect_inserter_destination(
                    simulation,
                    inserter,
                    &intent->endpoint)) {
                continue;
            }
            intent->inserter_id = inserter->entity_id;
            intent->item = inserter->held_item;
            intent->wins = true;
            ++count;
        }
    }
    resolve_inserter_intents(intents, count);
    for (index = 0U; index < count; ++index) {
        FactoryInserter *inserter;

        if (!intents[index].wins) {
            continue;
        }
        inserter = factory_inserter_store_find_mutable(
            &simulation->inserters, intents[index].inserter_id
        );
        if (factory_logistics_endpoint_transfer(
                simulation,
                (FactoryLogisticsEndpoint){
                    inserter->entity_id,
                    FACTORY_LOGISTICS_SLOT_INSERTER_HELD
                },
                intents[index].endpoint,
                intents[index].item) != FACTORY_LOGISTICS_RESULT_OK) {
            continue;
        }
        inserter->state = FACTORY_INSERTER_STATE_IDLE;
        inserter->progress = 0U;
    }
    free(intents);
}

static void update_inserters(FactorySimulation *simulation)
{
    update_inserter_drops(simulation);
    update_inserter_pickups(simulation);
}

FactoryResult factory_simulation_tick(FactorySimulation *simulation)
{
    size_t possible_entities;
    size_t event_limit;
    if (simulation == NULL) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    if (!factory_simulation_clock_can_advance(&simulation->clock))
        return FACTORY_RESULT_POWER_OVERFLOW;
    if (simulation->entities->count
            > SIZE_MAX - simulation->command_count) {
        return FACTORY_RESULT_POWER_OVERFLOW;
    }
    possible_entities =
        simulation->entities->count + simulation->command_count;
    if (possible_entities > (SIZE_MAX - 1U) / 8U) {
        return FACTORY_RESULT_POWER_OVERFLOW;
    }
    event_limit = possible_entities * 8U + 1U;
    if (!factory_event_batch_reserve(&simulation->events, event_limit)) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    simulation->events.count = 0U;
    simulation->events.recording = true;
    apply_commands(simulation);
    if (simulation->clock.time_of_day == FACTORY_CLOCK_SUNRISE)
        factory_simulation_emit_event(simulation, (FactoryEvent){
            .type = FACTORY_EVENT_SUNRISE});
    if (simulation->clock.time_of_day == FACTORY_CLOCK_SUNSET)
        factory_simulation_emit_event(simulation, (FactoryEvent){
            .type = FACTORY_EVENT_SUNSET});
    (void)factory_fluid_network_rebuild(simulation, true);
    (void)factory_heat_network_rebuild(simulation, true);
    factory_fluid_network_transfer(simulation);
    factory_burner_store_begin_tick(&simulation->burners, simulation);
    factory_fluid_machines_update(simulation);
    factory_steam_engine_begin_tick(simulation);
    factory_solar_generator_begin_tick(simulation);
    factory_accumulator_begin_tick(simulation);
    (void)factory_power_rebuild(simulation, true);
    factory_power_consume_generation(simulation);
    factory_burner_store_finish_tick(&simulation->burners, simulation);
    factory_reactor_store_update(&simulation->reactors, simulation);
    factory_heat_exchangers_update(simulation);
    factory_extractor_store_update(
        &simulation->extractors, simulation->world, simulation
    );
    update_producer_transfers(simulation);
    factory_belt_store_advance(&simulation->belts);
    update_belt_transfers(simulation);
    factory_refinery_store_update(&simulation->refineries, simulation);
    factory_assembler_store_update(&simulation->assemblers, simulation);
    factory_storage_store_update(&simulation->storages);
    update_inserters(simulation);
    simulation->events.recording = false;
    factory_simulation_clock_advance(&simulation->clock);
    return FACTORY_RESULT_OK;
}

uint64_t factory_simulation_get_tick(const FactorySimulation *simulation)
{
    return simulation == NULL ? 0U : simulation->clock.tick;
}

const FactoryWorld *factory_simulation_get_world(
    const FactorySimulation *simulation
)
{
    return simulation == NULL ? NULL : simulation->world;
}

size_t factory_simulation_get_pending_command_count(
    const FactorySimulation *simulation
)
{
    return simulation == NULL ? 0U : simulation->command_count;
}

size_t factory_simulation_get_command_result_count(
    const FactorySimulation *simulation
)
{
    return simulation == NULL ? 0U : simulation->result_count;
}

const FactoryCommandResult *factory_simulation_get_command_result(
    const FactorySimulation *simulation,
    size_t index
)
{
    if (simulation == NULL || index >= simulation->result_count) {
        return NULL;
    }
    return &simulation->results[index];
}

bool factory_simulation_entity_is_valid(
    const FactorySimulation *simulation,
    FactoryEntityId id
)
{
    return simulation != NULL
        && factory_entity_is_valid(simulation->entities, id);
}

size_t factory_simulation_get_entity_count(
    const FactorySimulation *simulation
)
{
    return simulation == NULL
        ? 0U
        : factory_entity_get_count(simulation->entities);
}

bool factory_simulation_is_extractor(
    const FactorySimulation *simulation,
    FactoryEntityId id
)
{
    return simulation != NULL
        && factory_extractor_store_find(&simulation->extractors, id) != NULL;
}

bool factory_simulation_get_extractor(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryExtractor *out_extractor
)
{
    const FactoryExtractor *extractor;

    if (simulation == NULL || out_extractor == NULL) {
        return false;
    }
    extractor = factory_extractor_store_find(&simulation->extractors, id);
    if (extractor == NULL) {
        return false;
    }
    *out_extractor = *extractor;
    return true;
}

bool factory_simulation_is_belt(
    const FactorySimulation *simulation,
    FactoryEntityId id
)
{
    return simulation != NULL
        && factory_belt_store_find(&simulation->belts, id) != NULL;
}

bool factory_simulation_get_belt(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryBelt *out_belt
)
{
    const FactoryBelt *belt;

    if (simulation == NULL || out_belt == NULL) {
        return false;
    }
    belt = factory_belt_store_find(&simulation->belts, id);
    if (belt == NULL) {
        return false;
    }
    *out_belt = *belt;
    return true;
}

bool factory_simulation_is_storage(
    const FactorySimulation *simulation,
    FactoryEntityId id
)
{
    return simulation != NULL
        && factory_storage_store_find(&simulation->storages, id) != NULL;
}

bool factory_simulation_get_storage(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryStorage *out_storage
)
{
    const FactoryStorage *storage;

    if (simulation == NULL || out_storage == NULL) {
        return false;
    }
    storage = factory_storage_store_find(&simulation->storages, id);
    if (storage == NULL) {
        return false;
    }
    *out_storage = *storage;
    return true;
}

bool factory_simulation_is_refinery(
    const FactorySimulation *simulation,
    FactoryEntityId id
)
{
    return simulation != NULL
        && factory_refinery_store_find(&simulation->refineries, id) != NULL;
}

bool factory_simulation_get_refinery(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryRefinery *out_refinery
)
{
    const FactoryRefinery *refinery;

    if (simulation == NULL || out_refinery == NULL) {
        return false;
    }
    refinery = factory_refinery_store_find(&simulation->refineries, id);
    if (refinery == NULL) {
        return false;
    }
    *out_refinery = *refinery;
    return true;
}

bool factory_simulation_is_assembler(
    const FactorySimulation *simulation,
    FactoryEntityId id
)
{
    return simulation != NULL
        && factory_assembler_store_find(&simulation->assemblers, id) != NULL;
}

bool factory_simulation_get_assembler(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryAssembler *out_assembler
)
{
    const FactoryAssembler *assembler;

    if (simulation == NULL || out_assembler == NULL) {
        return false;
    }
    assembler = factory_assembler_store_find(&simulation->assemblers, id);
    if (assembler == NULL) {
        return false;
    }
    *out_assembler = *assembler;
    return true;
}

bool factory_simulation_is_splitter(
    const FactorySimulation *simulation,
    FactoryEntityId id
)
{
    return simulation != NULL
        && factory_splitter_store_find(&simulation->splitters, id) != NULL;
}

bool factory_simulation_get_splitter(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactorySplitter *out_splitter
)
{
    const FactorySplitter *splitter;

    if (simulation == NULL || out_splitter == NULL) {
        return false;
    }
    splitter = factory_splitter_store_find(&simulation->splitters, id);
    if (splitter == NULL) {
        return false;
    }
    *out_splitter = *splitter;
    return true;
}

bool factory_simulation_is_inserter(
    const FactorySimulation *simulation,
    FactoryEntityId id
)
{
    return simulation != NULL
        && factory_inserter_store_find(&simulation->inserters, id) != NULL;
}

bool factory_simulation_get_inserter(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryInserter *out_inserter
)
{
    const FactoryInserter *inserter;

    if (simulation == NULL || out_inserter == NULL) {
        return false;
    }
    inserter = factory_inserter_store_find(&simulation->inserters, id);
    if (inserter == NULL) {
        return false;
    }
    *out_inserter = *inserter;
    return true;
}
