#include "foundation/simulation.h"

#include <inttypes.h>
#include <stdio.h>

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    (void)factory_simulation_submit_command(simulation, &command);
}

static const char *result_name(FactoryResult result)
{
    if (result == FACTORY_RESULT_OK) {
        return "success";
    }
    if (result == FACTORY_RESULT_INSUFFICIENT_CONSTRUCTION_UNITS) {
        return "insufficient construction units";
    }
    return "failed";
}

static uint32_t cost(FactoryEntityType type)
{
    uint32_t value = 0U;

    (void)factory_entity_construction_cost(type, &value);
    return value;
}

int main(void)
{
    FactoryWorld *world = factory_world_create(3U, 1U);
    FactorySimulation *simulation;
    const FactoryCommandResult *result;
    bool succeeded;

    if (world == NULL) {
        return 1;
    }
    simulation = factory_simulation_create(world);
    if (simulation == NULL) {
        factory_world_destroy(world);
        return 1;
    }

    (void)printf(
        "Initial construction units: %" PRIu32 "\n",
        factory_simulation_construction_units(simulation)
    );
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS,
        {.grant_construction_units = {20U}}
    });
    factory_simulation_tick(simulation);
    (void)printf(
        "Grant 20: %" PRIu32 "\n",
        factory_simulation_construction_units(simulation)
    );

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    factory_simulation_tick(simulation);
    (void)printf(
        "Place belt (cost %" PRIu32 "): %" PRIu32 " remaining\n",
        cost(FACTORY_ENTITY_TYPE_BELT),
        factory_simulation_construction_units(simulation)
    );

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {1, 0, FACTORY_DIRECTION_EAST}}
    });
    factory_simulation_tick(simulation);
    (void)printf(
        "Place inserter (cost %" PRIu32 "): %" PRIu32 " remaining\n",
        cost(FACTORY_ENTITY_TYPE_INSERTER),
        factory_simulation_construction_units(simulation)
    );

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {2, 0, FACTORY_DIRECTION_EAST}}
    });
    factory_simulation_tick(simulation);
    result = factory_simulation_get_command_result(simulation, 0U);
    (void)printf(
        "Attempt assembler (cost %" PRIu32 "): %s, %" PRIu32
        " remaining\n",
        cost(FACTORY_ENTITY_TYPE_ASSEMBLER),
        result_name(result->result),
        factory_simulation_construction_units(simulation)
    );

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {2U}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {1U}}
    });
    factory_simulation_tick(simulation);
    (void)printf(
        "Refund inserter + belt: %" PRIu32 " remaining\n",
        factory_simulation_construction_units(simulation)
    );

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    factory_simulation_tick(simulation);
    result = factory_simulation_get_command_result(simulation, 0U);
    (void)printf(
        "Place assembler: %s, %" PRIu32 " remaining\n",
        result_name(result->result),
        factory_simulation_construction_units(simulation)
    );

    succeeded = result->result == FACTORY_RESULT_OK;
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    return succeeded ? 0 : 1;
}
