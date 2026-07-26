#include "presentation_internal.h"

#include "assembler_recipe_internal.h"
#include "simulation_internal.h"

#include <stdlib.h>

struct FactoryPresentationSnapshot {
    uint64_t tick;
    uint64_t day;
    uint32_t time_of_day;
    FactoryPresentationEntity *entities;
    size_t entity_count;
    FactoryPresentationResource *resources;
    size_t resource_count;
    FactoryPresentationPowerEdge *power_edges;
    size_t power_edge_count;
};

static size_t allocation_successes_before_failure = SIZE_MAX;

void factory_presentation_test_fail_allocations_after(size_t successes)
{
    allocation_successes_before_failure = successes;
}

static void *presentation_calloc(size_t count, size_t width)
{
    if (count == 0U) return NULL;
    if (allocation_successes_before_failure != SIZE_MAX) {
        if (allocation_successes_before_failure == 0U) return NULL;
        --allocation_successes_before_failure;
    }
    return calloc(count, width);
}

static void release_contents(FactoryPresentationSnapshot *snapshot)
{
    free(snapshot->power_edges);
    free(snapshot->resources);
    free(snapshot->entities);
    *snapshot = (FactoryPresentationSnapshot){0};
}

FactoryPresentationSnapshot *factory_presentation_snapshot_create(void)
{
    return calloc(1U, sizeof(FactoryPresentationSnapshot));
}

void factory_presentation_snapshot_destroy(
    FactoryPresentationSnapshot *snapshot
)
{
    if (snapshot == NULL) return;
    release_contents(snapshot);
    free(snapshot);
}

void factory_presentation_snapshot_clear(
    FactoryPresentationSnapshot *snapshot
)
{
    if (snapshot != NULL) release_contents(snapshot);
}

static FactoryPresentationMachineStatus extractor_status(
    const FactorySimulation *simulation,
    const FactoryExtractor *extractor,
    bool powered,
    bool *can_progress
)
{
    const FactoryTile *tile = factory_world_get_tile(
        simulation->world, extractor->x, extractor->y
    );
    *can_progress = powered && extractor->output_amount == 0U
        && tile != NULL && tile->resource == extractor->resource_type
        && tile->resource_amount != 0U;
    if (!powered) return FACTORY_PRESENTATION_MACHINE_STATUS_UNPOWERED;
    if (extractor->output_amount != 0U)
        return FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_OUTPUT;
    if (tile == NULL || tile->resource != extractor->resource_type
        || tile->resource_amount == 0U)
        return FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_INPUT;
    return FACTORY_PRESENTATION_MACHINE_STATUS_WORKING;
}

static FactoryPresentationMachineStatus refinery_status(
    const FactoryRefinery *refinery, const FactoryRecipe *recipe, bool powered
)
{
    if (!powered) return FACTORY_PRESENTATION_MACHINE_STATUS_UNPOWERED;
    if (refinery->output_amount != 0U)
        return FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_OUTPUT;
    if (refinery->processing)
        return FACTORY_PRESENTATION_MACHINE_STATUS_WORKING;
    if (recipe == NULL) return FACTORY_PRESENTATION_MACHINE_STATUS_IDLE;
    if (refinery->input_item != recipe->input_item
        || refinery->input_amount != recipe->input_amount)
        return FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_INPUT;
    return FACTORY_PRESENTATION_MACHINE_STATUS_WORKING;
}

static bool assembler_inputs_ready(
    const FactoryAssembler *assembler,
    const FactoryAssemblerRecipe *recipe
)
{
    size_t i;
    for (i = 0U; i < FACTORY_ASSEMBLER_MAX_INPUT_TYPES; ++i)
        if (assembler->input_slots[i].count != recipe->input_amounts[i])
            return false;
    return true;
}

static FactoryPresentationMachineStatus assembler_status(
    const FactoryAssembler *assembler,
    const FactoryAssemblerRecipe *recipe,
    bool powered
)
{
    if (!powered) return FACTORY_PRESENTATION_MACHINE_STATUS_UNPOWERED;
    if (assembler->output_amount != 0U)
        return FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_OUTPUT;
    if (assembler->processing)
        return FACTORY_PRESENTATION_MACHINE_STATUS_WORKING;
    if (recipe == NULL) return FACTORY_PRESENTATION_MACHINE_STATUS_IDLE;
    if (!assembler_inputs_ready(assembler, recipe))
        return FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_INPUT;
    return FACTORY_PRESENTATION_MACHINE_STATUS_WORKING;
}

