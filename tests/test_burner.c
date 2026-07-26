#include "foundation/presentation.h"
#include "foundation/snapshot.h"
#include "burner_internal.h"
#include "logistics_endpoint_internal.h"
#include "simulation_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

static void check_release_sequence(
    FactoryEnergy energy,
    uint32_t duration,
    const FactoryEnergy *expected
)
{
    FactoryFuelDefinition definition = {
        FACTORY_ITEM_BIOMASS_PELLET, duration, energy,
        FACTORY_FUEL_CLASS_SOLID
    };
    FactoryEnergy total = 0U;
    for (uint32_t tick = 0U; tick < duration; ++tick) {
        FactoryEnergy released = UINT64_MAX;
        CHECK(factory_fuel_release_for_tick(
            &definition, tick, &released));
        CHECK(released == expected[tick]);
        total += released;
    }
    CHECK(total == energy);
    {
        FactoryEnergy ignored;
        CHECK(!factory_fuel_release_for_tick(
            &definition, duration, &ignored));
    }
}

static void test_exact_release_distribution(void)
{
    static const FactoryEnergy even[] = {2U, 2U, 2U, 2U, 2U};
    static const FactoryEnergy remainder[] = {4U, 3U, 3U};
    static const FactoryEnergy sparse[] = {1U, 1U, 1U, 0U, 0U};
    static const FactoryEnergy single[] = {7U};
    FactoryFuelDefinition invalid_duration = {
        FACTORY_ITEM_BIOMASS_PELLET, 0U, 1U, FACTORY_FUEL_CLASS_SOLID
    };
    FactoryFuelDefinition invalid_energy = {
        FACTORY_ITEM_BIOMASS_PELLET, 1U, 0U, FACTORY_FUEL_CLASS_SOLID
    };
    FactoryFuelDefinition overflow_capacity = {
        FACTORY_ITEM_BIOMASS_PELLET, 1U,
        FACTORY_BURNER_RELEASED_ENERGY_CAPACITY + 1U,
        FACTORY_FUEL_CLASS_SOLID
    };

    check_release_sequence(10U, 5U, even);
    check_release_sequence(10U, 3U, remainder);
    check_release_sequence(3U, 5U, sparse);
    check_release_sequence(7U, 1U, single);
    CHECK(!factory_fuel_definition_is_valid(&invalid_duration));
    CHECK(!factory_fuel_definition_is_valid(&invalid_energy));
    CHECK(!factory_fuel_definition_is_valid(&overflow_capacity));
}

static void test_capacity_multiple_items_and_demolition(void)
{
    FactoryWorld *world = factory_world_create(2U, 2U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryBurnerInspection inspection;
    FactoryLogisticsEndpoint input = {
        1U, FACTORY_LOGISTICS_SLOT_BURNER_INPUT
    };

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {0, 0}}
    });
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(
        simulation, input, FACTORY_ITEM_BIOMASS_PELLET)
        == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(
        simulation, input, FACTORY_ITEM_BIOMASS_PELLET)
        == FACTORY_LOGISTICS_RESULT_OK);

    for (uint32_t tick = 0U; tick < 1000U; ++tick)
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_burner(simulation, 1U, &inspection)
        == FACTORY_RESULT_OK);
    CHECK(!inspection.active);
    CHECK(inspection.inventory_quantity == 1U);
    CHECK(inspection.released_energy
        == FACTORY_BURNER_RELEASED_ENERGY_CAPACITY);
    CHECK(factory_simulation_get_event_count(simulation) == 1U);
    CHECK(factory_simulation_get_event(simulation, 0U)->type
        == FACTORY_EVENT_FUEL_EXHAUSTED);

    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(simulation) == 0U);
    CHECK(factory_simulation_get_burner(simulation, 1U, &inspection)
        == FACTORY_RESULT_OK);
    CHECK(inspection.inventory_quantity == 1U && !inspection.active);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {1U}}
    });
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    CHECK(factory_entity_is_valid(simulation->entities, 1U));

    CHECK(factory_burner_consume_energy(
        factory_burner_store_find_mutable(&simulation->burners, 1U),
        FACTORY_BURNER_RELEASED_ENERGY_CAPACITY));
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(simulation) == 1U);
    CHECK(factory_simulation_get_event(simulation, 0U)->type
        == FACTORY_EVENT_FUEL_IGNITED);
    CHECK(factory_simulation_get_burner(simulation, 1U, &inspection)
        == FACTORY_RESULT_OK);
    CHECK(inspection.inventory_quantity == 0U && inspection.active);
    CHECK(inspection.released_energy == 100U);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void clear_burner(FactorySimulation *simulation, FactoryEntityId id)
{
    FactoryBurner *burner =
        factory_burner_store_find_mutable(&simulation->burners, id);
    CHECK(burner != NULL);
    if (burner == NULL) return;
    burner->inventory_item = FACTORY_ITEM_NONE;
    burner->inventory_quantity = 0U;
    burner->current_fuel_item = FACTORY_ITEM_NONE;
    burner->remaining_burn_ticks = 0U;
    burner->released_energy = 0U;
}

