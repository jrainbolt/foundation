#include "foundation/snapshot.h"

#include "assembler_recipe_internal.h"
#include "simulation_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define SNAPSHOT_HEADER_SIZE 48U
#define SNAPSHOT_SECTION_HEADER_SIZE 16U
#define SNAPSHOT_SECTION_COUNT 20U

static const uint8_t snapshot_magic[8] = {
    'F', 'O', 'U', 'N', 'D', 'A', 'T', 'N'
};

typedef enum {
    SNAPSHOT_SECTION_METADATA = 1,
    SNAPSHOT_SECTION_ENTITIES,
    SNAPSHOT_SECTION_WORLD,
    SNAPSHOT_SECTION_EXTRACTORS,
    SNAPSHOT_SECTION_BELTS,
    SNAPSHOT_SECTION_SPLITTERS,
    SNAPSHOT_SECTION_REFINERIES,
    SNAPSHOT_SECTION_ASSEMBLERS,
    SNAPSHOT_SECTION_INSERTERS,
    SNAPSHOT_SECTION_STORAGES,
    SNAPSHOT_SECTION_POWER_POLES,
    SNAPSHOT_SECTION_POWER_GENERATORS,
    SNAPSHOT_SECTION_FLUID_STORAGES,
    SNAPSHOT_SECTION_PIPES,
    SNAPSHOT_SECTION_WATER_EXTRACTORS,
    SNAPSHOT_SECTION_BOILERS,
    SNAPSHOT_SECTION_STEAM_ENGINES,
    SNAPSHOT_SECTION_SOLAR_GENERATORS,
    SNAPSHOT_SECTION_COMMANDS,
    SNAPSHOT_SECTION_RESULTS
} SnapshotSection;

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t offset;
    bool failed;
} SnapshotWriter;

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
    bool truncated;
} SnapshotReader;

static bool checked_add(size_t *value, size_t amount)
{
    if (*value > SIZE_MAX - amount) {
        return false;
    }
    *value += amount;
    return true;
}

static bool checked_records(
    size_t *value,
    size_t count,
    size_t record_size
)
{
    if (count > SIZE_MAX / record_size) {
        return false;
    }
    return checked_add(value, count * record_size);
}

static void write_bytes(
    SnapshotWriter *writer,
    const uint8_t *bytes,
    size_t count
)
{
    size_t end = writer->offset;

    if (!checked_add(&end, count)) {
        writer->failed = true;
        return;
    }
    if (writer->data != NULL) {
        if (end > writer->capacity) {
            writer->failed = true;
            return;
        }
        (void)memcpy(writer->data + writer->offset, bytes, count);
    }
    writer->offset = end;
}

static void write_u32(SnapshotWriter *writer, uint32_t value)
{
    uint8_t bytes[4] = {
        (uint8_t)value,
        (uint8_t)(value >> 8U),
        (uint8_t)(value >> 16U),
        (uint8_t)(value >> 24U)
    };
    write_bytes(writer, bytes, sizeof(bytes));
}

static void write_i32(SnapshotWriter *writer, int32_t value)
{
    write_u32(writer, (uint32_t)value);
}

