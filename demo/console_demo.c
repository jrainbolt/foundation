#include "foundation/simulation.h"
#include "foundation/snapshot.h"

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

static const char *assembler_recipe_name(FactoryAssemblerRecipeId recipe_id)
{
    switch (recipe_id) {
        case FACTORY_ASSEMBLER_RECIPE_NONE:
            return "none";
        case FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT:
            return "electronic component";
        case FACTORY_ASSEMBLER_RECIPE_IRON_GEAR:
            return "iron gear";
        case FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE:
            return "copper wire";
        default:
            return "invalid";
    }
}

static void storage_output_demo(void)
{
    FactoryWorld *world = factory_world_create(5U, 3U);
    FactorySimulation *simulation;
    FactoryStorage source;
    FactoryStorage destination;
    FactoryInserter inserter;

    if (world == NULL
        || factory_world_add_resource(
            world, 0, 0, FACTORY_RESOURCE_IRON, 5U
        ) != FACTORY_RESULT_OK) {
        factory_world_destroy(world);
        return;
    }
    simulation = factory_simulation_create_with_construction_units(
        world, UINT32_MAX
    );
    if (simulation == NULL) {
        factory_world_destroy(world);
        return;
    }
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {2, 0}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {3, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {4, 0}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_POLE, {.place_power_pole = {2, 1}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {2, 2}}
    });
    for (uint32_t tick = 0U; tick < 70U; ++tick) {
        factory_simulation_tick(simulation);
    }
    (void)factory_simulation_get_storage(simulation, 3U, &source);
    (void)printf(
        "\nStorage output demo: inventory has %" PRIu32 " iron ore\n",
        source.iron_ore_amount
    );
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output = {3U, FACTORY_ITEM_IRON_ORE}}
    });
    factory_simulation_tick(simulation);
    (void)factory_simulation_get_storage(simulation, 3U, &source);
    (void)factory_simulation_get_inserter(simulation, 4U, &inserter);
    (void)printf(
        "Fill tick: buffer=%s, inserter state=%u\n",
        factory_item_name(source.output_item),
        (unsigned)inserter.state
    );
    factory_simulation_tick(simulation);
    (void)factory_simulation_get_storage(simulation, 3U, &source);
    (void)factory_simulation_get_inserter(simulation, 4U, &inserter);
    (void)printf(
        "Pickup tick: held=%s, buffer occupied=%s\n",
        factory_item_name(inserter.held_item),
        source.output_occupied ? "yes" : "no"
    );
    factory_simulation_tick(simulation);
    (void)factory_simulation_get_storage(simulation, 3U, &source);
    (void)printf(
        "Refill tick: buffer=%s, inventory=%" PRIu32 "\n",
        factory_item_name(source.output_item),
        source.iron_ore_amount
    );
    factory_simulation_tick(simulation);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output = {5U, FACTORY_ITEM_IRON_ORE}}
    });
    factory_simulation_tick(simulation);
    factory_simulation_tick(simulation);
    (void)factory_simulation_get_storage(simulation, 5U, &destination);
    (void)printf(
        "Uncollected destination buffer remains %s; inventory=%" PRIu32 "\n",
        factory_item_name(destination.output_item),
        destination.iron_ore_amount
    );

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void snapshot_demo(void)
{
    FactoryWorld *world = factory_world_create(2U, 1U);
    FactorySimulation *original;
    FactorySimulation *loaded = NULL;
    FactorySimulation *rejected = NULL;
    FactorySnapshotBuffer snapshot = {0};
    uint8_t saved_magic;
    FactoryResult corrupt_result;

    if (world == NULL) {
        return;
    }
    original = factory_simulation_create_with_construction_units(world, 50U);
    if (original == NULL) {
        factory_world_destroy(world);
        return;
    }
    submit(original, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    factory_simulation_tick(original);
    for (uint32_t tick = 0U; tick < 4U; ++tick) {
        factory_simulation_tick(original);
    }
    if (factory_simulation_create_snapshot(original, &snapshot)
            != FACTORY_RESULT_OK
        || factory_simulation_load_snapshot(
            snapshot.data, snapshot.size, &loaded
        ) != FACTORY_RESULT_OK) {
        factory_snapshot_buffer_destroy(&snapshot);
        factory_simulation_destroy(original);
        factory_world_destroy(world);
        return;
    }
    (void)printf(
        "\nSnapshot v%u: %zu bytes at tick %" PRIu64
        ", loaded entities=%zu\n",
        FACTORY_SNAPSHOT_VERSION,
        snapshot.size,
        factory_simulation_get_tick(loaded),
        factory_simulation_get_entity_count(loaded)
    );
    for (uint32_t tick = 0U; tick < 10U; ++tick) {
        factory_simulation_tick(original);
        factory_simulation_tick(loaded);
    }
    (void)printf(
        "Continuation ticks: original=%" PRIu64 ", loaded=%" PRIu64
        " (%s)\n",
        factory_simulation_get_tick(original),
        factory_simulation_get_tick(loaded),
        factory_simulation_get_tick(original)
                == factory_simulation_get_tick(loaded)
            ? "identical" : "different"
    );
    saved_magic = snapshot.data[0];
    snapshot.data[0] ^= 0xffU;
    corrupt_result = factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &rejected
    );
    snapshot.data[0] = saved_magic;
    (void)printf(
        "Corrupted magic rejected=%s (result %u)\n",
        corrupt_result == FACTORY_RESULT_SNAPSHOT_INVALID_MAGIC
            && rejected == NULL ? "yes" : "no",
        (unsigned)corrupt_result
    );

    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(original);
    factory_world_destroy(world);
}

