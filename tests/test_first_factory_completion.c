#include "foundation/construction_depot.h"
#include "foundation/simulation.h"
#include "foundation/snapshot.h"
#include "foundation/world.h"
#include "simulation_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    ++failures; } } while (false)

typedef struct {
    uint32_t depots;
    uint32_t extractors;
    uint32_t belts;
    uint32_t inserters;
    uint32_t storage;
    uint32_t refineries;
    uint32_t assemblers;
    uint32_t labs;
    uint32_t generators;
    uint32_t poles;
    uint32_t rails;
    uint32_t stations;
    uint32_t signals;
    uint32_t locomotives;
    uint32_t wagons;
} ConstructionLedger;

static uint32_t ledger_spend(const ConstructionLedger *ledger)
{
    return ledger->depots * FACTORY_CONSTRUCTION_COST_CONSTRUCTION_DEPOT
        + ledger->extractors * FACTORY_CONSTRUCTION_COST_EXTRACTOR
        + ledger->belts * FACTORY_CONSTRUCTION_COST_BELT
        + ledger->inserters * FACTORY_CONSTRUCTION_COST_INSERTER
        + ledger->storage * FACTORY_CONSTRUCTION_COST_STORAGE
        + ledger->refineries * FACTORY_CONSTRUCTION_COST_REFINERY
        + ledger->assemblers * FACTORY_CONSTRUCTION_COST_ASSEMBLER
        + ledger->labs * FACTORY_CONSTRUCTION_COST_RESEARCH_LAB
        + ledger->generators * FACTORY_CONSTRUCTION_COST_POWER_GENERATOR
        + ledger->poles * FACTORY_CONSTRUCTION_COST_POWER_POLE
        + ledger->rails * FACTORY_CONSTRUCTION_COST_RAIL
        + ledger->stations * FACTORY_CONSTRUCTION_COST_RAIL_STATION
        + ledger->signals * FACTORY_CONSTRUCTION_COST_RAIL_SIGNAL
        + ledger->locomotives * FACTORY_CONSTRUCTION_COST_LOCOMOTIVE
        + ledger->wagons * FACTORY_CONSTRUCTION_COST_CARGO_WAGON;
}

static const FactoryCommandResult *submit_tick(FactorySimulation *simulation,
    FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command) ==
        FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    return factory_simulation_get_command_result(simulation, 0U);
}

static uint32_t depot_total(const FactorySimulation *simulation);

static FactoryEntityId require_placement(FactorySimulation *simulation,
    FactoryCommand command)
{
    const FactoryCommandResult *result = submit_tick(simulation, command);
    if (result == NULL || result->result != FACTORY_RESULT_OK) {
        fprintf(stderr, "placement type=%u result=%u remaining=%u\n",
            (unsigned)command.type,
            result != NULL ? (unsigned)result->result : UINT32_MAX,
            depot_total(simulation));
    }
    CHECK(result != NULL && result->result == FACTORY_RESULT_OK);
    return result != NULL ? result->entity_id : 0U;
}

static uint32_t depot_total(const FactorySimulation *simulation)
{
    uint32_t total = 0U;
    for (size_t index = 0U; index < simulation->construction_depots.count;
        ++index) {
        total += simulation->construction_depots.items[index].material_quantity;
    }
    return total;
}

static uint32_t construction_material_total(const FactorySimulation *simulation)
{
    uint32_t total = depot_total(simulation);
    for (size_t index = 0U; index < simulation->belts.count; ++index) {
        if (simulation->belts.items[index].item ==
            FACTORY_ITEM_CONSTRUCTION_MATERIAL) ++total;
    }
    for (size_t index = 0U; index < simulation->inserters.count; ++index) {
        if (simulation->inserters.items[index].held_item ==
            FACTORY_ITEM_CONSTRUCTION_MATERIAL) {
            total += simulation->inserters.items[index].held_amount;
        }
    }
    return total;
}