static void write_u64(SnapshotWriter *writer, uint64_t value)
{
    uint8_t bytes[8];
    size_t index;

    for (index = 0U; index < sizeof(bytes); ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
    write_bytes(writer, bytes, sizeof(bytes));
}

static bool read_bytes(SnapshotReader *reader, uint8_t *bytes, size_t count)
{
    if (reader->offset > reader->size
        || count > reader->size - reader->offset) {
        reader->truncated = true;
        return false;
    }
    if (bytes != NULL) {
        (void)memcpy(bytes, reader->data + reader->offset, count);
    }
    reader->offset += count;
    return true;
}

static bool read_u32(SnapshotReader *reader, uint32_t *out_value)
{
    uint8_t bytes[4];

    if (!read_bytes(reader, bytes, sizeof(bytes))) {
        return false;
    }
    *out_value = (uint32_t)bytes[0]
        | ((uint32_t)bytes[1] << 8U)
        | ((uint32_t)bytes[2] << 16U)
        | ((uint32_t)bytes[3] << 24U);
    return true;
}

static bool read_i32(SnapshotReader *reader, int32_t *out_value)
{
    uint32_t value;

    if (!read_u32(reader, &value)) {
        return false;
    }
    *out_value = (int32_t)value;
    return true;
}

static bool read_u64(SnapshotReader *reader, uint64_t *out_value)
{
    uint8_t bytes[8];
    uint64_t value = 0U;
    size_t index;

    if (!read_bytes(reader, bytes, sizeof(bytes))) {
        return false;
    }
    for (index = 0U; index < sizeof(bytes); ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    *out_value = value;
    return true;
}

static bool item_valid_or_none(uint32_t value)
{
    return value <= (uint32_t)FACTORY_ITEM_BIOMASS_PELLET;
}

static bool item_valid(uint32_t value)
{
    return value >= (uint32_t)FACTORY_ITEM_IRON_ORE
        && item_valid_or_none(value);
}

static bool direction_valid(uint32_t value)
{
    return value <= (uint32_t)FACTORY_DIRECTION_WEST;
}

static bool snapshot_command_valid(const FactoryCommand *command)
{
    if (!factory_command_is_well_formed(command)) {
        return false;
    }
    if (command->type == FACTORY_COMMAND_SET_REFINERY_RECIPE) {
        return command->data.set_refinery_recipe.recipe_id
            >= FACTORY_RECIPE_NONE
            && command->data.set_refinery_recipe.recipe_id
                <= FACTORY_RECIPE_COPPER_PLATE;
    }
    return true;
}

static void write_section_header(
    SnapshotWriter *writer,
    SnapshotSection type,
    size_t count,
    size_t payload_size
)
{
    write_u32(writer, (uint32_t)type);
    write_u32(writer, 1U);
    write_u32(writer, (uint32_t)count);
    write_u32(writer, (uint32_t)payload_size);
}

static bool size_to_u32(size_t value)
{
    return value <= UINT32_MAX;
}

static bool section_size_valid(
    size_t count,
    size_t record_size,
    size_t prefix_size
)
{
    return count <= (UINT32_MAX - prefix_size) / record_size;
}

static FactoryResult snapshot_size_unvalidated(
    const FactorySimulation *simulation,
    size_t *out_size
)
{
    size_t size = SNAPSHOT_HEADER_SIZE
        + SNAPSHOT_SECTION_COUNT * SNAPSHOT_SECTION_HEADER_SIZE;
    size_t tiles;

    if ((size_t)simulation->world->width
        > SIZE_MAX / (size_t)simulation->world->height) {
        return FACTORY_RESULT_SNAPSHOT_SIZE_OVERFLOW;
    }
    tiles = (size_t)simulation->world->width
        * (size_t)simulation->world->height;
    if (!checked_add(&size, 16U)
        || !checked_add(&size, 8U)
        || !checked_records(
            &size, simulation->entities->count, 4U)
        || !checked_add(&size, 8U)
        || !checked_records(&size, tiles, 16U)
        || !checked_records(&size, simulation->extractors.count, 36U)
        || !checked_records(&size, simulation->belts.count, 24U)
        || !checked_records(&size, simulation->splitters.count, 24U)
        || !checked_records(&size, simulation->refineries.count, 48U)
        || !checked_records(&size, simulation->assemblers.count, 64U)
        || !checked_records(&size, simulation->inserters.count, 48U)
        || !checked_records(&size, simulation->storages.count, 60U)
        || !checked_records(&size, simulation->power_poles.count, 12U)
        || !checked_records(&size, simulation->power_generators.count, 44U)
        || !checked_records(&size, simulation->fluid_storages.count, 32U)
        || !checked_records(&size, simulation->pipes.count, 12U)
        || !checked_records(&size, simulation->water_extractors.count, 16U)
        || !checked_records(&size, simulation->boilers.count, 48U)
        || !checked_records(&size, simulation->steam_engines.count, 16U)
        || !checked_records(&size, simulation->solar_generators.count, 12U)
        || !checked_records(&size, simulation->command_count, 24U)
        || !checked_records(&size, simulation->result_count, 68U)
        || !size_to_u32(simulation->entities->count)
        || !size_to_u32(tiles)
        || !size_to_u32(simulation->extractors.count)
        || !size_to_u32(simulation->belts.count)
        || !size_to_u32(simulation->splitters.count)
        || !size_to_u32(simulation->refineries.count)
        || !size_to_u32(simulation->assemblers.count)
        || !size_to_u32(simulation->inserters.count)
        || !size_to_u32(simulation->storages.count)
        || !size_to_u32(simulation->power_poles.count)
        || !size_to_u32(simulation->power_generators.count)
        || !size_to_u32(simulation->fluid_storages.count)
        || !size_to_u32(simulation->pipes.count)
        || !size_to_u32(simulation->water_extractors.count)
        || !size_to_u32(simulation->boilers.count)
        || !size_to_u32(simulation->steam_engines.count)
        || !size_to_u32(simulation->solar_generators.count)
        || !section_size_valid(
            simulation->entities->count, 4U, 8U)
        || !section_size_valid(tiles, 16U, 8U)
        || !section_size_valid(simulation->extractors.count, 36U, 0U)
        || !section_size_valid(simulation->belts.count, 24U, 0U)
        || !section_size_valid(simulation->splitters.count, 24U, 0U)
        || !section_size_valid(simulation->refineries.count, 48U, 0U)
        || !section_size_valid(simulation->assemblers.count, 64U, 0U)
        || !section_size_valid(simulation->inserters.count, 48U, 0U)
        || !section_size_valid(simulation->storages.count, 60U, 0U)
        || !section_size_valid(simulation->power_poles.count, 12U, 0U)
        || !section_size_valid(simulation->power_generators.count, 44U, 0U)
        || !section_size_valid(simulation->fluid_storages.count, 32U, 0U)
        || !section_size_valid(simulation->pipes.count, 12U, 0U)
        || !section_size_valid(simulation->water_extractors.count, 16U, 0U)
        || !section_size_valid(simulation->boilers.count, 48U, 0U)
        || !section_size_valid(simulation->steam_engines.count, 16U, 0U)
        || !section_size_valid(simulation->solar_generators.count, 12U, 0U)
        || size > UINT64_MAX) {
        return FACTORY_RESULT_SNAPSHOT_SIZE_OVERFLOW;
    }
    *out_size = size;
    return FACTORY_RESULT_OK;
}

static bool entity_has_subsystem(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    int32_t *out_x,
    int32_t *out_y
)
{
    const FactoryExtractor *extractor =
        factory_extractor_store_find(&simulation->extractors, id);
    const FactoryBelt *belt =
        factory_belt_store_find(&simulation->belts, id);
    const FactorySplitter *splitter =
        factory_splitter_store_find(&simulation->splitters, id);
    const FactoryRefinery *refinery =
        factory_refinery_store_find(&simulation->refineries, id);
    const FactoryAssembler *assembler =
        factory_assembler_store_find(&simulation->assemblers, id);
    const FactoryInserter *inserter =
        factory_inserter_store_find(&simulation->inserters, id);
    const FactoryStorage *storage =
        factory_storage_store_find(&simulation->storages, id);
    const FactoryPowerPole *pole =
        factory_power_pole_store_find(&simulation->power_poles, id);
    const FactoryPowerGenerator *generator =
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
    bool tank = fluid_storage != NULL && water_extractor == NULL
        && steam_engine == NULL;
    size_t found = (extractor != NULL) + (belt != NULL)
        + (splitter != NULL) + (refinery != NULL) + (assembler != NULL)
        + (inserter != NULL) + (storage != NULL)
        + (pole != NULL) + (generator != NULL) + tank
        + (pipe != NULL) + (water_extractor != NULL) + (boiler != NULL)
        + (steam_engine != NULL) + (solar_generator != NULL);
    if (steam_engine != NULL && generator != NULL) --found;
    if (solar_generator != NULL && generator != NULL) --found;

    if (found != 1U) {
        return false;
    }
    if (extractor != NULL) {
        *out_x = extractor->x; *out_y = extractor->y;
    } else if (belt != NULL) {
        *out_x = belt->x; *out_y = belt->y;
    } else if (splitter != NULL) {
        *out_x = splitter->x; *out_y = splitter->y;
    } else if (refinery != NULL) {
        *out_x = refinery->x; *out_y = refinery->y;
    } else if (assembler != NULL) {
        *out_x = assembler->x; *out_y = assembler->y;
    } else if (inserter != NULL) {
        *out_x = inserter->x; *out_y = inserter->y;
    } else if (storage != NULL) {
        *out_x = storage->x; *out_y = storage->y;
    } else if (pole != NULL) {
        *out_x = pole->x; *out_y = pole->y;
    } else if (generator != NULL) {
        *out_x = generator->x; *out_y = generator->y;
    } else if (water_extractor != NULL) {
        *out_x = water_extractor->x; *out_y = water_extractor->y;
    } else if (boiler != NULL) {
        *out_x = boiler->x; *out_y = boiler->y;
    } else if (steam_engine != NULL) {
        *out_x = steam_engine->x; *out_y = steam_engine->y;
    } else if (solar_generator != NULL) {
        *out_x = solar_generator->x; *out_y = solar_generator->y;
    } else if (tank) {
        *out_x = fluid_storage->x; *out_y = fluid_storage->y;
    } else {
        *out_x = pipe->x; *out_y = pipe->y;
    }
    return true;
}

static bool validate_assembler(const FactoryAssembler *assembler)
{
    const FactoryAssemblerRecipe *recipe =
        factory_assembler_recipe_find(assembler->recipe_id);
    size_t slot;

    if (!direction_valid((uint32_t)assembler->output_direction)
        || !item_valid_or_none((uint32_t)assembler->output_item)
        || (assembler->processing ? 1U : 0U) > 1U) {
        return false;
    }
    if (assembler->recipe_id == FACTORY_ASSEMBLER_RECIPE_NONE) {
        return assembler->input_slots[0].item == FACTORY_ITEM_NONE
            && assembler->input_slots[0].count == 0U
            && assembler->input_slots[0].capacity == 0U
            && assembler->input_slots[1].item == FACTORY_ITEM_NONE
            && assembler->input_slots[1].count == 0U
            && assembler->input_slots[1].capacity == 0U
            && !assembler->processing
            && assembler->processing_progress == 0U
            && assembler->processing_duration == 0U
            && assembler->output_item == FACTORY_ITEM_NONE
            && assembler->output_amount == 0U;
    }
    if (recipe == NULL
        || assembler->processing_duration != recipe->processing_ticks
        || assembler->processing_progress >= recipe->processing_ticks
        || (!assembler->processing
            && assembler->processing_progress != 0U)
        || (assembler->processing
            && assembler->output_item != FACTORY_ITEM_NONE)) {
        return false;
    }
    for (slot = 0U; slot < FACTORY_ASSEMBLER_MAX_INPUT_TYPES; ++slot) {
        FactoryItemType expected = slot < recipe->input_count
            ? recipe->input_items[slot] : FACTORY_ITEM_NONE;
        uint32_t capacity = slot < recipe->input_count
            ? recipe->input_amounts[slot] : 0U;
        if (assembler->input_slots[slot].item != expected
            || assembler->input_slots[slot].capacity != capacity
            || assembler->input_slots[slot].count > capacity) {
            return false;
        }
    }
    return (assembler->output_amount == 0U
            && assembler->output_item == FACTORY_ITEM_NONE)
        || (assembler->output_item == recipe->output_item
            && assembler->output_amount != 0U
            && assembler->output_amount <= recipe->output_amount);
}

static FactoryResult validate_simulation(
    const FactorySimulation *simulation
)
{
    size_t index;
    size_t subsystem_count;
    FactoryEntityId max_id = 0U;

    if (simulation == NULL || simulation->world == NULL
        || simulation->entities == NULL
        || simulation->world->width == 0U
        || simulation->world->height == 0U
        || simulation->command_count > FACTORY_COMMAND_QUEUE_CAPACITY
        || simulation->result_count > FACTORY_COMMAND_QUEUE_CAPACITY
        || simulation->clock.day
            != simulation->clock.tick / FACTORY_CLOCK_TICKS_PER_DAY
        || simulation->clock.time_of_day
            != simulation->clock.tick % FACTORY_CLOCK_TICKS_PER_DAY) {
        return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    }
    if (simulation->fluid_storages.count
        < simulation->water_extractors.count + simulation->boilers.count * 2U
            + simulation->steam_engines.count)
        return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    if (simulation->power_generators.count
        < simulation->steam_engines.count + simulation->solar_generators.count)
        return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    subsystem_count = simulation->extractors.count
        + simulation->belts.count + simulation->splitters.count
        + simulation->refineries.count + simulation->assemblers.count
        + simulation->inserters.count + simulation->storages.count
        + simulation->power_poles.count
        + simulation->power_generators.count + simulation->pipes.count
        + simulation->water_extractors.count + simulation->boilers.count
        + simulation->steam_engines.count
        + simulation->solar_generators.count
        + simulation->fluid_storages.count
        - simulation->water_extractors.count - simulation->boilers.count * 2U
        - simulation->steam_engines.count
        - simulation->steam_engines.count
        - simulation->solar_generators.count;
    if (subsystem_count != simulation->entities->count) {
        return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    }
    for (index = 0U; index < simulation->entities->count; ++index) {
        FactoryEntityId id = simulation->entities->live_ids[index];
        int32_t x;
        int32_t y;
        size_t other;

        if (id == 0U || !entity_has_subsystem(simulation, id, &x, &y)
            || !factory_world_is_in_bounds(simulation->world, x, y)
            || factory_world_get_tile(
                simulation->world, x, y)->occupying_entity != id) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
        for (other = index + 1U;
            other < simulation->entities->count;
            ++other) {
            if (id == simulation->entities->live_ids[other]) {
                return FACTORY_RESULT_SNAPSHOT_CORRUPT;
            }
        }
        if (id > max_id) {
            max_id = id;
        }
    }
    if (simulation->entities->next_id == 0U
        || (simulation->entities->count != 0U
            && simulation->entities->next_id <= max_id)) {
        return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    }
    {
        size_t tile_count = (size_t)simulation->world->width
            * (size_t)simulation->world->height;
        for (index = 0U; index < tile_count; ++index) {
            const FactoryTile *tile = &simulation->world->tiles[index];
            int32_t entity_x;
            int32_t entity_y;
            if (tile->terrain != FACTORY_TERRAIN_GROUND
                || tile->resource > FACTORY_RESOURCE_COPPER
                || (tile->resource == FACTORY_RESOURCE_NONE
                    && tile->resource_amount != 0U)
                || (tile->occupying_entity != 0U
                    && !factory_entity_is_valid(
                        simulation->entities,
                        tile->occupying_entity))) {
                return FACTORY_RESULT_SNAPSHOT_CORRUPT;
            }
            if (tile->occupying_entity != 0U
                && (!entity_has_subsystem(
                        simulation, tile->occupying_entity,
                        &entity_x, &entity_y)
                    || entity_x
                        != (int32_t)(index % simulation->world->width)
                    || entity_y
                        != (int32_t)(index / simulation->world->width))) {
                return FACTORY_RESULT_SNAPSHOT_CORRUPT;
            }
        }
    }
    for (index = 0U; index < simulation->extractors.count; ++index) {
        const FactoryExtractor *value = &simulation->extractors.items[index];
        const FactoryTile *tile = factory_world_get_tile(
            simulation->world, value->x, value->y
        );
        if (!direction_valid((uint32_t)value->output_direction)
            || value->resource_type < FACTORY_RESOURCE_IRON
            || value->resource_type > FACTORY_RESOURCE_COPPER
            || !item_valid((uint32_t)value->produced_item)
            || value->produced_item != (
                value->resource_type == FACTORY_RESOURCE_IRON
                    ? FACTORY_ITEM_IRON_ORE : FACTORY_ITEM_COPPER_ORE)
            || tile == NULL
            || tile->resource != value->resource_type
            || !item_valid_or_none((uint32_t)value->output_item)
            || value->production_progress >= FACTORY_EXTRACTOR_PRODUCTION_TICKS
            || value->output_amount > 1U
            || ((value->output_amount == 0U)
                != (value->output_item == FACTORY_ITEM_NONE))) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
    }
    for (index = 0U; index < simulation->belts.count; ++index) {
        const FactoryBelt *value = &simulation->belts.items[index];
        if (!direction_valid((uint32_t)value->direction)
            || !item_valid_or_none((uint32_t)value->item)
            || value->movement_progress >= FACTORY_BELT_TRANSFER_TICKS
            || (value->item == FACTORY_ITEM_NONE
                && value->movement_progress != 0U)) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
    }
    for (index = 0U; index < simulation->splitters.count; ++index) {
        const FactorySplitter *value = &simulation->splitters.items[index];
        if (!direction_valid((uint32_t)value->facing)
            || !item_valid_or_none((uint32_t)value->item)
            || value->next_output > FACTORY_SPLITTER_OUTPUT_RIGHT) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
    }
    for (index = 0U; index < simulation->refineries.count; ++index) {
        const FactoryRefinery *value = &simulation->refineries.items[index];
        const FactoryRecipe *recipe = factory_recipe_get(value->recipe_id);
        if (!direction_valid((uint32_t)value->input_direction)
            || !direction_valid((uint32_t)value->output_direction)
            || value->input_direction == value->output_direction
            || !item_valid_or_none((uint32_t)value->input_item)
            || !item_valid_or_none((uint32_t)value->output_item)
            || (value->input_amount == 0U)
                != (value->input_item == FACTORY_ITEM_NONE)
            || (value->output_amount == 0U)
                != (value->output_item == FACTORY_ITEM_NONE)
            || (recipe == NULL && value->recipe_id != FACTORY_RECIPE_NONE)
            || (value->recipe_id == FACTORY_RECIPE_NONE
                && (value->input_item != FACTORY_ITEM_NONE
                    || value->input_amount != 0U
                    || value->output_item != FACTORY_ITEM_NONE
                    || value->output_amount != 0U
                    || value->processing
                    || value->processing_progress != 0U))
            || (value->processing && recipe == NULL)
            || (!value->processing && value->processing_progress != 0U)
            || (recipe != NULL
                && (value->processing_progress >= recipe->processing_ticks
                    || (value->input_amount != 0U
                        && (value->input_item != recipe->input_item
                            || value->input_amount
                                > recipe->input_amount))
                    || (value->output_amount != 0U
                        && (value->output_item != recipe->output_item
                            || value->output_amount
                                > recipe->output_amount))))) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
    }
    for (index = 0U; index < simulation->assemblers.count; ++index) {
        if (!validate_assembler(&simulation->assemblers.items[index])) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
    }
    for (index = 0U; index < simulation->inserters.count; ++index) {
        const FactoryInserter *value = &simulation->inserters.items[index];
        bool holding = value->held_amount == 1U
            && item_valid((uint32_t)value->held_item);
        int32_t dx = value->facing == FACTORY_DIRECTION_EAST ? 1
            : value->facing == FACTORY_DIRECTION_WEST ? -1 : 0;
        int32_t dy = value->facing == FACTORY_DIRECTION_SOUTH ? 1
            : value->facing == FACTORY_DIRECTION_NORTH ? -1 : 0;
        if (!direction_valid((uint32_t)value->facing)
            || value->state > FACTORY_INSERTER_STATE_DROPPING
            || value->progress >= FACTORY_INSERTER_ACTION_TICKS
            || (!holding
                && (value->held_amount != 0U
                    || value->held_item != FACTORY_ITEM_NONE))
            || ((value->state == FACTORY_INSERTER_STATE_HOLDING
                    || value->state == FACTORY_INSERTER_STATE_DROPPING)
                != holding)
            || value->source_x != value->x - dx
            || value->source_y != value->y - dy
            || value->destination_x != value->x + dx
            || value->destination_y != value->y + dy) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
    }
    for (index = 0U; index < simulation->storages.count; ++index) {
        const FactoryStorage *value = &simulation->storages.items[index];
        uint64_t total = (uint64_t)value->iron_ore_amount
            + value->iron_plate_amount + value->copper_ore_amount
            + value->copper_plate_amount
            + value->electronic_component_amount + value->iron_gear_amount
            + value->copper_wire_amount + value->biomass_pellet_amount;
        if (total > value->total_capacity
            || value->total_capacity != FACTORY_STORAGE_CAPACITY
            || !item_valid_or_none(
                (uint32_t)value->configured_output_item)
            || (value->output_occupied
                && !item_valid((uint32_t)value->output_item))
            || (!value->output_occupied
                && value->output_item != FACTORY_ITEM_NONE)) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
    }
    for (index = 0U; index < simulation->power_generators.count; ++index) {
        const FactoryPowerGenerator *generator =
            &simulation->power_generators.items[index];
        const FactorySteamEngine *steam_engine =
            factory_steam_engine_store_find(
                &simulation->steam_engines, generator->entity_id);
        const FactorySolarGenerator *solar_generator =
            factory_solar_generator_store_find(
                &simulation->solar_generators, generator->entity_id);
        const FactoryBurner *burner = factory_burner_store_find(
            &simulation->burners, generator->entity_id);
        const FactoryFuelDefinition *inventory_definition;
        const FactoryFuelDefinition *current_definition;
        if (generator->generation_capacity
                != FACTORY_BASIC_GENERATOR_CAPACITY)
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        if (steam_engine != NULL) {
            if (burner != NULL
                || steam_engine->recipe_id
                    != FACTORY_STEAM_GENERATION_RECIPE_BASIC)
                return FACTORY_RESULT_SNAPSHOT_CORRUPT;
            continue;
        }
        if (solar_generator != NULL) {
            if (burner != NULL) return FACTORY_RESULT_SNAPSHOT_CORRUPT;
            continue;
        }
        if (burner == NULL
            || burner->accepted_fuel_classes != FACTORY_FUEL_CLASS_SOLID) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
        inventory_definition =
            factory_fuel_definition_get(burner->inventory_item);
        current_definition =
            factory_fuel_definition_get(burner->current_fuel_item);
        if ((burner->inventory_quantity == 0U)
                != (burner->inventory_item == FACTORY_ITEM_NONE)
            || (burner->remaining_burn_ticks == 0U)
                != (burner->current_fuel_item == FACTORY_ITEM_NONE)
            || (inventory_definition != NULL
                && (inventory_definition->fuel_class
                    & burner->accepted_fuel_classes) == 0U)
            || (burner->inventory_quantity != 0U
                && inventory_definition == NULL)
            || (burner->remaining_burn_ticks != 0U
                && (current_definition == NULL
                    || burner->remaining_burn_ticks
                        >= current_definition->burn_duration_ticks))
            || burner->released_energy
                > FACTORY_BURNER_RELEASED_ENERGY_CAPACITY
            || (burner->remaining_burn_ticks != 0U
                && factory_burner_unreleased_energy(burner)
                    > FACTORY_BURNER_RELEASED_ENERGY_CAPACITY
                        - burner->released_energy)) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
    }
    if (simulation->burners.count
        != simulation->power_generators.count - simulation->steam_engines.count
            - simulation->solar_generators.count
            + simulation->boilers.count)
        return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    for (index = 0U; index < simulation->steam_engines.count; ++index) {
        const FactorySteamEngine *engine =
            &simulation->steam_engines.items[index];
        const FactoryFluidStorage *storage =
            factory_fluid_storage_store_find_slot(
                &simulation->fluid_storages, engine->entity_id,
                FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT);
        const FactoryPowerGenerator *generator =
            factory_power_generator_store_find(
                &simulation->power_generators, engine->entity_id);
        if (engine->recipe_id != FACTORY_STEAM_GENERATION_RECIPE_BASIC
            || engine->generated_last_tick > FACTORY_STEAM_ENGINE_MAX_OUTPUT
            || storage == NULL || generator == NULL
            || storage->capacity != FACTORY_STEAM_ENGINE_STORAGE_CAPACITY
            || storage->accepted_fluid_classes != FACTORY_FLUID_CLASS_VAPOR)
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    }
    for (index = 0U; index < simulation->solar_generators.count; ++index) {
        const FactorySolarGenerator *solar =
            &simulation->solar_generators.items[index];
        const FactoryPowerGenerator *generator =
            factory_power_generator_store_find(
                &simulation->power_generators, solar->entity_id);
        if (generator == NULL
            || generator->generation_capacity
                != FACTORY_SOLAR_GENERATOR_MAX_OUTPUT
            || solar->generated_last_tick
                > factory_solar_generator_available(
                    simulation, solar->entity_id))
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    }
    for (index = 0U; index < simulation->water_extractors.count; ++index) {
        const FactoryWaterExtractor *machine =
            &simulation->water_extractors.items[index];
        const FactoryFluidStorage *storage =
            factory_fluid_storage_store_find_slot(
                &simulation->fluid_storages, machine->entity_id,
                FACTORY_FLUID_STORAGE_DEFAULT);
        if (machine->progress >= FACTORY_WATER_EXTRACTOR_CYCLE_TICKS
            || storage == NULL
            || storage->capacity != FACTORY_WATER_EXTRACTOR_CAPACITY
            || storage->accepted_fluid_classes != FACTORY_FLUID_CLASS_AQUEOUS)
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    }
    for (index = 0U; index < simulation->boilers.count; ++index) {
        const FactoryBoiler *machine = &simulation->boilers.items[index];
        const FactoryFluidStorage *input =
            factory_fluid_storage_store_find_slot(
                &simulation->fluid_storages, machine->entity_id,
                FACTORY_FLUID_STORAGE_BOILER_INPUT);
        const FactoryFluidStorage *output =
            factory_fluid_storage_store_find_slot(
                &simulation->fluid_storages, machine->entity_id,
                FACTORY_FLUID_STORAGE_BOILER_OUTPUT);
        const FactoryBurner *burner =
            factory_burner_store_find(&simulation->burners,
                                      machine->entity_id);
        const FactoryFuelDefinition *inventory_definition =
            burner == NULL ? NULL
                : factory_fuel_definition_get(burner->inventory_item);
        const FactoryFuelDefinition *current_definition =
            burner == NULL ? NULL
                : factory_fuel_definition_get(burner->current_fuel_item);
        if (machine->recipe_id != FACTORY_FLUID_RECIPE_BOIL_WATER
            || input == NULL || output == NULL || burner == NULL
            || input->capacity != FACTORY_BOILER_STORAGE_CAPACITY
            || output->capacity != FACTORY_BOILER_STORAGE_CAPACITY
            || input->accepted_fluid_classes != FACTORY_FLUID_CLASS_AQUEOUS
            || output->accepted_fluid_classes != FACTORY_FLUID_CLASS_VAPOR
            || burner->accepted_fuel_classes != FACTORY_FUEL_CLASS_SOLID
            || (burner->inventory_quantity == 0U)
                != (burner->inventory_item == FACTORY_ITEM_NONE)
            || (burner->remaining_burn_ticks == 0U)
                != (burner->current_fuel_item == FACTORY_ITEM_NONE)
            || (burner->inventory_quantity != 0U
                && (inventory_definition == NULL
                    || (inventory_definition->fuel_class
                        & burner->accepted_fuel_classes) == 0U))
            || (burner->remaining_burn_ticks != 0U
                && (current_definition == NULL
                    || burner->remaining_burn_ticks
                        >= current_definition->burn_duration_ticks))
            || burner->released_energy
                > FACTORY_BURNER_RELEASED_ENERGY_CAPACITY
            || (burner->remaining_burn_ticks != 0U
                && factory_burner_unreleased_energy(burner)
                    > FACTORY_BURNER_RELEASED_ENERGY_CAPACITY
                        - burner->released_energy))
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    }
    for (index = 0U; index < simulation->fluid_storages.count; ++index) {
        const FactoryFluidStorage *storage =
            &simulation->fluid_storages.items[index];
        const FactoryFluidDefinition *definition =
            factory_fluid_definition_get(storage->fluid_type);
        const FactoryWaterExtractor *water_extractor =
            factory_water_extractor_store_find(
                &simulation->water_extractors, storage->owner_entity_id);
        const FactoryBoiler *boiler = factory_boiler_store_find(
            &simulation->boilers, storage->owner_entity_id);
        const FactorySteamEngine *steam_engine =
            factory_steam_engine_store_find(
                &simulation->steam_engines, storage->owner_entity_id);
        bool storage_shape_valid =
            (water_extractor != NULL
                && storage->slot == FACTORY_FLUID_STORAGE_DEFAULT
                && storage->capacity == FACTORY_WATER_EXTRACTOR_CAPACITY
                && storage->accepted_fluid_classes
                    == FACTORY_FLUID_CLASS_AQUEOUS)
            || (boiler != NULL
                && storage->slot == FACTORY_FLUID_STORAGE_BOILER_INPUT
                && storage->capacity == FACTORY_BOILER_STORAGE_CAPACITY
                && storage->accepted_fluid_classes
                    == FACTORY_FLUID_CLASS_AQUEOUS)
            || (boiler != NULL
                && storage->slot == FACTORY_FLUID_STORAGE_BOILER_OUTPUT
                && storage->capacity == FACTORY_BOILER_STORAGE_CAPACITY
                && storage->accepted_fluid_classes
                    == FACTORY_FLUID_CLASS_VAPOR)
            || (water_extractor == NULL && boiler == NULL
                && steam_engine != NULL
                && storage->slot
                    == FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT
                && storage->capacity == FACTORY_STEAM_ENGINE_STORAGE_CAPACITY
                && storage->accepted_fluid_classes
                    == FACTORY_FLUID_CLASS_VAPOR)
            || (water_extractor == NULL && boiler == NULL
                && steam_engine == NULL
                && storage->slot == FACTORY_FLUID_STORAGE_DEFAULT
                && storage->capacity == FACTORY_FLUID_TANK_CAPACITY
                && storage->accepted_fluid_classes
                    == (FACTORY_FLUID_CLASS_AQUEOUS
                        | FACTORY_FLUID_CLASS_VAPOR));
        if (!storage_shape_valid
            || storage->quantity > storage->capacity
            || (storage->quantity == 0U)
                != (storage->fluid_type == FACTORY_FLUID_NONE)
            || (storage->quantity != 0U
                && (definition == NULL
                    || (definition->fluid_class
                        & storage->accepted_fluid_classes) == 0U)))
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        for (size_t other = index + 1U;
             other < simulation->fluid_storages.count; ++other)
            if (storage->owner_entity_id
                    == simulation->fluid_storages.items[other].owner_entity_id
                && storage->slot
                    == simulation->fluid_storages.items[other].slot)
                return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    }
    for (index = 0U; index < simulation->command_count; ++index) {
        if (!snapshot_command_valid(&simulation->commands[index])) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
    }
    for (index = 0U; index < simulation->result_count; ++index) {
        const FactoryCommandResult *value = &simulation->results[index];
        if (!snapshot_command_valid(&value->command)
            || value->result > FACTORY_RESULT_FLUID_NETWORK_NOT_FOUND
            || value->entity_type > FACTORY_ENTITY_TYPE_SOLAR_GENERATOR
            || value->previous_assembler_recipe
                >= FACTORY_ASSEMBLER_RECIPE_COUNT
            || value->new_assembler_recipe
                >= FACTORY_ASSEMBLER_RECIPE_COUNT
            || !item_valid_or_none(value->previous_storage_output)
            || !item_valid_or_none(value->new_storage_output)) {
            return FACTORY_RESULT_SNAPSHOT_CORRUPT;
        }
    }
    return FACTORY_RESULT_OK;
}

static void write_command(
    SnapshotWriter *writer,
    const FactoryCommand *command
)
{
    uint32_t fields[5] = {0U, 0U, 0U, 0U, 0U};
    size_t index;

    switch (command->type) {
        case FACTORY_COMMAND_PLACE_EXTRACTOR:
            fields[0] = (uint32_t)command->data.place_extractor.x;
            fields[1] = (uint32_t)command->data.place_extractor.y;
            fields[2] = command->data.place_extractor.output_direction;
            break;
        case FACTORY_COMMAND_PLACE_BELT:
            fields[0] = (uint32_t)command->data.place_belt.x;
            fields[1] = (uint32_t)command->data.place_belt.y;
            fields[2] = command->data.place_belt.direction;
            break;
        case FACTORY_COMMAND_PLACE_STORAGE:
            fields[0] = (uint32_t)command->data.place_storage.x;
            fields[1] = (uint32_t)command->data.place_storage.y;
            break;
        case FACTORY_COMMAND_PLACE_REFINERY:
            fields[0] = (uint32_t)command->data.place_refinery.x;
            fields[1] = (uint32_t)command->data.place_refinery.y;
            fields[2] = command->data.place_refinery.input_direction;
            fields[3] = command->data.place_refinery.output_direction;
            break;
        case FACTORY_COMMAND_SET_REFINERY_RECIPE:
            fields[0] = command->data.set_refinery_recipe.refinery_entity;
            fields[1] = command->data.set_refinery_recipe.recipe_id;
            break;
        case FACTORY_COMMAND_PLACE_ASSEMBLER:
            fields[0] = (uint32_t)command->data.place_assembler.x;
            fields[1] = (uint32_t)command->data.place_assembler.y;
            fields[2] = command->data.place_assembler.output_direction;
            break;
        case FACTORY_COMMAND_DEMOLISH_ENTITY:
            fields[0] = command->data.demolish_entity.entity_id;
            break;
        case FACTORY_COMMAND_PLACE_SPLITTER:
            fields[0] = (uint32_t)command->data.place_splitter.x;
            fields[1] = (uint32_t)command->data.place_splitter.y;
            fields[2] = command->data.place_splitter.facing;
            break;
        case FACTORY_COMMAND_PLACE_INSERTER:
            fields[0] = (uint32_t)command->data.place_inserter.x;
            fields[1] = (uint32_t)command->data.place_inserter.y;
            fields[2] = command->data.place_inserter.facing;
            break;
        case FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS:
            fields[0] = command->data.grant_construction_units.amount;
            break;
        case FACTORY_COMMAND_SET_ASSEMBLER_RECIPE:
            fields[0] = command->data.set_assembler_recipe.assembler_entity;
            fields[1] = command->data.set_assembler_recipe.recipe_id;
            break;
        case FACTORY_COMMAND_SET_STORAGE_OUTPUT:
            fields[0] = command->data.set_storage_output.storage_entity;
            fields[1] = command->data.set_storage_output.item;
            break;
        case FACTORY_COMMAND_PLACE_POWER_POLE:
            fields[0] = (uint32_t)command->data.place_power_pole.x;
            fields[1] = (uint32_t)command->data.place_power_pole.y;
            break;
        case FACTORY_COMMAND_PLACE_POWER_GENERATOR:
            fields[0] = (uint32_t)command->data.place_power_generator.x;
            fields[1] = (uint32_t)command->data.place_power_generator.y;
            break;
        case FACTORY_COMMAND_PLACE_FLUID_TANK:
            fields[0] = (uint32_t)command->data.place_fluid_tank.x;
            fields[1] = (uint32_t)command->data.place_fluid_tank.y;
            break;
        case FACTORY_COMMAND_PLACE_PIPE:
            fields[0] = (uint32_t)command->data.place_pipe.x;
            fields[1] = (uint32_t)command->data.place_pipe.y;
            break;
        case FACTORY_COMMAND_FLUID_INSERT:
            fields[0] =
                command->data.fluid_insert.destination_entity_id;
            fields[1] = command->data.fluid_insert.fluid_type;
            fields[2] = command->data.fluid_insert.quantity;
            break;
        case FACTORY_COMMAND_FLUID_REMOVE:
            fields[0] = command->data.fluid_remove.source_entity_id;
            fields[1] = command->data.fluid_remove.quantity;
            break;
        case FACTORY_COMMAND_FLUID_TRANSFER:
            fields[0] = command->data.fluid_transfer.source_entity_id;
            fields[1] =
                command->data.fluid_transfer.destination_entity_id;
            fields[2] = command->data.fluid_transfer.quantity;
            break;
        case FACTORY_COMMAND_PLACE_WATER_EXTRACTOR:
            fields[0] = (uint32_t)command->data.place_water_extractor.x;
            fields[1] = (uint32_t)command->data.place_water_extractor.y;
            break;
        case FACTORY_COMMAND_PLACE_BOILER:
            fields[0] = (uint32_t)command->data.place_boiler.x;
            fields[1] = (uint32_t)command->data.place_boiler.y;
            break;
        case FACTORY_COMMAND_PLACE_STEAM_ENGINE:
            fields[0] = (uint32_t)command->data.place_steam_engine.x;
            fields[1] = (uint32_t)command->data.place_steam_engine.y;
            break;
        case FACTORY_COMMAND_PLACE_SOLAR_GENERATOR:
            fields[0] = (uint32_t)command->data.place_solar_generator.x;
            fields[1] = (uint32_t)command->data.place_solar_generator.y;
            break;
    }
    write_u32(writer, (uint32_t)command->type);
    for (index = 0U; index < 5U; ++index) {
        write_u32(writer, fields[index]);
    }
}

static bool read_command(SnapshotReader *reader, FactoryCommand *command)
{
    uint32_t type;
    uint32_t fields[5];
    size_t index;

    (void)memset(command, 0, sizeof(*command));
    if (!read_u32(reader, &type)) {
        return false;
    }
    for (index = 0U; index < 5U; ++index) {
        if (!read_u32(reader, &fields[index])) {
            return false;
        }
    }
    if (type > FACTORY_COMMAND_PLACE_SOLAR_GENERATOR) {
        return false;
    }
    command->type = (FactoryCommandType)type;
    {
        size_t used = 0U;
        switch (command->type) {
            case FACTORY_COMMAND_PLACE_REFINERY:
                used = 4U;
                break;
            case FACTORY_COMMAND_PLACE_EXTRACTOR:
            case FACTORY_COMMAND_PLACE_BELT:
            case FACTORY_COMMAND_PLACE_ASSEMBLER:
            case FACTORY_COMMAND_PLACE_SPLITTER:
            case FACTORY_COMMAND_PLACE_INSERTER:
            case FACTORY_COMMAND_FLUID_INSERT:
            case FACTORY_COMMAND_FLUID_TRANSFER:
                used = 3U;
                break;
            case FACTORY_COMMAND_PLACE_STORAGE:
            case FACTORY_COMMAND_SET_REFINERY_RECIPE:
            case FACTORY_COMMAND_SET_ASSEMBLER_RECIPE:
            case FACTORY_COMMAND_SET_STORAGE_OUTPUT:
            case FACTORY_COMMAND_PLACE_POWER_POLE:
            case FACTORY_COMMAND_PLACE_POWER_GENERATOR:
            case FACTORY_COMMAND_PLACE_FLUID_TANK:
            case FACTORY_COMMAND_PLACE_PIPE:
            case FACTORY_COMMAND_FLUID_REMOVE:
            case FACTORY_COMMAND_PLACE_WATER_EXTRACTOR:
            case FACTORY_COMMAND_PLACE_BOILER:
            case FACTORY_COMMAND_PLACE_STEAM_ENGINE:
            case FACTORY_COMMAND_PLACE_SOLAR_GENERATOR:
                used = 2U;
                break;
            case FACTORY_COMMAND_DEMOLISH_ENTITY:
            case FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS:
                used = 1U;
                break;
        }
        for (index = used; index < 5U; ++index) {
            if (fields[index] != 0U) {
                return false;
            }
        }
    }
    switch (command->type) {
        case FACTORY_COMMAND_PLACE_EXTRACTOR:
            command->data.place_extractor.x = (int32_t)fields[0];
            command->data.place_extractor.y = (int32_t)fields[1];
            command->data.place_extractor.output_direction =
                (FactoryDirection)fields[2];
            break;
        case FACTORY_COMMAND_PLACE_BELT:
            command->data.place_belt.x = (int32_t)fields[0];
            command->data.place_belt.y = (int32_t)fields[1];
            command->data.place_belt.direction =
                (FactoryDirection)fields[2];
            break;
        case FACTORY_COMMAND_PLACE_STORAGE:
            command->data.place_storage.x = (int32_t)fields[0];
            command->data.place_storage.y = (int32_t)fields[1];
            break;
        case FACTORY_COMMAND_PLACE_REFINERY:
            command->data.place_refinery.x = (int32_t)fields[0];
            command->data.place_refinery.y = (int32_t)fields[1];
            command->data.place_refinery.input_direction =
                (FactoryDirection)fields[2];
            command->data.place_refinery.output_direction =
                (FactoryDirection)fields[3];
            break;
        case FACTORY_COMMAND_SET_REFINERY_RECIPE:
            command->data.set_refinery_recipe.refinery_entity = fields[0];
            command->data.set_refinery_recipe.recipe_id =
                (FactoryRecipeId)fields[1];
            break;
        case FACTORY_COMMAND_PLACE_ASSEMBLER:
            command->data.place_assembler.x = (int32_t)fields[0];
            command->data.place_assembler.y = (int32_t)fields[1];
            command->data.place_assembler.output_direction =
                (FactoryDirection)fields[2];
            break;
        case FACTORY_COMMAND_DEMOLISH_ENTITY:
            command->data.demolish_entity.entity_id = fields[0];
            break;
        case FACTORY_COMMAND_PLACE_SPLITTER:
            command->data.place_splitter.x = (int32_t)fields[0];
            command->data.place_splitter.y = (int32_t)fields[1];
            command->data.place_splitter.facing =
                (FactoryDirection)fields[2];
            break;
        case FACTORY_COMMAND_PLACE_INSERTER:
            command->data.place_inserter.x = (int32_t)fields[0];
            command->data.place_inserter.y = (int32_t)fields[1];
            command->data.place_inserter.facing =
                (FactoryDirection)fields[2];
            break;
        case FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS:
            command->data.grant_construction_units.amount = fields[0];
            break;
        case FACTORY_COMMAND_SET_ASSEMBLER_RECIPE:
            command->data.set_assembler_recipe.assembler_entity = fields[0];
            command->data.set_assembler_recipe.recipe_id =
                (FactoryAssemblerRecipeId)fields[1];
            break;
        case FACTORY_COMMAND_SET_STORAGE_OUTPUT:
            command->data.set_storage_output.storage_entity = fields[0];
            command->data.set_storage_output.item =
                (FactoryItemType)fields[1];
            break;
        case FACTORY_COMMAND_PLACE_POWER_POLE:
            command->data.place_power_pole.x = (int32_t)fields[0];
            command->data.place_power_pole.y = (int32_t)fields[1];
            break;
        case FACTORY_COMMAND_PLACE_POWER_GENERATOR:
            command->data.place_power_generator.x = (int32_t)fields[0];
            command->data.place_power_generator.y = (int32_t)fields[1];
            break;
        case FACTORY_COMMAND_PLACE_FLUID_TANK:
            command->data.place_fluid_tank.x = (int32_t)fields[0];
            command->data.place_fluid_tank.y = (int32_t)fields[1];
            break;
        case FACTORY_COMMAND_PLACE_PIPE:
            command->data.place_pipe.x = (int32_t)fields[0];
            command->data.place_pipe.y = (int32_t)fields[1];
            break;
        case FACTORY_COMMAND_FLUID_INSERT:
            command->data.fluid_insert.destination_entity_id = fields[0];
            command->data.fluid_insert.fluid_type =
                (FactoryFluidType)fields[1];
            command->data.fluid_insert.quantity = fields[2];
            break;
        case FACTORY_COMMAND_FLUID_REMOVE:
            command->data.fluid_remove.source_entity_id = fields[0];
            command->data.fluid_remove.quantity = fields[1];
            break;
        case FACTORY_COMMAND_FLUID_TRANSFER:
            command->data.fluid_transfer.source_entity_id = fields[0];
            command->data.fluid_transfer.destination_entity_id = fields[1];
            command->data.fluid_transfer.quantity = fields[2];
            break;
        case FACTORY_COMMAND_PLACE_WATER_EXTRACTOR:
            command->data.place_water_extractor.x = (int32_t)fields[0];
            command->data.place_water_extractor.y = (int32_t)fields[1];
            break;
        case FACTORY_COMMAND_PLACE_BOILER:
            command->data.place_boiler.x = (int32_t)fields[0];
            command->data.place_boiler.y = (int32_t)fields[1];
            break;
        case FACTORY_COMMAND_PLACE_STEAM_ENGINE:
            command->data.place_steam_engine.x = (int32_t)fields[0];
            command->data.place_steam_engine.y = (int32_t)fields[1];
            break;
        case FACTORY_COMMAND_PLACE_SOLAR_GENERATOR:
            command->data.place_solar_generator.x = (int32_t)fields[0];
            command->data.place_solar_generator.y = (int32_t)fields[1];
            break;
    }
    return snapshot_command_valid(command);
}

static void write_snapshot(
    const FactorySimulation *simulation,
    SnapshotWriter *writer,
    size_t total_size
)
{
    size_t index;
    size_t tile_count = (size_t)simulation->world->width
        * (size_t)simulation->world->height;

    write_bytes(writer, snapshot_magic, sizeof(snapshot_magic));
    write_u32(writer, FACTORY_SNAPSHOT_VERSION);
    write_u32(writer, SNAPSHOT_HEADER_SIZE);
    write_u64(writer, total_size);
    write_u64(writer, total_size - SNAPSHOT_HEADER_SIZE);
    write_u64(writer, simulation->clock.tick);
    write_u32(writer, SNAPSHOT_SECTION_COUNT);
    write_u32(writer, 0U);

    write_section_header(writer, SNAPSHOT_SECTION_METADATA, 1U, 16U);
    write_u64(writer, simulation->clock.tick);
    write_u32(writer, simulation->construction_inventory.units);
    write_u32(writer, 0U);

    write_section_header(
        writer, SNAPSHOT_SECTION_ENTITIES,
        simulation->entities->count,
        8U + simulation->entities->count * 4U
    );
    write_u32(writer, simulation->entities->next_id);
    write_u32(writer, 0U);
    for (index = 0U; index < simulation->entities->count; ++index) {
        write_u32(writer, simulation->entities->live_ids[index]);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_WORLD, tile_count, 8U + tile_count * 16U
    );
    write_u32(writer, simulation->world->width);
    write_u32(writer, simulation->world->height);
    for (index = 0U; index < tile_count; ++index) {
        const FactoryTile *value = &simulation->world->tiles[index];
        write_u32(writer, value->terrain);
        write_u32(writer, value->resource);
        write_u32(writer, value->resource_amount);
        write_u32(writer, value->occupying_entity);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_EXTRACTORS,
        simulation->extractors.count, simulation->extractors.count * 36U
    );
    for (index = 0U; index < simulation->extractors.count; ++index) {
        const FactoryExtractor *value = &simulation->extractors.items[index];
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x); write_i32(writer, value->y);
        write_u32(writer, value->resource_type);
        write_u32(writer, value->produced_item);
        write_u32(writer, value->output_direction);
        write_u32(writer, value->production_progress);
        write_u32(writer, value->output_item);
        write_u32(writer, value->output_amount);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_BELTS,
        simulation->belts.count, simulation->belts.count * 24U
    );
    for (index = 0U; index < simulation->belts.count; ++index) {
        const FactoryBelt *value = &simulation->belts.items[index];
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x); write_i32(writer, value->y);
        write_u32(writer, value->direction);
        write_u32(writer, value->item);
        write_u32(writer, value->movement_progress);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_SPLITTERS,
        simulation->splitters.count, simulation->splitters.count * 24U
    );
    for (index = 0U; index < simulation->splitters.count; ++index) {
        const FactorySplitter *value = &simulation->splitters.items[index];
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x); write_i32(writer, value->y);
        write_u32(writer, value->facing);
        write_u32(writer, value->item);
        write_u32(writer, value->next_output);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_REFINERIES,
        simulation->refineries.count, simulation->refineries.count * 48U
    );
    for (index = 0U; index < simulation->refineries.count; ++index) {
        const FactoryRefinery *value = &simulation->refineries.items[index];
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x); write_i32(writer, value->y);
        write_u32(writer, value->input_direction);
        write_u32(writer, value->output_direction);
        write_u32(writer, value->recipe_id);
        write_u32(writer, value->input_item);
        write_u32(writer, value->input_amount);
        write_u32(writer, value->output_item);
        write_u32(writer, value->output_amount);
        write_u32(writer, value->processing_progress);
        write_u32(writer, value->processing ? 1U : 0U);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_ASSEMBLERS,
        simulation->assemblers.count, simulation->assemblers.count * 64U
    );
    for (index = 0U; index < simulation->assemblers.count; ++index) {
        const FactoryAssembler *value = &simulation->assemblers.items[index];
        size_t slot;
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x); write_i32(writer, value->y);
        write_u32(writer, value->output_direction);
        write_u32(writer, value->recipe_id);
        for (slot = 0U; slot < 2U; ++slot) {
            write_u32(writer, value->input_slots[slot].item);
            write_u32(writer, value->input_slots[slot].count);
            write_u32(writer, value->input_slots[slot].capacity);
        }
        write_u32(writer, value->output_item);
        write_u32(writer, value->output_amount);
        write_u32(writer, value->processing_progress);
        write_u32(writer, value->processing_duration);
        write_u32(writer, value->processing ? 1U : 0U);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_INSERTERS,
        simulation->inserters.count, simulation->inserters.count * 48U
    );
    for (index = 0U; index < simulation->inserters.count; ++index) {
        const FactoryInserter *value = &simulation->inserters.items[index];
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x); write_i32(writer, value->y);
        write_u32(writer, value->facing);
        write_u32(writer, value->held_item);
        write_u32(writer, value->held_amount);
        write_u32(writer, value->state);
        write_u32(writer, value->progress);
        write_i32(writer, value->source_x);
        write_i32(writer, value->source_y);
        write_i32(writer, value->destination_x);
        write_i32(writer, value->destination_y);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_STORAGES,
        simulation->storages.count, simulation->storages.count * 60U
    );
    for (index = 0U; index < simulation->storages.count; ++index) {
        const FactoryStorage *value = &simulation->storages.items[index];
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x); write_i32(writer, value->y);
        write_u32(writer, value->iron_ore_amount);
        write_u32(writer, value->iron_plate_amount);
        write_u32(writer, value->copper_ore_amount);
        write_u32(writer, value->copper_plate_amount);
        write_u32(writer, value->electronic_component_amount);
        write_u32(writer, value->iron_gear_amount);
        write_u32(writer, value->copper_wire_amount);
        write_u32(writer, value->biomass_pellet_amount);
        write_u32(writer, value->total_capacity);
        write_u32(writer, value->configured_output_item);
        write_u32(writer, value->output_item);
        write_u32(writer, value->output_occupied ? 1U : 0U);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_POWER_POLES,
        simulation->power_poles.count, simulation->power_poles.count * 12U
    );
    for (index = 0U; index < simulation->power_poles.count; ++index) {
        const FactoryPowerPole *value = NULL;
        size_t candidate;
        for (candidate = 0U;
            candidate < simulation->power_poles.count;
            ++candidate) {
            const FactoryPowerPole *item =
                &simulation->power_poles.items[candidate];
            size_t smaller = 0U;
            size_t other;
            for (other = 0U;
                other < simulation->power_poles.count;
                ++other) {
                if (simulation->power_poles.items[other].entity_id
                    < item->entity_id) ++smaller;
            }
            if (smaller == index) value = item;
        }
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x);
        write_i32(writer, value->y);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_POWER_GENERATORS,
        simulation->power_generators.count,
        simulation->power_generators.count * 44U
    );
    for (index = 0U; index < simulation->power_generators.count; ++index) {
        const FactoryPowerGenerator *value = NULL;
        size_t candidate;
        for (candidate = 0U;
            candidate < simulation->power_generators.count;
            ++candidate) {
            const FactoryPowerGenerator *item =
                &simulation->power_generators.items[candidate];
            size_t smaller = 0U;
            size_t other;
            for (other = 0U;
                other < simulation->power_generators.count;
                ++other) {
                if (simulation->power_generators.items[other].entity_id
                    < item->entity_id) ++smaller;
            }
            if (smaller == index) value = item;
        }
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x);
        write_i32(writer, value->y);
        write_u32(writer, value->generation_capacity);
        {
            const FactoryBurner *burner =
                factory_burner_store_find(&simulation->burners,
                                          value->entity_id);
            write_u32(writer,
                burner == NULL ? 0U : burner->accepted_fuel_classes);
            write_u32(writer,
                burner == NULL ? 0U : burner->inventory_item);
            write_u32(writer,
                burner == NULL ? 0U : burner->inventory_quantity);
            write_u32(writer,
                burner == NULL ? 0U : burner->current_fuel_item);
            write_u32(writer,
                burner == NULL ? 0U : burner->remaining_burn_ticks);
            write_u64(writer,
                burner == NULL ? 0U : burner->released_energy);
        }
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_FLUID_STORAGES,
        simulation->fluid_storages.count,
        simulation->fluid_storages.count * 32U
    );
    for (index = 0U; index < simulation->fluid_storages.count; ++index) {
        const FactoryFluidStorage *value =
            &simulation->fluid_storages.items[index];
        write_u32(writer, value->owner_entity_id);
        write_u32(writer, value->slot);
        write_i32(writer, value->x);
        write_i32(writer, value->y);
        write_u32(writer, value->accepted_fluid_classes);
        write_u32(writer, value->fluid_type);
        write_u32(writer, value->quantity);
        write_u32(writer, value->capacity);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_PIPES,
        simulation->pipes.count, simulation->pipes.count * 12U
    );
    for (index = 0U; index < simulation->pipes.count; ++index) {
        const FactoryPipe *value = &simulation->pipes.items[index];
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x);
        write_i32(writer, value->y);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_WATER_EXTRACTORS,
        simulation->water_extractors.count,
        simulation->water_extractors.count * 16U
    );
    for (index = 0U; index < simulation->water_extractors.count; ++index) {
        const FactoryWaterExtractor *value =
            &simulation->water_extractors.items[index];
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x);
        write_i32(writer, value->y);
        write_u32(writer, value->progress);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_BOILERS,
        simulation->boilers.count, simulation->boilers.count * 48U
    );
    for (index = 0U; index < simulation->boilers.count; ++index) {
        const FactoryBoiler *value = &simulation->boilers.items[index];
        const FactoryBurner *burner = factory_burner_store_find(
            &simulation->burners, value->entity_id);
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x);
        write_i32(writer, value->y);
        write_u32(writer, value->recipe_id);
        write_u32(writer, value->conversion_active ? 1U : 0U);
        write_u32(writer, burner->accepted_fuel_classes);
        write_u32(writer, burner->inventory_item);
        write_u32(writer, burner->inventory_quantity);
        write_u32(writer, burner->current_fuel_item);
        write_u32(writer, burner->remaining_burn_ticks);
        write_u64(writer, burner->released_energy);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_STEAM_ENGINES,
        simulation->steam_engines.count,
        simulation->steam_engines.count * 16U
    );
    for (index = 0U; index < simulation->steam_engines.count; ++index) {
        const FactorySteamEngine *value =
            &simulation->steam_engines.items[index];
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x);
        write_i32(writer, value->y);
        write_u32(writer, value->recipe_id);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_SOLAR_GENERATORS,
        simulation->solar_generators.count,
        simulation->solar_generators.count * 12U
    );
    for (index = 0U; index < simulation->solar_generators.count; ++index) {
        const FactorySolarGenerator *value =
            &simulation->solar_generators.items[index];
        write_u32(writer, value->entity_id);
        write_i32(writer, value->x);
        write_i32(writer, value->y);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_COMMANDS,
        simulation->command_count, simulation->command_count * 24U
    );
    for (index = 0U; index < simulation->command_count; ++index) {
        write_command(writer, &simulation->commands[index]);
    }

    write_section_header(
        writer, SNAPSHOT_SECTION_RESULTS,
        simulation->result_count, simulation->result_count * 68U
    );
    for (index = 0U; index < simulation->result_count; ++index) {
        const FactoryCommandResult *value = &simulation->results[index];
        write_command(writer, &value->command);
        write_u32(writer, value->result);
        write_u32(writer, value->entity_id);
        write_u32(writer, value->entity_type);
        write_i32(writer, value->x); write_i32(writer, value->y);
        write_u32(writer, value->construction_units_changed);
        write_u32(writer, value->construction_units_remaining);
        write_u32(writer, value->previous_assembler_recipe);
        write_u32(writer, value->new_assembler_recipe);
        write_u32(writer, value->previous_storage_output);
        write_u32(writer, value->new_storage_output);
    }
}

