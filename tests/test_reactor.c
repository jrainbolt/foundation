#include <foundation/event.h>
#include <foundation/heat.h>
#include <foundation/presentation.h>
#include <foundation/reactor.h>
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
    FactoryWorld *world = factory_world_create(5U, 5U);
    FactorySimulation *simulation;
    if (world == NULL) return NULL;
    simulation =
        factory_simulation_create_with_construction_units(world, 1000U);
    if (simulation == NULL) factory_world_destroy(world);
    return simulation;
}

static void destroy_borrowing(FactorySimulation *simulation)
{
    FactoryWorld *world;
    if (simulation == NULL) return;
    world = (FactoryWorld *)factory_simulation_get_world(simulation);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static FactoryResult submit(FactorySimulation *simulation, FactoryCommand c)
{
    return factory_simulation_submit_command(simulation, &c);
}

static int place(FactorySimulation *simulation)
{
    CHECK(submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_REACTOR_CORE,
        {.place_reactor_core = {1, 1}}}) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    return 0;
}

static int test_definition_heat_and_construction(void)
{
    FactorySimulation *simulation = make_simulation();
    FactoryReactorInspection reactor;
    FactoryHeatStorage heat = {0U, 10U};
    const FactoryNuclearFuelDefinition *fuel =
        factory_nuclear_fuel_definition_get(
            FACTORY_NUCLEAR_FUEL_BASIC_ROD);
    CHECK(simulation != NULL && fuel != NULL);
    CHECK(factory_nuclear_fuel_definition_count() == 1U
        && factory_nuclear_fuel_definition_at(0U) == fuel
        && factory_nuclear_fuel_definition_at(1U) == NULL);
    CHECK(fuel->total_heat_yield == 10000U
        && fuel->burn_duration_ticks == 100U
        && fuel->maximum_heat_output_per_tick == 100U);
    CHECK(factory_heat_storage_add(&heat, 10U)
        && !factory_heat_storage_add(&heat, 1U));
    CHECK(place(simulation) == 0);
    CHECK(factory_simulation_get_reactor(simulation, 1U, &reactor)
        == FACTORY_RESULT_OK);
    CHECK(reactor.stored_heat == 0U && reactor.heat_capacity == 10000U
        && reactor.inventory_quantity == 0U
        && reactor.active_fuel_id == FACTORY_NUCLEAR_FUEL_NONE
        && reactor.activity == FACTORY_REACTOR_IDLE);
    CHECK(submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_REACTOR_CORE,
        {.place_reactor_core = {1, 1}}}) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_TILE_OCCUPIED);
    simulation->reactors.items[0].heat_storage.stored_heat = 100U;
    simulation->reactors.items[0].active_fuel_id =
        FACTORY_NUCLEAR_FUEL_BASIC_ROD;
    simulation->reactors.items[0].remaining_heat_yield = 900U;
    CHECK(submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity = {1U}}}) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_reactor(simulation, 1U, &reactor)
        == FACTORY_RESULT_ENTITY_NOT_FOUND);
    destroy_borrowing(simulation);
    return 0;
}

static int test_fuel_validation_and_events(void)
{
    FactorySimulation *simulation = make_simulation();
    const FactoryCommandResult *first;
    const FactoryCommandResult *second;
    FactoryReactorInspection reactor;
    CHECK(simulation != NULL && place(simulation) == 0);
    CHECK(submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_INSERT_REACTOR_FUEL,
        {.insert_reactor_fuel = {
            1U, FACTORY_NUCLEAR_FUEL_BASIC_ROD}}}) == FACTORY_RESULT_OK);
    CHECK(submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_INSERT_REACTOR_FUEL,
        {.insert_reactor_fuel = {
            1U, FACTORY_NUCLEAR_FUEL_BASIC_ROD}}}) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    first = factory_simulation_get_command_result(simulation, 0U);
    second = factory_simulation_get_command_result(simulation, 1U);
    CHECK(first->result == FACTORY_RESULT_OK
        && second->result == FACTORY_RESULT_FUEL_INVENTORY_FULL);
    CHECK(factory_simulation_get_reactor(simulation, 1U, &reactor)
        == FACTORY_RESULT_OK);
    CHECK(reactor.active_fuel_id == FACTORY_NUCLEAR_FUEL_BASIC_ROD
        && reactor.inventory_quantity == 0U
        && reactor.stored_heat == 100U
        && reactor.remaining_heat_yield == 9900U
        && reactor.remaining_burn_ticks == 99U);
    CHECK(factory_simulation_get_event_count(simulation) == 2U);
    CHECK(factory_simulation_get_event(simulation, 0U)->type
        == FACTORY_EVENT_REACTOR_FUELED);
    CHECK(factory_simulation_get_event(simulation, 1U)->type
        == FACTORY_EVENT_REACTOR_HEAT_GENERATED);
    CHECK(submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_INSERT_REACTOR_FUEL,
        {.insert_reactor_fuel = {
            1U, FACTORY_NUCLEAR_FUEL_BASIC_ROD}}}) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_reactor(simulation, 1U, &reactor)
        == FACTORY_RESULT_OK && reactor.inventory_quantity == 1U);
    CHECK(submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_INSERT_REACTOR_FUEL,
        {.insert_reactor_fuel = {
            1U, FACTORY_NUCLEAR_FUEL_BASIC_ROD}}}) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_FUEL_INVENTORY_FULL);
    CHECK(submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_INSERT_REACTOR_FUEL,
        {.insert_reactor_fuel = {
            99U, FACTORY_NUCLEAR_FUEL_BASIC_ROD}}}) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_ENTITY_NOT_FOUND);
    CHECK(submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_INSERT_REACTOR_FUEL,
        {.insert_reactor_fuel = {1U, 99U}}})
        == FACTORY_RESULT_INVALID_ARGUMENT);
    destroy_borrowing(simulation);
    return 0;
}

