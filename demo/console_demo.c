#include "foundation/simulation.h"

#include <inttypes.h>
#include <stdio.h>

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    (void)factory_simulation_submit_command(simulation, &command);
}

static const char *state_name(FactoryInserterState state)
{
    switch (state) {
        case FACTORY_INSERTER_STATE_IDLE:
            return "idle";
        case FACTORY_INSERTER_STATE_PICKING_UP:
            return "picking up";
        case FACTORY_INSERTER_STATE_HOLDING:
            return "holding";
        case FACTORY_INSERTER_STATE_DROPPING:
            return "dropping";
    }
    return "invalid";
}

int main(void)
{
    FactoryWorld *world = factory_world_create(6U, 5U);
    FactorySimulation *simulation;
    FactoryInserter previous = {0};
    bool previous_processing = false;
    uint32_t previous_stored = 0U;

    if (world == NULL
        || factory_world_add_resource(
            world, 0, 0, FACTORY_RESOURCE_IRON, 1U
        ) != FACTORY_RESULT_OK
        || factory_world_add_resource(
            world, 0, 4, FACTORY_RESOURCE_COPPER, 1U
        ) != FACTORY_RESULT_OK) {
        factory_world_destroy(world);
        return 1;
    }
    simulation = factory_simulation_create(world);
    if (simulation == NULL) {
        factory_world_destroy(world);
        return 1;
    }

    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {2, 0, FACTORY_DIRECTION_EAST}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            3, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_SOUTH
        }}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 4, FACTORY_DIRECTION_EAST}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 4, FACTORY_DIRECTION_EAST}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {2, 4, FACTORY_DIRECTION_EAST}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            3, 4, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_NORTH
        }}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {3, 1, FACTORY_DIRECTION_SOUTH}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {3, 3, FACTORY_DIRECTION_NORTH}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {3, 2, FACTORY_DIRECTION_EAST}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {4, 2, FACTORY_DIRECTION_EAST}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage = {5, 2}}});
    factory_simulation_tick(simulation);
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe = {4U, FACTORY_RECIPE_IRON_PLATE}}});
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe = {8U, FACTORY_RECIPE_COPPER_PLATE}}});
    factory_simulation_tick(simulation);

    (void)printf(
        "Deposits -> belts -> inserters -> refineries -> inserters\n"
        "         -> assembler -> inserter -> storage\n"
    );
    for (uint32_t update = 0U; update < 140U; ++update) {
        FactoryInserter output;
        FactoryAssembler assembler;
        FactoryStorage storage;

        factory_simulation_tick(simulation);
        (void)factory_simulation_get_inserter(simulation, 12U, &output);
        (void)factory_simulation_get_assembler(simulation, 11U, &assembler);
        (void)factory_simulation_get_storage(simulation, 13U, &storage);
        if (output.state != previous.state
            || output.held_item != previous.held_item) {
            (void)printf(
                "Tick %" PRIu64 ": output inserter %-10s (%s)\n",
                factory_simulation_get_tick(simulation),
                state_name(output.state),
                factory_item_name(output.held_item)
            );
            previous = output;
        }
        if (assembler.processing != previous_processing) {
            (void)printf(
                "Tick %" PRIu64 ": assembler %s\n",
                factory_simulation_get_tick(simulation),
                assembler.processing ? "processing" : "output ready"
            );
            previous_processing = assembler.processing;
        }
        if (storage.electronic_component_amount != previous_stored) {
            (void)printf(
                "Tick %" PRIu64 ": component arrived in storage (%" PRIu32
                " total)\n",
                factory_simulation_get_tick(simulation),
                storage.electronic_component_amount
            );
            previous_stored = storage.electronic_component_amount;
        }
    }

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    return previous_stored == 1U ? 0 : 1;
}