FactoryResult factory_simulation_snapshot_size(
    const FactorySimulation *simulation,
    size_t *out_size
)
{
    FactoryResult result;

    if (simulation == NULL || out_size == NULL) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    result = validate_simulation(simulation);
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    return snapshot_size_unvalidated(simulation, out_size);
}

FactoryResult factory_simulation_save_snapshot(
    const FactorySimulation *simulation,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *out_written
)
{
    SnapshotWriter writer = {buffer, buffer_size, 0U, false};
    size_t required;
    FactoryResult result;

    if (simulation == NULL || out_written == NULL
        || (buffer == NULL && buffer_size != 0U)) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    *out_written = 0U;
    result = factory_simulation_snapshot_size(simulation, &required);
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    if (buffer == NULL || buffer_size < required) {
        return FACTORY_RESULT_SNAPSHOT_BUFFER_TOO_SMALL;
    }
    write_snapshot(simulation, &writer, required);
    if (writer.failed || writer.offset != required) {
        return FACTORY_RESULT_SNAPSHOT_SIZE_OVERFLOW;
    }
    *out_written = writer.offset;
    return FACTORY_RESULT_OK;
}

FactoryResult factory_simulation_create_snapshot(
    const FactorySimulation *simulation,
    FactorySnapshotBuffer *out_snapshot
)
{
    size_t size;
    size_t written;
    FactoryResult result;

    if (out_snapshot == NULL) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    out_snapshot->data = NULL;
    out_snapshot->size = 0U;
    result = factory_simulation_snapshot_size(simulation, &size);
    if (result != FACTORY_RESULT_OK) {
        return result;
    }
    out_snapshot->data = malloc(size);
    if (out_snapshot->data == NULL) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    result = factory_simulation_save_snapshot(
        simulation, out_snapshot->data, size, &written
    );
    if (result != FACTORY_RESULT_OK) {
        factory_snapshot_buffer_destroy(out_snapshot);
        return result;
    }
    out_snapshot->size = written;
    return FACTORY_RESULT_OK;
}

