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
    return simulation;
}

void factory_simulation_destroy(FactorySimulation *simulation)
{
    if (simulation == NULL) {
        return;
    }
    factory_storage_store_destroy(&simulation->storages);
    factory_belt_store_destroy(&simulation->belts);
    factory_inserter_store_destroy(&simulation->inserters);
    factory_splitter_store_destroy(&simulation->splitters);
    factory_assembler_store_destroy(&simulation->assemblers);
    factory_refinery_store_destroy(&simulation->refineries);
    factory_extractor_store_destroy(&simulation->extractors);
    factory_entity_manager_destroy(simulation->entities);
    free(simulation);
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
        if (assembler->iron_plate_amount != 0U
            || assembler->copper_plate_amount != 0U
            || assembler->output_item != FACTORY_ITEM_NONE
            || assembler->output_amount != 0U) {
            return FACTORY_RESULT_ENTITY_HAS_MATERIAL;
        }
        *out_type = FACTORY_ENTITY_TYPE_ASSEMBLER;
        *out_x = assembler->x;
        *out_y = assembler->y;
    } else if (storage != NULL) {
        if (factory_storage_get_total_amount(storage) != 0U) {
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
    FactoryResult result = validate_demolition(
        simulation, id, out_type, out_x, out_y
    );

    if (result != FACTORY_RESULT_OK) {
        return result;
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
        }
    }
    simulation->command_count = 0U;
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
            if (source->item == FACTORY_ITEM_IRON_PLATE) {
                destination = (FactoryLogisticsEndpoint){
                    destination_assembler->entity_id,
                    FACTORY_LOGISTICS_SLOT_IRON_INPUT
                };
            } else if (source->item == FACTORY_ITEM_COPPER_PLATE) {
                destination = (FactoryLogisticsEndpoint){
                    destination_assembler->entity_id,
                    FACTORY_LOGISTICS_SLOT_COPPER_INPUT
                };
            } else {
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
        *out_endpoint = (FactoryLogisticsEndpoint){
            assembler->entity_id,
            inserter->held_item == FACTORY_ITEM_IRON_PLATE
                ? FACTORY_LOGISTICS_SLOT_IRON_INPUT
                : FACTORY_LOGISTICS_SLOT_COPPER_INPUT
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

void factory_simulation_tick(FactorySimulation *simulation)
{
    if (simulation == NULL) {
        return;
    }
    apply_commands(simulation);
    factory_extractor_store_update(&simulation->extractors, simulation->world);
    update_producer_transfers(simulation);
    factory_belt_store_advance(&simulation->belts);
    update_belt_transfers(simulation);
    factory_refinery_store_update(&simulation->refineries);
    factory_assembler_store_update(&simulation->assemblers);
    update_inserters(simulation);
    ++simulation->tick;
}

uint64_t factory_simulation_get_tick(const FactorySimulation *simulation)
{
    return simulation == NULL ? 0U : simulation->tick;
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
