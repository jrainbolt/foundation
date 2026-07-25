#include "foundation/simulation.h"

#include <inttypes.h>
#include <stdio.h>

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    (void)factory_simulation_submit_command(simulation, &command);
}

static uint32_t accounted(
    const FactorySimulation *simulation,
    const FactoryWorld *world
)
{
    uint32_t total = factory_world_get_tile(world, 0, 1)->resource_amount;

    for (FactoryEntityId id = 1U; id <= 7U; ++id) {
        FactoryExtractor extractor;
        FactoryBelt belt;
        FactorySplitter splitter;
        FactoryStorage storage;

        if (factory_simulation_get_extractor(simulation, id, &extractor)) {
            total += extractor.output_amount;
        } else if (factory_simulation_get_belt(simulation, id, &belt)) {
            total += belt.item == FACTORY_ITEM_NONE ? 0U : 1U;
        } else if (factory_simulation_get_splitter(
                simulation, id, &splitter)) {
            total += splitter.item == FACTORY_ITEM_NONE ? 0U : 1U;
        } else if (factory_simulation_get_storage(simulation, id, &storage)) {
            total += factory_storage_get_total_amount(&storage);
        }
    }
    return total;
}

static void print_state(
    const FactorySimulation *simulation,
    const FactoryWorld *world
)
{
    FactorySplitter splitter;
    FactoryStorage left;
    FactoryStorage right;

    (void)factory_simulation_get_splitter(simulation, 3U, &splitter);
    (void)factory_simulation_get_storage(simulation, 4U, &left);
    (void)factory_simulation_get_storage(simulation, 6U, &right);
    (void)printf("Tick %" PRIu64
        ": left %" PRIu32 ", right %" PRIu32
        ", buffered %s, next %s, iron accounted %" PRIu32 "\n",
        factory_simulation_get_tick(simulation),
        left.iron_ore_amount,
        right.iron_ore_amount,
        factory_item_name(splitter.item),
        splitter.next_output == FACTORY_SPLITTER_OUTPUT_LEFT
            ? "left" : "right",
        accounted(simulation, world));
}

int main(void)
{
    FactoryWorld *world = factory_world_create(4U, 3U);
    FactorySimulation *simulation;
    uint32_t last_total = 0U;

    if (world == NULL || factory_world_add_resource(
            world, 0, 1, FACTORY_RESOURCE_IRON, 6U
        ) != FACTORY_RESULT_OK) {
        return 1;
    }
    simulation = factory_simulation_create(world);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_SPLITTER,
        {.place_splitter = {2, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage = {3, 0}}
    });
    /* Left output intentionally absent for the first item. */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {2, 2, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage = {3, 2}}
    });
    factory_simulation_tick(simulation);
    (void)printf("Splitter facing east: input west, left north, right south\n");
    (void)printf("Left output starts blocked; alternate routing is enabled.\n");
    print_state(simulation, world);

    while (factory_simulation_get_tick(simulation) < 130U) {
        FactoryStorage left;
        FactoryStorage right;
        uint32_t total;

        factory_simulation_tick(simulation);
        (void)factory_simulation_get_storage(simulation, 4U, &left);
        (void)factory_simulation_get_storage(simulation, 6U, &right);
        total = left.iron_ore_amount + right.iron_ore_amount;
        if (total != last_total) {
            print_state(simulation, world);
            last_total = total;
        }
        if (right.iron_ore_amount == 1U
            && factory_world_get_tile(world, 2, 0)->occupying_entity == 0U) {
            submit(simulation, (FactoryCommand){
                FACTORY_COMMAND_PLACE_BELT,
                {.place_belt = {2, 0, FACTORY_DIRECTION_EAST}}
            });
            factory_simulation_tick(simulation);
            (void)printf("Left output unblocked; round-robin resumes.\n");
            print_state(simulation, world);
        }
    }
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    return 0;
}