void factory_snapshot_buffer_destroy(FactorySnapshotBuffer *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    free(snapshot->data);
    snapshot->data = NULL;
    snapshot->size = 0U;
}

static bool read_section_header(
    SnapshotReader *reader,
    SnapshotSection expected,
    size_t record_size,
    size_t prefix_size,
    uint32_t *out_count
)
{
    uint32_t type;
    uint32_t version;
    uint32_t count;
    uint32_t payload;
    uint64_t expected_payload;

    if (!read_u32(reader, &type)
        || !read_u32(reader, &version)
        || !read_u32(reader, &count)
        || !read_u32(reader, &payload)) {
        return false;
    }
    expected_payload = (uint64_t)prefix_size
        + (uint64_t)count * (uint64_t)record_size;
    if (type != (uint32_t)expected || version != 1U
        || expected_payload > UINT32_MAX
        || payload != (uint32_t)expected_payload) {
        return false;
    }
    if (payload > reader->size - reader->offset) {
        reader->truncated = true;
        return false;
    }
    *out_count = count;
    return true;
}

static bool allocate_records(
    void **out_items,
    size_t count,
    size_t record_size
)
{
    if (count == 0U) {
        *out_items = NULL;
        return true;
    }
    if (count > SIZE_MAX / record_size) {
        return false;
    }
    *out_items = calloc(count, record_size);
    return *out_items != NULL;
}