static void test_belt_and_inserter_delivery(void)
{
    {
        FactoryWorld *world = factory_world_create(3U, 2U);
        FactorySimulation *simulation =
            factory_simulation_create_with_construction_units(
                world, UINT32_MAX);
        FactoryBurnerInspection burner;
        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_BELT,
            {.place_belt = {0, 0, FACTORY_DIRECTION_EAST}}
        });                                                 /* 1 */
        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_POWER_GENERATOR,
            {.place_power_generator = {1, 0}}
        });                                                 /* 2 */
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
        simulation->belts.items[0].item = FACTORY_ITEM_BIOMASS_PELLET;
        simulation->belts.items[0].movement_progress =
            FACTORY_BELT_TRANSFER_TICKS - 1U;
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_burner(simulation, 2U, &burner)
            == FACTORY_RESULT_OK);
        CHECK(burner.inventory_quantity == 1U);
        CHECK(factory_simulation_get_event_count(simulation) == 1U);
        CHECK(factory_simulation_get_event(simulation, 0U)->type
            == FACTORY_EVENT_ITEM_TRANSFERRED);
        factory_simulation_destroy(simulation);
        factory_world_destroy(world);
    }
    {
        FactoryWorld *world = factory_world_create(7U, 5U);
        FactorySimulation *simulation =
            factory_simulation_create_with_construction_units(
                world, UINT32_MAX);
        FactoryBurnerInspection burner;
        simulation->fixture_initial_generator_fuel = 10000U;
        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {1, 1}}
        });                                                 /* 1 */
        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter = {2, 1, FACTORY_DIRECTION_EAST}}
        });                                                 /* 2 */
        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_POWER_GENERATOR,
            {.place_power_generator = {3, 1}}
        });                                                 /* 3 target */
        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_POWER_POLE,
            {.place_power_pole = {3, 3}}
        });                                                 /* 4 */
        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_POWER_GENERATOR,
            {.place_power_generator = {3, 4}}
        });                                                 /* 5 support */
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
        clear_burner(simulation, 3U);
        simulation->storages.items[0].biomass_pellet_amount = 1U;
        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_SET_STORAGE_OUTPUT,
            {.set_storage_output = {1U, FACTORY_ITEM_BIOMASS_PELLET}}
        });
        for (uint32_t tick = 0U; tick < 5U; ++tick)
            CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_burner(simulation, 3U, &burner)
            == FACTORY_RESULT_OK);
        CHECK(burner.inventory_quantity == 0U);
        CHECK(burner.current_fuel_item == FACTORY_ITEM_BIOMASS_PELLET);
        CHECK(burner.active);
        factory_simulation_destroy(simulation);
        factory_world_destroy(world);
    }
}

static bool events_equal(
    const FactorySimulation *a, const FactorySimulation *b
)
{
    size_t count = factory_simulation_get_event_count(a);
    if (count != factory_simulation_get_event_count(b)) return false;
    for (size_t index = 0U; index < count; ++index) {
        const FactoryEvent *x = factory_simulation_get_event(a, index);
        const FactoryEvent *y = factory_simulation_get_event(b, index);
        if (x->type != y->type || x->tick != y->tick
            || x->entity_id != y->entity_id
            || x->related_entity_id != y->related_entity_id
            || x->entity_type != y->entity_type
            || x->item_type != y->item_type
            || x->quantity != y->quantity) return false;
    }
    return true;
}

static void check_continuation_at(uint32_t completed_burn_ticks)
{
    FactoryWorld *world = factory_world_create(2U, 2U);
    FactorySimulation *a =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactorySimulation *b = NULL;
    FactorySnapshotBuffer checkpoint = {0};
    FactorySnapshotBuffer x = {0};
    FactorySnapshotBuffer y = {0};
    FactoryLogisticsEndpoint input = {
        1U, FACTORY_LOGISTICS_SLOT_BURNER_INPUT
    };

    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {0, 0}}
    });
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(
        a, input, FACTORY_ITEM_BIOMASS_PELLET)
        == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(
        a, input, FACTORY_ITEM_BIOMASS_PELLET)
        == FACTORY_LOGISTICS_RESULT_OK);
    for (uint32_t tick = 0U; tick < completed_burn_ticks; ++tick)
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(a, &checkpoint)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        checkpoint.data, checkpoint.size, &b) == FACTORY_RESULT_OK);

    for (uint32_t tick = 0U; tick < 5U; ++tick) {
        FactoryBurnerInspection left;
        FactoryBurnerInspection right;
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
        CHECK(events_equal(a, b));
        CHECK(factory_simulation_get_burner(a, 1U, &left)
            == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_burner(b, 1U, &right)
            == FACTORY_RESULT_OK);
        CHECK(left.inventory_item == right.inventory_item);
        CHECK(left.inventory_quantity == right.inventory_quantity);
        CHECK(left.current_fuel_item == right.current_fuel_item);
        CHECK(left.remaining_burn_ticks == right.remaining_burn_ticks);
        CHECK(left.unreleased_fuel_energy
            == right.unreleased_fuel_energy);
        CHECK(left.released_energy == right.released_energy);
        CHECK(factory_simulation_create_snapshot(a, &x)
            == FACTORY_RESULT_OK);
        CHECK(factory_simulation_create_snapshot(b, &y)
            == FACTORY_RESULT_OK);
        CHECK(x.size == y.size
            && memcmp(x.data, y.data, x.size) == 0);
        factory_snapshot_buffer_destroy(&x);
        factory_snapshot_buffer_destroy(&y);
    }

    factory_snapshot_buffer_destroy(&checkpoint);
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(world);
}

