#include <foundation/world.h>
#include <foundation/content.h>
#include "world_internal.h"

#include <stdint.h>
#include <stdlib.h>

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
    return factory_world_create_with_seed(
        width,height,FACTORY_WORLD_DEFAULT_SEED);
}

FactoryWorld *factory_world_create_with_seed(
    uint32_t width,uint32_t height,FactoryWorldSeed seed)
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

    world->seed = seed;
    world->width = width;
    world->height = height;
    world->sealed = false;
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

FactoryWorldSeed factory_world_get_seed(const FactoryWorld *world)
{
    return world == NULL ? 0U : world->seed;
}

bool factory_world_validate(const FactoryWorld *world)
{
    size_t count,index;
    if(world==NULL||world->width==0U||world->height==0U||world->tiles==NULL
        ||(size_t)world->width>SIZE_MAX/(size_t)world->height)return false;
    count=(size_t)world->width*(size_t)world->height;
    for(index=0U;index<count;++index){const FactoryTile *tile=&world->tiles[index];
        if(factory_content_terrain_definition_get(tile->terrain)==NULL
            ||tile->resource>FACTORY_RESOURCE_COAL
            ||(tile->resource==FACTORY_RESOURCE_NONE&&tile->resource_amount!=0U)
            ||(tile->resource!=FACTORY_RESOURCE_NONE
                &&!factory_content_terrain_allows_resource(
                    tile->terrain,tile->resource)))return false;
    }
    return true;
}

void factory_world_seal(FactoryWorld *world)
{
    if (world != NULL) world->sealed = true;
}

FactoryResult factory_world_initialize_terrain(FactoryWorld *world,
    int32_t x,int32_t y,FactoryTerrainType terrain)
{
    FactoryTile *tile;
    if (world==NULL||factory_content_terrain_definition_get(terrain)==NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    if (world->sealed) return FACTORY_RESULT_INVALID_STATE;
    tile=factory_world_get_mutable_tile(world,x,y);
    if(tile==NULL)return FACTORY_RESULT_OUT_OF_BOUNDS;
    if(tile->resource!=FACTORY_RESOURCE_NONE||tile->occupying_entity!=0U)
        return FACTORY_RESULT_TILE_OCCUPIED;
    tile->terrain=terrain;
    return FACTORY_RESULT_OK;
}

FactoryTerrainType factory_world_get_terrain(const FactoryWorld *world,
    int32_t x,int32_t y)
{
    const FactoryTile *tile=factory_world_get_tile(world,x,y);
    return tile==NULL?FACTORY_TERRAIN_NONE:tile->terrain;
}

FactoryResult factory_world_validate_buildable_footprint(
    const FactoryWorld *world,int32_t x,int32_t y,uint32_t width,uint32_t height)
{
    uint32_t dx,dy;
    if(world==NULL||width==0U||height==0U)return FACTORY_RESULT_INVALID_ARGUMENT;
    if(x<0||y<0||(uint64_t)(uint32_t)x+width>world->width
        ||(uint64_t)(uint32_t)y+height>world->height)
        return FACTORY_RESULT_OUT_OF_BOUNDS;
    for(dy=0U;dy<height;++dy)for(dx=0U;dx<width;++dx){
        const FactoryTile *tile=factory_world_get_tile(world,
            x+(int32_t)dx,y+(int32_t)dy);
        const FactoryTerrainDefinition *definition=
            factory_content_terrain_definition_get(tile->terrain);
        if(definition==NULL||!definition->buildable)
            return FACTORY_RESULT_TERRAIN_BLOCKED;
    }
    for(dy=0U;dy<height;++dy)for(dx=0U;dx<width;++dx)
        if(factory_world_get_tile(world,x+(int32_t)dx,y+(int32_t)dy)
            ->occupying_entity!=0U)return FACTORY_RESULT_TILE_OCCUPIED;
    return FACTORY_RESULT_OK;
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
            && resource != FACTORY_RESOURCE_COPPER
            && resource != FACTORY_RESOURCE_COAL)
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
    if (!factory_content_terrain_allows_resource(tile->terrain,resource)) {
        return FACTORY_RESULT_INVALID_ARGUMENT;
    }

    tile->resource = resource;
    tile->resource_amount = amount;
    return FACTORY_RESULT_OK;
}

FactoryResult factory_world_get_resource_deposit(const FactoryWorld *world,
    int32_t x,int32_t y,FactoryResourceDepositInspection *out)
{
    const FactoryTile *tile;
    if(world==NULL||out==NULL)return FACTORY_RESULT_INVALID_ARGUMENT;
    tile=factory_world_get_tile(world,x,y);
    if(tile==NULL)return FACTORY_RESULT_OUT_OF_BOUNDS;
    if(tile->resource==FACTORY_RESOURCE_NONE)return FACTORY_RESULT_NO_RESOURCE;
    *out=(FactoryResourceDepositInspection){x,y,tile->resource,
        tile->resource_amount,tile->occupying_entity,
        tile->resource_amount==0U};
    return FACTORY_RESULT_OK;
}

bool factory_resource_deposit_is_depleted(const FactoryWorld *world,
    int32_t x,int32_t y)
{
    FactoryResourceDepositInspection deposit;
    return factory_world_get_resource_deposit(world,x,y,&deposit)
        ==FACTORY_RESULT_OK&&deposit.depleted;
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
        case FACTORY_RESOURCE_COAL:
            return "coal";
        default:
            return "invalid resource";
    }
}
