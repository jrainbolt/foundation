#include "foundation/simulation.h"

#include <stdlib.h>

#include "belt_internal.h"
#include "extractor_internal.h"
#include "refinery_internal.h"
#include "storage_internal.h"
#include "world_internal.h"

typedef enum {
    TRANSFER_TO_BELT = 0,
    TRANSFER_TO_STORAGE,
    TRANSFER_TO_REFINERY
} TransferKind;

typedef enum {
    SOURCE_BELT = 0,
    SOURCE_EXTRACTOR,
    SOURCE_REFINERY
} TransferSourceKind;

typedef struct {
    FactoryEntityId source_id;
    FactoryEntityId destination_id;
    FactoryItemType item;
    TransferKind kind;
    TransferSourceKind source_kind;
    bool wins;
} TransferIntent;

struct FactorySimulation {
    uint64_t tick;
    FactoryWorld *world;
    FactoryEntityManager *entities;
    FactoryExtractorStore extractors;
    FactoryRefineryStore refineries;
    FactoryBeltStore belts;
    FactoryStorageStore storages;
    FactoryCommand commands[FACTORY_COMMAND_QUEUE_CAPACITY];
    size_t command_count;
    FactoryCommandResult results[FACTORY_COMMAND_QUEUE_CAPACITY];
    size_t result_count;
};

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

static void apply_commands(FactorySimulation *simulation)
{
    size_t index;

    simulation->result_count = simulation->command_count;
    for (index = 0U; index < simulation->command_count; ++index) {
        FactoryCommandResult *result = &simulation->results[index];

        result->command = simulation->commands[index];
        result->entity_id = 0U;
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
        }
    }
    simulation->command_count = 0U;
}

static void add_producer_intent(
    FactorySimulation *simulation,
    TransferIntent *intents,
    size_t *count,
    FactoryEntityId source_id,
    TransferSourceKind source_kind,
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
    if (belt == NULL || belt->item != FACTORY_ITEM_NONE) {
        return;
    }
    intents[*count].source_id = source_id;
    intents[*count].destination_id = belt->entity_id;
    intents[*count].item = item;
    intents[*count].kind = TRANSFER_TO_BELT;
    intents[*count].source_kind = source_kind;
    intents[*count].wins = true;
    ++*count;
}

static void resolve_transfers(TransferIntent *intents, size_t count)
{
    size_t first;
    size_t second;

    for (first = 0U; first < count; ++first) {
        for (second = first + 1U; second < count; ++second) {
            if (intents[first].destination_id
                == intents[second].destination_id) {
                if (intents[first].source_id < intents[second].source_id) {
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
    size_t capacity =
        simulation->extractors.count + simulation->refineries.count;
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
                SOURCE_EXTRACTOR, source->x, source->y,
                source->output_direction, source->output_item
            );
        }
    }
    for (index = 0U; index < simulation->refineries.count; ++index) {
        const FactoryRefinery *source = &simulation->refineries.items[index];

        if (source->output_item != FACTORY_ITEM_NONE) {
            add_producer_intent(
                simulation, intents, &count, source->entity_id,
                SOURCE_REFINERY, source->x, source->y,
                source->output_direction, source->output_item
            );
        }
    }
    resolve_transfers(intents, count);
    for (index = 0U; index < count; ++index) {
        FactoryBelt *destination;

        if (!intents[index].wins) {
            continue;
        }
        destination = factory_belt_store_find_mutable(
            &simulation->belts, intents[index].destination_id
        );
        destination->item = intents[index].item;
        destination->movement_progress = 0U;
        if (intents[index].source_kind == SOURCE_EXTRACTOR) {
            FactoryExtractor *source = factory_extractor_store_find_mutable(
                &simulation->extractors, intents[index].source_id
            );
            source->output_item = FACTORY_ITEM_NONE;
            source->output_amount = 0U;
        } else {
            FactoryRefinery *source = factory_refinery_store_find_mutable(
                &simulation->refineries, intents[index].source_id
            );
            source->output_item = FACTORY_ITEM_NONE;
            source->output_amount = 0U;
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
        const FactoryStorage *destination_storage;
        const FactoryRefinery *destination_refinery;
        const FactoryRecipe *destination_recipe;
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
        destination_recipe = destination_refinery == NULL
            ? NULL
            : factory_recipe_get(destination_refinery->recipe_id);
        if (destination_belt != NULL
            && destination_belt->item == FACTORY_ITEM_NONE) {
            intents[intent_count].kind = TRANSFER_TO_BELT;
        } else if (destination_storage != NULL
            && (source->item == FACTORY_ITEM_IRON_ORE
                || source->item == FACTORY_ITEM_IRON_PLATE
                || source->item == FACTORY_ITEM_COPPER_ORE
                || source->item == FACTORY_ITEM_COPPER_PLATE)
            && factory_storage_get_total_amount(destination_storage)
                < destination_storage->total_capacity) {
            intents[intent_count].kind = TRANSFER_TO_STORAGE;
        } else if (destination_refinery != NULL
            && destination_recipe != NULL
            && source->item == destination_recipe->input_item
            && destination_refinery->input_item == FACTORY_ITEM_NONE) {
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
            intents[intent_count].kind = TRANSFER_TO_REFINERY;
        } else {
            continue;
        }
        intents[intent_count].source_id = source->entity_id;
        intents[intent_count].destination_id = tile->occupying_entity;
        intents[intent_count].item = source->item;
        intents[intent_count].source_kind = SOURCE_BELT;
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
        FactoryBelt *source;

        if (!intents[index].wins) {
            continue;
        }
        source = factory_belt_store_find_mutable(
            &simulation->belts, intents[index].source_id
        );
        if (intents[index].kind == TRANSFER_TO_BELT) {
            FactoryBelt *destination = factory_belt_store_find_mutable(
                &simulation->belts, intents[index].destination_id
            );
            destination->item = intents[index].item;
            destination->movement_progress = 0U;
        } else if (intents[index].kind == TRANSFER_TO_STORAGE) {
            FactoryStorage *destination = factory_storage_store_find_mutable(
                &simulation->storages, intents[index].destination_id
            );
            if (intents[index].item == FACTORY_ITEM_IRON_ORE) {
                ++destination->iron_ore_amount;
            } else if (intents[index].item == FACTORY_ITEM_IRON_PLATE) {
                ++destination->iron_plate_amount;
            } else if (intents[index].item == FACTORY_ITEM_COPPER_ORE) {
                ++destination->copper_ore_amount;
            } else {
                ++destination->copper_plate_amount;
            }
        } else {
            FactoryRefinery *destination =
                factory_refinery_store_find_mutable(
                    &simulation->refineries, intents[index].destination_id
                );
            destination->input_item = intents[index].item;
            destination->input_amount = 1U;
        }
        source->item = FACTORY_ITEM_NONE;
        source->movement_progress = 0U;
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