static bool read_bool32(SnapshotReader *reader, bool *out_value)
{
    uint32_t value;
    if (!read_u32(reader, &value) || value > 1U) {
        return false;
    }
    *out_value = value != 0U;
    return true;
}

static bool load_sections(
    SnapshotReader *reader,
    FactorySimulation *simulation
)
{
    uint32_t count;
    uint32_t value;
    uint32_t reserved;
    uint64_t tick;
    size_t index;

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_METADATA, 0U, 16U, &count)
        || count != 1U
        || !read_u64(reader, &tick)
        || !read_u32(reader, &simulation->construction_inventory.units)
        || !read_u32(reader, &reserved)
        || reserved != 0U) {
        return false;
    }
    factory_simulation_clock_set(&simulation->clock, tick);

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_ENTITIES, 4U, 8U, &count)
        || !read_u32(reader, &simulation->entities->next_id)
        || !read_u32(reader, &reserved)
        || reserved != 0U
        || !allocate_records(
            (void **)&simulation->entities->live_ids,
            count, sizeof(FactoryEntityId))) {
        return false;
    }
    simulation->entities->count = count;
    simulation->entities->capacity = count;
    for (index = 0U; index < count; ++index) {
        if (!read_u32(
                reader, &simulation->entities->live_ids[index])) {
            return false;
        }
    }

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_WORLD, 16U, 8U, &count)
        || !read_u32(reader, &simulation->world->width)
        || !read_u32(reader, &simulation->world->height)
        || simulation->world->width == 0U
        || simulation->world->height == 0U
        || (uint64_t)simulation->world->width
            * simulation->world->height != count
        || !allocate_records(
            (void **)&simulation->world->tiles,
            count, sizeof(FactoryTile))) {
        return false;
    }
    for (index = 0U; index < count; ++index) {
        FactoryTile *tile = &simulation->world->tiles[index];
        if (!read_u32(reader, &value)) return false;
        tile->terrain = (FactoryTerrainType)value;
        if (!read_u32(reader, &value)) return false;
        tile->resource = (FactoryResourceType)value;
        if (!read_u32(reader, &tile->resource_amount)
            || !read_u32(reader, &tile->occupying_entity)) return false;
    }

