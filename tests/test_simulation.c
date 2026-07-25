#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

typedef struct {
    uint64_t tick;
    FactoryEntityId entity;
    uint32_t progress;
    uint32_t deposit;
} RunState;

static RunState run_scenario(void)
{
    FactoryWorld *world = factory_world_create(3U, 3U);
    FactorySimulation *simulation;
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {1, 1, FACTORY_DIRECTION_EAST}}
    };
    const FactoryCommandResult *result;
    FactoryExtractor extractor;
    RunState state;
    uint32_t index;

    CHECK(world != NULL);
    CHECK(factory_world_add_resource(
        world, 1, 1, FACTORY_RESOURCE_IRON, 10U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    CHECK(simulation != NULL);
    CHECK(factory_simulation_submit_command(
        simulation, &command
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_get_tile(world, 1, 1)->occupying_entity == 0U);
    for (index = 0U; index < 7U; ++index) {
        factory_simulation_tick(simulation);
    }
    result = factory_simulation_get_command_result(simulation, 0U);
    /* Results are for the latest tick, so the placement result was replaced. */
    CHECK(result == NULL);
    state.entity = factory_world_get_tile(world, 1, 1)->occupying_entity;
    CHECK(factory_simulation_get_extractor(
        simulation, state.entity, &extractor
    ));
    state.tick = factory_simulation_get_tick(simulation);
    state.progress = extractor.production_progress;
    state.deposit = factory_world_get_tile(world, 1, 1)->resource_amount;
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    return state;
}

int main(void)
{
    FactoryWorld *world = factory_world_create(1U, 1U);
    FactorySimulation *simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    RunState first;
    RunState second;

    CHECK(factory_simulation_create_with_construction_units(NULL, UINT32_MAX) == NULL);
    CHECK(simulation != NULL);
    CHECK(factory_simulation_get_tick(simulation) == 0U);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_tick(simulation) == 1U);
    CHECK(factory_simulation_get_command_result_count(simulation) == 0U);
    factory_simulation_tick(NULL);
    factory_simulation_destroy(simulation);
    factory_simulation_destroy(NULL);
    factory_world_destroy(world);

    first = run_scenario();
    second = run_scenario();
    CHECK(first.tick == second.tick);
    CHECK(first.entity == second.entity);
    CHECK(first.progress == second.progress);
    CHECK(first.deposit == second.deposit);

    if (failures != 0) {
        return 1;
    }
    (void)printf("All simulation tests passed.\n");
    return 0;
}
