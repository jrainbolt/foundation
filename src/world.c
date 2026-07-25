#include <foundation/world.h>

#include <stdint.h>
#include <stdlib.h>

struct FactoryWorld {
    uint32_t width;
    uint32_t height;
    FactoryTile *tiles;
};

static size_t factory_world_index(
    const FactoryWorld *world,
    uint32_t x,
    uint32_t y
)
{
    return ((size_t)y * (size_t)world->width) + (size_t)x;
}

static FactoryTile *factory_world_get_mutable_tile(
    FactoryWorld *world,
    int32_t x,
    int32_t y
)
{
    if (!factory_world_is_in_bounds(world, x, y)) {
        return NULL;
    }

    return &world->tiles[factory_world_index(
        world,
        (uint32_t)x,
        (uint32_t)y
    )];
}

FactoryWorld *factory_world_create(uint32_t width, uint32_t height)
{
    FactoryWorld *world = NULL;
    size_t tile_count = 0U;
    size_t index = 0U;

    if (width == 0U || height == 0U) {
        return NULL;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return NULL;
    }

    tile_count = (size_t)width * (size_t)height;
    if (tile_count > SIZE_MAX / sizeof(FactoryTile)) {
        return NULL;
    }

    world = malloc(sizeof(*world));
    if (world == NULL) {
        return NULL;
    }

    world->tiles = malloc(tile_count * sizeof(*world->tiles));
    if (world->tiles == NULL) {
        free(world);
        return NULL;
    }

    world->width = width;
    world->height = height;
    for (index = 0U; index < tile_count; ++index) {
        world->tiles[index].terrain = FACTORY_TERRAIN_GROUND;
        world->tiles[index].resource = FACTORY_RESOURCE_NONE;
        world->tiles[index].resource_amount = 0U;
        world->tiles[index].occupying_entity = 0U;
    }

    return world;
}

void factory_world_destroy(FactoryWorld *world)
{
    if (world == NULL) {
        return;
    }

    free(world->tiles);
    free(world);
}

uint32_t factory_world_get_width(const FactoryWorld *world)
{
    return world == NULL ? 0U : world->width;
}

uint32_t factory_world_get_height(const FactoryWorld *world)
{
    return world == NULL ? 0U : world->height;
}

bool factory_world_is_in_bounds(const FactoryWorld *world, int32_t x, int32_t y)
{
    return world != NULL
        && x >= 0
        && y >= 0
        && (uint32_t)x < world->width
        && (uint32_t)y < world->height;
}

const FactoryTile *factory_world_get_tile(
    const FactoryWorld *world,
    int32_t x,
    int32_t y
)
{
    if (!factory_world_is_in_bounds(world, x, y)) {
        return NULL;
    }

    return &world->tiles[factory_world_index(
        world,
        (uint32_t)x,
        (uint32_t)y
    )];
}

FactoryResult factory_world_add_resource(
    FactoryWorld *world,
    int32_t x,
    int32_t y,
    FactoryResourceType resource,
    uint32_t amount
)
{
    FactoryTile *tile = NULL;

    if (world == NULL
        || (resource != FACTORY_RESOURCE_IRON
            && resource != FACTORY_RESOURCE_COPPER)
        || amount == 0U) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    if (!factory_world_is_in_bounds(world, x, y)) {
        return FACTORY_RESULT_OUT_OF_BOUNDS;
    }

    tile = factory_world_get_mutable_tile(world, x, y);
    if (tile->resource != FACTORY_RESOURCE_NONE) {
        return FACTORY_RESULT_TILE_OCCUPIED;
    }
    if (tile->terrain != FACTORY_TERRAIN_GROUND) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }

    tile->resource = resource;
    tile->resource_amount = amount;
    return FACTORY_RESULT_OK;
}

FactoryResult factory_world_set_occupying_entity(
    FactoryWorld *world,
    int32_t x,
    int32_t y,
    FactoryEntityId entity_id
)
{
    FactoryTile *tile = factory_world_get_mutable_tile(world, x, y);

    if (tile == NULL || entity_id == 0U) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    if (tile->occupying_entity != 0U) {
        return FACTORY_RESULT_TILE_OCCUPIED;
    }
    tile->occupying_entity = entity_id;
    return FACTORY_RESULT_OK;
}

FactoryResult factory_world_consume_resource(
    FactoryWorld *world,
    int32_t x,
    int32_t y,
    uint32_t amount
)
{
    FactoryTile *tile = factory_world_get_mutable_tile(world, x, y);

    if (tile == NULL || amount == 0U || tile->resource_amount < amount) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }
    tile->resource_amount -= amount;
    return FACTORY_RESULT_OK;
}

FactoryResult factory_world_clear_occupying_entity(
    FactoryWorld *world,
    int32_t x,
    int32_t y,
    FactoryEntityId expected_entity_id
)
{
    FactoryTile *tile = factory_world_get_mutable_tile(world, x, y);

    if (tile == NULL || expected_entity_id == 0U
        || tile->occupying_entity != expected_entity_id) {
        return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    }
    tile->occupying_entity = 0U;
    return FACTORY_RESULT_OK;
}

const char *factory_resource_name(FactoryResourceType resource)
{
    switch (resource) {
        case FACTORY_RESOURCE_NONE:
            return "none";
        case FACTORY_RESOURCE_IRON:
            return "iron";
        case FACTORY_RESOURCE_COPPER:
            return "copper";
        default:
            return "invalid resource";
    }
}