#define LOAD_STORE(SECTION, STORE, TYPE, RECORD_SIZE)                       \
    if (!read_section_header(                                               \
            reader, SECTION, RECORD_SIZE, 0U, &count)                       \
        || !allocate_records(                                               \
            (void **)&simulation->STORE.items, count, sizeof(TYPE))) {      \
        return false;                                                       \
    }                                                                       \
    simulation->STORE.count = count;                                        \
    simulation->STORE.capacity = count

    LOAD_STORE(SNAPSHOT_SECTION_EXTRACTORS, extractors, FactoryExtractor, 36U);
    for (index = 0U; index < count; ++index) {
        FactoryExtractor *v = &simulation->extractors.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x) || !read_i32(reader, &v->y)
            || !read_u32(reader, &value)) return false;
        v->resource_type = (FactoryResourceType)value;
        if (!read_u32(reader, &value)) return false;
        v->produced_item = (FactoryItemType)value;
        if (!read_u32(reader, &value)) return false;
        v->output_direction = (FactoryDirection)value;
        if (!read_u32(reader, &v->production_progress)
            || !read_u32(reader, &value)) return false;
        v->output_item = (FactoryItemType)value;
        if (!read_u32(reader, &v->output_amount)) return false;
    }

    LOAD_STORE(SNAPSHOT_SECTION_BELTS, belts, FactoryBelt, 24U);
    for (index = 0U; index < count; ++index) {
        FactoryBelt *v = &simulation->belts.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x) || !read_i32(reader, &v->y)
            || !read_u32(reader, &value)) return false;
        v->direction = (FactoryDirection)value;
        if (!read_u32(reader, &value)) return false;
        v->item = (FactoryItemType)value;
        if (!read_u32(reader, &v->movement_progress)) return false;
    }

    LOAD_STORE(SNAPSHOT_SECTION_SPLITTERS, splitters, FactorySplitter, 24U);
    for (index = 0U; index < count; ++index) {
        FactorySplitter *v = &simulation->splitters.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x) || !read_i32(reader, &v->y)
            || !read_u32(reader, &value)) return false;
        v->facing = (FactoryDirection)value;
        if (!read_u32(reader, &value)) return false;
        v->item = (FactoryItemType)value;
        if (!read_u32(reader, &value)) return false;
        v->next_output = (FactorySplitterOutput)value;
    }

    LOAD_STORE(SNAPSHOT_SECTION_REFINERIES, refineries, FactoryRefinery, 48U);
    for (index = 0U; index < count; ++index) {
        FactoryRefinery *v = &simulation->refineries.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x) || !read_i32(reader, &v->y)
            || !read_u32(reader, &value)) return false;
        v->input_direction = (FactoryDirection)value;
        if (!read_u32(reader, &value)) return false;
        v->output_direction = (FactoryDirection)value;
        if (!read_u32(reader, &value)) return false;
        v->recipe_id = (FactoryRecipeId)value;
        if (!read_u32(reader, &value)) return false;
        v->input_item = (FactoryItemType)value;
        if (!read_u32(reader, &v->input_amount)
            || !read_u32(reader, &value)) return false;
        v->output_item = (FactoryItemType)value;
        if (!read_u32(reader, &v->output_amount)
            || !read_u32(reader, &v->processing_progress)
            || !read_bool32(reader, &v->processing)) return false;
    }

    LOAD_STORE(SNAPSHOT_SECTION_ASSEMBLERS, assemblers, FactoryAssembler, 64U);
    for (index = 0U; index < count; ++index) {
        FactoryAssembler *v = &simulation->assemblers.items[index];
        size_t slot;
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x) || !read_i32(reader, &v->y)
            || !read_u32(reader, &value)) return false;
        v->output_direction = (FactoryDirection)value;
        if (!read_u32(reader, &value)) return false;
        v->recipe_id = (FactoryAssemblerRecipeId)value;
        for (slot = 0U; slot < 2U; ++slot) {
            if (!read_u32(reader, &value)) return false;
            v->input_slots[slot].item = (FactoryItemType)value;
            if (!read_u32(reader, &v->input_slots[slot].count)
                || !read_u32(
                    reader, &v->input_slots[slot].capacity)) return false;
        }
        if (!read_u32(reader, &value)) return false;
        v->output_item = (FactoryItemType)value;
        if (!read_u32(reader, &v->output_amount)
            || !read_u32(reader, &v->processing_progress)
            || !read_u32(reader, &v->processing_duration)
            || !read_bool32(reader, &v->processing)) return false;
    }

    LOAD_STORE(SNAPSHOT_SECTION_INSERTERS, inserters, FactoryInserter, 48U);
    for (index = 0U; index < count; ++index) {
        FactoryInserter *v = &simulation->inserters.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x) || !read_i32(reader, &v->y)
            || !read_u32(reader, &value)) return false;
        v->facing = (FactoryDirection)value;
        if (!read_u32(reader, &value)) return false;
        v->held_item = (FactoryItemType)value;
        if (!read_u32(reader, &v->held_amount)
            || !read_u32(reader, &value)) return false;
        v->state = (FactoryInserterState)value;
        if (!read_u32(reader, &v->progress)
            || !read_i32(reader, &v->source_x)
            || !read_i32(reader, &v->source_y)
            || !read_i32(reader, &v->destination_x)
            || !read_i32(reader, &v->destination_y)) return false;
    }

    LOAD_STORE(SNAPSHOT_SECTION_STORAGES, storages, FactoryStorage, 60U);
    for (index = 0U; index < count; ++index) {
        FactoryStorage *v = &simulation->storages.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x) || !read_i32(reader, &v->y)
            || !read_u32(reader, &v->iron_ore_amount)
            || !read_u32(reader, &v->iron_plate_amount)
            || !read_u32(reader, &v->copper_ore_amount)
            || !read_u32(reader, &v->copper_plate_amount)
            || !read_u32(reader, &v->electronic_component_amount)
            || !read_u32(reader, &v->iron_gear_amount)
            || !read_u32(reader, &v->copper_wire_amount)
            || !read_u32(reader, &v->biomass_pellet_amount)
            || !read_u32(reader, &v->total_capacity)
            || !read_u32(reader, &value)) return false;
        v->configured_output_item = (FactoryItemType)value;
        if (!read_u32(reader, &value)) return false;
        v->output_item = (FactoryItemType)value;
        if (!read_bool32(reader, &v->output_occupied)) return false;
    }