static bool power_for(
    const FactorySimulation *simulation,
    FactoryEntityId id
)
{
    FactoryPowerConsumerInspection consumer;
    return factory_simulation_get_power_consumer(
        simulation, id, &consumer
    ) == FACTORY_RESULT_OK && consumer.powered;
}

static FactoryPowerTotal network_allocated(
    const FactorySimulation *simulation, FactoryPowerNetworkId id
)
{
    size_t i;
    FactoryPowerNetworkInspection network;
    for (i = 0U;
        i < factory_simulation_get_power_network_count(simulation); ++i)
        if (factory_simulation_get_power_network(
                simulation, i, &network) == FACTORY_RESULT_OK
            && network.network_id == id) return network.allocated_power;
    return 0U;
}

static FactoryResult populate_entity(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryPresentationEntity *out
)
{
    const FactoryExtractor *extractor =
        factory_extractor_store_find(&simulation->extractors, id);
    const FactoryBelt *belt = factory_belt_store_find(&simulation->belts, id);
    const FactoryStorage *storage =
        factory_storage_store_find(&simulation->storages, id);
    const FactoryRefinery *refinery =
        factory_refinery_store_find(&simulation->refineries, id);
    const FactoryAssembler *assembler =
        factory_assembler_store_find(&simulation->assemblers, id);
    const FactorySplitter *splitter =
        factory_splitter_store_find(&simulation->splitters, id);
    const FactoryInserter *inserter =
        factory_inserter_store_find(&simulation->inserters, id);
    const FactoryFluidStorage *fluid_storage =
        factory_fluid_storage_store_find(&simulation->fluid_storages, id);
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
        factory_heat_conductor_store_find(&simulation->heat_conductors,id);
    const FactoryHeatExchanger *heat_exchanger =
        factory_heat_exchanger_store_find(&simulation->heat_exchangers,id);
    FactoryPipeInspection pipe;
    FactoryPowerPoleInspection pole;
    FactoryPowerGeneratorInspection generator;

    *out = (FactoryPresentationEntity){
        .entity_id = id,
        .direction = FACTORY_PRESENTATION_DIRECTION_NONE
    };
    if (extractor != NULL) {
        bool can_progress;
        out->entity_type = FACTORY_ENTITY_TYPE_EXTRACTOR;
        out->x = extractor->x; out->y = extractor->y;
        out->direction = extractor->output_direction;
        out->powered = power_for(simulation, id);
        out->status = extractor_status(
            simulation, extractor, out->powered, &can_progress
        );
        out->data.extractor = (FactoryPresentationExtractor){
            extractor->resource_type, extractor->produced_item,
            extractor->production_progress,
            FACTORY_EXTRACTOR_PRODUCTION_TICKS,
            extractor->output_item, extractor->output_amount, can_progress
        };
    } else if (belt != NULL) {
        out->entity_type = FACTORY_ENTITY_TYPE_BELT;
        out->x = belt->x; out->y = belt->y;
        out->direction = belt->direction;
        out->data.belt = (FactoryPresentationBelt){
            belt->item, belt->item == FACTORY_ITEM_NONE ? 0U : 1U,
            belt->movement_progress, FACTORY_BELT_TRANSFER_TICKS
        };
    } else if (refinery != NULL) {
        const FactoryRecipe *recipe = factory_recipe_get(refinery->recipe_id);
        out->entity_type = FACTORY_ENTITY_TYPE_REFINERY;
        out->x = refinery->x; out->y = refinery->y;
        out->direction = refinery->output_direction;
        out->powered = power_for(simulation, id);
        out->status = refinery_status(refinery, recipe, out->powered);
        out->data.refinery = (FactoryPresentationRefinery){
            refinery->recipe_id, refinery->input_item,
            refinery->input_amount, refinery->output_item,
            refinery->output_amount, refinery->processing_progress,
            recipe == NULL ? 0U : recipe->processing_ticks,
            refinery->processing
        };
    } else if (assembler != NULL) {
        FactoryAssemblerRecipe recipe_value;
        const FactoryAssemblerRecipe *recipe = NULL;
        if (factory_assembler_recipe_get(
                assembler->recipe_id, &recipe_value)) recipe = &recipe_value;
        out->entity_type = FACTORY_ENTITY_TYPE_ASSEMBLER;
        out->x = assembler->x; out->y = assembler->y;
        out->direction = assembler->output_direction;
        out->powered = power_for(simulation, id);
        out->status = assembler_status(assembler, recipe, out->powered);
        out->data.assembler.recipe_id = assembler->recipe_id;
        out->data.assembler.input_slots[0] = assembler->input_slots[0];
        out->data.assembler.input_slots[1] = assembler->input_slots[1];
        out->data.assembler.output_item = assembler->output_item;
        out->data.assembler.output_quantity = assembler->output_amount;
        out->data.assembler.progress = assembler->processing_progress;
        out->data.assembler.duration = assembler->processing_duration;
        out->data.assembler.processing = assembler->processing;
    } else if (storage != NULL) {
        out->entity_type = FACTORY_ENTITY_TYPE_STORAGE;
        out->x = storage->x; out->y = storage->y;
        out->data.storage.item_quantities[0] = storage->iron_ore_amount;
        out->data.storage.item_quantities[1] = storage->iron_plate_amount;
        out->data.storage.item_quantities[2] = storage->copper_ore_amount;
        out->data.storage.item_quantities[3] = storage->copper_plate_amount;
        out->data.storage.item_quantities[4] =
            storage->electronic_component_amount;
        out->data.storage.item_quantities[5] = storage->iron_gear_amount;
        out->data.storage.item_quantities[6] = storage->copper_wire_amount;
        out->data.storage.item_quantities[7] =
            storage->biomass_pellet_amount;
        out->data.storage.total_capacity = storage->total_capacity;
        out->data.storage.configured_output_item =
            storage->configured_output_item;
        out->data.storage.output_item = storage->output_item;
        out->data.storage.output_quantity =
            storage->output_occupied ? 1U : 0U;
    } else if (splitter != NULL) {
        out->entity_type = FACTORY_ENTITY_TYPE_SPLITTER;
        out->x = splitter->x; out->y = splitter->y;
        out->direction = splitter->facing;
        out->data.splitter = (FactoryPresentationSplitter){
            splitter->item,
            splitter->item == FACTORY_ITEM_NONE ? 0U : 1U,
            splitter->next_output
        };
    } else if (inserter != NULL) {
        out->entity_type = FACTORY_ENTITY_TYPE_INSERTER;
        out->x = inserter->x; out->y = inserter->y;
        out->direction = inserter->facing;
        out->powered = power_for(simulation, id);
        out->status = !out->powered
            ? FACTORY_PRESENTATION_MACHINE_STATUS_UNPOWERED
            : inserter->state == FACTORY_INSERTER_STATE_IDLE
                ? FACTORY_PRESENTATION_MACHINE_STATUS_IDLE
                : FACTORY_PRESENTATION_MACHINE_STATUS_WORKING;
        out->data.inserter = (FactoryPresentationInserter){
            inserter->held_item, inserter->held_amount,
            inserter->state, inserter->progress,
            inserter->source_x, inserter->source_y,
            inserter->destination_x, inserter->destination_y
        };
    } else if (water_extractor != NULL) {
        FactoryWaterExtractorInspection machine;
        out->entity_type = FACTORY_ENTITY_TYPE_WATER_EXTRACTOR;
        out->x = water_extractor->x; out->y = water_extractor->y;
        if (factory_simulation_get_water_extractor(
                simulation, id, &machine) != FACTORY_RESULT_OK)
            return FACTORY_RESULT_ENTITY_NOT_FOUND;
        out->status = machine.stored_water
                + FACTORY_WATER_EXTRACTOR_OUTPUT_QUANTITY
                > machine.capacity
            ? FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_OUTPUT
            : FACTORY_PRESENTATION_MACHINE_STATUS_WORKING;
        out->data.water_extractor = (FactoryPresentationWaterExtractor){
            machine.stored_water, machine.capacity,
            machine.progress, machine.duration, FACTORY_FLUID_NETWORK_NONE};
        for (size_t i = 0U; i < simulation->fluid_networks.port_count; ++i)
            if (simulation->fluid_networks.ports[i].owner_entity_id == id
                && simulation->fluid_networks.ports[i].storage_slot
                    == FACTORY_FLUID_STORAGE_DEFAULT)
                out->data.water_extractor.output_network_id =
                    simulation->fluid_networks.ports[i].network_id;
    } else if (boiler != NULL) {
        FactoryBoilerInspection machine;
        FactoryBurnerInspection burner;
        out->entity_type = FACTORY_ENTITY_TYPE_BOILER;
        out->x = boiler->x; out->y = boiler->y;
        if (factory_simulation_get_boiler(simulation, id, &machine)
                != FACTORY_RESULT_OK
            || factory_simulation_get_burner(simulation, id, &burner)
                != FACTORY_RESULT_OK)
            return FACTORY_RESULT_ENTITY_NOT_FOUND;
        out->status = machine.conversion_active
            ? FACTORY_PRESENTATION_MACHINE_STATUS_WORKING
            : FACTORY_PRESENTATION_MACHINE_STATUS_IDLE;
        out->data.boiler = (FactoryPresentationBoiler){
            machine.stored_water, machine.water_capacity,
            machine.stored_steam, machine.steam_capacity,
            FACTORY_FLUID_NETWORK_NONE, FACTORY_FLUID_NETWORK_NONE,
            {
                burner.inventory_item, burner.inventory_quantity,
                burner.current_fuel_item, burner.remaining_burn_ticks,
                burner.total_burn_duration_ticks,
                burner.unreleased_fuel_energy,
                burner.released_energy, burner.active
            },
            machine.conversion_active
        };
        for (size_t i = 0U; i < simulation->fluid_networks.port_count; ++i)
            if (simulation->fluid_networks.ports[i].owner_entity_id == id) {
                if (simulation->fluid_networks.ports[i].storage_slot
                    == FACTORY_FLUID_STORAGE_BOILER_INPUT)
                    out->data.boiler.input_network_id =
                        simulation->fluid_networks.ports[i].network_id;
                else if (simulation->fluid_networks.ports[i].storage_slot
                    == FACTORY_FLUID_STORAGE_BOILER_OUTPUT)
                    out->data.boiler.output_network_id =
                        simulation->fluid_networks.ports[i].network_id;
            }
    } else if (accumulator != NULL) {
        FactoryAccumulatorInspection inspection;
        out->entity_type = FACTORY_ENTITY_TYPE_ACCUMULATOR;
        out->x = accumulator->x; out->y = accumulator->y;
        if (factory_simulation_get_accumulator(
                simulation, id, &inspection) != FACTORY_RESULT_OK)
            return FACTORY_RESULT_ENTITY_NOT_FOUND;
        out->status = inspection.activity == FACTORY_ACCUMULATOR_IDLE
            ? FACTORY_PRESENTATION_MACHINE_STATUS_IDLE
            : FACTORY_PRESENTATION_MACHINE_STATUS_WORKING;
        out->data.accumulator = (FactoryPresentationAccumulator){
            inspection.stored_energy, inspection.capacity,
            inspection.maximum_charge_rate,
            inspection.maximum_discharge_rate,
            inspection.charged_last_tick,
            inspection.discharged_last_tick,
            inspection.activity, inspection.network_id,
            inspection.connected};
    } else if (reactor != NULL) {
        FactoryReactorInspection inspection;
        FactoryHeatPortInspection port={0};
        out->entity_type = FACTORY_ENTITY_TYPE_REACTOR_CORE;
        out->x = reactor->x; out->y = reactor->y;
        if (factory_simulation_get_reactor(
                simulation, id, &inspection) != FACTORY_RESULT_OK)
            return FACTORY_RESULT_ENTITY_NOT_FOUND;
        out->status =
            inspection.activity == FACTORY_REACTOR_GENERATING
                ? FACTORY_PRESENTATION_MACHINE_STATUS_WORKING
                : inspection.activity == FACTORY_REACTOR_BLOCKED_HEAT_FULL
                    ? FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_OUTPUT
                    : FACTORY_PRESENTATION_MACHINE_STATUS_IDLE;
        out->data.reactor = (FactoryPresentationReactor){
            inspection.stored_heat, inspection.heat_capacity,
            inspection.inventory_fuel_id, inspection.inventory_quantity,
            inspection.active_fuel_id, inspection.remaining_burn_ticks,
            inspection.remaining_heat_yield,
            inspection.generated_last_tick, inspection.activity,
            FACTORY_HEAT_NETWORK_NONE,false};
        if (factory_simulation_get_heat_port(simulation,id,
                FACTORY_HEAT_PORT_REACTOR_OUTPUT,&port)==FACTORY_RESULT_OK) {
            out->data.reactor.heat_network_id=port.network_id;
            out->data.reactor.heat_connected=port.connected;
        }
    } else if (heat_conductor != NULL) {
        FactoryHeatConductorInspection inspection;
        out->entity_type=FACTORY_ENTITY_TYPE_HEAT_CONDUCTOR;
        out->x=heat_conductor->x; out->y=heat_conductor->y;
        if (factory_simulation_get_heat_conductor(simulation,id,&inspection)
                !=FACTORY_RESULT_OK) return FACTORY_RESULT_ENTITY_NOT_FOUND;
        out->data.heat_conductor=(FactoryPresentationHeatConductor){
            inspection.connection_mask,inspection.network_id,
            inspection.connected};
    } else if (heat_exchanger != NULL) {
        FactoryHeatExchangerInspection inspection;
        out->entity_type=FACTORY_ENTITY_TYPE_HEAT_EXCHANGER;
        out->x=heat_exchanger->x; out->y=heat_exchanger->y;
        if (factory_simulation_get_heat_exchanger(simulation,id,&inspection)
                !=FACTORY_RESULT_OK) return FACTORY_RESULT_ENTITY_NOT_FOUND;
        out->status=inspection.activity==FACTORY_HEAT_EXCHANGER_WORKING
            ? FACTORY_PRESENTATION_MACHINE_STATUS_WORKING
            : inspection.activity==FACTORY_HEAT_EXCHANGER_BLOCKED_STEAM_FULL
                ? FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_OUTPUT
                : FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_INPUT;
        out->data.heat_exchanger=(FactoryPresentationHeatExchanger){
            inspection.heat_network_id,inspection.water_network_id,
            inspection.steam_network_id,inspection.stored_water,
            inspection.water_capacity,inspection.stored_steam,
            inspection.steam_capacity,inspection.consumed_heat_last_tick,
            inspection.consumed_water_last_tick,
            inspection.produced_steam_last_tick,inspection.activity};
    } else if (solar_generator != NULL) {
        FactorySolarGeneratorInspection machine;
        FactoryPowerGeneratorInspection power_generator;
        out->entity_type = FACTORY_ENTITY_TYPE_SOLAR_GENERATOR;
        out->x = solar_generator->x; out->y = solar_generator->y;
        if (factory_simulation_get_solar_generator(
                simulation, id, &machine) != FACTORY_RESULT_OK
            || factory_simulation_get_power_generator(
                simulation, id, &power_generator) != FACTORY_RESULT_OK)
            return FACTORY_RESULT_ENTITY_NOT_FOUND;
        out->status = machine.active
            ? FACTORY_PRESENTATION_MACHINE_STATUS_WORKING
            : FACTORY_PRESENTATION_MACHINE_STATUS_IDLE;
        out->data.solar_generator = (FactoryPresentationSolarGenerator){
            power_generator.network_id, machine.maximum_output,
            machine.available_output, machine.generated_last_tick,
            machine.active};
    } else if (steam_engine != NULL) {
        FactorySteamEngineInspection machine;
        FactoryPowerGeneratorInspection generator;
        out->entity_type = FACTORY_ENTITY_TYPE_STEAM_ENGINE;
        out->x = steam_engine->x; out->y = steam_engine->y;
        if (factory_simulation_get_steam_engine(simulation, id, &machine)
                != FACTORY_RESULT_OK
            || factory_simulation_get_power_generator(
                simulation, id, &generator) != FACTORY_RESULT_OK)
            return FACTORY_RESULT_ENTITY_NOT_FOUND;
        out->status = machine.active
            ? FACTORY_PRESENTATION_MACHINE_STATUS_WORKING
            : machine.stored_steam == 0U
                ? FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_INPUT
                : FACTORY_PRESENTATION_MACHINE_STATUS_IDLE;
        out->data.steam_engine = (FactoryPresentationSteamEngine){
            machine.stored_steam, machine.steam_capacity,
            FACTORY_FLUID_NETWORK_NONE, generator.network_id,
            machine.maximum_output_per_tick,
            factory_power_source_available_generation(simulation, id),
            machine.generated_last_tick, machine.active};
        for (size_t i = 0U; i < simulation->fluid_networks.port_count; ++i)
            if (simulation->fluid_networks.ports[i].owner_entity_id == id
                && simulation->fluid_networks.ports[i].storage_slot
                    == FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT)
                out->data.steam_engine.steam_network_id =
                    simulation->fluid_networks.ports[i].network_id;
    } else if (fluid_storage != NULL) {
        out->entity_type = FACTORY_ENTITY_TYPE_FLUID_TANK;
        out->x = fluid_storage->x;
        out->y = fluid_storage->y;
        out->data.fluid_storage = (FactoryPresentationFluidStorage){
            fluid_storage->fluid_type,
            fluid_storage->quantity,
            fluid_storage->capacity,
            FACTORY_FLUID_NETWORK_NONE
        };
        for (size_t i = 0U;
            i < simulation->fluid_networks.port_count; ++i)
            if (simulation->fluid_networks.ports[i].owner_entity_id == id)
                out->data.fluid_storage.network_id =
                    simulation->fluid_networks.ports[i].network_id;
    } else if (factory_simulation_get_pipe(
            simulation, id, &pipe) == FACTORY_RESULT_OK) {
        out->entity_type = FACTORY_ENTITY_TYPE_PIPE;
        out->x = pipe.x; out->y = pipe.y;
        out->data.pipe = (FactoryPresentationPipe){
            pipe.connection_mask, pipe.network_id};
    } else if (factory_simulation_get_power_pole(
            simulation, id, &pole) == FACTORY_RESULT_OK) {
        out->entity_type = FACTORY_ENTITY_TYPE_POWER_POLE;
        out->x = pole.x; out->y = pole.y;
        out->data.power_pole = (FactoryPresentationPowerPole){
            pole.network_id, pole.machine_radius, pole.wire_radius,
            pole.connected_pole_count
        };
    } else if (factory_simulation_get_power_generator(
            simulation, id, &generator) == FACTORY_RESULT_OK) {
        FactoryBurnerInspection burner;
        if (factory_simulation_get_burner(
                simulation, id, &burner) != FACTORY_RESULT_OK)
            return FACTORY_RESULT_ENTITY_NOT_FOUND;
        out->entity_type = FACTORY_ENTITY_TYPE_POWER_GENERATOR;
        out->x = generator.x; out->y = generator.y;
        out->data.power_source = (FactoryPresentationPowerSource){
            generator.generation_capacity, generator.attached_pole_id,
            generator.network_id,
            network_allocated(simulation, generator.network_id),
            generator.connected,
            {
                burner.inventory_item, burner.inventory_quantity,
                burner.current_fuel_item, burner.remaining_burn_ticks,
                burner.total_burn_duration_ticks,
                burner.unreleased_fuel_energy,
                burner.released_energy, burner.active
            }
        };
    } else {
        return FACTORY_RESULT_UNSUPPORTED_ENTITY;
    }
    return FACTORY_RESULT_OK;
}

