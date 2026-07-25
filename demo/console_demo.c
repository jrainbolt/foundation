#include "foundation/simulation.h"

#include <inttypes.h>
#include <stdio.h>

static uint32_t accounted(
    const FactorySimulation *simulation,
    const FactoryWorld *world,
    const FactoryEntityId ids[7],
    int32_t row,
    FactoryItemType ore,
    FactoryItemType plate
)
{
    uint32_t total = factory_world_get_tile(world, 0, row)->resource_amount;

    for (size_t index = 0U; index < 7U; ++index) {
        FactoryExtractor extractor;
        FactoryBelt belt;
        FactoryRefinery refinery;
        FactoryStorage storage;

        if (factory_simulation_get_extractor(
                simulation, ids[index], &extractor)) {
            total += extractor.output_item == ore ? extractor.output_amount : 0U;
        } else if (factory_simulation_get_belt(
                simulation, ids[index], &belt)) {
            total += belt.item == ore || belt.item == plate ? 1U : 0U;
        } else if (factory_simulation_get_refinery(
                simulation, ids[index], &refinery)) {
            const FactoryRecipe *recipe = factory_recipe_get(refinery.recipe_id);
            total += refinery.input_item == ore ? refinery.input_amount : 0U;
            total += refinery.processing && recipe != NULL
                && recipe->input_item == ore ? 1U : 0U;
            total += refinery.output_item == plate
                ? refinery.output_amount : 0U;
        } else if (factory_simulation_get_storage(
                simulation, ids[index], &storage)) {
            total += ore == FACTORY_ITEM_IRON_ORE
                ? storage.iron_ore_amount + storage.iron_plate_amount
                : storage.copper_ore_amount + storage.copper_plate_amount;
        }
    }
    return total;
}

static void print_checkpoint(
    const FactorySimulation *simulation,
    const FactoryWorld *world,
    const FactoryEntityId iron[7],
    const FactoryEntityId copper[7]
)
{
    FactoryRefinery iron_refinery;
    FactoryRefinery copper_refinery;
    FactoryStorage iron_storage;
    FactoryStorage copper_storage;

    (void)factory_simulation_get_refinery(
        simulation, iron[3], &iron_refinery
    );
    (void)factory_simulation_get_refinery(
        simulation, copper[3], &copper_refinery
    );
    (void)factory_simulation_get_storage(simulation, iron[6], &iron_storage);
    (void)factory_simulation_get_storage(
        simulation, copper[6], &copper_storage
    );
    (void)printf("\nTick: %" PRIu64 "\n",
        factory_simulation_get_tick(simulation));
    (void)printf("Iron refinery: recipe %d, %s, progress %" PRIu32 "/10"
        ", output %s\n",
        (int)iron_refinery.recipe_id,
        iron_refinery.processing ? "processing" : "idle",
        iron_refinery.processing_progress,
        factory_item_name(iron_refinery.output_item));
    (void)printf("Copper refinery: recipe %d, %s, progress %" PRIu32 "/10"
        ", output %s\n",
        (int)copper_refinery.recipe_id,
        copper_refinery.processing ? "processing" : "idle",
        copper_refinery.processing_progress,
        factory_item_name(copper_refinery.output_item));
    (void)printf("Iron storage: iron plate %" PRIu32 "\n",
        iron_storage.iron_plate_amount);
    (void)printf("Copper storage: copper plate %" PRIu32 "\n",
        copper_storage.copper_plate_amount);
    (void)printf("Iron units accounted for: %" PRIu32 "\n",
        accounted(
            simulation, world, iron, 0,
            FACTORY_ITEM_IRON_ORE, FACTORY_ITEM_IRON_PLATE
        ));
    (void)printf("Copper units accounted for: %" PRIu32 "\n",
        accounted(
            simulation, world, copper, 1,
            FACTORY_ITEM_COPPER_ORE, FACTORY_ITEM_COPPER_PLATE
        ));
}

static FactoryCommand placement(size_t index, int32_t row)
{
    switch (index) {
        case 0U:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
                {.place_extractor = {0, row, FACTORY_DIRECTION_EAST}}};
        case 1U:
        case 2U:
        case 4U:
        case 5U:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_BELT,
                {.place_belt = {(int32_t)index, row, FACTORY_DIRECTION_EAST}}};
        case 3U:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_REFINERY,
                {.place_refinery = {
                    3, row, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
                }}};
        default:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
                {.place_storage = {6, row}}};
    }
}

int main(void)
{
    FactoryWorld *world = factory_world_create(7U, 2U);
    FactorySimulation *simulation;
    FactoryEntityId iron[7];
    FactoryEntityId copper[7];

    if (world == NULL
        || factory_world_add_resource(
            world, 0, 0, FACTORY_RESOURCE_IRON, 100U
        ) != FACTORY_RESULT_OK
        || factory_world_add_resource(
            world, 0, 1, FACTORY_RESOURCE_COPPER, 100U
        ) != FACTORY_RESULT_OK) {
        return 1;
    }
    simulation = factory_simulation_create(world);
    for (int32_t row = 0; row < 2; ++row) {
        for (size_t index = 0U; index < 7U; ++index) {
            FactoryCommand command = placement(index, row);
            if (factory_simulation_submit_command(
                    simulation, &command
                ) != FACTORY_RESULT_OK) {
                return 1;
            }
        }
    }
    factory_simulation_tick(simulation);
    for (size_t index = 0U; index < 7U; ++index) {
        iron[index] =
            factory_simulation_get_command_result(simulation, index)->entity_id;
        copper[index] = factory_simulation_get_command_result(
            simulation, index + 7U
        )->entity_id;
    }
    (void)printf("Entities placed:\nIron:  E>>R>>S\nCopper:E>>R>>S\n");
    print_checkpoint(simulation, world, iron, copper);

    {
        FactoryCommand iron_recipe = {FACTORY_COMMAND_SET_REFINERY_RECIPE,
            {.set_refinery_recipe = {
                iron[3], FACTORY_RECIPE_IRON_PLATE
            }}};
        FactoryCommand copper_recipe = {FACTORY_COMMAND_SET_REFINERY_RECIPE,
            {.set_refinery_recipe = {
                copper[3], FACTORY_RECIPE_COPPER_PLATE
            }}};
        (void)factory_simulation_submit_command(simulation, &iron_recipe);
        (void)factory_simulation_submit_command(simulation, &copper_recipe);
    }
    factory_simulation_tick(simulation);
    (void)printf("\nRefinery recipes selected.\n");
    print_checkpoint(simulation, world, iron, copper);

    while (factory_simulation_get_tick(simulation) < 48U) {
        uint64_t tick;
        factory_simulation_tick(simulation);
        tick = factory_simulation_get_tick(simulation);
        if (tick == 29U || tick == 38U || tick == 48U) {
            print_checkpoint(simulation, world, iron, copper);
        }
        if (tick == 29U) {
            FactoryCommand rejected = {
                FACTORY_COMMAND_SET_REFINERY_RECIPE,
                {.set_refinery_recipe = {
                    iron[3], FACTORY_RECIPE_COPPER_PLATE
                }}
            };
            (void)factory_simulation_submit_command(simulation, &rejected);
            factory_simulation_tick(simulation);
            (void)printf(
                "\nBusy iron-refinery recipe switch result: %d (rejected)\n",
                (int)factory_simulation_get_command_result(
                    simulation, 0U
                )->result
            );
        }
    }
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    return 0;
}