#undef LOAD_STORE

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_POWER_POLES, 12U, 0U, &count)
        || !allocate_records(
            (void **)&simulation->power_poles.items,
            count, sizeof(FactoryPowerPole))) {
        return false;
    }
    simulation->power_poles.count = count;
    simulation->power_poles.capacity = count;
    for (index = 0U; index < count; ++index) {
        FactoryPowerPole *v = &simulation->power_poles.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x)
            || !read_i32(reader, &v->y)) return false;
    }

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_POWER_GENERATORS, 44U, 0U, &count)
        || !allocate_records(
            (void **)&simulation->power_generators.items,
            count, sizeof(FactoryPowerGenerator))) {
        return false;
    }
    simulation->power_generators.count = count;
    simulation->power_generators.capacity = count;
    if (!allocate_records(
            (void **)&simulation->burners.items,
            count, sizeof(FactoryBurner))) return false;
    simulation->burners.count = 0U;
    simulation->burners.capacity = count;
    for (index = 0U; index < count; ++index) {
        FactoryPowerGenerator *v =
            &simulation->power_generators.items[index];
        FactoryBurner burner_value = {0};
        FactoryBurner *burner = &burner_value;
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x)
            || !read_i32(reader, &v->y)
            || !read_u32(reader, &v->generation_capacity)
            || !read_u32(reader, &burner->accepted_fuel_classes)
            || !read_u32(reader, &value)) return false;
        burner->owner_entity_id = v->entity_id;
        burner->inventory_item = (FactoryItemType)value;
        if (!read_u32(reader, &burner->inventory_quantity)
            || !read_u32(reader, &value)) return false;
        burner->current_fuel_item = (FactoryItemType)value;
        if (!read_u32(reader, &burner->remaining_burn_ticks)
            || !read_u64(reader, &burner->released_energy)) return false;
        if (burner->accepted_fuel_classes != 0U)
            simulation->burners.items[simulation->burners.count++] = *burner;
    }

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_FLUID_STORAGES, 32U, 0U, &count)
        || !allocate_records(
            (void **)&simulation->fluid_storages.items,
            count, sizeof(FactoryFluidStorage))) {
        return false;
    }
    simulation->fluid_storages.count = count;
    simulation->fluid_storages.capacity = count;
    for (index = 0U; index < count; ++index) {
        FactoryFluidStorage *v = &simulation->fluid_storages.items[index];
        if (!read_u32(reader, &v->owner_entity_id)
            || !read_u32(reader, &value)
            || !read_i32(reader, &v->x)
            || !read_i32(reader, &v->y)
            || !read_u32(reader, &v->accepted_fluid_classes)) return false;
        v->slot = (FactoryFluidStorageSlot)value;
        if (!read_u32(reader, &value)) return false;
        v->fluid_type = (FactoryFluidType)value;
        if (!read_u32(reader, &v->quantity)
            || !read_u32(reader, &v->capacity)) return false;
    }

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_PIPES, 12U, 0U, &count)
        || !allocate_records(
            (void **)&simulation->pipes.items,
            count, sizeof(FactoryPipe))) {
        return false;
    }
    simulation->pipes.count = count;
    simulation->pipes.capacity = count;
    for (index = 0U; index < count; ++index) {
        FactoryPipe *v = &simulation->pipes.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x)
            || !read_i32(reader, &v->y)) return false;
    }

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_WATER_EXTRACTORS, 16U, 0U, &count)
        || !allocate_records(
            (void **)&simulation->water_extractors.items,
            count, sizeof(FactoryWaterExtractor))) return false;
    simulation->water_extractors.count = count;
    simulation->water_extractors.capacity = count;
    for (index = 0U; index < count; ++index) {
        FactoryWaterExtractor *v = &simulation->water_extractors.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x)
            || !read_i32(reader, &v->y)
            || !read_u32(reader, &v->progress)) return false;
    }

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_BOILERS, 48U, 0U, &count)
        || !allocate_records(
            (void **)&simulation->boilers.items,
            count, sizeof(FactoryBoiler))) return false;
    simulation->boilers.count = count;
    simulation->boilers.capacity = count;
    if (count != 0U) {
        size_t burner_count = simulation->burners.count + count;
        FactoryBurner *resized;
        if (burner_count < count
            || burner_count > SIZE_MAX / sizeof(*resized)) return false;
        resized = realloc(simulation->burners.items,
                          burner_count * sizeof(*resized));
        if (resized == NULL) return false;
        simulation->burners.items = resized;
        simulation->burners.capacity = burner_count;
    }
    for (index = 0U; index < count; ++index) {
        FactoryBoiler *v = &simulation->boilers.items[index];
        FactoryBurner *burner =
            &simulation->burners.items[simulation->burners.count + index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x)
            || !read_i32(reader, &v->y)
            || !read_u32(reader, &value)) return false;
        v->recipe_id = (FactoryFluidRecipeId)value;
        if (!read_bool32(reader, &v->conversion_active)
            || !read_u32(reader, &burner->accepted_fuel_classes)
            || !read_u32(reader, &value)) return false;
        burner->owner_entity_id = v->entity_id;
        burner->inventory_item = (FactoryItemType)value;
        if (!read_u32(reader, &burner->inventory_quantity)
            || !read_u32(reader, &value)) return false;
        burner->current_fuel_item = (FactoryItemType)value;
        if (!read_u32(reader, &burner->remaining_burn_ticks)
            || !read_u64(reader, &burner->released_energy)) return false;
    }
    simulation->burners.count += count;

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_STEAM_ENGINES, 16U, 0U, &count)
        || !allocate_records(
            (void **)&simulation->steam_engines.items,
            count, sizeof(FactorySteamEngine))) return false;
    simulation->steam_engines.count = count;
    simulation->steam_engines.capacity = count;
    for (index = 0U; index < count; ++index) {
        FactorySteamEngine *v = &simulation->steam_engines.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x)
            || !read_i32(reader, &v->y)
            || !read_u32(reader, &value)) return false;
        v->recipe_id = (FactorySteamGenerationRecipeId)value;
        v->generated_last_tick = 0U;
    }

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_SOLAR_GENERATORS, 12U, 0U, &count)
        || !allocate_records(
            (void **)&simulation->solar_generators.items,
            count, sizeof(FactorySolarGenerator))) return false;
    simulation->solar_generators.count = count;
    simulation->solar_generators.capacity = count;
    for (index = 0U; index < count; ++index) {
        FactorySolarGenerator *v = &simulation->solar_generators.items[index];
        if (!read_u32(reader, &v->entity_id)
            || !read_i32(reader, &v->x)
            || !read_i32(reader, &v->y)) return false;
        v->generated_last_tick = 0U;
    }

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_COMMANDS, 24U, 0U, &count)
        || count > FACTORY_COMMAND_QUEUE_CAPACITY) {
        return false;
    }
    simulation->command_count = count;
    for (index = 0U; index < count; ++index) {
        if (!read_command(reader, &simulation->commands[index])) {
            return false;
        }
    }

    if (!read_section_header(
            reader, SNAPSHOT_SECTION_RESULTS, 68U, 0U, &count)
        || count > FACTORY_COMMAND_QUEUE_CAPACITY) {
        return false;
    }
    simulation->result_count = count;
    for (index = 0U; index < count; ++index) {
        FactoryCommandResult *v = &simulation->results[index];
        if (!read_command(reader, &v->command)
            || !read_u32(reader, &value)) return false;
        v->result = (FactoryResult)value;
        if (!read_u32(reader, &v->entity_id)
            || !read_u32(reader, &value)) return false;
        v->entity_type = (FactoryEntityType)value;
        if (!read_i32(reader, &v->x) || !read_i32(reader, &v->y)
            || !read_u32(reader, &v->construction_units_changed)
            || !read_u32(reader, &v->construction_units_remaining)
            || !read_u32(reader, &value)) return false;
        v->previous_assembler_recipe = (FactoryAssemblerRecipeId)value;
        if (!read_u32(reader, &value)) return false;
        v->new_assembler_recipe = (FactoryAssemblerRecipeId)value;
        if (!read_u32(reader, &value)) return false;
        v->previous_storage_output = (FactoryItemType)value;
        if (!read_u32(reader, &value)) return false;
        v->new_storage_output = (FactoryItemType)value;
    }
    return true;
}

