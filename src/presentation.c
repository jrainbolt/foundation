#include "presentation_internal.h"

#include "assembler_recipe_internal.h"
#include "simulation_internal.h"

#include <stdlib.h>

struct FactoryPresentationSnapshot {
    uint64_t tick;
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
    next.tick = simulation->tick;
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
