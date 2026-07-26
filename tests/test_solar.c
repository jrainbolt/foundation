#include <foundation/clock.h>
#include <foundation/event.h>
#include <foundation/presentation.h>
#include <foundation/simulation.h>
#include <foundation/snapshot.h>
#include <foundation/solar.h>

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
            world, 2, 1, FACTORY_RESOURCE_IRON, 1000U)
        != FACTORY_RESULT_OK) {
        factory_world_destroy(world);
        return NULL;
    }
    simulation =
        factory_simulation_create_with_construction_units(world, 1000U);
    if (simulation == NULL) factory_world_destroy(world);
    return simulation;
}

static int submit_layout(FactorySimulation *simulation)
{
    const FactoryCommand commands[] = {
        {FACTORY_COMMAND_PLACE_POWER_POLE, {.place_power_pole = {1, 1}}},
        {FACTORY_COMMAND_PLACE_SOLAR_GENERATOR,
            {.place_solar_generator = {1, 2}}},
        {FACTORY_COMMAND_PLACE_EXTRACTOR,
            {.place_extractor = {2, 1, FACTORY_DIRECTION_EAST}}}
    };
    for (size_t i = 0U; i < sizeof(commands) / sizeof(commands[0]); ++i)
        if (factory_simulation_submit_command(simulation, &commands[i])
            != FACTORY_RESULT_OK) return 1;
    return 0;
}

static int test_clock_curve_and_events(void)
{
    FactorySimulation *simulation = make_simulation();
    CHECK(simulation != NULL);
    CHECK(factory_simulation_clock_get_tick(simulation) == 0U);
    CHECK(factory_simulation_clock_get_day(simulation) == 0U);
    CHECK(factory_simulation_clock_get_time_of_day(simulation) == 0U);
    CHECK(factory_solar_intensity(0U) == 0U);
    CHECK(factory_solar_intensity(600U) == 0U);
    CHECK(factory_solar_intensity(750U) == 500U);
    CHECK(factory_solar_intensity(900U) == 1000U);
    CHECK(factory_solar_intensity(1200U) == 1000U);
    CHECK(factory_solar_intensity(1650U) == 500U);
    CHECK(factory_solar_intensity(1800U) == 0U);
    for (uint32_t i = 0U; i <= FACTORY_CLOCK_SUNRISE; ++i)
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(simulation) == 1U);
    CHECK(factory_simulation_get_event(simulation, 0U)->type
        == FACTORY_EVENT_SUNRISE);
    for (uint32_t i = FACTORY_CLOCK_SUNRISE + 1U;
         i <= FACTORY_CLOCK_SUNSET; ++i)
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(simulation) == 1U);
    CHECK(factory_simulation_get_event(simulation, 0U)->type
        == FACTORY_EVENT_SUNSET);
    while (factory_simulation_clock_get_day(simulation) == 0U)
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_clock_get_time_of_day(simulation) == 0U);
    factory_simulation_destroy(simulation);
    return 0;
}

static int test_generation_presentation_and_continuation(void)
{
    FactorySimulation *simulation = make_simulation();
    FactorySimulation *loaded = NULL;
    FactorySnapshotBuffer snapshot = {0};
    FactorySolarGeneratorInspection solar;
    FactoryPresentationSnapshot *presentation;
    const FactoryPresentationEntity *entity = NULL;
    CHECK(simulation != NULL && submit_layout(simulation) == 0);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_solar_generator(simulation, 2U, &solar)
        == FACTORY_RESULT_OK);
    CHECK(solar.available_output == 0U && solar.generated_last_tick == 0U);
    while (factory_simulation_clock_get_time_of_day(simulation) < 900U)
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_solar_generator(simulation, 2U, &solar)
        == FACTORY_RESULT_OK);
    CHECK(solar.available_output == FACTORY_SOLAR_GENERATOR_MAX_OUTPUT);
    CHECK(solar.generated_last_tick == FACTORY_POWER_DEMAND_EXTRACTOR);

    presentation = factory_presentation_snapshot_create();
    CHECK(presentation != NULL);
    CHECK(factory_presentation_snapshot_rebuild(presentation, simulation)
        == FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_get_time_of_day(presentation) == 901U);
    for (size_t i = 0U;
         i < factory_presentation_snapshot_get_entity_count(presentation); ++i) {
        const FactoryPresentationEntity *candidate =
            factory_presentation_snapshot_get_entity(presentation, i);
        if (candidate->entity_id == 2U) entity = candidate;
    }
    CHECK(entity != NULL
        && entity->entity_type == FACTORY_ENTITY_TYPE_SOLAR_GENERATOR);
    CHECK(entity->data.solar_generator.actual_output
        == FACTORY_POWER_DEMAND_EXTRACTOR);
    factory_presentation_snapshot_destroy(presentation);

    CHECK(factory_simulation_create_snapshot(simulation, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &loaded) == FACTORY_RESULT_OK);
    for (uint32_t i = 0U; i < 20U; ++i) {
        FactorySolarGeneratorInspection a;
        FactorySolarGeneratorInspection b;
        CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(loaded) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_solar_generator(simulation, 2U, &a)
            == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_solar_generator(loaded, 2U, &b)
            == FACTORY_RESULT_OK);
        CHECK(a.available_output == b.available_output);
        CHECK(a.generated_last_tick == b.generated_last_tick);
    }
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(simulation);
    return 0;
}