static int test_pause_resume_and_exhaustion(void)
{
    FactorySimulation *simulation = make_simulation();
    FactoryReactorInspection inspection;
    FactorySnapshotBuffer snapshot = {0};
    const FactoryEvent *event;
    CHECK(simulation != NULL && place(simulation) == 0);
    CHECK(submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_INSERT_REACTOR_FUEL,
        {.insert_reactor_fuel = {
            1U, FACTORY_NUCLEAR_FUEL_BASIC_ROD}}}) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    simulation->reactors.items[0].heat_storage.stored_heat =
        FACTORY_REACTOR_HEAT_CAPACITY;
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_reactor(simulation, 1U, &inspection)
        == FACTORY_RESULT_OK);
    CHECK(inspection.activity == FACTORY_REACTOR_BLOCKED_HEAT_FULL
        && inspection.remaining_heat_yield == 9900U
        && inspection.generated_last_tick == 0U);
    CHECK(factory_simulation_create_snapshot(simulation, &snapshot)
        == FACTORY_RESULT_OK);
    factory_snapshot_buffer_destroy(&snapshot);
    simulation->reactors.items[0].heat_storage.stored_heat =
        FACTORY_REACTOR_HEAT_CAPACITY - 50U;
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_reactor(simulation, 1U, &inspection)
        == FACTORY_RESULT_OK);
    CHECK(inspection.stored_heat == FACTORY_REACTOR_HEAT_CAPACITY
        && inspection.generated_last_tick == 50U
        && inspection.remaining_heat_yield == 9850U
        && inspection.remaining_burn_ticks == 99U);
    simulation->reactors.items[0].heat_storage.stored_heat = 0U;
    simulation->reactors.items[0].remaining_heat_yield = 50U;
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_reactor(simulation, 1U, &inspection)
        == FACTORY_RESULT_OK);
    CHECK(inspection.stored_heat == 50U
        && inspection.active_fuel_id == FACTORY_NUCLEAR_FUEL_NONE
        && inspection.remaining_heat_yield == 0U);
    CHECK(factory_simulation_get_event_count(simulation) == 2U);
    event = factory_simulation_get_event(simulation, 1U);
    CHECK(event->type == FACTORY_EVENT_REACTOR_FUEL_EXHAUSTED
        && event->entity_id == 1U
        && event->nuclear_fuel_id == FACTORY_NUCLEAR_FUEL_BASIC_ROD
        && event->tick == factory_simulation_get_tick(simulation) - 1U);
    CHECK(factory_simulation_create_snapshot(simulation, &snapshot)
        == FACTORY_RESULT_OK);
    factory_snapshot_buffer_destroy(&snapshot);
    destroy_borrowing(simulation);
    return 0;
}

static int test_snapshot_continuation_and_presentation(void)
{
    FactorySimulation *a = make_simulation();
    FactorySimulation *b = NULL;
    FactorySnapshotBuffer snapshot = {0};
    FactoryPresentationSnapshot *presentation;
    const FactoryPresentationEntity *entity;
    CHECK(a != NULL && place(a) == 0);
    CHECK(submit(a, (FactoryCommand){
        FACTORY_COMMAND_INSERT_REACTOR_FUEL,
        {.insert_reactor_fuel = {
            1U, FACTORY_NUCLEAR_FUEL_BASIC_ROD}}}) == FACTORY_RESULT_OK);
    for (uint32_t i = 0U; i < 7U; ++i)
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(a, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(snapshot.data, snapshot.size, &b)
        == FACTORY_RESULT_OK);
    presentation = factory_presentation_snapshot_create();
    CHECK(presentation != NULL);
    CHECK(factory_presentation_snapshot_rebuild(presentation, b)
        == FACTORY_RESULT_OK);
    entity = factory_presentation_snapshot_get_entity(presentation, 0U);
    CHECK(entity != NULL
        && entity->entity_type == FACTORY_ENTITY_TYPE_REACTOR_CORE
        && entity->data.reactor.stored_heat == 700U
        && entity->data.reactor.remaining_heat_yield == 9300U
        && entity->data.reactor.generated_last_tick == 0U
        && entity->data.reactor.activity == FACTORY_REACTOR_IDLE);
    for (uint32_t i = 0U; i < 20U; ++i) {
        FactoryReactorInspection left;
        FactoryReactorInspection right;
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_reactor(a, 1U, &left)
            == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_reactor(b, 1U, &right)
            == FACTORY_RESULT_OK);
        CHECK(left.stored_heat == right.stored_heat
            && left.remaining_heat_yield == right.remaining_heat_yield
            && left.remaining_burn_ticks == right.remaining_burn_ticks
            && left.generated_last_tick == right.generated_last_tick);
    }
    factory_presentation_snapshot_destroy(presentation);
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(b);
    destroy_borrowing(a);
    return 0;
}

int main(void)
{
    CHECK(test_definition_heat_and_construction() == 0);
    CHECK(test_fuel_validation_and_events() == 0);
    CHECK(test_pause_resume_and_exhaustion() == 0);
    CHECK(test_snapshot_continuation_and_presentation() == 0);
    puts("reactor tests passed");
    return 0;
}