static void power_demo(void)
{
    FactoryWorld *world = factory_world_create(10U, 3U);
    FactorySimulation *simulation;
    FactoryPowerNetworkInspection network;
    FactoryPowerConnectionInspection connection;
    FactoryPowerConsumerInspection consumer;

    if (world == NULL
        || factory_world_add_resource(
            world, 3, 0, FACTORY_RESOURCE_IRON, 10U
        ) != FACTORY_RESULT_OK) {
        factory_world_destroy(world);
        return;
    }
    simulation = factory_simulation_create_with_construction_units(
        world, UINT32_MAX
    );
    if (simulation == NULL) {
        factory_world_destroy(world);
        return;
    }
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {0, 0}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {0, 1}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {3, 0, FACTORY_DIRECTION_SOUTH}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {6, 0}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {9, 0, FACTORY_DIRECTION_SOUTH}}
    });
    factory_simulation_tick(simulation);
    (void)factory_simulation_get_power_network(simulation, 0U, &network);
    (void)printf(
        "\nPower network %" PRIu32 ": poles=%" PRIu32
        ", generation=%" PRIu64 ", demand=%" PRIu64
        ", unused=%" PRIu64 "\n",
        network.network_id,
        network.pole_count,
        network.total_generation,
        network.total_demand,
        network.unused_generation
    );
    if (factory_simulation_get_power_connection(
            simulation, 0U, &connection) == FACTORY_RESULT_OK) {
        (void)printf(
            "Canonical pole connection: %" PRIu32 " - %" PRIu32 "\n",
            connection.pole_a, connection.pole_b
        );
    }
    (void)factory_simulation_get_power_consumer(simulation, 3U, &consumer);
    (void)printf(
        "Extractor 3 powered=%s, attached pole=%" PRIu32 "\n",
        consumer.powered ? "yes" : "no",
        consumer.attached_pole_id
    );
    (void)factory_simulation_get_power_consumer(simulation, 5U, &consumer);
    (void)printf(
        "Assembler 5 powered=%s, network=%" PRIu32 "\n",
        consumer.powered ? "yes" : "no",
        consumer.network_id
    );

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

int main(void)
{
    FactoryWorld *world = factory_world_create(3U, 1U);
    FactorySimulation *simulation;
    const FactoryCommandResult *result;
    FactoryAssembler assembler;
    FactoryEntityId assembler_id = 0U;
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
    assembler_id = result->entity_id;
    (void)printf(
        "Place assembler: %s, %" PRIu32 " remaining\n",
        result_name(result->result),
        factory_simulation_construction_units(simulation)
    );

    succeeded = result->result == FACTORY_RESULT_OK;
    if (succeeded) {
        (void)factory_simulation_get_assembler(
            simulation, assembler_id, &assembler
        );
        (void)printf(
            "\nAssembler %" PRIu32 " default recipe: %s\n",
            assembler_id,
            assembler_recipe_name(assembler.recipe_id)
        );
        for (FactoryAssemblerRecipeId recipe_id =
                FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT;
            recipe_id < FACTORY_ASSEMBLER_RECIPE_COUNT;
            recipe_id = (FactoryAssemblerRecipeId)(recipe_id + 1)) {
            FactoryAssemblerRecipe recipe;

            if (factory_assembler_recipe_get(recipe_id, &recipe)) {
                (void)printf(
                    "Recipe %-20s: %" PRIu32 " input slot(s), %" PRIu32
                    " x %s output, %" PRIu32 " ticks\n",
                    assembler_recipe_name(recipe_id),
                    recipe.input_count,
                    recipe.output_amount,
                    factory_item_name(recipe.output_item),
                    recipe.processing_ticks
                );
            }
            submit(simulation, (FactoryCommand){
                FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
                {.set_assembler_recipe = {assembler_id, recipe_id}}
            });
        }
        factory_simulation_tick(simulation);
        (void)factory_simulation_get_assembler(
            simulation, assembler_id, &assembler
        );
        (void)printf(
            "FIFO selections end on: %s; input 0 is %s 0/%" PRIu32 "\n",
            assembler_recipe_name(assembler.recipe_id),
            factory_item_name(assembler.input_slots[0].item),
            assembler.input_slots[0].capacity
        );
    }
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    storage_output_demo();
    snapshot_demo();
    power_demo();
    return succeeded ? 0 : 1;
}