static int test_transactional_demolition(void)
{
    FactorySimulation *simulation = make_simulation();
    FactoryCommand place = {FACTORY_COMMAND_PLACE_SOLAR_GENERATOR,
        {.place_solar_generator = {1, 1}}};
    FactoryCommand demolish = {FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity = {1U}}};
    const FactoryCommandResult *result;
    CHECK(simulation != NULL);
    CHECK(factory_simulation_submit_command(simulation, &place)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    result = factory_simulation_get_command_result(simulation, 0U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_OK
        && result->entity_type == FACTORY_ENTITY_TYPE_SOLAR_GENERATOR);
    CHECK(factory_simulation_submit_command(simulation, &demolish)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    result = factory_simulation_get_command_result(simulation, 0U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_OK
        && result->entity_type == FACTORY_ENTITY_TYPE_SOLAR_GENERATOR);
    CHECK(factory_simulation_get_solar_generator(
        simulation, 1U, &(FactorySolarGeneratorInspection){0})
        == FACTORY_RESULT_ENTITY_NOT_FOUND);
    factory_simulation_destroy(simulation);
    return 0;
}

static int test_mixed_burner_and_solar_generation(void)
{
    FactoryWorld *world = factory_world_create(10U, 10U);
    FactorySimulation *simulation;
    FactoryPowerNetworkInspection network;
    FactorySolarGeneratorInspection solar;
    const int32_t positions[11][2] = {
        {3, 3}, {4, 3}, {5, 3}, {6, 3}, {3, 4}, {5, 4},
        {6, 4}, {3, 5}, {4, 5}, {5, 5}, {6, 5}
    };
    CHECK(world != NULL);
    for (size_t i = 0U; i < 11U; ++i)
        CHECK(factory_world_add_resource(
            world, positions[i][0], positions[i][1],
            FACTORY_RESOURCE_IRON, 1000U) == FACTORY_RESULT_OK);
    simulation =
        factory_simulation_create_with_construction_units(world, 1000U);
    CHECK(simulation != NULL);
    factory_simulation_clock_set(&simulation->clock, 900U);
    simulation->fixture_initial_generator_fuel = 10000U;
    const FactoryCommand sources[] = {
        {FACTORY_COMMAND_PLACE_POWER_POLE, {.place_power_pole = {4, 4}}},
        {FACTORY_COMMAND_PLACE_POWER_GENERATOR,
            {.place_power_generator = {2, 2}}},
        {FACTORY_COMMAND_PLACE_SOLAR_GENERATOR,
            {.place_solar_generator = {2, 3}}}
    };
    for (size_t i = 0U; i < sizeof(sources) / sizeof(sources[0]); ++i)
        CHECK(factory_simulation_submit_command(simulation, &sources[i])
            == FACTORY_RESULT_OK);
    for (size_t i = 0U; i < 11U; ++i) {
        FactoryCommand extractor = {FACTORY_COMMAND_PLACE_EXTRACTOR,
            {.place_extractor = {
                positions[i][0], positions[i][1], FACTORY_DIRECTION_EAST}}};
        CHECK(factory_simulation_submit_command(simulation, &extractor)
            == FACTORY_RESULT_OK);
    }
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_power_network_count(simulation) == 1U);
    CHECK(factory_simulation_get_power_network(simulation, 0U, &network)
        == FACTORY_RESULT_OK);
    CHECK(network.total_demand == 110U && network.allocated_power == 110U);
    CHECK(factory_simulation_get_solar_generator(simulation, 3U, &solar)
        == FACTORY_RESULT_OK);
    CHECK(solar.generated_last_tick == 10U);
    factory_simulation_destroy(simulation);
    return 0;
}

int main(void)
{
    CHECK(test_clock_curve_and_events() == 0);
    CHECK(test_generation_presentation_and_continuation() == 0);
    CHECK(test_transactional_demolition() == 0);
    CHECK(test_mixed_burner_and_solar_generation() == 0);
    puts("solar tests passed");
    return 0;
}
