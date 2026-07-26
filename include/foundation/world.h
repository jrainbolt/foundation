#ifndef FOUNDATION_WORLD_H
#define FOUNDATION_WORLD_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/entity.h"

typedef enum {
    FACTORY_TERRAIN_GROUND = 0
} FactoryTerrainType;

typedef enum {
    FACTORY_RESOURCE_NONE = 0,
    FACTORY_RESOURCE_IRON,
    FACTORY_RESOURCE_COPPER
} FactoryResourceType;

typedef struct {
    FactoryTerrainType terrain;
    FactoryResourceType resource;
    uint32_t resource_amount;
    FactoryEntityId occupying_entity;
} FactoryTile;

typedef struct FactoryWorld FactoryWorld;

typedef enum {
    FACTORY_RESULT_OK = 0,
    FACTORY_RESULT_INVALID_ARGUMENT,
    FACTORY_RESULT_OUT_OF_BOUNDS,
    FACTORY_RESULT_TILE_OCCUPIED,
    FACTORY_RESULT_OUT_OF_MEMORY,
    FACTORY_RESULT_NO_RESOURCE,
    FACTORY_RESULT_UNSUPPORTED_RESOURCE,
    FACTORY_RESULT_QUEUE_FULL,
    FACTORY_RESULT_INVALID_STATE,
    FACTORY_RESULT_ENTITY_NOT_FOUND,
    FACTORY_RESULT_ENTITY_BUSY,
    FACTORY_RESULT_ENTITY_HAS_MATERIAL,
    FACTORY_RESULT_UNSUPPORTED_ENTITY,
    FACTORY_RESULT_INTERNAL_STATE_MISMATCH,
    FACTORY_RESULT_INSUFFICIENT_CONSTRUCTION_UNITS,
    FACTORY_RESULT_CONSTRUCTION_INVENTORY_OVERFLOW,
    FACTORY_RESULT_ASSEMBLER_NOT_EMPTY,
    FACTORY_RESULT_STORAGE_OUTPUT_NOT_EMPTY,
    FACTORY_RESULT_SNAPSHOT_BUFFER_TOO_SMALL,
    FACTORY_RESULT_SNAPSHOT_INVALID_MAGIC,
    FACTORY_RESULT_SNAPSHOT_UNSUPPORTED_VERSION,
    FACTORY_RESULT_SNAPSHOT_TRUNCATED,
    FACTORY_RESULT_SNAPSHOT_CORRUPT,
    FACTORY_RESULT_SNAPSHOT_SIZE_OVERFLOW,
    FACTORY_RESULT_SNAPSHOT_IO_ERROR,
    FACTORY_RESULT_POWER_NOT_APPLICABLE,
    FACTORY_RESULT_POWER_NETWORK_NOT_FOUND,
    FACTORY_RESULT_POWER_OVERFLOW,
    FACTORY_RESULT_FLUID_INCOMPATIBLE,
    FACTORY_RESULT_FLUID_MISMATCH,
    FACTORY_RESULT_FLUID_CAPACITY_EXCEEDED,
    FACTORY_RESULT_INSUFFICIENT_FLUID,
    FACTORY_RESULT_FLUID_NETWORK_NOT_FOUND
} FactoryResult;

/*
 * Creates a world whose coordinates range from (0, 0), inclusive, to
 * (width, height), exclusive. The caller owns the returned world and must
 * destroy it with factory_world_destroy. Returns NULL for zero dimensions,
 * overflow, or allocation failure.
 */
FactoryWorld *factory_world_create(uint32_t width, uint32_t height);

/* Destroys a world and its tiles. Passing NULL is safe. */
void factory_world_destroy(FactoryWorld *world);

/* Return zero when world is NULL. */
uint32_t factory_world_get_width(const FactoryWorld *world);
uint32_t factory_world_get_height(const FactoryWorld *world);

/* Returns false for a NULL world or coordinates outside the world. */
bool factory_world_is_in_bounds(const FactoryWorld *world, int32_t x, int32_t y);

/*
 * Returns a read-only, engine-owned tile pointer, or NULL for invalid
 * coordinates or a NULL world. The pointer remains valid until the world is
 * destroyed and must not be freed by the caller.
 */
const FactoryTile *factory_world_get_tile(
    const FactoryWorld *world,
    int32_t x,
    int32_t y
);

/*
 * Adds a finite resource deposit to an empty ground tile. Returns
 * INVALID_ARGUMENT for a NULL world, FACTORY_RESOURCE_NONE, an unknown
 * resource type, or zero amount; OUT_OF_BOUNDS for invalid coordinates; and
 * TILE_OCCUPIED when a resource already exists. Failure leaves the tile
 * unchanged.
 */
FactoryResult factory_world_add_resource(
    FactoryWorld *world,
    int32_t x,
    int32_t y,
    FactoryResourceType resource,
    uint32_t amount
);

/* Returns a stable name, or "invalid resource" for an unknown value. */
const char *factory_resource_name(FactoryResourceType resource);

#endif
