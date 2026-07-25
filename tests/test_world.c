#include "factory/world.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                     \
            (void)fprintf(                                                      \
                stderr,                                                         \
                "FAIL %s:%d: %s\n",                                             \
                __FILE__,                                                       \
                __LINE__,                                                       \
                #condition                                                      \
            );                                                                  \
            ++failures;                                                         \
        }                                                                       \
    } while (false)

static void test_creation_and_dimensions(void)
{
    FactoryWorld *world = factory_world_create(4U, 3U);
    CHECK(world != NULL);
    CHECK(factory_world_get_width(world) == 4U);
    CHECK(factory_world_get_height(world) == 3U);
    factory_world_destroy(world);

    CHECK(factory_world_create(0U, 3U) == NULL);
    CHECK(factory_world_create(3U, 0U) == NULL);
    CHECK(factory_world_get_width(NULL) == 0U);
    CHECK(factory_world_get_height(NULL) == 0U);
}

static void test_bounds(void)
{
    FactoryWorld *world = factory_world_create(4U, 3U);
    CHECK(world != NULL);
    CHECK(factory_world_is_in_bounds(world, 0, 0));
    CHECK(factory_world_is_in_bounds(world, 3, 2));
    CHECK(!factory_world_is_in_bounds(world, -1, 0));
    CHECK(!factory_world_is_in_bounds(world, 0, -1));
    CHECK(!factory_world_is_in_bounds(world, 4, 0));
    CHECK(!factory_world_is_in_bounds(world, 0, 3));
    CHECK(!factory_world_is_in_bounds(NULL, 0, 0));
    CHECK(factory_world_get_tile(NULL, 0, 0) == NULL);
    CHECK(factory_world_get_tile(world, -1, 0) == NULL);
    CHECK(factory_world_get_tile(world, 4, 0) == NULL);
    factory_world_destroy(world);
}

static void test_initialization_and_row_major_access(void)
{
    FactoryWorld *world = factory_world_create(3U, 2U);
    const FactoryTile *first = NULL;
    const FactoryTile *second = NULL;
    const FactoryTile *next_row = NULL;
    int32_t y = 0;

    CHECK(world != NULL);
    for (y = 0; y < 2; ++y) {
        int32_t x = 0;
        for (x = 0; x < 3; ++x) {
            const FactoryTile *tile = factory_world_get_tile(world, x, y);
            CHECK(tile != NULL);
            CHECK(tile->terrain == FACTORY_TERRAIN_GROUND);
            CHECK(tile->resource == FACTORY_RESOURCE_NONE);
            CHECK(tile->resource_amount == 0U);
            CHECK(tile->occupying_entity == 0U);
        }
    }

    first = factory_world_get_tile(world, 0, 0);
    second = factory_world_get_tile(world, 1, 0);
    next_row = factory_world_get_tile(world, 0, 1);
    CHECK(second == first + 1);
    CHECK(next_row == first + 3);
    factory_world_destroy(world);
}

static void test_resource_placement(void)
{
    FactoryWorld *world = factory_world_create(4U, 4U);
    const FactoryTile *tile = NULL;

    CHECK(world != NULL);
    CHECK(factory_world_add_resource(
        world, 2, 3, FACTORY_RESOURCE_IRON, 100U
    ) == FACTORY_RESULT_OK);
    tile = factory_world_get_tile(world, 2, 3);
    CHECK(tile->resource == FACTORY_RESOURCE_IRON);
    CHECK(tile->resource_amount == 100U);

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 0U
    ) == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_NONE, 50U
    ) == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_world_add_resource(
        world, -1, 0, FACTORY_RESOURCE_IRON, 50U
    ) == FACTORY_RESULT_OUT_OF_BOUNDS);
    CHECK(factory_world_add_resource(
        world, 4, 0, FACTORY_RESOURCE_IRON, 50U
    ) == FACTORY_RESULT_OUT_OF_BOUNDS);
    CHECK(factory_world_add_resource(
        NULL, 0, 0, FACTORY_RESOURCE_IRON, 50U
    ) == FACTORY_RESULT_INVALID_ARGUMENT);

    CHECK(factory_world_add_resource(
        world, 2, 3, FACTORY_RESOURCE_IRON, 999U
    ) == FACTORY_RESULT_TILE_OCCUPIED);
    tile = factory_world_get_tile(world, 2, 3);
    CHECK(tile->resource == FACTORY_RESOURCE_IRON);
    CHECK(tile->resource_amount == 100U);
    factory_world_destroy(world);
}

static void test_destroy_and_varied_sizes(void)
{
    static const uint32_t sizes[][2] = {
        {1U, 1U},
        {2U, 7U},
        {13U, 5U},
        {32U, 16U}
    };
    size_t index = 0U;

    factory_world_destroy(NULL);
    for (index = 0U; index < sizeof(sizes) / sizeof(sizes[0]); ++index) {
        FactoryWorld *world = factory_world_create(
            sizes[index][0],
            sizes[index][1]
        );
        CHECK(world != NULL);
        CHECK(factory_world_get_width(world) == sizes[index][0]);
        CHECK(factory_world_get_height(world) == sizes[index][1]);
        factory_world_destroy(world);
    }
}

int main(void)
{
    test_creation_and_dimensions();
    test_bounds();
    test_initialization_and_row_major_access();
    test_resource_placement();
    test_destroy_and_varied_sizes();

    if (failures != 0) {
        (void)fprintf(stderr, "%d test assertion(s) failed.\n", failures);
        return 1;
    }

    (void)printf("All world tests passed.\n");
    return 0;
}
