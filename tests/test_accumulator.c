#include <foundation/accumulator.h>
#include <foundation/clock.h>
#include <foundation/event.h>
#include <foundation/presentation.h>
#include <foundation/simulation.h>
#include <foundation/snapshot.h>

#include <stdio.h>

#include "simulation_internal.h"

#define CHECK(c) do { if (!(c)) {                                           \
    fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #c);\
    return 1;                                                               \
} } while (0)

static FactorySimulation *make_simulation(void)
{
    FactoryWorld *world = factory_world_create(8U, 8U);
    FactorySimulation *simulation;
    if (world == NULL) return NULL;
    if (factory_world_add_resource(
            world, 3, 2, FACTORY_RESOURCE_IRON, 1000U)
        != FACTORY_RESULT_OK) {
        factory_world_destroy(world);
        return NULL;
    }
    simulation =
        factory_simulation_create_with_construction_units(world, 1000U);
    if (simulation == NULL) factory_world_destroy(world);
    return simulation;
}

static void destroy_borrowing_simulation(FactorySimulation *simulation)
{
    FactoryWorld *world;
    if (simulation == NULL) return;
    world = (FactoryWorld *)factory_simulation_get_world(simulation);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static int submit(FactorySimulation *s, FactoryCommand command)
{
    return factory_simulation_submit_command(s, &command)
        == FACTORY_RESULT_OK ? 0 : 1;
}

static int test_construction_and_demolition(void)
{
    FactorySimulation *s = make_simulation();
    FactoryAccumulatorInspection a;
    const FactoryCommandResult *result;
    CHECK(s != NULL);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_ACCUMULATOR,
        {.place_accumulator = {1, 1}}}) == 0);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_accumulator(s, 1U, &a) == FACTORY_RESULT_OK);
    CHECK(a.stored_energy == 0U
        && a.capacity == FACTORY_ACCUMULATOR_CAPACITY
        && a.maximum_charge_rate == FACTORY_ACCUMULATOR_MAX_CHARGE_RATE
        && a.maximum_discharge_rate
            == FACTORY_ACCUMULATOR_MAX_DISCHARGE_RATE
        && !a.connected && a.activity == FACTORY_ACCUMULATOR_IDLE);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity = {1U}}}) == 0);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    result = factory_simulation_get_command_result(s, 0U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_OK
        && result->entity_type == FACTORY_ENTITY_TYPE_ACCUMULATOR);
    destroy_borrowing_simulation(s);
    return 0;
}

static int test_charge_and_event(void)
{
    FactorySimulation *s = make_simulation();
    FactoryAccumulatorInspection a;
    FactoryPowerNetworkInspection network;
    const FactoryEvent *event = NULL;
    CHECK(s != NULL);
    factory_simulation_clock_set(&s->clock, 900U);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {2, 2}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_SOLAR_GENERATOR,
        {.place_solar_generator = {1, 2}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_ACCUMULATOR,
        {.place_accumulator = {2, 1}}}) == 0);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_accumulator(s, 3U, &a) == FACTORY_RESULT_OK);
    CHECK(a.connected && a.stored_energy == 100U
        && a.charged_last_tick == 100U
        && a.discharged_last_tick == 0U
        && a.activity == FACTORY_ACCUMULATOR_CHARGING);
    CHECK(factory_simulation_get_power_network(s, 0U, &network)
        == FACTORY_RESULT_OK);
    CHECK(network.total_generation == 100U
        && network.accumulator_charge == 100U
        && network.accumulator_discharge == 0U
        && network.unused_generation == 0U);
    for (size_t i = 0U; i < factory_simulation_get_event_count(s); ++i)
        if (factory_simulation_get_event(s, i)->type
            == FACTORY_EVENT_ACCUMULATOR_CHARGED)
            event = factory_simulation_get_event(s, i);
    CHECK(event != NULL && event->entity_id == 3U
        && event->quantity == 100U && event->related_quantity == 100U
        && event->tick == 900U);
    destroy_borrowing_simulation(s);
    return 0;
}

static int test_charge_capacity_rate_and_id_order(void)
{
    FactorySimulation *s = make_simulation();
    FactoryAccumulatorInspection first;
    FactoryAccumulatorInspection second;
    CHECK(s != NULL);
    factory_simulation_clock_set(&s->clock, 900U);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {2, 2}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_SOLAR_GENERATOR,
        {.place_solar_generator = {1, 2}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_ACCUMULATOR,
        {.place_accumulator = {2, 1}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_ACCUMULATOR,
        {.place_accumulator = {3, 2}}}) == 0);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_accumulator(s, 3U, &first)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_accumulator(s, 4U, &second)
        == FACTORY_RESULT_OK);
    CHECK(first.stored_energy == 100U && second.stored_energy == 0U);

    s->accumulators.items[0].stored_energy =
        FACTORY_ACCUMULATOR_CAPACITY - 10U;
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_accumulator(s, 3U, &first)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_accumulator(s, 4U, &second)
        == FACTORY_RESULT_OK);
    CHECK(first.stored_energy == FACTORY_ACCUMULATOR_CAPACITY
        && first.charged_last_tick == 10U);
    CHECK(second.stored_energy == 90U
        && second.charged_last_tick == 90U);
    destroy_borrowing_simulation(s);
    return 0;
}