FactoryResult factory_simulation_load_snapshot(
    const uint8_t *buffer,
    size_t buffer_size,
    FactorySimulation **out_simulation
)
{
    SnapshotReader reader = {buffer, buffer_size, 0U, false};
    uint8_t magic[8];
    uint32_t version;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t payload_size;
    uint64_t header_tick;
    uint32_t section_count;
    uint32_t reserved;
    FactoryWorld *world = NULL;
    FactorySimulation *simulation = NULL;
    FactoryResult result;

    if (out_simulation == NULL) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    *out_simulation = NULL;
    if (buffer == NULL || buffer_size == 0U) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    if (buffer_size < SNAPSHOT_HEADER_SIZE) {
        return FACTORY_RESULT_SNAPSHOT_TRUNCATED;
    }
    if (!read_bytes(&reader, magic, sizeof(magic))
        || memcmp(magic, snapshot_magic, sizeof(magic)) != 0) {
        return FACTORY_RESULT_SNAPSHOT_INVALID_MAGIC;
    }
    if (!read_u32(&reader, &version)) {
        return FACTORY_RESULT_SNAPSHOT_TRUNCATED;
    }
    if (version != FACTORY_SNAPSHOT_VERSION) {
        return FACTORY_RESULT_SNAPSHOT_UNSUPPORTED_VERSION;
    }
    if (!read_u32(&reader, &header_size)
        || !read_u64(&reader, &total_size)
        || !read_u64(&reader, &payload_size)
        || !read_u64(&reader, &header_tick)
        || !read_u32(&reader, &section_count)
        || !read_u32(&reader, &reserved)) {
        return FACTORY_RESULT_SNAPSHOT_TRUNCATED;
    }
    if (total_size > buffer_size) {
        return FACTORY_RESULT_SNAPSHOT_TRUNCATED;
    }
    if (header_size != SNAPSHOT_HEADER_SIZE
        || total_size != buffer_size
        || payload_size != total_size - SNAPSHOT_HEADER_SIZE
        || section_count != SNAPSHOT_SECTION_COUNT
        || reserved != 0U) {
        return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    }
    world = calloc(1U, sizeof(*world));
    if (world == NULL) {
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    simulation = factory_simulation_create_with_construction_units(world, 0U);
    if (simulation == NULL) {
        free(world);
        return FACTORY_RESULT_OUT_OF_MEMORY;
    }
    simulation->owns_world = true;
    if (!load_sections(&reader, simulation)) {
        result = reader.truncated
            ? FACTORY_RESULT_SNAPSHOT_TRUNCATED
            : FACTORY_RESULT_SNAPSHOT_CORRUPT;
        factory_simulation_destroy(simulation);
        return result;
    }
    if (reader.offset != reader.size || simulation->clock.tick != header_tick) {
        factory_simulation_destroy(simulation);
        return FACTORY_RESULT_SNAPSHOT_CORRUPT;
    }
    for (size_t i = 0U; i < simulation->fluid_storages.count; ++i) {
        const FactoryFluidStorage *storage =
            &simulation->fluid_storages.items[i];
        if (!factory_fluid_port_store_reserve_one(&simulation->fluid_ports)) {
            factory_simulation_destroy(simulation);
            return FACTORY_RESULT_OUT_OF_MEMORY;
        }
        factory_fluid_port_store_add(
            &simulation->fluid_ports, storage->owner_entity_id,
            storage->slot, storage->x, storage->y,
            storage->slot == FACTORY_FLUID_STORAGE_BOILER_INPUT
                ? FACTORY_FLUID_CONNECTION_WEST
                : storage->slot
                    == FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT
                    ? FACTORY_FLUID_CONNECTION_WEST
                : storage->slot == FACTORY_FLUID_STORAGE_BOILER_OUTPUT
                    ? FACTORY_FLUID_CONNECTION_EAST
                    : FACTORY_FLUID_CONNECTION_ALL,
            storage->accepted_fluid_classes);
    }
    result = validate_simulation(simulation);
    if (result != FACTORY_RESULT_OK) {
        factory_simulation_destroy(simulation);
        return result;
    }
    result = factory_power_rebuild(simulation, false);
    if (result != FACTORY_RESULT_OK) {
        factory_simulation_destroy(simulation);
        return result;
    }
    simulation->fluid_networks.dirty = true;
    result = factory_fluid_network_rebuild(simulation, false);
    if (result != FACTORY_RESULT_OK) {
        factory_simulation_destroy(simulation);
        return result;
    }
    *out_simulation = simulation;
    return FACTORY_RESULT_OK;
}