static uint32_t loose_item_total(const FactorySimulation *simulation,
    FactoryItemType item)
{
    uint32_t total = 0U;
    for (size_t index = 0U; index < simulation->extractors.count; ++index) {
        if (simulation->extractors.items[index].output_item == item) {
            total += simulation->extractors.items[index].output_amount;
        }
    }
    for (size_t index = 0U; index < simulation->inserters.count; ++index) {
        if (simulation->inserters.items[index].held_item == item) {
            total += simulation->inserters.items[index].held_amount;
        }
    }
    return total;
}

static void verify_seed_six_budget(void)
{
    FactoryWorldGenerationConfig generation;
    FactoryWorld *world = factory_world_create_with_seed(64U, 48U, UINT64_C(6));
    factory_world_generation_default_config(&generation);
    generation.water_threshold = 0U;
    generation.rock_threshold = 0U;
    CHECK(factory_world_generate(world, &generation) == FACTORY_RESULT_OK);
    CHECK(factory_world_get_start_x(world) == 32);
    CHECK(factory_world_get_start_y(world) == 24);
    CHECK(factory_world_get_tile(world, 26, 17)->resource ==
        FACTORY_RESOURCE_IRON);
    CHECK(factory_world_get_tile(world, 26, 17)->resource_amount == 522U);
    CHECK(factory_world_get_tile(world, 43, 24)->resource ==
        FACTORY_RESOURCE_COPPER);
    CHECK(factory_world_get_tile(world, 43, 24)->resource_amount == 1119U);
    CHECK(factory_world_get_tile(world, 37, 17)->resource ==
        FACTORY_RESOURCE_COAL);
    CHECK(factory_world_get_tile(world, 37, 17)->resource_amount == 1000U);

    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, 525U);
    FactoryEntityId starter = require_placement(simulation,
        (FactoryCommand){FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
            {.place_construction_depot={32, 24}}});
    FactoryConstructionDepot inspected;
    CHECK(factory_simulation_construction_units(simulation) == 0U);
    CHECK(factory_simulation_construction_bootstrap_completed(simulation));
    CHECK(factory_simulation_get_construction_depot(simulation, starter,
        &inspected) && inspected.material_quantity == 500U);

    /* One finite starter pellet is the sole non-construction initial item.
     * No progression item is inserted anywhere in this harness. */
    const FactoryCommand supply[] = {
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={32, 23, FACTORY_DIRECTION_NORTH}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={32, 22, FACTORY_DIRECTION_NORTH}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={32, 21, FACTORY_DIRECTION_NORTH}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={32, 20, FACTORY_DIRECTION_NORTH}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={32, 19, FACTORY_DIRECTION_WEST}}},
        {FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
            {.place_construction_depot={31, 19}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={33, 24, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={34, 24, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={35, 24, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={36, 24, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={37, 24, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={38, 24, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={39, 24, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
            {.place_construction_depot={40, 24}}},
        {FACTORY_COMMAND_PLACE_POWER_POLE,
            {.place_power_pole={31, 22}}},
        {FACTORY_COMMAND_PLACE_POWER_POLE,
            {.place_power_pole={34, 22}}},
        {FACTORY_COMMAND_PLACE_POWER_POLE,
            {.place_power_pole={39, 23}}},
        {FACTORY_COMMAND_PLACE_POWER_GENERATOR,
            {.place_power_generator={30, 23}}}
    };
    ConstructionLedger ledger = {0};
    ledger.inserters = 2U;
    ledger.belts = 10U;
    ledger.depots = 2U;
    ledger.poles = 3U;
    ledger.generators = 1U;
    FactoryEntityId iron_coal_depot = 0U;
    FactoryEntityId copper_depot = 0U;
    for (size_t index = 0U; index < sizeof(supply) / sizeof(supply[0]);
        ++index) {
        if (supply[index].type == FACTORY_COMMAND_PLACE_POWER_GENERATOR) {
            simulation->fixture_initial_generator_fuel = 1U;
        }
        FactoryEntityId id = require_placement(simulation, supply[index]);
        if (index == 5U) iron_coal_depot = id;
        if (index == 13U) copper_depot = id;
    }
    CHECK(depot_total(simulation) == 393U);
    CHECK(factory_simulation_get_construction_depot(simulation,
        iron_coal_depot, &inspected) && inspected.material_quantity == 0U);
    CHECK(factory_simulation_get_construction_depot(simulation,
        copper_depot, &inspected) && inspected.material_quantity == 0U);

    for (uint32_t tick = 0U; tick < 1000U; ++tick) {
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
        FactoryConstructionDepot a, b;
        CHECK(factory_simulation_get_construction_depot(simulation,
            iron_coal_depot, &a));
        CHECK(factory_simulation_get_construction_depot(simulation,
            copper_depot, &b));
        if (a.material_quantity >= 120U && b.material_quantity >= 30U) break;
    }
    FactoryConstructionDepot a, b;
    CHECK(factory_simulation_get_construction_depot(simulation,
        iron_coal_depot, &a) && a.material_quantity >= 120U);
    CHECK(factory_simulation_get_construction_depot(simulation,
        copper_depot, &b) && b.material_quantity >= 30U);
    CHECK(depot_total(simulation) <= 393U);

    const FactoryCommand production[] = {
        {FACTORY_COMMAND_PLACE_EXTRACTOR,
            {.place_extractor={26, 17, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_EXTRACTOR,
            {.place_extractor={43, 24, FACTORY_DIRECTION_WEST}}},
        {FACTORY_COMMAND_PLACE_EXTRACTOR,
            {.place_extractor={37, 17, FACTORY_DIRECTION_SOUTH}}},
        {FACTORY_COMMAND_PLACE_REFINERY,
            {.place_refinery={27, 24, FACTORY_DIRECTION_WEST,
                FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_REFINERY,
            {.place_refinery={27, 25, FACTORY_DIRECTION_WEST,
                FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_REFINERY,
            {.place_refinery={28, 26, FACTORY_DIRECTION_NORTH,
                FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler={29, 26, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler={30, 26, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler={31, 26, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler={32, 26, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler={34, 26, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler={35, 26, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_RESEARCH_LAB,
            {.place_research_lab={36, 26}}},
        {FACTORY_COMMAND_PLACE_STORAGE, {.place_storage={28, 27}}},
        {FACTORY_COMMAND_PLACE_STORAGE, {.place_storage={29, 27}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={26, 18, FACTORY_DIRECTION_SOUTH}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={43, 25, FACTORY_DIRECTION_SOUTH}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={37, 18, FACTORY_DIRECTION_SOUTH}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={27, 23, FACTORY_DIRECTION_SOUTH}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={28, 24, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={29, 25, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={30, 25, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={31, 25, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={32, 25, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={34, 25, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={35, 25, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={36, 25, FACTORY_DIRECTION_SOUTH}}}
    };
    for (size_t index = 0U;
        index < sizeof(production) / sizeof(production[0]); ++index) {
        const FactoryCommandResult *result = submit_tick(simulation,
            production[index]);
        if (result == NULL || result->result != FACTORY_RESULT_OK) {
            fprintf(stderr, "production index=%zu type=%u result=%u global=%u\n",
                index, (unsigned)production[index].type,
                result != NULL ? (unsigned)result->result : UINT32_MAX,
                depot_total(simulation));
            CHECK(false);
            break;
        }
    }
    ledger.extractors = 3U;
    ledger.refineries = 3U;
    ledger.assemblers = 6U;
    ledger.labs = 1U;
    ledger.storage = 2U;
    ledger.inserters += 12U;

    const FactoryRailGeometry top[7] = {FACTORY_RAIL_CURVE_SE,
        FACTORY_RAIL_HORIZONTAL, FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL, FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL, FACTORY_RAIL_CURVE_SW};
    const FactoryRailGeometry bottom[7] = {FACTORY_RAIL_CURVE_NE,
        FACTORY_RAIL_HORIZONTAL, FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL, FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL, FACTORY_RAIL_CURVE_NW};
    for (int32_t index = 0; index < 7; ++index) {
        require_placement(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={28 + index, 14, top[index]}}});
        require_placement(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={28 + index, 18, bottom[index]}}});
    }
    for (int32_t index = 0; index < 3; ++index) {
        require_placement(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={28, 15 + index, FACTORY_RAIL_VERTICAL}}});
        require_placement(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={34, 15 + index, FACTORY_RAIL_VERTICAL}}});
    }
    ledger.rails = 20U;
    require_placement(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_RAIL_STATION,
        {.place_rail_station={33, 13, FACTORY_DIRECTION_SOUTH}}});
    ledger.stations = 1U;
    CHECK(ledger_spend(&ledger) == 476U);
    FactorySnapshotBuffer midpoint = {0};
    FactorySimulation *loaded = NULL;
    CHECK(factory_simulation_create_snapshot(simulation, &midpoint) ==
        FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(midpoint.data, midpoint.size,
        &loaded) == FACTORY_RESULT_OK);
    const FactoryCommandResult *rejected = submit_tick(simulation,
        (FactoryCommand){FACTORY_COMMAND_PLACE_RAIL_STATION,
            {.place_rail_station={33, 19, FACTORY_DIRECTION_NORTH}}});
    const FactoryCommandResult *loaded_rejected = submit_tick(loaded,
        (FactoryCommand){FACTORY_COMMAND_PLACE_RAIL_STATION,
            {.place_rail_station={33, 19, FACTORY_DIRECTION_NORTH}}});
    CHECK(rejected != NULL && rejected->result ==
        FACTORY_RESULT_CONSTRUCTION_SUPPLY_INSUFFICIENT);
    CHECK(loaded_rejected != NULL && loaded_rejected->result ==
        FACTORY_RESULT_CONSTRUCTION_SUPPLY_INSUFFICIENT);
    CHECK(construction_material_total(simulation) == 24U);
    FactorySnapshotBuffer final_a = {0}, final_b = {0};
    CHECK(factory_simulation_create_snapshot(simulation, &final_a) ==
        FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(loaded, &final_b) ==
        FACTORY_RESULT_OK);
    CHECK(final_a.size == final_b.size &&
        memcmp(final_a.data, final_b.data, final_a.size) == 0);
    CHECK(factory_world_get_tile(world, 26, 17)->resource_amount
        + loose_item_total(simulation, FACTORY_ITEM_IRON_ORE) == 522U);
    CHECK(factory_world_get_tile(world, 43, 24)->resource_amount
        + loose_item_total(simulation, FACTORY_ITEM_COPPER_ORE) == 1119U);
    CHECK(factory_world_get_tile(world, 37, 17)->resource_amount
        + loose_item_total(simulation, FACTORY_ITEM_COAL) == 1000U);

    FactoryConstructionDepot starter_state, iron_coal_state, copper_state;
    CHECK(factory_simulation_get_construction_depot(simulation, starter,
        &starter_state));
    CHECK(factory_simulation_get_construction_depot(simulation,
        iron_coal_depot, &iron_coal_state));
    CHECK(factory_simulation_get_construction_depot(simulation,
        copper_depot, &copper_state));

    const uint32_t projected_victory_cost = 530U;
    printf("canonical construction stopped at UNLOAD Station: remaining=24 "
        "cost=30 shortfall=6 projected-victory-cost=%u\n",
        projected_victory_cost);
    printf("construction ledger: depots=2/50 extractors=3/30 belts=10/10 "
        "inserters=14/56 storage=2/16 refineries=3/45 assemblers=6/120 "
        "lab=1/40 generator=1/30 poles=3/9 rails=20/40 station=1/30 "
        "spent=476\n");
    printf("remaining depot material: starter=%u iron-coal=%u copper=%u "
        "in-transit=%u\n", starter_state.material_quantity,
        iron_coal_state.material_quantity, copper_state.material_quantity,
        construction_material_total(simulation) - depot_total(simulation));

    printf("zero-injection seed 6 bootstrap: 525 -> depot %u with 500; "
        "relay trunks spent=107 remaining=%u relays=(%u@31,19,%u@40,24)\n",
        starter, depot_total(simulation), iron_coal_depot, copper_depot);
    printf("seed 6 resources: iron=(26,17 d=13 q=522) "
        "copper=(43,24 d=11 q=1119) coal=(37,17 d=12 q=1000)\n");

    factory_snapshot_buffer_destroy(&final_a);
    factory_snapshot_buffer_destroy(&final_b);
    factory_snapshot_buffer_destroy(&midpoint);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

int main(void)
{
    verify_seed_six_budget();
    return failures == 0 ? 0 : 1;
}