static void test_save_load_release_continuation(void)
{
    check_continuation_at(0U);    /* before ignition */
    check_continuation_at(1U);    /* immediately after ignition */
    check_continuation_at(500U);  /* midway */
    check_continuation_at(999U);  /* one tick before completion */
    check_continuation_at(1000U); /* completed, buffered, between items */
}

static void test_definition_and_burner_lifecycle(void)
{
    FactoryWorld *world = factory_world_create(3U, 2U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactorySimulation *loaded = NULL;
    FactorySnapshotBuffer snapshot = {0};
    FactoryBurnerInspection burner;
    FactoryPresentationSnapshot *presentation =
        factory_presentation_snapshot_create();
    const FactoryPresentationEntity *entity;
    const FactoryFuelDefinition *definition =
        factory_fuel_definition_get(FACTORY_ITEM_BIOMASS_PELLET);

    CHECK(factory_fuel_definition_count() == 1U);
    CHECK(definition != NULL
        && factory_fuel_definition_is_valid(definition));
    CHECK(factory_fuel_definition_at(1U) == NULL);
    CHECK(factory_fuel_definition_get(FACTORY_ITEM_IRON_ORE) == NULL);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {1, 1}}
    });
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_burner(simulation, 1U, &burner)
        == FACTORY_RESULT_OK);
    CHECK(!burner.active && burner.released_energy == 0U);
    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        (FactoryLogisticsEndpoint){1U, FACTORY_LOGISTICS_SLOT_BURNER_INPUT},
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM);
    CHECK(factory_logistics_endpoint_insert(
        simulation,
        (FactoryLogisticsEndpoint){1U, FACTORY_LOGISTICS_SLOT_BURNER_INPUT},
        FACTORY_ITEM_BIOMASS_PELLET
    ) == FACTORY_LOGISTICS_RESULT_OK);

    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(simulation) == 1U);
    CHECK(factory_simulation_get_event(simulation, 0U)->type
        == FACTORY_EVENT_FUEL_IGNITED);
    CHECK(factory_simulation_get_burner(simulation, 1U, &burner)
        == FACTORY_RESULT_OK);
    CHECK(burner.active);
    CHECK(burner.current_fuel_item == FACTORY_ITEM_BIOMASS_PELLET);
    CHECK(burner.remaining_burn_ticks
        == definition->burn_duration_ticks - 1U);
    CHECK(burner.total_burn_duration_ticks
        == definition->burn_duration_ticks);
    CHECK(burner.unreleased_fuel_energy
        == definition->energy - 100U);
    CHECK(burner.released_energy == 100U);

    CHECK(factory_presentation_snapshot_rebuild(presentation, simulation)
        == FACTORY_RESULT_OK);
    entity = factory_presentation_snapshot_get_entity(presentation, 0U);
    CHECK(entity != NULL && entity->data.power_source.burner.active);
    CHECK(entity != NULL
        && entity->data.power_source.burner.remaining_burn_ticks
            == burner.remaining_burn_ticks);

    CHECK(factory_simulation_create_snapshot(simulation, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &loaded) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_burner(loaded, 1U, &burner)
        == FACTORY_RESULT_OK);
    CHECK(burner.active && burner.released_energy == 100U);
    CHECK(factory_simulation_get_event_count(loaded) == 0U);

    for (uint32_t tick = 1U;
         tick < definition->burn_duration_ticks; ++tick)
        CHECK(factory_simulation_tick(loaded) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(loaded) == 1U);
    CHECK(factory_simulation_get_event(loaded, 0U)->type
        == FACTORY_EVENT_FUEL_EXHAUSTED);
    CHECK(factory_simulation_get_burner(loaded, 1U, &burner)
        == FACTORY_RESULT_OK);
    CHECK(!burner.active && burner.remaining_burn_ticks == 0U);

    factory_presentation_snapshot_destroy(presentation);
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

int main(void)
{
    test_exact_release_distribution();
    test_definition_and_burner_lifecycle();
    test_capacity_multiple_items_and_demolition();
    test_belt_and_inserter_delivery();
    test_save_load_release_continuation();
    return failures == 0 ? 0 : 1;
}