FactoryResult factory_presentation_snapshot_rebuild(
    FactoryPresentationSnapshot *snapshot,
    const FactorySimulation *simulation
)
{
    FactoryPresentationSnapshot next = {0};
    uint32_t width;
    uint32_t height;
    size_t i;
    size_t resource_index = 0U;
    FactoryEntityId previous = 0U;
    if (snapshot == NULL || simulation == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    next.tick = simulation->clock.tick;
    next.day = simulation->clock.day;
    next.time_of_day = simulation->clock.time_of_day;
    next.entity_count = simulation->entities->count;
    next.power_edge_count = simulation->power.connection_count;
    width = factory_world_get_width(simulation->world);
    height = factory_world_get_height(simulation->world);
    if (width > INT32_MAX || height > INT32_MAX
        || (height != 0U && width > SIZE_MAX / height))
        return FACTORY_RESULT_SNAPSHOT_SIZE_OVERFLOW;
    for (uint32_t y = 0U; y < height; ++y)
        for (uint32_t x = 0U; x < width; ++x) {
            const FactoryTile *tile = factory_world_get_tile(
                simulation->world, (int32_t)x, (int32_t)y
            );
            if (tile != NULL && tile->resource != FACTORY_RESOURCE_NONE)
                ++next.resource_count;
        }
    if ((next.entity_count != 0U
            && (next.entities = presentation_calloc(
                next.entity_count, sizeof(*next.entities))) == NULL)
        || (next.resource_count != 0U
            && (next.resources = presentation_calloc(
                next.resource_count, sizeof(*next.resources))) == NULL)
        || (next.power_edge_count != 0U
            && (next.power_edges = presentation_calloc(
                next.power_edge_count, sizeof(*next.power_edges))) == NULL)) {
        release_contents(&next);
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    for (i = 0U; i < next.entity_count; ++i) {
        FactoryEntityId selected = 0U;
        size_t j;
        for (j = 0U; j < simulation->entities->count; ++j) {
            FactoryEntityId candidate = simulation->entities->live_ids[j];
            if (candidate > previous
                && (selected == 0U || candidate < selected))
                selected = candidate;
        }
        if (selected == 0U
            || populate_entity(
                simulation, selected, &next.entities[i])
                != FACTORY_RESULT_OK) {
            release_contents(&next);
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
        previous = selected;
    }
    for (uint32_t y = 0U; y < height; ++y)
        for (uint32_t x = 0U; x < width; ++x) {
            const FactoryTile *tile = factory_world_get_tile(
                simulation->world, (int32_t)x, (int32_t)y
            );
            if (tile != NULL && tile->resource != FACTORY_RESOURCE_NONE)
                next.resources[resource_index++] =
                    (FactoryPresentationResource){
                        (int32_t)x, (int32_t)y, tile->resource,
                        tile->resource_amount, tile->occupying_entity
                    };
        }
    for (i = 0U; i < next.power_edge_count; ++i) {
        FactoryPowerConnectionInspection edge =
            simulation->power.connections[i];
        FactoryPowerPoleInspection pole_a;
        FactoryPowerPoleInspection pole_b;
        if (edge.pole_a == 0U || edge.pole_a >= edge.pole_b
            || factory_simulation_get_power_pole(
                simulation, edge.pole_a, &pole_a) != FACTORY_RESULT_OK
            || factory_simulation_get_power_pole(
                simulation, edge.pole_b, &pole_b) != FACTORY_RESULT_OK
            || (i != 0U
                && (next.power_edges[i - 1U].pole_a > edge.pole_a
                    || (next.power_edges[i - 1U].pole_a == edge.pole_a
                        && next.power_edges[i - 1U].pole_b >= edge.pole_b)))) {
            release_contents(&next);
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
        next.power_edges[i] = (FactoryPresentationPowerEdge){
            edge.pole_a, edge.pole_b
        };
    }
    release_contents(snapshot);
    *snapshot = next;
    return FACTORY_RESULT_OK;
}

uint64_t factory_presentation_snapshot_get_tick(
    const FactoryPresentationSnapshot *snapshot
)
{
    return snapshot == NULL ? 0U : snapshot->tick;
}

uint64_t factory_presentation_snapshot_get_day(
    const FactoryPresentationSnapshot *snapshot)
{
    return snapshot == NULL ? 0U : snapshot->day;
}

uint32_t factory_presentation_snapshot_get_time_of_day(
    const FactoryPresentationSnapshot *snapshot)
{
    return snapshot == NULL ? 0U : snapshot->time_of_day;
}

size_t factory_presentation_snapshot_get_entity_count(
    const FactoryPresentationSnapshot *snapshot
)
{
    return snapshot == NULL ? 0U : snapshot->entity_count;
}

const FactoryPresentationEntity *factory_presentation_snapshot_get_entity(
    const FactoryPresentationSnapshot *snapshot, size_t index
)
{
    if (snapshot == NULL || index >= snapshot->entity_count) return NULL;
    return &snapshot->entities[index];
}

size_t factory_presentation_snapshot_get_resource_count(
    const FactoryPresentationSnapshot *snapshot
)
{
    return snapshot == NULL ? 0U : snapshot->resource_count;
}

const FactoryPresentationResource *factory_presentation_snapshot_get_resource(
    const FactoryPresentationSnapshot *snapshot, size_t index
)
{
    if (snapshot == NULL || index >= snapshot->resource_count) return NULL;
    return &snapshot->resources[index];
}

size_t factory_presentation_snapshot_get_power_edge_count(
    const FactoryPresentationSnapshot *snapshot
)
{
    return snapshot == NULL ? 0U : snapshot->power_edge_count;
}

const FactoryPresentationPowerEdge *
factory_presentation_snapshot_get_power_edge(
    const FactoryPresentationSnapshot *snapshot, size_t index
)
{
    if (snapshot == NULL || index >= snapshot->power_edge_count) return NULL;
    return &snapshot->power_edges[index];
}