static int test_discharge_indivisibility_and_order(void)
{
    FactorySimulation *s = make_simulation();
    FactoryAccumulatorInspection first;
    FactoryAccumulatorInspection second;
    FactoryPowerConsumerInspection consumer;
    CHECK(s != NULL);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {2, 2}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_ACCUMULATOR,
        {.place_accumulator = {1, 2}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_ACCUMULATOR,
        {.place_accumulator = {2, 1}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {3, 2, FACTORY_DIRECTION_EAST}}}) == 0);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    s->accumulators.items[0].stored_energy = 5U;
    s->accumulators.items[1].stored_energy = 4U;
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(!factory_power_is_entity_powered(s, 4U));
    CHECK(s->accumulators.items[0].stored_energy == 5U
        && s->accumulators.items[1].stored_energy == 4U);
    s->accumulators.items[1].stored_energy = 100U;
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_power_consumer(s, 4U, &consumer)
        == FACTORY_RESULT_OK && consumer.powered);
    CHECK(factory_simulation_get_accumulator(s, 2U, &first)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_accumulator(s, 3U, &second)
        == FACTORY_RESULT_OK);
    CHECK(first.discharged_last_tick == 5U && first.stored_energy == 0U);
    CHECK(second.discharged_last_tick == 5U && second.stored_energy == 95U);
    destroy_borrowing_simulation(s);
    return 0;
}

static int test_snapshot_continuation_and_presentation(void)
{
    FactorySimulation *a = make_simulation();
    FactorySimulation *b = NULL;
    FactorySnapshotBuffer snapshot = {0};
    FactoryPresentationSnapshot *presentation;
    const FactoryPresentationEntity *entity = NULL;
    CHECK(a != NULL);
    factory_simulation_clock_set(&a->clock, 900U);
    CHECK(submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {2, 2}}}) == 0);
    CHECK(submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_SOLAR_GENERATOR,
        {.place_solar_generator = {1, 2}}}) == 0);
    CHECK(submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_ACCUMULATOR,
        {.place_accumulator = {2, 1}}}) == 0);
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(a, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &b) == FACTORY_RESULT_OK);
    presentation = factory_presentation_snapshot_create();
    CHECK(presentation != NULL);
    CHECK(factory_presentation_snapshot_rebuild(presentation, b)
        == FACTORY_RESULT_OK);
    for (size_t i = 0U;
         i < factory_presentation_snapshot_get_entity_count(presentation); ++i) {
        const FactoryPresentationEntity *candidate =
            factory_presentation_snapshot_get_entity(presentation, i);
        if (candidate->entity_id == 3U) entity = candidate;
    }
    CHECK(entity != NULL && entity->entity_type
        == FACTORY_ENTITY_TYPE_ACCUMULATOR);
    CHECK(entity->data.accumulator.stored_energy == 100U
        && entity->data.accumulator.charged_last_tick == 0U
        && entity->data.accumulator.connected);
    for (uint32_t i = 0U; i < 20U; ++i) {
        FactoryAccumulatorInspection left;
        FactoryAccumulatorInspection right;
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_accumulator(a, 3U, &left)
            == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_accumulator(b, 3U, &right)
            == FACTORY_RESULT_OK);
        CHECK(left.stored_energy == right.stored_energy
            && left.charged_last_tick == right.charged_last_tick
            && left.discharged_last_tick == right.discharged_last_tick);
    }
    factory_presentation_snapshot_destroy(presentation);
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(b);
    destroy_borrowing_simulation(a);
    return 0;
}

static int test_solar_daylight_charge_and_night_discharge(void)
{
    FactorySimulation *s = make_simulation();
    FactoryAccumulatorInspection accumulator;
    FactorySolarGeneratorInspection solar;
    FactoryPowerConsumerInspection consumer;
    CHECK(s != NULL);
    factory_simulation_clock_set(&s->clock, 900U);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {2, 2}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_SOLAR_GENERATOR,
        {.place_solar_generator = {1, 2}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_ACCUMULATOR,
        {.place_accumulator = {2, 1}}}) == 0);
    CHECK(submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {3, 2, FACTORY_DIRECTION_EAST}}}) == 0);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_solar_generator(s, 2U, &solar)
        == FACTORY_RESULT_OK && solar.generated_last_tick == 100U);
    CHECK(factory_simulation_get_accumulator(s, 3U, &accumulator)
        == FACTORY_RESULT_OK);
    CHECK(accumulator.stored_energy == 90U
        && accumulator.charged_last_tick == 90U);
    factory_simulation_clock_set(&s->clock, FACTORY_CLOCK_SUNSET);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_accumulator(s, 3U, &accumulator)
        == FACTORY_RESULT_OK);
    CHECK(accumulator.stored_energy == 80U
        && accumulator.discharged_last_tick == 10U);
    CHECK(factory_simulation_get_power_consumer(s, 4U, &consumer)
        == FACTORY_RESULT_OK && consumer.powered);
    destroy_borrowing_simulation(s);
    return 0;
}

int main(void)
{
    CHECK(test_construction_and_demolition() == 0);
    CHECK(test_charge_and_event() == 0);
    CHECK(test_charge_capacity_rate_and_id_order() == 0);
    CHECK(test_discharge_indivisibility_and_order() == 0);
    CHECK(test_snapshot_continuation_and_presentation() == 0);
    CHECK(test_solar_daylight_charge_and_night_discharge() == 0);
    puts("accumulator tests passed");
    return 0;
}
